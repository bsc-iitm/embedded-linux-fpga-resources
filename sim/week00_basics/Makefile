TOPLEVEL_LANG ?= verilog

PWD := $(shell pwd)

VERILOG_SOURCES := $(PWD)/rtl/counter.v
TOPLEVEL := counter
MODULE := test_counter

SIM ?= verilator
WAVES ?= 1

ifeq ($(SIM),verilator)
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
	@echo "Runs counter demo with cocotb"

.PHONY: tb tb_clean
# Pure Verilog TB using Icarus Verilog (simple comparison)
tb:
	@echo "[iverilog] Building Verilog TB..."
	iverilog -g2012 -o tb.vvp tb/counter_tb.v rtl/counter.v
	@echo "[vvp] Running..."
	vvp tb.vvp
	@echo "VCD: counter_tb.vcd (open with gtkwave)"

tb_clean:
	rm -f tb.vvp counter_tb.vcd
