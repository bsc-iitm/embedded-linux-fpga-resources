# Verilator build for bare-metal Smart Timer test

VERILATOR = verilator
VERILATOR_FLAGS = --cc --exe --build --trace-fst -Wall
VERILOG_SOURCES = rtl/pwm_core.v rtl/smart_timer_axil.v
CPP_SOURCES = test_baremetal.cpp
TOP_MODULE = smart_timer_axil

.PHONY: all clean run

all: obj_dir/V$(TOP_MODULE)

obj_dir/V$(TOP_MODULE): $(VERILOG_SOURCES) $(CPP_SOURCES)
	$(VERILATOR) $(VERILATOR_FLAGS) --top-module $(TOP_MODULE) \
		$(VERILOG_SOURCES) $(CPP_SOURCES)

run: obj_dir/V$(TOP_MODULE)
	@echo "Running Smart Timer bare-metal test..."
	./obj_dir/V$(TOP_MODULE)

clean:
	rm -rf obj_dir *.fst *.vcd

help:
	@echo "Targets:"
	@echo "  make       - Build Verilator executable"
	@echo "  make run   - Build and run test"
	@echo "  make clean - Remove generated files"
	@echo ""
	@echo "Output:"
	@echo "  - Console log showing register operations"
	@echo "  - demo1_baremetal.fst (waveform, view with gtkwave)"
