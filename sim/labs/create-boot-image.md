## Basic Linux kernel bootup

Once you have followed the steps for setup, you will have a new `bash` shell with several environment variables that are needed for compilation.  Now we will look at what we need to boot up a basic image without any hardware on it.  This is just to get through all the steps needed to make a bootable image on the SD card.

The steps described in this document are generic across all the kernels we will compile.  After compiling busybox, we should compile the modules, install them into the appropriate place, and then recompile the kernel to create a `zImage` file, with which we create the bootable image as described below.  

### Kernel and Busybox compilation

Go the `../downloads/` folder and follow instructions in the [README](../downloads/README.md).  Once this is done, you should have the following:

- `/tmp/initramfs` folder containins the initial RAM filesystem that will be used by the kernel to find files, modules, drivers etc.  We will later install drivers into this folder and regenerate the kernel image.
- a full kernel image in `$KDIR/arch/arm/boot/zImage`

### Creating bootable SD card

Once you have a compiled kernel, you need to create a file in a specific format that can be booted by the board.  As discussed in the course material, the boot process typically involves multiple stages: first stage boot, U-Boot, and finally the kernel.  Here we will not go through the process of reconstructing the first two stages, and just want a demo that takes us as close as possible to the demo in class.

The SD card image that can be created by flashing the Pynq image has two partitions: partition 1 has very few files, and is the one we are interested in.  Partition 2 is the complete partition containing all the files required for booting a full Linux system and running Pynq - we will not use it any further.

In partition 1, do not touch any of the files except for a file named `image.ub` - this is the final image that contains all the information (kernel with initramfs etc.) that is needed for booting. Our goal here is to create our own `image.ub` and use it to boot into a basic shell so we can run commands similar to those demonstrated in the videos.

The first step for that (creating the kernel image) has already been done above.  Now we need to follow two more steps:

#### Creating the device tree

The `image.ub` file needs to contain two things: the kernel image, and a device tree.  The kernel is already compiled.  To create the device tree, we will need to compile the DTS file (device tree specification) and create a DTB (device tree blob or binary).  The basic device tree for our examples is available at [pynq-z1.dts](./pynq-z1.dts).  So we do the following:

```bash
# Go to the lab folder
cd $LDIR
# Create the binfiles folder if not already present
mkdir -p binfiles
dtc -I dts -O dtb -o binfiles/pynq-z1.dtb pynq-z1.dts
```

*Note*: You can do this even if you are actually using the Pynq-Z2 board - they both use the same underlying FPGA, so the same DT works in both cases.

#### Creating bootable image on SD card

Now that we have the kernel and the DTB, we need to create the boot image (`image.ub` file).  This is done in the `binfiles` folder mentioned above:

```bash
cd $LDIR/binfiles  # If not already there
ln -s $KDIR/arch/arm/boot/zImage .   # Use a symbolic link so you don't have to copy each time
mkimage -f ../boot.its image.ub      # Create the boot image
```

Now you need to take this bootable `image.ub` and put it on SD card.  If you use a USB SD card reader/writer, then when you plug it in to the PC you should see the partition p1 (and p2) visible in the file browser.  Now you can just use regular file commands or the GUI to copy `image.ub` into partition 1 (overwriting the file already present).

Safely eject the SD card, and plug it into the Pynq board.  

#### Booting and viewing on serial port

We will use a regular mini-USB cable to both provide power to the Pynq board, and also to see the output that it generates on the serial port (similar to what you would see in the renode demos).  In our case, the same serial port will actually show both UART0 and UART1, so you may end up seeing duplicate messages.  

To see this, we need to install the `gtkterm` software on the host PC.  Then plug in the USB cable, and turn on power to the Pynq board.

**NOTE**: Ensure that the jumper setting for the Pynq board is correct: it should be set to *SD card* mode and there should be a jumper in place to get power from the USB source.

Configure `gtkterm`:

- Port: `/dev/ttyUSB1` - usually.  If this doesn't work, you should run the command `sudo dmesg` and see where it reports seeing the serial port (after turning on power to the board)
- Params: `115200 N 1` - speed, no parity, 1 stop bit (standard values)

If necessary, press the reset button on the board after changing settings, and hopefully you should now see:

- first the initial boot messages from U-boot - do not press a key here, or it will stop and wait for further input
- boot messages from the kernel we just compiled.  Typically the first few messages come very quickly, and then there is a pause for several seconds (nearly 40-50) before it finally goes through and prints messages on the screen.

At this point, you have successfully booted a custom compiled kernel on the Pynq board.

For all the subsequent exercises, you will need to follow the procedures given here to create the kernel and `image.ub`, and finally boot and see the messages in `gtkterm`.  In those cases, we will also have custom hardware and kernel drivers, but the rest of the process remains the same.


