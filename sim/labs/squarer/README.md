# Squarer Demo: MMIO vs DMA Comparison

This demo shows the performance difference between per-sample MMIO access
and bulk DMA transfer for a simple computation (x * x).

## Overview

Two hardware instances compute `y = x * x`:

| Instance | Interface | CPU Operations per N samples |
|----------|-----------|------------------------------|
| squarer_mmio | AXI-Lite registers | 2N (write + read per sample) |
| squarer_dma | AXI Stream + DMA | ~4 (DMA setup only) |

## Directory Structure

```
squarer/
├── rtl/
│   ├── squarer_mmio.v      # AXI-Lite: DATA_IN/DATA_OUT registers
│   └── squarer_stream.v    # AXI Stream for DMA integration
├── driver/
│   ├── squarer_mmio.c      # Char device, per-sample register access
│   ├── squarer_dma.c       # Char device, DMA bulk transfer
│   └── Makefile
├── sw/
│   ├── test_squarer.c      # Userspace comparison program
│   └── Makefile
├── VIVADO.md               # Block design setup guide
└── README.md
```

## Building

### Drivers (cross-compile for ARM)
```bash
cd driver
make KDIR=/path/to/linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

### Test Program
```bash
cd sw
make CC=arm-linux-gnueabihf-gcc
```

## Usage

### On the PYNQ board:

```bash
# Load drivers
insmod squarer_mmio.ko
insmod squarer_dma.ko

# Verify devices exist
ls -l /dev/squarer_*

# Run comparison test
./test_squarer 1024
```

### Expected Output

```
Squarer Driver Comparison
=========================
Samples: 1024

Testing MMIO driver (/dev/squarer_mmio)...
  Time: 2500000 ns (2500.00 us)
  Per sample: 2441 ns
  Errors: 0

Testing DMA driver (/dev/squarer_dma)...
  Time: 15000 ns (15.00 us)
  Per sample: 15 ns
  Errors: 0

Summary
-------
MMIO:   2500000 ns  (1024 samples, 2 reg ops each = 2048 MMIO ops)
DMA:      15000 ns  (1024 samples in single bulk transfer)
Speedup: 166.7x
```

## How It Works

### MMIO Driver (Slow Path)

The userspace program interface is simple:
```c
int fd = open("/dev/squarer_mmio", O_RDWR);
write(fd, input, n * sizeof(int16_t));   // provide input data
read(fd, output, n * sizeof(int32_t));   // compute and get results
```

Inside the driver's `read()` function:
```c
for (i = 0; i < n; i++) {
    writel(input[i], base + REG_DATA_IN);   // write to HW register
    output[i] = readl(base + REG_DATA_OUT); // read from HW register
}
```

Each MMIO operation crosses the PS-PL boundary, taking ~1-2 microseconds.
For 1024 samples, that's 2048 MMIO operations = ~2-4 milliseconds.

### DMA Driver (Fast Path)

Same userspace interface:
```c
int fd = open("/dev/squarer_dma", O_RDWR);
write(fd, input, n * sizeof(int16_t));
read(fd, output, n * sizeof(int32_t));
```

But inside the driver's `read()`:
```c
// Just 4 register writes to set up DMA
writel(input_dma_addr, dma_base + MM2S_SA);
writel(input_bytes,    dma_base + MM2S_LENGTH);
writel(output_dma_addr, dma_base + S2MM_DA);
writel(output_bytes,    dma_base + S2MM_LENGTH);
// Then wait for completion interrupt
```

DMA transfers data at ~1 word per clock cycle (100 MHz = 200 MB/s for 16-bit).
For 1024 samples, that's ~10-20 microseconds including setup overhead.

## Key Takeaways

1. **MMIO is simple but slow**: Each register access has ~1us overhead
2. **DMA requires setup but is fast**: ~4 register writes, then hardware does the rest
3. **Speedup scales with data size**: More samples = more dramatic DMA advantage
4. **Driver interface can hide complexity**: Both use same write/read API

## Hardware Requirements

See [VIVADO.md](VIVADO.md) for block design setup instructions.

Required Vivado IP:
- squarer_mmio (custom AXI-Lite IP)
- squarer_stream (custom AXI Stream IP)
- AXI DMA
- AXI Interconnect
