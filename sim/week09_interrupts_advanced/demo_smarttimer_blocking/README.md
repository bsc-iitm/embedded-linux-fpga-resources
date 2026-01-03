# Smart Timer with Blocking Read

Demonstrates wait queues for blocking I/O: user process blocks on `read()` until N timer wraps occur.

## Purpose

- Show a minimal, race-free blocking I/O pattern using wait queues.
- Use a char device (`/dev/smarttimer0`) to block until the next wrap interrupt.

## Hardware

**Reuses Week 8 Smart Timer RTL** (no changes needed):
- IRQ output asserts on counter wrap (level-triggered)
- STATUS.WRAP bit set on wrap, cleared via W1C
- Same register map as Week 8

## Driver Logic (high level)

- read(): compute `target = wrap_count + 1`; sleep until `wrap_count >= target`.
- IRQ: clear W1C, increment `wrap_count`, wake the wait queue.
- Sysfs controls (`ctrl`, `period`, `duty`, `status`, `irq_count`) mirror Week 8.

## Implementation Notes

- RTL, Verilator wrapper, overlay, and DTB are copied from Week 8 (SPI 33 wiring).
- Driver adds a misc char device for blocking reads; sysfs remains unchanged.

## Testing Workflow

### 1. Load Driver
```bash
insmod smarttimer_blocking.ko
```

### 2. Configure Timer
```bash
echo 50000000 > /sys/devices/platform/smarttimer/period  # 1 second @ 50 MHz
echo 1 > /sys/devices/platform/smarttimer/enable
```

### 3. Test Blocking Read (char device)
```bash
# Terminal 1: blocks until next wrap
cat /dev/smarttimer0
echo "Woke up!"

# Terminal 2: watch wrap count increment
watch cat /sys/devices/platform/smarttimer/irq_count
```

### 4. Test Signal Interruption
```bash
# Start blocking read
cat /dev/smarttimer0 &
PID=$!

# After a moment, send signal
kill -INT $PID  # Should return -EINTR (interrupted system call)
```

### 5. Test Multiple Waiters
```bash
# Open 3 terminals, all block on read
# IRQ handler should wake all of them simultaneously
cat /dev/smarttimer0  # Terminal 1
cat /dev/smarttimer0  # Terminal 2
cat /dev/smarttimer0  # Terminal 3
# All wake after next wrap
```

## Expected Output

```
# dmesg
[   10.123] smarttimer_blocking: probe: mapped 0x70000000, IRQ 65
[   15.456] smarttimer_blocking: IRQ handler: wrap #1
[   16.456] smarttimer_blocking: IRQ handler: wrap #2
[   17.456] smarttimer_blocking: IRQ handler: wrap #3
```

```
# /proc/interrupts
           CPU0
 65:          5   GIC-0  smarttimer
```

## Debugging Scenarios

### Process Blocks Forever
1. Check if IRQ fires: `cat /proc/interrupts` (count incrementing?)
2. Add printk in handler before `wake_up_interruptible()`
3. Verify `wrap_count` increments: `cat /sys/.../wrap_count`
4. Check VCD: is `irq_out` toggling?

### Race Condition (Missed Wakeup)
- `wait_event_interruptible()` rechecks condition on every wakeup (race-free)
- If you implement manually with `prepare_to_wait()`, must check condition before sleeping

### Signal Not Interrupting Wait
- Use `wait_event_interruptible()` (not `wait_event()`)
- Return `-ERESTARTSYS` to indicate interrupted syscall

## Advanced Variant (Optional)

**Wait for N wraps** (not just 1):

```c
ssize_t smarttimer_read(struct file *file, char __user *buf, size_t count, ...) {
    u32 n_wraps;
    int target;

    if (copy_from_user(&n_wraps, buf, sizeof(u32)))
        return -EFAULT;

    target = atomic_read(&st->wrap_count) + n_wraps;

    if (wait_event_interruptible(st->wait,
                                   atomic_read(&st->wrap_count) >= target))
        return -ERESTARTSYS;

    return sizeof(u32);
}
```

User writes number of wraps to wait for:
```c
uint32_t n = 5;  // Wait for 5 wraps
read(fd, &n, sizeof(n));  // Blocks until 5 wraps occur
```

## Files to Create

- `driver/smarttimer_blocking.c` - Enhanced driver with wait queue
- `driver/Makefile` - Standard kernel module build
- `renode/dts/zynq-zed-smarttimer-blocking.dts` - Device tree (same as Week 8)
- `renode/overlays/smarttimer_cosim.repl` - Peripheral wiring
- `renode/scripts/demo_blocking.resc` - Renode startup script
- `rtl/Makefile` - Build Verilator library (or use CMake)

## Key Code Patterns

**Initialization**:
```c
init_waitqueue_head(&st->wait);
atomic_set(&st->wrap_count, 0);
```

**Wait**:
```c
if (wait_event_interruptible(st->wait, condition))
    return -ERESTARTSYS;
```

**Wakeup**:
```c
wake_up_interruptible(&st->wait);  // In IRQ handler
```

## References

- `kernel/sched/wait.c` - Wait queue implementation
- `include/linux/wait.h` - Wait queue API
- LDD3 Chapter 6: Advanced Char Driver Operations
