#!/bin/bash

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

# Lab folder
export LDIR=$PWD
# Downloads (kernel/busybox source and build configs live here)
export DL=$LDIR/downloads
# Kernel path
export KDIR=$DL/linux-6.6
# Busybox path
export BDIR=$DL/busybox-1.32.0

exec /bin/bash
