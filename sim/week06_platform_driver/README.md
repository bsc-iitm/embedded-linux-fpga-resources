# Week 6 — Platform Driver (Clean Walkthrough)

Purpose: a teaching-focused, minimal platform driver that binds via Device Tree and exposes a few sysfs knobs for the Smart Timer. This is a cleaned, step-by-step version meant for Week 6 discussion (binding, `devm_*`, sysfs patterns).

Reuses the same DT setup and Renode machine from Week 5. If you already have the Smart Timer node in your DTB and the Renode stub at 0x70000000, you can load this module in the guest to see probe/binding and basic MMIO access.

## Build (out-of-tree)

Environment variables (example for ARM):

```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make -C /path/to/linux M=$(pwd) modules
# or
make KDIR=/path/to/linux
```

Output: `smarttimer_platform.ko`

Optionally, install into your initramfs before packing it, similar to Week 5:

```bash
make -C "$KDIR" M="$(pwd)" modules_install INSTALL_MOD_PATH=/tmp/initramfs
```

## Device Tree

Same node as Week 5 (compatible and reg window):

```dts
axi {
  smarttimer0: smart-timer@70000000 {
    compatible = "acme,smart-timer-v1";
    reg = <0x70000000 0x1000>;
    status = "okay";
  };
};
```

## Run in Renode

Use the Week 5 `.resc` script that already overlays a simple `Memory.MappedMemory` at `0x70000000` and boots a Zynq Linux image with your DTB. Inside the guest, run:

```bash
insmod smarttimer_platform.ko
dmesg | tail -n +1 | grep smarttimer
```

Interact via sysfs (paths vary; search for the attributes):

```bash
find /sys -name ctrl -path '*smarttimer*'
cat /sys/.../ctrl
echo 1 > /sys/.../ctrl   # set EN bit, for example
```

## What To Notice (Week 6 topics)

- Binding via OF match table and `compatible`.
- `platform_get_resource` + `devm_ioremap_resource` for safe mapping.
- `platform_set_drvdata` and retrieving it in sysfs callbacks.
- Small, readable sysfs attributes to poke registers.

## Week 6 DT + Renode (SmartTimer + FFT)

We add a second peripheral (FFT demo) with its own `compatible` and MMIO range.

- New DTB (full DTS copy with both nodes):
  - Source: `sim/week06_renode/dts/zynq-zed-week6.dts`
  - Build DTB:
    ```bash
    dtc -I dts -O dtb \
      -o sim/week06_renode/binfiles/zynq-zed-week6.dtb \
      sim/week06_renode/dts/zynq-zed-week6.dts
    ```
- New Renode overlay mapping both windows:
  - `sim/week06_renode/overlays/peripherals_stub_week6.repl`
  - Maps:
    - SmartTimer @ `0x7000_0000` (4 KiB)
    - FFT @ `0x7000_1000` (4 KiB)

### Update the Renode script

Copy Week 5 script and point it to the new overlay + DTB (example):

```bash
cp sim/week05_renode/week5_zynq.resc sim/week06_renode/week6_zynq.resc
```

Edit `sim/week06_renode/week6_zynq.resc`:

- Replace the overlay line with:
  ```
  machine LoadPlatformDescription @../week06_renode/overlays/peripherals_stub_week6.repl
  ```
- Point `$dtb` to the new DTB:
  ```
  $dtb=@../week06_renode/binfiles/zynq-zed-week6.dtb
  ```

Then start Renode and run:

```renode
(i) @sim/week06_renode/week6_zynq.resc
```

Inside the guest:

```bash
# Platform driver (SmartTimer)
modprobe smarttimer_platform

# FFT block demo (separate compatible)
modprobe fft_block_demo
```

Check modaliases if autoload is desired:

```bash
cat /sys/bus/platform/devices/*smart*/modalias
cat /sys/bus/platform/devices/*fft*/modalias
```
