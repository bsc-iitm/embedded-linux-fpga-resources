---
marp: true
paginate: true
title: Week 1 — MMIO & AXI‑Lite Basics
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

# MMIO and AXI-Lite

---

# Week 1 — MMIO & AXI‑Lite Basics

- Goals: MMIO model, AXI‑Lite handshakes, clean register design
- Tools: cocotb + Verilator, GTKWave
- Outcome: read/write a tiny register bank in simulation

---

## Review of Digital Systems

<div class="columns-2">
<div>

- **Logic gates**: Combinational, Sequential
- **Architecture**: 
  - **Datapath**: logic gates with enable signals
  - **Control**: FSM based control logic
</div>
<div>

![drop-shadow](../assets/datapath_control.png)
</div>
</div>

---

#### Embedded C Programming - CPU and Memory

![drop-shadow](../assets/cpu-mem.png)

---

#### Embedded C Programming - Memory Map

![width:25cm drop-shadow](../assets/cpu-mmio.png)

---

## Registers

- Configurable hardware devices: set/get parameters
  - PWM width, duty cycle; read events
- Control / Status registers
- Not suitable for high speed transfer
  - Mainly to configure transfers

---

## Example: Counter

- Enable: 1-bit register (r/w)
- Up/Down: 1-bit register (r/w)
- Load: 32-bit data value (w)
- Present count: 32-bit data value (r)
- Threshold: 32-bit data value (w)
- Trigger: 1-bit output register (w)

---

## Memory Mapped IO

![CPU to peripherals](../assets/mmio-address-map.svg)

- CPU uses loads/stores to MMIO address ranges
- Interconnect routes transactions to peripherals
- Each peripheral exposes a small **register bank**

---

## MMIO Fundamentals

- Memory vs MMIO: similar access, different side‑effects
- Alignment and width: assume 32-b data width
- Caches: MMIO is typically uncached/strongly ordered 
  - Details of cache architectures out of scope
  - *L1 faster than L2 faster than L3 faster than main memory...*

---

<!-- _class: lead -->

# The AXI Bus Protocol

---

## AXI Overview

Bus protocol definition from ARM.  Widely used by others, including Xilinx.

- Protocol for data transfer; transactions; scalability
- [Xilinx AXI Reference](https://docs.amd.com/v/u/en-US/ug1037-vivado-axi-reference-guide)
- [ARM official documentation](https://developer.arm.com/documentation/102202/0300/AXI-protocol-overview?lang=en)

---

## Channel Architecture

![AXI channels](../assets/axi-channels.svg)

- Five channels: AW,W,B and AR,R
- Writes: address + data $\rightarrow$ response
- Reads: address $\rightarrow$ data + response

---

## AXI Handshake

![AXI Handshake](../assets/axi-hs-directions.svg)

- Old Terminology: _Master/Slave_ - new **Manager/Subordinate**
- **Source/Destination** depends on type of transfer

<small>Courtesy: AXI Specification, &copy; ARM</small>

---

## Handshake Mechanics

![height:12cm](../assets/axi-hs-1.svg)

<small>Courtesy: AXI Specification, &copy; ARM</small>

---

## Handshake Mechanics

![height:12cm](../assets/axi-hs-2.svg)

<small>Courtesy: AXI Specification, &copy; ARM</small>

---

## Handshake Mechanics

![height:12cm](../assets/axi-hs-3.svg)

<small>Courtesy: AXI Specification, &copy; ARM</small>

---

## Handshake Rules

- A source (master) cannot wait for `READY` to be asserted before asserting `VALID`.
- A destination (slave) can wait for `VALID` to be asserted before asserting `READY`.

$\Rightarrow$ `READY` can be asserted at any time: before or after `VALID`

---

### AXI Write Transaction - Address

![height:12cm](../assets/axi-write-1.svg)

<small>Courtesy: AXI Specification, &copy; ARM</small>

---

```verilog
  // Write address handshake/latch
  always @(posedge ACLK or negedge ARESETn) begin
    if (!ARESETn) begin
      aw_hs_done <= 1'b0;
      awaddr_q   <= {ADDR_WIDTH{1'b0}};
    end else begin
      if (!aw_hs_done && saxi_awvalid && saxi_awready) begin
        aw_hs_done <= 1'b1;
        awaddr_q   <= saxi_awaddr;
      end
      // Clear after write response accepted
      if (saxi_bvalid && saxi_bready) begin
        aw_hs_done <= 1'b0;
      end
    end
  end
```

---

### AXI Write Transaction - Data

![height:12cm](../assets/axi-write-2.svg)

<small>Courtesy: AXI Specification, &copy; ARM</small>

---

```verilog
  // Write data handshake/latch
  always @(posedge ACLK or negedge ARESETn) begin
    if (!ARESETn) begin
      w_hs_done <= 1'b0;
      wdata_q   <= {DATA_WIDTH{1'b0}};
      wstrb_q   <= {DATA_WIDTH/8{1'b0}};
    end else begin
      if (!w_hs_done && saxi_wvalid && saxi_wready) begin
        w_hs_done <= 1'b1;
        wdata_q   <= saxi_wdata;
        wstrb_q   <= saxi_wstrb;
      end
      if (saxi_bvalid && saxi_bready) begin
        w_hs_done <= 1'b0;
      end
    end
  end
```

---

### AXI Write Transaction - Response

![height:12cm](../assets/axi-write-3.svg)

<small>Courtesy: AXI Specification, &copy; ARM</small>

---

### AXI Read Transaction - Address

![height:12cm](../assets/axi-read-1.svg)

<small>Courtesy: AXI Specification, &copy; ARM</small>

---

### AXI Read Transaction - Data and Response

![height:12cm](../assets/axi-read-2.svg)

<small>Courtesy: AXI Specification, &copy; ARM</small>

---

## Transfers vs Transactions

- **Transfer**: single exchange of information with one `VALID`/`READY` handshake
- **Transaction**: Entire burst/sequence of Address transfer, one or more data transfers, and response transfer (for writes)

We will focus on simple transfers using AXI-Lite for now

---

## Summary

- Configurable digital hardware
  - Focused hardware for high performance
  - Software for flexibility and control
- Bus protocol critical for standardization
  - AXI
  - Handshaking