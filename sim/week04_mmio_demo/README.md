# Week 4 Linux Module (ARM) — mmio_demo

## Build
- Ensure you have an ARM Linux kernel tree built for Renode and headers available.
- Set `KDIR` to the kernel build directory (with a configured `.config`).
- Cross-compile:

```
make -C sim/week04_mmio_demo \
  KDIR=/abs/path/to/linux/build \
  ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

Artifacts: `mmio_demo.ko`

## Run inside Renode Linux
- Copy `mmio_demo.ko` into the guest (e.g., initramfs or a shared/9p mount).
- Load with base address from the Renode platform:

```
insmod mmio_demo.ko phys_base=0x70000000 window_size=0x1000
```

- Interact:
  - `cat /dev/mmio_demo`
  - `cat /sys/devices/virtual/misc/mmio_demo/ctrl`
  - `echo 0x2 > /sys/devices/virtual/misc/mmio_demo/ctrl` (W1P pulse)
  - `echo 0x1 > /sys/devices/virtual/misc/mmio_demo/status` (W1C)

> Week 5 will switch to DT + `platform_driver` with `devm_ioremap_resource()`.

