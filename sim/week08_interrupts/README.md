# Week 8 Demo: Smart Timer with Interrupt Support

## Overview

This demo shows end-to-end interrupt flow from hardware RTL through Renode GIC to Linux driver handler.

**Key Components**:
1. Smart Timer RTL with IRQ output (asserts on counter wrap)
2. Device tree with interrupt specification
3. Renode platform with GIC wiring
4. Minimal Linux driver with IRQ counter

**Learning Goals**:
- Understand hardware interrupt assertion/acknowledgment
- Configure device tree interrupt bindings
- Wire interrupts through Renode GIC
- Write simple IRQ handler
- Debug interrupt routing issues

## Directory Structure

```
sim/week08_interrupts/
├── README.md                           # This file
└── demo_smarttimer_irq/
    ├── rtl/
    │   ├── smarttimer_axil_irq.v       # Smart Timer with irq_out port
    │   ├── Makefile                     # Build and test
    │   └── tests/
    │       ├── test_smarttimer_irq.py   # Cocotb tests for IRQ behavior
    │       └── Makefile                 # Test runner
    ├── verilator_cosim/
    │   ├── wrapper.cpp                  # Verilator wrapper for Renode
    │   ├── CMakeLists.txt               # Build libVsmartimer.so
    │   └── README.md                    # Build instructions
    ├── driver/
    │   ├── smarttimer_irq_simple.c      # Linux platform driver
    │   ├── Makefile                     # Cross-compile for ARM
    │   └── README.md                    # Usage instructions
    └── renode/
        ├── smarttimer_irq.dts           # Device tree with interrupt
        ├── smarttimer_irq.repl          # Platform description with GIC
        ├── demo_irq.resc                # Renode startup script
        └── README.md                    # How to run

```

## RTL Design: smarttimer_axil_irq.v

### Changes from Week 6 Smart Timer

**New port**:
```verilog
output wire irq_out  // Interrupt output (level-triggered, active high)
```

**New register bit**: STATUS[0] = WRAP
- Set by hardware when counter wraps (counter == period)
- Cleared by software writing 1 (W1C)
- Drives irq_out signal

### Register Map

```
Offset  Register  Access  Bits  Description
------  --------  ------  ----  -----------
0x000   CTRL      RW      [0]   EN: Enable timer
                          [1]   RST: Reset timer (W1P, self-clearing)
0x004   STATUS    RO/W1C  [0]   WRAP: Wrap occurred (W1C)
0x008   PERIOD    RW      [31:0] Timer period (clock cycles)
0x00C   DUTY      RW      [31:0] Duty cycle (clock cycles)
```

### Interrupt Behavior

**Assert IRQ when**:
1. Timer is enabled (CTRL.EN = 1)
2. Counter reaches PERIOD value
3. Counter wraps to 0
4. STATUS.WRAP is set

**De-assert IRQ when**:
- Software writes 1 to STATUS.WRAP (W1C operation)

**Type**: Level-triggered, active high
- IRQ stays high until acknowledged
- If WRAP bit is not cleared, IRQ remains asserted

### RTL Implementation Outline

