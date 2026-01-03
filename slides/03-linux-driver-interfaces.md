---
marp: true
paginate: true
title:  Week 3 - Linux Drivers - From Registers to Files
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

# Linux Drivers: From Registers to Files

---

## Objectives

- See how Linux exposes hardware as files (devices, sysfs)
- Bridge: MMIO registers $\rightarrow$ kernel API $\rightarrow$ user space
- Build and load a minimal driver on any Linux PC

---

## System Power-On

- System power on reset: PC loaded with default value
  - Fetches instructions - usually JMP to space with more code
  - Flash memory, load from disk etc.: 
  - BIOS - Basic Input/Output System
  - **Bootstrapping** / **Boot-up**
- Embedded Microcontrollers - may not have OS: **Bare-Metal**

---

## Limitations of Bare-Metal

- All functionality in basic system
- No multi-tasking / time multiplexing
- Need full control over every step of process

Operating System: first program that runs after reset

- Take over system function
- Provide facilities to other programs

---

## OS Quick Overview

- Process scheduling: decides which program runs when
- Memory management: virtual memory, protection, paging
- Storage & filesystems: files, directories, permissions
- I/O stack: drivers, DMA, interrupts, power
- Networking: sockets, routing, offloads
- Security: isolation, capabilities, namespaces/containers

Note: We focus only on driver side of the I/O stack

---

## Recap: Registers and MMIO

- Peripherals present a register map (Week 1–2)
- Software reads/writes via MMIO (AXI‑Lite on SoC)
- In Linux, drivers map MMIO with `ioremap()` and use `readl/writel`
- Today: how that register model becomes user‑visible files

---

## Bare‑Metal Access (No OS)

```c
// Purpose: direct register access on bare metal (no OS)
#include <stdint.h>
#define PWM_BASE    0x40000000u
#define REG32(a)    (*(volatile uint32_t *)(a))
#define PWM_CTRL    (PWM_BASE + 0x00)
#define PWM_PERIOD  (PWM_BASE + 0x04)
#define PWM_DUTY    (PWM_BASE + 0x08)
#define PWM_STATUS  (PWM_BASE + 0x0C)
#define CTRL_EN     (1u << 0)
#define CTRL_RST    (1u << 1)  // write 1 = pulse
#define ST_WRAP     (1u << 0)  // W1C
#define ST_UPD      (1u << 1)
```

---

# Bare-Metal Access (No OS) - contd.

```c
int main(void) {
  REG32(PWM_CTRL) = CTRL_RST;        // pulse reset (reads as 0)
  REG32(PWM_PERIOD) = 0x000000FF;    // configure
  REG32(PWM_DUTY)   = 0x00000020;
  REG32(PWM_CTRL)  |= CTRL_EN;       // enable
  while (!(REG32(PWM_STATUS) & ST_WRAP)) { /* spin */ }
  REG32(PWM_STATUS) = ST_WRAP;       // W1C: clear wrap flag
  for(;;){}                          // loop forever
}
```

Notes: fixed physical addresses, no protection, full timing control.

---

## Bare Metal vs OS

<div class="columns-2">

<div>

#### Bare Metal

- Single program controls CPU
- Direct physical register access
- No memory protection
- Full timing control
- Polling / ISRs 
- Fast startup
- Limited portability
</div>

<div>

#### Operating System

- Many processes
- Scheduler
- Virtual Memory, Protection
- Drivers (MMIO)
- Userspace vs. Kernel space (`ioctl`, `/sys`, `/dev`...)
- Portability, Isolation, Resource Management
</div>

</div>

---

<!-- _class: lead -->

# Deeper into the OS Kernel

---

## Kernel vs User Space

- User space: your apps; limited privileges; uses syscalls
- Kernel space: OS core and drivers; full access
- Boundary: files, `ioctl`, `mmap`, sysfs, netlink, ...
- Design goal: safe and simple interfaces for users

---

#### CPU Privilege Levels

![CPU privilege modes diagram](../assets/cpu-privilege-modes.svg)

---

## Kernel vs User Space

- User mode cannot execute privileged instructions (MMIO, page table ops)
- Transitions into kernel via system calls, interrupts, faults
- Drivers run in kernel mode to safely touch hardware

---

## Virtual Memory Basics

#### Why

- Processes should be *isolated* from each other 
- multi-tasking, security

#### How

- Each process sees a *virtual memory space* - addresses that are different from other processes
- OS kernel *translates* virtual $\rightarrow$ physical and back

---

## Virtual Memory Basics

<div class="columns-2">
<div>

![](../assets/virtual-memory-map.svg)
</div>

<div>

- Each process sees its own virtual address space
- Page tables translate virtual $\rightarrow$ physical frames
- MMIO regions live in physical address space (not directly visible to apps)
</div>

---

## MMIO and ioremap

<div class="columns-2">

<div>

![MMIO and ioremap mapping](../assets/mmio-ioremap.svg)

</div>

<div>

