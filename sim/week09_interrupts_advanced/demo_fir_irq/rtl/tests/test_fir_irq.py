"""Cocotb tests for Week 9 FIR with IRQ (fir_filter_axil_irq).

Focus:
- STATUS.DONE set on completion and clears on W1C
- irq_out level follows STATUS.DONE
- Minimal compute check for 1-sample flow
"""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge
from cocotbext.axi import AxiLiteBus, AxiLiteMaster

CLK_PERIOD_NS = 10


async def reset(dut):
    dut.aresetn.value = 0
    for _ in range(5):
        await RisingEdge(dut.aclk)
    dut.aresetn.value = 1
    for _ in range(2):
        await RisingEdge(dut.aclk)


def mk_axil_master(dut) -> AxiLiteMaster:
    bus = AxiLiteBus.from_prefix(dut, "saxi")
    return AxiLiteMaster(bus, dut.aclk, dut.aresetn, reset_active_level=False)


async def read32(axil: AxiLiteMaster, addr: int) -> int:
    r = await axil.read(addr, 4)
    return int.from_bytes(r.data, byteorder="little")


async def write32(axil: AxiLiteMaster, addr: int, value: int):
    await axil.write(addr, value.to_bytes(4, "little"))


@cocotb.test
async def test_done_and_irq_behavior(dut):
    """DONE and irq_out assert/deassert; DONE clears via W1C."""
    cocotb.start_soon(Clock(dut.aclk, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Configure simple 1-sample operation with coeff0 = 0.5 (Q15 0x4000)
    await write32(axil, 0x10, 0x4000)   # COEFF0
    await write32(axil, 0x14, 0x0000)
    await write32(axil, 0x18, 0x0000)
    await write32(axil, 0x1C, 0x0000)
    await write32(axil, 0x100, 0x1000)  # DATA_IN[0] = 0x1000
    await write32(axil, 0x08, 1)        # LEN = 1

    # Enable + START
    await write32(axil, 0x00, 0x3)

    # Wait for DONE to set and irq_out to assert
    for _ in range(200):
        status = await read32(axil, 0x04)
        if status & 0x1:
            break
        await RisingEdge(dut.aclk)

    status = await read32(axil, 0x04)
    assert (status & 0x1) == 1, "STATUS.DONE should be set"
    assert int(dut.irq_out.value) == 1, "irq_out should be asserted when DONE is set"

    # Clear DONE via W1C; irq_out should deassert
    await write32(axil, 0x04, 0x1)
    await RisingEdge(dut.aclk)
    status = await read32(axil, 0x04)
    assert (status & 0x1) == 0, "STATUS.DONE should clear on W1C"
    assert int(dut.irq_out.value) == 0, "irq_out should deassert after DONE clears"


@cocotb.test
async def test_single_sample_result(dut):
    """Verify output for a single sample pass with coeff0 = 1.0 (Q15 0x7FFF)."""
    cocotb.start_soon(Clock(dut.aclk, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # coeff0=1.0, others 0
    await write32(axil, 0x10, 0x7FFF)  # ~1.0 in Q15
    await write32(axil, 0x14, 0x0000)
    await write32(axil, 0x18, 0x0000)
    await write32(axil, 0x1C, 0x0000)
    await write32(axil, 0x100, 0x1234)  # DATA_IN[0]
    await write32(axil, 0x08, 1)        # LEN = 1

    # Enable + START
    await write32(axil, 0x00, 0x3)

    # Wait for DONE
    for _ in range(200):
        status = await read32(axil, 0x04)
        if status & 0x1:
            break
        await RisingEdge(dut.aclk)

    # Read output sample 0 (lower 16 bits valid)
    y0 = await read32(axil, 0x200)
    y0 &= 0xFFFF

    # For coeff0 ~1.0, expect y0 ~= x0 after Q15 scale (saturation not triggered here)
    assert y0 == 0x1233, f"Expected 0x1233 (or something close), got 0x{y0:04x}"

