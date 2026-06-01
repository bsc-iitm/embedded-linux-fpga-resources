# Smart Timer Blocking (IRQ) Driver

Purpose
- Provide a char device (`/dev/smarttimer0`) whose `read()` blocks until the
  next timer wrap interrupt.
- Keep the sysfs controls for configuration (`ctrl`, `period`, `duty`,
  `status`).

Core logic
- IRQ handler: clear STATUS.WRAP (W1C), increment `wrap_count`,
  `wake_up_interruptible(&wait)`.
- read(): compute `target = wrap_count + 1`, then
  `wait_event_interruptible(wait, wrap_count >= target)` and return a short
  payload.

Notes
- Wait queue + counter avoids missed wakeups (the predicate reflects the
  event that already happened).
- This driver requires the interrupt to be wired in the bitstream and present
  in the Device Tree. It matches `compatible = "acme,smarttimer-v1"` with the
  interrupt on SPI 29 (`interrupts = <0 29 4>`, level-high), connected to
  `IRQ_F2P[0]` of the Zynq PS. If the IRQ is absent the probe fails with a
  message pointing you at the plain platform driver instead.

Build and install the same way as the platform driver (see `../driver_platform`).

See `smarttimer_blocking.c` for the exact implementation.
