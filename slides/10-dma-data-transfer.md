---
marp: true
paginate: true
title: Week 10 — DMA and High-Performance Data Transfer
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

# DMA and High-Performance Data Transfer

Offloading Bulk Transfers • Cache Coherency • AXI DMA

---

# Goals

- Understand Direct Memory Access (DMA) benefits 
- Configure AXI DMA controller
- Use coherent DMA buffers in Linux
- Handle cache coherency correctly
- Understand tradeoffs between polling, IRQ, and DMA

---

# Why DMA? - AXI Lite

![height:14cm](../assets/axilite.png)

---

# Why DMA? - MMIO Vector Mode

MMIO vector mode :
- CPU writes each sample via AXI-Lite
- Limited bandwidth (several cycles per transfer) 
  - 10s of MBps for microcontrollers
- CPU busy during transfers

---

# Why DMA?

Separate bus master that can initiate transactions on bus: **Direct Memory Access**

DMA benefits:

- Higher throughput (hardware-dependent)
- CPU free during transfers
- Only interrupted on completion

---

# When to Use DMA

<div class="columns-2">
<div>

**Use DMA**
- Large buffers (>64 samples)
- Streaming data
- High-rate peripherals
- Real-time constraints

</div>
<div>

**Use MMIO**
- Small transfers (<16 samples)
- Infrequent operations
- Simple control registers
- Low-latency requirements (<1 µs)

</div>
</div>

---

# DMA Concepts

**Bus Mastering**: DMA controller initiates memory transactions

**Physical Addresses**: DMA uses physical, not virtual addresses

**Cache Coherency**: Ensure CPU and DMA see same data

**Interrupts**: DMA signals completion via IRQ

---

# AXI Stream Primer

![height:8cm](../assets/axi-lite-handshake.svg)

| TVALID | TREADY | TDATA |
|--------|--------|-------|
| source has valid data | sink can accept data | payload |

---

# AXI Stream

Why stream?
- Continuous/real-time data (samples, video, network)
- Natural flow control and framing

---

# AXI DMA + AXI Stream

- MM2S: Memory → AXI Stream (drives TVALID/TDATA/TLAST)
- S2MM: AXI Stream → Memory (consume on TVALID && TREADY)

<div class="columns-2">
<div>

Programming model:
- Write **physical** source/dest addresses
- Write LENGTH to start transfer (DMA asserts TLAST at end)
- Enable IRQs and wait for completion

</div>
<div>

Implications:
- MM2S rate is limited by downstream TREADY
- S2MM writes only when upstream TVALID && TREADY
- TLAST from upstream can split S2MM into frames

</div>
</div>

---

# Cache Coherence

- Cache memory: hardware controlled fast memory: temporary storage
- Data in cache can be accessed much faster than main memory
- What happens when two processors / bus masters try to read from a memory address?

---

# Cache Coherency Problem

```
CPU writes data → CPU cache (not yet in memory)
DMA reads from memory → sees stale data!

DMA writes to memory
CPU reads data → gets stale cache!
```

Solutions:
- Coherent buffers: hardware keeps cache in sync
- Streaming: explicit sync with `dma_sync_*()`
- CMA: contiguous memory allocator for large buffers

---

# Coherent vs Streaming DMA

<div class="columns-2">
<div>

### Coherent DMA
```c
buf = dma_alloc_coherent(dev,
    size, &dma_handle, GFP_KERNEL);
// No sync needed
// Hardware maintains coherency
```

</div>
<div>

### Streaming DMA
```c
dma_handle = dma_map_single(dev,
    buf, size, direction);
dma_sync_for_device(dev, ...);
// ... DMA transfer ...
dma_sync_for_cpu(dev, ...);
```

</div>
</div>

Use coherent for simplicity; streaming for large buffers

---

# AXI DMA Controller

- **MM2S** (Memory-to-Stream): memory → AXI Stream → peripheral
- **S2MM** (Stream-to-Memory): peripheral → AXI Stream → memory

Key registers (per channel):
- Control: run, reset, IRQ enable
- Status: idle, IRQ on complete
- Address: source/dest physical address
- Length: transfer size (bytes)

---

# DMA Without AXI Stream (AXI4 Bursts)

Do we need streams to benefit from DMA? Not always.

Memory-mapped (AXI4) DMA helps when:
- Offloading memory-to-memory copies (e.g., large memcpy)
- Moving blocks to/from a peripheral’s memory-mapped buffer window that supports bursts

---

# DMA Without AXI Stream

<div class="columns-2">
<div>

Pros vs Stream:
- Simple for block copies; leverages bus burst efficiency
- No explicit TLAST/frame handling required

</div>
<div>

