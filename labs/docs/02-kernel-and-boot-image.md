# 02 - Kernel and Boot Image

This stage gets you from source code to a custom kernel booting on the board,
with no custom hardware yet. The goal is to walk through every step needed to
produce a bootable `image.ub` and see the kernel come up on the serial console.
Every later experiment reuses this same procedure - only the drivers and the
device tree change.

The board boot chain is: first-stage bootloader -> U-Boot -> kernel. We do not
rebuild the first two stages. The `BOOT.BIN` already on the SD card's partition
1 is the first-stage bootloader and U-Boot; its job is to find `image.ub` and
load it. We only ever replace `image.ub`.

## Step 1: Build the kernel, busybox and initramfs

Go to the `downloads` folder and follow its
[README](../downloads/README.md) to download and build the Linux kernel and
busybox. After that you should have:

- `/tmp/initramfs` - the initial RAM filesystem the kernel mounts at boot. We
  will install drivers into it later and regenerate the image.
- a compiled kernel at `$KDIR/arch/arm/boot/zImage`.

The minimal initramfs (device nodes and an `/init` that drops to a shell) is
created by the helper script in this folder:

```bash
$LDIR/initramfs-setup.sh
```

When you later add kernel modules, install them into the initramfs *before*
rebuilding the kernel:

```bash
# in each module folder, after `make`
make -C "$KDIR" M="$(pwd)" modules_install INSTALL_MOD_PATH=/tmp/initramfs
# then rebuild the kernel so the initramfs is repacked into the zImage
make -j -C $KDIR
```

## Step 2: Build the device tree blob

`image.ub` contains two things: the kernel and a device tree. The reference
device tree for these labs is [pynq-z1.dts](../pynq-z1.dts). Compile it to a
blob (DTB):

```bash
cd $LDIR
mkdir -p binfiles
dtc -I dts -O dtb -o binfiles/pynq-z1.dtb pynq-z1.dts
```

*Note*: the same DTB works on the Pynq-Z2 - both boards use the same Zynq
device.

## Step 3: Build the boot image (image.ub)

`image.ub` is a U-Boot FIT image that packages the `zImage` and the DTB
together. It is described by [boot.its](../boot.its), which references
`binfiles/zImage` and `binfiles/pynq-z1.dtb` with the load and entry addresses
set to `0x00080000`.

Run `mkimage` from `$LDIR` (the folder that holds `boot.its`). `mkimage` invokes
`dtc`, which resolves the `/incbin/` data files *relative to the location of the
`.its` file*, so the paths in `boot.its` are written relative to `$LDIR`:

```bash
cd $LDIR
cp $KDIR/arch/arm/boot/zImage binfiles/zImage   # copy the freshly built kernel in
mkimage -f boot.its binfiles/image.ub
```

*Copy, do not symlink:* `mkimage`/`dtc` does not reliably read a symlinked
`zImage` here (it reports `Couldn't open "zImage"`), so copy the real file. The
trade-off is that the copy does not track later kernel builds - **every time you
rebuild the kernel you must re-run the `cp` above before `mkimage`**, or
`image.ub` will contain a stale kernel.

## Step 4: Copy to the SD card

The Pynq SD image has two partitions. Partition 1 has only a few boot files;
partition 2 is the full Pynq root filesystem (unused here). **Only touch
`image.ub` on partition 1** - leave every other file alone.

Plug the SD card into the host with a USB reader. Copy the new `image.ub` onto
partition 1, overwriting the existing one (file manager or `cp` both work).
Safely eject the card and insert it into the board.

## Step 5: Boot and view the serial console

Power and console both come over a single mini-USB cable.

- **Jumpers**: set the board to *SD card* boot mode, and set the power-source
  jumper to draw power from USB.
- Install and configure `gtkterm`:
  - Port: `/dev/ttyUSB1` (usual). If that fails, run `sudo dmesg` after
    powering the board and use the port it reports.
  - Params: `115200 N 1` (speed, no parity, 1 stop bit).
- The same physical port carries both UART0 and UART1, so you may see some
  messages duplicated.

Power on (or press reset after changing settings). You should see:

1. U-Boot messages first - do **not** press a key, or U-Boot will stop and wait
   for input.
2. Kernel messages from the kernel you just built. The first few lines appear
   quickly, then there is a pause of roughly 40-50 seconds before it finishes
   coming up and gives you a shell.

At this point you have booted a custom kernel on the board. Every subsequent
experiment repeats Steps 1-5 with custom hardware and drivers added.

---

## Appendix: working with FIT images

Useful when debugging the image or starting from an existing `image.ub`.

| Task | Command |
|------|---------|
| List FIT contents and metadata | `dumpimage -l image.ub` |
| Extract component N | `dumpimage -T flat_dt -p N -o output image.ub` |
| Build FIT image (from `$LDIR`) | `mkimage -f boot.its binfiles/image.ub` |
| Dump FIT structure | `fdtdump image.ub` |

To extract the kernel and DTB from a working reference image (component 0 is
the kernel, component 1 is the DTB):

```bash
dumpimage -T flat_dt -p 0 -o extracted-kernel image.ub
dumpimage -T flat_dt -p 1 -o extracted.dtb   image.ub
```

The load and entry addresses in `boot.its` come from `dumpimage -l` on a
working image. Common problems:

- **Wrong load address**: kernel hangs or crashes immediately. Verify against a
  known-good image with `dumpimage -l`.
- **Using vmlinux instead of zImage**: produces a huge image; always use the
  compressed `zImage` from `arch/arm/boot/`.
- **DTB mismatch**: peripherals fail to initialise. Use a DTB that matches the
  kernel version.

Next: [03 - Smart timer](./03-smarttimer.md).
