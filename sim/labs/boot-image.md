# Creating U-Boot FIT Images (image.ub)

The SD card partition 1 (`p1`) contains a file named `BOOT.BIN` - this is the first stage bootloader that also contains U-Boot etc.  The job of this executable is to find the `image.ub` and load it into memory.  Therefore, we need to create an appopriate image from the kernel image that we have compiled.

## Prerequisites
- `u-boot-tools` package installed (provides `mkimage`, `dumpimage`, `fdtdump`)
- A reference working `image.ub` (optional but helpful)
- Compiled Linux kernel with zImage

## 1. Extract Information from Existing FIT Image

### List contents and metadata
```bash
dumpimage -l image.ub
```
Key fields to note:
- **Load Address** and **Entry Point** (must match for kernel)
- **Compression** type
- **Architecture**

### Extract kernel
```bash
dumpimage -T flat_dt -p 0 -o extracted-kernel image.ub
```

### Extract DTB
```bash
dumpimage -T flat_dt -p 1 -o extracted.dtb image.ub
```

### View full FIT structure (verbose)
```bash
fdtdump image.ub | head -150
```

## 2. Locate Required Components

### zImage (compressed kernel)
After building the kernel:
```
arch/arm/boot/zImage      # ARM 32-bit
arch/arm64/boot/Image     # ARM 64-bit
```

### Device Tree Blob (DTB)
```
arch/arm/boot/dts/<board>.dtb
```

## 3. Create FIT Image Source (.its)

```dts
/dts-v1/;

/ {
    description = "Boot Image";
    #address-cells = <1>;

    images {
        kernel {
            description = "Linux Kernel";
            data = /incbin/("zImage");
            type = "kernel";
            arch = "arm";
            os = "linux";
            compression = "none";
            load = <0x00080000>;    /* from dumpimage -l */
            entry = <0x00080000>;   /* from dumpimage -l */
            hash { algo = "sha1"; };
        };

        fdt {
            description = "Device Tree Blob";
            data = /incbin/("board.dtb");
            type = "flat_dt";
            arch = "arm";
            compression = "none";
            hash { algo = "sha1"; };
        };
    };

    configurations {
        default = "config";
        config {
            description = "Boot Linux kernel with FDT";
            kernel = "kernel";
            fdt = "fdt";
        };
    };
};
```

## 4. Build the FIT Image

```bash
mkimage -f boot.its image.ub
```

## Quick Reference

| Task | Command |
|------|---------|
| List FIT contents | `dumpimage -l image.ub` |
| Extract component N | `dumpimage -T flat_dt -p N -o output image.ub` |
| Build FIT image | `mkimage -f boot.its image.ub` |
| Dump FIT structure | `fdtdump image.ub` |

## Common Issues

- **Wrong load address**: Kernel hangs or crashes immediately. Verify with `dumpimage -l` on working image.
- **Using vmlinux instead of zImage**: Results in huge image; use compressed zImage from `arch/arm/boot/`.
- **DTB mismatch**: Board fails to initialize peripherals. Use DTB matching kernel version or extract from working image.
