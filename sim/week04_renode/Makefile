# Purpose: build bare-metal ARM (Cortex-A9) demo for Renode

CROSS ?= arm-none-eabi-
CC := $(CROSS)gcc
AS := $(CROSS)as
LD := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

BARE_DIR := bare
OUT := $(BARE_DIR)/bare.elf

CFLAGS := -mcpu=cortex-a9 -marm -Os -ffreestanding -fno-builtin -nostdlib -Wall -Wextra
LDFLAGS := -T $(BARE_DIR)/linker.ld -nostdlib

all: $(OUT)

$(OUT): $(BARE_DIR)/start.o $(BARE_DIR)/main.o
	$(CC) $(CFLAGS) -Wl,$(LDFLAGS) -o $@ $^

$(BARE_DIR)/start.o: $(BARE_DIR)/start.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(BARE_DIR)/main.o: $(BARE_DIR)/main.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(BARE_DIR)/*.o $(OUT)

