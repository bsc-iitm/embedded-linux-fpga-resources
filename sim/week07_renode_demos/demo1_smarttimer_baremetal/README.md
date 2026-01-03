# Demo 1: Smart Timer with Bare Metal

This demo illustrates co-simulating the Smart Timer RTL inside Renode using the Verilator Integration Library. It also includes a standalone Verilator test.

## Overview

**Hardware**: Smart Timer V1 (PWM peripheral from Week 2)
**Software**: Bare-metal C program to configure PWM
**Goal**: Show same register operations work in both simulation environments

## Directory Structure

```
demo1_smarttimer_baremetal/
├── verilator_sim/          # Hardware validation (Verilator)
│   ├── rtl/ -> ../../../week02_timer/rtl/
│   ├── test_baremetal.cpp  # C++ testbench mimicking bare-metal code
│   └── Makefile
├── renode_baremetal/       # Bare‑metal app + Renode co-simulation
│   ├── src/
│   │   └── timer_test.c    # Bare-metal C code
│   └── renode/
│       ├── smarttimer_cosim.repl # CoSimulated peripheral (AXI-Lite) + PL011 UART2
│       └── demo1.resc            # Startup script (loads libVtop.so)
└── README.md
```

## Part 1: Verilator Hardware Validation

### Purpose
Validate that Smart Timer RTL behaves correctly according to specification.

### Run
```bash
cd verilator_sim
make
```

### What It Tests
- Write PERIOD and DUTY registers
- Enable timer (CTRL.EN=1)
- Verify counter increments and PWM output toggles correctly
- Check WRAP flag sets and clears (W1C)

### Output
- Console log showing register read/write operations
- Waveform file (`sim_build/*.fst`) showing internal signals

## Part 2: Renode Bare‑Metal + Verilator Co‑Simulation

### Purpose
Develop bare-metal software that runs on Cortex‑A9 in Renode while the Smart Timer RTL is executed by Verilator in‑process.

### Setup
The Smart Timer is modeled by the real RTL compiled into a shared library (`libVtop.so`) and loaded via Renode’s IntegrationLibrary.

### Run

**Prerequisites:**
- Set `RENODE_PATH` environment variable (see main sim/README.md)
- Example: `export RENODE_PATH=/path/to/renode_portable`

```bash
cd verilator_cosim
mkdir -p build && cd build
cmake -DUSER_RENODE_DIR=${RENODE_PATH} ..
make -j

# Run Renode
cd ../../renode_baremetal/renode
${RENODE_PATH}/renode demo1.resc
```

### What It Does
- Boots ARM CPU (no Linux, bare metal)
- Loads `timer_test.elf`
- Co-simulated Smart Timer mapped at 0x70000000 (AXI‑Lite)
- PL011 console on `uart2` at 0x70001000
- Program exercises PERIOD/DUTY/CTRL/STATUS

### Output
```
Starting emulation...
[ARM] Writing PERIOD=0x000000FF
[ARM] Writing DUTY=0x0000007F
[ARM] Setting CTRL.EN=1
[ARM] Reading STATUS: 0x00000001 (WRAP set)
[ARM] Clearing WRAP with W1C
[ARM] Reading STATUS: 0x00000000
Test complete!
```

## Connecting the Dots

| Aspect | Verilator | Renode |
|--------|-----------|--------|
| **What** | RTL signals and timing | Register interface |
| **Focus** | Hardware correctness | Software logic |
| **Speed** | Cycle-accurate (slower) | Functional (faster) |
| **Output** | Waveforms, internal state | Console logs, memory state |

**Key Insight**: The bare-metal C code uses the **same register offsets and values** in both environments. Verilator proves the hardware works; Renode lets you develop the software.

## Teaching Points

1. **Separation of Concerns**: Hardware and software teams can work in parallel
2. **Register Spec is the Contract**: Both sides implement the same memory map
3. **Validation Strategy**:
   - Verilator: Detailed RTL verification
   - Renode: Fast software iteration
   - FPGA: Final integration test

4. **Real RTL**: Renode uses the actual RTL via Verilator; useful for HW/SW integration before FPGA.

## Next Steps

- **Week 8**: Add Linux and platform driver (Demo 2 preview)
- **Week 9**: Add interrupt handling to both Verilator and Renode
- **FPGA deployment**: Deploy to hardware (e.g., Pynq-Z1) and compare behavior

## References

- Smart Timer RTL: `verilator_cosim/rtl/smart_timer_axil.v` (symlink to Week 2)
- Smart Timer register specification available in week02_timer documentation
