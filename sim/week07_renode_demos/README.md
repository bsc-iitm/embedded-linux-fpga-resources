# Week 7 — Renode Demos: Co-Simulation Workflow

This directory contains two teaching demonstrations that illustrate the hardware/software co-development workflow using Verilator and Renode.

## Teaching Philosophy

**Real-world workflow**:
1. **Verilator**: Validate RTL behavior in isolation (fast, detailed traces)
2. **Renode**: Develop drivers and software with system context (full Linux stack)
3. **Hardware**: Deploy to FPGA with confidence

These demos show each step and how they connect conceptually.

## Demo 1: Smart Timer + Bare Metal

**Goal**: Show Verilator validation and bare-metal Renode development

**Location**: `demo1_smarttimer_baremetal/`

**Workflow**:
- `verilator_sim/`: Standalone Verilator testbench validates Smart Timer RTL
- `renode_baremetal/`: Bare-metal C code runs in Renode with stub peripheral
- Students see: Same register operations work in both environments

**Learning**: Hardware validation and software development can proceed in parallel

## Demo 2: FIR Filter + Linux Driver

**Goal**: Show full stack with Linux driver development

**Location**: `demo2_fir_linux/`

**Workflow**:
- `verilator_sim/`: FIR filter Verilator tests (Week 7 existing work)
- `driver/`: Simple Linux platform driver for FIR
- `renode_linux/`: Boot Linux in Renode with FIR stub, load driver

**Learning**: Driver development uses Renode for fast iteration; Verilator validates RTL correctness; real FPGA would combine both

## Key Insight

**Verilator** answers: "Does my hardware logic work correctly?"
**Renode** answers: "Does my software work with the hardware interface?"
**FPGA** proves: "Does everything work together in silicon?"

This separation of concerns is essential for efficient embedded system development.

## Next Steps

The next natural step would be to deploy these peripherals to actual FPGA hardware (e.g., Pynq-Z1), validating that Renode predictions match real hardware behavior.
