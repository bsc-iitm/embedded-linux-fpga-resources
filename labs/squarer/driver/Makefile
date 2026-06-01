obj-m += squarer_mmio.o
obj-m += squarer_dma.o

KDIR ?= /lib/modules/$(shell uname -r)/build
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

all:
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
