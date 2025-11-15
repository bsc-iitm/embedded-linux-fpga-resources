---
marp: true
paginate: true
title: Week 7 — Hardware Co-Simulation with Verilator and Renode
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
    font-size: 0.65em;
  }
  code {
    font-size: 0.85em;
  }
math: mathjax
---

<!-- _class: lead -->

# Hardware Co-Simulation

Verilator + Renode Integration

---

# Goals

- Understand hardware co-simulation workflow
- Learn how Verilator converts Verilog to C++ libraries
- Integrate real RTL into Renode system emulation
- Validate hardware with bare-metal and Linux software

---

# What is Hardware Co-Simulation?

**Co-Simulation**: Running real hardware (RTL) alongside system emulation

<div class="columns-2">
<div>

**Pure Emulation**
- C# stub models in Renode
- Fast execution
- Easy setup
- May differ from real HW

</div>
<div>

**Co-Simulation**
- Real Verilog via Verilator
- Synthesizable code
- Accurate behavior
- Slower but realistic

</div>
</div>

**Use co-sim for**: Protocol validation, DSP blocks, pre-synthesis testing

---

# Why Co-Simulation?

**Traditional Flow - Problem**: Gap between HW/SW dev
$\Rightarrow$ Hardware team $\rightarrow$ Design RTL $\rightarrow$ Synthesize $\rightarrow$ FPGA
$\Rightarrow$ Software team $\rightarrow$ Wait $\rightarrow$ Write driver $\rightarrow$ Debug on HW
$\Rightarrow$ **Bottleneck!**

**Co-Simulation Flow**:
$\Rightarrow$ Hardware team $\rightarrow$ Design RTL $\rightarrow$ Verilator simulation 
$\Rightarrow$ Software team $\rightarrow$ Write driver $\rightarrow$ Test in Renode 
$\Rightarrow$ **Both teams validate together** $\rightarrow$ FPGA

---

# Verilator: Verilog to C++ Conversion

Verilog RTL (.v) $\rightarrow$ `verilator --cc --exe wrapper.cpp` $\rightarrow$
Generated C++ (cycle model) $\rightarrow$ 
`g++ $\rightarrow$ libVtop.so` (shared library) $\rightarrow$
Renode loads as peripheral


**Key Points**:
- Converts `always @(posedge clk)` to C++ step functions
- Each bus transaction advances simulation clock
- Same RTL that goes to FPGA synthesis

---

### Renode Integration Architecture

![height:14cm](../assets/renode-integration-arch.svg)

---

# Clock Frequencies: Renode vs Verilator

**Two separate clock domains**:

<div class="columns-2">
<div>

**Renode Virtual Time**
- Emulated system clock
- Can run faster or slower than real-time
- Deterministic
- Example: 100 MHz CPU

</div>
<div>

**Verilator Simulation**
- Cycle-accurate HDL clock
- Each bus access = N cycles
- Configurable in wrapper
- Example: 50 MHz peripheral

</div>
</div>

---

# Clock Frequencies: Renode vs Verilator

**Interaction**: Bus transactions advance both clocks
- Renode MMIO read/write $\rightarrow$ triggers Verilator cycles
- Verilator advances by configured cycles per transaction
- No wall-clock dependency (fully deterministic)

---

# Demo 1: Smart Timer + Bare Metal

**Purpose**: Show co-simulation with simplest software (no OS)

<div class="columns-2">
<div>

**Three Steps**:
1. **Verilator Sim**: Validate Smart Timer RTL standalone
2. **Bare-Metal Code**: Write ARM code to configure timer
3. **Renode Co-Sim**: Run bare-metal on emulated CPU + real RTL
</div>
<div>

**What It Demonstrates**:
- Same register interface works in Verilator and Renode
- Hardware/software teams can work in parallel
- Early validation before FPGA deployment
</div>
</div>

---

![height:18cm](../assets/demo1-workflow.svg)

---

# Introducing the FIR Filter

