#!/bin/bash

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
# Kernel path
export KDIR=$PWD/../downloads/linux-6.6
# Busybox path
export BDIR=$PWD/../downloads/busybox-1.32.0
# Lab folder
export LDIR=$PWD

exec /bin/bash
