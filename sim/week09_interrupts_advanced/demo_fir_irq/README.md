# FIR Filter with Interrupt-Driven Completion

Purpose
- Add an interrupt on DONE to the Week 7 FIR and expose a blocking char device that returns output samples when processing completes.

## Learning Objectives

- Add interrupt capability to vector peripheral (FIR filter)
- Use spinlock for status flags (IRQ + process context)
- Use mutex for configuration data (process context only)
- Implement blocking read that waits for processing completion
- Understand when to use each locking primitive
- Compare polling (Week 7) vs interrupt-driven (Week 9) performance

Hardware
- STATUS.DONE (bit 0, RO/W1C) is set when processing finishes and cleared by writing 1.
- `irq_out` is level-high while DONE is set.
- Address map and Q15 data match Week 7.

### Register Map (Updated)

```
Offset  Register      Access   Description
------  ------------  ------   -----------
0x000   CTRL          RW       [0]=START (W1P), [1]=RESET
0x004   STATUS        RO/W1C   [0]=DONE (set by HW, W1C by SW)
0x008   LEN           RW       Number of samples to process
0x00C   COEFF[0]      RW       Q15 coefficient h[0]
0x010   COEFF[1]      RW       Q15 coefficient h[1]
0x014   COEFF[2]      RW       Q15 coefficient h[2]
0x018   COEFF[3]      RW       Q15 coefficient h[3]
0x100   DATA_IN[0]    WO       Input sample 0 (Q15)
0x104   DATA_IN[1]    WO       Input sample 1 (Q15)
...
0x17C   DATA_IN[31]   WO       Input sample 31 (Q15)
0x200   DATA_OUT[0]   RO       Output sample 0 (Q15)
0x204   DATA_OUT[1]   RO       Output sample 1 (Q15)
...
0x27C   DATA_OUT[31]  RO       Output sample 31 (Q15)
```

Driver logic
- Spinlock protects `done`/`processing` flags (updated in IRQ and process context).
- Mutex protects coefficient updates (process-only path).
- IRQ handler: W1C clear DONE, set `done = true`, `processing = false`, wake wait queue.
- read(): `wait_event_interruptible(wait, READ_ONCE(done))`, copy DATA_OUT[], then clear `done`.

Integration
- SPI 34 in DT: `interrupts = <GIC_SPI 34 IRQ_TYPE_LEVEL_HIGH>`; overlay wires `0 -> gic@34`.
- See renode/demo_fir_irq.resc for kernel/DTB boot; `/dev/fir0` blocks on read until DONE.

- [ ] Sysfs Attributes:
  - [ ] `coefficients` (write): mutex protects coefficient array
  - [ ] `input_data` (write): copy to input_buf
  - [ ] `output_data` (read): blocks on wait queue, then copies output_buf
  - [ ] `start` (write): set processing=true (spinlock), write CTRL.START
  - [ ] `len` (read/write): number of samples

- [ ] Blocking Read:
  ```c
  static ssize_t output_data_show(...) {
      // Wait for processing to complete
      if (wait_event_interruptible(fir->wait, !fir->processing))
          return -ERESTARTSYS;

      // Read output buffer from hardware
      for (i = 0; i < fir->len; i++)
          fir->output_buf[i] = readl(fir->base + DATA_OUT_BASE + i*4);

      // Format and return to user
      return scnprintf(buf, PAGE_SIZE, ...);
  }
  ```

## Testing Workflow

### 1. Load Driver
```bash
insmod fir_irq.ko
dmesg | grep fir
# Should see: probe succeeded, IRQ 66 registered
```

### 2. Configure Coefficients
```bash
# Set 4-tap lowpass filter coefficients (Q15 format)
echo "8192 16384 16384 8192" > /sys/devices/platform/fir/coefficients
```

### 3. Write Input Data
```bash
# 32 samples: impulse at sample 10
echo "0 0 0 0 0 0 0 0 0 0 32767 0 0 ..." > /sys/devices/platform/fir/input_data
echo 32 > /sys/devices/platform/fir/len
```

### 4. Start Processing (Non-Blocking)
```bash
echo 1 > /sys/devices/platform/fir/start
# Returns immediately, hardware runs in background
```

### 5. Block on Read (Wait for Completion)
```bash
# This blocks until DONE interrupt fires
cat /sys/devices/platform/fir/output_data
# Prints: 8192 16384 16384 8192 0 0 0 ...
```

