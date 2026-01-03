# Renode

- Uses Week 8 DTB and overlay; wiring remains SPI 33 → GIC 65.
- Script: `renode/demo_blocking.resc` loads kernel + DTB and points to `build/libVtop.so`.
- In guest: insmod `smarttimer_blocking.ko`, set period/en, `cat /dev/smarttimer0` to block until wrap.
