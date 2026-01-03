// Purpose: PWM core with free-running counter and compare (from week07)
`timescale 1ns/1ps

module pwm_core (
  input  wire        clk,
  input  wire        rstn,
  input  wire        en,
  input  wire [31:0] period,
  input  wire [31:0] duty,
  output reg         pwm_out,
  output reg         wrap
);
  reg [31:0] cnt;
  wire [31:0] duty_clip = (duty > period) ? period : duty;

  always @(posedge clk or negedge rstn) begin
    if (!rstn) begin
      cnt  <= 32'd0;
      wrap <= 1'b0;
    end else if (!en) begin
      cnt  <= 32'd0;
      wrap <= 1'b0;
    end else begin
      wrap <= 1'b0;
      if (cnt == period) begin
        cnt  <= 32'd0;
        wrap <= 1'b1; // one-cycle pulse on wrap
      end else begin
        cnt <= cnt + 1'b1;
      end
    end
  end

  always @(posedge clk or negedge rstn) begin
    if (!rstn) begin
      pwm_out <= 1'b0;
    end else begin
      pwm_out <= (cnt < duty_clip);
    end
  end
endmodule
