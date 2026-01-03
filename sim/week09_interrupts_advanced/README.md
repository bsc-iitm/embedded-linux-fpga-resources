# Week 9 — Advanced Interrupt Driver Patterns

This directory contains demos for Week 9: wait queues, locking, and interrupt-driven blocking I/O.

## Directory Structure

```
week09_interrupts_advanced/
├── demo_smarttimer_blocking/   # Smart Timer with blocking read (wait for N wraps)
├── demo_fir_irq/               # FIR filter with interrupt-driven completion
└── demo_perf_comparison/       # Performance comparison: polling vs interrupts
```

## Learning Progression

1. demo_smarttimer_blocking: Char device `read()` blocks until next wrap (wait queue).
2. demo_fir_irq: FIR signals DONE via IRQ; `read()` blocks until DONE; spinlock + mutex split.
3. demo_perf_comparison: High-level guidance to compare polling vs interrupt drivers.

## Prerequisites

- Week 8: Basic interrupt handling, minimal IRQ handler
- Week 7: Verilator co-simulation, FIR filter hardware
- Week 6: Platform drivers, sysfs

## Key Concepts Demonstrated

- Wait queues for blocking I/O
- Spinlocks with `spin_lock_irqsave()` for IRQ-safe critical sections
- Mutexes for process-context-only shared data
- Top-half/bottom-half pattern (optional, in comments)
- Interrupt context restrictions
- Performance analysis: polling vs interrupts

## Quick Start

Typical workflow
- Build RTL and Verilator library (`rtl/` → `verilator_cosim/`)
- Compile device tree (`renode/`)
- Build driver module (`driver/`)
- Run in Renode, load driver in guest, test blocking behavior

## Common Debugging Scenarios

- **Process blocks forever**: Check if interrupt fires (`/proc/interrupts`), verify wakeup in handler
- **Deadlock**: Missing `spin_lock_irqsave()` (use lockdep)
- **Race condition**: Missing lock around shared data
- **Interrupt storm**: Not clearing W1C in handler (check VCD)