- More complex than Smart Timer (batch processing, arrays)
- Introduces Q15 fixed-point arithmetic
- Vector-mode data transfer pattern
- Same co-simulation principles apply
  - Linux driver + Renode co-simulation (full stack)

---

# FIR Filter Fundamentals

**Finite Impulse Response** filter: weighted sum of recent samples
$$y[n] = \sum_{k=0}^{3} c[k] \cdot x[n-k]$$

- $x[n]$ = input samples
- $c[k]$ = filter coefficients (taps)
- $y[n]$ = filtered output

**Properties**: Always stable, linear phase, straightforward HW

---

# Example: 4-Tap Moving Average

Set all coefficients to 0.25:

$$y[n] = 0.25 \cdot (x[n] + x[n-1] + x[n-2] + x[n-3])$$

Smooths signal by averaging last 4 samples.

**Input**: `[4, 8, 12, 16, 20]`
**Output**: `[1, 3, 6, 10, 14]`

(Zero-padding for $n < k$)

---

# Q15 Fixed-Point Format

**16-bit signed fractional representation**

```
Bit layout:
  [15] Sign bit
  [14:0] Fractional part

Value = (signed integer) / 32768
```

**Range**: $[-1.0, 0.9999...]$

---

# Q15 Examples

| Hex Value | Decimal | Q15 Value |
|-----------|---------|-----------|
| `0x7FFF` | 32767 | ≈ 1.0 |
| `0x4000` | 16384 | 0.5 |
| `0x2000` | 8192 | 0.25 |
| `0x0000` | 0 | 0.0 |
| `0x8000` | -32768 | -1.0 |

---

# Q15 Arithmetic

<div class="columns-2">
<div>

**Addition**:
```
Q15 + Q15 = Q15
(direct integer add)
```

**Multiplication**:
```
Q15 × Q15 = Q30
(shift right by 15 $\rightarrow$ Q15)
```

</div>
<div>

**Why Q15 for FIR?**
- Coefficients typically $[-1, +1]$
- Compact (16 bits)
- Integer ALU + shift
- Precision: ~0.003%

</div>
</div>

---

# FIR Filter Memory Map

<div class="columns-3">
<div>

**Control**
```
0x000 CTRL
0x004 STATUS
0x008 LEN
```

</div>
<div>

**Coefficients**
```
0x010 COEFF0
0x014 COEFF1
0x018 COEFF2
0x01C COEFF3
```

</div>
<div>

**Data Arrays**
```
0x100 DATA_IN[32]
...
0x200 DATA_OUT[32]
```

</div>
</div>

**Total**: 4 KiB window

---

# Register Semantics

**CTRL** (RW): `[0]=EN  [1]=START(W1P)  [2]=RESET(W1P)`

**STATUS** (RO/W1C): `[0]=DONE(W1C)  [1]=READY`

**LEN** (RW): Number of samples (1-32)

**COEFF[0:3]** (RW): 16-bit signed Q15 values

**DATA_IN[0:31]** (WO): Input samples (16-bit each)

**DATA_OUT[0:31]** (RO): Filtered output samples

---

# Hardware Architecture

![height:15cm](../assets/fir-hardware-arch.svg)

---

# Processing State Machine

**IDLE**: READY=1, waiting for START pulse

**PROCESSING**:
- Loop over samples 0..LEN-1
- For each: compute FIR equation
- Write result to DATA_OUT[]

**DONE**: Set STATUS.DONE=1, return to IDLE

---

# FIR Computation (Verilog)

```verilog
// For each output sample n:
acc = 0;
for (k = 0; k < NUM_TAPS; k = k + 1) begin
  if (n >= k) begin
    // Q15 × Q15 = Q30
    acc = acc + (coeff[k] * data_in[n - k]);
  end
end
// Extract Q15 result from Q30
data_out[n] = acc[30:15];
```

Zero-padding at boundaries: `x[n-k] = 0` if `n < k`

---

# Usage Flow

