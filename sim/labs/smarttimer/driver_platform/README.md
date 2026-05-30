# Smart Timer Platform Driver

A minimal platform driver for the Smart Timer that binds via the Device Tree
and exposes a few sysfs knobs to poke the hardware registers. This is the
plain driver (no interrupt handling); see `../driver_irq` for the version that
also services the wrap interrupt.

## Build (out-of-tree)

The lab environment (`setup.sh`) already exports `ARCH`, `CROSS_COMPILE` and
`KDIR`, so a plain `make` is enough:

```bash
make
```

Output: `smarttimer_platform.ko`

Install into the initramfs before packing it (same step as for every module
in the labs):

```bash
make -C "$KDIR" M="$(pwd)" modules_install INSTALL_MOD_PATH=/tmp/initramfs
```

## Device Tree

The driver matches the smart timer node in `pynq-z1.dts`:

```dts
smarttimer0: smart-timer@70000000 {
    compatible = "acme,smarttimer-v1";
    reg = <0x70000000 0x10000>;
    status = "okay";
};
```

The address must match the entry in the Vivado Address Editor, otherwise the
kernel will fault when it tries to access the registers.

## Run on the board

After booting the custom image, activating the level shifters and programming
the bitstream (see the [walkthrough](../../walk-through.md)):

```bash
modprobe smarttimer_platform
dmesg | grep smarttimer
```

Interact via sysfs under the platform device:

```bash
find /sys/bus/platform/devices -name ctrl -path '*smart*'
cat /sys/.../ctrl
echo 1 > /sys/.../ctrl   # set the EN bit to start the timer
```

The exposed attributes are `ctrl`, `period`, `duty` and `status`.

## What to notice

- Binding via the OF match table and `compatible` string.
- `platform_get_resource` + `devm_ioremap_resource` for safe register mapping.
- `platform_set_drvdata` and retrieving it in the sysfs callbacks.
- Small, readable sysfs attributes that map directly onto hardware registers.
