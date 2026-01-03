---
marp: true
paginate: true
title: Week 12 — Performance and Production
author: Nitin Chandrachoodan
theme: gaia
style: |
  .columns-2 { display: grid; grid-template-columns: repeat(2, 1fr); gap: 1rem; font-size: 0.8em; }
  .columns-3 {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 1rem;
    font-size: 0.65em;
  }
  code { font-size: 0.85em; }
math: mathjax
---

<!-- _class: lead -->

# Performance and Production

Real-Time Linux • Power Management • Production Topics

---

# Goals

- Understand real-time performance challenges and solutions
- Learn power management strategies for embedded systems
- Overview security and production considerations

---

<!-- _class: lead -->

# Part 1: Real-Time Performance

Latency Sources • PREEMPT_RT • Determinism vs Throughput

---

### Latency Sources in Linux

Linux optimized for **throughput**, not **determinism**

<div class="columns-2">
<div>

**Scheduling Latency**
- Process scheduler preemption delays
- Lock contention: lower-priority tasks
- Unpredictable scheduling decisions

**Interrupt Latency**
- IRQs disabled in critical sections
- Higher-priority IRQs block others
- Interrupt controller overhead

</div>
<div>

**Cache Effects**
- Context switches flush caches
- DMA invalidates cache lines
- Unpredictable timing from misses

**Page Faults**
- Major faults require disk I/O
- Minor faults require allocation
- Demand paging adds latency

</div>
</div>

For FPGA applications (motor control, DAQ), **predictable response** > average throughput

---

# PREEMPT_RT Patch Set

Converts Linux into real-time OS by making kernel fully preemptible

<div class="columns-3">
<div>

**1. Spinlocks $\rightarrow$ RT-Mutexes**
- Allow preemption in critical sections
- Priority inheritance prevents inversion
- Reduces worst-case latency

**2. Threaded IRQ Handlers**
- Handler: process context
- Can be preempted
- improves IRQ latency

</div>
<div>

**3. High-Resolution Timers**
- $\mu$s-precision timers
- *vs* jiffy-based (1-10ms) 
- Enables finer scheduling granularity

</div>
<div>

**Benefits**
- Bounded worst-case latency
- Predictable response times
- Priority inheritance guarantees

**Tradeoffs**
- Lower average throughput
- More context switches

</div>
</div>

---

# Determinism vs Throughput

![height:13cm](../assets/rt-throughput-tradeoff.svg)

**Soft real-time**: Occasional deadline misses tolerable (video streaming)
**Hard real-time**: Missing deadlines = system failure (motor control)

<!-- Skip this slide for now

---

# Measuring Interrupt Latency

Tools for latency measurement and analysis:

<div class="columns-2">
<div>

**cyclictest**
```bash
cyclictest -p 99 -m -n -i 1000
```

- High-priority thread wakes periodically
- Measures scheduled vs actual wake time
- Reports min/avg/max + histogram
- Gold standard for RT validation

</div>
<div>

**ftrace latency histograms**
```bash
echo 'irqsoff' > \
  /sys/kernel/debug/tracing/current_tracer
cat /sys/kernel/debug/tracing/\
  tracing_max_latency
```

- IRQ disable time
- Preemption disable time
- Wakeup latency distributions

</div>
</div>

Understanding worst-case latency essential for hard real-time

-->

---

<!-- _class: lead -->

# Part 2: Power Management

Runtime PM • Clock Gating • DVFS • Partial Reconfiguration

---

# Why Power Management Matters

Power is critical in embedded systems:

<div class="columns-2">
<div>

**Constraints**
- Battery life (mobile/remote)
- Thermal limits (passive cooling)
- Energy costs (always-on systems)
- Reliability (lower temp = longer life)

</div>
<div>

**Zynq Challenges**
- PS + PL both consume power
- FPGA static + dynamic power
- Always-on peripherals
- High-performance requirements

</div>
</div>

Power management essential for production Zynq systems

---

# Runtime PM Framework

Devices enter low-power states when idle

**Concept**: Usage count tracking; suspend when zero