```verilog
module smarttimer_axil_irq (
    input  wire        clk,
    input  wire        resetn,

    // AXI-Lite slave interface
    input  wire [31:0] s_axi_awaddr,
    input  wire        s_axi_awvalid,
    output wire        s_axi_awready,

    input  wire [31:0] s_axi_wdata,
    input  wire [3:0]  s_axi_wstrb,
    input  wire        s_axi_wvalid,
    output wire        s_axi_wready,

    output wire [1:0]  s_axi_bresp,
    output wire        s_axi_bvalid,
    input  wire        s_axi_bready,

    input  wire [31:0] s_axi_araddr,
    input  wire        s_axi_arvalid,
    output wire        s_axi_arready,

    output wire [31:0] s_axi_rdata,
    output wire [1:0]  s_axi_rresp,
    output wire        s_axi_rvalid,
    input  wire        s_axi_rready,

    // PWM output
    output wire        pwm_out,

    // Interrupt output (NEW)
    output wire        irq_out
);

// Register addresses
localparam ADDR_CTRL   = 4'h0;
localparam ADDR_STATUS = 4'h4;
localparam ADDR_PERIOD = 4'h8;
localparam ADDR_DUTY   = 4'hC;

// Registers
reg        ctrl_en;
reg [31:0] period;
reg [31:0] duty;
reg        status_wrap;  // NEW: Sticky wrap flag

// Timer counter
reg [31:0] counter;

// Wrap event detection
wire wrap_event = ctrl_en && (counter == period) && (period != 0);

// Timer counter logic
always @(posedge clk) begin
    if (!resetn || !ctrl_en) begin
        counter <= 32'd0;
    end else begin
        if (counter >= period)
            counter <= 32'd0;
        else
            counter <= counter + 1;
    end
end

// PWM output
assign pwm_out = ctrl_en && (counter < duty);

// STATUS.WRAP logic (sticky, W1C)
wire status_write = /* AXI write to STATUS */;
wire [31:0] wdata = /* AXI write data */;

always @(posedge clk) begin
    if (!resetn) begin
        status_wrap <= 1'b0;
    end else begin
        if (wrap_event) begin
            status_wrap <= 1'b1;  // Set on wrap
        end else if (status_write && wdata[0]) begin
            status_wrap <= 1'b0;  // Clear on W1C
        end
    end
end

// Interrupt output (level-triggered)
assign irq_out = status_wrap;

// AXI-Lite read logic
always @(*) begin
    case (araddr[3:0])
        ADDR_CTRL:   rdata = {31'd0, ctrl_en};
        ADDR_STATUS: rdata = {31'd0, status_wrap};
        ADDR_PERIOD: rdata = period;
        ADDR_DUTY:   rdata = duty;
        default:     rdata = 32'd0;
    endcase
end

// AXI-Lite write logic
// (Handle CTRL.EN, CTRL.RST, PERIOD, DUTY, STATUS W1C)
// ... (omitted for brevity, see full RTL)

endmodule
```

### Testbench Verification (Cocotb)

**Test cases** (`tests/test_smarttimer_irq.py`):

1. **test_irq_asserts_on_wrap**:
   - Configure timer with short period (e.g., 10 cycles)
   - Enable timer
   - Wait for wrap event
   - Verify `irq_out` goes high
   - Verify STATUS.WRAP = 1

2. **test_irq_clears_on_w1c**:
   - Trigger wrap event (IRQ high)
   - Write 1 to STATUS.WRAP
   - Verify `irq_out` goes low
   - Verify STATUS.WRAP = 0

3. **test_irq_not_asserted_when_disabled**:
   - Configure period
   - Leave timer disabled (CTRL.EN = 0)
   - Verify `irq_out` stays low even after period cycles

4. **test_multiple_wraps**:
   - Enable timer with short period
   - Let it wrap multiple times
   - Clear WRAP after each wrap
   - Verify IRQ can fire repeatedly

## Device Tree: smarttimer_irq.dts

```dts
/dts-v1/;

/ {
    #address-cells = <1>;
    #size-cells = <1>;
    compatible = "arm,vexpress";
    model = "ARM Versatile Express with Smart Timer IRQ";

    chosen {
        bootargs = "console=ttyAMA0,115200";
        linux,initrd-start = <0x82000000>;
        linux,initrd-end = <0x83000000>;
    };

    memory@80000000 {
        device_type = "memory";
        reg = <0x80000000 0x08000000>;  // 128 MB
    };

    soc {
        #address-cells = <1>;
        #size-cells = <1>;
        compatible = "simple-bus";
        ranges;
        interrupt-parent = <&gic>;

        gic: interrupt-controller@8000000 {
            compatible = "arm,cortex-a9-gic";
            #interrupt-cells = <3>;
            interrupt-controller;
            reg = <0x08000000 0x1000>,  // Distributor
                  <0x08001000 0x1000>;  // CPU interface
        };

        uart0: uart@9000000 {
            compatible = "arm,pl011", "arm,primecell";
            reg = <0x09000000 0x1000>;
            interrupts = <0 5 4>;  // SPI 5, level high
        };

        smarttimer: smarttimer@70000000 {
            compatible = "acme,smarttimer-irq-v1";
            reg = <0x70000000 0x1000>;
            interrupts = <0 33 4>;  // SPI 33, level high (GIC ID 65)
        };
    };
};
```

**Key points**:
- `interrupts = <0 33 4>`:
  - Cell 0: `0` = GIC_SPI (shared peripheral interrupt)
  - Cell 1: `33` = Interrupt number (maps to GIC HW ID 65 = 32 + 33)
  - Cell 2: `4` = IRQ_TYPE_LEVEL_HIGH
