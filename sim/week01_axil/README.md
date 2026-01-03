# Week 1 cocotb Demo — AXI-Lite Register Bank

This directory contains a minimal AXI-Lite slave RTL (two registers) and a cocotb test that writes/reads those registers.

Structure
- `rtl/axil_regbank.v`: AXI-Lite slave with `CTRL @ 0x0` and `DATA @ 0x4`.
- `tests/test_axil.py`: cocotb tests using `cocotbext-axi`'s `AxiLiteMaster`.
- `Makefile`: runs cocotb with Verilator by default; Icarus as an option.

Prerequisites
- Python 3, cocotb and cocotbext-axi installed (from `sim/requirements.txt`).
- One simulator: Verilator (recommended) or Icarus Verilog.
- Optional: GTKWave for viewing VCD/FST.

Run (Verilator)
- `make` or `make SIM=verilator`

Run (Icarus)
- `make SIM=icarus`

Waves
- Verilator: set `WAVES=1` (default) to produce traces.
- Icarus: wave output may require additional flags; refer to cocotb docs.

Expected Behavior
- CTRL reset to 0; write `CTRL.EN=1`.
- DATA write/read round-trip `0x00001234` using `AxiLiteMaster`.
- Partial low-halfword write (2 bytes) updates only low 16 bits via WSTRB.
