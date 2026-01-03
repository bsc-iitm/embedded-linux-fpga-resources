# Week 4 Renode (ARM) Demos

## Core Ideas

Building large systems requires extensive simulation.  Simulating a full CPU is extremely time consuming, so we need higher level tools.

### Emulation vs Simulation

- Verilog RTL: Simulation.  We simulate the Verilog code in software to see how the hardware would behave.  Examples include IVerilog, Verilator, Xilinx ISim etc.
- Hardware Emulation: Sometimes, the RTL simulation is too slow.  So we could speed it up by accelerating parts of this in hardware.  This is not exactly synthesis - we normally target FPGAs to partially synthesize and emulate the hardware behaviour, but are not targeting full timing closure and minimum area etc.  Examples include Synopsys Zebu, Cadence Palladium etc.
- Software/System Emulation: When we are trying to build a complete system, we want something that can "emulate" the hardware behaviour reasonably accurately, down to either the instruction set level, or even better to the cycle-accurate level.  These are typically called system emulators.  Examples include QEMU, Renode etc.

So here we are going to build a system emulation platform and run it using the renode tool, and demonstrate how we can run Linux on a system like this.  Later we will include the ability to co-simulate Verilog modules and build a system emulator that can also be used to test our hardware.

## Contents

### Installing Renode

This has to be done first.  Follow the steps in the `../downloads` folder to download and install the portable version of renode for Linux.  This also has instructions to install a package used for renode integration with verilator in later exercises.

### Bare Metal Demo
- Loosely based on the stock Renode vexpress-a9 platform and overlays:
  - CPU: Arm Cortex A9
  - RAM @ 0x8000_0000 (for the bare-metal ELF link address)
  - SmartTimer MMIO window @ 0x7000_0000 (size 0x1000)
- `bare/`: minimal bare-metal demo (sets EN, PERIOD, DUTY; pulses RST; clears STATUS).
- `week4_bare.resc`: Renode script to load the platform and run the bare-metal ELF.
- Ideally every access from the CPU to the AXI bus should be getting logged, but in practice if seems less reliable.
- Build: `make -C sim/week04_renode CROSS=arm-none-eabi-`
- Run: in Renode, `i @sim/week04_renode/week4_bare.resc`
  - Observe: `sysbus LogPeripheralAccess smarttimer` shows MMIO writes/reads reflecting W1P/W1C.

### Base Linux Demo

To boot Linux on a platform, we need several items:

- CPU + memory
- UART for seeing boot messages
- Linux kernel that has been compiled with knowledge of the peripherals (esp. UART)
- Ability to insert modules if needed

As a first cut, we will just go with the demo from Renode itself: this has been adapted to `sim/week04_renode/week4_linux_from_demo.resc`.  This just sets up a system, loads a kernel that has already been created by renode, and starts it.

The platform has a smarttimer peripheral attached, and we can examine this through `sysbus WhatIsAt 0x70000000`, but the kernel is not built with modules, so we cannot compile and run it.

### Linux Kernel Compilation

What is a kernel?  Essentially an executable file.  In fact, it basically looks like an ELF file, and needs to be loaded into an appropriate region of system memory.

Show the process of configuring a basic kernel:

- use the `multi_v7_defconfig` configuration as a generic ARM target
- add a custom command line so it will set up the console
- compile busybox and attach it to the kernel
- Compile and generate `vmlinux`.  Use `objdump` to examine this and see that it is also a general ELF file.

There are several steps here that need to be done in a very specific way:

- config of kernel
- config of busybox (turn off things like `tc` that throw compile errors)
- set up the initramfs include - this needs some very specific steps and the files need to be properly owned by root to work properly
- compile kernel to `vmlinux`
- compile to `zImage` if needed 
- set up renode platform with memory and redirects if needed - this is Zynq specific since traditional ARM kernels assumed a reset PC of 0xc0000000 or something like that, while Zynq systems tend to have DRAM at 0x0.  Depending on how we set up the platform, we may need to create a fake redirect like this.

For this set of exercises, a *known good config* has been provided.  You can use that for all the exercises, but are strongly encouraged to play around with your own configs to see what changes.  However, this can easily become hard to debug, so it is left out of scope of this course.

### Linux Modules

- Set up the kernel as above.
- Cross-compile `sim/week04_mmio_demo/mmio_demo.c` for ARM (see that folder’s README/Makefile; use Kbuild with `ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-`).
- Modify the initramfs setup by putting the `.ko` compiled output somewhere in it.  Then after boot use `insmod` to actually load and demonstrate it.

## Takeaways

- Emulation vs Simulation.  Why we need system emulators.  ISA, cycle accurate, co-simulation concepts
- Renode as a platform for emulation: basic setup with ARM and simple C code.  Examining memory contents, looking at the bus etc.  Power on reset and PC initialization.  Loading executable code into RAM.
- Booting stock Linux kernel on renode: UART for logging output.  Linux as just another executable.  Loading into specific part of RAM.  
- Compiling a custom kernel from base Linux source.  Configuration, command line parameters, initramfs
- Loading a module

## Compilation

### Busybox

Ensure your environment variables are set (should be in `.bashrc`):

```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
```

If not already done, download and install `busybox` as per the [instructions](../downloads/README.md)

Set up the `init` process as below


```
# Required device nodes
mkdir -p /tmp/initramfs/{bin,sbin,etc,proc,sys,dev,usr/bin}
cd /tmp/initramfs
sudo mknod -m 600 dev/console c 5 1
sudo mknod -m 666 dev/null    c 1 3

# Minimal /init
cat > /tmp/initramfs/init <<'EOF'
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev
echo "Userspace up; dropping to shell."

# Setting up for autoloading drivers if needed
for f in /sys/bus/platform/devices/*/modalias ; do
	[ -f "$f" ] && modprobe "$(cat "$f")" 2>/dev/null || true
done

exec /bin/sh < /dev/console > /dev/console 2>&1
EOF
chmod +x /tmp/initramfs/init
```

If you are adding a custom module, then you will need to go to the module folder and run `make` to install into this `/tmp/initramfs`.  Change the permissions below only after doing that.

```bash
# Make sure it is owned by root for correct permissions
sudo chown -R root\: /tmp/initramfs
```

### Linux Kernel

**Prerequisites:** Complete Busybox build first (kernel expects `/tmp/initramfs` to exist)

Download Linux kernel source and [compile as per instructions](../downloads/README.md).

**Note:** The Renode scripts expect to find kernel and device tree files in the local `binfiles/` directory. It is recommended that you copy the file `cp $KDIR/vmlinux binfiles/`.  You could also use symlinks, but the kernel changes for each week, so later changes may cause earlier exercises to fail.
