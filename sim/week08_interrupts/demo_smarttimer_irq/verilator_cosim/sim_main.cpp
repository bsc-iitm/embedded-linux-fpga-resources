// Smart Timer (IRQ) Verilator+Renode Integration via IntegrationLibrary

#include <verilated.h>
#include "Vsmarttimer_axil_irq_cosim.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#if VM_TRACE
# include <verilated_vcd_c.h>
#endif
#include "src/renode_bus.h"
#include "src/buses/axilite.h"

RenodeAgent *smarttimer = new RenodeAgent;
Vsmarttimer_axil_irq_cosim *top = new Vsmarttimer_axil_irq_cosim;
VerilatedVcdC *tfp;
vluint64_t main_time = 0;
static bool cosim_verbose = false;

static inline void host_print(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void eval() {
#if VM_TRACE
    main_time++;
    tfp->dump(main_time);
#endif
    top->eval();
    smarttimer->handleInterrupts();
}

void initAgent(RenodeAgent* agent) {
    // RenodeAgent *agent = new RenodeAgent();
    AxiLite* bus = new AxiLite();

    bus->clk = &top->ACLK;
    bus->rst = &top->ARESETn;

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

    bus->araddr  = (uint64_t *)&top->saxi_araddr;
    bus->arvalid = &top->saxi_arvalid;
    bus->arready = &top->saxi_arready;
    bus->rdata   = (uint64_t *)&top->saxi_rdata;
    bus->rresp   = &top->saxi_rresp;
    bus->rvalid  = &top->saxi_rvalid;
    bus->rready  = &top->saxi_rready;

    bus->evaluateModel = &eval;
    agent->addBus(bus);
    if(cosim_verbose) {
        agent->log(LOG_LEVEL_INFO, "smarttimer_irq_cosim: AXI-Lite wired (DATA=64, ADDR=32)");
    }
    agent->registerInterrupt(&top->irq_out, 0);
    // return agent;
}

RenodeAgent *Init() {
    cosim_verbose = (getenv("COSIM_VERBOSE") != nullptr);
    if(cosim_verbose) {
        host_print("[smarttimer_irq_cosim] Init() called (pid=%d)", getpid());
    }
    // RenodeAgent *agent = initAgent();
    smarttimer->connectNative();
    initAgent(smarttimer);
    // if(cosim_verbose) agent->log(LOG_LEVEL_INFO, "smarttimer_irq_cosim: connecting native");
    // agent->connectNative();
    if(cosim_verbose) smarttimer->log(LOG_LEVEL_INFO, "smarttimer_irq_cosim: Native connection established");
    // return agent;
    return smarttimer;
}

int main(int argc, char **argv, char **env) {
    if(argc < 3) {
        printf("Usage: %s {receiverPort} {senderPort} [{address}]\n", argv[0]);
        exit(-1);
    }
    const char *address = argc < 4 ? "127.0.0.1" : argv[3];

    Verilated::commandArgs(argc, argv);
#if VM_TRACE
    Verilated::traceEverOn(true);
    tfp = new VerilatedVcdC;
    top->trace(tfp, 99);
    tfp->open("smarttimer_irq_cosim.vcd");
#endif
    // RenodeAgent *agent = initAgent();
    // agent->connect(atoi(argv[1]), atoi(argv[2]), address);
    // agent->simulate();
    smarttimer->connect(atoi(argv[1]), atoi(argv[2]), address);
    initAgent(smarttimer);
    smarttimer->simulate();
    smarttimer->reset();
    top->final();
    exit(0);
}

