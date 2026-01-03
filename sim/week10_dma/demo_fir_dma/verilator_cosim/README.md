# Verilator Co-Simulation Directory

## Purpose

Build Verilator shared library for FIR filter with AXI Stream interface, integrated with Renode via AXI Stream connections.

## Files Expected

1. `rtl/fir_stream_top.v` - Top-level wrapper with AXI-Lite config and AXI Stream data
2. `verilator_wrapper.cpp` - C++ integration layer for Renode
3. `Makefile` - Build Verilator library

## Top-Level Wrapper

### fir_stream_top.v

```verilog
module fir_stream_top (
    input  wire        clk,
    input  wire        resetn,

    // AXI-Lite (config registers: COEFF, LEN)
    input  wire [11:0] s_axil_awaddr,
    input  wire        s_axil_awvalid,
    output wire        s_axil_awready,
    input  wire [31:0] s_axil_wdata,
    input  wire [3:0]  s_axil_wstrb,
    input  wire        s_axil_wvalid,
    output wire        s_axil_wready,
    output wire [1:0]  s_axil_bresp,
    output wire        s_axil_bvalid,
    input  wire        s_axil_bready,
    input  wire [11:0] s_axil_araddr,
    input  wire        s_axil_arvalid,
    output wire        s_axil_arready,
    output wire [31:0] s_axil_rdata,
    output wire [1:0]  s_axil_rresp,
    output wire        s_axil_rvalid,
    input  wire        s_axil_rready,

    // AXI Stream Slave (input from DMA MM2S)
    input  wire        s_axis_tvalid,
    output wire        s_axis_tready,
    input  wire [15:0] s_axis_tdata,
    input  wire        s_axis_tlast,

    // AXI Stream Master (output to DMA S2MM)
    output wire        m_axis_tvalid,
    input  wire        m_axis_tready,
    output wire [15:0] m_axis_tdata,
    output wire        m_axis_tlast
);

// Instantiate FIR filter with AXI Stream
fir_stream u_fir (
    .clk(clk),
    .resetn(resetn),
    // AXI-Lite
    .s_axil_awaddr(s_axil_awaddr),
    // ... (full AXI-Lite signals)
    // AXI Stream
    .s_axis_tvalid(s_axis_tvalid),
    .s_axis_tready(s_axis_tready),
    .s_axis_tdata(s_axis_tdata),
    .s_axis_tlast(s_axis_tlast),
    .m_axis_tvalid(m_axis_tvalid),
    .m_axis_tready(m_axis_tready),
    .m_axis_tdata(m_axis_tdata),
    .m_axis_tlast(m_axis_tlast)
);

endmodule
```

## Verilator Wrapper

### verilator_wrapper.cpp

Renode expects specific C++ interface for Verilator integration:

