# Week 7 — 4-Tap FIR Filter with AXI-Lite Interface

This directory contains a simple 4-tap FIR (Finite Impulse Response) filter with an AXI-Lite register interface. The filter uses Q15 fixed-point arithmetic and supports batch processing of up to 32 samples.

## Features

- **4 configurable taps**: Load coefficients via AXI-Lite registers
- **Batch processing**: Write input samples to memory-mapped array, read filtered outputs
- **Q15 fixed-point**: 16-bit signed values (range: -1.0 to ~0.9999)
- **Vector mode data transfer**: Similar to Week 6 FFT demo pattern
- **Status reporting**: DONE and READY flags

## Memory Map

```
Offset  Register    Access  Description
------  --------    ------  -----------
0x000   CTRL        RW      [0]=EN, [1]=START (W1P), [2]=RESET (W1P)
0x004   STATUS      RO/W1C  [0]=DONE (W1C), [1]=READY
0x008   LEN         RW      Number of samples to process (1-32)
0x00C   (reserved)

0x010   COEFF0      RW      Coefficient 0 (16-bit signed, Q15)
0x014   COEFF1      RW      Coefficient 1
0x018   COEFF2      RW      Coefficient 2
0x01C   COEFF3      RW      Coefficient 3

0x100   DATA_IN[]   WO      Input samples (32 x 32-bit words, lower 16 bits used)
        ...
0x17C   DATA_IN[31]

0x200   DATA_OUT[]  RO      Output samples (32 x 32-bit words, lower 16 bits valid)
        ...
0x27C   DATA_OUT[31]
```

## Register Semantics

- **CTRL.START** (bit 1): Write-1-pulse. Triggers FIR computation. Reads as 0.
- **CTRL.RESET** (bit 2): Write-1-pulse. Resets filter state. Reads as 0.
- **STATUS.DONE** (bit 0): Write-1-clear. Set when processing completes. Write 1 to clear.
- **STATUS.READY** (bit 1): Read-only. High when filter is idle and ready for new data.

## FIR Operation

The filter implements the standard FIR equation:

```
y[n] = sum(coeff[k] * x[n-k]) for k = 0 to 3
```

Where:
- `x[n]` are input samples (zero-padded for n < 0)
- `coeff[k]` are the filter taps
- `y[n]` are output samples

### Q15 Fixed-Point Format

- 16-bit signed integer representing values in range [-1.0, 0.9999...]
- 1 sign bit + 15 fractional bits
- Example values:
  - `0x7FFF` = 32767 = ~1.0
  - `0x4000` = 16384 = 0.5
  - `0x2000` = 8192 = 0.25
  - `0x0000` = 0 = 0.0
  - `0x8000` = -32768 = -1.0

### Example: Averaging Filter

For a simple 4-tap moving average filter, set all coefficients to 0.25:

```
COEFF0 = COEFF1 = COEFF2 = COEFF3 = 0x2000 (8192 = 0.25 in Q15)
```

With input `[4, 8, 12, 16, 20]`, the outputs will be:
- `y[0]` = 4 × 0.25 = 1
- `y[1]` = (4+8) × 0.25 = 3
- `y[2]` = (4+8+12) × 0.25 = 6
- `y[3]` = (4+8+12+16) × 0.25 = 10
- `y[4]` = (8+12+16+20) × 0.25 = 14

## Usage Flow

1. **Load coefficients**: Write to COEFF0-COEFF3 registers
2. **Write input data**: Write samples to DATA_IN[0] through DATA_IN[LEN-1]
3. **Set length**: Write number of samples to LEN register
4. **Start processing**: Write 0x3 to CTRL (EN=1, START=1)
5. **Wait for completion**: Poll STATUS.DONE or use interrupt (future weeks)
6. **Read results**: Read filtered samples from DATA_OUT[0] through DATA_OUT[LEN-1]
7. **Clear done flag**: Write 0x1 to STATUS to clear DONE bit

## Run Tests

Prerequisites:
- Activate shared Python environment: `source ../sim/.venv/bin/activate`
- Install Verilator: See `../README.md`

Run with Verilator (default):

```bash
make SIM=verilator
```

Run with Icarus Verilog:

```bash
make SIM=icarus
```

View waveforms (Verilator):

```bash
gtkwave sim_build/fir_filter_axil.fst
```

## Test Cases

1. **test_register_readback**: Basic AXI-Lite read/write of control registers
2. **test_w1p_signals**: Verify START and RESET are self-clearing (W1P)
3. **test_status_done_w1c**: Verify DONE flag clears on write-1 (W1C)
4. **test_simple_averaging_filter**: 4-tap averaging with ramp input
5. **test_single_tap_passthrough**: Single-tap passthrough (coeff[0]=1.0)
6. **test_data_array_boundary**: Verify array access at boundaries

## Files

- `rtl/fir_filter_axil.v` — FIR filter with AXI-Lite slave interface
- `tests/test_fir.py` — Cocotb testbench
- `Makefile` — Cocotb build configuration

## Next Steps

In Week 8, we'll write a Linux platform driver for this FIR filter and demonstrate it running in Renode with Verilator co-simulation.
