// Simple 4-tap Q15 FIR core
// - Signed 16-bit input/output (Q1.15)
// - 4 signed 16-bit coefficients
// - Single-cycle combinational MAC, registered output
// - No saturation (simple truncation with rounding)

module fir_q15_core (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        en,

    input  wire signed [15:0] x_in,
    output reg  signed [15:0] y_out
);
    // Coefficients (loaded externally)
    reg signed [15:0] h0, h1, h2, h3;
    // Expose simple write interface via tasks

    // Delay line
    reg signed [15:0] d0, d1, d2, d3;

    // Products (33-bit to hold 16x16 + sign)
    wire signed [31:0] p0 = d0 * h0;
    wire signed [31:0] p1 = d1 * h1;
    wire signed [31:0] p2 = d2 * h2;
    wire signed [31:0] p3 = d3 * h3;

    wire signed [33:0] acc = {{2{p0[31]}}, p0} + {{2{p1[31]}}, p1}
                           + {{2{p2[31]}}, p2} + {{2{p3[31]}}, p3};

    // Round and truncate from Q2.30 to Q1.15
    wire signed [31:0] acc_rounded = acc[31:0] + 32'sd16384;  // +0.5 in Q15
    wire signed [15:0] acc_q15 = acc_rounded[31:16];

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            d0 <= 16'sd0; d1 <= 16'sd0; d2 <= 16'sd0; d3 <= 16'sd0;
            h0 <= 16'sd0; h1 <= 16'sd0; h2 <= 16'sd0; h3 <= 16'sd0;
            y_out <= 16'sd0;
        end else if (en) begin
            // Shift register
            d3 <= d2;
            d2 <= d1;
            d1 <= d0;
            d0 <= x_in;

            // Output
            y_out <= acc_q15;
        end
    end

    // Coefficient write helpers (synthesizable via AXI-Lite wrapper)
    task set_coeff(input integer idx, input signed [15:0] val);
        begin
            case (idx)
                0: h0 = val;
                1: h1 = val;
                2: h2 = val;
                3: h3 = val;
                default: ;
            endcase
        end
    endtask

endmodule