```c
// In probe
pm_runtime_enable(&pdev->dev);

// Active use
pm_runtime_get_sync(&pdev->dev);
// ... use hardware ...
pm_runtime_put(&pdev->dev);  // Allow suspend when idle

// In remove
pm_runtime_disable(&pdev->dev);
```

---

# Runtime PM Callbacks

```c
static int my_runtime_suspend(struct device *dev) {
    // Gate clocks, power down peripheral
    return 0;
}

static int my_runtime_resume(struct device *dev) {
    // Power up, restore state
    return 0;
}

static const struct dev_pm_ops my_pm_ops = {
    .runtime_suspend = my_runtime_suspend,
    .runtime_resume = my_runtime_resume,
};
```

For FPGA peripherals: Gate clocks to unused accelerators

---

# Clock Gating and Power Domains

<div class="columns-2">
<div>

**Clock Gating**: Disable clock to idle modules

Dynamic power = $C \times V^2 \times f \times \text{Activity}$

Gating clock $\Rightarrow f = 0 \Rightarrow$ Zero dynamic power

Hardware retains state, just not clocking

</div>
<div>

**Device Tree Clock Specification**:

```dts
my_device: accel@43c00000 {
    compatible = "acme,accelerator";
    reg = <0x43c00000 0x10000>;
    clocks = <&clkc 15>;  // FCLK0
};
```
</div>
</div>

Runtime PM framework automatically gates clock on suspend

---

# Power Domains

**Power Domains**: Group of devices sharing power rail

Can power down entire domain when all devices suspended

**Zynq**: PL (Programmable Logic) domain can be powered off independently of PS

Enables complete FPGA shutdown when unused

---

# Dynamic Voltage and Frequency Scaling

<div class="columns-2">
<div>

Reduce voltage and frequency when high performance not needed:

- Dynamic power $\propto V^2 \times f$
- Leakage power $\propto V$

</div>
<div>

**Example**: Zynq can run at 667MHz, 533MHz, 400MHz

**Linux cpufreq framework**: Manages CPU frequency scaling

**FPGA clocks**: Similar mechanisms for PL clock rates

**Tradeoff**: Lower frequency = less performance, better power efficiency
</div>
</div>

---

# Power vs Energy

**Power:**
- Instantaneous - results in heat
- **Thermal management**

**Energy:**
- Over entire time duration
- Low power but long time can mean more energy
- **Battery life**

---

### FPGA Partial Reconfiguration

Zynq 7000 (and others): Reconfigure part of FPGA

**Use case**: Load specialized accelerators on-demand

- Boot with minimal FPGA configuration
- Load image processing when camera active
- Unload, reconfigure for audio DSP when needed

**Benefits**: Smaller bitstreams, lower static power, dynamic allocation

**Complexity**: Careful floorplanning, static/dynamic boundaries, PR-aware design

---

<!-- _class: lead -->

# Part 3: Production Considerations

Secure Boot • TrustZone • Filesystems • Updates • Optimization

---

# Secure Boot Chain

Prevent unauthorized code execution by verifying each boot stage:

![height:12cm](../assets/secure-boot-chain.svg)

Each stage verifies next stage signature

---

# Zynq Secure Boot

**BootROM** → **FSBL** → **U-Boot** → **Kernel** → **Rootfs**

<div class="columns-2">
<div>

**1. BootROM (SoC Internal)**
- Verify FSBL signature using eFUSE key
- RSA public key burned into eFUSE
- If signature invalid, boot halts

**2. FSBL**
- Verify U-Boot signature
- Check integrity before execution

</div>
<div>

**3. U-Boot**
- Verify kernel signature (FIT images)
- Check DTB and initramfs

**4. Kernel**
- Verify rootfs (dm-verity)
- Signed squashfs images

</div>
</div>

Unbroken chain of trust from power-on to userspace

---

# FPGA Bitstream Authentication

Bitstream can be encrypted (AES) and authenticated (HMAC)

<div class="columns-2">
<div>

**Process**:
1. Generate AES key, store in battery-backed RAM or eFUSE
2. Encrypt bitstream in Vivado
3. CSU decrypts bitstream during configuration
4. Verify HMAC before programming PL

</div>
<div>

