---
marp: true
paginate: true
title: Week 8 — Interrupts Part 1 - Hardware and Device Tree
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
    font-size: 0.65em;
  }
  code {
    font-size: 0.85em;
  }
math: mathjax
---

<!-- _class: lead -->

# Hardware Interrupts

Hardware Design + Device Tree + IRQ Flow

---

# Goals

- Understand why interrupts are necessary (vs polling)
- Learn interrupt types (level vs edge-triggered)
- Understand ARM GIC interrupt routing
- Add IRQ capability to Smart Timer hardware
- Configure interrupt wiring via device tree
- Prove end-to-end interrupt flow works

---

# Why Interrupts?

**The Polling Problem**:
```c
// CPU is stuck in a loop, burning cycles
while (!(readl(STATUS) & DONE_BIT)) {
    // Can't do other work
    // Wastes power
}
process_result();
```

---

# Why Interrupts?

**The Interrupt Solution**:
- CPU registers a handler once
- Goes off to do other work
- Hardware notifies CPU when event occurs
- Handler runs, processes event, returns

---

# Polling vs Interrupts

<div class="columns-2">
<div>

**Polling**
- CPU busy-waits in loop
- Simple to implement
- Good for very fast ops (<1 µs)
- Wastes power and cycles

</div>
<div>

**Interrupts**
- CPU does other work
- Event-driven (responsive)
- Good for infrequent/slow events
- Requires handler setup

</div>
</div>

**When to use which?**
- Fast operations (<1 µs): Polling
- Slow/unpredictable (ms-sec): Interrupts
- Infrequent events: Interrupts

---

# Interrupt Types

<div class="columns-2">
<div>

