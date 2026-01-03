// Week 7 Demo 2: Simple FIR Filter Driver
// Purpose: Teaching demo for Renode with FIR peripheral stub
// Based on Week 6 FFT demo pattern

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BSES Week 7");
MODULE_DESCRIPTION("Week 7: Simple FIR Filter Driver for Renode Demo");

// FIR Filter memory map
#define OFF_CTRL     0x00u   // [0]=EN, [1]=START(W1P), [2]=RESET(W1P)
#define OFF_STATUS   0x04u   // [0]=DONE(W1C), [1]=READY
#define OFF_LEN      0x08u   // Number of samples (1-32)
#define OFF_COEFF0   0x10u
#define OFF_COEFF1   0x14u
#define OFF_COEFF2   0x18u
#define OFF_COEFF3   0x1Cu
#define OFF_DATA_IN  0x100u  // 32 x u32
#define OFF_DATA_OUT 0x200u  // 32 x u32

#define MAX_SAMPLES  32u
#define NUM_TAPS     4u

struct fir_dev {
    void __iomem *base;
    struct device *dev;
    u32 in_pos;         // Input write position
    struct mutex lock;
};

// Sysfs: status (read STATUS register)
static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u32 st = readl(fir->base + OFF_STATUS);
    return scnprintf(buf, PAGE_SIZE, "0x%08x (DONE=%d, READY=%d)\n",
                     st, st & 0x1, (st >> 1) & 0x1);
}
static DEVICE_ATTR_RO(status);

// Sysfs: len (read LEN register)
static ssize_t len_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u32 l = readl(fir->base + OFF_LEN);
    return scnprintf(buf, PAGE_SIZE, "%u\n", l);
}
static DEVICE_ATTR_RO(len);

// Sysfs: coeff (write "c0,c1,c2,c3" to load filter taps)
static ssize_t coeff_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    char *s, *p, *tok;
    u32 coeffs[NUM_TAPS];
    int i = 0;

    s = kstrndup(buf, min_t(size_t, count, PAGE_SIZE - 1), GFP_KERNEL);
    if (!s)
        return -ENOMEM;

    mutex_lock(&fir->lock);
    p = s;
    while ((tok = strsep(&p, " ,\t\n")) != NULL && i < NUM_TAPS) {
        if (!*tok)
            continue;
        if (kstrtou32(tok, 0, &coeffs[i]))
            break;
        i++;
    }

    // Write coefficients to hardware
    if (i >= 1) writel(coeffs[0] & 0xFFFF, fir->base + OFF_COEFF0);
    if (i >= 2) writel(coeffs[1] & 0xFFFF, fir->base + OFF_COEFF1);
    if (i >= 3) writel(coeffs[2] & 0xFFFF, fir->base + OFF_COEFF2);
    if (i >= 4) writel(coeffs[3] & 0xFFFF, fir->base + OFF_COEFF3);

    mutex_unlock(&fir->lock);

    dev_info(dev, "Loaded %d coefficients\n", i);
    kfree(s);
    return count;
}

static ssize_t coeff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u32 c[NUM_TAPS];

    c[0] = readl(fir->base + OFF_COEFF0) & 0xFFFF;
    c[1] = readl(fir->base + OFF_COEFF1) & 0xFFFF;
    c[2] = readl(fir->base + OFF_COEFF2) & 0xFFFF;
    c[3] = readl(fir->base + OFF_COEFF3) & 0xFFFF;

    return scnprintf(buf, PAGE_SIZE, "0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
                     c[0], c[1], c[2], c[3]);
}
static DEVICE_ATTR_RW(coeff);

