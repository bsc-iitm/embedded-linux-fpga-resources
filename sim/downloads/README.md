# Downloads Directory

Before doing this, please follow the instructions in [Lab README](../labs/README.md) and run the `setup.sh` script present there.  The following instructions assume you have got all the appropriate environment variables already set up.

For the lab sessions, you only need busybox and the kernel - you need renode for simulations that are not addressed in the lab sessions.

## Required Downloads

### Busybox Source

Download busybox for creating the initramfs:

```bash
cd $DL
# wget https://busybox.net/downloads/busybox-1_32_0.tar.gz
wget https://launchpad.net/busybox/main/1.32.0/+download/busybox-1.32.0.tar.bz2
tar -xf busybox-1.32.0.tar.bz2
```

#### Configure and build busybox 

Note: here `$SIM` refers to the path where the top level folder for exercises is.

It is important that you do this before trying to compile the kernel since the kernel looks for the `/tmp/initramfs` folder and the build will fail without that.

```bash
cd busybox-1.32.0
# Copy in a known good config - there are some issues with some packages like `tc`
cp $SIM/week04_renode/config-busybox .config
# Set up some include files etc for compilation - if there are any questions just
# accept the defaults for the new settings.
make oldconfig
# You can use `-j$(nproc)` to run fully parallel if you have enough RAM
# `-j4` means use 4 processors, which is a safer option to avoid running out of memory
make -j4 CROSS_COMPILE=arm-linux-gnueabihf- install CONFIG_PREFIX=/tmp/initramfs
```

#### Set up the init script

For the kernel to actually give you a shell interface to type commands, you need to make some additional files in the initramfs.  To do this, run the following commands:

```bash
cd $LDIR
./initramfs-setup.sh
```

Now you can compile the kernel and it should result in a bootable kernel image.

### Linux Kernel Source

Download the Linux kernel source (version 6.6 recommended):

```bash
cd $DL
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.tar.xz
tar -xf linux-6.6.tar.xz
```

#### Configure and build kernel

(note: here `$SIM` refers to the path where the top level folder for exercises is.)

```bash
cd linux-6.6
# Copy a known good config
# Otherwise you can go through `make defconfig` etc but there are a lot
# of params that change from one version to another.
cp $SIM/week04_renode/config-linux .config
# make oldconfig sets up scripts required for compiling
make oldconfig
# Replace 4 with a suitable number depending on the number of cores in your system
make -j4
```

Once this is done, there should be a kernel in `$KDIR/arch/arm/boot/zImage`. You will use this later to build a boot image to put on the SD card.

### Renode emulator installation

*NOTE*: This is **not** required for the in-person lab sessions.  

```bash
cd $DL
wget https://github.com/renode/renode/releases/download/v1.16.0/renode-1.16.0.linux-portable.tar.gz
tar -xf renode-1.16.0.linux-portable.tar.gz
```

### Build Artifacts

After following the kernel build instructions in `../week04_renode/README.md`, you'll have:
- `linux-6.6/vmlinux` - Linux kernel ELF
- `binfiles/` - Device tree blobs (dtb files), rootfs, etc.

## Directory Structure

Expected structure after setup:
```
~/Desktop/downloads/
├── linux-6.6/              # Kernel source (you download and build)
│   ├── vmlinux            # Built kernel
│   └── ...
├── busybox-1.36.1/        # Busybox source (you download and build)
│   └── ...
├── binfiles/              # Compiled binaries and device trees
│   ├── zynq-zed.dtb
│   ├── rootfs-minimal.cpio
│   └── ...
└── README.md              # This file
```

Note: The `.gitignore` in this directory prevents large binary files from being committed to git.
