# Purpose: Minimal cocotb demo on a simple counter DUT
# Highlights:
# - Start a clock and perform reset
# - Drive inputs and await FallingEdge
#   - We use FallingEdge in the TB to prevent races against the DUT
# - Use Python assertions and helper coroutines for clarity

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import FallingEdge


CLK_NS = 10


async def reset(dut):
    dut.rstn.value = 0
    dut.en.value = 0
    # A couple of cycles with reset asserted
    for _ in range(2):
        await FallingEdge(dut.clk)
    dut.rstn.value = 1
    # Let one cycle pass after deassertion
    await FallingEdge(dut.clk)


async def tick(dut, cycles=1):
    for _ in range(cycles):
        await FallingEdge(dut.clk)


@cocotb.test
async def counter_basic_counting(dut):
    """Counter increments when enabled and holds when disabled."""
    # Start a periodic clock on dut.clk
    cocotb.start_soon(Clock(dut.clk, CLK_NS, units="ns").start())

    # Apply reset and check initial value
    await reset(dut)
    assert int(dut.q.value) == 0, f"After reset, q should be 0, got {int(dut.q.value)}"

    # Enable counting for 5 cycles
    dut.en.value = 1
    await tick(dut, 5)
    assert int(dut.q.value) == 5, f"Expected q=5 after 5 cycles, got {int(dut.q.value)}"

    # Disable and ensure the value holds over multiple cycles
    dut.en.value = 0
    snap = int(dut.q.value)
    await tick(dut, 3)
    assert int(dut.q.value) == snap, "q changed while en=0"

    # Re-enable and count a few more cycles
    dut.en.value = 1
    await tick(dut, 2)
    assert int(dut.q.value) == snap + 2, "q did not increment correctly after re-enabling"


@cocotb.test
async def counter_wrap_example(dut):
    """Demonstrate wrap-around behavior with small counters (WIDTH)
    This serves to show how assertions can document expected boundary behavior.
    """
    cocotb.start_soon(Clock(dut.clk, CLK_NS, units="ns").start())
    await reset(dut)

    # Run for one cycle
    dut.en.value = 1
    await tick(dut, 1)
    assert int(dut.q.value) == 1

    # Run for 2 cycles to confirm counting is happening
    await tick(dut, 2)
    assert int(dut.q.value) == 3

    # Run until wraparound and confirm that value comes back to 1
    WIDTH=8
    await tick(dut, 2**WIDTH-2)
    assert int(dut.q.value) == 1
