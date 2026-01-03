# Renode Directory - FIR Filter with DMA Co-Simulation

## Purpose

Renode configuration for FIR filter with AXI DMA co-simulation. Demonstrates full-stack integration: Linux driver + DMA controller + Verilator RTL.

## Files Expected

1. `dts/zynq-fir-dma.dts` - Device tree with FIR and AXI DMA nodes
2. `overlays/fir_dma_cosim.repl` - Platform description with DMA and Verilator peripheral
3. `scripts/demo_fir_dma.resc` - Renode script to boot and run demo

## Device Tree

### FIR Filter Node

```dts
fir_filter: fir@43c00000 {
    compatible = "acme,fir-dma-v1";
    reg = <0x43c00000 0x1000>;  // FIR config registers
    status = "okay";
};
```

### AXI DMA Node

```dts
axi_dma: dma@40400000 {
    compatible = "xlnx,axi-dma-1.00.a";
    reg = <0x40400000 0x10000>;
    interrupts = <GIC_SPI 35 IRQ_TYPE_LEVEL_HIGH>;
    #dma-cells = <1>;
    xlnx,include-mm2s;
    xlnx,include-s2mm;
    xlnx,sg-length-width = <16>;
    status = "okay";

    dma-channel@40400000 {
        compatible = "xlnx,axi-dma-mm2s-channel";
        interrupts = <GIC_SPI 34 IRQ_TYPE_LEVEL_HIGH>;
        xlnx,datawidth = <0x20>;
        xlnx,device-id = <0x0>;
    };

    dma-channel@40400030 {
        compatible = "xlnx,axi-dma-s2mm-channel";
        interrupts = <GIC_SPI 35 IRQ_TYPE_LEVEL_HIGH>;
        xlnx,datawidth = <0x20>;
        xlnx,device-id = <0x1>;
    };
};
```

### Association

```dts
&fir_filter {
    dmas = <&axi_dma 0>, <&axi_dma 1>;
    dma-names = "tx", "rx";
};
```

## Platform Description (.repl)

```
// AXI DMA controller
axi_dma: DMA.XilinxAXIDMA @ sysbus 0x40400000
    mm2s_irq -> gic@34
    s2mm_irq -> gic@35

// FIR filter (Verilator co-simulation)
fir_filter: Verilated.VerilatorIntegrationLibrary @ sysbus 0x43c00000
    size: 0x1000
    address: "libVfir_stream_top.so"
    numberOfTimeDomains: 1
    limitBuffer: 100000

// Connect AXI Stream: DMA MM2S -> FIR -> DXI S2MM
axi_dma.mm2s_axis -> fir_filter.s_axis
fir_filter.m_axis -> axi_dma.s2mm_axis
```

Notes:
- DMA MM2S IRQ on GIC SPI 34
- DMA S2MM IRQ on GIC SPI 35 (driver uses this for completion)
- AXI Stream connections link DMA and FIR
- Verilator library exposes AXI Stream slave/master

## Boot Script (.resc)

```
# Load Zynq machine
using sysbus
mach create "fir-dma-demo"

# Load base platform
machine LoadPlatformDescription @platforms/cpus/zynq-7000.repl

# Load DMA and FIR overlay
machine LoadPlatformDescriptionFromString "axi_dma: DMA.XilinxAXIDMA @ sysbus 0x40400000"
machine LoadPlatformDescriptionFromString "    mm2s_irq -> gic@34"
machine LoadPlatformDescriptionFromString "    s2mm_irq -> gic@35"

machine LoadPlatformDescriptionFromString "fir_filter: Verilated.VerilatorIntegrationLibrary @ sysbus 0x43c00000"
machine LoadPlatformDescriptionFromString "    size: 0x1000"
machine LoadPlatformDescriptionFromString "    address: \"libVfir_stream_top.so\""
machine LoadPlatformDescriptionFromString "    numberOfTimeDomains: 1"

# Connect AXI Stream
connector Connect axi_dma.mm2s_axis fir_filter.s_axis
connector Connect fir_filter.m_axis axi_dma.s2mm_axis

# Load kernel and device tree
sysbus LoadELF @/path/to/zImage
sysbus LoadFdt @dts/zynq-fir-dma.dtb 0x00008000 "console=ttyPS0,115200 root=/dev/ram0 rw" true

# Configure UART
showAnalyzer uart0

# Start
start
```

