# Verilator Co-Simulation Library - FIR Filter with IRQ

## Build Instructions

Similar to Week 7, but with IRQ-enabled RTL.

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(fir_cosim)

find_package(verilator REQUIRED)

set(RTL_SOURCES
    ${CMAKE_SOURCE_DIR}/../rtl/fir_q15_axil_irq.v
    ${CMAKE_SOURCE_DIR}/../rtl/fir_q15_core.v
)

verilate(Vfir
    SOURCES ${RTL_SOURCES}
    VERILATOR_ARGS --trace --trace-structs -Wall
    TOP_MODULE fir_q15_axil_irq
    PREFIX Vfir
)

add_library(Vfir SHARED
    ${VERILATOR_ROOT}/include/verilated.cpp
    ${VERILATOR_ROOT}/include/verilated_vcd_c.cpp
)

target_include_directories(Vfir PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${VERILATOR_ROOT}/include
)
```

## Build

```bash
mkdir build && cd build
cmake ..
make
# Produces: libVfir.so
```

## GPIO Signal Mapping

```
GPIO[0] = irq_out  // IRQ output to GIC
```

Renode wiring:
```
0 -> gic@66
```

## VCD Signals

Key signals to watch in GTKWave:
- AXI-Lite transactions (awaddr, wdata, araddr, rdata)
- FIR core: `state`, `tap_index`, `sample_index`
- Interrupt: `processing_complete`, `status_done`, `irq_out`
- STATUS register: read/write transactions, W1C behavior
# Verilator Co-Sim

- Wrapper widens AXI-Lite data to 64-bit and forwards `irq_out` as GPIO[0].
- Build produces `build/libVtop.so` for Renode to load.
