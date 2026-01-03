# Week 3 Demo — Virtual Register Driver (No Hardware)

### Goals
- Build and load a tiny Linux kernel module on any PC.
- Expose a fake register bank via:
  - Character device: `/dev/vreg_demo` (read shows snapshot)
  - Sysfs attributes: `/sys/class/misc/vreg_demo/device/{ctrl,period,duty,status}`

### Prerequisites
- Linux kernel headers for your running kernel (Ubuntu 24.04):
  ```bash
  sudo apt-get install build-essential linux-headers-$(uname -r)
  ```
- A shell with `make` and `gcc` available (included in build-essential)

**IMPORTANT**: For this demo alone, you should NOT have the `export ARCH=` and `export CROSS_COMPILE=` environment variables set.  Having these will make the compiler try to compile for ARM, whereas the purpose of this demo is to show how it works on a regular Linux system.

To remove these variables, type the following at the shell:

```bash
unset KDIR
unset ARCH
unset CROSS_COMPILE
```

### Build
- From repo root:
  - `make -C sim/week03_driver`

### Load / Unload
- Load: `sudo insmod sim/week03_driver/vreg_demo.ko`
- Check dmesg: `dmesg | tail -n +1 | tail -50`
- Device node: `/dev/vreg_demo` should exist (misc device)
- Sysfs path: `/sys/class/misc/vreg_demo/`
- Unload: `sudo rmmod vreg_demo`

### Interact
- Read snapshot via char device:
  - `cat /dev/vreg_demo`
- Set registers via sysfs (accepts decimal or hex like `0x10`):
  - `echo 0x1 | sudo tee /sys/class/misc/vreg_demo/ctrl`
  - `echo 0xFF | sudo tee /sys/class/misc/vreg_demo/period`
  - `echo 0x20 | sudo tee /sys/class/misc/vreg_demo/duty`
  - `cat /sys/class/misc/vreg_demo/status`

### Notes
- This is a pure RAM‑backed “device” for teaching. In real drivers you’d map MMIO with `ioremap()` and use `readl()/writel()` instead of plain variables.
- Permissions: for class demos, use `sudo`. Production drivers often ship udev rules to allow non‑root access.
- Secure Boot: unsigned modules may be blocked; either disable Secure Boot or sign the module if needed.

