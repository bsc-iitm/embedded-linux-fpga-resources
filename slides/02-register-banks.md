---
marp: true
paginate: true
title: Week 2 — Register Banks & Behaviors (AXI‑Lite)
author: Nitin Chandrachoodan
theme: gaia
style: |
  .columns-2 {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 1rem;
    font-size: 0.75em;
  }
  .columns-3 {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 1rem;
    font-size: 0.75em;
  }
math: mathjax
---

<!-- _class: lead -->

# Register Banks & AXI‑Lite

---

# Week 2 — Register Design Goals

- Build clean, byte‑maskable register banks on AXI‑Lite
- Common behaviors: RW, RO, W1C, self‑clearing pulses
- Shadow registers - make updates coherent with datapath timing
- Demo: `smart_timer_axil.v`

---

## Why Register Banks?

- Software control of hardware blocks via MMIO
- Small, discoverable API: address map + documented side‑effects
- Decouples datapath timing from software transactions
- Enables debug/observability with status bits and counters

---

## Datasheet Information

- **Functional Description**
  - **What** does it do, what are the limitations, when would you use it
- **Programmers Model**
  - **How** do you go about using it

---

## Example: ARM UART

<div class="columns-2">
<div>

![](../assets/arm-uart.svg)
</div>
<div>

- Interfaces
- Functionality (DMA, APB, FIFO structure)
- Interrupt
</div>
</div>

---

## Programmers Model for UART

- List of registers and usage scenarios
- **Base address** generally **NOT** fixed.  **Offsets** within address space fixed.
- Reserved and Unused Locations: subject to change - should not be used/accessed

---

## ARM UART Summary of Registers

