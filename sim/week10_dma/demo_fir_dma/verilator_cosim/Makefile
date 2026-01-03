VERILATOR = verilator
VFLAGS = --cc --exe --build -j 0 --shared --Mdir obj_dir
VFLAGS += -CFLAGS "-fPIC" -LDFLAGS "-shared"

RTL_DIR = ../rtl
SRC = $(RTL_DIR)/fir_stream_top.v $(RTL_DIR)/fir_stream.v $(RTL_DIR)/fir_q15_core.v

all: libVfir_stream_top.so

libVfir_stream_top.so: $(SRC) verilator_wrapper.cpp
	$(VERILATOR) $(VFLAGS) \
		--top-module fir_stream_top \
		-o ../libVfir_stream_top.so \
		$(SRC) verilator_wrapper.cpp

clean:
	rm -rf obj_dir ../libVfir_stream_top.so

.PHONY: all clean

