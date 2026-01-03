// Wrapper: widen AXI-Lite data to 64-bit for Renode IntegrationLibrary
`timescale 1ns/1ps

module fir_filter_axil_cosim(
  input  wire         ACLK,
  input  wire         ARESETn,

  // AXI-Lite write address channel
  input  wire [31:0]  saxi_awaddr,
  input  wire         saxi_awvalid,
  output wire         saxi_awready,

  // AXI-Lite write data channel (64-bit, lower 32 used)
  input  wire [63:0]  saxi_wdata,
  input  wire  [7:0]  saxi_wstrb,
  input  wire         saxi_wvalid,
  output wire         saxi_wready,

  // AXI-Lite write response channel
  output wire  [1:0]  saxi_bresp,
  output wire         saxi_bvalid,
  input  wire         saxi_bready,

  // AXI-Lite read address channel
  input  wire [31:0]  saxi_araddr,
  input  wire         saxi_arvalid,
  output wire         saxi_arready,

  // AXI-Lite read data channel (64-bit, lower 32 valid)
  output wire [63:0]  saxi_rdata,
  output wire  [1:0]  saxi_rresp,
  output wire         saxi_rvalid,
  input  wire         saxi_rready
);

  // Internal wires to 32-bit core
  wire        core_awready;
  wire        core_wready;
  wire  [1:0] core_bresp;
  wire        core_bvalid;
  wire        core_arready;
  wire [31:0] core_rdata;
  wire  [1:0] core_rresp;
  wire        core_rvalid;

  // Latching for independent AW/W handshakes (outer)
  reg         have_aw;
  reg         have_w;
  reg  [31:0] awaddr_lat;
  reg  [31:0] wdata_lat;
  reg   [3:0] wstrb_lat;
  reg         inflight;     // a write issued to core, waiting for response

  // Outer ready when no latched value and no inflight
  assign saxi_awready = (!have_aw) && (!inflight);
  assign saxi_wready  = (!have_w)  && (!inflight);

  // Latch outer channel payloads
  always @(posedge ACLK or negedge ARESETn) begin
    if(!ARESETn) begin
      have_aw   <= 1'b0;
      have_w    <= 1'b0;
      awaddr_lat<= 32'd0;
      wdata_lat <= 32'd0;
      wstrb_lat <= 4'd0;
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
      // After core accepts the combined write (see below), clear latches
      if(inflight && core_bvalid && saxi_bready) begin
        // completion handled in response block
      end
    end
  end

  // Drive core: assert both valid in the same cycle when both payloads are ready and core is ready
  reg issue_pulse;
  always @(posedge ACLK or negedge ARESETn) begin
    if(!ARESETn) begin
      issue_pulse <= 1'b0;
      inflight    <= 1'b0;
    end else begin
      issue_pulse <= 1'b0;
      if(!inflight && have_aw && have_w && core_awready && core_wready) begin
        issue_pulse <= 1'b1;   // one-cycle pulse
        inflight    <= 1'b1;
        have_aw     <= 1'b0;
        have_w      <= 1'b0;
      end
      // Mark completion when core bvalid handshake finishes
      if(inflight && core_bvalid && saxi_bready) begin
        inflight <= 1'b0;
      end
    end
  end

  // Zero-extend 32-bit read data to 64-bit
  assign saxi_rdata = {32'd0, core_rdata};

  // Core instance
  fir_filter_axil u_core (
    .aclk         (ACLK),
    .aresetn      (ARESETn),

    .saxi_awvalid (issue_pulse),
    .saxi_awready (core_awready),
    .saxi_awaddr  (awaddr_lat),

    .saxi_wvalid  (issue_pulse),
    .saxi_wready  (core_wready),
    .saxi_wdata   (wdata_lat),
    .saxi_wstrb   (wstrb_lat),

    .saxi_bvalid  (core_bvalid),
    .saxi_bready  (saxi_bready),
    .saxi_bresp   (core_bresp),

    .saxi_arvalid (saxi_arvalid),
    .saxi_arready (core_arready),
    .saxi_araddr  (saxi_araddr),

    .saxi_rvalid  (core_rvalid),
    .saxi_rready  (saxi_rready),
    .saxi_rdata   (core_rdata),
    .saxi_rresp   (core_rresp)
  );

  // Pass-through of response and read handshake
  assign saxi_bresp   = core_bresp;
  assign saxi_bvalid  = core_bvalid;
  assign saxi_arready = core_arready;
  assign saxi_rresp   = core_rresp;
  assign saxi_rvalid  = core_rvalid;

endmodule
