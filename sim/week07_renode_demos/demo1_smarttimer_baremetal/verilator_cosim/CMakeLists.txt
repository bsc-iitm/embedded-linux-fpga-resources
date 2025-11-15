cmake_minimum_required(VERSION 3.8)

# Name of the project
project(smart_timer_cosim)

# Verilog files to be Verilated (top module first, then dependencies)
set(VTOP
    rtl/smart_timer_axil_cosim.v
    rtl/smart_timer_axil.v
    rtl/pwm_core.v
)

# Top module name
set(TOP_MODULE smart_timer_axil_cosim)

# C/C++ source files to be compiled
set(CSOURCES sim_main.cpp)

# Additional compiling arguments
set(COMP_LIB_ARGS -fPIC)

# Additional verilating arguments
set(VERI_LIB_ARGS -Wno-WIDTH)

# CMake file doing the hard job
include(../../../configure-and-verilate.cmake)