**Level-Triggered**
```
IRQ signal:
     _______________
____|               |____
    ^               ^
  Assert          Clear
 (event)        (SW ack)
```
- Signal stays high until cleared
- Safe (can't miss)
- Requires acknowledgment
- **Most MMIO peripherals**

</div>
<div>

**Edge-Triggered**
```
IRQ signal:
     __
____|  |_______________
    ^
  Edge pulse
  (event)
```
- Only transition matters
- Can miss if not handled fast
- No acknowledgment needed
- **External signals, Message Signaled Interrupts**

</div>
</div>

---

# ARM Generic Interrupt Controller

**Purpose**: Route interrupts from many sources to CPU cores

- **Distributor (GICD)**: Rec. all INT signals, handles priority/routing
- **CPU Interface (GICC)**: One per core, delivers interrupts to CPU

**Interrupt Types**:
- **SGI** (0-15): Software-generated (inter-processor)
- **PPI** (16-31): Per-CPU private (local timers)
- **SPI** (32-1019): Shared peripherals **(not the other SPI!)**

---

![height:15cm](../assets/gic-architecture.svg)

---

# Interrupt Routing Flow

<div class="columns-2">
<div>

1. Peripheral asserts `irq_out` signal
2. GIC Distributor receives on input line
3. GIC checks priority and target CPU
4. CPU Interface signals CPU core (IRQ line)
5. CPU saves context, jumps to vector table

</div>

<div>

6. Linux kernel dispatches to registered handler
7. Our driver handler executes
8. Handler clears interrupt source (W1C)
9. Kernel writes EOI (End Of Interrupt) to GIC
10. CPU restores context and resumes

</div>
</div>

---

# Device Tree Interrupt Bindings

**Purpose**: Describe interrupt connections (like a wiring diagram)

```dts
smarttimer: smarttimer@70000000 {
    compatible = "acme,smarttimer-irq-v1";
    reg = <0x70000000 0x1000>;
    interrupts = <GIC_SPI 33 IRQ_TYPE_LEVEL_HIGH>;
    interrupt-parent = <&gic>;
};
```

- `interrupts`: Connection specification (3 cells for GIC)
- `interrupt-parent`: Which interrupt controller (usually GIC)

---

# GIC Interrupt Cells (3 cells)

```dts
interrupts = <TYPE  IRQ_NUM  FLAGS>;
```

<div class="columns-3">
<div>

**Cell 1: Interrupt Type**
- `GIC_SPI` (0): Shared peripheral (32-1019)
- `GIC_PPI` (1): Private per-CPU (16-31)

</div>
<div>

**Cell 2: Interrupt Number**
- For SPI: 0-987 (adds to base 32)
  - SPI 33 → GIC hardware ID 65 (32+33)

</div>
<div>

**Cell 3: Flags**
- `IRQ_TYPE_LEVEL_HIGH` (4): Level, active high
- `IRQ_TYPE_LEVEL_LOW` (8): Level, active low
- `IRQ_TYPE_EDGE_RISING` (1): Edge, rising
</div>
</div>

---

# Example: Smart Timer Interrupt

```dts
gic: interrupt-controller@8000000 {
    compatible = "arm,cortex-a9-gic";
    #interrupt-cells = <3>;
    interrupt-controller;
    reg = <0x8000000 0x1000>, <0x8001000 0x1000>;
};

smarttimer: smarttimer@70000000 {
    compatible = "acme,smarttimer-irq-v1";
    reg = <0x70000000 0x1000>;
    interrupts = <GIC_SPI 33 IRQ_TYPE_LEVEL_HIGH>;
};
```

**Translation**: SPI interrupt 33 (GIC ID 65), level-triggered

---

# Hardware: Smart Timer with IRQ

**Interrupt Event**: Counter wrap (overflow)

1. Counter reaches PERIOD, wraps to 0
2. Set `STATUS.WRAP` bit (sticky flag)
3. Assert `irq_out` signal (stays high)
4. Software reads STATUS, sees WRAP=1
5. Software writes 1 to STATUS.WRAP (W1C)
6. Hardware clears WRAP bit and `irq_out`

**Level-triggered**: IRQ stays high until acknowledged

---

# Register Map

Offset | Register   |  Access  | Description
------ | ---------- |  ------  | -----------
0x000  | CTRL       |  RW      | [0]=EN, [1]=RST (W1P)
0x004  | STATUS     |  RO/W1C  | [0]=WRAP (W1C)
0x008  | PERIOD     |  RW      | Timer period
0x00C  | DUTY       |  RW      | Duty cycle

---

# Wrap bit - RTL

<div class="columns-2">
<div>

`STATUS.WRAP` bit:
- Set by hardware on wrap event
- Cleared by software (W1C)
- Drives `irq_out` signal

</div>
<div>

```verilog
module smarttimer_axil_irq (
    // ... AXI-Lite signals ...
    output wire irq_out  // New: interrupt output
);

// Detect wrap event
wire wrap_event = timer_en && (counter == period);

// STATUS.WRAP sticky flag
always @(posedge clk) begin
    if (!resetn)
        status_wrap <= 1'b0;
    else if (wrap_event)
        status_wrap <= 1'b1;  // Set on wrap
    else if (status_write && wdata[0])
        status_wrap <= 1'b0;  // Clear on W1C
end

// IRQ output (level-triggered)
assign irq_out = status_wrap;
```
</div>
</div>

---

# Renode Integration

**Peripheral Definition (.repl)**:
```
gic: IRQControllers.ARM_GenericInterruptController @ {
        sysbus new Bus.BusMultiRegistration { address: 0xF8F01000; size: 0x1000; region: "distributor" };
        sysbus new Bus.BusMultiRegistration { address: 0xF8F00100; size: 0x100; region: "cpuInterface" }
    }

smarttimer: CoSimulated.CoSimulatedPeripheral @ sysbus <0x70000000, +0x1000>
    frequency: 10000
    cosimToRenodeSignalRange: <0, +1> // map irq as signal 
    0 -> gic@33  // Route GPIO[0] to GIC input 65 (SPI 33)

cpu: CPU.ARMv7A @ sysbus
    cpuType: "cortex-a9"
    genericInterruptController: gic
```

---

# Minimal Linux Driver Pattern

**Goal**: Prove interrupts work (simple counter)

1. Request IRQ in probe: `devm_request_irq()`
2. Simple handler:
   - Increment atomic counter
   - Print message (rate-limited)
   - Acknowledge interrupt (W1C)
3. Sysfs attribute to read counter

---

# Debugging Interrupts

**Problem**: IRQ never fires (count stays 0)

<div class="columns-2">
<div>

**Check Hardware**:
- Verilator VCD: Is `irq_out` high?
- RTL bug in wrap detection?

**Check Wiring**:
- Renode `.repl`: `-> gic@X` correct?

</div>
<div>

**Check Device Tree**:
- `interrupts` property correct?
- Matches `.repl` wiring?

**Check Kernel**:
```bash
cat /proc/interrupts
dmesg | grep smarttimer
```

</div>
</div>

---

# Debugging Interrupts (2)

<div class="columns-2">
<div>

**Problem**: IRQ fires once then stops

**Cause**: Not clearing interrupt source

**Solution**: Ensure W1C write in handler:
```c
writel(STATUS_WRAP_BIT, stdev->base + STATUS_OFFSET);
```

</div>
<div>

**Problem**: Kernel says "nobody cared"

**Cause**: Handler returning `IRQ_NONE` too many times

**Solution**: Check status bit before returning `IRQ_NONE`
</div>
</div>

---

# Performance: Interrupt Overhead

<div class="columns-2">
<div>

- Context save: ~50-100 cycles
- GIC acknowledge: ~10-20 cycles
- Handler execution: ~100 cycles
- EOI: ~10-20 cycles
- Context restore: ~50-100 cycles
- **Total**: ~200-400 cycles
</div>

<div>

**Example**: 50 MHz timer, 1 Hz wrap rate
- Overhead: 400 / 50,000,000 = 0.0008% (negligible)

**Rule of Thumb**: Interrupts efficient for <1 kHz per source

</div>
</div>

---

# When Polling is Better

<div class="columns-2">
<div>

For **very fast operations** (<1 µs):
```c
// Polling may be faster than interrupt overhead
while (!(readl(STATUS) & DONE_BIT)) {
    cpu_relax();
}
```

</div>
<div>

**Use polling when**:
- Operation takes <1 µs
- Driver initialization (one-time)
- Tight loop, no other work

**Use interrupts when**:
- Unknown completion time
- Want CPU to do other work
- Multiple event sources
</div>
</div>

---

# Summary: Concepts

<div class="columns-2">
<div>

**Why Interrupts**:
- CPU efficiency
- Power savings
- Responsiveness

**Types**:
- Level (persistent)
- Edge (transient)
- We use level

</div>
<div>

**GIC Architecture**:
- Distributor routes
- CPU interface delivers
- SPI for peripherals

**Device Tree**:
- 3-cell format
- Describes wiring
- `interrupts` property

</div>
</div>
