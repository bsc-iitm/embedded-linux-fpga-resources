# Verilator Co-Sim

- Same build flow as Week 8; produces a shared library loaded by Renode.
- Wrapper widens AXI-Lite data to 64-bit and forwards `irq_out` as GPIO[0].
- Output: `libVtop.so` in `build/` (referenced by the .resc script).
