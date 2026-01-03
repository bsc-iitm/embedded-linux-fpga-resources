# Week 10 — DMA and High-Performance Data Transfer

This directory contains demonstrations of AXI DMA integration with the FIR filter peripheral.

## Contents

- `demo_fir_dma/` - FIR filter with AXI DMA for bulk data transfer

## Learning Focus

- Configure AXI DMA controller (MM2S and S2MM channels)
- Allocate DMA-coherent buffers in Linux driver
- Handle DMA completion interrupts
- Contrast data paths: MMIO (Week 7) vs IRQ (Week 9) vs DMA (Week 10)
- Understand cache coherency and physical vs virtual addresses

> Measurement note
> - Renode mixes virtual and cycle-accurate time; it’s not suitable for wall-clock performance comparisons between MMIO and DMA.
> - This week’s Renode demo is functional: it validates configuration, DMA transfers, and completion IRQs.
> - We will run the quantitative performance comparison on real hardware later and update these materials with measured numbers.

## Demo Overview

### FIR Filter with DMA

Replaces Week 7/9's MMIO vector mode with DMA:
- AXI DMA MM2S: memory → AXI Stream → FIR input
- FIR processing: AXI Stream in → AXI Stream out
- AXI DMA S2MM: FIR output → AXI Stream → memory
- Driver uses `dma_alloc_coherent()` for input/output buffers
- DMA completion IRQ wakes blocking read

Benefits over MMIO (expected on hardware):
- Lower CPU overhead during transfer and processing
- Scales better for large buffers (streaming-friendly)
- Higher throughput when bus/master and memory allow

## Prerequisites

- Week 9: Interrupt-driven FIR filter
- Week 7: FIR filter hardware and co-simulation basics
- Understanding of AXI Stream protocol

## Running the Demo

See `demo_fir_dma/README.md` for detailed instructions.
