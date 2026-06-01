// Squarer MMIO Driver
// Demonstrates per-sample register access (slow path)
//
// Usage:
//   write(fd, input_array, n * sizeof(int16_t))  - provide input samples
//   read(fd, output_array, n * sizeof(int32_t))  - compute and read results
//
// Each read triggers n register writes + n register reads (2n MMIO ops)

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DRV_NAME "squarer_mmio"
#define MAX_SAMPLES (256 * 1024)  // 256K samples: 512KB input, 1MB output

// Register offsets
#define REG_DATA_IN  0x00
#define REG_DATA_OUT 0x04

struct squarer_mmio_dev {
    void __iomem *base;
    struct miscdevice misc;
    struct mutex lock;

    // Buffers for input/output
    s16 *input_buf;
    s32 *output_buf;
    size_t count;  // number of samples
};

static ssize_t squarer_write(struct file *file, const char __user *buf,
                             size_t len, loff_t *off)
{
    struct squarer_mmio_dev *dev = container_of(file->private_data,
                                    struct squarer_mmio_dev, misc);
    size_t count = len / sizeof(s16);

    if (count == 0 || count > MAX_SAMPLES)
        return -EINVAL;

    mutex_lock(&dev->lock);

    if (copy_from_user(dev->input_buf, buf, count * sizeof(s16))) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }
    dev->count = count;

    mutex_unlock(&dev->lock);
    return count * sizeof(s16);
}

static ssize_t squarer_read(struct file *file, char __user *buf,
                            size_t len, loff_t *off)
{
    struct squarer_mmio_dev *dev = container_of(file->private_data,
                                    struct squarer_mmio_dev, misc);
    size_t i;
    size_t out_bytes;

    mutex_lock(&dev->lock);

    if (dev->count == 0) {
        mutex_unlock(&dev->lock);
        return 0;
    }

    out_bytes = dev->count * sizeof(s32);
    if (len < out_bytes)
        out_bytes = (len / sizeof(s32)) * sizeof(s32);

    // This is the slow path: one register write + read per sample
    for (i = 0; i < out_bytes / sizeof(s32); i++) {
        // Write input to hardware
        writel((u32)(u16)dev->input_buf[i], dev->base + REG_DATA_IN);
        // Read result from hardware
        dev->output_buf[i] = (s32)readl(dev->base + REG_DATA_OUT);
    }

    if (copy_to_user(buf, dev->output_buf, out_bytes)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    mutex_unlock(&dev->lock);
    return out_bytes;
}

static const struct file_operations squarer_fops = {
    .owner = THIS_MODULE,
    .write = squarer_write,
    .read  = squarer_read,
};

static int squarer_mmio_probe(struct platform_device *pdev)
{
    struct squarer_mmio_dev *dev;
    struct resource *res;
    int ret;

    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    dev->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(dev->base))
        return PTR_ERR(dev->base);

    dev->input_buf = devm_kmalloc(&pdev->dev, MAX_SAMPLES * sizeof(s16), GFP_KERNEL);
    dev->output_buf = devm_kmalloc(&pdev->dev, MAX_SAMPLES * sizeof(s32), GFP_KERNEL);
    if (!dev->input_buf || !dev->output_buf)
        return -ENOMEM;

    mutex_init(&dev->lock);

    dev->misc.minor = MISC_DYNAMIC_MINOR;
    dev->misc.name = "squarer_mmio";
    dev->misc.fops = &squarer_fops;

    ret = misc_register(&dev->misc);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register misc device\n");
        return ret;
    }

    platform_set_drvdata(pdev, dev);
    dev_info(&pdev->dev, "squarer_mmio: registered /dev/squarer_mmio\n");
    return 0;
}

static int squarer_mmio_remove(struct platform_device *pdev)
{
    struct squarer_mmio_dev *dev = platform_get_drvdata(pdev);
    misc_deregister(&dev->misc);
    return 0;
}

static const struct of_device_id squarer_mmio_of_match[] = {
    { .compatible = "demo,squarer-mmio" },
    { }
};
MODULE_DEVICE_TABLE(of, squarer_mmio_of_match);

static struct platform_driver squarer_mmio_driver = {
    .driver = {
        .name = DRV_NAME,
        .of_match_table = squarer_mmio_of_match,
    },
    .probe  = squarer_mmio_probe,
    .remove = squarer_mmio_remove,
};
module_platform_driver(squarer_mmio_driver);

MODULE_AUTHOR("Demo");
MODULE_DESCRIPTION("Squarer MMIO driver - per-sample register access");
MODULE_LICENSE("GPL");
