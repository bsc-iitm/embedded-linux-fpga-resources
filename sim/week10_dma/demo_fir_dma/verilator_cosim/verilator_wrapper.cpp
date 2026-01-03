// Verilator wrapper for Renode Integration Library
// Exposes AXI-Lite and AXI-Stream interfaces of fir_stream_top

#include "Vfir_stream_top.h"
#include "verilated.h"

extern "C" {
    void* create_model() {
        Verilated::traceEverOn(false);
        auto* top = new Vfir_stream_top();
        top->clk = 0;
        top->rst_n = 0;
        for (int i = 0; i < 4; ++i) { top->eval(); }
        top->rst_n = 1;
        return top;
    }

    void delete_model(void* instance) {
        auto* top = static_cast<Vfir_stream_top*>(instance);
        delete top;
    }

    void eval_model(void* instance) {
        auto* top = static_cast<Vfir_stream_top*>(instance);
        top->clk = 1; top->eval();
        top->clk = 0; top->eval();
    }

    // AXI-Lite 32-bit helpers
    uint32_t axil_read(void* instance, uint32_t addr) {
        auto* top = static_cast<Vfir_stream_top*>(instance);
        top->s_axil_arvalid = 1;
        top->s_axil_araddr = addr;
        while (!top->s_axil_arready) { eval_model(instance); }
        top->s_axil_arvalid = 0;
        while (!top->s_axil_rvalid)   { eval_model(instance); }
        uint32_t data = top->s_axil_rdata;
        top->s_axil_rready = 1; eval_model(instance); top->s_axil_rready = 0;
        return data;
    }

    void axil_write(void* instance, uint32_t addr, uint32_t data) {
        auto* top = static_cast<Vfir_stream_top*>(instance);
        top->s_axil_awvalid = 1; top->s_axil_awaddr = addr;
        top->s_axil_wvalid  = 1; top->s_axil_wdata  = data; top->s_axil_wstrb = 0xF;
        while (!top->s_axil_awready || !top->s_axil_wready) { eval_model(instance); }
        top->s_axil_awvalid = 0; top->s_axil_wvalid = 0;
        while (!top->s_axil_bvalid) { eval_model(instance); }
        top->s_axil_bready = 1; eval_model(instance); top->s_axil_bready = 0;
    }

    // AXI-Stream slave (input)
    int axis_slave_ready(void* instance) {
        auto* top = static_cast<Vfir_stream_top*>(instance);
        return top->s_axis_tready;
    }

    void axis_slave_write(void* instance, uint16_t data, int last) {
        auto* top = static_cast<Vfir_stream_top*>(instance);
        top->s_axis_tvalid = 1;
        top->s_axis_tdata = data;
        top->s_axis_tlast = last ? 1 : 0;
        while (!top->s_axis_tready) { eval_model(instance); }
        eval_model(instance);
        top->s_axis_tvalid = 0;
    }

    // AXI-Stream master (output)
    int axis_master_valid(void* instance) {
        auto* top = static_cast<Vfir_stream_top*>(instance);
        return top->m_axis_tvalid;
    }

    uint16_t axis_master_read(void* instance, int* last) {
        auto* top = static_cast<Vfir_stream_top*>(instance);
        uint16_t data = top->m_axis_tdata;
        *last = top->m_axis_tlast;
        top->m_axis_tready = 1; eval_model(instance); top->m_axis_tready = 0;
        return data;
    }
}

