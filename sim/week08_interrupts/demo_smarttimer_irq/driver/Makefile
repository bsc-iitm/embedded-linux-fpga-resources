obj-m += smarttimer_irq_simple.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# For cross-compilation (ARM)
# make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KDIR=/path/to/kernel
