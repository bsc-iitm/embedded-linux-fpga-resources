---
marp: true
paginate: true
title:  Week 5 - Device Tree - Describing Hardware for Linux
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

# Device Tree - Describing Hardware for Linux

---

## Objectives

- Why Device Tree (DT) and how Linux uses it
- From quick module params $\rightarrow$ *declarative* DT
- Author and load a DTB in Renode (Zynq)
- Contrast with older board files and x86 (ACPI/PCI)

---

## Recap

- Ran Zynq system in Renode; booted Linux
- Poked SmartTimer MMIO at `0x7000_0000`
- Loaded `mmio_demo.ko` with `phys_base=0x70000000`
- Great for bring‑up, but not scalable across boards/instances

---

## Why Device Tree?

- Data, not code: describe hardware once, reuse drivers
- Firmware/bootloader passes DTB to kernel at boot
- Drivers match on `compatible`, read `reg`, `interrupts`, etc.
- One kernel + many boards (no recompile for address changes)

---

## Older board specific code

```c
// old board file (simplified)
static struct resource uart_resources[] = {
  { .start = 0x101f1000, .end = 0x101f1fff, .flags = IORESOURCE_MEM, },
  { .start = 5, .end = 5, .flags = IORESOURCE_IRQ, },
};

static struct platform_device uart_device = {
  .name = "my-uart",
  .id = -1,
  .num_resources = ARRAY_SIZE(uart_resources),
  .resource = uart_resources,
};
```

---

## DT based approach

```dts
uart0: serial@101f1000 {
  compatible = "arm,pl011";
  reg = <0x101f1000 0x1000>;
  interrupts = <5>;
};
```

> Reuse the same binary across different boards

---

## Is Device Tree Essential?

- Bare metal: directly patch device info into bringup code
  - Not scalable
- x86 systems: *Advanced Configuration and Power Interface* 
  - ACPI interface initialized through BIOS
  - PCI/USB etc: self-enumerating
- Non-x86 (ARM, RISC-V) Linux: best done through DT
  - Possible without DT but not scalable

---

## DT Basics (What to recognize now)

- Nodes and properties (text DTS $\rightarrow$ binary DTB)
- `compatible`, `reg` (address,size), `status`
- Bus parents: `/axi` (this ZedBoard DTS)
- Address/size cells, `ranges` on bus nodes
- Inspect live tree at `/proc/device-tree`

---

## Decompile the original DTB

```bash
# Create editable DTS from the existing DTB
dtc -I dtb -O dts -o dts/zynq-zed.dts \
  binfiles/zynq-zed.dtb
```

Key items in `zynq-zed.dts` to notice now:
- Root `model`, top-level `compatible`
- `/chosen` (cmdline, console), `/aliases`
- `/memory`, `/cpus`, GIC/timers
- `/axi` bus with `#address-cells`, `#size-cells`, `ranges`

---

## Add SmartTimer Node (@0x70000000)

```dts
// Parent is the AXI bus on this DTS
/ {
  axi {
    smarttimer0: smart-timer@70000000 {
      compatible = "acme,smart-timer-v1";
      reg = <0x70000000 0x1000>;
      status = "okay";
    };
  };
}
```

- Unit‑address matches first cell in `reg`
- 4 KiB window consistent with REPL overlay

---

## Overlay Flow (Alternative)

```dts
// dts/overlay-smarttimer.dts
/dts-v1/; /plugin/;
/ {
  fragment@0 {
    target-path = "/axi";
    __overlay__ {
      smarttimer0: smart-timer@70000000 {
        compatible = "acme,smart-timer-v1";
        reg = <0x70000000 0x1000>;
        status = "okay";
      };
    };
  };
}
```

---

## Overlay Flow (apply overlay)

```bash
# Build overlay & apply to base DTB
dtc -I dts -O dtb -o binfiles/overlay-smarttimer.dtbo \
  dts/overlay-smarttimer.dts
fdtoverlay -i binfiles/zynq-zed.dtb \
  -o binfiles/zynq-zed-smarttimer.dtb \
  binfiles/overlay-smarttimer.dtbo
```

---

## Boot With the New DTB (Renode)

```renode
(i) @week5_zynq.resc
```

- Reuses the previous kernel
- Points DTB to `binfiles/zynq-zed-smarttimer.dtb`
- MMIO window still mapped by REPL at `0x70000000`

---

## Verify in the Guest

```bash
ls -R /proc/device-tree/axi/
hexdump -C /proc/device-tree/axi/smart-timer@70000000/compatible
hexdump -C /proc/device-tree/axi/smart-timer@70000000/reg
```

- Confirms node visibility and address

---

## DT $\rightarrow$ Driver Binding

```c
// Purpose: match table used by platform drivers
static const struct of_device_id smarttimer_of_match[] = {
  { .compatible = "acme,smart-timer-v1" },
  { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, smarttimer_of_match);
```

---

## Driver Probe

```c
// Skeleton probe:
static int smarttimer_probe(struct platform_device *pdev)
{
  struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  void __iomem *base = devm_ioremap_resource(&pdev->dev, res);
  if (IS_ERR(base)) return PTR_ERR(base);
  /* create sysfs, etc. */
  return 0;
}
```

---

## Older Method: board-*.c & platform_data

```c
// Purpose: legacy per-board device registration (simplified)
static struct resource st_res[] = {
  { .start = 0x70000000, .end = 0x70000fff, .flags = IORESOURCE_MEM },
};

static struct platform_device st_dev = {
  .name = "smarttimer", // matches driver name
  .id = 0,
  .num_resources = ARRAY_SIZE(st_res),
  .resource = st_res,
  .dev = { .platform_data = NULL },
};
```

---

## Older Method (contd).

```c
static void __init zedboard_init(void)
{
  platform_device_register(&st_dev);
}
```

- Pros: quick for one fixed board
- Cons: 
  - kernel rebuild for changes
  - duplicates logic
  - poor portability

---

## DT vs Board Files (at a glance)

- DT: declarative, passed at boot, reusable drivers
- Board files: C code per board, compile‑time resources
- Transition path: use DT where possible; some legacy still exists

---

## What about x86?

- x86 platforms typically use PCI enumeration + ACPI tables
  - PCI/PCIe: discoverable buses; devices self‑describe (VID:DID)
  - ACPI: firmware tables describe non‑discoverable devices and resources
- DT is rare on PCs, common on ARM/embedded
- Outcome is similar: kernel learns devices + resources without hard-coding

---

## Pitfalls & Tips

- `compatible` mismatch $\rightarrow$ no binding
- Wrong `reg` cells/size $\rightarrow$ ioremap failures
- Place nodes under the correct bus (`/axi` here)
- Don’t encode policy in DT (keep it HW description)

---

## Summary

- DT cleanly separates hardware description from drivers
- We modified the Zynq DT to add SmartTimer at `0x7000_0000`
- Loaded platform driver using `modprobe` - yet to see how it works
