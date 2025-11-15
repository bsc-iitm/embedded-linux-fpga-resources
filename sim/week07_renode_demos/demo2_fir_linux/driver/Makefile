obj-m += fir_simple.o

# Default: build against kernel sources in environment
KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

help:
	@echo "FIR filter driver build"
	@echo ""
	@echo "For host kernel:"
	@echo "  make"
	@echo ""
	@echo "For cross-compile (ARM):"
	@echo "  make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KDIR=/path/to/linux"
	@echo ""
	@echo "Output: fir_simple.ko"

.PHONY: all clean help
