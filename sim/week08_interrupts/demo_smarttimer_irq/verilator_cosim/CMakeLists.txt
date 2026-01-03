cmake_minimum_required(VERSION 3.8)

project(smarttimer_irq_cosim)

# Verilog sources: top wrapper first, then core(s)
set(VTOP
    rtl/smarttimer_axil_irq_cosim.v
    ../rtl/smarttimer_axil_irq.v
    ../rtl/pwm_core.v
)

set(TOP_MODULE smarttimer_axil_irq_cosim)

set(CSOURCES sim_main.cpp)

set(COMP_LIB_ARGS -fPIC)
set(VERI_LIB_ARGS -Wno-WIDTH)

# Use the same IntegrationLibrary-based flow as Week 7
include(../../../configure-and-verilate.cmake)
