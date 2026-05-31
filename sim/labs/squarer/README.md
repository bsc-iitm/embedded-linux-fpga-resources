# Squarer Demo: MMIO vs DMA

This folder holds the RTL, drivers and test program for the squarer
experiment, which compares per-sample MMIO access against bulk DMA transfer
for computing `y = x * x`.

The full step-by-step instructions (Vivado block design, drivers, device tree,
running and comparing on the board) are in
**[docs/04 - Squarer: MMIO vs DMA](../docs/04-squarer-mmio-dma.md)**.

## Contents

```
squarer/
├── rtl/
│   ├── squarer_mmio.v      # AXI-Lite: DATA_IN/DATA_OUT registers
│   └── squarer_stream.v    # AXI Stream for DMA integration
├── driver/
│   ├── squarer_mmio.c      # Char device, per-sample register access
│   ├── squarer_dma.c       # Char device, DMA bulk transfer
│   └── Makefile
└── sw/
    ├── test_squarer.c      # Userspace comparison program
    └── Makefile
```
