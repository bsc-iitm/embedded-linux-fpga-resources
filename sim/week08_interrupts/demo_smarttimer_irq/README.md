# Smart Timer with IRQ Demo

End-to-end interrupt demo: RTL → Renode GIC → Linux driver

## Quick Start

### 1. Test RTL
```bash
cd rtl
make test
```

### 2. Build Verilator Library
```bash
cd verilator_cosim
mkdir build && cd build
cmake ..
make
# Produces libVsmartimer.so
```

### 3. Compile Device Tree
```bash
cd renode
dtc -I dts -O dtb -o smarttimer_irq.dtb smarttimer_irq.dts
```

### 4. Build Driver
```bash
cd driver
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KDIR=/path/to/kernel
```

### 5. Run in Renode
```bash
cd renode
# Edit demo_irq.resc with your kernel/initramfs paths
renode demo_irq.resc
```

### 6. Test in Linux
```bash
insmod smarttimer_irq_simple.ko
echo 50000000 > /sys/devices/platform/smarttimer/period
echo 1 > /sys/devices/platform/smarttimer/enable
watch cat /sys/devices/platform/smarttimer/irq_count
```

## What It Shows

- IRQ asserts on counter wrap
- Handler increments atomic counter and prints message
- Disabling timer stops IRQ count
- W1C clears interrupt source