- Devices expose registers at physical addresses (MMIO - AXI Bus interconnect)
- Kernel maps that physical range into kernel virtual memory with `ioremap()`
- Drivers then use `readl/writel` on the mapped pointer
- Userspace reaches hardware through driver‑provided files (`/dev`, sysfs), not by physical addresses
</div>

---

## Addressing Overview

- Virtual Address (userspace): per‑process; stable view; cannot see physical
- Kernel Virtual Address: kernel's own VA space; can map MMIO via `ioremap()`
- Physical Address: actual DRAM/device addresses on the SoC
- Bus/MMIO Address: device's register window in physical address space
- I/O Virtual Address (IOMMU, optional): DMA remapped address seen by devices
- **Takeaway**: userspace uses files/syscalls; kernel translates to MMIO safely

---

<!-- _class: lead -->

# Linux Device Drivers

---

## Linux Device Model (Basics)

- Devices (physical/virtual) attach to buses (platform, PCI, I2C, …)
- Drivers bind to devices and expose interfaces
- Common user‑facing surfaces:
  - Character devices under `/dev/*` (open/read/write/ioctl)
  - Sysfs attributes under `/sys/*` (text knobs, RO/RW)
  - Optional: `debugfs` for debugging, not for production

---

## Interfaces to Peripherals (Map)

1) MMIO registers in hardware (AXI‑Lite, APB, …)
2) Driver maps registers (`ioremap`), implements policy
3) Exposes user API:
   - Char dev: `/dev/yourdev` with `read/write/ioctl`
   - Sysfs: `/sys/class/…/attr` for simple control/status
   - UAPI headers for stable `ioctl`/`mmap` (if needed)

---

## From Registers to Files (Example)

```text
Hardware idea (smart timer example):
  CTRL, PERIOD, DUTY, STATUS

Linux demo:
  /dev/vreg_demo          # read shows snapshot
  /sys/.../ctrl           # RW (enable/reset pulse)
  /sys/.../period         # RW
  /sys/.../duty           # RW
  /sys/.../status         # RO (W1C semantics limited in demo)
```

---

## Typical Register Access in Drivers

```c
// Pseudocode
void __iomem *base = devm_ioremap_resource(dev, res);
u32 ctrl   = readl(base + CTRL_OFF);
writel(ctrl | EN_BIT, base + CTRL_OFF);
```

- `readl/writel` ensure proper ordering and width
- Use `devm_*` helpers to simplify lifetime management

---

## Device Types

- Character devices: byte‑oriented, sequential I/O (`read/write/ioctl`)
  - Examples: `/dev/ttyS0`, `/dev/i2c-1`, `/dev/input/event*`, GPUs/DRM
- Block devices: block‑oriented, buffered I/O via page cache
  - Examples: `/dev/sda`, `/dev/mmcblk0p1`, `/dev/nvme0n1p1`, loop devices
- Network interfaces: not files under `/dev`; accessed via sockets
  - Visible under `/sys/class/net/*` and tools like `ip`, `ethtool`
- Pseudo devices: randomness, nulls, terminals (`/dev/null`, `/dev/zero`, `/dev/urandom`)

---

## `ioctl`

- System call - I/O control
- Send device-specific control commands from user-space $\rightarrow$ kernel $\rightarrow$ device

```c
int fd = open("/dev/mydev", O_RDWR);
ioctl(fd, MYDEV_RESET);
```

---

## Character Devices

- Good for streaming data or structured control
- Register a device: major/minor auto with `misc_register`
- Implement file ops: `.read`, `.write`, optional `.ioctl`

---

## /dev vs /sys (What they are)

- `/dev` (devtmpfs + udev): device nodes (special files) - major/minor
  - Entry points for driver file ops; e.g., `/dev/ttyS0`, `/dev/sda`
  - Udev - names/permissions/... (e.g., `/dev/disk/by-uuid/...`)
- `/sys` (sysfs): live view of kernel objects (devices, buses, ...)
  - Hierarchies: `/sys/devices`, `/sys/bus/*`, `/sys/class/*`, ...
  - Attributes: small text files for status/controls; not bulk data
  - Udev listens to sysfs uevents to populate `/dev`

---

## Sysfs Attributes 

- One text file per attribute; simple RO/RW controls
- Bind lifetime to your device (e.g., misc device)
- Great for small integers, on/off, mode, status
- Avoid heavy data transfer; use char dev for that

---

## Permissions & Udev

- Devices appear under `/dev` with owner/group/mode
- Udev rules can grant access to non‑root users
- For class devices, attributes under `/sys/class/...`

`sudo` used to simplify - not recommended in general

---

## Beyond the Basics (Pointers)

- Interrupts and threaded IRQ handlers
- DMA and coherent memory
- Device Tree / ACPI for hardware description
- Power management (runtime/system)
- Userspace APIs: `ioctl`, `mmap`, `uapi` headers

We won't go deep here this week; focus stays on interfaces.

---

## Summary

- Linux exposes hardware cleanly as files and attributes
- Register concepts map directly to driver APIs
- Start simple: sysfs + char device; add complexity later
- You can practice on any Linux machine — no board needed