[Summary of Registers](https://developer.arm.com/documentation/ddi0183/g/programmers-model/summary-of-registers?lang=en)

- Brief listing to quickly understand nature of registers
- Detailed per-register bit-wise information

---

<!-- _class: lead -->

## Smart Timer

---

## Pulse-Width Modulation core

```verilog
module pwm_core (
  input  wire        clk,
  input  wire        rstn,
  input  wire        en,
  input  wire [31:0] period,
  input  wire [31:0] duty,
  output reg         pwm_out,
  output reg         wrap
);
```

---

## Basic Functionality

<div class="columns-2">
<div>

#### PWM Core
- Count up to period
- Set PWM output as long as count < duty 
- Indicate if *wrap* (overflow) occurs
</div>

<div>

#### Software controllable interface

- Set period for the counter
- Set duty cycle
- Check whether wraparound has occurred
- Reset/Clear the counter when needed
</div>
</div>

---

## Smart Timer V1 (PWM) — Register Map

```
Base +0x00 CTRL   RW     [bit0 EN] [bit1 RST=W1P]     others RAZ/WI
Base +0x04 PERIOD RW     terminal count               reset=0x000000FF
Base +0x08 DUTY   RW     compare threshold            reset=0x00000000
Base +0x0C STATUS RO/W1C [bit0 WRAP sticky] [bit1 UPD_PENDING]
```

- Shadows: writes to PERIOD/DUTY while EN=1 set `UPD_PENDING`; commit to actives on wrap
- STATUS.WRAP is sticky; clear with W1C

---

## AXI‑Lite Address Decode (word select)

```verilog
// Inside smart_timer_axil.v
wire do_write = aw_hs_done && w_hs_done && ~saxi_bvalid;
wire [1:0] word_sel_w = awaddr_q[3:2];
wire [1:0] word_sel_r = araddr_q[3:2];
```

- Word‑aligned map: bits [3:2] select 0x00/0x04/0x08/0x0C
- Register aliasing: 0x00 or 0x01 are decoded to the same location.  Avoid confusion in hardware: enforce word level access.  
  - `wstrb` signal used for byte level control if needed

---

## Respect Byte Enables (`WSTRB`)

```verilog
// PERIOD byte‑granular writes
for (i = 0; i < DATA_WIDTH/8; i = i + 1) begin
  if (wstrb_q[i]) period_shadow[i*8 +: 8] <= wdata_q[i*8 +: 8];
end
```

- Always honor `WSTRB` for partial/unaligned software writes
- Document which bytes matter for short fields (e.g., CTRL uses only byte 0)

---

## Self‑Clearing Pulse (Write‑1‑to‑Pulse)

```verilog
// Default low each cycle unless reasserted by a write
ctrl_rst_pulse <= 1'b0;
// CTRL bit1 is RST pulse: write 1 -> one‑cycle pulse; reads as 0
if (wstrb_q[0]) begin
  ctrl_en        <= wdata_q[0];
  ctrl_rst_pulse <= wdata_q[1];
end
```

- Use for actions, not state: soft reset, start capture, kick watchdog
- Readback should not latch pulses; return 0 for the pulse bit

---

## Sticky Status with W1C

```verilog
// STATUS bit0: WRAP sticky; clear on W1C
if (wstrb_q[0] && wdata_q[0]) status_wrap <= 1'b0;

// Set on event elsewhere
if (wrap_pulse) status_wrap <= 1'b1;
```

- W1C (Write‑1‑to‑Clear) is robust against lost events and shared flag bits
- Prefer W1C over read‑to‑clear for multi‑source status

---

#### Shadow vs Active — Coherent Updates

```verilog
// While enabled, writes set UPD_PENDING and defer commit to wrap
if (ctrl_en) upd_pending <= 1'b1; else begin
  period_active <= period_shadow;
  upd_pending   <= 1'b0;
end

// Commit on wrap (datapath boundary)
if (status_wrap && upd_pending) begin
  period_active <= period_shadow;
  upd_pending   <= 1'b0;
end
```

- Avoids mid‑cycle glitches; updates take effect at a safe boundary
- Pattern generalizes to timers, PWM, rate generators, dividers

---

## Readback Paths

```verilog
case (saxi_araddr[3:2])
  2'b00: saxi_rdata <= {30'd0, 1'b0, ctrl_en}; // RST reads 0
  2'b01: saxi_rdata <= period_shadow;          // echo shadows
  2'b10: saxi_rdata <= duty_shadow;
  2'b11: saxi_rdata <= {30'd0, upd_pending, status_wrap};
endcase
```

- Make RO fields and pulse bits read predictably (0 or current state)
- Keep reserved bits RAZ/WI (read‑as‑zero / writes‑ignored)

---

## Real‑World Register Maps (References)

- AMD/Xilinx AXI Timer (PG079): dual timers, control/status, W1C flags
  - https://docs.amd.com/r/en-US/pg079-axi-timer
- ARM PrimeCell UART (PL011) TRM: data, status, control, masked IRQ
  - https://developer.arm.com/documentation/ddi0183/latest

---

## Common Register Behaviours (Patterns)

- RW/RO/WO: basic access control; document reset values
- W1C (Write‑1‑to‑Clear): interrupt/status flags; avoids read‑modify‑write races
- W1S (Write‑1‑to‑Set): enables, pending bits, feature latches
- W1T (Write‑1‑to‑Toggle): GPIO toggle, clock gating flip
- W0C/W0S: rarer; be explicit if used

---

## Common Register Behaviours (Contd.)
- RC (Read‑to‑Clear): common for flags
- Self‑clearing pulse (W1P): start/stop/reset actions; reads return 0
- Sticky‑until‑reset: fault latches that survive clears
- Lock (write‑once until reset): freeze configuration after boot
- Set/Clear alias regs: `REG_SET` and `REG_CLR` to avoid RMW hazards
- Shadowed updates: coherent apply on boundary (wrap/frame/vblank)
- Indirect index/data: large tables via two registers

---

## Design Checklist

- Define reset values and RAZ/WI reserved bits
- Honor `WSTRB` and alignment; respond `OKAY`/`SLVERR` consistently
- Appropriate use of pulses; avoid latching write‑only bits
- Provide sticky status with W1C; avoid RC for shared flags
- Use shadows for atomicity and glitch‑free updates
- Keep decode small; document address map clearly

---

## Summary

- Clean register banks turn datapaths into usable peripherals
- AXI‑Lite basics + disciplined register behaviors = robust MMIO
- Real datasheets use the same patterns: W1C, pulses, shadows, locks

