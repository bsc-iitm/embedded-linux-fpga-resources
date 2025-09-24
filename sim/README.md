# Simulation Environment Setup (Shared)

## Goals
- One shared Python environment for all simulations under `sim/`.
- Use `uv` to manage a lightweight virtualenv and dependencies.
- Keep simulator/tooling installs simple and system-wide (not via Python).

## System Packages (Install Once)
- Verilog simulators (choose at least one):
  - Verilator (recommended):
    - Ubuntu/Debian: `sudo apt-get install verilator`
    - Fedora: `sudo dnf install verilator`
    - macOS (Homebrew): `brew install verilator`
  - Icarus Verilog (alternative):
    - Ubuntu/Debian: `sudo apt-get install iverilog`
    - Fedora: `sudo dnf install iverilog`
    - macOS: `brew install icarus-verilog`
- Waveform viewer (optional but useful):
  - GTKWave: `sudo apt-get install gtkwave` or `brew install gtkwave`

Notes
- Verilator is faster and integrates well with cocotb; Icarus is simpler and fine for basics.
- Windows: use WSL and follow Ubuntu instructions.

## Install uv (Python environment manager)
- If you don’t have `uv`:
  - `curl -LsSf https://astral.sh/uv/install.sh | sh`
  - Or via Homebrew: `brew install uv`
- Ensure `uv` is on PATH (restart shell if needed): `uv --version`

## Create the Shared Environment
- From the repository root or `sim/` directory:
  - `cd sim`
  - `uv venv .venv`
  - Activate:
    - bash/zsh: `source .venv/bin/activate`
    - fish: `source .venv/bin/activate.fish`
    - PowerShell: `.venv\Scripts\Activate.ps1`

## Install Python Requirements
- Inside the activated environment:
  - `uv pip install -r requirements.txt`
- File: `sim/requirements.txt` (kept minimal and shared)

## Verify Setup
- Check cocotb is visible: `python -c "import cocotb, sys; print('cocotb', cocotb.__version__)"`
- Check simulators on PATH:
  - `verilator --version` (if using Verilator)
  - `iverilog -V` (if using Icarus)

## Run a Demo (Week 1)
- Activate the env: `source sim/.venv/bin/activate`
- Navigate to Week 1 demo: `cd sim/week01_axil`
- Run with Verilator (default): `make`
- Or with Icarus: `make SIM=icarus`
- Open waves (if generated): `gtkwave sim_build/…/*.vcd` or FST as applicable.

## Troubleshooting
- `cocotb-config` not found or error `/Makefile.sim`: activate the env before `make`:
  - `source sim/.venv/bin/activate`
  - Then `cd sim/week00_basics && make` (or another sim folder)
- Permission errors on Verilator install: use your OS package manager, not `pip`.
- No waves: for Verilator, ensure `VERILATOR_TRACE=1` (set by Makefile) and rerun; for Icarus, consult cocotb docs for VCD generation.

## What’s Installed (Python)
- `cocotb` for coroutine-based testbenches.
- `cocotbext-axi` for reusable AXI/AXI-Lite bus functional models.
- Add more packages as needed and update `requirements.txt` to keep all sims aligned.
