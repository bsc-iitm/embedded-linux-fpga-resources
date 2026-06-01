# 03 - Smart Timer

The smart timer is a counter with a pulse-width-modulation (PWM) output. You
can set the period and duty cycle, and start and stop it, through an AXI-Lite
register interface. It also raises an interrupt each time the counter wraps.

In this experiment you will:

1. Build a Vivado block design containing the smart timer, an ILA to watch the
   AXI bus, and a counter driving the LEDs.
2. Generate a bitstream.
3. Build and install the platform drivers.
4. Boot, program the FPGA, and exercise the timer through sysfs.

The RTL is in [`../smarttimer/rtl`](../smarttimer/rtl) and the drivers are in
[`../smarttimer/driver_platform`](../smarttimer/driver_platform) (plain
platform driver) and [`../smarttimer/driver_irq`](../smarttimer/driver_irq)
(adds interrupt handling).

## Step 1: Create the Vivado project

Create a new Vivado project and select the correct part for the board:
`XC7Z020CLG400-1` (see [01 - Environment setup](./01-environment-setup.md) for
why). Board files that let you pick the board by name are not assumed to be
installed, so select the part directly.

Add the RTL from `smarttimer/rtl` to the project. Vivado may make the AXI-Lite
module the *top* module of the project - ignore that for now, since the block
design will become the top.

## Step 2: Build the block design

Create a block design (any name) and recreate the design shown here:

![smart timer block design](../smarttimer/block-design.png)

Notes:

- Run **Block Automation** and **Connection Automation** when offered - Vivado
  wires up a lot of the AXI and clock/reset plumbing for you.
- Double-click the **Zynq PS** and check:
  - **Clock**: enable the PS-to-PL fabric clock `FCLK0` at 100 MHz (50 MHz also
    works if you prefer).
  - **Interrupts**: enable Fabric-to-PS interrupts (the Shared Peripheral
    Interrupts, SPI). We use a single interrupt. It maps by default to
    interrupt 61, which is SPI number `61 - 32 = 29` - this is the `29` that
    appears in the device tree (`interrupts = <0 29 4>`).
- Open the **Address Editor** and make sure the smart timer's address matches
  the device tree entry (`0x70000000`, see Step 5). If they disagree the kernel
  will try to access an address with no hardware behind it and the system will
  hang.

### ILA and debug logic

The design includes an **Integrated Logic Analyzer (ILA)**, plus a counter and
some output pins.

- The ILA mixes AXI ports (to watch the input and output of the AXI
  interconnect - one port is enough if you prefer) with a plain port on the
  `pwm_out` signal. When you start the timer via the `ctrl` register you should
  see the PWM activate with the configured period and duty cycle.
- The **counter** is a "sign of life" indicator. With a 100 MHz input clock it
  counts far too fast to see, so a **slice** picks out higher bits (e.g.
  `[27:24]`) that change at a visible rate to blink the LEDs.

### Pin constraints

Add an XDC constraint file (the name does not matter) mapping the counter slice
output to the board LEDs. This is only for debugging, but without the mapping
the bitstream generation will fail. Use `LVCMOS33` for the LED pins.

The base XDC file for the Pynq-Z1 board is in this folder.  If you add the entire file
you will most likely face problems as most of the signals there are not in your
design.  Extract only the entries for the LEDs and put them in a new constraint file.

## Step 3: Generate the bitstream

Validate the design, generate the block design, create an HDL wrapper, then run
synthesis, implementation, and Generate Bitstream.

## Step 4: Build and install the drivers

There are two drivers for the smart timer:

- **driver_platform** - a plain platform driver exposing sysfs controls.
- **driver_irq** - adds a blocking char device (`/dev/smarttimer0`) whose
  `read()` blocks until the next wrap interrupt, on top of the same sysfs
  controls.

Both match the same compatible string (`acme,smarttimer-v1`), and this single
bitstream already has the interrupt wired, so the two drivers can be tested
against the same hardware. Build and install each one (the environment from
`setup.sh` provides `ARCH`, `CROSS_COMPILE` and `KDIR`):

```bash
cd $LDIR/smarttimer/driver_platform   # then repeat for driver_irq
make
make -C "$KDIR" M="$(pwd)" modules_install INSTALL_MOD_PATH=/tmp/initramfs
```

Then rebuild the kernel so the modules are packed into the initramfs:

```bash
make -j4 -C $KDIR
```

This produces a fresh `zImage`; rebuild `image.ub` as in
[02 - Kernel and boot image](./02-kernel-and-boot-image.md) - remember to
re-copy `zImage` into `binfiles` before `mkimage`, since it is a copy, not a
symlink - and copy the new `image.ub` to the SD card.

## Step 5: Device tree

The smart timer node lives in [`../pynq-z1.dts`](../pynq-z1.dts). Uncomment it:

```dts
smarttimer0: smart-timer@70000000 {
    compatible = "acme,smarttimer-v1";
    reg = <0x70000000 0x10000>;
    interrupts = <0 29 4>;          // SPI 29 (IRQ_F2P[0]), level-high
    interrupt-parent = <0x04>;
    status = "okay";
};
```

The `reg` address must match the Address Editor in Vivado. Recompile the DTB
and rebuild `image.ub`.

## Step 6: Boot and test on the board

Boot the board with the new `image.ub`. While it boots, open the Vivado
Hardware Manager and connect - it should show the `xc7z020` device.

### Activate the level shifters

The PL is isolated from the PS by level shifters that are off at reset. Turn
them on from the board shell (you must do this every boot):

```bash
devmem 0xF8000900 32 0xF
```

Without this the PL gets no clock and the ILA shows nothing. You can run it
before or after programming the bitstream (refresh the ILA display if you
program first).

### Program and load the driver

Program the bitstream over JTAG from the Hardware Manager. With the level
shifters on, the ILA signals should appear.

Load a driver from the board's serial shell:

```bash
modprobe smarttimer_platform   # or: modprobe smarttimer_blocking
```

Find the device under sysfs and interact with it:

```bash
ls /sys/bus/platform/devices/        # find the smart-timer device
# it exposes: ctrl, period, duty, status
echo 1 > /sys/bus/platform/devices/<smarttimer>/ctrl   # start the timer
```

Writing `1` to `ctrl` starts the timer; you should see the PWM on the ILA and
the LEDs blinking from the counter. If you loaded `smarttimer_blocking`, a
`read()` on `/dev/smarttimer0` blocks until the next wrap interrupt - the
response you see depends on which driver is loaded.

Next: [04 - Squarer: MMIO vs DMA](./04-squarer-mmio-dma.md).
