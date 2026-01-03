# RTL Directory - FIR Filter with IRQ

## Files to Create

1. `fir_q15_axil_irq.v` - AXI-Lite wrapper with STATUS.DONE and irq_out
2. `fir_q15_core.v` - FIR computation core (unchanged from Week 7)
3. `test_fir_irq.py` - Cocotb testbench for IRQ behavior
4. `Makefile` - Build and test

## Key Changes from Week 7

### Add STATUS Register (offset 0x004)

```verilog
// STATUS register: bit 0 = DONE
reg status_done;
output wire irq_out;

// Set DONE when processing completes
always @(posedge clk) begin
    if (!resetn)
        status_done <= 1'b0;
    else if (processing_complete)  // Signal from FIR core FSM
        status_done <= 1'b1;
    else if (status_write && wdata[0])  // W1C
        status_done <= 1'b0;
end

// Level-triggered interrupt
assign irq_out = status_done;
```

### Update AXI-Lite Read

```verilog
always @(*) begin
    case (araddr)
        12'h000: rdata = ctrl_reg;
        12'h004: rdata = {31'b0, status_done};  // NEW
        12'h008: rdata = len_reg;
        12'h00C: rdata = coeff[0];
        // ... rest unchanged ...
    endcase
end
```

### Processing Complete Signal

From FIR core state machine:

```verilog
// In fir_q15_core.v
output reg processing_complete;

always @(posedge clk) begin
    case (state)
        // ...
        ST_DONE: begin
            processing_complete <= 1'b1;  // Pulse for 1 cycle
            state <= ST_IDLE;
        end
        default: processing_complete <= 1'b0;
    endcase
end
```

## Cocotb Tests

### test_fir_irq.py

```python
import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, ClockCycles

@cocotb.test()
async def test_irq_on_completion(dut):
    """Test that irq_out asserts when processing completes."""
    # Setup
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())

    # Reset
    dut.resetn.value = 0
    await ClockCycles(dut.clk, 5)
    dut.resetn.value = 1

    # Configure coefficients
    await write_axil(dut, 0x00C, 0x2000)  # COEFF[0] = 0.25
    await write_axil(dut, 0x010, 0x4000)  # COEFF[1] = 0.5
    await write_axil(dut, 0x014, 0x4000)  # COEFF[2] = 0.5
    await write_axil(dut, 0x018, 0x2000)  # COEFF[3] = 0.25

    # Write input data
    await write_axil(dut, 0x100, 0x1000)  # IN[0]
    await write_axil(dut, 0x104, 0x2000)  # IN[1]
    # ...

    # Set length
    await write_axil(dut, 0x008, 32)

    # Start processing
    await write_axil(dut, 0x000, 0x01)  # CTRL.START = 1

    # Wait for completion (irq_out should assert)
    timeout = 1000
    for _ in range(timeout):
        await RisingEdge(dut.clk)
        if dut.irq_out.value == 1:
            break
    else:
        assert False, "IRQ never asserted"

    # Read STATUS register
    status = await read_axil(dut, 0x004)
    assert status & 0x01, "STATUS.DONE not set"

    # Clear STATUS.DONE (W1C)
    await write_axil(dut, 0x004, 0x01)

    # Check irq_out deasserted
    await ClockCycles(dut.clk, 2)
    assert dut.irq_out.value == 0, "IRQ not cleared after W1C"

    # Read STATUS again (should be 0)
    status = await read_axil(dut, 0x004)
    assert (status & 0x01) == 0, "STATUS.DONE not cleared"

async def write_axil(dut, addr, data):
    """Write via AXI-Lite."""
    dut.awvalid.value = 1
    dut.awaddr.value = addr
    dut.wvalid.value = 1
    dut.wdata.value = data
    dut.wstrb.value = 0xF
    dut.bready.value = 1

    await RisingEdge(dut.clk)
    while not (dut.awready.value and dut.wready.value):
        await RisingEdge(dut.clk)

    dut.awvalid.value = 0
    dut.wvalid.value = 0

    # Wait for bresp
    while not dut.bvalid.value:
        await RisingEdge(dut.clk)

    dut.bready.value = 0

async def read_axil(dut, addr):
    """Read via AXI-Lite."""
    dut.arvalid.value = 1
    dut.araddr.value = addr
    dut.rready.value = 1

    await RisingEdge(dut.clk)
    while not dut.arready.value:
        await RisingEdge(dut.clk)

    dut.arvalid.value = 0

    # Wait for rvalid
    while not dut.rvalid.value:
        await RisingEdge(dut.clk)

    data = int(dut.rdata.value)
    dut.rready.value = 0

    return data
```

Run:
```bash
make test
# Check VCD: irq_out asserts after processing, clears on W1C
```

## Build

```bash
make clean
make test
# Should see: all IRQ tests pass
```

## VCD Signals to Check

- `status_done` - Should set on completion, clear on W1C
- `irq_out` - Should mirror `status_done`
- `processing_complete` - Pulse from FIR core when done
- AXI-Lite transactions for STATUS read/write
# RTL

- Based on Week 7 FIR with Q15 data and vector I/O.
- Adds STATUS.DONE (bit 0, RO/W1C) and level‑high `irq_out` while DONE is set.
- No other changes to the datapath or register layout.
