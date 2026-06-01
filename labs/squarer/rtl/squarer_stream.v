// Squarer with AXI Stream interface (for DMA)
// Input: 16-bit signed samples via AXI Stream slave
// Output: 32-bit signed results via AXI Stream master
// Single-cycle throughput (1 sample per clock when no backpressure)

module squarer_stream (
    input  wire        clk,
    input  wire        rst_n,

    // AXI Stream slave (input) - 16-bit
    input  wire        s_axis_tvalid,
    output wire        s_axis_tready,
    input  wire [15:0] s_axis_tdata,
    input  wire        s_axis_tlast,

    // AXI Stream master (output) - 32-bit
    output reg         m_axis_tvalid,
    input  wire        m_axis_tready,
    output reg  [31:0] m_axis_tdata,
    output reg         m_axis_tlast
);

    // Ready to accept when output can be sent
    assign s_axis_tready = m_axis_tready || !m_axis_tvalid;

    // Input handshake
    wire accept = s_axis_tvalid && s_axis_tready;

    // Compute x * x (signed)
    wire signed [15:0] x_in = s_axis_tdata;
    wire signed [31:0] x_squared = x_in * x_in;

    // Pipeline register for output
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            m_axis_tvalid <= 1'b0;
            m_axis_tdata  <= 32'd0;
            m_axis_tlast  <= 1'b0;
        end else begin
            if (accept) begin
                m_axis_tvalid <= 1'b1;
                m_axis_tdata  <= x_squared;
                m_axis_tlast  <= s_axis_tlast;
            end else if (m_axis_tvalid && m_axis_tready) begin
                m_axis_tvalid <= 1'b0;
                m_axis_tlast  <= 1'b0;
            end
        end
    end

endmodule
