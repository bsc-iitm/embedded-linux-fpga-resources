// Purpose: Minimal AXI4-Lite register bank with CTRL (0x0) and DATA (0x4)
// Notes: Single-beat transactions, 32-bit data, handles AW/W in any order.
// Implements wstrb, OKAY for valid regs, SLVERR for others.

`timescale 1ns/1ps

module axil_regbank #(
  parameter ADDR_WIDTH = 4,
  parameter DATA_WIDTH = 32
)(
  input  wire                   ACLK,
  input  wire                   ARESETn,

  // Write address channel
  input  wire [ADDR_WIDTH-1:0]  saxi_awaddr,
  input  wire                   saxi_awvalid,
  output wire                   saxi_awready,

  // Write data channel
  input  wire [DATA_WIDTH-1:0]  saxi_wdata,
  input  wire [DATA_WIDTH/8-1:0] saxi_wstrb,
  input  wire                   saxi_wvalid,
  output wire                   saxi_wready,

  // Write response channel
  output reg  [1:0]             saxi_bresp,
  output reg                    saxi_bvalid,
  input  wire                   saxi_bready,

  // Read address channel
  input  wire [ADDR_WIDTH-1:0]  saxi_araddr,
  input  wire                   saxi_arvalid,
  output wire                   saxi_arready,

  // Read data channel
  output reg  [DATA_WIDTH-1:0]  saxi_rdata,
  output reg  [1:0]             saxi_rresp,
  output reg                    saxi_rvalid,
  input  wire                   saxi_rready
);

  localparam OKAY  = 2'b00;
  localparam SLVERR= 2'b10;

  // Simple regs
  reg [DATA_WIDTH-1:0] reg_ctrl;
  reg [DATA_WIDTH-1:0] reg_data;

  // Latching write address and data (independent arrival)
  reg                   aw_hs_done;
  reg [ADDR_WIDTH-1:0]  awaddr_q;
  reg                   w_hs_done;
  reg [DATA_WIDTH-1:0]  wdata_q;
  reg [DATA_WIDTH/8-1:0] wstrb_q;

  // Read address handshake
  reg                   ar_hs_done;
  reg [ADDR_WIDTH-1:0]  araddr_q;

  // Ready signals based on whether we've latched the payloads
  assign saxi_awready = (~aw_hs_done) & ARESETn;
  assign saxi_wready  = (~w_hs_done)  & ARESETn;
  assign saxi_arready = (~ar_hs_done) & (~saxi_rvalid) & ARESETn;

  // Write address handshake/latch
  always @(posedge ACLK or negedge ARESETn) begin
    if (!ARESETn) begin
      aw_hs_done <= 1'b0;
      awaddr_q   <= {ADDR_WIDTH{1'b0}};
    end else begin
      if (!aw_hs_done && saxi_awvalid && saxi_awready) begin
        aw_hs_done <= 1'b1;
        awaddr_q   <= saxi_awaddr;
      end
      // Clear after write response accepted
      if (saxi_bvalid && saxi_bready) begin
        aw_hs_done <= 1'b0;
      end
    end
  end

  // Write data handshake/latch
  always @(posedge ACLK or negedge ARESETn) begin
    if (!ARESETn) begin
      w_hs_done <= 1'b0;
      wdata_q   <= {DATA_WIDTH{1'b0}};
      wstrb_q   <= {DATA_WIDTH/8{1'b0}};
    end else begin
      if (!w_hs_done && saxi_wvalid && saxi_wready) begin
        w_hs_done <= 1'b1;
        wdata_q   <= saxi_wdata;
        wstrb_q   <= saxi_wstrb;
      end
      if (saxi_bvalid && saxi_bready) begin
        w_hs_done <= 1'b0;
      end
    end
  end

  // Perform the write when both AW and W are captured
  wire do_write = aw_hs_done && w_hs_done && ~saxi_bvalid;

  // Address decode on word offsets (assume 32-bit words)
  wire [1:0] word_sel_w = awaddr_q[3:2];
  wire [1:0] word_sel_r = araddr_q[3:2];

  integer i;
  always @(posedge ACLK or negedge ARESETn) begin
    if (!ARESETn) begin
      reg_ctrl   <= {DATA_WIDTH{1'b0}};
      reg_data   <= {DATA_WIDTH{1'b0}};
      saxi_bvalid <= 1'b0;
      saxi_bresp  <= OKAY;
    end else begin
      if (do_write) begin
        case (word_sel_w)
          2'b00: begin // CTRL @ 0x0
            for (i = 0; i < DATA_WIDTH/8; i = i + 1) begin
              if (wstrb_q[i]) reg_ctrl[i*8 +: 8] <= wdata_q[i*8 +: 8];
            end
            saxi_bresp <= OKAY;
          end
          2'b01: begin // DATA @ 0x4
            for (i = 0; i < DATA_WIDTH/8; i = i + 1) begin
              if (wstrb_q[i]) reg_data[i*8 +: 8] <= wdata_q[i*8 +: 8];
            end
            saxi_bresp <= OKAY;
          end
          default: begin
            // Invalid address
            saxi_bresp <= SLVERR;
          end
        endcase
        saxi_bvalid <= 1'b1;
      end
      if (saxi_bvalid && saxi_bready) begin
        saxi_bvalid <= 1'b0;
      end
    end
  end

  // Read address handshake/latch
  always @(posedge ACLK or negedge ARESETn) begin
    if (!ARESETn) begin
      ar_hs_done <= 1'b0;
      araddr_q   <= {ADDR_WIDTH{1'b0}};
      saxi_rvalid <= 1'b0;
      saxi_rresp  <= OKAY;
      saxi_rdata  <= {DATA_WIDTH{1'b0}};
    end else begin
      if (!ar_hs_done && saxi_arvalid && saxi_arready) begin
        ar_hs_done <= 1'b1;
        araddr_q   <= saxi_araddr;
        // Prepare data/resp immediately; present on rvalid
        case (saxi_araddr[3:2])
          2'b00: begin saxi_rdata <= reg_ctrl; saxi_rresp <= OKAY; end
          2'b01: begin saxi_rdata <= reg_data; saxi_rresp <= OKAY; end
          default: begin saxi_rdata <= {DATA_WIDTH{1'b0}}; saxi_rresp <= SLVERR; end
        endcase
        saxi_rvalid <= 1'b1;
      end
      if (saxi_rvalid && saxi_rready) begin
        saxi_rvalid <= 1'b0;
        ar_hs_done   <= 1'b0;
      end
    end
  end

endmodule