- `interrupt-parent = <&gic>`: Inherited from soc node
- `compatible = "acme,smarttimer-irq-v1"`: Updated for IRQ version

**Compilation**:
```bash
dtc -I dts -O dtb -o smarttimer_irq.dtb smarttimer_irq.dts
```

## Renode Platform: smarttimer_irq.repl

```
using "platforms/cpus/cortex-a9.repl"

gic: IRQControllers.ARM_GenericInterruptController @ sysbus 0x8000000
    architectureVersion: IRQControllers.ARM_GenericInterruptControllerVersion.GICv1

uart0: UART.PL011 @ sysbus 0x9000000
    -> gic@37  // SPI 5 → GIC ID 37

smarttimer: Verilated.VerilatedPeripheral @ sysbus 0x70000000
    frequency: 50000000      // 50 MHz peripheral clock
    limitBuffer: 100000       // Verilator cycle buffer
    timeout: 10000            // Transaction timeout (ms)
    address: "libVsmartimer.so"
    -> gic@65                 // SPI 33 → GIC ID 65 (32 + 33)

cpu: CPU.ARMv7A @ sysbus
    cpuType: "cortex-a9"
    genericInterruptController: gic

memory: Memory.MappedMemory @ sysbus 0x80000000
    size: 0x08000000  // 128 MB
```

**Key wiring**:
- `smarttimer -> gic@65`: Routes Smart Timer IRQ output to GIC input 65
- GIC ID 65 = SPI base (32) + interrupt number (33)
- Must match device tree `interrupts` property

## Renode Script: demo_irq.resc

```
# Create machine
mach create "smarttimer-irq-demo"

# Load platform description
machine LoadPlatformDescription @smarttimer_irq.repl

# Load kernel
sysbus LoadELF @zImage

# Load device tree
sysbus LoadFdt @smarttimer_irq.dtb 0x82000000

# Load initramfs (with driver .ko file)
sysbus LoadBinary @initramfs.cpio.gz 0x82000000

# Configure UART
showAnalyzer uart0

# Optional: Enable logging
logLevel 3 gic
logLevel 3 smarttimer

# Start emulation
start
```

## Linux Driver: smarttimer_irq_simple.c

### Driver Architecture

**Goal**: Minimal driver to prove IRQ flow works

**Features**:
1. Platform driver binding via device tree
2. Request IRQ in probe using `devm_request_irq()`
3. Simple IRQ handler:
   - Increment atomic counter
   - Print rate-limited message
   - Clear STATUS.WRAP (W1C)
4. Sysfs attributes:
   - `irq_count` (RO): Read IRQ counter
   - `enable` (WO): Enable/disable timer
   - `period` (WO): Set timer period

### Key Code Sections

**Device structure**:
```c
struct smarttimer_dev {
    struct device *dev;
    void __iomem *base;
    int irq;
    atomic_t irq_count;
};
```

**IRQ handler**:
```c
static irqreturn_t smarttimer_irq_handler(int irq, void *dev_id)
{
    struct smarttimer_dev *stdev = dev_id;
    u32 status;

    // Read STATUS register
    status = readl(stdev->base + STATUS_OFFSET);

    // Check if our interrupt (WRAP bit set)
    if (status & STATUS_WRAP_BIT) {
        // Increment counter (atomic, safe in IRQ context)
        atomic_inc(&stdev->irq_count);

        // Print message (rate-limited to avoid spam)
        dev_info_ratelimited(stdev->dev, "Timer wrap IRQ #%d\n",
                            atomic_read(&stdev->irq_count));

        // CRITICAL: Acknowledge interrupt (W1C)
        // Without this, IRQ will fire again immediately
        writel(STATUS_WRAP_BIT, stdev->base + STATUS_OFFSET);

        return IRQ_HANDLED;  // We handled this interrupt
    }

    // Not our interrupt (shouldn't happen, but be safe)
    return IRQ_NONE;
}
```