## Running the Demo

### 1. Build Device Tree

```bash
cd dts
dtc -I dts -O dtb -o zynq-fir-dma.dtb zynq-fir-dma.dts
```

### 2. Build Verilator Library

```bash
cd ../verilator_cosim
make
# Produces libVfir_stream_top.so
```

### 3. Start Renode

```bash
renode scripts/demo_fir_dma.resc
```

### 4. In Renode Monitor

```
# Wait for Linux boot
# Check device tree
(machine-0) sysbus.cpu PerformanceInMips 100

# Load driver (if not built into kernel)
(machine-0) sysbus LoadFile @/path/to/fir_dma.ko 0x80000000
```

### 5. In Linux Guest (UART)

```bash
# Load driver
insmod /lib/modules/.../fir_dma.ko

# Verify
cat /proc/interrupts | grep fir
ls /sys/devices/platform/fir/

# Configure and run
echo "8192 16384 16384 8192" > /sys/devices/platform/fir/coefficients
echo "0 0 0 0 0 0 0 0 0 0 32767 0 ..." > /sys/devices/platform/fir/input_data
echo 32 > /sys/devices/platform/fir/len
echo 1 > /sys/devices/platform/fir/start

# This blocks until DMA completes
cat /sys/devices/platform/fir/output_data
```

## Debugging

### Check DMA Interrupt Wiring

```
(machine-0) sysbus WhatPeripheralsAreOnInterrupt 34
(machine-0) sysbus WhatPeripheralsAreOnInterrupt 35
# Should show axi_dma
```

### Monitor DMA Registers

```
(machine-0) sysbus ReadDoubleWord 0x40400004  # MM2S_DMASR
(machine-0) sysbus ReadDoubleWord 0x40400034  # S2MM_DMASR
# Check for error bits or idle status
```

### Monitor AXI Stream

Enable logging in Verilator peripheral:

```
(machine-0) fir_filter LogLevel 3
# Shows TVALID, TREADY, TDATA, TLAST transactions
```

### Check DMA Transactions

```
(machine-0) axi_dma LogLevel 3
# Shows MM2S reads, S2MM writes, completion IRQs
```

### VCD Trace

If Verilator library built with `--trace`:

```
(machine-0) fir_filter DumpWaveforms "/tmp/fir_dma.vcd"
# Generates VCD for GTKWave analysis
```

## Key Concepts

### AXI Stream in Renode

- Connectors link DMA and custom peripherals
- TVALID, TREADY, TDATA, TLAST signals
- Renode handles flow control and buffering

### DMA Memory Access

- DMA reads from guest DRAM (via `sysbus` memory)
- Physical addresses from driver (`dma_addr_t`)
- Coherent buffers: Renode models cache coherency

### Co-Simulation Timing

- DMA runs in Renode virtual time
- Verilator FIR runs cycle-accurate
- Renode advances Verilator time domain on AXI Stream transactions

## Performance Analysis (Functional Only)

Renode is not suitable for absolute timing comparisons across MMIO vs DMA because RTL and virtual devices run on different clocks/timing models. Use this demo to validate DMA programming flow and correctness (IRQ, data movement). We will collect performance on real hardware and update materials accordingly.

## Integration Notes

- DMA controller is a Renode built-in model (`DMA.XilinxAXIDMA`)
- FIR filter is Verilator-based custom peripheral
- Driver interacts with both via MMIO and DMA APIs
- AXI Stream connects hardware blocks transparently

## References

- Renode AXI DMA: `src/Infrastructure/src/Emulator/Peripherals/DMA/XilinxAXIDMA.cs`
- Verilator integration: Renode documentation
- Week 9 Renode setup (interrupt-driven FIR)
