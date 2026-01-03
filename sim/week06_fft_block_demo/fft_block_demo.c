// Week 6: FFT-style block transfer demo (MMIO array window)
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BSES Week 6");
MODULE_DESCRIPTION("Week 6: FFT block transfer demo via MMIO array window");

#define OFF_CTRL   0x00u  // bit0 EN, bit1 START (pulse)
#define OFF_STATUS 0x04u  // bit0 DONE
#define OFF_LEN    0x08u  // number of complex samples (max 32)
#define OFF_DATA   0x100u // 64 x u32 (32 complex interleaved: re0,im0,re1,im1,...)

#define MAX_WORDS  64u

struct fft_demo_dev {
    void __iomem *base;
    struct device *dev;
    u32 pos;            // number of words written so far (0..MAX_WORDS)
    struct mutex lock;  // serialize sysfs access
};

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fft_demo_dev *fd = dev_get_drvdata(dev);
    u32 st = readl(fd->base + OFF_STATUS) & 0x1u;
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", st);
}
static DEVICE_ATTR_RO(status);

static ssize_t len_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fft_demo_dev *fd = dev_get_drvdata(dev);
    u32 l = readl(fd->base + OFF_LEN);
    return scnprintf(buf, PAGE_SIZE, "%u\n", l);
}
static DEVICE_ATTR_RO(len);

static ssize_t ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct fft_demo_dev *fd = dev_get_drvdata(dev);
    unsigned long val;

    // Allow textual commands for teaching convenience
    if (sysfs_streq(buf, "reset")) {
        mutex_lock(&fd->lock);
        fd->pos = 0;
        writel(0, fd->base + OFF_LEN);
        mutex_unlock(&fd->lock);
        return count;
    }
    if (sysfs_streq(buf, "start")) {
        u32 ctrl = readl(fd->base + OFF_CTRL) | (1u << 1);
        writel(ctrl, fd->base + OFF_CTRL);
        ctrl &= ~(1u << 1);
        writel(ctrl, fd->base + OFF_CTRL);
        return count;
    }

    // Numeric control (bit0 EN, bit1 START, bit2 RESET)
    if (kstrtoul(buf, 0, &val))
        return -EINVAL;

    if (val & (1u << 2)) {
        mutex_lock(&fd->lock);
        fd->pos = 0;
        writel(0, fd->base + OFF_LEN);
        mutex_unlock(&fd->lock);
    }

    // Apply EN/START bits to CTRL
    {
        u32 ctrl = readl(fd->base + OFF_CTRL);
        ctrl = (ctrl & ~0x3u) | ((u32)val & 0x3u);
        writel(ctrl, fd->base + OFF_CTRL);
        if (val & (1u << 1)) { // pulse START
            ctrl |= (1u << 1);
            writel(ctrl, fd->base + OFF_CTRL);
            ctrl &= ~(1u << 1);
            writel(ctrl, fd->base + OFF_CTRL);
        }
    }

    return count;
}
static DEVICE_ATTR_WO(ctrl);

static ssize_t vector_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct fft_demo_dev *fd = dev_get_drvdata(dev);
    char *s, *p, *tok;
    u32 val;

    s = kstrndup(buf, min_t(size_t, count, PAGE_SIZE - 1), GFP_KERNEL);
    if (!s)
        return -ENOMEM;

    mutex_lock(&fd->lock);
    p = s;
    while ((tok = strsep(&p, " ,\t\n")) != NULL) {
        if (!*tok)
            continue;
        if (kstrtou32(tok, 0, &val))
            break;
        if (fd->pos >= MAX_WORDS)
            break;
        writel(val, fd->base + OFF_DATA + fd->pos * sizeof(u32));
        fd->pos++;
    }
    // Update LEN as number of complex pairs
    writel(fd->pos / 2, fd->base + OFF_LEN);
    mutex_unlock(&fd->lock);

    kfree(s);
    return count;
}
static DEVICE_ATTR_WO(vector);

static struct attribute *fft_attrs[] = {
    &dev_attr_status.attr,
    &dev_attr_len.attr,
    &dev_attr_ctrl.attr,
    &dev_attr_vector.attr,
    NULL,
};
static const struct attribute_group fft_group = { .attrs = fft_attrs };

static int fft_demo_probe(struct platform_device *pdev)
{
    struct fft_demo_dev *fd;
    struct resource *res;
    int rc;

    fd = devm_kzalloc(&pdev->dev, sizeof(*fd), GFP_KERNEL);
    if (!fd) return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    fd->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(fd->base)) return PTR_ERR(fd->base);

    fd->dev = &pdev->dev;
    fd->pos = 0;
    mutex_init(&fd->lock);
    platform_set_drvdata(pdev, fd);

    rc = sysfs_create_group(&pdev->dev.kobj, &fft_group);
    if (rc) return rc;

    dev_info(&pdev->dev, "fft_demo bound: %pR\n", res);
    return 0;
}

static int fft_demo_remove(struct platform_device *pdev)
{
    sysfs_remove_group(&pdev->dev.kobj, &fft_group);
    return 0;
}

static const struct of_device_id fft_of_match[] = {
    { .compatible = "acme,fft32-v1" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, fft_of_match);

static struct platform_driver fft_demo_driver = {
    .probe  = fft_demo_probe,
    .remove = fft_demo_remove,
    .driver = {
        .name = "fft_block_demo",
        .of_match_table = fft_of_match,
    },
};

module_platform_driver(fft_demo_driver);
