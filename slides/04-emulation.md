---
marp: true
paginate: true
title:  Week 4 - Emulation
author: Nitin Chandrachoodan
theme: gaia
style: |
  .columns-2 {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 1rem;
    font-size: 0.75em;
  }
  .columns-3 {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 1rem;
    font-size: 0.75em;
  }
math: mathjax
---

<!-- _class: lead -->

# Emulation: System Level Design

---

## Objectives

- Simulation vs. Emulation
- Introduction to Renode for emulation
  - Bare Metal, System specification, booting a stock kernel
  - `peek`/`poke` - bus inspection
- Linux kernel compilation
  - Config, busybox, initramfs
- Integrating a module

---

## Simulation 

- *Model* of system behaviour
- Mimics the **what**, not necessarily the **how**
- Example: Verilog simulator
  - Track signal changes 
  - Mathematical model of gates: behavioural/gate-level
  - Simulation speed unrelated to real-world timing

---

## Emulation

- Recreate *exact* behaviour of a system (define *exact*?)
- Example: run binary code generated for a processor
  - Instruction set level emulation
  - Gate level emulation (behaviour of each gate)
  - Cycle-accurate emulation 
- **Goal**: Functional equivalence with system

---

## Renode

- Open source software development framework (from Antmicro)
- Emulate several processor families
  - Instruction-accurate emulation
- Full system emulation
  - Standard peripherals like UART, SPI etc.
  - Custom peripherals through software extensions
- Co-simulation with RTL

---

## Installation of Software

- `sudo apt update` - for installing packages
- `sudo apt install gcc-arm-none-eabi` - compiler for bare-metal
- `sudo apt install gcc-arm-linux-gnueabihf` - compiler for Linux
- `wget https://builds.renode.io/renode-latest.linux-portable.tar.gz` - download renode
  - `tar xvzf renode-latest.linux-portable.tar.gz` - unpack
  - *Rename the folder if desired for ease of use*

---

## Demo: Basic Renode System + Bare Metal ELF

- [Bare metal renode script](../sim/week04_renode/week4_bare.resc)
- First compile the `bare.elf` - inside the `sim/week04_renode/bare` folder
- start renode, run `i @week4_bare.resc`
- Run the code with `start` inside renode monitor
- Examine bus structure `sysbus WhatIsAt 0x70000000`
- Examine memory addresses `sysbus ReadDoubleWord 0x70000004` etc.

---

## Demo: Stock single-node VExpress with Linux

- stock example from renode source code
- downloads kernel and rootfs
- loads into appropriate areas of memory
- boots into full Linux system
  - some errors, no modules etc.: extremely basic

```
i @scripts/single-node/vexpress.resc
```

---

## Demo: Stock single-node Zynq system with Linux

- more complex example with peripherals - ZedBoard
- downloads kernel, rootfs, and Device Tree Blob (DTB)
- Functioning Linux bootup

```
i @scripts/single-node/zedboard.resc
```

---

## Downloading Linux kernel and friends

- `sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf bc bison flex libssl-dev make dwarves cpio`
- https://www.kernel.org/
  - Download https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.110.tar.xz
  - Extract `tar -xf linux-6.6.110.tar.xz`
- Busybox
  - https://www.busybox.net/downloads/busybox-1.32.1.tar.bz2
- Put the appropriate `.config` files in the extracted folders
  - Files are available in `sim/week04_renode` - rename as needed

```
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make -j
```

---

## Demo: Preparing our own kernel

- Download LTS kernel code
- Use standard config and compile
- Download busybox code, compile with standard config
- Create initramfs
- Boot

---

## Demo: Custom module

- Build module against our compiled kernel tree
- `insmod` to load kernel after getting it into initramfs

---

## Summary

- System emulation lets us iterate fast on full stacks
  - Renode wires CPU, RAM, UART, and MMIO
- Bare‑metal bring‑up: load ELF, poke registers
- Stock Linux boot
- Custom kernel: 
  - pick `multi_v7_defconfig`, set console cmdline, bundle initramfs; produce `vmlinux`/`zImage`
- Zynq specifics matter (DRAM base, reset PC, possible address redirects)
