// AXI-Lite bridge + 64-bit data widen for Smart Timer with IRQ
// Wraps the 32-bit s_axi_* ports in smarttimer_axil_irq to a 64-bit AXI-Lite bus

`timescale 1ns/1ps

module smarttimer_axil_irq_cosim(
  input  wire         ACLK,
  input  wire         ARESETn,

  // AXI-Lite slave (64-bit data/32-bit addr) expected by IntegrationLibrary
  input  wire [31:0]  saxi_awaddr,
  input  wire         saxi_awvalid,
  output wire         saxi_awready,

  input  wire [63:0]  saxi_wdata,
  input  wire  [7:0]  saxi_wstrb,
  input  wire         saxi_wvalid,
  output wire         saxi_wready,

  output wire  [1:0]  saxi_bresp,
  output wire         saxi_bvalid,
  input  wire         saxi_bready,

  input  wire [31:0]  saxi_araddr,
  input  wire         saxi_arvalid,
  output wire         saxi_arready,

  output wire [63:0]  saxi_rdata,
  output wire  [1:0]  saxi_rresp,
  output wire         saxi_rvalid,
  input  wire         saxi_rready,

  // Passthrough outputs
  output wire         pwm_out,
  output wire         irq_out
);

  // Bridge state for AW/W join
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

  // Instantiate IRQ-capable core with 32-bit porting
  smarttimer_axil_irq u_core (
    .clk           (ACLK),
    .resetn        (ARESETn),

    .s_axi_awaddr  ({26'b0, awaddr_lat[5:0]}),
    .s_axi_awvalid (issue_pulse),
    .s_axi_awready (core_awready),

    .s_axi_wdata   (wdata_lat),
    .s_axi_wstrb   (wstrb_lat),
    .s_axi_wvalid  (issue_pulse),
    .s_axi_wready  (core_wready),

    .s_axi_bresp   (core_bresp),
    .s_axi_bvalid  (core_bvalid),
    .s_axi_bready  (saxi_bready),

    .s_axi_araddr  ({26'b0, saxi_araddr[5:0]}),
    .s_axi_arvalid (saxi_arvalid),
    .s_axi_arready (core_arready),

    .s_axi_rdata   (core_rdata),
    .s_axi_rresp   (core_rresp),
    .s_axi_rvalid  (core_rvalid),
    .s_axi_rready  (saxi_rready),

    .pwm_out       (pwm_out),
    .irq_out       (irq_out)
  );

  // Pass-through responses
  assign saxi_bresp   = core_bresp;
  assign saxi_bvalid  = core_bvalid;
  assign saxi_arready = core_arready;
  assign saxi_rresp   = core_rresp;
  assign saxi_rvalid  = core_rvalid;

endmodule

