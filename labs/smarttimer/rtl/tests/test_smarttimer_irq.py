import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, ClockCycles
from cocotbext.axi import AxiLiteBus, AxiLiteMaster

CTRL_OFFSET = 0x0
STATUS_OFFSET = 0x4
PERIOD_OFFSET = 0x8
DUTY_OFFSET = 0xC


def mk_axil_master(dut) -> AxiLiteMaster:
    bus = AxiLiteBus.from_prefix(dut, "s_axi")
    return AxiLiteMaster(bus, dut.clk, dut.resetn, reset_active_level=False)


async def axil_write(axil: AxiLiteMaster, addr: int, data: int):
    await axil.write(addr, data.to_bytes(4, "little"))


async def axil_read(axil: AxiLiteMaster, addr: int) -> int:
    r = await axil.read(addr, 4)
    return int.from_bytes(r.data, byteorder="little")


async def reset_dut(dut):
    dut.resetn.value = 0
    await ClockCycles(dut.clk, 5)
    dut.resetn.value = 1
    await ClockCycles(dut.clk, 2)


@cocotb.test
async def test_irq_asserts_on_wrap(dut):
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())

    await reset_dut(dut)
    axil = mk_axil_master(dut)

    # Set short period
    await axil_write(axil, PERIOD_OFFSET, 10)
    await axil_write(axil, DUTY_OFFSET, 5)

    r = await axil_read(axil, PERIOD_OFFSET)
    assert r == 10

    # Enable timer
    await axil_write(axil, CTRL_OFFSET, 0x1)

    # Wait for wrap
    await ClockCycles(dut.clk, 15)

    # Check IRQ asserted
    assert dut.irq_out.value == 1, "IRQ should be high after wrap"

    # Check STATUS.WRAP set
    status = await axil_read(axil, STATUS_OFFSET)
    assert status & 0x1, "STATUS.WRAP should be set"


@cocotb.test
async def test_irq_clears_on_w1c(dut):
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())

    await reset_dut(dut)
    axil = mk_axil_master(dut)

    # Trigger wrap
    await axil_write(axil, PERIOD_OFFSET, 5)
    await axil_write(axil, CTRL_OFFSET, 0x1)
    await ClockCycles(dut.clk, 10)

    # Verify IRQ high
    assert dut.irq_out.value == 1

    # Clear STATUS.WRAP via W1C
    await axil_write(axil, STATUS_OFFSET, 0x1)
    await ClockCycles(dut.clk, 2)

    # Check IRQ cleared
    assert dut.irq_out.value == 0, "IRQ should be low after W1C"

    status = await axil_read(axil, STATUS_OFFSET)
    assert (status & 0x1) == 0, "STATUS.WRAP should be cleared"


@cocotb.test
async def test_irq_not_when_disabled(dut):
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())

    await reset_dut(dut)
    axil = mk_axil_master(dut)

    # Set period but don't enable
    await axil_write(axil, PERIOD_OFFSET, 5)
    await axil_write(axil, CTRL_OFFSET, 0x0)  # EN=0

    # Wait past period
    await ClockCycles(dut.clk, 20)

    # IRQ should stay low
    assert dut.irq_out.value == 0, "IRQ should not assert when disabled"

    status = await axil_read(axil, STATUS_OFFSET)
    assert (status & 0x1) == 0, "STATUS.WRAP should not set when disabled"


@cocotb.test
async def test_multiple_wraps(dut):
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())

    await reset_dut(dut)
    axil = mk_axil_master(dut)

    await axil_write(axil, PERIOD_OFFSET, 8)
    await axil_write(axil, CTRL_OFFSET, 0x1)

    for i in range(3):
        # Wait for wrap
        await ClockCycles(dut.clk, 12)
        assert dut.irq_out.value == 1, f"IRQ should be high on wrap {i+1}"

        # Clear
        await axil_write(axil, STATUS_OFFSET, 0x1)
        await ClockCycles(dut.clk, 2)
        assert dut.irq_out.value == 0, f"IRQ should clear after wrap {i+1}"


@cocotb.test
async def test_default_reset_values(dut):
    """Verify default values after reset: PERIOD=0xFF, DUTY=0xAA"""
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())

    await reset_dut(dut)
    axil = mk_axil_master(dut)

    period = await axil_read(axil, PERIOD_OFFSET)
    duty = await axil_read(axil, DUTY_OFFSET)
    ctrl = await axil_read(axil, CTRL_OFFSET)
    status = await axil_read(axil, STATUS_OFFSET)

    assert period == 0xFF, f"PERIOD reset value should be 0xFF, got {period:#x}"
    assert duty == 0xAA, f"DUTY reset value should be 0xAA, got {duty:#x}"
    assert ctrl == 0, "CTRL should be 0 after reset"
    assert status == 0, "STATUS should be 0 after reset"


@cocotb.test
async def test_shadow_commit_on_disable(dut):
    """Shadow registers commit to active when timer is disabled"""
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())

    await reset_dut(dut)
    axil = mk_axil_master(dut)

    # Enable timer with default values
    await axil_write(axil, CTRL_OFFSET, 0x1)
    await ClockCycles(dut.clk, 2)

    # Write new values while enabled (goes to shadow)
    await axil_write(axil, PERIOD_OFFSET, 20)
    await axil_write(axil, DUTY_OFFSET, 10)
    await ClockCycles(dut.clk, 2)

    # Check UPD_PENDING is set (bit 1 of STATUS)
    status = await axil_read(axil, STATUS_OFFSET)
    assert status & 0x2, "UPD_PENDING should be set after write while enabled"

    # Disable timer - should commit shadow to active
    await axil_write(axil, CTRL_OFFSET, 0x0)
    await ClockCycles(dut.clk, 2)

    # Re-enable and verify new values are active
    await axil_write(axil, CTRL_OFFSET, 0x1)

    # Wait enough cycles - if period is now 20, wrap at ~21 cycles
    await ClockCycles(dut.clk, 25)

    assert dut.irq_out.value == 1, "Wrap should occur with new period"


@cocotb.test
async def test_upd_pending_clears_on_wrap(dut):
    """UPD_PENDING clears when wrap occurs and values are committed"""
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())

    await reset_dut(dut)
    axil = mk_axil_master(dut)

    # Set short period and enable
    await axil_write(axil, PERIOD_OFFSET, 8)
    await axil_write(axil, CTRL_OFFSET, 0x1)
    await ClockCycles(dut.clk, 2)

    # Write new values while running
    await axil_write(axil, PERIOD_OFFSET, 15)
    await ClockCycles(dut.clk, 1)

    # Verify UPD_PENDING is set
    status = await axil_read(axil, STATUS_OFFSET)
    assert status & 0x2, "UPD_PENDING should be set"

    # Wait for wrap
    await ClockCycles(dut.clk, 10)

    # Clear WRAP bit
    await axil_write(axil, STATUS_OFFSET, 0x1)
    await ClockCycles(dut.clk, 2)

    # Check UPD_PENDING cleared
    status = await axil_read(axil, STATUS_OFFSET)
    assert (status & 0x2) == 0, "UPD_PENDING should clear after wrap"
