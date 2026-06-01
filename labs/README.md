# In-Person Lab: Embedded Linux on the Pynq Board

These are the instructions for the on-campus lab sessions. The focus is to
build custom hardware in Vivado, run a custom Linux kernel on a Pynq FPGA
board, and demonstrate the hardware and its drivers working together on the
real board. You are expected to be able to make changes on your own and
demonstrate them live.

This lab is purely Vivado + on-board work. It does not use Renode or
simulation - those are covered elsewhere in the course, not here.

## What you will build

Two experiments, sharing one boot/kernel workflow:

1. **Smart timer** - a counter with a PWM output, controlled over AXI-Lite,
   with an interrupt on counter wrap. You build the block design (with an ILA
   to watch the AXI bus and a counter to blink the LEDs), then exercise it
   through a platform driver and an interrupt-driven driver.
2. **Squarer (MMIO vs DMA)** - compute `y = x * x` over a block of data two
   ways: per-sample AXI-Lite (MMIO) and bulk AXI DMA streaming. You measure and
   compare the throughput of the two paths.

## The lab, step by step

Work through the guides in order. Each builds on the previous one.

| # | Guide | What you do |
|---|-------|-------------|
| 01 | [Environment setup](./docs/01-environment-setup.md) | Board info, packages, repo checkout, `setup.sh` |
| 02 | [Kernel and boot image](./docs/02-kernel-and-boot-image.md) | Build the kernel/initramfs/DTB, make `image.ub`, boot the board |
| 03 | [Smart timer](./docs/03-smarttimer.md) | Vivado design + drivers + on-board test |
| 04 | [Squarer: MMIO vs DMA](./docs/04-squarer-mmio-dma.md) | DMA streaming design + drivers + performance comparison |

Start with 01 and 02 to get a plain custom kernel booting, then do 03 and 04
to add hardware and drivers.

## Important: enable the PL level shifters

After every boot, the programmable logic (PL) is isolated from the processing
system (PS) until you enable the level shifters. Run this on the board shell
before expecting the PL to respond:

```
devmem 0xF8000900 32 0xF
```

Without it the PL gets no clock and the ILA shows nothing. *Question to think
about: how would you automate this so you do not have to type it each boot?*

## Quick command reference

```sh
# Compile busybox
make -C $BDIR -j$(nproc) CROSS_COMPILE=arm-linux-gnueabihf- install CONFIG_PREFIX=/tmp/initramfs
# Set up the initramfs skeleton
$LDIR/initramfs-setup.sh
# Compile a module (in its folder), then install it into the initramfs
make
make -C "$KDIR" M="$(pwd)" modules_install INSTALL_MOD_PATH=/tmp/initramfs
# Rebuild the kernel so the initramfs is repacked
make -j -C $KDIR
# Compile the device tree
dtc -I dts -O dtb pynq-z1.dts -o binfiles/pynq-z1.dtb
# Build the boot image (run from $LDIR, where boot.its lives).
# Copy (do not symlink) the kernel in; re-copy after every kernel rebuild.
cp $KDIR/arch/arm/boot/zImage binfiles/zImage
mkimage -f boot.its binfiles/image.ub
```
