// Purpose: Pure Verilog testbench for counter.v (for comparison with cocotb)
// Demonstrates manual clock/reset generation, stimulus, and checks + VCD dump

`timescale 1ns/1ps

module counter_tb;
  localparam WIDTH = 8;

  reg clk;
  reg rstn;
  reg en;
  wire [WIDTH-1:0] q;

  // DUT
  counter #(.WIDTH(WIDTH)) dut (
    .clk(clk),
    .rstn(rstn),
    .en(en),
    .q(q)
  );

  // 100 MHz clock (10ns period)
  initial clk = 0;
  always #5 clk = ~clk;

  // VCD waveform dump
  initial begin
    $dumpfile("counter_tb.vcd");
    $dumpvars(0, counter_tb);
  end

  // Stimulus
  integer i;
  initial begin
    // Init
    rstn = 0;
    en   = 0;

    // Hold reset for a few cycles
    repeat (4) @(negedge clk);
    rstn = 1;
    @(negedge clk);

    // After reset, q should be 0
    if (q !== {WIDTH{1'b0}}) begin
      $error("[TB] ERROR: q not zero after reset: %0d", q);
    end

    // Enable and count 5 cycles
    en = 1;
    for (i = 0; i < 5; i = i + 1) @(negedge clk);
    if (q !== 8'd5) begin
      $error("[TB] ERROR: expected q=5, got %0d", q);
    end

    // Disable and ensure it holds
    en = 0;
    @(negedge clk);
    if (q !== 8'd5) begin
      $error("[TB] ERROR: q changed while en=0, got %0d", q);
    end
    repeat (2) @(negedge clk);
    if (q !== 8'd5) begin
      $display("[TB] ERROR: q changed while en=0, got %0d", q);
    end

    // Re-enable and count 2 more cycles
    en = 1;
    repeat (2) @(negedge clk);
    if (q !== 8'd7) begin
      $error("[TB] ERROR: expected q=7 after re-enable, got %0d", q);
    end

    $display("[TB] PASS");
    #20;
    $finish;
  end

endmodule

