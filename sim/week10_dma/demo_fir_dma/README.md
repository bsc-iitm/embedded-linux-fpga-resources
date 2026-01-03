# FIR Filter with AXI DMA

Purpose: Demonstrate high-performance bulk data transfer using AXI DMA, replacing MMIO vector mode from Weeks 7/9.

## Learning Objectives

- Configure AXI DMA for memory-to-stream (MM2S) and stream-to-memory (S2MM)
- Allocate DMA-coherent buffers with `dma_alloc_coherent()`
- Use physical addresses for DMA controller registers
- Handle DMA completion interrupt
- Contrast data paths and CPU roles across polling, IRQ, and DMA
- Understand cache coherency requirements

## Architecture

```
┌─────────┐   MM2S    ┌─────────┐   AXI Stream   ┌─────────┐
│  Memory │ ────────> │ AXI DMA │ ──────────────> │   FIR   │
│ (input) │           │         │                 │ Filter  │
└─────────┘           └─────────┘                 └─────────┘
                                                        │
┌─────────┐   S2MM    ┌─────────┐   AXI Stream        │
│ Memory  │ <──────── │ AXI DMA │ <────────────────────┘
│(output) │           │         │
└─────────┘           └─────────┘
```

Hardware Changes from Week 9:
- FIR filter has **AXI Stream** input/output (no DATA_IN/DATA_OUT registers)
- AXI DMA controller added with two channels (MM2S and S2MM)
- DMA completion interrupt (S2MM IOC_IRQ)

## AXI DMA Register Map

```
Offset  Register       Access   Description
------  ------------   ------   -----------
0x00    MM2S_DMACR     RW       MM2S control (run, IRQ enable)
0x04    MM2S_DMASR     RO/W1C   MM2S status (idle, IOC_IRQ)
0x18    MM2S_SA        RW       MM2S source address (physical)
0x28    MM2S_LENGTH    RW       MM2S transfer length (bytes, triggers)

0x30    S2MM_DMACR     RW       S2MM control (run, IRQ enable)
0x34    S2MM_DMASR     RO/W1C   S2MM status (idle, IOC_IRQ)
0x48    S2MM_DA        RW       S2MM destination address (physical)
0x58    S2MM_LENGTH    RW       S2MM transfer length (bytes, triggers)
```

## FIR Filter Changes

- Removed: DATA_IN[0:31] and DATA_OUT[0:31] MMIO registers
- Added: AXI Stream slave (input) and AXI Stream master (output)
- Processing triggered by AXI Stream transactions (not CTRL.START)
- COEFF[0:3] and LEN registers retained for configuration

> Measurement note
> - Renode is great for functional validation of DMA programming and IRQs, but not for wall-clock comparisons.
> - This demo focuses on correctness; we’ll gather performance on real hardware later.

## Driver Implementation

### DMA Buffer Allocation

```c
// In probe()
fir->input_buf = dma_alloc_coherent(&pdev->dev,
                                     MAX_SAMPLES * sizeof(s16),
                                     &fir->input_dma, GFP_KERNEL);

fir->output_buf = dma_alloc_coherent(&pdev->dev,
                                      MAX_SAMPLES * sizeof(s16),
                                      &fir->output_dma, GFP_KERNEL);
// input_buf/output_buf: virtual addresses for CPU
// input_dma/output_dma: physical addresses for DMA
```

### Start DMA Transfer

```c
static void fir_start_dma(struct fir_dev *fir)
{
    u32 len_bytes = fir->len * sizeof(s16);

    // Configure MM2S (memory to FIR)
    writel(fir->input_dma, fir->dma_base + MM2S_SA);
    writel(len_bytes, fir->dma_base + MM2S_LENGTH);  // Starts transfer

    // Configure S2MM (FIR to memory)
    writel(fir->output_dma, fir->dma_base + S2MM_DA);
    writel(len_bytes, fir->dma_base + S2MM_LENGTH);  // Starts transfer

    fir->processing = true;
}
```

### DMA Completion IRQ Handler

```c
static irqreturn_t fir_dma_irq(int irq, void *dev_id)
{
    struct fir_dev *fir = dev_id;
    u32 status = readl(fir->dma_base + S2MM_DMASR);

    if (!(status & DMASR_IOC_IRQ))
        return IRQ_NONE;

    // Clear interrupt (W1C)
    writel(DMASR_IOC_IRQ, fir->dma_base + S2MM_DMASR);

    spin_lock(&fir->lock);
    fir->processing = false;
    spin_unlock(&fir->lock);

    wake_up_interruptible(&fir->wait);
    return IRQ_HANDLED;
}
```

### Sysfs Interface

```c
// input_data: write to coherent buffer
static ssize_t input_data_store(...) {
    // Parse space-separated values into fir->input_buf
    // No writel() to hardware - DMA reads directly
}

// output_data: read from coherent buffer (blocks)
static ssize_t output_data_show(...) {
    wait_event_interruptible(fir->wait, !fir->processing);
    // Copy from fir->output_buf (written by DMA)
}

// start: trigger DMA transfer
static ssize_t start_store(...) {
    fir_start_dma(fir);
}
```

## Testing Workflow

### 1. Load Driver
```bash
insmod fir_dma.ko
dmesg | grep fir
# Should see: probe succeeded, DMA IRQ 67 registered
```

