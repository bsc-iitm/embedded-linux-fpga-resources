#!/bin/bash
# Required device nodes
mkdir -p /tmp/initramfs/{bin,sbin,etc,proc,sys,dev,usr/bin}
cd /tmp/initramfs
sudo mknod -m 600 dev/console c 5 1
sudo mknod -m 666 dev/null    c 1 3

# Minimal /init
cat > /tmp/initramfs/init <<'EOF'
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev
echo "Userspace up; dropping to shell."

# Setting up for autoloading drivers if needed
# for f in /sys/bus/platform/devices/*/modalias ; do
#         [ -f "$f" ] && modprobe "$(cat "$f")" 2>/dev/null || true
# done

exec /bin/sh < /dev/console > /dev/console 2>&1
EOF
chmod +x /tmp/initramfs/init
