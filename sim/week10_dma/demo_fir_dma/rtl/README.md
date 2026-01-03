# RTL Directory - FIR Filter with AXI Stream Interface

## Purpose

Modify Week 7/9 FIR filter to use AXI Stream for data input/output instead of MMIO registers.

## Files Expected

1. `fir_stream.v` - FIR filter with AXI Stream slave (input) and master (output)
2. `fir_axil_cfg.v` - AXI-Lite interface for configuration (COEFF, LEN)
3. `fir_q15_core.v` - FIR computation core (unchanged from Week 7)
4. `test_fir_stream.py` - Cocotb testbench for AXI Stream integration

## Key Changes from Week 9

### Remove MMIO Data Registers

```verilog
// REMOVED from Week 7/9:
// DATA_IN[0:31]  @ 0x100-0x17C
// DATA_OUT[0:31] @ 0x200-0x27C
// CTRL.START bit (replaced by AXI Stream TVALID)
```

### Add AXI Stream Slave (Input)

```verilog
input  wire        s_axis_tvalid,  // Data valid
output wire        s_axis_tready,  // Ready to accept
input  wire [15:0] s_axis_tdata,   // Q15 sample
input  wire        s_axis_tlast,   // Last sample in frame

// Accept data when both valid and ready
wire stream_handshake = s_axis_tvalid && s_axis_tready;
```

### Add AXI Stream Master (Output)

```verilog
output reg         m_axis_tvalid,  // Output valid
input  wire        m_axis_tready,  // Downstream ready
output reg  [15:0] m_axis_tdata,   // Q15 filtered sample
output reg         m_axis_tlast,   // Last output sample

// Transfer occurs when both valid and ready
wire output_xfer = m_axis_tvalid && m_axis_tready;
```

### AXI Stream Flow Control

```verilog
// FSM states
localparam ST_IDLE    = 2'd0;
localparam ST_RECV    = 2'd1;  // Receiving input stream
localparam ST_PROC    = 2'd2;  // Processing (if needed)
localparam ST_SEND    = 2'd3;  // Sending output stream

always @(posedge clk) begin
    if (!resetn) begin
        state <= ST_IDLE;
        s_axis_tready <= 1'b0;
        m_axis_tvalid <= 1'b0;
    end else begin
        case (state)
            ST_IDLE: begin
                s_axis_tready <= 1'b1;  // Ready to receive
                if (s_axis_tvalid) begin
                    // Start receiving
                    state <= ST_RECV;
                end
            end

            ST_RECV: begin
                if (stream_handshake) begin
                    // Store sample, compute output
                    input_buf[sample_cnt] <= s_axis_tdata;
                    // Compute FIR output (can pipeline)
                    if (s_axis_tlast) begin
                        s_axis_tready <= 1'b0;
                        state <= ST_SEND;
                    end
                end
            end

            ST_SEND: begin
                m_axis_tvalid <= 1'b1;
                m_axis_tdata <= output_buf[out_cnt];
                if (output_xfer) begin
                    out_cnt <= out_cnt + 1;
                    if (out_cnt == len_reg - 1) begin
                        m_axis_tlast <= 1'b1;
                        state <= ST_IDLE;
                    end
                end
            end
        endcase
    end
end
```

## Configuration Registers (AXI-Lite)

Retained from Week 7:

```
Offset  Register      Access   Description
------  ------------  ------   -----------
0x00C   COEFF[0]      RW       Q15 coefficient h[0]
0x010   COEFF[1]      RW       Q15 coefficient h[1]
0x014   COEFF[2]      RW       Q15 coefficient h[2]
0x018   COEFF[3]      RW       Q15 coefficient h[3]
0x008   LEN           RW       Number of samples
```

STATUS and CTRL removed (processing driven by AXI Stream).

## Cocotb Tests

### test_fir_stream.py

```python
import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, ClockCycles

@cocotb.test()
async def test_stream_transfer(dut):
    """Test FIR with AXI Stream input/output."""
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())

    # Reset
    dut.resetn.value = 0
    await ClockCycles(dut.clk, 5)
    dut.resetn.value = 1

    # Configure coefficients via AXI-Lite
    await write_axil(dut, 0x00C, 0x2000)  # COEFF[0]
    await write_axil(dut, 0x010, 0x4000)  # COEFF[1]
    await write_axil(dut, 0x014, 0x4000)  # COEFF[2]
    await write_axil(dut, 0x018, 0x2000)  # COEFF[3]
    await write_axil(dut, 0x008, 8)       # LEN = 8 samples

    # Send input stream
    input_samples = [0x1000, 0x2000, 0x3000, 0x4000,
                     0x5000, 0x6000, 0x7000, 0x7FFF]

    for i, sample in enumerate(input_samples):
        dut.s_axis_tvalid.value = 1
        dut.s_axis_tdata.value = sample
        dut.s_axis_tlast.value = 1 if i == len(input_samples)-1 else 0

        # Wait for tready
        while not dut.s_axis_tready.value:
            await RisingEdge(dut.clk)

        await RisingEdge(dut.clk)

    dut.s_axis_tvalid.value = 0

    # Receive output stream
    dut.m_axis_tready.value = 1
    output_samples = []

    for _ in range(8):
        # Wait for tvalid
        while not dut.m_axis_tvalid.value:
            await RisingEdge(dut.clk)

        output_samples.append(int(dut.m_axis_tdata.value))
        await RisingEdge(dut.clk)

    print(f"Output: {[hex(x) for x in output_samples]}")
    # Verify filtered output matches expected
```

## Build and Test

```bash
make clean
make test
# Check VCD: AXI Stream handshakes, TLAST assertion
```

## VCD Signals to Check

- `s_axis_tvalid`, `s_axis_tready` - Input handshake
- `s_axis_tdata` - Input samples
- `s_axis_tlast` - Frame boundary
- `m_axis_tvalid`, `m_axis_tready` - Output handshake
- `m_axis_tdata` - Filtered samples
- `m_axis_tlast` - Output frame end
- FSM state transitions

## Integration with AXI DMA

AXI DMA connects to FIR via AXI Stream:

```
DMA MM2S ──[AXI Stream]──> FIR s_axis
FIR m_axis ──[AXI Stream]──> DMA S2MM
```

DMA automatically:
- Reads from memory and drives s_axis
- Monitors m_axis and writes to memory
- Handles TLAST for frame boundaries
- Asserts completion IRQ

## Key Differences from MMIO Mode

| Aspect           | MMIO (Week 7/9)         | AXI Stream (Week 10)      |
|------------------|-------------------------|---------------------------|
| Data input       | CPU writes DATA_IN[i]   | DMA drives s_axis         |
| Data output      | CPU reads DATA_OUT[i]   | DMA reads m_axis          |
| Start trigger    | CTRL.START              | s_axis_tvalid assertion   |
| Completion       | STATUS.DONE + IRQ       | DMA S2MM completion IRQ   |
| CPU involvement  | High (every sample)     | Minimal (only setup)      |
| Throughput       | ~4 MB/s                 | ~90 MB/s                  |

## References

- Week 7 FIR filter (MMIO mode)
- AXI Stream protocol (IHI0051A)
- Xilinx AXI DMA datasheet (PG021)
