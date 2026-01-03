// AXI-Lite bridge + 64-bit data widen for Smart Timer
`timescale 1ns/1ps

module smart_timer_axil_cosim(
  input  wire         ACLK,
  input  wire         ARESETn,

  // Write address channel (32-bit)
  input  wire [31:0]  saxi_awaddr,
  input  wire         saxi_awvalid,
  output wire         saxi_awready,

  // Write data channel (64-bit, lower 32 used)
  input  wire [63:0]  saxi_wdata,
  input  wire  [7:0]  saxi_wstrb,
  input  wire         saxi_wvalid,
  output wire         saxi_wready,

  // Write response
  output wire  [1:0]  saxi_bresp,
  output wire         saxi_bvalid,
  input  wire         saxi_bready,

  // Read address channel
  input  wire [31:0]  saxi_araddr,
  input  wire         saxi_arvalid,
  output wire         saxi_arready,

  // Read data channel (64-bit, lower 32 valid)
  output wire [63:0]  saxi_rdata,
  output wire  [1:0]  saxi_rresp,
  output wire         saxi_rvalid,
  input  wire         saxi_rready,

  // PWM output passthrough
  output wire         pwm_out
);

  // Bridge state for AW/W joining
  reg         have_aw;
  reg         have_w;
  reg  [31:0] awaddr_lat;
  reg  [31:0] wdata_lat;
  reg   [3:0] wstrb_lat;
  reg         inflight;
  reg         issue_pulse;

  // Core wires
  wire        core_awready;
  wire        core_wready;
  wire  [1:0] core_bresp;
  wire        core_bvalid;
  wire        core_arready;
  wire [31:0] core_rdata;
  wire  [1:0] core_rresp;
  wire        core_rvalid;

  // Ready for outer channels when nothing latched/inflight
  assign saxi_awready = (!have_aw) && (!inflight);
  assign saxi_wready  = (!have_w)  && (!inflight);

  // Latch payloads
  always @(posedge ACLK or negedge ARESETn) begin
    if(!ARESETn) begin
      have_aw    <= 1'b0;
      have_w     <= 1'b0;
      awaddr_lat <= 32'd0;
      wdata_lat  <= 32'd0;
      wstrb_lat  <= 4'd0;
    end else begin
      if(saxi_awvalid && saxi_awready) begin
        have_aw    <= 1'b1;
        awaddr_lat <= saxi_awaddr;
      end
      if(saxi_wvalid && saxi_wready) begin
        have_w    <= 1'b1;
        wdata_lat <= saxi_wdata[31:0];
        wstrb_lat <= saxi_wstrb[3:0];
      end
    end
  end

  // Issue combined write to core
  always @(posedge ACLK or negedge ARESETn) begin
    if(!ARESETn) begin
      issue_pulse <= 1'b0;
      inflight    <= 1'b0;
    end else begin
      issue_pulse <= 1'b0;
      if(!inflight && have_aw && have_w && core_awready && core_wready) begin
        issue_pulse <= 1'b1;
        inflight    <= 1'b1;
        have_aw     <= 1'b0;
        have_w      <= 1'b0;
      end
      if(inflight && core_bvalid && saxi_bready) begin
        inflight <= 1'b0;
      end
    end
  end

  // Zero-extend read data to 64-bit
  assign saxi_rdata = {32'd0, core_rdata};

  // Instantiate original core with 32-bit porting
  smart_timer_axil #(
    .ADDR_WIDTH(6),
    .DATA_WIDTH(32)
  ) u_core (
    .ACLK         (ACLK),
    .ARESETn      (ARESETn),

    .saxi_awaddr  (awaddr_lat[5:0]),
    .saxi_awvalid (issue_pulse),
    .saxi_awready (core_awready),

    .saxi_wdata   (wdata_lat),
    .saxi_wstrb   (wstrb_lat),
    .saxi_wvalid  (issue_pulse),
    .saxi_wready  (core_wready),

    .saxi_bresp   (core_bresp),
    .saxi_bvalid  (core_bvalid),
    .saxi_bready  (saxi_bready),

    .saxi_araddr  (saxi_araddr[5:0]),
    .saxi_arvalid (saxi_arvalid),
    .saxi_arready (core_arready),

    .saxi_rdata   (core_rdata),
    .saxi_rresp   (core_rresp),
    .saxi_rvalid  (core_rvalid),
    .saxi_rready  (saxi_rready),

    .pwm_out      (pwm_out)
  );

  // Pass-through responses
  assign saxi_bresp   = core_bresp;
  assign saxi_bvalid  = core_bvalid;
  assign saxi_arready = core_arready;
  assign saxi_rresp   = core_rresp;
  assign saxi_rvalid  = core_rvalid;

endmodule