1. **Configure**: Write COEFF0-COEFF3
2. **Load data**: Write DATA_IN[0] through DATA_IN[LEN-1]
3. **Set length**: Write LEN register
4. **Start**: Write `0x3` to CTRL (EN=1, START=1)
5. **Wait**: Poll STATUS.DONE (or use IRQ later)
6. **Read results**: Read DATA_OUT[0] through DATA_OUT[LEN-1]
7. **Acknowledge**: Write `0x1` to STATUS (W1C)

---

# FIR Demo 1: Standalone Verilator

**Purpose**: Pure RTL validation (no system integration yet)

**Test Case**: 4-tap averaging filter
- Coefficients: `[0x2000, 0x2000, 0x2000, 0x2000]`
- Input: `[4, 8, 12, 16, 20]`
- Expected: `[1, 3, 6, 10, 14]`

---

# Testbench Validation

Cocotb tests verify:
- AXI-Lite register read/write
- W1P behavior (START, RESET self-clear)
- W1C behavior (STATUS.DONE clears on write)
- FIR computation accuracy
- Passthrough test (single tap)

**Key Point**: Same RTL we'll use in Renode next

---

# FIR Demo 2: Linux + Co-Simulation

**Purpose**: Full-stack integration with Linux driver

**Architecture**:
Linux Driver (fir_simple.c)
$\Rightarrow$ Renode AXI Bus
$\Rightarrow$ Verilator Co-Sim (libVtop.so)
$\Rightarrow$ FIR Filter RTL (fir_filter_axil.v)

**Same RTL** from standalone Verilator demo, now integrated with OS

---

# Demo 2: Components

1. **`driver/`**: Linux platform driver with sysfs interface
   - `coeff`, `data_in`, `data_out` attributes
   - Handles Q15 conversion, batch processing

2. **`renode/`**: Device tree + peripheral definition
   - FIR node @ 0x70002000
   - Co-simulated peripheral (`.repl`)

3. **`verilator_cosim/`**: RTL wrapper + build
   - Produces `libVtop.so` for Renode

---

# Demo 2: What It Shows

- **Driver development** with real RTL (not stub)
- **Linux sysfs** interface for user-space access
- **Vector-mode** data transfer (32-sample arrays)
- **Full debugging**: dmesg + Verilator traces + Renode logs

**Workflow**: Same as Demo 1 (Smart Timer)
1. Validate RTL standalone $\rightarrow$ Verilator
2. Write driver code $\rightarrow$ Linux kernel
3. Test integration $\rightarrow$ Renode co-sim

---

# Performance

- Pure emulation: Real-time or faster
- Co-simulation: 10-100× slower (still interactive)
- Acceptable for development, too slow for regression tests

Set emulated clock frequency low enough to get fast simulation.  Not accurate model of AXI bus transactions.

---

# Debugging

**Debugging Co-Simulation Hangs**:

1. **FSM stuck?** Check state transitions in Verilog
2. **AXI deadlock?** Verify valid/ready handshake logic
3. **Clock not advancing?** Ensure bus transactions tick Verilator
4. **Infinite loop?** Check LEN bounds and counters

**Tools**: Verilator VCD traces, Renode monitor, `dmesg` logs

---

# Summary: Cosimulation

**Co-Simulation Concepts**:
- Verilator converts Verilog to C++ libraries
- Renode loads RTL as peripheral models
- Separate clock domains (virtual time vs cycle-accurate)
- Deterministic, repeatable testing

---

# Summary: Workflow

<div class="columns-2">
<div>

**Hardware Co-Simulation Workflow**:
1. **Validate RTL** standalone with Verilator (pure simulation)
2. **Integrate with system** via Renode co-simulation
3. **Test with software** (bare-metal or Linux)
4. **Deploy to FPGA** with confidence
</div>

<div>

**Benefits**:
- Parallel hardware/software development
- Early bug detection (before synthesis)
- Same RTL from simulation to silicon
- Full-stack debugging capability
</div>
</div>

