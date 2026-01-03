# Smart Timer Verilator+Renode Co-Simulation

This directory contains **real** Verilator+Renode co-simulation setup for the Smart Timer peripheral. The Verilog RTL runs cycle-accurately inside Renode.

## What This Is

- **Real co-simulation**: Smart Timer Verilog executes in Verilator within Renode
- **Not a stub**: Actual RTL behavior, not Python approximation
- **Cycle-accurate**: Hardware timing is preserved

## Prerequisites

1. **Verilator** (already installed: `verilator --version`)
2. **Renode** v1.16.0+ with Integration Library (already have it)
3. **CMake** 3.10+ (`cmake --version`)
4. **C++ compiler** with C++14 support

## Directory Structure

```
verilator_cosim/
├── CMakeLists.txt              # Build configuration
├── sim_main.cpp                 # C++ wrapper connecting Verilog to Renode
├── build/                       # Build output (created by CMake)
└── README.md
```

## Build Instructions

### Step 1: Configure with CMake

```bash
cd verilator_cosim
mkdir -p build
cd build
cmake ..
```

This will:
- Find Verilator
- Locate Renode's Integration Library
- Configure the build

### Step 2: Build the Shared Library

```bash
make
```

This will:
- Run Verilator on Smart Timer RTL
- Compile the C++ wrapper
- Link with Renode's AXI-Lite bus interface
- Produce `libVtop.so`

**Expected output**: `build/libVtop.so`

## Using in Renode

### Use with this demo

This repository already includes Renode files to load the co-simulated peripheral via `CoSimulatedPeripheral`:

- `renode_baremetal/renode/smarttimer_cosim.repl` defines:
  - `smarttimer: CoSimulated.CoSimulatedPeripheral @ sysbus <0x70000000, +0x1000>`
  - `uart2: UART.PL011 @ sysbus 0x70001000`
- `renode_baremetal/renode/demo1.resc` loads the REPL and points the Smart Timer to `build/libVtop.so` with `SimulationFilePathLinux`.

Run it with:

```bash
cd ../renode_baremetal/renode
renode demo1.resc
```

## How It Works

### Architecture

```
┌─────────────────────────────────────────┐
│  Renode (C# / Mono)                     │
│                                         │
│  ┌─────────────┐                        │
│  │ CPU (ARM)   │                        │
│  └──────┬──────┘                        │
│         │ AXI Bus                       │
│  ┌──────▼──────────────────────────┐    │
│  │ sysbus @ 0x70000000            │    │
│  │                                 │    │
│  │ ┌─────────────────────────┐    │    │
│  │ │ libVtop.so              │    │    │
│  │ │ (loaded peripheral)     │    │    │
│  │ └────────┬────────────────┘    │    │
│  │          │                      │    │
│  │    ┌─────▼──────────┐           │    │
│  │    │ Renode         │           │    │
│  │    │ Integration    │           │    │
│  │    │ Layer (C++)    │           │    │
│  │    └─────┬──────────┘           │    │
│  │          │ AXI-Lite Protocol    │    │
│  │    ┌─────▼──────────┐           │    │
│  │    │ Verilator      │           │    │
│  │    │ Model (C++)    │           │    │
│  │    │                │           │    │
│  │    │ smart_timer_   │           │    │
│  │    │ axil.v         │           │    │
│  │    └────────────────┘           │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

### Data Flow

1. **CPU writes to 0x70000000**
2. **Renode sysbus** routes to loaded peripheral
3. **Integration layer** translates to AXI-Lite transaction
4. **Verilator model** executes Verilog:
   - Performs handshakes
   - Updates internal registers
   - Advances state machine
5. **Response** flows back through layers to CPU

### Timing

- **Verilator ticks** advance on each bus transaction
- **Cycle-accurate** within peripheral
- **Not real-time**: simulation time, not wall-clock time

## Notes

- This demo uses only the co-simulation path; the Python peripheral stub has been removed to keep focus on HW/SW integration.

## Troubleshooting

### Build Fails: "Cannot find Integration Library"

Check path in CMakeLists.txt matches your Renode installation:
```cmake
set(INTEGRATION_LIB_PATH "${RENODE_PATH}/plugins/IntegrationLibrary")
```

### Verilator Errors

Ensure no warnings from Verilog (we fixed those already in commit 526e785).

### Renode Can't Load Library

```bash
# Check library was built:
ls -la build/libVtop.so

# Check it's a valid shared library:
file build/libVtop.so
# Should say: "ELF 64-bit LSB shared object"

# Check symbols:
nm -D build/libVtop.so | c++filt | grep " Init(\)"
# Should show: "Init()"
```

### Co-Simulation Runs But Behaves Incorrectly

Enable Verilator tracing (modify wrapper to call `dut->trace()`) and examine waveforms to debug RTL behavior.

## Performance Notes

Co-simulation is **significantly slower** than pure emulation:
- Each bus transaction advances Verilator clock
- Typical slowdown: 10-100x
- Still interactive for development
- Not suitable for long boot sequences

For validation and integration testing: use this co-simulation.

## Next Steps

After building and testing Smart Timer co-simulation:
1. Apply same pattern to FIR filter (Demo 2)
2. Compare co-sim behavior to standalone Verilator test
3. Deploy to FPGA (Week 12) and validate they match

## References

- Renode Integration Library: `${RENODE_PATH}/plugins/IntegrationLibrary/`
- Renode Docs: https://renode.readthedocs.io
- Integration repo: https://github.com/antmicro/renode-verilator-integration
