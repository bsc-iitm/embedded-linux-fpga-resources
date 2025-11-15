# Demo 2: FIR Filter with Linux Driver

This demo shows the complete software development workflow: hardware validation with Verilator (Week 7 existing work) and driver development with Renode.

## Overview

**Hardware**: 4-tap FIR filter with AXI-Lite interface
**Software**: Linux platform driver with sysfs interface
**Goal**: Demonstrate full-stack development in emulation before FPGA deployment

## Directory Structure

```
demo2_fir_linux/
├── driver/
│   ├── fir_simple.c       # Platform driver
│   └── Makefile
├── renode/
│   ├── overlays/
│   │   └── fir_cosim.repl    # FIR co-sim peripheral (AXI-Lite target)
│   ├── dts/
│   │   └── fir-node.dts      # Device tree fragment (on AXI bus)
│   └── demo2.resc            # Startup script
├── verilator_cosim/
│   ├── CMakeLists.txt        # Build libVtop.so
│   ├── rtl/
│   │   └── fir_filter_axil_cosim.v  # 64-bit AXI-Lite wrapper
│   └── sim_main.cpp          # AxiLite wiring + logs
└── README.md
```

## Part 1: Hardware Validation (Verilator)

**Location**: `../../week07_fir_verilator/`

**Already Complete**: Standalone Verilator tests validate FIR RTL correctness

```bash
cd ../../week07_fir_verilator
make run
```

This proves the hardware logic is correct before software development begins.

## Part 2: Driver Development (Renode + Linux + Verilator Co‑Sim)

### Build Driver

**Prerequisites:**
- Linux kernel source in `../../downloads/linux-6.6/` (see Week 4 instructions)
- Environment variables set (ARCH, CROSS_COMPILE - see main sim/README.md)

```bash
cd driver

# For ARM (cross-compile):
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KDIR=../../downloads/linux-6.6

# Output: fir_simple.ko
```

### Build Verilator Co‑Sim Library

**Prerequisites:**
- Set `RENODE_PATH` environment variable (see main sim/README.md)
- Example: `export RENODE_PATH=/path/to/renode_portable`

```bash
cd verilator_cosim
mkdir -p build && cd build
cmake -DUSER_RENODE_DIR=${RENODE_PATH} ..
# Build only the shared library used by Renode
cmake --build . --target libVtop -j
```

### Prepare Renode

1. **Get Linux kernel and DTB**:
   - Use kernel from Week 4/5: symlink or copy `vmlinux` to `renode/binfiles/`
   - Compile device tree with FIR node: `dtc -I dts -O dtb -o renode/binfiles/zynq-zed-fir.dtb renode/dts/fir-node.dts`

2. **Binaries location**:
   ```
   renode/binfiles/vmlinux         # Kernel (from downloads or week04)
   renode/binfiles/zynq-zed-fir.dtb  # Device tree with FIR node
   ```

3. **Copy driver module**:
   ```
   # Include fir_simple.ko in initramfs or copy to guest FS
   ```

### Run Demo

```bash
cd renode
renode demo2.resc

# In Renode monitor:
(monitor) start
```

### Inside Linux Guest

```bash
# 1. Load driver
insmod /path/to/fir_simple.ko

# 2. Find device
ls /sys/bus/platform/devices/70002000.fir-filter/

# 3. Load coefficients (4-tap averaging: 0.25 each)
# Q15 format: 0.25 = 8192 = 0x2000
echo "8192,8192,8192,8192" > /sys/.../coeff

# 4. Write input data
echo "reset" > /sys/.../data_in
echo "10,20,30,40,50" > /sys/.../data_in

# 5. Start processing
echo "start" > /sys/.../ctrl

# 6. Check status
cat /sys/.../status
# Should show: 0x00000003 (DONE=1, READY=1)

# 7. Read filtered output
cat /sys/.../data_out
# Expected: 2, 7, 15, 25, 35 (approx, due to Q15 rounding)
```

## Understanding the Results

**Input**: `[10, 20, 30, 40, 50]`
**Coefficients**: `[0.25, 0.25, 0.25, 0.25]` (4-tap averaging)

**FIR Equation**: `y[n] = 0.25 * (x[n] + x[n-1] + x[n-2] + x[n-3])`

With zero-padding for `n < k`:
- `y[0]` = 0.25 × 10 = 2.5 → 2
- `y[1]` = 0.25 × (10 + 20) = 7.5 → 7
- `y[2]` = 0.25 × (10 + 20 + 30) = 15
- `y[3]` = 0.25 × (10 + 20 + 30 + 40) = 25
- `y[4]` = 0.25 × (20 + 30 + 40 + 50) = 35

## Co‑Simulation Notes

- This demo uses the real FIR RTL via Verilator; the previous Python stub has been removed to avoid confusion.
- The AXI‑Lite interface is widened to 64‑bit data in a thin wrapper to match Renode’s Integration Library, while the core remains 32‑bit internally.

## Driver Features

**Sysfs Interface**:
- `coeff` (RW): Load/read filter coefficients
- `data_in` (WO): Write input samples
- `data_out` (RO): Read filtered results
- `ctrl` (WO): Start processing or reset
- `status` (RO): Check DONE and READY flags
- `len` (RO): Number of samples

**Design Patterns** (from Week 6):
- Vector mode writes (comma-separated values)
- Mutex for concurrent access protection
- Textual commands (`"reset"`, `"start"`) for ease of testing

## Comparing to Verilator

| Aspect | Verilator (standalone) | Renode (co‑sim) |
|--------|-------------------------|------------------|
| **RTL Signals** | Waveforms, internals | Handshakes via IntegrationLibrary |
| **Timing** | Cycle-accurate | Per-transaction ticks (not wall‑clock) |
| **Use Case** | HW verification | SW+HW integration under Linux |
| **Speed** | Slower | Slower than stub, but practical |
| **Full System** | No (peripheral only) | Yes (Linux + drivers) |

**Workflow**:
1. ✅ Verilator: Validate RTL correctness
2. ✅ Renode: Develop and test driver
3. ⏳ FPGA: Integrate both on real hardware

## Teaching Points

1. **Separation of Concerns**: Hardware and software teams can work in parallel
2. **Functional vs Timing**: Co‑sim runs real RTL; timing advances per AXI‑Lite accesses
3. **Register Contract**: Same memory map in Verilator, Renode, and FPGA
4. **Iterative Development**: Fast Renode cycles for driver logic, Verilator for HW corner cases

## Troubleshooting

**Driver doesn't load**:
- Check device tree has FIR node at 0x70002000
- Verify `compatible = "acme,fir-filter-v1"` matches driver
- Look for probe errors in `dmesg`

**No sysfs attributes**:
- Check `sysfs_create_group` return value
- Verify `/sys/bus/platform/devices/` has FIR device

**Unexpected output**:
- Verify Q15 coefficient values (0.25 = 8192)
- Check input data range (16-bit signed)
- Review Renode peripheral logs (enable with `logLevel 0 fir_filter`)

## Next Steps

- **Week 8**: Detailed study of driver patterns (compare Smart Timer vs FIR)
- **Week 9**: Add interrupt handling (DONE flag triggers IRQ)
- **FPGA deployment**: Deploy to hardware (e.g., Pynq-Z1) and compare Renode predictions to real hardware

## References

- FIR Filter RTL: `../../week07_fir_verilator/rtl/fir_filter_axil.v`
- FIR specification and details available in week07_fir_verilator documentation
- Week 6 FFT Demo: `../../week06_fft_block_demo/` (similar pattern)