```cpp
#include <verilated.h>
#include "Vfir_stream_top.h"

// Renode integration hooks
extern "C" {
    void* construct() {
        return new Vfir_stream_top;
    }

    void destruct(void* instance) {
        delete static_cast<Vfir_stream_top*>(instance);
    }

    void reset(void* instance) {
        Vfir_stream_top* top = static_cast<Vfir_stream_top*>(instance);
        top->resetn = 0;
        top->eval();
        top->resetn = 1;
    }

    void eval(void* instance) {
        static_cast<Vfir_stream_top*>(instance)->eval();
    }

    // AXI-Lite read/write callbacks (called by Renode)
    uint32_t axil_read(void* instance, uint32_t addr) {
        Vfir_stream_top* top = static_cast<Vfir_stream_top*>(instance);
        // Set up read transaction
        top->s_axil_arvalid = 1;
        top->s_axil_araddr = addr;
        top->s_axil_rready = 1;
        // Wait for handshake
        while (!top->s_axil_arready || !top->s_axil_rvalid) {
            top->clk = 1;
            top->eval();
            top->clk = 0;
            top->eval();
        }
        uint32_t data = top->s_axil_rdata;
        top->s_axil_arvalid = 0;
        return data;
    }

    void axil_write(void* instance, uint32_t addr, uint32_t data) {
        Vfir_stream_top* top = static_cast<Vfir_stream_top*>(instance);
        // Set up write transaction
        top->s_axil_awvalid = 1;
        top->s_axil_awaddr = addr;
        top->s_axil_wvalid = 1;
        top->s_axil_wdata = data;
        top->s_axil_wstrb = 0xF;
        top->s_axil_bready = 1;
        // Wait for handshake
        while (!top->s_axil_awready || !top->s_axil_wready) {
            top->clk = 1;
            top->eval();
            top->clk = 0;
            top->eval();
        }
        // Wait for response
        while (!top->s_axil_bvalid) {
            top->clk = 1;
            top->eval();
            top->clk = 0;
            top->eval();
        }
        top->s_axil_awvalid = 0;
        top->s_axil_wvalid = 0;
    }

    // AXI Stream callbacks (called by Renode connector)
    int axis_slave_ready(void* instance) {
        Vfir_stream_top* top = static_cast<Vfir_stream_top*>(instance);
        return top->s_axis_tready;
    }

    void axis_slave_write(void* instance, uint16_t data, int last) {
        Vfir_stream_top* top = static_cast<Vfir_stream_top*>(instance);
        top->s_axis_tvalid = 1;
        top->s_axis_tdata = data;
        top->s_axis_tlast = last;
        // Cycle clock until handshake
        while (!top->s_axis_tready) {
            top->clk = 1;
            top->eval();
            top->clk = 0;
            top->eval();
        }
        top->clk = 1;
        top->eval();
        top->clk = 0;
        top->eval();
        top->s_axis_tvalid = 0;
    }

    int axis_master_valid(void* instance) {
        Vfir_stream_top* top = static_cast<Vfir_stream_top*>(instance);
        return top->m_axis_tvalid;
    }

    uint16_t axis_master_read(void* instance, int* last) {
        Vfir_stream_top* top = static_cast<Vfir_stream_top*>(instance);
        uint16_t data = top->m_axis_tdata;
        *last = top->m_axis_tlast;
        top->m_axis_tready = 1;
        // Cycle clock for handshake
        top->clk = 1;
        top->eval();
        top->clk = 0;
        top->eval();
        top->m_axis_tready = 0;
        return data;
    }
}
```

## Makefile

```makefile
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
	rm -rf obj_dir libVfir_stream_top.so

.PHONY: all clean
```

## Build

```bash
make
# Produces libVfir_stream_top.so for Renode
```

## Integration with Renode

Renode loads the `.so` and calls C++ interface:

1. **MMIO (AXI-Lite)**: Linux driver writes COEFF/LEN via `axil_write()`
2. **DMA MM2S → FIR**: DMA calls `axis_slave_write()` for each sample
3. **FIR → DMA S2MM**: DMA polls `axis_master_valid()` and calls `axis_master_read()`

All AXI Stream flow control handled by Renode connector.

## Key Differences from Week 9

| Aspect              | Week 9                     | Week 10                          |
|---------------------|----------------------------|----------------------------------|
| Data interface      | AXI-Lite DATA_IN/DATA_OUT  | AXI Stream slave/master          |
| Verilator wrapper   | Only AXI-Lite              | AXI-Lite + AXI Stream callbacks  |
| Renode connection   | Single MMIO peripheral     | MMIO + stream connectors         |
| Testing             | CPU read/write             | DMA drives AXI Stream            |

## Testing in Renode

After loading `.repl` and starting:

```
(machine-0) fir_filter LogLevel 3
# Enable debug output for AXI Stream transactions

# In Linux guest, run driver test
# Renode logs should show:
#   - DMA MM2S writing to FIR s_axis
#   - FIR processing
#   - FIR m_axis writing to DMA S2MM
#   - S2MM completion IRQ
```

## VCD Trace (Optional)

Build with tracing:

```makefile
VFLAGS += --trace --trace-structs
```

Then in Renode:

```
(machine-0) fir_filter DumpWaveforms "/tmp/fir_dma.vcd"
```

Analyze with GTKWave:
- AXI-Lite transactions (config writes)
- AXI Stream handshakes (TVALID/TREADY/TDATA/TLAST)
- FIR core processing state

## References

- Renode Verilator integration: https://renode.readthedocs.io/en/latest/advanced/verilator.html
- Week 9 Verilator wrapper (MMIO only)
- AXI Stream protocol (IHI0051A)