**Benefits**:
- Prevents bitstream tampering
- Protects IP from reverse engineering
- Ensures only authorized bitstreams run

</div>
</div>

---

# ARM TrustZone

Hardware isolation between secure and non-secure worlds

<div class="columns-2">
<div>

**Secure World**
- Trusted OS (OP-TEE)
- Cryptographic keys
- Secure boot validation
- Bitstream decryption

</div>
<div>

**Normal World**
- Linux kernel
- Applications
- Device drivers

</div>
</div>

Use case: Linux drivers call secure world for crypto operations but never see keys

---

### Filesystem Choices

<div class="columns-2">
<div>

**ext4**
- Full-featured, Good performance
- Not optimized for flash wear
- Use: eMMC with wear leveling

**squashfs**
- Read-only, compressed, fast read
- Immutable (cannot modify)
- Use: Production read-only root

</div>
<div>

**ubifs**
- Designed for raw NAND (no eMMC)
- Wear leveling, bad block handling
- Atomic operations

**tmpfs**
- RAM-backed, Very fast, no flash wear
- Lost on reboot
- Use: `/tmp`, `/var/log`

</div>
</div>

**Common pattern**: squashfs root + overlayfs writable + tmpfs volatile

---

# Update Mechanisms

Reliable field updates critical for deployed systems

**A/B Partition Scheme**:
1. Device boots from partition A
2. Update writes new image to B, marks B bootable, A fallback
3. Reboot into partition B
4. If boot fails, revert to partition A

**Benefit**: Atomic updates, always have working fallback

---

# Update Mechanisms (Continued)

<div class="columns-2">
<div>

**Atomic Updates**

Ensure update fully succeeds or fails:
- Write to inactive partition
- Verify checksum
- Atomically flip boot flag
- Power loss → old system boots

</div>
<div>

**Delta Updates**

Only transmit differences:
- Reduces bandwidth (cellular/satellite)
- Tools: `bsdiff`, `xdelta`, OSTree
- More complex, requires version match

</div>
</div>

---

# Kernel Optimization

<div class="columns-2">
<div>

**Module vs Built-in**
- Built-in: Always loaded, faster boot
- Module: On-demand, smaller image
- Production: Critical drivers built-in

**Size Reduction**
- Disable unused features
- Strip debug, LZMA compression
- Minimal initramfs

</div>
<div>

**Boot Time Optimization**
- Reduce initramfs size (busybox)
- Parallelize init
- `quiet` kernel parameter
- Disable unused hardware in DT
- `bootdelay=0` in U-Boot

**Results**: 5-10s typical → 2-3s optimized

</div>
</div>

Minimal Zynq kernel: under 5MB (vs 10-15MB default)

---

<!-- _class: lead -->

# Course Summary

From MMIO Registers to Production Systems

---

# Course Overview

**Hardware Integration** (Weeks 1-7)
- MMIO, AXI-Lite interfaces, device tree, platform drivers
- Co-simulation with Verilator/Renode

**Driver Development** (Weeks 8-10)
- Interrupt handling, wait queues, locking
- DMA transfers, cache coherency

**System Architecture** (Weeks 11-12)
- Boot flow (BootROM, FSBL, U-Boot, kernel)
- Memory management (MMU, virtual/physical addressing)
- Real-time performance, power management, production

---

# Further Study

<div class="columns-2">
<div>

**Essential Resources**
- *Linux Device Drivers* (LDD3)
- *Linux Kernel Development* 
- kernel.org documentation
- Bootlin, Linux Foundation training

**Advanced Topics**
- PREEMPT_RT, cyclictest, ftrace/perf
- PCIe, high-speed interfaces
- Partial reconfiguration
- Yocto/Buildroot, secure boot, OTA

</div>
<div>

**Alternative Platforms**
- Zynq UltraScale+ MPSoC
- Intel SoC FPGAs
- RISC-V + FPGA

**Community**
- Linux kernel mailing lists
- Xilinx forums
- Embedded Linux Conference (ELC)
- FOSDEM, Linux Plumbers

</div>
</div>

---

<!-- _class: lead -->

## Linux + FPGA = powerful embedded systems

Continue learning: kernel source, hardware experiments, build projects
