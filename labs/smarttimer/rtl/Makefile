# Makefile for Smart Timer IRQ RTL tests

SIM ?= verilator
TOPLEVEL_LANG ?= verilog

VERILOG_SOURCES = $(PWD)/smarttimer_axil_irq.v $(PWD)/pwm_core.v
TOPLEVEL = smarttimer_axil_irq
MODULE = tests.test_smarttimer_irq

include $(shell cocotb-config --makefiles)/Makefile.sim

.PHONY: clean test view

test:
	$(MAKE) sim SIM=$(SIM)

view:
	gtkwave sim_build/*.fst &

clean::
	rm -rf sim_build __pycache__ tests/__pycache__ *.fst *.vcd results.xml
