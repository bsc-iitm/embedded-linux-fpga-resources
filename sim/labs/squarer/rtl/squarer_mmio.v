// Squarer with AXI-Lite interface
// Write 16-bit x to DATA_IN, read 32-bit x*x from DATA_OUT
// Single-cycle computation, result available immediately after write

module squarer_mmio (
    input  wire        clk,
    input  wire        rst_n,

    // AXI-Lite slave (32-bit)
    input  wire        s_axil_awvalid,
    output reg         s_axil_awready,
    input  wire [31:0] s_axil_awaddr,

    input  wire        s_axil_wvalid,
    output reg         s_axil_wready,
    input  wire [31:0] s_axil_wdata,
    input  wire [3:0]  s_axil_wstrb,

    output reg         s_axil_bvalid,
    input  wire        s_axil_bready,
    output wire [1:0]  s_axil_bresp,

    input  wire        s_axil_arvalid,
    output reg         s_axil_arready,
    input  wire [31:0] s_axil_araddr,

    output reg         s_axil_rvalid,
    input  wire        s_axil_rready,
    output reg  [31:0] s_axil_rdata,
    output wire [1:0]  s_axil_rresp
);

    assign s_axil_bresp = 2'b00;  // OKAY
    assign s_axil_rresp = 2'b00;

    // Register addresses
    localparam ADDR_DATA_IN  = 4'h0;  // 0x00: write x (16-bit)
    localparam ADDR_DATA_OUT = 4'h4;  // 0x04: read x*x (32-bit)

    // Data registers
    reg signed [15:0] data_in;
    wire signed [31:0] data_out;

    // Compute x * x (signed multiplication)
    assign data_out = data_in * data_in;

    // AXI-Lite write handling
    reg [31:0] awaddr_r;
    reg aw_done, w_done;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            s_axil_awready <= 1'b0;
            s_axil_wready  <= 1'b0;
            s_axil_bvalid  <= 1'b0;
            aw_done <= 1'b0;
            w_done  <= 1'b0;
            awaddr_r <= 32'd0;
            data_in <= 16'd0;
        end else begin
            // AW channel
            if (s_axil_awvalid && !aw_done) begin
                s_axil_awready <= 1'b1;
                awaddr_r <= s_axil_awaddr;
                aw_done <= 1'b1;
            end else begin
                s_axil_awready <= 1'b0;
            end

            // W channel
            if (s_axil_wvalid && !w_done) begin
                s_axil_wready <= 1'b1;
                w_done <= 1'b1;
            end else begin
                s_axil_wready <= 1'b0;
            end

            // Write to register when both channels complete
            if (aw_done && w_done && !s_axil_bvalid) begin
                if (awaddr_r[3:0] == ADDR_DATA_IN)
                    data_in <= s_axil_wdata[15:0];
                s_axil_bvalid <= 1'b1;
            end

            // B channel
            if (s_axil_bvalid && s_axil_bready) begin
                s_axil_bvalid <= 1'b0;
                aw_done <= 1'b0;
                w_done <= 1'b0;
            end
        end
    end

    // AXI-Lite read handling
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            s_axil_arready <= 1'b0;
            s_axil_rvalid  <= 1'b0;
            s_axil_rdata   <= 32'd0;
        end else begin
            // AR channel
            if (s_axil_arvalid && !s_axil_rvalid) begin
                s_axil_arready <= 1'b1;
                s_axil_rvalid  <= 1'b1;
                case (s_axil_araddr[3:0])
                    ADDR_DATA_IN:  s_axil_rdata <= {16'd0, data_in};
                    ADDR_DATA_OUT: s_axil_rdata <= data_out;
                    default:       s_axil_rdata <= 32'hDEADBEEF;
                endcase
            end else begin
                s_axil_arready <= 1'b0;
            end

            // R channel
            if (s_axil_rvalid && s_axil_rready) begin
                s_axil_rvalid <= 1'b0;
            end
        end
    end

endmodule
