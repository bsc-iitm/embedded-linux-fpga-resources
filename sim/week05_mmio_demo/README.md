# Week 5 — Smart Timer Platform Driver (autoload via DT)

This is a minimal platform driver that binds to a Device Tree node with
`compatible = "acme,smart-timer-v1"` and maps a single MMIO resource via
`devm_ioremap_resource()`. It exposes a few sysfs attributes to prove access.

It is intended for the Week 5 demo to show that, once the DTB describes the
hardware, the kernel can discover and autoload the driver (details of the
discovery/binding process will be covered in Week 6).

## Build (out-of-tree)

Set `KDIR` to the kernel build directory matching the guest kernel inside Renode
and cross toolchain variables as needed (examples for ARM):

```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make -C /path/to/linux M=$(pwd) modules
# or
make KDIR=/path/to/linux
```

Output: `smarttimer_platform.ko`

Now we also need to update the module into `initramfs`.  Follow the procedure from Week 4 to compile
`busybox` and set up the `/tmp/initramfs` first.  Then, before recompiling the kernel, run

```bash
make -C "$KDIR" M="$(pwd)" modules_install INSTALL_MOD_PATH=/tmp/initramfs
```

in the present directory.  This will compile and install the module into the appropriate subfolder.  
If necessary, some subfolders may need to be created, or the `make` command may need to be re-run.

## Device Tree requirement

Ensure your DT has a node under `/axi` (per Week 5) that matches the driver’s
compatible and resource window:

```dts
axi {
  smarttimer0: smart-timer@70000000 {
    compatible = "acme,smart-timer-v1";
    reg = <0x70000000 0x1000>;
    status = "okay";
  };
};
```

## Autoload in the initramfs

For autoload via DT modalias → modprobe:
- Copy the module into the guest’s modules tree, e.g.:
  - `lib/modules/$(uname -r)/kernel/drivers/misc/smarttimer_platform.ko`
  - or `lib/modules/$(uname -r)/extra/smarttimer_platform.ko`
- Generate module dependency/alias files inside the image:
  - Run `depmod -a $(uname -r)` against the target root (during initramfs build).
    This produces `modules.dep{,.bin}` and `modules.alias{,.bin}` used by modprobe.
- Ensure `modprobe` (and udev or mdev rules that act on uevents) is available in the initramfs.
  - BusyBox `modprobe` is sufficient; uevents from OF devices will trigger modalias lookups.

On boot, when the kernel instantiates the DT device, it emits a uevent with the
OF modalias (derived from `MODULE_DEVICE_TABLE(of, ...)`). `modprobe` will use
`modules.alias` to load `smarttimer_platform.ko` automatically. You should then
see a `probe` message in `dmesg`.

If autoload is not available in your minimal image, you can still verify binding
with a manual `insmod smarttimer_platform.ko`.

## Sysfs (minimal demo)

The driver creates attributes on the device to read/write registers:
- `ctrl` (RW), `period` (RW), `duty` (RW), `status` (RW; write supports W1C bit0)

Example (inside guest):

```bash
# After autoload, find the device path (example path varies):
find /sys -name ctrl -path '*smarttimer*'
cat /sys/.../ctrl
```

## Notes
- This module is intentionally simple to keep Week 5 focused. Week 6 will
  discuss discovery, autoloading, error handling, and multi-instance patterns
  in more depth.
