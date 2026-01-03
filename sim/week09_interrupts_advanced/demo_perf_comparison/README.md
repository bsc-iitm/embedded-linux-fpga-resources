# Performance Comparison: Polling vs Interrupts

Purpose
- Compare polling vs interrupt-driven drivers on the same FIR hardware.

Approach
- Use the Week 7/9 FIR plus two drivers (polling vs IRQ); measure latency and CPU time for batches of operations.

Notes
- Polling can reduce latency for very short operations; interrupts free CPU for other work.

