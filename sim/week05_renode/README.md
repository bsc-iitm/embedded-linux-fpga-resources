# Week 5 — Renode + Device Tree Workflow (Zynq/ZedBoard)

This builds on Week 4’s Zynq demo. We will:
- Decompile the existing DTB used in Week 4
- Add a Smart Timer node (`compatible`, `reg`)
- Recompile a new DTB and boot with it in Renode
- Load the same Week 4 driver (still module-parameter based) to interact with MMIO

## Files
- Base DTB (from Week 4): `../week04_renode/binfiles/zynq-zed.dtb`
- New script: `week5_zynq.resc` (based on Week 4 script)
- Example overlay: `dts/overlay-smarttimer.dts`
- Output DTB (you generate): `binfiles/zynq-zed-smarttimer.dtb`

## Prerequisites
- Device Tree Compiler: `sudo apt-get install device-tree-compiler`
- Renode installed (set `RENODE_PATH` environment variable)
- Week 4 kernel image (in `../week04_renode/binfiles/vmlinux`)

## Step 1: Decompile the DTB

```bash
# From sim/week05_renode directory
mkdir -p dts binfiles

# Decompile the Week 4 DTB to a human-readable DTS
dtc -I dtb -O dts \
  -o dts/zynq-zed.dts \
  ../week04_renode/binfiles/zynq-zed.dtb
```

Open `dts/zynq-zed.dts` and locate a suitable bus node.
For this decompiled ZedBoard DTS, the parent is `axi` (a `simple-bus` at `/axi`).

### zynq-zed.dts — Key Things To Notice (brief)
- Root node: `model`, top-level `compatible` identifying the board/soc.
- `/aliases`: stable names for devices (e.g., `serial0`, `ethernet0`).
- `/chosen`: boot arguments, stdout console (`stdout-path`).
- `/memory`: DRAM base and size.
- `/cpus` and CPU node(s): CPU type and properties.
- Interrupt controller (GIC) and timer nodes: how interrupts are wired.
- Clocks and clock providers: PLLs and clock trees referenced by peripherals.
- SoC/AMBA bus nodes (e.g., `/amba`, `/soc`, `/amba_pl`): parents for on-chip and PL peripherals; check `#address-cells`, `#size-cells`, and `ranges`.
- Peripherals (UART, SD/MMC, Ethernet, I2C/SPI, GPIO): each with `compatible`, `reg`, and optional interrupts.
- Pin control and regulators (if present): referenced by peripheral nodes.

We will explain these in slides later; for Week 5, focus on locating the bus node that should parent the Smart Timer and confirming `#address-cells/#size-cells` and `ranges`.

## Step 2: Add the Smart Timer node

Insert a node describing the Smart Timer MMIO window (mirrors the Week 4 mapping):

```dts
smarttimer0: smart-timer@70000000 {
  compatible = "acme,smart-timer-v1";
  reg = <0x70000000 0x1000>; // 4 KiB window
  status = "okay";
};
```

Place this under the appropriate bus node. In this DTS, use `/axi`.
On other Zynq variants you may see `/amba_pl` or `/amba` — adjust accordingly.

Alternatively, use the provided overlay as a guide: `dts/overlay-smarttimer.dts`
which targets `/axi` for this DTS. Compile and apply it with `fdtoverlay` (requires dtc >= 1.6.0):

```bash
# Build overlay binary (.dtbo)
dtc -I dts -O dtb -o binfiles/overlay-smarttimer.dtbo \
  dts/overlay-smarttimer.dts

# Apply overlay onto the base DTB (adjust target-path inside overlay if needed)
fdtoverlay -i ../week04_renode/binfiles/zynq-zed.dtb \
  -o binfiles/zynq-zed-smarttimer.dtb \
  binfiles/overlay-smarttimer.dtbo
```

If you edited the decompiled DTS directly instead of using an overlay, recompile:

```bash
dtc -I dts -O dtb \
  -o binfiles/zynq-zed-smarttimer.dtb \
  dts/zynq-zed.dts
```

## Step 3: Boot Renode with the new DTB

From the Renode console, load the Week 5 script:

```renode
(i) @week5_zynq.resc
```

The script will load the kernel ELF and your new DTB. Make sure you're running Renode from the `sim/week05_renode/` directory, or adjust the path accordingly.

## Step 4: Interact with the device (same Week 4 driver)

For Week 5, we still use the Week 4 demo driver (`mmio_demo.ko`) with a module parameter while
we introduce DT concepts. In Week 6, we’ll switch to a true platform driver that binds via DT.

Guest commands (example):

```bash
# Inside the guest Linux (Renode console)
insmod /root/mmio_demo.ko phys_base=0x70000000 window_size=0x1000
cat /dev/mmio_demo
# Or via sysfs under /sys/devices/virtual/misc/mmio_demo/
```

## Verifying DT in the guest

- Check presence of node: `ls -R /proc/device-tree/`
- Inspect properties: `hexdump -C /proc/device-tree/axi/smart-timer@70000000/compatible` (path may vary)
- Confirm `reg` value: `hexdump -C /proc/device-tree/.../reg`

## Notes
- Exact bus path differs across Zynq device trees. If `amba_pl` isn't present, search for `axi` or `amba` nodes.
- The overlay's `target-path` must point to the parent bus node where the device should live.
- Week 6 shows how to use `platform_driver` and `devm_ioremap_resource()` for automatic binding via device tree.

***

Troubleshooting
- `fdtoverlay: command not found` — either install a newer dtc or edit the decompiled DTS directly and recompile.
- Node doesn’t appear in `/proc/device-tree` — check that the new DTB is actually the one passed at boot in the `.resc`.
- Driver can’t see MMIO effects — ensure `phys_base` matches the DT `reg` base (0x70000000) and the Renode REPL maps the same window.