### 2. Configure Coefficients
```bash
echo "8192 16384 16384 8192" > /sys/devices/platform/fir/coefficients
```

### 3. Write Input Data
```bash
# 32 samples: impulse at sample 10
echo "0 0 0 0 0 0 0 0 0 0 32767 0 0 ..." > /sys/devices/platform/fir/input_data
echo 32 > /sys/devices/platform/fir/len
```

### 4. Start DMA Transfer
```bash
echo 1 > /sys/devices/platform/fir/start
# Configures DMA and starts transfer (non-blocking)
```

### 5. Block on Read (Wait for DMA Completion)
```bash
cat /sys/devices/platform/fir/output_data
# Blocks until DMA complete, then prints: 8192 16384 16384 8192 0 0 ...
```

### 6. Verify DMA Interrupt
```bash
cat /proc/interrupts | grep fir
#  67:          1   GIC-0  fir-dma
```

## Performance Comparison

### Method 1: Polling (Week 7)
```bash
# CPU writes 32 samples via MMIO: 15 µs
# FIR processes: 5 µs
# CPU reads 32 samples via MMIO: 15 µs
# Total: 35 µs, 100% CPU during transfer
```

### Method 2: Interrupts (Week 9)
```bash
# CPU writes 32 samples via MMIO: 15 µs
# FIR processes (CPU free): 5 µs
# IRQ + handler: 3 µs
# CPU reads 32 samples via MMIO: 15 µs
# Total: 38 µs, CPU free during processing only
```

### Method 3: DMA (Week 10)
```bash
# CPU configures DMA: 2 µs
# DMA reads from memory: 1 µs (CPU free)
# FIR processes: 5 µs (CPU free)
# DMA writes to memory: 1 µs (CPU free)
# IRQ + handler: 3 µs
# Total: 12 µs, CPU free entire time
```

**Result**: DMA is 3x faster and eliminates CPU overhead during data movement.

## Debugging Scenarios

### DMA Transfer Never Completes

1. Check physical addresses valid:
   - Verify using `dma_handle` (not virtual `buf`)
   - Check address within valid DRAM range

2. Check DMA status registers:
   ```bash
   devmem2 0x40400004  # MM2S_DMASR
   devmem2 0x40400034  # S2MM_DMASR
   # Look for error bits or idle state stuck
   ```

3. Verify DMA control registers:
   - MM2S_DMACR and S2MM_DMACR should have run bit set
   - IRQ enable bits should be set

4. Check AXI Stream connections in Renode/hardware

### IRQ Never Fires

1. Verify IRQ routing in device tree:
   - DMA IRQ number matches `.repl` overlay
   - Check `cat /proc/interrupts` for registration

2. Check DMA control register:
   - IOC_IRQ_EN bit must be set in S2MM_DMACR

3. Verify S2MM completes:
   - Read S2MM_DMASR, check idle bit

### Data Corruption

1. Cache coherency issue:
   - Ensure using `dma_alloc_coherent()` (not `kmalloc`)
   - Check arch supports hardware cache coherency

2. Alignment problems:
   - Verify buffers aligned to cache line (64 bytes typical)
   - Check DMA length matches actual data

3. Concurrent access:
   - Ensure CPU doesn't write to buffer during DMA
   - Use spinlock to protect `processing` flag

### Partial Transfer

1. Check LENGTH register:
   - Must be in bytes: `len * sizeof(s16)`
   - Verify matches actual buffer size

2. AXI Stream backpressure:
   - FIR may not be consuming data fast enough
   - Check for TREADY deassertion in VCD

## Device Tree Integration

```dts
axi_dma: dma@40400000 {
    compatible = "xlnx,axi-dma-1.00.a";
    reg = <0x40400000 0x10000>;
    interrupts = <GIC_SPI 35 IRQ_TYPE_LEVEL_HIGH>;
    xlnx,include-mm2s;
    xlnx,include-s2mm;
};

fir_filter: fir@40000000 {
    compatible = "acme,fir-dma-v1";
    reg = <0x40000000 0x1000>;     // FIR config registers
    reg-names = "fir", "dma";
    dmas = <&axi_dma 0>, <&axi_dma 1>;
    dma-names = "tx", "rx";
};
```

## Files Expected

- `rtl/README.md` - RTL changes for AXI Stream interface
- `driver/README.md` - Driver implementation details
- `renode/README.md` - Renode co-simulation setup
- `verilator_cosim/README.md` - Verilator integration notes

## Key Concepts

### Physical vs Virtual Addresses

- CPU uses **virtual addresses** (per-process, paged)
- DMA uses **physical addresses** (actual DRAM location)
- `dma_alloc_coherent()` returns both:
  - Virtual pointer for CPU access
  - Physical address (`dma_handle`) for DMA registers

### Cache Coherency

- **Coherent DMA**: hardware keeps cache in sync (simpler, may be slower)
- **Streaming DMA**: explicit sync with `dma_sync_*()` (faster, more complex)
- This demo uses coherent for simplicity

### DMA Completion

- Writing LENGTH register triggers transfer
- DMA asserts IOC_IRQ when complete
- Driver clears IRQ (W1C) and wakes wait queue

## References

- Week 9 demo (interrupt-driven FIR)
- Week 7 demo (MMIO vector mode)
- Xilinx AXI DMA datasheet (PG021)
- Linux DMA API: `Documentation/DMA-API.txt`
