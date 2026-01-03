TOPLEVEL_LANG ?= verilog

PWD := $(shell pwd)

VERILOG_SOURCES := $(PWD)/fir_filter_axil_irq.v
TOPLEVEL := fir_filter_axil_irq
MODULE := test_fir_irq

# Simulator selection: verilator (default) or icarus
SIM ?= verilator
WAVES ?= 1

# Extra flags for Verilator tracing
ifeq ($(SIM),verilator)
	export EXTRA_ARGS += --trace --trace-structs
endif

# Ensure Python can import tests/ module
export PYTHONPATH := $(PWD)/tests:$(PYTHONPATH)

# Resolve cocotb-config path; emit a clear error if not found
COCOTB_MAKEFILES := $(shell cocotb-config --makefiles 2>/dev/null)
ifeq ($(strip $(COCOTB_MAKEFILES)),)
$(error cocotb-config not found. Activate your env and install cocotb.)
endif
include $(COCOTB_MAKEFILES)/Makefile.sim

.PHONY: help
help:
	@echo "make [SIM=verilator|icarus] [WAVES=1]"
	@echo "Artifacts in sim_build/ (verilator) or dump.vcd (icarus)"

