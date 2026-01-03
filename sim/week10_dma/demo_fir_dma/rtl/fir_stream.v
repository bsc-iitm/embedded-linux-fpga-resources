// AXI-Stream wrapper around fir_q15_core
// - s_axis: slave input stream (tdata[15:0], tvalid/tready, tlast)
// - m_axis: master output stream (tdata[15:0], tvalid/tready, tlast)
// - Simple flow: accept when both valid/ready; propagate TLAST with one-cycle latency

module fir_stream (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        en,

    // AXI Stream slave (input)
    input  wire        s_axis_tvalid,
    output wire        s_axis_tready,
    input  wire [15:0] s_axis_tdata,
    input  wire        s_axis_tlast,

    // AXI Stream master (output)
    output reg         m_axis_tvalid,
    input  wire        m_axis_tready,
    output reg  [15:0] m_axis_tdata,
    output reg         m_axis_tlast,

    // Coefficients
    input  wire [15:0] coeff0,
    input  wire [15:0] coeff1,
    input  wire [15:0] coeff2,
    input  wire [15:0] coeff3
);
    // Ready when enabled and output can be accepted next cycle
    assign s_axis_tready = en && (m_axis_tready || !m_axis_tvalid);

    // FIR core
    wire signed [15:0] y_out;
    reg  signed [15:0] coeff0_r, coeff1_r, coeff2_r, coeff3_r;
    reg  signed [15:0] x_in_r;
    reg                last_r;

    // Coefficients registered for timing
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            coeff0_r <= 16'sd0;
            coeff1_r <= 16'sd0;
            coeff2_r <= 16'sd0;
            coeff3_r <= 16'sd0;
        end else begin
            coeff0_r <= coeff0;
            coeff1_r <= coeff1;
            coeff2_r <= coeff2;
            coeff3_r <= coeff3;
        end
    end

    // Drive core input when accepting a sample
    wire accept = s_axis_tvalid && s_axis_tready;
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            x_in_r <= 16'sd0;
            last_r <= 1'b0;
        end else if (accept) begin
            x_in_r <= s_axis_tdata;
            last_r <= s_axis_tlast;
        end
    end

    fir_q15_core u_core (
        .clk   (clk),
        .rst_n (rst_n),
        .en    (en && accept),
        .x_in  (x_in_r),
        .y_out (y_out)
    );

    // Output stage: one-cycle latency from input accept
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            m_axis_tvalid <= 1'b0;
            m_axis_tdata  <= 16'd0;
            m_axis_tlast  <= 1'b0;
        end else begin
            if (accept) begin
                m_axis_tvalid <= 1'b1;
                m_axis_tdata  <= y_out;
                m_axis_tlast  <= last_r;
            end else if (m_axis_tvalid && m_axis_tready) begin
                m_axis_tvalid <= 1'b0;
            end
        end
    end

endmodule

