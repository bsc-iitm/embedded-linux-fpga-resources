"""Cocotb tests for 4-tap FIR filter with AXI-Lite interface.

Tests register access, coefficient loading, and FIR computation.
"""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, Timer
from cocotbext.axi import AxiLiteBus, AxiLiteMaster


CLK_PERIOD_NS = 10


async def reset(dut):
    """Apply and release active-low reset."""
    dut.aresetn.value = 0
    for _ in range(5):
        await RisingEdge(dut.aclk)
    dut.aresetn.value = 1
    for _ in range(2):
        await RisingEdge(dut.aclk)


def mk_axil_master(dut) -> AxiLiteMaster:
    """Create AXI-Lite master from bus signals."""
    bus = AxiLiteBus.from_prefix(dut, "saxi")
    return AxiLiteMaster(bus, dut.aclk, dut.aresetn, reset_active_level=False)


async def read32(axil, addr: int) -> int:
    """Read 32-bit value from AXI-Lite."""
    r = await axil.read(addr, 4)
    return int.from_bytes(r.data, byteorder="little")


async def write32(axil, addr: int, value: int):
    """Write 32-bit value to AXI-Lite."""
    await axil.write(addr, value.to_bytes(4, "little"))


def signed16(val):
    """Convert to signed 16-bit integer."""
    if val & 0x8000:
        return val - 0x10000
    return val


