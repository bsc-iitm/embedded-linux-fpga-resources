cmake_minimum_required(VERSION 3.8)

project(fir_filter_cosim)

# Verilog sources (wrapper first, then core)
set(VTOP
    rtl/fir_filter_axil_cosim.v
    ../../../week07_fir_verilator/rtl/fir_filter_axil.v
)

set(TOP_MODULE fir_filter_axil_cosim)

set(CSOURCES sim_main.cpp)

set(COMP_LIB_ARGS -fPIC)
set(VERI_LIB_ARGS -Wno-WIDTH)

include(../../../configure-and-verilate.cmake)

