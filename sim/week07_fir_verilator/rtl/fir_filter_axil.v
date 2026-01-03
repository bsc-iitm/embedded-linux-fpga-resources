// Purpose: 4-tap FIR filter with AXI-Lite register interface
// Features:
// - Configurable coefficients via AXI-Lite registers
// - Batch processing of up to 32 samples
// - Q15 fixed-point arithmetic (16-bit signed)
// - Vector mode: write samples to DATA_IN[], read from DATA_OUT[]

module fir_filter_axil #(
  parameter NUM_TAPS = 4,
  parameter MAX_SAMPLES = 32,
  parameter DATA_WIDTH = 16  // Q15 format
)(
  input wire aclk,
  input wire aresetn,

  // AXI-Lite slave interface (lower-case signals for cocotb compatibility)
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
  output wire  [1:0] saxi_rresp
);

  // Register map offsets
  localparam ADDR_CTRL    = 12'h000;
  localparam ADDR_STATUS  = 12'h004;
  localparam ADDR_LEN     = 12'h008;
  localparam ADDR_COEFF0  = 12'h010;
  localparam ADDR_COEFF1  = 12'h014;
  localparam ADDR_COEFF2  = 12'h018;
  localparam ADDR_COEFF3  = 12'h01C;
  localparam ADDR_DATA_IN = 12'h100;  // 32 words @ 0x100-0x17C
  localparam ADDR_DATA_OUT= 12'h200;  // 32 words @ 0x200-0x27C

  // Registers
  reg        ctrl_en;
  reg        ctrl_start;   // W1P (write-1-pulse)
  reg        ctrl_reset;   // W1P
  reg        status_done;  // W1C (write-1-clear)
  reg        status_ready;
  reg  [4:0] len;          // 0-32

  reg signed [DATA_WIDTH-1:0] coeff [0:NUM_TAPS-1];
  reg signed [DATA_WIDTH-1:0] data_in [0:MAX_SAMPLES-1];
  reg signed [DATA_WIDTH-1:0] data_out [0:MAX_SAMPLES-1];

  // FIR processing state machine
  localparam IDLE       = 2'd0;
  localparam PROCESSING = 2'd1;
  localparam DONE       = 2'd2;

  reg [1:0] fir_state;
  reg [4:0] sample_idx;

  // AXI-Lite write channel
  reg        aw_received;
  reg [11:0] write_addr;
  wire       write_valid = saxi_awvalid & saxi_wvalid & !aw_received;

  assign saxi_awready = !aw_received;
  assign saxi_wready  = !aw_received;
  assign saxi_bresp   = 2'b00;  // OKAY

  always @(posedge aclk) begin
    if (!aresetn) begin
      aw_received <= 1'b0;
      saxi_bvalid <= 1'b0;
      ctrl_en     <= 1'b0;
      ctrl_start  <= 1'b0;
      ctrl_reset  <= 1'b0;
      len         <= 5'd0;
      coeff[0]    <= 16'sd8192;  // Default: 0.25 in Q15
      coeff[1]    <= 16'sd8192;
      coeff[2]    <= 16'sd8192;
      coeff[3]    <= 16'sd8192;
    end else begin
      // W1P self-clearing
      ctrl_start <= 1'b0;
      ctrl_reset <= 1'b0;

      // Write transaction
      if (write_valid) begin
        aw_received <= 1'b1;
        write_addr  <= saxi_awaddr[11:0];

        case (saxi_awaddr[11:0])
          ADDR_CTRL: begin
            ctrl_en    <= saxi_wdata[0];
            ctrl_start <= saxi_wdata[1];  // Pulse
            ctrl_reset <= saxi_wdata[2];  // Pulse
          end
          ADDR_STATUS: begin
            // W1C for DONE bit
            if (saxi_wdata[0]) status_done <= 1'b0;
          end
          ADDR_LEN: begin
            len <= saxi_wdata[4:0];
          end
          ADDR_COEFF0: coeff[0] <= saxi_wdata[DATA_WIDTH-1:0];
          ADDR_COEFF1: coeff[1] <= saxi_wdata[DATA_WIDTH-1:0];
          ADDR_COEFF2: coeff[2] <= saxi_wdata[DATA_WIDTH-1:0];
          ADDR_COEFF3: coeff[3] <= saxi_wdata[DATA_WIDTH-1:0];
          default: begin
            // DATA_IN array write
            if (saxi_awaddr[11:8] == 4'h1) begin  // 0x100-0x1FF
              data_in[saxi_awaddr[6:2]] <= saxi_wdata[DATA_WIDTH-1:0];
            end
          end
        endcase
      end

      // Write response channel
      if (aw_received && !saxi_bvalid) begin
        saxi_bvalid <= 1'b1;
      end
      if (saxi_bvalid && saxi_bready) begin
        saxi_bvalid <= 1'b0;
        aw_received <= 1'b0;
      end

      // FIR processing updates status_done
      if (fir_state == DONE) begin
        status_done <= 1'b1;
      end
    end
  end

  // AXI-Lite read channel
  reg        ar_received;
  reg [11:0] read_addr;
  wire       read_valid = saxi_arvalid & !ar_received;

  assign saxi_arready = !ar_received;
  assign saxi_rresp   = 2'b00;  // OKAY

  always @(posedge aclk) begin
    if (!aresetn) begin
      ar_received <= 1'b0;
      saxi_rvalid <= 1'b0;
      saxi_rdata  <= 32'd0;
    end else begin
      // Read address
      if (read_valid) begin
        ar_received <= 1'b1;
        read_addr   <= saxi_araddr[11:0];
      end

      // Read data
      if (ar_received && !saxi_rvalid) begin
        saxi_rvalid <= 1'b1;

        case (read_addr)
          ADDR_CTRL:   saxi_rdata <= {29'd0, 1'b0, 1'b0, ctrl_en};  // W1P reads as 0
          ADDR_STATUS: saxi_rdata <= {30'd0, status_ready, status_done};
          ADDR_LEN:    saxi_rdata <= {27'd0, len};
          ADDR_COEFF0: saxi_rdata <= {{(32-DATA_WIDTH){coeff[0][DATA_WIDTH-1]}}, coeff[0]};
          ADDR_COEFF1: saxi_rdata <= {{(32-DATA_WIDTH){coeff[1][DATA_WIDTH-1]}}, coeff[1]};
          ADDR_COEFF2: saxi_rdata <= {{(32-DATA_WIDTH){coeff[2][DATA_WIDTH-1]}}, coeff[2]};
          ADDR_COEFF3: saxi_rdata <= {{(32-DATA_WIDTH){coeff[3][DATA_WIDTH-1]}}, coeff[3]};
          default: begin
            // DATA_OUT array read
            if (read_addr[11:8] == 4'h2) begin  // 0x200-0x2FF
              saxi_rdata <= {{(32-DATA_WIDTH){data_out[read_addr[6:2]][DATA_WIDTH-1]}}, data_out[read_addr[6:2]]};
            end else begin
              saxi_rdata <= 32'd0;
            end
          end
        endcase
      end

      // Read response
      if (saxi_rvalid && saxi_rready) begin
        saxi_rvalid <= 1'b0;
        ar_received <= 1'b0;
      end
    end
  end

  // FIR computation state machine
  reg signed [31:0] acc;  // Temp Accumulator for MAC operations
  integer init_idx;       // For initialization loop
  integer tap_idx;        // For FIR tap loop

  always @(posedge aclk) begin
    if (!aresetn || ctrl_reset) begin
      fir_state    <= IDLE;
      sample_idx   <= 5'd0;
      status_ready <= 1'b1;
      //acc          <= 32'sd0;
      for (init_idx = 0; init_idx < MAX_SAMPLES; init_idx = init_idx + 1) begin
        data_out[init_idx] <= 16'sd0;
      end
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
            // Compute FIR for current sample
            // y[n] = sum(coeff[k] * x[n-k]) for k=0 to NUM_TAPS-1
            // Handle boundary: x[n-k] = 0 if n-k < 0

            // Simple implementation: accumulate over multiple cycles
            // For teaching simplicity, we compute in one cycle (small tap count)
            acc = 32'sd0;

            for (tap_idx = 0; tap_idx < NUM_TAPS; tap_idx = tap_idx + 1) begin
              if (sample_idx >= tap_idx[4:0]) begin
                // Q15 * Q15 = Q30, shift right by 15 to get Q15
                acc = acc + (coeff[tap_idx] * data_in[sample_idx - tap_idx[4:0]]);
              end
            end

            // Store result (Q30 >> 15 = Q15)
            data_out[sample_idx] <= acc[30:15];  // Extract Q15 result

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

endmodule
