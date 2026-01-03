# Week 2 — Smart Timer (PWM) with AXI-Lite

This simulation demonstrates a small PWM "Smart Timer V1" peripheral with an AXI‑Lite register interface.

- Registers:
  - `CTRL @ 0x00` (RW): bit0 `EN`, bit1 `RST` (self‑clearing), others reserved (RAZ/WI)
  - `PERIOD @ 0x04` (RW): terminal count; default 0x000000FF
  - `DUTY @ 0x08` (RW): compare threshold; clipped to `PERIOD`
  - `STATUS @ 0x0C` (RO/W1C): bit0 `WRAP` sticky; bit1 `UPD_PENDING`; others reserved
- Updates to PERIOD/DUTY while running are latched into shadows and committed on counter wrap.

**NOTE**: Due to some changes for later weeks, the simulation currently (as of week 8) does not run correctly as expected.  This will be fixed separately.

## Run

- Ensure Python env is active and cocotb + cocotbext-axi are installed (see `../requirements.txt`).
- From this directory:

```
make SIM=verilator
```

Waveforms are enabled for Verilator via `--trace` in the Makefile.

## Files
- `rtl/pwm_core.v` — free‑running counter + compare; emits a one‑cycle `wrap` pulse.
- `rtl/smart_timer_axil.v` — AXI‑Lite slave with register bank + PWM integration.
- `tests/test_timer.py` — cocotb tests using `AxiLiteMaster` (lower‑case `saxi_*` signals like Week 1).
- `Makefile` — invokes cocotb build with Verilator by default.
