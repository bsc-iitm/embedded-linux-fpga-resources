---
marp: true
paginate: true
title: Course Introduction - Embedded Linux and FPGAs
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

<!--
_class: lead
-->

# Course Introduction

## Embedded Linux and FPGAs

Nitin Chandrachoodan, IIT Madras

---

## Goals

- Understand memory-mapped I/O and AXI-Lite register interfaces
- Learn essential Linux kernel and driver concepts
- Map FPGA hardware into Linux via Device Tree
- Build complete hardware-software integration with interrupts

---

## Pre-requisites

- Embedded **C programming**: 
  - pointers, device registers, configuring peripherals
  - memory mapped IO
- Digital System Design with **Verilog**:
  - Basic CPU architecture; instruction set
  - Combinational and Sequential Circuits
  - Finite State Machines

---

## Why?

- Modern digital systems 
  - too complex for pure hardware implementation
  - very high performance: not possible in pure software
- Hardware / Software interface
  - Custom peripherals to perform computations: **hardware**
  - General control logic: **software**

---

## How?

- Bus based architectures
  - AXI-Lite, AXI, Wishbone
- Memory Mapped IO
- Device Drivers and Operating System interface

---

## Course Outline

- The Hardware Interface
  - Registers, AXI-Lite bus, Memory Mapped IO
- The Software Interface
  - Core Linux concepts, kernel modules, device tree, drivers, performance

---

## What is NOT covered

- Advanced drivers, buses, PCI
- High performance considerations
  - Only Overview of DMA, interrupts etc.
- Deep dive OS concepts
  - Booting
  - Resource and Memory management

The course will focus on concepts at a fairly high level: you will need to do a proper OS course to supplement the knowledge here.

---

## Reference Material

<!-- _class: small-text -->

### Books
- "Linux Device Drivers, 3rd Edition" by J. Corbet, A. Rubini, and G. Kroah-Hartman (O’Reilly). *Old but good reference*
- [Exploring Zynq MPSoC](https://www.zynq-mpsoc-book.com/) with Pynq and Machine Learning Applications, Xilinx and Univ. of Strathclyde (free download)

### Online
- [Linux Kernel Docs](https://docs.kernel.org/)
- [Xilinx Wiki](https://xilinx-wiki.atlassian.net/wiki/spaces/A/overview)

