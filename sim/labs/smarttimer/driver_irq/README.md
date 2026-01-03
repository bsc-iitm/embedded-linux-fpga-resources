# Smart Timer Blocking Driver

Purpose
- Provide a char device (`/dev/smarttimer0`) whose `read()` blocks until the next timer wrap interrupt.
- Keep Week 8 sysfs controls for configuration (`ctrl`, `period`, `duty`, `status`, `irq_count`).

Core logic
- IRQ handler: clear STATUS.WRAP (W1C), increment `wrap_count`, `wake_up_interruptible(&wait)`.
- read(): compute `target = wrap_count + 1`, then `wait_event_interruptible(wait, wrap_count >= target)` and return a short payload.

Notes
- Wait queue + atomic counter avoids missed wakeups (predicate reflects the event).
- Device tree and wiring are unchanged from Week 8 (SPI 33, level-high).

See smarttimer_blocking.c for the exact implementation used in this demo.
