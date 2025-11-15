// Smart Timer with AXI-Lite interface and IRQ support
// Derived from week07 core (smart_timer_axil) with minimal IRQ addition.
// Differences vs week07:
// - Register map adjusted to match week08 driver/tests:
//   0x00 CTRL   [bit0 EN (RW), bit1 RST (W1P)]
//   0x04 STATUS [bit0 WRAP (RO/W1C), bit1 UPD_PENDING (RO)]
//   0x08 PERIOD [RW]
//   0x0C DUTY   [RW]
// - irq_out asserts when STATUS.WRAP=1 (cleared by W1C)

`timescale 1ns/1ps

module smarttimer_axil_irq (
  input  wire        clk,
  input  wire        resetn,

  // AXI-Lite slave (32-bit)
  input  wire [31:0] s_axi_awaddr,
  input  wire        s_axi_awvalid,
  output wire        s_axi_awready,

  input  wire [31:0] s_axi_wdata,
  input  wire  [3:0] s_axi_wstrb,
  input  wire        s_axi_wvalid,
  output wire        s_axi_wready,

  output reg   [1:0] s_axi_bresp,
  output reg         s_axi_bvalid,
  input  wire        s_axi_bready,

  input  wire [31:0] s_axi_araddr,
  input  wire        s_axi_arvalid,
  output wire        s_axi_arready,

  output reg  [31:0] s_axi_rdata,
  output reg   [1:0] s_axi_rresp,
  output reg         s_axi_rvalid,
  input  wire        s_axi_rready,

  // Observable outputs
  output wire        pwm_out,
  output wire        irq_out
);

  localparam OKAY = 2'b00;

  // AXI-Lite plumbing (week07 style)
  reg                 aw_hs_done;
  reg [31:0]          awaddr_q;
  reg                 w_hs_done;
  reg [31:0]          wdata_q;
  reg  [3:0]          wstrb_q;

  reg                 ar_hs_done;
  reg [31:0]          araddr_q;

  assign s_axi_awready = (~aw_hs_done) & resetn;
  assign s_axi_wready  = (~w_hs_done)  & resetn;
  assign s_axi_arready = (~ar_hs_done) & (~s_axi_rvalid) & resetn;

  // Write address handshake/latch
  always @(posedge clk or negedge resetn) begin
    if (!resetn) begin
      aw_hs_done <= 1'b0;
      awaddr_q   <= 32'd0;
    end else begin
      if (!aw_hs_done && s_axi_awvalid && s_axi_awready) begin
        aw_hs_done <= 1'b1;
        awaddr_q   <= s_axi_awaddr;
      end
      if (s_axi_bvalid && s_axi_bready) begin
        aw_hs_done <= 1'b0;
      end
    end
  end

  // Write data handshake/latch
  always @(posedge clk or negedge resetn) begin
    if (!resetn) begin
      w_hs_done <= 1'b0;
      wdata_q   <= 32'd0;
      wstrb_q   <= 4'd0;
    end else begin
      if (!w_hs_done && s_axi_wvalid && s_axi_wready) begin
        w_hs_done <= 1'b1;
        wdata_q   <= s_axi_wdata;
        wstrb_q   <= s_axi_wstrb;
      end
      if (s_axi_bvalid && s_axi_bready) begin
        w_hs_done <= 1'b0;
      end
    end
  end

  wire do_write = aw_hs_done && w_hs_done && ~s_axi_bvalid;
  wire [1:0] word_sel_w = awaddr_q[3:2];

  // Registers & state (week07 features)
  reg        ctrl_en;
  reg        ctrl_rst_pulse;  // W1P
  reg [31:0] period_shadow, period_active;
  reg [31:0] duty_shadow,   duty_active;
  reg        upd_pending; // STATUS[1]
  reg        status_wrap; // STATUS[0], sticky W1C

  localparam [31:0] PERIOD_RST = 32'h0000_00FF;
  localparam [31:0] DUTY_RST   = 32'h0000_0000;

  // Write side-effects
  integer i;
  always @(posedge clk or negedge resetn) begin
    if (!resetn) begin
      ctrl_en        <= 1'b0;
      ctrl_rst_pulse <= 1'b0;
      period_shadow  <= PERIOD_RST;
      duty_shadow    <= DUTY_RST;
      upd_pending    <= 1'b0;
      status_wrap    <= 1'b0;
      s_axi_bvalid   <= 1'b0;
      s_axi_bresp    <= OKAY;
    end else begin
      ctrl_rst_pulse <= 1'b0; // self-clear
      // Sticky updates
      if (wrap_pulse) status_wrap <= 1'b1;
      if (status_wrap && upd_pending)
        upd_pending <= 1'b0;

      if (do_write) begin
        case (word_sel_w)
          2'b00: begin // CTRL @0x00
            if (wstrb_q[0]) begin
              ctrl_en        <= wdata_q[0];
              ctrl_rst_pulse <= wdata_q[1];
            end
            s_axi_bresp <= OKAY;
          end
          2'b01: begin // STATUS @0x04 (W1C for WRAP)
            if (wstrb_q[0] && wdata_q[0]) status_wrap <= 1'b0;
            s_axi_bresp <= OKAY;
          end
          2'b10: begin // PERIOD @0x08 (shadowed)
            for (i = 0; i < 4; i = i + 1) begin
              if (wstrb_q[i]) period_shadow[i*8 +: 8] <= wdata_q[i*8 +: 8];
            end
            upd_pending <= ctrl_en; // commit at wrap when running
            s_axi_bresp <= OKAY;
          end
          2'b11: begin // DUTY @0x0C (shadowed)
            for (i = 0; i < 4; i = i + 1) begin
              if (wstrb_q[i]) duty_shadow[i*8 +: 8] <= wdata_q[i*8 +: 8];
            end
            upd_pending <= ctrl_en;
            s_axi_bresp <= OKAY;
          end
        endcase
        s_axi_bvalid <= 1'b1;
      end

      if (s_axi_bvalid && s_axi_bready) begin
        s_axi_bvalid <= 1'b0;
      end
    end
  end

  // Read address handshake and data mux
  always @(posedge clk or negedge resetn) begin
    if (!resetn) begin
      ar_hs_done <= 1'b0;
      araddr_q   <= 32'd0;
      s_axi_rvalid <= 1'b0;
      s_axi_rresp  <= OKAY;
      s_axi_rdata  <= 32'd0;
    end else begin
      if (!ar_hs_done && s_axi_arvalid && s_axi_arready) begin
        ar_hs_done <= 1'b1;
        araddr_q   <= s_axi_araddr;
        case (s_axi_araddr[3:2])
          2'b00: begin // CTRL readback: EN visible; RST reads as 0
            s_axi_rdata <= {30'd0, 1'b0, ctrl_en};
            s_axi_rresp <= OKAY;
          end
          2'b01: begin // STATUS
            s_axi_rdata <= {30'd0, upd_pending, status_wrap};
            s_axi_rresp <= OKAY;
          end
          2'b10: begin // PERIOD (shadow)
            s_axi_rdata <= period_shadow;
            s_axi_rresp <= OKAY;
          end
          2'b11: begin // DUTY (shadow)
            s_axi_rdata <= duty_shadow;
            s_axi_rresp <= OKAY;
          end
        endcase
        s_axi_rvalid <= 1'b1;
      end
      if (s_axi_rvalid && s_axi_rready) begin
        s_axi_rvalid <= 1'b0;
        ar_hs_done   <= 1'b0;
      end
    end
  end

  // PWM integration (week07)
  wire rstn_core = resetn & ~ctrl_rst_pulse; // software reset pulse
  wire wrap_pulse;

  // Commit active values at wrap or when disabled
  always @(posedge clk or negedge resetn) begin
    if (!resetn) begin
      period_active <= PERIOD_RST;
      duty_active   <= DUTY_RST;
    end else begin
      if (!ctrl_en) begin
        period_active <= period_shadow;
        duty_active   <= duty_shadow;
      end else if (status_wrap && upd_pending) begin
        period_active <= period_shadow;
        duty_active   <= duty_shadow;
      end
    end
  end

  pwm_core u_pwm (
    .clk     (clk),
    .rstn    (rstn_core),
    .en      (ctrl_en),
    .period  (period_active),
    .duty    (duty_active),
    .pwm_out (pwm_out),
    .wrap    (wrap_pulse)
  );

  // IRQ asserted on sticky status
  assign irq_out = status_wrap;

endmodule
