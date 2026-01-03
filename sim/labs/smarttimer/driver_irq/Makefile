obj-m += smarttimer_blocking.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Cross-compile example:
# make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KDIR=/path/to/kernel

