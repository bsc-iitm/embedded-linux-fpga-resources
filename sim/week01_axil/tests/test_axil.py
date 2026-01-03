"""cocotb tests for minimal AXI-Lite register bank using cocotbext-axi.

Replaces hand-written bus driving with AxiLiteMaster for clarity and reuse.
"""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge
from cocotbext.axi import AxiLiteBus, AxiLiteMaster


CLK_PERIOD_NS = 10


async def reset(dut):
    """Apply and release active-low reset. Keep other signals quiescent."""
    dut.ARESETn.value = 0
    # Hold reset for a few cycles
    for _ in range(5):
        await RisingEdge(dut.ACLK)
    dut.ARESETn.value = 1
    for _ in range(2):
        await RisingEdge(dut.ACLK)


def mk_axil_master(dut) -> AxiLiteMaster:
    """Create an AXI-Lite master bound to the DUT's S_AXI* signals."""
    bus = AxiLiteBus.from_prefix(dut, "saxi")
    # Active-low reset on ARESETn
    return AxiLiteMaster(bus, dut.ACLK, dut.ARESETn, reset_active_level=False)

async def read32(axil, addr: int) -> int:
    r = await axil.read(addr, 4)
    return int.from_bytes(r.data, byteorder="little")

@cocotb.test
async def basic_rw(dut):
    """Reset, write CTRL and DATA with AxiLiteMaster, read back DATA."""
    cocotb.start_soon(Clock(dut.ACLK, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # CTRL.EN = 1
    await axil.write(0x0, (1).to_bytes(4, byteorder="little"))

    # DATA = 0x00001234
    await axil.write(0x4, (0x00001234).to_bytes(4, byteorder="little"))

    # Read back
    data = await read32(axil, 0x4)
    assert data == 0x00001234, f"DATA mismatch: got 0x{data:08x}"


@cocotb.test
async def partial_write_and_readback(dut):
    """Exercise WSTRB via a partial (2-byte) write then read back full word."""
    cocotb.start_soon(Clock(dut.ACLK, CLK_PERIOD_NS, units="ns").start())
    await reset(dut)

    axil = mk_axil_master(dut)

    # Initialize location to 0x12345678 then perform a 2-byte write to low half
    await axil.write(0x4, (0x12345678).to_bytes(4, byteorder="little"))
    await axil.write(0x4, b"\xCD\xAB")  # low 16 bits = 0xABCD

    val = await read32(axil, 0x4)
    assert val == 0x1234ABCD, f"Expected 0x1234ABCD, got 0x{val:08x}"
