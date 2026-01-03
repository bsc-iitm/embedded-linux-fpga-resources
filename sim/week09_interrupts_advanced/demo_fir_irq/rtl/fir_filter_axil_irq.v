// Week 9: FIR filter with DONE interrupt (copy from Week 7 + irq_out)
`timescale 1ns/1ps

module fir_filter_axil_irq #(
  parameter NUM_TAPS = 4,
  parameter MAX_SAMPLES = 32,
  parameter DATA_WIDTH = 16
)(
  input wire aclk,
  input wire aresetn,

  input  wire        saxi_awvalid,
  output wire        saxi_awready,
  input  wire [31:0] saxi_awaddr,

  input  wire        saxi_wvalid,
  output wire        saxi_wready,
  input  wire [31:0] saxi_wdata,
  input  wire  [3:0] saxi_wstrb,

  output reg         saxi_bvalid,
  input  wire        saxi_bready,
  output wire  [1:0] saxi_bresp,

  input  wire        saxi_arvalid,
  output wire        saxi_arready,
  input  wire [31:0] saxi_araddr,

  output reg         saxi_rvalid,
  input  wire        saxi_rready,
  output reg  [31:0] saxi_rdata,
  output wire  [1:0] saxi_rresp,

  output wire        irq_out
);

  localparam ADDR_CTRL    = 12'h000;
  localparam ADDR_STATUS  = 12'h004;
  localparam ADDR_LEN     = 12'h008;
  localparam ADDR_COEFF0  = 12'h010;
  localparam ADDR_COEFF1  = 12'h014;
  localparam ADDR_COEFF2  = 12'h018;
  localparam ADDR_COEFF3  = 12'h01C;
  localparam ADDR_DATA_IN = 12'h100;
  localparam ADDR_DATA_OUT= 12'h200;

  reg        ctrl_en;
  reg        ctrl_start;
  reg        ctrl_reset;
  reg        status_done;
  reg        status_ready;
  reg  [4:0] len;

  reg signed [DATA_WIDTH-1:0] coeff [0:NUM_TAPS-1];
  reg signed [DATA_WIDTH-1:0] data_in [0:MAX_SAMPLES-1];
  reg signed [DATA_WIDTH-1:0] data_out [0:MAX_SAMPLES-1];

  localparam IDLE       = 2'd0;
  localparam PROCESSING = 2'd1;
  localparam DONE       = 2'd2;
  reg [1:0] fir_state;
  reg [4:0] sample_idx;

  reg        aw_received;
  reg [11:0] write_addr;
  wire       write_valid = saxi_awvalid & saxi_wvalid & !aw_received;

  assign saxi_awready = !aw_received;
  assign saxi_wready  = !aw_received;
  assign saxi_bresp   = 2'b00;

  always @(posedge aclk) begin
    if (!aresetn) begin
      aw_received <= 1'b0;
      saxi_bvalid <= 1'b0;
      ctrl_en     <= 1'b0;
      ctrl_start  <= 1'b0;
      ctrl_reset  <= 1'b0;
      len         <= 5'd0;
      coeff[0]    <= 16'sd8192;
      coeff[1]    <= 16'sd8192;
      coeff[2]    <= 16'sd8192;
      coeff[3]    <= 16'sd8192;
      status_done <= 1'b0;
    end else begin
      ctrl_start <= 1'b0;
      ctrl_reset <= 1'b0;

      if (write_valid) begin
        aw_received <= 1'b1;
        write_addr  <= saxi_awaddr[11:0];

        case (saxi_awaddr[11:0])
          ADDR_CTRL: begin
            ctrl_en    <= saxi_wdata[0];
            ctrl_start <= saxi_wdata[1];
            ctrl_reset <= saxi_wdata[2];
          end
          ADDR_STATUS: begin
            if (saxi_wdata[0]) status_done <= 1'b0; // W1C
          end
          ADDR_LEN: begin
            len <= saxi_wdata[4:0];
          end
          ADDR_COEFF0: coeff[0] <= saxi_wdata[DATA_WIDTH-1:0];
          ADDR_COEFF1: coeff[1] <= saxi_wdata[DATA_WIDTH-1:0];
          ADDR_COEFF2: coeff[2] <= saxi_wdata[DATA_WIDTH-1:0];
          ADDR_COEFF3: coeff[3] <= saxi_wdata[DATA_WIDTH-1:0];
          default: begin
            if (saxi_awaddr[11:8] == 4'h1) begin
              data_in[saxi_awaddr[6:2]] <= saxi_wdata[DATA_WIDTH-1:0];
            end
          end
        endcase
      end

      if (saxi_bvalid && saxi_bready) begin
        saxi_bvalid <= 1'b0;
        aw_received <= 1'b0;
      end

      if (fir_state == DONE)
        status_done <= 1'b1;

      if (write_valid)
        saxi_bvalid <= 1'b1;
    end
  end

  reg        ar_received;
  reg [11:0] read_addr;
  wire       read_valid = saxi_arvalid & !ar_received;

  assign saxi_arready = !ar_received;
  assign saxi_rresp   = 2'b00;

  always @(posedge aclk) begin
    if (!aresetn) begin
      ar_received <= 1'b0;
      saxi_rvalid <= 1'b0;
      saxi_rdata  <= 32'd0;
    end else begin
      if (read_valid) begin
        ar_received <= 1'b1;
        read_addr   <= saxi_araddr[11:0];
      end

      if (ar_received && !saxi_rvalid) begin
        saxi_rvalid <= 1'b1;
        case (read_addr)
          ADDR_CTRL:   saxi_rdata <= {29'd0, 1'b0, 1'b0, ctrl_en};
          ADDR_STATUS: saxi_rdata <= {30'd0, status_ready, status_done};
          ADDR_LEN:    saxi_rdata <= {27'd0, len};
          ADDR_COEFF0: saxi_rdata <= {{(32-DATA_WIDTH){coeff[0][DATA_WIDTH-1]}}, coeff[0]};
          ADDR_COEFF1: saxi_rdata <= {{(32-DATA_WIDTH){coeff[1][DATA_WIDTH-1]}}, coeff[1]};
          ADDR_COEFF2: saxi_rdata <= {{(32-DATA_WIDTH){coeff[2][DATA_WIDTH-1]}}, coeff[2]};
          ADDR_COEFF3: saxi_rdata <= {{(32-DATA_WIDTH){coeff[3][DATA_WIDTH-1]}}, coeff[3]};
          default: begin
            if (read_addr[11:8] == 4'h2)
              saxi_rdata <= {{(32-DATA_WIDTH){data_out[read_addr[6:2]][DATA_WIDTH-1]}}, data_out[read_addr[6:2]]};
            else
              saxi_rdata <= 32'd0;
          end
        endcase
      end

      if (saxi_rvalid && saxi_rready) begin
        saxi_rvalid <= 1'b0;
        ar_received <= 1'b0;
      end
    end
  end

  reg signed [31:0] acc;
  integer init_idx;
  integer tap_idx;

  always @(posedge aclk) begin
    if (!aresetn || ctrl_reset) begin
      fir_state    <= IDLE;
      sample_idx   <= 5'd0;
      status_ready <= 1'b1;
      for (init_idx = 0; init_idx < MAX_SAMPLES; init_idx = init_idx + 1)
        data_out[init_idx] <= 16'sd0;
    end else begin
      case (fir_state)
        IDLE: begin
          status_ready <= 1'b1;
          if (ctrl_en && ctrl_start && len > 0) begin
            fir_state    <= PROCESSING;
            sample_idx   <= 5'd0;
            status_ready <= 1'b0;
          end
        end
        PROCESSING: begin
          if (sample_idx < len) begin
            acc = 32'sd0;
            for (tap_idx = 0; tap_idx < NUM_TAPS; tap_idx = tap_idx + 1) begin
              if (sample_idx >= tap_idx[4:0])
                acc = acc + (coeff[tap_idx] * data_in[sample_idx - tap_idx[4:0]]);
            end
            data_out[sample_idx] <= acc[30:15];
            sample_idx <= sample_idx + 1;
          end else begin
            fir_state <= DONE;
          end
        end
        DONE: begin
          fir_state    <= IDLE;
          status_ready <= 1'b1;
        end
        default: fir_state <= IDLE;
      endcase
    end
  end

  assign irq_out = status_done;

endmodule

