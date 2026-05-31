# 01 - Environment Setup

This is the first stage of the lab. By the end of it you will have the
toolchain installed, the repository checked out, and a shell with the
environment variables that the later stages depend on.

## Board and part information

- **Zynq**: A family of FPGAs from Xilinx (AMD) that also contain an ARM
  processor built into them. The ARM processor is a *hard IP* (hard macro)
  because it is designed directly into the chip and supports only limited
  configuration.
- **PS** (Processing System): the ARM processor. You can configure aspects
  such as the clock and interrupt connections, but the instruction set, bus
  width etc. are fixed.
- **PL** (Programmable Logic): the FPGA fabric - lookup tables (LUTs),
  flip-flops (FFs), block RAM (BRAM) and DSP slices (multipliers, not used
  here).
- **Pynq**: an Open Source software framework (Python libraries plus a Linux
  distribution) for talking to hardware on the board. We do not use the Pynq
  software stack itself, but we start from the Pynq SD card image.

The lab uses a **Pynq-Z1** or **Pynq-Z2** board (both are available and both
work). Key facts:

- **Part number**: `XC7Z020CLG400-1`. Zynq XC7Z family, the 020 device (second
  smallest), CLG400 package (400 pins), `-1` speed grade (slowest - anything
  that works here works on faster grades). You need this exact part when you
  create a Vivado project.
- **XDC file**: you map some outputs to the on-board LEDs. The LED pin
  locations are fixed by the board designer, so you cannot choose arbitrary
  pins. Get the official XDC for the board from the Pynq website (or search
  online) and uncomment only the pins you actually use. The IO standard for
  the LEDs is `LVCMOS33` (3.3V CMOS).

## System requirements

These instructions are tested on Ubuntu 22.04 and 24.04 (both work). The lab
machines already have the Xilinx tools installed.

- **Vivado 2021.1** if you are setting up your own machine (this is the tested
  version). Newer versions should work for these simple designs but are not
  tested.
- On recent Ubuntu the Vivado GUI usually needs the `libtinfo5` package, which
  is not installed by default.
- At least **8 GB RAM** (16 GB preferred - Vivado struggles at 8 GB), and
  **10-20 GB** of free disk on top of the Vivado install.

The board boots from an SD card. Flash the standard Pynq base image to the SD
card first: we will only replace the `image.ub` file on partition 1, so you
need to start from a working card. Flashing the base image is not covered here.

## Required packages

```bash
sudo apt-get update
sudo apt-get install curl

# Device tree compiler (used to build the DTB for the boot image)
sudo apt-get install device-tree-compiler

# ARM cross-compilation toolchain
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# Build essentials
sudo apt-get install build-essential flex bison libssl-dev libelf-dev cmake git

# U-Boot tools (provides mkimage / dumpimage for the boot image)
sudo apt-get install u-boot-tools

# Serial terminal for the board console
sudo apt-get install gtkterm
```

## Repository checkout and environment

All later steps assume you have checked out the repository and are working
inside `sim/labs`:

```bash
git clone https://github.com/bsc-iitm/embedded-linux-fpga-resources.git
cd embedded-linux-fpga-resources/sim/labs
# Open a new shell with the lab environment set up
./setup.sh
```

`setup.sh` exports the variables used throughout the lab and drops you into a
fresh shell:

| Variable | Meaning |
|----------|---------|
| `ARCH` | `arm` |
| `CROSS_COMPILE` | `arm-linux-gnueabihf-` |
| `LDIR` | the `sim/labs` folder |
| `SIM` | the `sim` folder (parent of `labs`) |
| `DL` | the `sim/downloads` folder |
| `KDIR` | kernel source/build dir (`$DL/linux-6.6`) |
| `BDIR` | busybox source dir (`$DL/busybox-1.32.0`) |

You are welcome to write your own scripts or command sequences instead - the
variables above are just there to keep the later commands short.

Next: [02 - Kernel and boot image](./02-kernel-and-boot-image.md).
