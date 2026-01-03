# Week 6 — Chunked MMIO Demo (FFT-style block)

Goal: show how a platform driver can handle small block transfers (e.g., 32 complex samples) into a device’s MMIO “data window”, without diving into HDL details. This illustrates memory model choices for chunked data: register-FIFO vs MMIO array vs DMA.

We reuse Renode with two MMIO windows for teaching: SmartTimer at 0x70000000 and FFT at 0x70001000. There is no real FFT; this is purely about driver-side structure and API.

## Memory Map (teaching stub)

- `0x00 CTRL`  — bit0 `EN`, bit1 `START`
- `0x04 STATUS`— bit0 `DONE`
- `0x08 LEN`   — number of complex samples written (max 32)
- `0x100 DATA` — 64 x 32-bit words (interleaved real, imag) = 32 complex

## Driver model

This driver binds via DT and exposes:
- `vector` (WO): appends up to 64 u32 values across multiple writes; updates `LEN` (complex count)
- `ctrl` (WO): accepts `reset` (clear position and `LEN`) and `start` (pulse START). Numeric writes also supported: bit0 EN, bit1 START (pulsed), bit2 RESET
- `len` (RO): shows current `LEN` (number of complex samples)

This keeps concepts simple for Week 6; for production designs and larger data, prefer DMA to avoid PIO overhead, and avoid using sysfs for bulk binary data.

## Build

```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make -C /path/to/linux M=$(pwd) modules
# or
make KDIR=/path/to/linux
```

Produces: `fft_block_demo.ko`

## Device Tree

This module matches a dedicated FFT node: `compatible = "acme,fft32-v1"`.
For Week 6, we add a second device alongside the SmartTimer:

```dts
axi {
  smarttimer0: smart-timer@70000000 {
    compatible = "acme,smart-timer-v1";
    reg = <0x70000000 0x1000>;
    status = "okay";
  };

  fft0: fft32@70001000 {
    compatible = "acme,fft32-v1";
    reg = <0x70001000 0x1000>;
    status = "okay";
  };
};
```

## Try it in the guest

```bash
modprobe fft_block_demo

# Use the device path (example path may vary)
ls /sys/bus/platform/devices | grep 70001000

# Start from a clean buffer
echo "reset" > /sys/bus/platform/devices/70001000.fft32/ctrl

# Append values (position advances each write)
echo "1,2,3,4" > /sys/bus/platform/devices/70001000.fft32/vector
echo "5,6"     > /sys/bus/platform/devices/70001000.fft32/vector

# Inspect length (complex pairs)
cat /sys/bus/platform/devices/70001000.fft32/len

# Trigger processing when ready
echo "start" > /sys/bus/platform/devices/70001000.fft32/ctrl

# Optional: status (DONE bit remains 0 in the stub)
cat /sys/bus/platform/devices/70001000.fft32/status
```

## Notes — When to pick which model

- Register FIFO: write one word at a time to `DATA`. Simple HW, add `LEVEL`/`READY` in `STATUS`. Good for low throughput.
- MMIO array: expose a small on-device SRAM window. Driver `memcpy`-like loops into it, then writes `LEN`/`START`. Good for modest bursts (e.g., 32–256 items).
- DMA: for large or frequent buffers, allocate DMA-safe memory and program the device with buffer address/length. Minimizes CPU copy; requires DMA engine in HW.

### Teaching stub control notes
- `vector` appends values into the `DATA[]` window and updates `LEN`.
- `ctrl` accepts textual commands:
  - `reset`: clears `LEN` and resets the write position to 0
  - `start`: pulses START (bit1) in `CTRL`
  - numeric value: sets EN/START (bit0/bit1); bit2 acts as reset
