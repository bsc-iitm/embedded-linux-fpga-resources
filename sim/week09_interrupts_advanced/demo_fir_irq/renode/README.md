# Renode Configuration - FIR Filter with IRQ

## Files Needed

1. **Device Tree**: `dts/zynq-zed-fir-irq.dts`
2. **Peripheral Overlay**: `overlays/fir_cosim.repl`
3. **Startup Script**: `scripts/demo_fir_irq.resc`

## Device Tree

```dts
fir: fir@71000000 {
    compatible = "acme,fir-q15-irq-v1";
    reg = <0x71000000 0x1000>;
    interrupts = <GIC_SPI 34 IRQ_TYPE_LEVEL_HIGH>;
};
```

**SPI 34** → GIC hardware ID 66 (32 + 34)

Compile:
```bash
dtc -I dts -O dtb -o fir_irq.dtb dts/zynq-zed-fir-irq.dts
```

## Peripheral Overlay (.repl)

```
fir: CoSimulated.CoSimulatedPeripheral @ sysbus <0x71000000, +0x1000>
    frequency: 10000  # 10 kHz update rate
    cosimToRenodeSignalRange: <0, +1>  # GPIO[0] = irq_out
    0 -> gic@66  # SPI 34 → GIC ID 66

cpu: CPU.ARMv7A @ sysbus
    cpuType: "cortex-a9"
    genericInterruptController: gic
```

Verify:
```
(monitor) sysbus WhatPeripheralsAreOnInterrupt 66
fir
```

## Startup Script (.resc)

```tcl
mach create "fir_irq_demo"
machine LoadPlatformDescription @platforms/cpus/zynq-7000.repl
machine LoadPlatformDescription @overlays/fir_cosim.repl

# Load Verilator library
fir SimulationFilePathLinux @/path/to/libVfir.so

# Load kernel and DTB
sysbus LoadELF @/path/to/zImage
sysbus LoadFdt @fir_irq.dtb 0x100 "console=ttyPS0,115200 root=/dev/ram"
sysbus LoadBinary @/path/to/initramfs.cpio 0x4000000

showAnalyzer sysbus.uart1
start
```

## Testing in Renode

```bash
renode scripts/demo_fir_irq.resc
```

In guest:
```bash
insmod /lib/modules/fir_irq.ko

# Configure
echo "8192 16384 16384 8192" > /sys/devices/platform/fir/coefficients
echo "0 0 0 0 0 0 0 0 0 0 32767 0 0 0 0 0 ..." > /sys/devices/platform/fir/input_data
echo 32 > /sys/devices/platform/fir/len

# Start (non-blocking)
echo 1 > /sys/devices/platform/fir/start

# Read (blocks until DONE interrupt)
cat /sys/devices/platform/fir/output_data
# Should print: 8192 16384 16384 8192 0 0 ...

# Verify interrupt
cat /proc/interrupts | grep fir
#  66:          1   GIC-0  fir
```

## Debugging

```
(monitor) logLevel 3 fir
(monitor) logLevel 3 gic
(monitor) sysbus LogPeripheralAccess fir
```

Check VCD trace:
```bash
gtkwave fir.vcd &
# Watch: irq_out, status_done, processing_complete
```
# Renode

- SPI 34 → GIC 66 wiring (`0 -> gic@34`) in the overlay.
- Device tree fragment at `renode/dts/zynq-zed-fir-irq.dts`.
- Script `renode/demo_fir_irq.resc` boots kernel + DTB and points to `build/libVtop.so`.
