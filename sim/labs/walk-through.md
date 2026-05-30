# Lab session walkthrough

This is a detailed walkthrough for the lab experiments to be done and demonstrated in the *in-person* lab sessions.  There are two major experiments that will be covered here:

- Basic smart timer + platform driver + interrupt driver
- DMA based communication with streaming peripheral

The `smarttimer` is the same as what was used in the video demos: it is basically a counter with a pulse-width modulation (PWM) output, and you can control the duty cycle and period, as well as start and stop the timer, through a platform driver interface.  The communication will be done using the AXI-lite interface, and we will also see how to use an ILA to check the behaviour of the bus.

The DMA experiment is meant to show a slightly advanced usage of communication between the processor and peripherals.  The DMA module is capable of transferring data in burst mode at high speed and convert it to a streaming interface on the FPGA side, which will result in higher throughput when trying to transfer data between the processor and the logic.


## Pre-requisites and assumptions

This walkthrough is limited to what is required for the labs: they will NOT cover general background material on the course, and may also not be helpful to understand the rest of the course material.  In particular, `renode` will not be discussed in the labs: these exercises can be done purely in simulation, and the focus of the lab sessions is to get familiar with using the board itself.

You should have already covered all the lecture material, so the terms used here should be familiar with you: for example *module*, *kernel*, *driver*, *interrupt* etc. will be discussed as in the lectures and the demos shown in the videos.

You also need to be comfortable with running commands on the Linux command line, and the meaning and use of Environment variables.  Some env vars and scripts will be used here to make the compilation process easier, but you are welcome to experiment with creating your own scripts and combinations of command sequences.

## Terminology and Board Information

- **Zynq**: A family of FPGAs from Xilinx (AMD) that also contain an ARM processor built into them.  The ARM processor is called a *hard IP* or *hard macro* because it is directly designed into the chip, and supports only limited configuration.  
- **PS**: This is the *Processing System* on the Zynq platform - the ARM processor.  You can configure some aspects of this, such as the clock, interrupt connections etc., but things like the processor instruction set, the bus width etc. are fixed.
- **PL**: This is the *Programmable Logic* - the actual FPGA part, which contains lookup tables (LUTs) and registers or flip-flops (FFs).  It also contains memory blocks (BRAM) and multiplier units (DSP slices), though we will not be making use of those here.
- **Pynq**: This is a software framework - a set of libraries and programs in Python that provide functions to communicate with specific types of hardware on the board.  Pynq is not directly provided by Xilinx - this was developed independently as Open Source, and can be adapted to other boards.  Pynq is provided as a complete Linux distribution, with which you can boot into a Linux shell, and also connect to a Jupyter notebook server and run Python scripts.

The board we will use in the lab is a Pynq board - either the Pynq-Z1 or Pynq-Z2 (both are supported and available in the lab).  The main things to note here are:

- Part number: it uses the Xilinx *XC7Z020CLG400-1* FPGA.  This is a Zynq processor (XC7Z family), fairly small (010 is the smallest in the family, but there are much bigger variants - 020 is second), has 400 pins (CLG400 package) and the `-1` refers to the speed grade.  It is possible that the actual speed grade may be higher, but we choose the lowest speed grade (slowest chip), since anything that works on that should also work on faster chips without problems.  The part number is important when you create a project.
- XDC file: You will need to map some of the outputs to the LEDs present on the board.  The connections to the LEDs are fixed by whoever designed the board, so you cannot just choose any value you want here.  The pin locations are given as a coordinate system: for example `G10` would refer to row `G`, column `10` or something like that.  You should get the actual XDC file for the board either from the Pynq website or search online, and make sure only the pins you actually need are uncommented.  The IO voltage standard (`LVCMOS33` for low-voltage 3.3V CMOS) must also be as per the specifications given by the board designers.

## Basic Linux Environment and Setup

These instructions have been tested on a Linux system running Ubuntu 22.04 or 24.04 (both are known to work).  The lab machines already have the Xilinx software installed.  

If you are setting up your own system, please use Vivado 2021.1 as that is the version that has been tested.

In order to get the Vivado GUI working, you will most likely need to install a package called `libtinfo5` which is not available by default on Ubuntu.  Newer versions of Vivado may work better, but have not yet been tested (they should work - the designs here are quite simple).

You should have a system with at least 8 GB RAM (16 GB is preferred) - the Vivado compilation struggles even with 8GB.  You will probably need 10-20GB disk space apart from what is needed for installing Vivado.

The Pynq-Z1 board that you use requires an SD card to boot.  You should first download the Pynq base image and flash it to the SD card - you don't really need this, but we will be replacing the `image.ub` file that is used there with our own version, so you need to start with a working SD card.  Installing this is not described here.

