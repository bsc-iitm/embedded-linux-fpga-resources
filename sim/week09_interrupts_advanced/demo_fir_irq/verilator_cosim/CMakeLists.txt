cmake_minimum_required(VERSION 3.8)

project(fir_filter_irq_cosim)

set(VTOP
    rtl/fir_filter_axil_irq_cosim.v
    ../rtl/fir_filter_axil_irq.v
)

set(TOP_MODULE fir_filter_axil_irq_cosim)

set(CSOURCES sim_main.cpp)

set(COMP_LIB_ARGS -fPIC)
set(VERI_LIB_ARGS -Wno-WIDTH)

include(../../../configure-and-verilate.cmake)

