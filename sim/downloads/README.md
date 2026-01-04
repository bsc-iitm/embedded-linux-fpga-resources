# Downloads Directory

This directory is for kernel sources, busybox, and compiled binaries that you'll build during the exercises.  As mentioned in the top level README, you should ensure that you have set up a symlink properly so that this is accessible as `~/Desktop/downloads`.

## Required Downloads

### Renode emulator installation

```bash
cd sim/downloads
wget https://github.com/renode/renode/releases/download/v1.16.0/renode-1.16.0.linux-portable.tar.gz
tar -xf renode-1.16.0.linux-portable.tar.gz
```


### Linux Kernel Source

Download the Linux kernel source (version 6.6 recommended):

```bash
cd sim/downloads
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

### Busybox Source

Download busybox for creating the initramfs:

```bash
cd sim/downloads
# wget https://busybox.net/downloads/busybox-1_32_0.tar.gz
wget https://launchpad.net/busybox/main/1.32.0/+download/busybox-1.32.0.tar.bz2
tar -xf busybox-1.32.0.tar.bz2
```

#### Configure and build busybox 

(note: here `$SIM` refers to the path where the top level folder for exercises is.)

```bash
cd busybox-1.32.0
# Copy in a known good config - there are some issues with some packages like `tc`
cp $SIM/week04_renode/config-busybox .config
# Set up some include files etc for compilation
make oldconfig
make -j$(nproc) CROSS_COMPILE=arm-linux-gnueabihf- install CONFIG_PREFIX=/tmp/initramfs
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