### 6. Verify Interrupt
```bash
cat /proc/interrupts | grep fir
#  66:          1   GIC-0  fir
```

## Debugging Scenarios

### Interrupt Never Fires
1. Check VCD: is `irq_out` asserted? Is STATUS.DONE set?
2. Renode: `sysbus WhatPeripheralsAreOnInterrupt 66`
3. Device tree: verify SPI number matches `.repl` wiring
4. Driver: `cat /proc/interrupts` (is IRQ registered?)

### Process Blocks Forever
1. Verify interrupt fires: `cat /proc/interrupts` (count incrementing?)
2. Add printk in handler before `wake_up_interruptible()`
3. Check `processing` flag: add sysfs attribute to read it
4. VCD: verify STATUS.DONE assertion timing

### Deadlock
1. Cause: using `spin_lock()` instead of `spin_lock_irqsave()`
   - IRQ fires while process holds lock
   - Handler tries to acquire same lock → deadlock
2. Solution: always use `irqsave` variant for locks shared with IRQ
3. Enable lockdep: `CONFIG_PROVE_LOCKING=y`

### Data Corruption in Coefficients
1. Cause: missing mutex around coefficient writes
2. Add `WARN_ON(!mutex_is_locked(&fir->coeff_lock))` in critical sections
3. Use lockdep to find missing locks

## Performance Comparison (vs Week 7 Polling)

### Polling (Week 7)
```c
writel(CTRL_START, base + CTRL);
do {
    status = readl(base + STATUS);
} while (!(status & STATUS_DONE));
// Total: ~10 µs (5 µs HW + 5 µs polling), 100% CPU
```

### Interrupts (Week 9)
```c
writel(CTRL_START, base + CTRL);
wait_event_interruptible(fir->wait, !fir->processing);
// Total: ~13 µs (5 µs HW + 8 µs IRQ overhead), 0% CPU during wait
```

**Verdict**: Interrupts ~3 µs slower but CPU is free to do other work.

## Advanced Topics (Optional)

### Top-Half / Bottom-Half

If output buffer copy is slow, defer to workqueue:

```c
struct fir_dev {
    struct work_struct work;
    // ...
};

static void fir_work_func(struct work_struct *work) {
    struct fir_dev *fir = container_of(work, struct fir_dev, work);
    // Copy output buffer (can take time)
    for (i = 0; i < fir->len; i++)
        fir->output_buf[i] = readl(fir->base + DATA_OUT_BASE + i*4);
    wake_up_interruptible(&fir->wait);
}

static irqreturn_t fir_irq_handler(int irq, void *dev_id) {
    // Top-half: minimal work
    writel(STATUS_DONE_BIT, fir->base + STATUS_REG);
    schedule_work(&fir->work);  // Defer to bottom-half
    return IRQ_HANDLED;
}
```

### Multiple Outstanding Requests

Use FIFO and per-request completion:

```c
struct fir_request {
    s16 input[32];
    s16 output[32];
    struct completion done;
    struct list_head list;
};

// Queue request, wait on individual completion
// IRQ handler pops from queue, completes one request at a time
```

## Files to Create

- `rtl/fir_q15_axil_irq.v` - Modified RTL with STATUS.DONE and irq_out
- `rtl/fir_q15_core.v` - Unchanged from Week 7
- `rtl/test_fir_irq.py` - Updated Cocotb tests for IRQ behavior
- `driver/fir_irq.c` - Interrupt-driven driver
- `driver/Makefile`
- `renode/dts/zynq-zed-fir-irq.dts` - Device tree with interrupt property
- `renode/overlays/fir_cosim.repl` - Wiring with GIC
- `renode/scripts/demo_fir_irq.resc`

## Key Locking Patterns

**Mutex (Process-Only)**:
```c
mutex_lock(&fir->coeff_lock);
fir->coefficients[i] = value;
mutex_unlock(&fir->coeff_lock);
```

**Spinlock (IRQ-Safe)**:
```c
unsigned long flags;
spin_lock_irqsave(&fir->status_lock, flags);
fir->processing = false;
spin_unlock_irqrestore(&fir->status_lock, flags);
```

**Wait Queue**:
```c
// Wait: blocks until !processing
wait_event_interruptible(fir->wait, !fir->processing);

// Wake: from IRQ handler
wake_up_interruptible(&fir->wait);
```

## References

- Week 7 demo (polling version)
- Week 8 demo (basic IRQ handler)
- `kernel/locking/spinlock.c`
- `kernel/locking/mutex.c`
