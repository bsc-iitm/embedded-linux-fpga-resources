---
marp: true
paginate: true
title:  Week 6 - Platform Drivers - Binding and Basics
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

# Platform Drivers - Binding and Basics

---

# Platform Drivers

- Goal: understand how Linux binds a driver to a device via DT (platform bus)
- Implement minimal, safe driver: `probe/remove`, `devm_*`, `readl/writel`
- Keep user API stable (small sysfs for demo)
- Compare with alternatives (UIO, misc, DMA, other OS)

---

## Platform vs. Others (Concepts)

- Platform device: on-SoC, described by DT/ACPI; not discoverable via PCI/USB
- Binding: `compatible` in DT $\rightarrow$ OF match table $\rightarrow$ `probe()`
- Alternates:
  - PCI/USB: hardware-discoverable; drivers bind via IDs
  - UIO/misc: simpler user-space approach but limited policies
  - Subsystems: IIO, GPIO, PWM, SPI/I2C client drivers

---

## Device Tree $\rightarrow$ Binding

```dts
smart-timer@70000000 {
  compatible = "acme,smart-timer-v1";
  reg = <0x70000000 0x1000>;
};
```

```c
static const struct of_device_id st_of[] = {
  { .compatible = "acme,smart-timer-v1" },  { }
};
MODULE_DEVICE_TABLE(of, st_of);
```

- Kernel creates device from DT, emits uevent with `MODALIAS=of:...`
- User space matches modalias $\rightarrow$ `modprobe` loads module

---

## What does of_* mean?

- "of_" = Open Firmware (Device Tree) core in Linux
- `struct of_device_id`: DT match table for drivers
  - `compatible` strings the driver supports
  - Used at runtime to match DT nodes to drivers
  - Export `MODULE_DEVICE_TABLE(of, ...)` - DT modalias for autoload
- Used on any architecture with DT (ARM, RISC-V, PowerPC, MIPS)
- *Parallels*: ACPI uses `struct acpi_device_id`, PCI uses `struct pci_device_id`

---

## Driver Skeleton (Probe)

```c
static int st_probe(struct platform_device *pdev)
{
  struct resource *res;
  st = devm_kzalloc(&pdev->dev, sizeof(*st), GFP_KERNEL);
  if (!st) return -ENOMEM;
  res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  st->base = devm_ioremap_resource(&pdev->dev, res);
  if (IS_ERR(st->base)) return PTR_ERR(st->base);
  platform_set_drvdata(pdev, st);
  sysfs_create_group(&pdev->dev.kobj, &attrs);
  return 0;
}
```

---

## Probe helpers - why these APIs?

- `platform_get_resource(pdev, IORESOURCE_MEM, 0)`
  - Retrieves address/size from firmware (DT/ACPI) as a `struct resource`
  - Driver stays data-driven; no hard-coded addresses

---

## Probe helpers - why these APIs? (contd.)

- `devm_ioremap_resource(&dev, res)`
  - Requests region, maps MMIO $\rightarrow$ `void __iomem *` and handles cleanup automatically
  - Validates resource; safer than a plain `ioremap`

---

## Probe helpers - why these APIs? (contd.)

- `platform_set_drvdata(pdev, st)` / `dev_get_drvdata(dev)`
  - Stores per-device state; avoids globals; supports multi-instance drivers

---

## Probe helpers - why these APIs? (contd.)

- `sysfs_create_group(&dev.kobj, &attrs)`
  - Exposes user-visible attributes on the device node; must use sysfs APIs

---

## Probe helpers - why these APIs?

Why not "regular" functions?
- Kernel drivers must use kernel subsystems (no libc); helpers enforce lifetime, safety, and policy
- Direct globals/hard-coded addresses break portability and concurrency
- Managed (`devm_*`) resources prevent leaks across error paths

---

## Driver Skeleton (Remove)

```c
static int st_remove(struct platform_device *pdev)
{
  sysfs_remove_group(&pdev->dev.kobj, &attrs);
  return 0; // devm_* frees allocations and unmaps
}

static struct platform_driver st_drv = {
  .probe = st_probe,
  .remove = st_remove,
  .driver = {
    .name = "smarttimer",
    .of_match_table = st_of,
  },
};
module_platform_driver(st_drv);
```

---

## Minimal Sysfs (Demo Surface)

```c
static ssize_t ctrl_show(...){
  u32 v = readl(st->base + OFF_CTRL) & 0x3; return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t ctrl_store(...){
  unsigned long val; if (kstrtoul(buf, 0, &val)) return -EINVAL;
  writel((u32)val & 0x3, st->base + OFF_CTRL); return cnt;
}
static DEVICE_ATTR_RW(ctrl);
```

- Keep user API consistent across weeks; small and safe

---

## Chunked Transfers (FFT-style)

- Options for sending small blocks (e.g., 32 complex samples):
  - Register FIFO: write one word at a time; simple HW, higher CPU
  - MMIO array: small SRAM window; tight `writel` loop, then `LEN/START`
  - DMA: preferred for large/frequent buffers (program addr/len, IRQ on done)
- Demo module: `fft_block_demo.ko` writes up to 64 u32 into `DATA[]`

---

## MMIO Array Example (Demo)

```text
0x00 CTRL   (bit0 EN, bit1 START)
0x04 STATUS (bit0 DONE)
0x08 LEN    (# complex)
0x100 DATA  (64 x u32 $\rightarrow$ 32 complex re/im)
```

```c
// parse values $\rightarrow$ writel() into OFF_DATA + i*4
writel(n/2, base + OFF_LEN); // complex count
// pulse START
```

---

## Autoload vs. insmod

- insmod: manual; demo-friendly, not scalable
- Autoload on boot (recommended):
  - `MODULE_DEVICE_TABLE(of, ...)` present (modalias)
  - Module installed into initramfs under `lib/modules/...`
  - `depmod -a` run against that root to produce `modules.alias`
  - User space must react to uevents and call `modprobe` (udev or mdev setup)

---

## Initramfs: What to Add

- Ensure tools present:
  - `/sbin/modprobe` (kmod or BusyBox `modprobe`)
  - udev (systemd-udevd) or configure BusyBox `mdev` for hotplug
- Build steps:
  - `make -C $KDIR M=$PWD modules_install INSTALL_MOD_PATH=$INITRAMFS`
  - `depmod -a -b $INITRAMFS $(kernel_version)`
  - Include `modules.dep`/`modules.alias` in the image

---

## Other OS Contrast (Brief)

- Windows (WDF): INF declares HW IDs; PnP manager matches and loads driver; registry provides resources
- Zephyr/RTOS: static device tables or devicetree at build-time; drivers bound at init via macros
- Bare metal: fixed addresses, no dynamic binding; driver init is manual

Takeaway: Linux + DT enables late binding and multi-instance via a data-driven description.

---

## Wrap-up

- Platform driver: DT binding, `probe/remove`, `devm_*`, small sysfs
- Chunked data: FIFO vs MMIO array vs DMA
- Autoload: DT modalias + depmod + modprobe/udev in initramfs
