# Build bare-metal timer test for ARM

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
CFLAGS = -mcpu=cortex-a9 -marm -nostdlib -ffreestanding -O2
LDFLAGS = -T link.ld

# Source files
C_SRC = src/timer_test.c
ASM_SRC = src/startup.s

# Object files
OBJS = startup.o timer_test.o

ELF = timer_test.elf
BIN = timer_test.bin

all: $(ELF) $(BIN)

startup.o: $(ASM_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

timer_test.o: $(C_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) $(ELF) $(BIN) *.o

# For Renode testing
renode: $(ELF)
	@echo "Starting Renode with timer_test.elf..."
	@echo "Load in Renode with: sysbus LoadELF @$(shell pwd)/$(ELF)"
	cd renode && renode demo1.resc

help:
	@echo "Bare-metal build for Smart Timer test"
	@echo ""
	@echo "Prerequisites:"
	@echo "  - arm-none-eabi-gcc (ARM bare-metal toolchain)"
	@echo "  - Renode installed"
	@echo ""
	@echo "Targets:"
	@echo "  make         - Build ELF and BIN"
	@echo "  make renode  - Build and start Renode"
	@echo "  make clean   - Remove generated files"
	@echo ""
	@echo "Note: You need a linker script (link.ld) for proper memory layout"

.PHONY: all clean renode help
