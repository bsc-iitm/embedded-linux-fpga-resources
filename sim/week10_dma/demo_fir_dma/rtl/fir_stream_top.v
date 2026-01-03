// Top-level: AXI-Lite config + AXI-Stream FIR
// AXI-Lite (32-bit addr/data) with basic single-beat transactions

module fir_stream_top (
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
    output wire [1:0]  s_axil_rresp,

    // AXI Stream slave (input)
    input  wire        s_axis_tvalid,
    output wire        s_axis_tready,
    input  wire [15:0] s_axis_tdata,
    input  wire        s_axis_tlast,

    // AXI Stream master (output)
    output wire        m_axis_tvalid,
    input  wire        m_axis_tready,
    output wire [15:0] m_axis_tdata,
    output wire        m_axis_tlast
);
    // AXI-Lite responses
    assign s_axil_bresp = 2'b00;
    assign s_axil_rresp = 2'b00;

    // Register map
    localparam ADDR_CTRL    = 12'h000;
    localparam ADDR_STATUS  = 12'h004;
    localparam ADDR_LEN     = 12'h008;
    localparam ADDR_COEFF0  = 12'h010;
    localparam ADDR_COEFF1  = 12'h014;
    localparam ADDR_COEFF2  = 12'h018;
    localparam ADDR_COEFF3  = 12'h01C;

    // Registers
    reg        reg_en;
    reg [31:0] reg_status;
    reg [15:0] reg_len;
    reg [15:0] reg_coeff0, reg_coeff1, reg_coeff2, reg_coeff3;

    // AXI-Lite write
    reg [31:0] awaddr_r;
    reg        aw_hs, w_hs;
    wire       wr_fire = aw_hs && w_hs;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            s_axil_awready <= 1'b0;
            s_axil_wready  <= 1'b0;
            s_axil_bvalid  <= 1'b0;
            awaddr_r       <= 32'd0;
            aw_hs          <= 1'b0;
            w_hs           <= 1'b0;
        end else begin
            // AW channel
            if (!s_axil_awready && s_axil_awvalid) begin
                s_axil_awready <= 1'b1;
                awaddr_r <= s_axil_awaddr;
                aw_hs    <= 1'b1;
            end else if (wr_fire) begin
                s_axil_awready <= 1'b0;
                aw_hs <= 1'b0;
            end
            // W channel
            if (!s_axil_wready && s_axil_wvalid) begin
                s_axil_wready <= 1'b1;
                w_hs <= 1'b1;
            end else if (wr_fire) begin
                s_axil_wready <= 1'b0;
                w_hs <= 1'b0;
            end
            // B channel
            if (wr_fire && !s_axil_bvalid) begin
                s_axil_bvalid <= 1'b1;
            end else if (s_axil_bvalid && s_axil_bready) begin
                s_axil_bvalid <= 1'b0;
            end
        end
    end

    wire [11:0] waddr = awaddr_r[11:0];
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            reg_en     <= 1'b1;
            reg_status <= 32'd0;
            reg_len    <= 16'd32;
            reg_coeff0 <= 16'd0;
            reg_coeff1 <= 16'd0;
            reg_coeff2 <= 16'd0;
            reg_coeff3 <= 16'd0;
        end else if (wr_fire) begin
            case (waddr)
                ADDR_CTRL:   reg_en <= s_axil_wdata[0];
                ADDR_STATUS: reg_status <= reg_status & ~s_axil_wdata; // W1C if ever used
                ADDR_LEN:    reg_len <= s_axil_wdata[15:0];
                ADDR_COEFF0: reg_coeff0 <= s_axil_wdata[15:0];
                ADDR_COEFF1: reg_coeff1 <= s_axil_wdata[15:0];
                ADDR_COEFF2: reg_coeff2 <= s_axil_wdata[15:0];
                ADDR_COEFF3: reg_coeff3 <= s_axil_wdata[15:0];
                default: ;
            endcase
        end
    end

    // AXI-Lite read
    reg [31:0] araddr_r;
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            s_axil_arready <= 1'b0;
            s_axil_rvalid  <= 1'b0;
            s_axil_rdata   <= 32'd0;
            araddr_r       <= 32'd0;
        end else begin
            if (!s_axil_arready && s_axil_arvalid) begin
                s_axil_arready <= 1'b1;
                araddr_r <= s_axil_araddr;
            end else begin
                s_axil_arready <= 1'b0;
            end

            if (s_axil_arready && s_axil_arvalid && !s_axil_rvalid) begin
                case (araddr_r[11:0])
                    ADDR_CTRL:   s_axil_rdata <= {31'd0, reg_en};
                    ADDR_STATUS: s_axil_rdata <= reg_status;
                    ADDR_LEN:    s_axil_rdata <= {16'd0, reg_len};
                    ADDR_COEFF0: s_axil_rdata <= {16'd0, reg_coeff0};
                    ADDR_COEFF1: s_axil_rdata <= {16'd0, reg_coeff1};
                    ADDR_COEFF2: s_axil_rdata <= {16'd0, reg_coeff2};
                    ADDR_COEFF3: s_axil_rdata <= {16'd0, reg_coeff3};
                    default:     s_axil_rdata <= 32'hDEADBEEF;
                endcase
                s_axil_rvalid <= 1'b1;
            end else if (s_axil_rvalid && s_axil_rready) begin
                s_axil_rvalid <= 1'b0;
            end
        end
    end

    // FIR stream instance
    fir_stream u_stream (
        .clk           (clk),
        .rst_n         (rst_n),
        .en            (reg_en),
        .s_axis_tvalid (s_axis_tvalid),
        .s_axis_tready (s_axis_tready),
        .s_axis_tdata  (s_axis_tdata),
        .s_axis_tlast  (s_axis_tlast),
        .m_axis_tvalid (m_axis_tvalid),
        .m_axis_tready (m_axis_tready),
        .m_axis_tdata  (m_axis_tdata),
        .m_axis_tlast  (m_axis_tlast),
        .coeff0        (reg_coeff0),
        .coeff1        (reg_coeff1),
        .coeff2        (reg_coeff2),
        .coeff3        (reg_coeff3)
    );

endmodule