**Probe function**:
```c
static int smarttimer_probe(struct platform_device *pdev)
{
    struct smarttimer_dev *stdev;
    struct resource *res;
    int ret;

    // Allocate device structure
    stdev = devm_kzalloc(&pdev->dev, sizeof(*stdev), GFP_KERNEL);
    if (!stdev)
        return -ENOMEM;

    stdev->dev = &pdev->dev;

    // Map MMIO region
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    stdev->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(stdev->base))
        return PTR_ERR(stdev->base);

    // Get IRQ number from device tree
    stdev->irq = platform_get_irq(pdev, 0);
    if (stdev->irq < 0) {
        dev_err(&pdev->dev, "Failed to get IRQ\n");
        return stdev->irq;
    }

    // Initialize counter
    atomic_set(&stdev->irq_count, 0);

    // Request IRQ with devm (auto cleanup on remove)
    ret = devm_request_irq(&pdev->dev, stdev->irq,
                          smarttimer_irq_handler,
                          IRQF_SHARED,  // Allow IRQ line sharing
                          dev_name(&pdev->dev),
                          stdev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to request IRQ %d: %d\n",
                stdev->irq, ret);
        return ret;
    }

    // Save driver data for sysfs callbacks
    platform_set_drvdata(pdev, stdev);

    dev_info(&pdev->dev, "Probed at 0x%lx, IRQ %d\n",
             (unsigned long)res->start, stdev->irq);

    return 0;
}
```

**Sysfs attributes**:
```c
// Read IRQ count
static ssize_t irq_count_show(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *stdev = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", atomic_read(&stdev->irq_count));
}
static DEVICE_ATTR_RO(irq_count);

// Enable/disable timer
static ssize_t enable_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    struct smarttimer_dev *stdev = dev_get_drvdata(dev);
    unsigned long val;

    if (kstrtoul(buf, 0, &val))
        return -EINVAL;

    writel(val ? 0x1 : 0x0, stdev->base + CTRL_OFFSET);
    return count;
}
static DEVICE_ATTR_WO(enable);

// Set period
static ssize_t period_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    struct smarttimer_dev *stdev = dev_get_drvdata(dev);
    unsigned long val;

    if (kstrtoul(buf, 0, &val))
        return -EINVAL;

    writel(val, stdev->base + PERIOD_OFFSET);
    return count;
}
static DEVICE_ATTR_WO(period);
```

**Build**: See `driver/Makefile` for cross-compilation commands.

## Demo Test Procedure

### Step 1: Build RTL and Validate

```bash
cd sim/week08_interrupts/demo_smarttimer_irq/rtl
make test

# Expected output:
# - All Cocotb tests pass
# - VCD waveform shows irq_out assertions
# - STATUS.WRAP behavior verified
```

### Step 2: Build Verilator Co-Sim Library

```bash
cd ../verilator_cosim
mkdir build && cd build
cmake ..
make

# Produces: libVsmartimer.so
```

### Step 3: Compile Device Tree

```bash
cd ../../renode
dtc -I dts -O dtb -o smarttimer_irq.dtb smarttimer_irq.dts

# Verify:
fdtdump smarttimer_irq.dtb | grep -A5 smarttimer
```

### Step 4: Build Linux Driver

```bash
cd ../driver
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

# Produces: smarttimer_irq_simple.ko
# Copy to initramfs or mount via 9p
```

### Step 5: Run Renode

```bash
cd ../renode
renode demo_irq.resc

# Renode monitor will start
# UART analyzer window will open
```

### Step 6: Linux Guest Testing

In Renode UART console:

```bash
# Load driver module
insmod /root/smarttimer_irq_simple.ko

# Check probe message
dmesg | tail -n 5
# Expected: "smarttimer-irq smarttimer: Probed at 0x70000000, IRQ 65"

# Set timer period (50M cycles = 1 sec at 50 MHz)
echo 50000000 > /sys/devices/platform/smarttimer/period

# Enable timer
echo 1 > /sys/devices/platform/smarttimer/enable

# Check IRQ count (should start at 0)
cat /sys/devices/platform/smarttimer/irq_count

# Wait a few seconds, check again (should increment)
sleep 3
cat /sys/devices/platform/smarttimer/irq_count

# Watch it increase in real-time
watch -n 1 'cat /sys/devices/platform/smarttimer/irq_count'

# Check kernel log (rate-limited messages)
dmesg | grep "Timer wrap IRQ"

# Disable timer
echo 0 > /sys/devices/platform/smarttimer/enable

# Verify count stops increasing
cat /sys/devices/platform/smarttimer/irq_count
sleep 3
cat /sys/devices/platform/smarttimer/irq_count
# (should be same value)

# Re-enable and count continues
echo 1 > /sys/devices/platform/smarttimer/enable
sleep 2
cat /sys/devices/platform/smarttimer/irq_count
# (should have increased)
```

