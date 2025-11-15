# Smart Timer Co‑Simulation Guide (Renode + Verilator)

This guide focuses on the real hardware/software co‑simulation path. The Smart Timer RTL runs inside Renode via the Verilator Integration Library.

## Build the Verilated Peripheral

**Prerequisites:**
- Set `RENODE_PATH` environment variable (see main sim/README.md)
- Example: `export RENODE_PATH=/path/to/renode_portable`

```bash
cd verilator_cosim
mkdir -p build && cd build
cmake -DUSER_RENODE_DIR=${RENODE_PATH} ..
make -j

# Output: libVtop.so
```

Notes
- The AXI‑Lite interface uses 32‑bit addresses and 64‑bit data (upper bits unused).
- The wrapper exports `Init()` and uses native (in‑process) connection.

## Run Renode

```bash
cd ../../renode_baremetal/renode
${RENODE_PATH}/renode demo1.resc
```

What the script does
- Loads Zynq‑7000 platform (Cortex‑A9)
- Adds a PL011 UART at `0x70001000` (shown in analyzer)
- Loads the Verilated Smart Timer at `0x70000000` using `libVtop.so`
- Loads the bare‑metal ELF (`timer_test.elf`) and sets PC=0x0

## Sanity Checks (Renode monitor)

```text
# Optional logging
sysbus LogPeripheralAccess true

# Probe the timer registers
sysbus WriteDoubleWord 0x70000004 0xFF
sysbus ReadDoubleWord  0x70000004
```

## Debugging Cosim

- Enable verbose logs from the Verilator side:
  - Launch Renode as: `COSIM_VERBOSE=1 renode demo1.resc`
- In Renode, increase log level: `logLevel 2` (WARNING) or `logLevel 1` (INFO)
- If timeouts occur on access, verify that Renode loads the correct library and the `libVtop.so` timestamp matches your last build.

## Notes

- The earlier Python stub (functional model) has been removed to avoid confusion. This demo now uses only the co‑simulation path.
- The standalone Verilator test (`verilator_sim/`) remains available for pure RTL validation.

## References

- Verilator co-sim README: `verilator_cosim/README.md`
- Renode Integration Library: `${RENODE_PATH}/plugins/IntegrationLibrary/`
