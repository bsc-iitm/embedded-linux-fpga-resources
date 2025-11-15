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