Make sure the following packages are installed on your system (you can run the commands below):


```bash
# Install various packages
sudo apt-get update
sudo apt-get install curl

# Device tree compiler (used to build the DTB for the boot image)
sudo apt-get install device-tree-compiler

# ARM cross-compilation toolchain
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# Build essentials
sudo apt-get install build-essential flex bison libssl-dev libelf-dev cmake git
```

### Repository checkout

All the following instructions will assume that you check out the repo using git, and then run the remaining commands inside the folder that is created as a result.  Do the following either from your home directory or some other suitable directory:

```bash
git clone https://github.com/bsc-iitm/embedded-linux-fpga-resources.git
cd embedded-linux-fpga-resources/sim/labs
# Open a new shell with suitable environment
./setup.sh
```

### Bootable kernel image

Follow the instructions in the document to [create a bootable image](./create-boot-image.md).  

## Experiment 1

After you have completed all the steps till here, you are ready to boot a custom kernel onto the board.  Now we will proceed to create our custom designs and put them on the board.

Follow the instructions for [smart timer](./smart-timer.md) to create a bit file for the smart timer.  Once this is done, boot up the kernel and test as described here.

Once you have copied the `image.ub` with the drivers for the smart timer, the system will boot and the serial port will show the relevant messages.  

While this is happening, you can open the hardware manager in Vivado to connect to the board.  It should show the `xc7z020` device and allow you to program the FPGA with the bit file that you generated by following the instructions above.

### Activating the level shifters for PL

The programmable logic (PL) is separate from the processing system (PS) by means of some protection circuitry in the form of level shifters.  These are present inside the FPGA and are controllable through the software, since there are registers that enable them and these registers are addressable from inside the ARM processor.

The command to turn on the PL is

```
devmem 0xF8000900 32 0xF
```

You will need to manually do this every time after you boot up, in order to activate the PL.  Without this, no clock signals are available to the PL, and debugging using ILA is also not possible.  Note that you can do this before you actually program the bitfile.

### Program the bitfile and load drivers

Program the bitfile onto the FPGA fabric using the JTAG connection.  If you had already activated the level shifters, then you should see the ILA signals.  If not, then you may need to refresh the display after turning on the level shifters.

Once that is done, you can load the drivers from the shell interface (serial port connection).  

```
modprobe smarttimer_blocking
```

should load the driver for the blocking interface.

Now you can examine `/sys/bus/platform/devices/` and you should be able to find a device for the smart timer.  This folder should contain files for `duty`, `period`, `ctrl` and `status`, and allow you to interact with the hardware module that was created with the RTL code that you synthesized.

Writing `1` to the `ctrl` file will activate the timer.

Depending on which driver you loaded, you should see a response to the interrupts.

## Experiment 2

The purpose of this experiment is to create a design where we send large chunks of data into the PL, and see two ways by which this can be done:

- AXI lite bus interface
- Streaming interface fed by AXI DMA

The AXI lite interface transfers data one at a time, and generally has a significantly slower throughput than burst mode transfers.  Since we don't want to implement a full AXI master interface (it is quite complex) we take the easy way out and let the AXI DMA module do this for us: it can handle burst transfers, and convert them to a streaming interface on the PL side.

A streaming interface basically means that data gets transferres based on the `valid/ready` handshake, and there is no addressing involved.  This generally makes it possible to have higher throughput since we don't need to request data one address at a time.

Note that the AXI DMA is different from the DMA that is already present inside the ARM processor system: that cannot be directly accessed by the fabric, but is used for things like ethernet that are completely inside the SoC and do not directly communicate with the fabric.

### Addressing

Make sure the addresses specified in the map match with the entries in the DTS, and of course make sure the corresponding entries in the DTS are uncommented.  Also comment out the `smarttimer` entry since that will otherwise clash with the IRQ entry for the DMA.

### Drivers

The drivers for both forms are in the same folder and can be compiled at once and installed using the same steps as for Experiment 1

### Test code

In addition to the drivers, we also want a test code that can actually use the drivers to communicate from the Linux userspace to the hardware.  This is provided in the `sw` folder, and can be compiled using a simple `make` command.  After that, this file needs to be manually copied into the initramfs before building the kernel:

```sh
cd $LDIR/squarer/sw
make
cp ./test_squarer /tmp/initramfs
# Recompile the kernel
make -j4 -C $KDIR
# Create the image file
```

After all this is done, once again copy the `image.ub` to the SD card, boot up, program the bit file, turn on the level shifters, load the drivers, and finally run the test code:

```sh
./test_squarer          # without argument runs with N=1024
./test_squarer 2048     # can provide an argument - try numbers between 8 to 2048
```

