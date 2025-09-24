// Purpose: Simple synchronous up-counter with enable and active-low reset
// Minimal DUT to demonstrate cocotb clock/reset/drives/asserts

`timescale 1ns/1ps

module counter #(
  parameter WIDTH = 8
)(
  input  wire             clk,
  input  wire             rstn,   // active-low synchronous reset
  input  wire             en,
  output reg  [WIDTH-1:0] q
);

  always @(posedge clk) begin
    if (!rstn) begin
      q <= {WIDTH{1'b0}};
    end else if (en) begin
      q <= q + 1'b1;
    end
  end

endmodule

