---
marp: true
paginate: true
title: Week 9 — Interrupts Part 2 - Advanced Driver Patterns
author: Nitin Chandrachoodan
theme: gaia
style: |
  .columns-2 { display: grid; grid-template-columns: repeat(2, 1fr); gap: 1rem; font-size: 0.8em; }
  code { font-size: 0.85em; }
math: mathjax
---

<!-- _class: lead -->

# Advanced Interrupt Patterns

Wait Queues • Locking • Deferred Work

---

# Goals

- Blocking I/O with wait queues
- Correct locking across IRQ and process
- Defer heavy work (top/bottom halves)
- Apply to FIR DONE interrupt
- Polling vs interrupt trade-offs

---

# Review: Minimal IRQ Handler

```c
// Minimal handler: ack + count
static irqreturn_t timer_irq(int irq, void *dev_id) {
    u32 s = readl(base + STATUS_REG);
    if (!(s & WRAP_BIT)) return IRQ_NONE;
    writel(WRAP_BIT, base + STATUS_REG);  // W1C
    atomic_inc(&irq_count);
    return IRQ_HANDLED;
}
```

Limitations: no blocking I/O, shared-state protection, or deferral

---

# Interrupt Context

Handlers are atomic (cannot sleep). Keep them short.

<div class="columns-2">
<div>

**Cannot**
- `msleep()` / `mutex_lock()`
- `copy_to_user()`
- `kmalloc(..., GFP_KERNEL)`

</div>
<div>

**Can**
- `readl()` / `writel()`
- Atomics
- `spin_lock_irqsave()`
- `wake_up_interruptible()`

</div>
</div>

Defer heavy work via tasklet/workqueue

---

# Wait Queues: Blocking I/O

```c
// read(): block until next wrap
int target = atomic_read(&st->wrap_count) + 1;
if (wait_event_interruptible(st->wait,
        atomic_read(&st->wrap_count) >= target))
    return -ERESTARTSYS;
return 0;
```

```c
// IRQ: ack + bump + wake
u32 s = readl(base + STATUS_REG);
if (s & WRAP_BIT) {
    writel(WRAP_BIT, base + STATUS_REG);
    atomic_inc(&st->wrap_count);
    wake_up_interruptible(&st->wait);
}
```
---

# Wait Queue Details

- Predicate must reflect a state variable
- Predicates can read atomics or locked fields
- Use `-ERESTARTSYS` to support signals
- Initialize once in `probe()`: `init_waitqueue_head(&wq)`

---

# Common Patterns

```c
// Wait for N events
int goal = atomic_read(&cnt) + N;
wait_event_interruptible(wq, atomic_read(&cnt) >= goal);
```

```c
// Timeout
long t = wait_event_interruptible_timeout(wq, cond, msecs_to_jiffies(10));
if (t == 0) return -ETIMEDOUT;
```

---

# Locks

```c
lock(&my_lock);  // This line blocks here until lock is acquired
/* Code only reaches here after successfully getting the lock */
critical_section();   // Protected code
unlock(&my_lock);
```

- Protect certain sections of code: only after getting the lock can we do this
- Any other thread trying to change the code must also get the lock
- Custom machine instructions: `CMPXCHG` on x86, `LDREX/STREX` on ARM etc.

---

# Mutex vs Spinlock

<div class="columns-2">
<div>

### Mutex

- Flag/counter to indicate lock state (can be checked/modified)
- wait queue in OS for threads sleeping on this lock
- system calls to block/wake threads

</div>
<div>

### Spinlock

- Busy-wait loop: 
```c
while (atomic_test_and_set(&lock->value) != 0) {} ;
```
- Burns CPU cycles while waiting
- `spin_lock_irqsave` - safer version - disables interrupts until unlocked

</div>
</div>

---

# Locking (IRQ + Process)

```c
// Shared state touched in IRQ and process
unsigned long flags;
spin_lock_irqsave(&st->lock, flags);
/* small critical section */
spin_unlock_irqrestore(&st->lock, flags);
```

- Spinlock for IRQ-shared state
- Mutex for process-only paths
- Never take mutex in an IRQ handler

---

# Mutex vs Spinlock

<div class="columns-2">
<div>

**Context**
- Mutex: process context only, may sleep
- Spinlock: IRQ or process context, never sleeps

**Behavior**
- Mutex: scheduler may switch; fair, low CPU waste
- Spinlock: busy-waits; tiny, fast sections only

</div>
<div>

**Use Cases**
- Mutex: long ops, copy_to_user, parse input, allocate memory
- Spinlock: flags/counters shared with IRQ, very short updates

**IRQ Note**
- If IRQ touches data: use `spin_lock_irqsave()` in process path; never use a mutex in IRQ

</div>
</div>

---

# Locking Do/Don't

Do:
- Use `spin_lock_irqsave()` for data shared with IRQ
- Keep critical sections short
- Use `WRITE_ONCE()/READ_ONCE()` for flags

Don't:
- Take a mutex in IRQ
- Sleep in IRQ or tasklet
- Hold a spinlock across `copy_to_user()`


---

# Defer Work (Bottom Half)

```c
static void st_workfn(struct work_struct *w) { /* can sleep */ }
DECLARE_WORK(st_work, st_workfn);

static irqreturn_t st_irq(int irq, void *d) {
  /* ack + bookkeeping */
  schedule_work(&st_work);
  return IRQ_HANDLED;
}
```

- Tasklet: atomic, very short
- Workqueue: process context, can sleep

---

# Workqueue vs Tasklet

Tasklet:
- Runs in softirq (atomic)
- Very short work, no sleep

Workqueue:
- Process context
- Can sleep, do I/O

Guideline: Prefer workqueues unless you need atomic bottom halves

---

# FIR: Minimal IRQ Handler

```c
static irqreturn_t fir_irq(int irq, void *dev_id) {
    struct fir *f = dev_id;
    u32 s = readl(f->base + STATUS);
    if (!(s & DONE_BIT)) return IRQ_NONE;
    writel(DONE_BIT, f->base + STATUS);   // W1C
    spin_lock(&f->lock);
    f->done = true;                       // update flag
    spin_unlock(&f->lock);
    wake_up_interruptible(&f->wait);
    return IRQ_HANDLED;
}
```
---

# FIR DONE Interrupt

Flow:
 Write coefficients and input $\rightarrow$ Start processing $\rightarrow$  `read()` blocks $\rightarrow$ IRQ asserts on DONE → wake and copy results

```c
// Block until DONE
if (wait_event_interruptible(fir->wait, READ_ONCE(fir->done)))
    return -ERESTARTSYS;
/* copy out */
```

Keep flags (`done`, `processing`) consistent under a spinlock

---

# Polling vs Interrupts

- Polling: lower latency for very fast ops (<1 µs), wastes CPU
- Interrupts: slightly higher latency, frees CPU for work
- Choose based on operation time, rate, and system load

---

# Debugging

**IRQ storm**: forgot W1C → check VCD/`/proc/interrupts`

**Spurious IRQs**: return `IRQ_NONE` if status not set

**Missed wakeup**: predicate `wait_event*()` correctly

**Deadlock**: use `spin_lock_irqsave()` for IRQ-shared state

---

# Summary

- Handlers are atomic: keep minimal
- Wait queues enable blocking I/O
- Spinlocks for IRQ-shared state; mutex for process-only
- Defer heavy/slow work to a workqueue
- Apply same patterns to FIR DONE


