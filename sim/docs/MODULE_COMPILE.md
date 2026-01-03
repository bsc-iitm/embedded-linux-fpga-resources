# Steps to compile and install a module in initramfs

We will be using *out-of-tree* builds to compile and install modules.  This means that the module source code is not directly inside the directory tree of the Linux kernel.  

You still need either a full compiled kernel, or at least all the headers for one.  In our case, we are creating a custom kernel since we want to control exactly what goes in there, and use the `initramfs` to load just the files we need.  A normal Linux setup would probably use a separate initial ramdisk that is loaded from the command line.  Here we are keeping things simple.

## Build (out-of-tree)

Ensure that the following env variables are properly set:

- `KDIR`: point to the kernel build directory matching the guest kernel
- `ARCH=arm`
- `CROSS_COMPILE=arm-linux-gnueabihf-`

The compilation of the kernel is done with:

```
make -C "$KDIR$" M=$(pwd) modules
```
This will create a `.ko` file as output.

Now we also need to update the module into `initramfs`.  Set up the `/tmp/initramfs` first.  Then, before recompiling the kernel, run

```bash
make -C "$KDIR" M="$(pwd)" modules_install INSTALL_MOD_PATH=/tmp/initramfs
```

and then go to `$KDIR` and run

```bash
make -j4
```

This will cause a fresh `vmlinux` kernel to be created, which includes the modules from the path above.