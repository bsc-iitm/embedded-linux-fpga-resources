# Week 3 Demo — Virtual Register Driver (No Hardware)

Goals
- Build and load a tiny Linux kernel module on any PC.
- Expose a fake register bank via:
  - Character device: `/dev/vreg_demo` (read shows snapshot)
  - Sysfs attributes: `/sys/class/misc/vreg_demo/device/{ctrl,period,duty,status}`

Prerequisites
- Linux kernel headers for your running kernel:
  - Ubuntu/Debian: `sudo apt-get install build-essential linux-headers-$(uname -r)`
  - Fedora/RHEL/CentOS: `sudo dnf install @development-tools kernel-devel` (or `yum`)
  - Arch: `sudo pacman -S base-devel linux-headers`
- A shell with `make` and `gcc` available.

Build
- From repo root:
  - `make -C sim/week03_driver`

Load / Unload
- Load: `sudo insmod sim/week03_driver/vreg_demo.ko`
- Check dmesg: `dmesg | tail -n +1 | tail -50`
- Device node: `/dev/vreg_demo` should exist (misc device)
- Sysfs path: `/sys/class/misc/vreg_demo/device/`
- Unload: `sudo rmmod vreg_demo`

Interact
- Read snapshot via char device:
  - `cat /dev/vreg_demo`
- Set registers via sysfs (accepts decimal or hex like `0x10`):
  - `echo 0x1 | sudo tee /sys/class/misc/vreg_demo/device/ctrl`
  - `echo 0xFF | sudo tee /sys/class/misc/vreg_demo/device/period`
  - `echo 0x20 | sudo tee /sys/class/misc/vreg_demo/device/duty`
  - `cat /sys/class/misc/vreg_demo/device/status`

Notes
- This is a pure RAM‑backed “device” for teaching. In real drivers you’d map MMIO with `ioremap()` and use `readl()/writel()` instead of plain variables.
- Permissions: for class demos, use `sudo`. Production drivers often ship udev rules to allow non‑root access.
- Secure Boot: unsigned modules may be blocked; either disable Secure Boot or sign the module if needed.