@cocotb.test
async def test_register_readback(dut):
    """Test basic register read/write."""
    cocotb.start_soon(Clock(dut.aclk, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Test CTRL register
    await write32(axil, 0x00, 0x1)  # Set EN
    ctrl = await read32(axil, 0x00)
    assert (ctrl & 0x1) == 0x1, "CTRL.EN should be set"

    # Test LEN register
    await write32(axil, 0x08, 16)
    len_val = await read32(axil, 0x08)
    assert len_val == 16, f"LEN should be 16, got {len_val}"

    # Test coefficient registers
    await write32(axil, 0x10, 0x2000)  # COEFF0
    await write32(axil, 0x14, 0x4000)  # COEFF1
    coeff0 = await read32(axil, 0x10)
    coeff1 = await read32(axil, 0x14)
    assert (coeff0 & 0xFFFF) == 0x2000, f"COEFF0 mismatch: {coeff0:04x}"
    assert (coeff1 & 0xFFFF) == 0x4000, f"COEFF1 mismatch: {coeff1:04x}"


@cocotb.test
async def test_w1p_signals(dut):
    """Test that START and RESET are write-1-pulse (self-clearing)."""
    cocotb.start_soon(Clock(dut.aclk, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Write START bit
    await write32(axil, 0x00, 0x2)  # bit1=START
    await RisingEdge(dut.aclk)
    ctrl = await read32(axil, 0x00)
    assert (ctrl & 0x2) == 0, "START should be self-clearing (read as 0)"

    # Write RESET bit
    await write32(axil, 0x00, 0x4)  # bit2=RESET
    await RisingEdge(dut.aclk)
    ctrl = await read32(axil, 0x00)
    assert (ctrl & 0x4) == 0, "RESET should be self-clearing (read as 0)"


@cocotb.test
async def test_status_done_w1c(dut):
    """Test STATUS.DONE is write-1-clear."""
    cocotb.start_soon(Clock(dut.aclk, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Set up simple filter: 1 sample
    await write32(axil, 0x10, 0x4000)  # COEFF0 = 0.5 in Q15
    await write32(axil, 0x14, 0x0000)
    await write32(axil, 0x18, 0x0000)
    await write32(axil, 0x1C, 0x0000)

    await write32(axil, 0x100, 0x1000)  # DATA_IN[0] = 0x1000
    await write32(axil, 0x08, 1)        # LEN = 1

    # Start processing
    await write32(axil, 0x00, 0x3)  # EN=1, START=1

    # Wait for DONE
    for _ in range(100):
        status = await read32(axil, 0x04)
        if status & 0x1:  # DONE bit
            break
        await RisingEdge(dut.aclk)

    status = await read32(axil, 0x04)
    assert (status & 0x1) == 0x1, "STATUS.DONE should be set after processing"

    # Clear DONE with W1C
    await write32(axil, 0x04, 0x1)
    status = await read32(axil, 0x04)
    assert (status & 0x1) == 0, "STATUS.DONE should clear on W1C write"


@cocotb.test
async def test_simple_averaging_filter(dut):
    """Test 4-tap averaging filter: coeffs = [0.25, 0.25, 0.25, 0.25]."""
    cocotb.start_soon(Clock(dut.aclk, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Set coefficients for averaging: 0.25 in Q15 = 8192 (0x2000)
    for i in range(4):
        await write32(axil, 0x10 + i*4, 0x2000)

    # Input: ramp pattern [4, 8, 12, 16, 20]
    test_data = [0x0004, 0x0008, 0x000C, 0x0010, 0x0014]
    for i, val in enumerate(test_data):
        await write32(axil, 0x100 + i*4, val)

    await write32(axil, 0x08, len(test_data))  # LEN

    # Start processing
    await write32(axil, 0x00, 0x3)  # EN=1, START=1

    # Wait for DONE
    done = False
    for _ in range(200):
        status = await read32(axil, 0x04)
        if status & 0x1:
            done = True
            break
        await RisingEdge(dut.aclk)

    assert done, "Processing should complete with DONE=1"

    # Read results and verify
    # Expected results for 4-tap averaging with zero-padding at start:
    # y[0] = (4*0.25) = 1
    # y[1] = (4+8)*0.25 = 3
    # y[2] = (4+8+12)*0.25 = 6
    # y[3] = (4+8+12+16)*0.25 = 10
    # y[4] = (8+12+16+20)*0.25 = 14

    expected = [1, 3, 6, 10, 14]

    dut._log.info("Reading FIR output:")
    for i in range(len(test_data)):
        result = await read32(axil, 0x200 + i*4)
        result_s16 = signed16(result & 0xFFFF)
        dut._log.info(f"  data_out[{i}] = {result_s16} (expected ~{expected[i]})")
        # Allow small rounding error
        assert abs(result_s16 - expected[i]) <= 1, f"Output {i} mismatch: {result_s16} vs {expected[i]}"


@cocotb.test
async def test_single_tap_passthrough(dut):
    """Test single-tap filter: coeff[0]=1.0, others=0 (passthrough)."""
    cocotb.start_soon(Clock(dut.aclk, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Coefficients: [1.0, 0, 0, 0] in Q15
    await write32(axil, 0x10, 0x7FFF)  # COEFF0 = ~1.0 (max Q15)
    await write32(axil, 0x14, 0x0000)
    await write32(axil, 0x18, 0x0000)
    await write32(axil, 0x1C, 0x0000)

    # Input: [100, 200, 300]
    test_data = [100, 200, 300]
    for i, val in enumerate(test_data):
        await write32(axil, 0x100 + i*4, val)

    await write32(axil, 0x08, len(test_data))

    # Process
    await write32(axil, 0x00, 0x3)

    # Wait for completion
    for _ in range(200):
        status = await read32(axil, 0x04)
        if status & 0x1:
            break
        await RisingEdge(dut.aclk)

    # Output should be approximately input (within Q15 precision)
    for i, expected in enumerate(test_data):
        result = await read32(axil, 0x200 + i*4)
        result_s16 = signed16(result & 0xFFFF)
        dut._log.info(f"Passthrough: in={expected}, out={result_s16}")
        # 0x7FFF/0x8000 ≈ 0.9999, so output slightly less than input
        assert abs(result_s16 - expected) <= 1, f"Passthrough failed at {i}"


@cocotb.test
async def test_data_array_boundary(dut):
    """Test writing and reading at array boundaries."""
    cocotb.start_soon(Clock(dut.aclk, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Write to first and last positions of DATA_IN
    await write32(axil, 0x100, 0xAAAA)  # DATA_IN[0]
    await write32(axil, 0x17C, 0x5555)  # DATA_IN[31]

    # Readback via processing (passthrough filter)
    await write32(axil, 0x10, 0x7FFF)
    for i in range(1, 4):
        await write32(axil, 0x10 + i*4, 0)

    await write32(axil, 0x08, 32)  # Process all 32 samples
    await write32(axil, 0x00, 0x3)

    # Wait for done
    for _ in range(500):
        status = await read32(axil, 0x04)
        if status & 0x1:
            break
        await RisingEdge(dut.aclk)

    # Check boundaries
    out0 = await read32(axil, 0x200)
    out31 = await read32(axil, 0x27C)

    dut._log.info(f"Boundary test: out[0]={out0:04x}, out[31]={out31:04x}")
    assert (out0 & 0xFFFF) != 0, "Output[0] should be non-zero"
    assert (out31 & 0xFFFF) != 0, "Output[31] should be non-zero"