// Sysfs: data_in (write samples, comma-separated)
static ssize_t data_in_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    char *s, *p, *tok;
    u32 val;

    s = kstrndup(buf, min_t(size_t, count, PAGE_SIZE - 1), GFP_KERNEL);
    if (!s)
        return -ENOMEM;

    mutex_lock(&fir->lock);

    // Special command: "reset" clears position
    if (sysfs_streq(buf, "reset")) {
        fir->in_pos = 0;
        writel(0, fir->base + OFF_LEN);
        mutex_unlock(&fir->lock);
        kfree(s);
        return count;
    }

    // Parse and write samples
    p = s;
    while ((tok = strsep(&p, " ,\t\n")) != NULL) {
        if (!*tok)
            continue;
        if (kstrtou32(tok, 0, &val))
            break;
        if (fir->in_pos >= MAX_SAMPLES)
            break;

        writel(val & 0xFFFF, fir->base + OFF_DATA_IN + fir->in_pos * sizeof(u32));
        fir->in_pos++;
    }

    // Update LEN register
    writel(fir->in_pos, fir->base + OFF_LEN);

    mutex_unlock(&fir->lock);

    kfree(s);
    return count;
}
static DEVICE_ATTR_WO(data_in);

// Sysfs: data_out (read filtered samples)
static ssize_t data_out_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u32 len = readl(fir->base + OFF_LEN);
    ssize_t written = 0;
    int i;

    if (len > MAX_SAMPLES)
        len = MAX_SAMPLES;

    for (i = 0; i < len && written < PAGE_SIZE - 20; i++) {
        u32 val = readl(fir->base + OFF_DATA_OUT + i * sizeof(u32));
        s16 sample = (s16)(val & 0xFFFF);  // Sign-extend
        written += scnprintf(buf + written, PAGE_SIZE - written, "%d%s",
                             sample, (i < len - 1) ? ", " : "\n");
    }

    return written;
}
static DEVICE_ATTR_RO(data_out);

// Sysfs: ctrl (textual commands and numeric writes)
static ssize_t ctrl_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    unsigned long val;

    // Textual commands
    if (sysfs_streq(buf, "reset")) {
        mutex_lock(&fir->lock);
        fir->in_pos = 0;
        writel(0, fir->base + OFF_LEN);
        writel(0x4, fir->base + OFF_CTRL);  // Pulse RESET bit
        mutex_unlock(&fir->lock);
        return count;
    }

    if (sysfs_streq(buf, "start")) {
        writel(0x3, fir->base + OFF_CTRL);  // EN=1, START=1
        return count;
    }

    // Numeric write
    if (kstrtoul(buf, 0, &val))
        return -EINVAL;

    writel((u32)val & 0x7, fir->base + OFF_CTRL);
    return count;
}
static DEVICE_ATTR_WO(ctrl);

static struct attribute *fir_attrs[] = {
    &dev_attr_status.attr,
    &dev_attr_len.attr,
    &dev_attr_coeff.attr,
    &dev_attr_data_in.attr,
    &dev_attr_data_out.attr,
    &dev_attr_ctrl.attr,
    NULL,
};
static const struct attribute_group fir_group = { .attrs = fir_attrs };

static int fir_probe(struct platform_device *pdev)
{
    struct fir_dev *fir;
    struct resource *res;
    int rc;

    fir = devm_kzalloc(&pdev->dev, sizeof(*fir), GFP_KERNEL);
    if (!fir)
        return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    fir->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(fir->base))
        return PTR_ERR(fir->base);

    fir->dev = &pdev->dev;
    fir->in_pos = 0;
    mutex_init(&fir->lock);
    platform_set_drvdata(pdev, fir);

    rc = sysfs_create_group(&pdev->dev.kobj, &fir_group);
    if (rc)
        return rc;

    dev_info(&pdev->dev, "FIR filter driver bound: %pR\n", res);
    return 0;
}

static int fir_remove(struct platform_device *pdev)
{
    sysfs_remove_group(&pdev->dev.kobj, &fir_group);
    return 0;
}

static const struct of_device_id fir_of_match[] = {
    { .compatible = "acme,fir-filter-v1" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, fir_of_match);

static struct platform_driver fir_driver = {
    .probe  = fir_probe,
    .remove = fir_remove,
    .driver = {
        .name = "fir_simple",
        .of_match_table = fir_of_match,
    },
};

module_platform_driver(fir_driver);
