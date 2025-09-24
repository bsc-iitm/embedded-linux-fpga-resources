// Purpose: Smart Timer V1 (PWM) with AXI4-Lite register interface
// Registers (32-bit words):
//  0x00 CTRL   [bit0 EN (RW), bit1 RST (W1P self-clear), others reserved RAZ/WI]
//  0x04 PERIOD [RW]  default 0x0000_00FF
//  0x08 DUTY   [RW]  default 0x0000_0000
//  0x0C STATUS [RO/W1C] bit0 WRAP(sticky), bit1 UPD_PENDING

`timescale 1ns/1ps

module smart_timer_axil #(
  parameter ADDR_WIDTH = 6,
  parameter DATA_WIDTH = 32
)(
  input  wire                    ACLK,
  input  wire                    ARESETn,

  // Write address channel
  input  wire [ADDR_WIDTH-1:0]   saxi_awaddr,
  input  wire                    saxi_awvalid,
  output wire                    saxi_awready,

  // Write data channel
  input  wire [DATA_WIDTH-1:0]   saxi_wdata,
  input  wire [DATA_WIDTH/8-1:0] saxi_wstrb,
  input  wire                    saxi_wvalid,
  output wire                    saxi_wready,

  // Write response channel
  output reg  [1:0]              saxi_bresp,
  output reg                     saxi_bvalid,
  input  wire                    saxi_bready,

  // Read address channel
  input  wire [ADDR_WIDTH-1:0]   saxi_araddr,
  input  wire                    saxi_arvalid,
  output wire                    saxi_arready,

  // Read data channel
  output reg  [DATA_WIDTH-1:0]   saxi_rdata,
  output reg  [1:0]              saxi_rresp,
  output reg                     saxi_rvalid,
  input  wire                    saxi_rready,

  // Observable PWM output for simulation/demo
  output wire                    pwm_out
);

  localparam OKAY   = 2'b00;
  localparam SLVERR = 2'b10;

  // -----------------
  // AXI-Lite plumbing
  // -----------------
  reg                   aw_hs_done;
  reg [ADDR_WIDTH-1:0]  awaddr_q;
  reg                   w_hs_done;
  reg [DATA_WIDTH-1:0]  wdata_q;
  reg [DATA_WIDTH/8-1:0] wstrb_q;

  reg                   ar_hs_done;
  reg [ADDR_WIDTH-1:0]  araddr_q;

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

  wire do_write = aw_hs_done && w_hs_done && ~saxi_bvalid;
  wire [1:0] word_sel_w = awaddr_q[3:2];
  wire [1:0] word_sel_r = araddr_q[3:2];

  // -----------------
  // Registers & logic
  // -----------------
  // CTRL fields
  reg        ctrl_en;
  reg        ctrl_rst_pulse;  // self-clearing pulse on write

  // Shadow and active values for PERIOD/DUTY
  reg [31:0] period_shadow, period_active;
  reg [31:0] duty_shadow,   duty_active;
  reg        upd_pending; // exposed in STATUS bit1

  // STATUS sticky flags (W1C)
  reg status_wrap;

  // Default reset values
  localparam [31:0] PERIOD_RST = 32'h0000_00FF;
  localparam [31:0] DUTY_RST   = 32'h0000_0000;

  // Write side-effects
  integer i;
  always @(posedge ACLK or negedge ARESETn) begin
    if (!ARESETn) begin
      // Reset registers
      ctrl_en        <= 1'b0;
      ctrl_rst_pulse <= 1'b0;
      period_shadow  <= PERIOD_RST;
      duty_shadow    <= DUTY_RST;
      upd_pending    <= 1'b0;
      status_wrap    <= 1'b0;
      saxi_bvalid    <= 1'b0;
      saxi_bresp     <= OKAY;
    end else begin
      // Default self-clearing pulse low unless reasserted by write
      ctrl_rst_pulse <= 1'b0;

      if (do_write) begin
        case (word_sel_w)
          2'b00: begin // CTRL
            // Only bits [1:0] are writable; RAZ/WI for others
            // Apply WSTRB at byte granularity
            // Build a masked write value for the 2 LSBs
            // Byte 0 carries bits [7:0]
            if (wstrb_q[0]) begin
              ctrl_en        <= wdata_q[0];
              ctrl_rst_pulse <= wdata_q[1]; // write 1 triggers a pulse
            end
            // Writes to bytes 1..3 are ignored (reserved)
            saxi_bresp <= OKAY;
          end
          2'b01: begin // PERIOD @0x04
            for (i = 0; i < DATA_WIDTH/8; i = i + 1) begin
              if (wstrb_q[i]) period_shadow[i*8 +: 8] <= wdata_q[i*8 +: 8];
            end
            // If running, mark pending; if stopped, commit immediately
            if (ctrl_en) upd_pending <= 1'b1; else begin
              period_active <= period_shadow;
              upd_pending   <= 1'b0;
            end
            saxi_bresp <= OKAY;
          end
          2'b10: begin // DUTY @0x08
            for (i = 0; i < DATA_WIDTH/8; i = i + 1) begin
              if (wstrb_q[i]) duty_shadow[i*8 +: 8] <= wdata_q[i*8 +: 8];
            end
            if (ctrl_en) upd_pending <= 1'b1; else begin
              duty_active <= duty_shadow;
              upd_pending <= 1'b0;
            end
            saxi_bresp <= OKAY;
          end
          2'b11: begin // STATUS @0x0C (W1C for bit0)
            if (wstrb_q[0] && wdata_q[0]) status_wrap <= 1'b0; // W1C WRAP
            saxi_bresp <= OKAY;
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
        case (saxi_araddr[3:2])
          2'b00: begin // CTRL readback: EN visible; RST reads as 0
            saxi_rdata <= {30'd0, 1'b0, ctrl_en};
            saxi_rresp <= OKAY;
          end
          2'b01: begin // PERIOD (shadow)
            saxi_rdata <= period_shadow;
            saxi_rresp <= OKAY;
          end
          2'b10: begin // DUTY (shadow)
            saxi_rdata <= duty_shadow;
            saxi_rresp <= OKAY;
          end
          2'b11: begin // STATUS: bit0=WRAP(sticky), bit1=UPD_PENDING
            saxi_rdata <= {30'd0, upd_pending, status_wrap};
            saxi_rresp <= OKAY;
          end
        endcase
        saxi_rvalid <= 1'b1;
      end
      if (saxi_rvalid && saxi_rready) begin
        saxi_rvalid <= 1'b0;
        ar_hs_done   <= 1'b0;
      end
    end
  end

  // -----------------
  // PWM integration
  // -----------------
  wire rstn_core = ARESETn & ~ctrl_rst_pulse; // software reset pulse
  wire wrap_pulse;

  // Active values reset
  always @(posedge ACLK or negedge ARESETn) begin
    if (!ARESETn) begin
      period_active <= PERIOD_RST;
      duty_active   <= DUTY_RST;
    end else begin
      // Commit deferred updates at wrap or when disabled
      if (!ctrl_en) begin
        period_active <= period_shadow;
        duty_active   <= duty_shadow;
      end else if (status_wrap && upd_pending) begin
        period_active <= period_shadow;
        duty_active   <= duty_shadow;
        upd_pending   <= 1'b0;
      end
    end
  end

  // STATUS sticky set on wrap
  always @(posedge ACLK or negedge ARESETn) begin
    if (!ARESETn) begin
      status_wrap <= 1'b0;
    end else begin
      if (wrap_pulse) status_wrap <= 1'b1;
    end
  end

  pwm_core u_pwm (
    .clk     (ACLK),
    .rstn    (rstn_core),
    .en      (ctrl_en),
    .period  (period_active),
    .duty    (duty_active),
    .pwm_out (pwm_out),
    .wrap    (wrap_pulse)
  );

endmodule