Cons vs Stream:
- Less natural for continuous pipelines or variable-rate producers
- Peripheral must expose a burst-capable window or buffer

</div>
</div>

Design tip:
- For streaming accelerators, prefer AXI Stream + DMA
- For batched/block operations, AXI4 bursts via mem2mem DMA are effective

---

# AXI DMA Register Map

```
Offset  Register       Description
------  ------------   -----------
0x00    MM2S_DMACR     MM2S control (run, IRQ enable)
0x04    MM2S_DMASR     MM2S status (idle, IOC_IRQ)
0x18    MM2S_SA        MM2S source address (physical)
0x28    MM2S_LENGTH    MM2S length (triggers transfer)

0x30    S2MM_DMACR     S2MM control
0x34    S2MM_DMASR     S2MM status
0x48    S2MM_DA        S2MM destination address
0x58    S2MM_LENGTH    S2MM length (triggers transfer)
```

---

# FIR Filter with DMA Architecture

![height:8cm](../assets/fir-flow.png)

FIR has **no DATA_IN/DATA_OUT registers** — DMA handles all data movement

---

# Allocate DMA Buffers

```c
// In probe()
fir->input_buf = dma_alloc_coherent(&pdev->dev,
                                     MAX_SAMPLES * sizeof(s16),
                                     &fir->input_dma, GFP_KERNEL);

fir->output_buf = dma_alloc_coherent(&pdev->dev,
                                      MAX_SAMPLES * sizeof(s16),
                                      &fir->output_dma, GFP_KERNEL);

// input_dma, output_dma = physical addresses
```

In `remove()`: `dma_free_coherent()`

---

### Start DMA Transfer

```c
static void fir_start_dma(struct fir_dev *fir)
{
    u32 len_bytes = fir->len * sizeof(s16);

    // MM2S: memory to FIR
    writel(fir->input_dma, fir->dma_base + MM2S_SA);
    writel(len_bytes, fir->dma_base + MM2S_LENGTH); // start transfer

    // S2MM: FIR to memory
    writel(fir->output_dma, fir->dma_base + S2MM_DA);
    writel(len_bytes, fir->dma_base + S2MM_LENGTH); // start transfer

    fir->processing = true; // synchronization
}
```

---

### DMA Completion IRQ

```c
static irqreturn_t fir_dma_irq(int irq, void *dev_id)
{
    struct fir_dev *fir = dev_id;
    u32 status = readl(fir->dma_base + S2MM_DMASR);

    if (!(status & DMASR_IOC_IRQ))
        return IRQ_NONE;

    writel(DMASR_IOC_IRQ, fir->dma_base + S2MM_DMASR); // Clear

    spin_lock(&fir->lock);
    fir->processing = false;
    spin_unlock(&fir->lock);

    wake_up_interruptible(&fir->wait);
    return IRQ_HANDLED;
}
```

---

### User Interface (Sysfs)

```c
// input_data: write to coherent buffer (no MMIO)
static ssize_t input_data_store(...)
{
    // Parse values into fir->input_buf - DMA reads from this
}

// output_data: read from coherent buffer
static ssize_t output_data_show(...)
{
    wait_event_interruptible(fir->wait, !fir->processing);
    // Copy from fir->output_buf (written by DMA)
}
```

No `readl(DATA_OUT)` or `writel(DATA_IN)` — DMA handles it

---

<!-- Note: Renode is not reliable for wall-clock performance measurements. Keep this qualitative. -->
### Performance Comparison (Qualitative)

<div class="columns-2">
<div>

### Polling 
- CPU copies every sample via MMIO
- Lowest control latency, highest CPU load

### Interrupts 
- CPU still copies data, but can sleep during processing
- Similar data-move cost as polling, lower CPU load

</div>
<div>

### DMA 
- CPU configures descriptors/addresses then sleeps
- Hardware performs bulk transfer end-to-end
- Typically higher throughput; CPU overhead minimal

</div>
</div>

---

### DMA vs MMIO: Throughput (Qualitative)

- MMIO: throughput limited by per-register transactions; scales poorly
- DMA: throughput dominated by memory/bus bandwidth; scales well
- For large buffers, DMA is usually much faster than MMIO

---

# Example 1

<div class="columns-3">
<div>

Assumptions
- MMIO per 32-bit op = 0.25 µs → per sample (write+read) = 0.50 µs
- DMA setup = 5 µs, Effective bandwidth B = 50 MB/s
- Bytes/sample roundtrip (in+out) = 4 → DMA data time/sample = 4/B = 0.08 µs

</div>
<div>

