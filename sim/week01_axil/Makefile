TOPLEVEL_LANG ?= verilog

PWD := $(shell pwd)

VERILOG_SOURCES := $(PWD)/rtl/axil_regbank.v
TOPLEVEL := axil_regbank
MODULE := test_axil

# Simulator selection: verilator (default) or icarus
SIM ?= verilator
WAVES ?= 1

# Extra flags for Verilator tracing
ifeq ($(SIM),verilator)
#   export VERILATOR_TRACE ?= 1
	export EXTRA_ARGS += --trace --trace-structs
endif

# Ensure Python can import tests/ module
export PYTHONPATH := $(PWD)/tests:$(PYTHONPATH)

# Resolve cocotb-config path; emit a clear error if not found
COCOTB_MAKEFILES := $(shell cocotb-config --makefiles 2>/dev/null)
ifeq ($(strip $(COCOTB_MAKEFILES)),)
$(error cocotb-config not found. Activate env: `source ../.venv/bin/activate` or install cocotb in your Python env)
endif
include $(COCOTB_MAKEFILES)/Makefile.sim

.PHONY: help
help:
	@echo "make [SIM=verilator|icarus] [WAVES=1]"
	@echo "Artifacts (waves): sim_build/ or dump.vcd depending on sim"
