# Lab Operations

These are notes specifically for the on-campus lab sessions for the course.  The main focus here is that students should be able to demonstrate functionality directly on the FPGA boards, and as such we do not have Renode based experiments as part of this lab session at the moment.  This could change in future, and the pre-reqs for the lab will involve demonstrating renode based assignments.

For now, the main steps for what is expected are described below.  The actual demonstrations will be shown live during the lab sessions and students need to do these on their own and should be able to demonstrate the ability to make changes on their own.

## Goals

- Demonstrate a basic Verilog design and simulation with cocotb: fixed AXI
- Compile a Linux kernel with config for Pynq Z1 board
- Create bit files 
- Compile kernel modules
- Demonstrate bootup and communication

## Experiments

### E1: basic AXI simulation with cocotb

This is just a repeat of work already done in the main session.  This is just to be shown as proof that you are familiar with the setup for writing code and simulating it.

Code to be simulated is present in `smarttimer/rtl`.  You should be able to set up the Python env and simulate it, and add/modify an existing test.

### E2: Compile a basic Linux kernel 

- Demonstrate how to download the linux source
- Use the config as given in the course (or as updated for the labs)
- Create a standard initramfs (follow steps)
- Create vmlinux and zImage

### E3: Create a bit file with smarttimer 

- Create a Xilinx project with Zynq - target Pynq-Z1 board
  - Ensure that PL clock `FCLK0` is enabled.
- Add verilog modules for smarttimer - use RTL in `smarttimer/rtl`
- Add an ILA to monitor the AXI bus
- Add a counter and connect bits `[27:24]` to `leds` (use AXI slice IP)
- Add a pin connection XDC file that maps the LEDs to the correct pins
- Synthesize, Implement, and then Generate bit file

### E4: Compile drivers for smarttimer 

- Compile the initramfs from busybox
- Compile modules from sources and install
- Create initramfs and `vmlinux` (use `initramfs-setup.sh`)

### E5: Create boot image and boot

- Integrate the vmlinux and extracted DTS to get `image.ub`
- Place onto SD-card
- Boot

### E6: Module squarer: with MMIO and DMA

- Two versions of a simple "squarer" module are provided, along with drivers
- Create the project, compile the drivers, compile the test code
  - Use MMIO and DMA to transfer large amounts of data into and out of system
  - Measure time taken
- Vary the input size and see how DMA changes in performance
- Add an ILA to the AXI bus and see how the MMIO and DMA handle data transfer

## IMPORTANT

Must enable the level shifters!  After booting into the custom image, you need to execute the following command, otherwise the PL will not be enabled.  Question: how will you automate this?

```
devmem 0xF8000900 32 0xF
```

## USEFUL COMMANDS

```sh
# Compile busybox
make -C $BDIR -j$(nproc) CROSS_COMPILE=arm-linux-gnueabihf- install CONFIG_PREFIX=/tmp/initramfs
# Set up the initramfs links
$LDIR/initramfs-setup.sh
# Compile modules (in individual folders)
# First run `make` to compile, then 
make -C "$KDIR" M="$(pwd)" modules_install INSTALL_MOD_PATH=/tmp/initramfs
# Compile the kernel with the new initramfs
make -j -C $KDIR
# Compile the DTB if needed
dtc -I dts -O dtb pynq-z1.dts -o binfiles/pynq-z1.dtb
# Go into `binfiles` and run
mkimage -f boot.its
```

