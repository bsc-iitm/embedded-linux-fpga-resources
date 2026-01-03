// FIR (IRQ) Verilator+Renode Integration via IntegrationLibrary

#include <verilated.h>
#include "Vfir_filter_axil_irq_cosim.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#if VM_TRACE
# include <verilated_vcd_c.h>
#endif
#include "src/renode_bus.h"
#include "src/buses/axilite.h"

static Vfir_filter_axil_irq_cosim *top = new Vfir_filter_axil_irq_cosim;
static RenodeAgent *agent = new RenodeAgent;
#if VM_TRACE
static VerilatedVcdC *tfp;
#endif
static vluint64_t main_time = 0;
static bool cosim_verbose = false;

static inline void host_print(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void eval() {
#if VM_TRACE
    main_time++;
    tfp->dump(main_time);
#endif
    top->eval();
    agent->handleInterrupts();
}

static void initAgent(RenodeAgent *a) {
    AxiLite* bus = new AxiLite();

    // Clock/reset
    bus->clk = &top->ACLK;
    bus->rst = &top->ARESETn;   // active-low reset

    // Write address/data (64-bit data, 32-bit addr)
    bus->awaddr  = (uint64_t *)&top->saxi_awaddr;
    bus->awvalid = &top->saxi_awvalid;
    bus->awready = &top->saxi_awready;
    bus->wdata   = (uint64_t *)&top->saxi_wdata;
    bus->wstrb   = &top->saxi_wstrb;
    bus->wvalid  = &top->saxi_wvalid;
    bus->wready  = &top->saxi_wready;
    bus->bresp   = &top->saxi_bresp;
    bus->bvalid  = &top->saxi_bvalid;
    bus->bready  = &top->saxi_bready;

    // Read address/data
    bus->araddr  = (uint64_t *)&top->saxi_araddr;
    bus->arvalid = &top->saxi_arvalid;
    bus->arready = &top->saxi_arready;
    bus->rdata   = (uint64_t *)&top->saxi_rdata;
    bus->rresp   = &top->saxi_rresp;
    bus->rvalid  = &top->saxi_rvalid;
    bus->rready  = &top->saxi_rready;

    bus->evaluateModel = &eval;
    a->addBus(bus);
    if(cosim_verbose) {
        a->log(LOG_LEVEL_INFO, "fir_irq_cosim: AXI-Lite wired (DATA=64, ADDR=32)");
    }

    // Register irq_out as GPIO[0]
    a->registerInterrupt(&top->irq_out, 0);
}

RenodeAgent *Init() {
    cosim_verbose = (getenv("COSIM_VERBOSE") != nullptr);
    if(cosim_verbose) host_print("[fir_irq_cosim] Init() (pid=%d)", getpid());
    agent->connectNative();
    initAgent(agent);
    if(cosim_verbose) agent->log(LOG_LEVEL_INFO, "fir_irq_cosim: Native connection established");
    return agent;
}

int main(int argc, char **argv, char **env) {
    if(argc < 3) {
        printf("Usage: %s {receiverPort} {senderPort} [{address}]\n", argv[0]);
        return -1;
    }
    const char *address = argc < 4 ? "127.0.0.1" : argv[3];

    Verilated::commandArgs(argc, argv);
#if VM_TRACE
    Verilated::traceEverOn(true);
    tfp = new VerilatedVcdC;
    top->trace(tfp, 99);
    tfp->open("fir_irq_cosim.vcd");
#endif

    initAgent(agent);
    agent->connect(atoi(argv[1]), atoi(argv[2]), address);
    agent->simulate();
    top->final();
    return 0;
}
