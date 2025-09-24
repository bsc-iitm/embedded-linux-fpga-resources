"""cocotb tests for Smart Timer V1 (PWM) with AXI-Lite.

"""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, Timer
from cocotbext.axi import AxiLiteBus, AxiLiteMaster


CLK_PERIOD_NS = 10


async def reset(dut):
    """Apply and release active-low reset. Keep other signals quiescent."""
    dut.ARESETn.value = 0
    for _ in range(5):
        await RisingEdge(dut.ACLK)
    dut.ARESETn.value = 1
    for _ in range(2):
        await RisingEdge(dut.ACLK)


def mk_axil_master(dut) -> AxiLiteMaster:
    bus = AxiLiteBus.from_prefix(dut, "saxi")
    return AxiLiteMaster(bus, dut.ACLK, dut.ARESETn, reset_active_level=False)


async def read32(axil, addr: int) -> int:
    r = await axil.read(addr, 4)
    return int.from_bytes(r.data, byteorder="little")


@cocotb.test
async def config_run_wrap_and_w1c(dut):
    """Program PERIOD/DUTY, enable, observe WRAP sticky, and clear via W1C."""
    cocotb.start_soon(Clock(dut.ACLK, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Ensure disabled; CTRL.EN=0
    await axil.write(0x0, (0x0).to_bytes(4, "little"))
    status = await read32(axil, 0x0)
    assert status == 0, "Initial value in reg should be 0"

    # PERIOD=9, DUTY=3; commit immediately since EN=0
    await axil.write(0x4, (9).to_bytes(4, "little"))
    await axil.write(0x8, (3).to_bytes(4, "little"))
    period = await read32(axil, 0x4)
    assert period == 9, "Period does not match set value"

    # Enable
    await axil.write(0x0, (0x1).to_bytes(4, "little"))

    # Wait until STATUS.WRAP (bit0) becomes 1, indicating a wrap occurred
    wrap = 0
    for i in range(200):
        status = await read32(axil, 0xC)
        wrap = status & 0x1
        print(f"Cycle {i}")
        if wrap:
            break
        await RisingEdge(dut.ACLK)
    assert wrap == 1, "Expected WRAP sticky bit to set after a period"

@cocotb.test
async def clear_status_w1c(dut):
    """Set up the counter and run till wrap.  Write 1 to clear status."""
    cocotb.start_soon(Clock(dut.ACLK, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    PERIOD = 10
    # Set up the counter with low period
    await axil.write(0x4, (PERIOD).to_bytes(4, "little"))
    await axil.write(0x8, (4).to_bytes(4, "little"))
    await axil.write(0x0, (1).to_bytes(4, "little"))  # CTRL_EN
    for _ in range(PERIOD + 1):
        await RisingEdge(dut.ACLK)

    # Verify that status is high
    status = await read32(axil, 0xC)
    assert (status & 0x1) == 1, "Counter should have wrapped by now."

    # Stop the counter - still possible race condition after last read 
    # and before clearing the status if period is very low.
    await axil.write(0x0, (0x0).to_bytes())
    # Clear WRAP via W1C
    await axil.write(0xC, (0x1).to_bytes(4, "little"))
    status = await read32(axil, 0xC)
    assert (status & 0x1) == 0, f"WRAP should clear on W1C write: {status=}"

@cocotb.test
async def reset_self_clear(dut):
    """Check that CTRL.RST is self-clearing"""
    cocotb.start_soon(Clock(dut.ACLK, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Exercise CTRL.RST pulse (write bit1) and ensure it doesn't latch high
    await axil.write(0x0, (0x2).to_bytes(4, "little"))
    ctrl = await read32(axil, 0x0)
    assert (ctrl & 0x2) == 0, "CTRL.RST should read as 0 (self-clearing)"


@cocotb.test
async def update_pending_commits_on_wrap(dut):
    """While running, writes set UPD_PENDING; commit occurs on wrap and clears it."""
    cocotb.start_soon(Clock(dut.ACLK, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Set up the counter with low period
    await axil.write(0x0, (0x0).to_bytes(4, "little"))
    await axil.write(0x4, (15).to_bytes(4, "little"))
    await axil.write(0x8, (4).to_bytes(4, "little"))
    await axil.write(0x0, (0x1).to_bytes(4, "little"))

    # Wait a bit, then write new DUTY while running
    for _ in range(10):
        await RisingEdge(dut.ACLK)
    await axil.write(0x8, (10).to_bytes(4, "little"))

    # UPD_PENDING (bit1) should go high
    status = await read32(axil, 0xC)
    assert (status & 0x2) == 0x2, "UPD_PENDING should set when writing while enabled"

    # Wait for a wrap event: poll WRAP, then clear it; after at most a few wraps, UPD_PENDING clears
    cleared = False
    for _ in range(300):
        status = await read32(axil, 0xC)
        if status & 0x1:
            # clear wrap
            await axil.write(0xC, (0x1).to_bytes(4, "little"))
        # Check pending flag cleared (commit done)
        if (status & 0x2) == 0:
            cleared = True
            break
        await RisingEdge(dut.ACLK)
    assert cleared, "UPD_PENDING should clear after a wrap commits shadow to active"