Model
- MMIO: T_mmio(N) = N × 0.50 µs
- DMA: T_dma(N) = 5 µs + N × 0.08 µs
- Break-even: N* ≈ 5 / (0.50 − 0.08) ≈ 12 samples

</div>
<div>

Examples
- N=8:  T_mmio=4.0 µs vs T_dma=5.64 µs → MMIO faster
- N=16: T_mmio=8.0 µs vs T_dma=6.28 µs → DMA faster
- N=64: T_mmio=32 µs vs T_dma=10.12 µs → DMA clearly faster

</div>
</div>


Takeaway: With modest setup costs, DMA wins for fairly small buffers (>~12–16 samples).

---

# Example 2

<div class="columns-3">
<div>

Assumptions
- MMIO per 32-bit op = 0.15 µs → per sample (write+read) = 0.30 µs
- DMA setup = 30 µs, Effective bandwidth B = 100 MB/s → 4/B = 0.04 µs/sample

</div>
<div>

Model
- MMIO: T_mmio(N) = N × 0.30 µs
- DMA: T_dma(N) = 30 µs + N × 0.04 µs
- Break-even: N* ≈ 30 / (0.30 − 0.04) ≈ 116 samples

</div>
<div>

Examples
- N=64:  19.2 µs vs 32.56 µs → MMIO faster
- N=128: 38.4 µs vs 35.12 µs → DMA slightly faster
- N=512: 153.6 µs vs 50.48 µs → DMA much faster

</div>
</div>

Takeaway: With higher setup latency or fast MMIO, DMA pays off at larger N.

---

## Rule of Thumb (When to Use DMA)

General model (roundtrip samples):
- $T_{mmio}(N) = N \times (t_{wr} + t_{rd})$
- $T_{dma}(N) = t_{setup} + N \times (bytes\_per\_sample / B)$

Break-even threshold:
- $N^* \approx \frac {t_{setup}}{ [(t_{wr} + t_{rd}) − (bytes\_per\_sample / B)]}$

- DMA when N >> N*, or CPU must be free during transfers
- MMIO for very small N or rare, latency-sensitive ops

---

# Physical vs Virtual Addresses

CPU uses **virtual addresses** (per-process)

DMA uses **physical addresses** (actual RAM)

```c
// WRONG: DMA can't use virtual address
writel(fir->input_buf, dma_base + MM2S_SA);  // BUG!

// CORRECT: Use physical address from dma_alloc_coherent()
writel(fir->input_dma, dma_base + MM2S_SA);  // OK
```

`dma_alloc_coherent()` returns both virtual (for CPU) and physical (for DMA)

---

# Debugging DMA Issues

**DMA stalls**: Check physical addresses valid (use `dma_handle`, not `buf`)

**Cache incoherency**: Verify `dma_alloc_coherent()` used (not `kmalloc`)

**IRQ never fires**: Check control register IRQ enable bit

**Partial transfer**: Verify LENGTH matches actual data size

**Alignment**: DMA may require 4-byte or cache-line alignment

---

# Scatter-Gather (Advanced)

For non-contiguous buffers:

```c
struct dma_desc {
    u32 next_desc;      // Physical addr of next descriptor
    u32 buffer_addr;    // Physical addr of data
    u32 control;        // Length + flags
    u32 status;         // Completion status
};
```

Chain descriptors for fragmented memory

Enables zero-copy from user buffers

---

# Device Tree Integration

```dts
axi_dma: dma@40400000 {
    compatible = "xlnx,axi-dma-1.00.a";
    reg = <0x40400000 0x10000>;
    interrupts = <GIC_SPI 35 IRQ_TYPE_LEVEL_HIGH>;  // S2MM IRQ
    xlnx,include-sg;
    xlnx,include-mm2s;
    xlnx,include-s2mm;
};

fir_filter: fir@40000000 {
    compatible = "acme,fir-dma-v1";
    reg = <0x40000000 0x10000>;
    dmas = <&axi_dma 0>, <&axi_dma 1>;  // MM2S, S2MM
    dma-names = "tx", "rx";
};
```

---

# Summary

- DMA offloads bulk transfers from CPU
- Use `dma_alloc_coherent()` for simple buffer management
- Configure addresses and length, wait for IRQ
- Higher throughput than MMIO for moderate/large buffers
- Essential for high-rate streaming (video, network, ADC)

---

# Key Takeaways

| Method      | CPU Load | Latency | Throughput | Use Case          |
|-------------|----------|---------|------------|-------------------|
| Polling     | 100%     | Lowest  | Low        | <16 samples       |
| Interrupts  | Low      | Medium  | Low        | Infrequent events |
| DMA         | Minimal  | Medium  | High       | Streaming, >64 samples |

Choose based on data size, rate, and CPU budget
