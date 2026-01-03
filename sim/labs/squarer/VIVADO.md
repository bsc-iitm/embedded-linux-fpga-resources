# Vivado Project Setup for Squarer Demo

This guide describes how to create the Vivado block design with two squarer
instances: one using MMIO (slow) and one using DMA (fast).

## Overview

```
                          +------------------+
    +--------+            | squarer_mmio     |
    | Zynq   |---[GP0]--->| (AXI-Lite)       |
    |   PS   |            +------------------+
    |        |
    |        |            +------------------+     +----------------+
    |        |---[GP0]--->| AXI DMA          |<--->| squarer_stream |
    |        |            | (control)        |     | (AXI Stream)   |
    |        |            +------------------+     +----------------+
    |        |                  |
    |        |---[HP0]----------+ (data path)
    |        |
    |        |<-- IRQ_F2P[0] -- (DMA S2MM completion)
    +--------+
```

## Step 1: Add RTL Sources

1. Create new Vivado project for PYNQ-Z1 (xc7z020clg400-1)
2. Add RTL sources:
   - `squarer/rtl/squarer_mmio.v`
   - `squarer/rtl/squarer_stream.v`

## Step 2: Package Custom IPs

### Package squarer_mmio

1. Tools -> Create and Package New IP -> Package current project
2. Name: `squarer_mmio`, Vendor: `demo`
3. In IP Packager:
   - Ports and Interfaces: Select `s_axil_*` -> Auto Infer AXI4-Lite Slave
   - Name interface: `s_axil`
   - Set address range: 4K
4. Package IP

### Package squarer_stream

1. Create new packaging project with `squarer_stream.v`
2. Name: `squarer_stream`, Vendor: `demo`
3. In IP Packager:
   - Select `s_axis_*` -> Auto Infer AXI Stream Slave (16-bit)
   - Select `m_axis_*` -> Auto Infer AXI Stream Master (32-bit)
4. Package IP

## Step 3: Create Block Design

1. IP Integrator -> Create Block Design
2. Name: `squarer_demo`

### Add IP Blocks

1. **ZYNQ7 Processing System**
   - Run Block Automation
   - Double-click to configure:
     - PS-PL Configuration -> HP Slave AXI Interface -> Enable S AXI HP0
     - Interrupts -> Fabric Interrupts -> PL-PS -> IRQ_F2P[0:0]

2. **squarer_mmio** (your custom IP)

3. **AXI DMA**
   - Double-click to configure:
     - Disable Scatter Gather
     - Memory Map Data Width: 32
     - Stream Data Width: 32 (output is 32-bit)
     - Max Burst Size: 256

4. **squarer_stream** (your custom IP)

5. **AXI Interconnect** (for GP0 connections)

6. **AXI SmartConnect** (for HP0 connections)

7. **Concat** (for interrupts if needed)

## Step 4: Connect the Design

### Clock and Reset
- FCLK_CLK0 -> all IP clocks
- Use Processor System Reset for synchronized resets

### AXI-Lite Path (Control)
```
Zynq M_AXI_GP0 --> AXI Interconnect --> squarer_mmio (s_axil)
                                    --> AXI DMA (S_AXI_LITE)
```

### AXI DMA Data Path
```
AXI DMA M_AXI_MM2S --> AXI SmartConnect --> Zynq S_AXI_HP0
AXI DMA M_AXI_S2MM --> AXI SmartConnect --> Zynq S_AXI_HP0
```

### AXI Stream (Data Flow)

NOTE: Input is 16-bit, output is 32-bit. You may need width converters.

```
AXI DMA M_AXIS_MM2S (32-bit) --> AXI4-Stream Data Width Converter --> squarer_stream s_axis (16-bit)
squarer_stream m_axis (32-bit) --> AXI DMA S_AXIS_S2MM (32-bit)
```

For simplicity, configure DMA with 16-bit stream width on MM2S side:
- MM2S Stream Data Width: 16
- S2MM Stream Data Width: 32

Or use direct connection if widths match.

### Interrupts
```
AXI DMA s2mm_introut --> Zynq IRQ_F2P[0:0]
```

## Step 5: Address Map

| Block | Offset | Range |
|-------|--------|-------|
| squarer_mmio | 0x43C0_0000 | 4K |
| axi_dma | 0x4040_0000 | 64K |

## Step 6: Generate and Export

1. Validate Design (F6)
2. Generate Block Design
3. Create HDL Wrapper
4. Generate Bitstream
5. File -> Export Hardware (include bitstream)

## Block Design Diagram

```
+-------------------------------------------------------------------------+
|                                                                         |
|   +-------------+      +------------------+                             |
|   | Zynq PS     |      | squarer_mmio     |                             |
|   |             |      |                  |                             |
|   | M_AXI_GP0 --|----->| s_axil           |                             |
|   |             |  |   +------------------+                             |
|   |             |  |                                                    |
|   |             |  |   +------------------+      +------------------+   |
|   |             |  +-->| AXI DMA          |      | squarer_stream   |   |
|   |             |      |                  |      |                  |   |
|   |             |      | M_AXIS_MM2S -----+----->| s_axis (16-bit)  |   |
|   | S_AXI_HP0 <-+------| M_AXI_MM2S       |      |                  |   |
|   |             |      | M_AXI_S2MM       |      | m_axis (32-bit) -+--+|
|   |             |      |                  |      +------------------+  ||
|   | IRQ_F2P[0] <+------| s2mm_introut     |<---------------------------+|
|   |             |      | S_AXIS_S2MM <----+-----------------------------+
|   +-------------+      +------------------+                             |
|                                                                         |
+-------------------------------------------------------------------------+
```

## Quick Verification

After loading bitstream and drivers:

```bash
# Load drivers
insmod squarer_mmio.ko
insmod squarer_dma.ko

# Run test
./test_squarer 1024
```

Expected output:
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
