CC = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -O2 -static

all: test_squarer

test_squarer: test_squarer.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f test_squarer

.PHONY: all clean