### Expected Results

**Successful demo shows**:
1. Driver probes successfully and finds IRQ 65
2. IRQ count increments roughly once per second (period = 50M cycles)
3. Disabling timer stops IRQ count from increasing (no spurious interrupts)
4. Re-enabling timer resumes IRQ count
5. `dmesg` shows IRQ handler messages (rate-limited)

**Key validation**:
- End-to-end IRQ flow: RTL → Renode GIC → Linux → Driver
- Hardware event (wrap) causes software action (counter increment)
- Acknowledgment works (W1C clears interrupt source)

## Debugging Common Issues

### Issue 1: IRQ Count Never Increases

**Symptoms**: `irq_count` stays at 0, no dmesg messages

**Debug steps**:
```bash
# Check if driver loaded and probed
dmesg | grep smarttimer

# Check interrupt registration
cat /proc/interrupts | grep smarttimer
# Should show: "65: ... smarttimer"

# In Renode monitor:
(monitor) smarttimer ReadDoubleWord 0x4
# Check if STATUS.WRAP bit is being set

# Enable verbose GIC logging
(monitor) logLevel 3 gic
# Check if GIC is receiving interrupts
```

**Common causes**:
- IRQ wiring mismatch in `.repl` file (wrong GIC ID)
- Device tree `interrupts` property incorrect
- Timer not enabled or period = 0
- Verilator library not found or crashed

### Issue 2: Renode Hangs or Very Slow

**Symptoms**: Emulation becomes unresponsive after loading driver

**Cause**: Interrupt storm (IRQ firing continuously, not being cleared)

**Debug**:
```bash
# In Renode monitor before hang:
(monitor) smarttimer ReadDoubleWord 0x4
# If STATUS.WRAP is stuck at 1, IRQ isn't being cleared

# Check handler code:
# - Ensure W1C write is present
# - Verify STATUS_WRAP_BIT value matches RTL
```

**Fix**: Add/fix W1C write in IRQ handler:
```c
writel(STATUS_WRAP_BIT, stdev->base + STATUS_OFFSET);
```

### Issue 3: Kernel "nobody cared" Error

**Symptoms**:
```
irq 65: nobody cared (try booting with the "irqpoll" option)
handlers:
[<...>] smarttimer_irq_handler
```

**Cause**: Handler returned `IRQ_NONE` too many times (kernel thinks IRQ is stuck)

**Fix**: Ensure handler properly checks status and returns `IRQ_HANDLED`:
```c
if (status & STATUS_WRAP_BIT) {
    // ... handle ...
    return IRQ_HANDLED;  // Not IRQ_NONE!
}
```

### Issue 4: Build Failures

**Verilator compilation errors**:
- Check Verilator version: `verilator --version` (need 4.0+)
- Verify RTL syntax is synthesizable (no `$display`, `initial` blocks)

**Driver build errors**:
- Ensure kernel headers installed: `apt-get install linux-headers-$(uname -r)`
- Check cross-compiler path: `arm-linux-gnueabihf-gcc --version`

**Renode .so load failures**:
- Verify library path in `.repl` is absolute or relative to Renode working dir
- Check library built with `-fPIC` flag: `file libVsmartimer.so | grep shared`

## Next Steps (Week 9)

This demo proves basic interrupt flow works. Week 9 will add:

1. **Blocking I/O**: Replace polling with `wait_event_interruptible()`
2. **Synchronization**: Add spinlocks for shared data (IRQ vs process context)
3. **Advanced patterns**: Top-half/bottom-half split, workqueues
4. **FIR filter IRQ**: Completion notification instead of polling
5. **Performance analysis**: Measure latency improvements

**Key difference**:
- **This week**: Count interrupts (proof of concept)
- **Next week**: Use interrupts to implement efficient I/O patterns

## References

- ARM GIC Architecture Specification v2.0
- Linux Kernel Documentation: `Documentation/devicetree/bindings/interrupt-controller/arm,gic.yaml`
- Renode documentation: Interrupt handling and Verilator integration
- Device Tree Specification v0.3, Chapter 2.4: Interrupt Properties
