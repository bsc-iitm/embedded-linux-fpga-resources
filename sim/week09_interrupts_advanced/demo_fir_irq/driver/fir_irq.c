// FIR filter driver with DONE interrupt and blocking read (Week 9)

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kernel.h>

#define REG_CTRL     0x000
#define REG_STATUS   0x004
#define REG_LEN      0x008
#define REG_COEFF0   0x010
#define REG_DATA_IN  0x100
#define REG_DATA_OUT 0x200

#define STATUS_DONE_BIT  (1u << 0)

#define FIR_MAX_LEN 32
#define FIR_NTAPS   4

struct fir_dev {
    struct device *dev;
    void __iomem *base;
    int irq;

    struct mutex coeff_lock;
    spinlock_t status_lock;
    wait_queue_head_t wait;

    bool processing;
    bool done;

    s16 coeff[FIR_NTAPS];
    u32 len;        // number of samples (1..32)
    u32 in_pos;     // current DATA_IN write position

    struct miscdevice miscdev;
};

static irqreturn_t fir_irq_handler(int irq, void *dev_id)
{
    struct fir_dev *fir = dev_id;
    u32 s = readl(fir->base + REG_STATUS);
    if (!(s & STATUS_DONE_BIT))
        return IRQ_NONE;

    writel(STATUS_DONE_BIT, fir->base + REG_STATUS);
    spin_lock(&fir->status_lock);
    fir->done = true;
    fir->processing = false;
    spin_unlock(&fir->status_lock);
    wake_up_interruptible(&fir->wait);
    return IRQ_HANDLED;
}

static int fir_open(struct inode *inode, struct file *file)
{
    struct fir_dev *fir = container_of(file->private_data, struct fir_dev, miscdev);
    file->private_data = fir;
    return 0;
}

static ssize_t fir_read(struct file *file, char __user *ubuf, size_t len, loff_t *ppos)
{
    struct fir_dev *fir = file->private_data;
    u32 i, hw_len;
    s16 tmp[FIR_MAX_LEN];
    size_t bytes;

    /* One-shot semantics per open: return EOF on subsequent reads */
    if (*ppos > 0)
        return 0;

    if (wait_event_interruptible(fir->wait, READ_ONCE(fir->done)))
        return -ERESTARTSYS;

    hw_len = readl(fir->base + REG_LEN);
    if (hw_len > FIR_MAX_LEN) hw_len = FIR_MAX_LEN;
    bytes = min_t(size_t, len, hw_len * sizeof(s16));
    for (i = 0; i < hw_len && (i * 2) < bytes; i++)
        tmp[i] = (s16)(readl(fir->base + REG_DATA_OUT + i*4) & 0xFFFF);

    if (copy_to_user(ubuf, tmp, bytes))
        return -EFAULT;

    // One-shot read; clear done for next cycle
    spin_lock(&fir->status_lock);
    fir->done = false;
    spin_unlock(&fir->status_lock);

    *ppos += bytes;
    return bytes;
}

static const struct file_operations fir_fops = {
    .owner = THIS_MODULE,
    .open  = fir_open,
    .read  = fir_read,
    .llseek = no_llseek,
};

/* ------------------- Sysfs interface (coeff/data/len/ctrl/output) ------------------- */

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u32 st = readl(fir->base + REG_STATUS);
    return scnprintf(buf, PAGE_SIZE, "0x%08x (DONE=%d)\n", st, !!(st & STATUS_DONE_BIT));
}
static DEVICE_ATTR_RO(status);

static ssize_t len_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u32 l = readl(fir->base + REG_LEN);
    return scnprintf(buf, PAGE_SIZE, "%u\n", l);
}
static DEVICE_ATTR_RO(len);

/* coeff: read/write 4 Q15 taps (lower 16b), comma/space separated */
static ssize_t coeff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u32 c0 = readl(fir->base + REG_COEFF0) & 0xFFFF;
    u32 c1 = readl(fir->base + REG_COEFF0 + 4) & 0xFFFF;
    u32 c2 = readl(fir->base + REG_COEFF0 + 8) & 0xFFFF;
    u32 c3 = readl(fir->base + REG_COEFF0 + 12) & 0xFFFF;
    return scnprintf(buf, PAGE_SIZE, "0x%04x, 0x%04x, 0x%04x, 0x%04x\n", c0, c1, c2, c3);
}

static ssize_t coeff_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    char *tmp = NULL, *p, *tok;
    u32 vals[FIR_NTAPS];
    int i = 0;

    tmp = kstrndup(buf, min_t(size_t, count, PAGE_SIZE - 1), GFP_KERNEL);
    if (!tmp)
        return -ENOMEM;

    mutex_lock(&fir->coeff_lock);
    p = tmp;
    while ((tok = strsep(&p, ", \t\n")) && i < FIR_NTAPS) {
        if (!*tok)
            continue;
        if (kstrtou32(tok, 0, &vals[i]))
            break;
        i++;
    }
    if (i >= 1) writel(vals[0] & 0xFFFF, fir->base + REG_COEFF0 + 0);
    if (i >= 2) writel(vals[1] & 0xFFFF, fir->base + REG_COEFF0 + 4);
    if (i >= 3) writel(vals[2] & 0xFFFF, fir->base + REG_COEFF0 + 8);
    if (i >= 4) writel(vals[3] & 0xFFFF, fir->base + REG_COEFF0 + 12);
    mutex_unlock(&fir->coeff_lock);

    kfree(tmp);
    return count;
}
static DEVICE_ATTR_RW(coeff);

/* data_in: write samples (comma/space separated). Special command "reset" to clear. */
static ssize_t data_in_store(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    char *tmp, *p, *tok;
    u32 val;

    tmp = kstrndup(buf, min_t(size_t, count, PAGE_SIZE - 1), GFP_KERNEL);
    if (!tmp)
        return -ENOMEM;

    mutex_lock(&fir->coeff_lock);

    if (sysfs_streq(buf, "reset")) {
        fir->in_pos = 0;
        writel(0, fir->base + REG_LEN);
        mutex_unlock(&fir->coeff_lock);
        kfree(tmp);
        return count;
    }

    p = tmp;
    while ((tok = strsep(&p, ", \t\n")) != NULL) {
        if (!*tok)
            continue;
        if (kstrtou32(tok, 0, &val))
            break;
        if (fir->in_pos >= FIR_MAX_LEN)
            break;
        writel(val & 0xFFFF, fir->base + REG_DATA_IN + fir->in_pos * sizeof(u32));
        fir->in_pos++;
    }
    writel(fir->in_pos, fir->base + REG_LEN);

    mutex_unlock(&fir->coeff_lock);
    kfree(tmp);
    return count;
}
static DEVICE_ATTR_WO(data_in);

/* data_out: read formatted output samples (len lines) */
static ssize_t data_out_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u32 len = readl(fir->base + REG_LEN);
    ssize_t written = 0;
    u32 i;
    if (len > FIR_MAX_LEN) len = FIR_MAX_LEN;
    for (i = 0; i < len && written < PAGE_SIZE - 20; i++) {
        u32 v = readl(fir->base + REG_DATA_OUT + i * sizeof(u32));
        s16 s = (s16)(v & 0xFFFF);
        written += scnprintf(buf + written, PAGE_SIZE - written, "%d%s",
                             s, (i < len - 1) ? ", " : "\n");
    }
    return written;
}
static DEVICE_ATTR_RO(data_out);

/* ctrl: "start" or "reset" or numeric write to CTRL */
static ssize_t ctrl_store(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    unsigned long val;
    unsigned long flags;

    if (sysfs_streq(buf, "reset")) {
        mutex_lock(&fir->coeff_lock);
        fir->in_pos = 0;
        writel(0, fir->base + REG_LEN);
        writel(0x4, fir->base + REG_CTRL);  // RESET pulse
        mutex_unlock(&fir->coeff_lock);
        return count;
    }
    if (sysfs_streq(buf, "start")) {
        spin_lock_irqsave(&fir->status_lock, flags);
        fir->processing = true;
        fir->done = false;
        spin_unlock_irqrestore(&fir->status_lock, flags);
        writel(0x3, fir->base + REG_CTRL);  // EN=1, START=1
        return count;
    }

    if (kstrtoul(buf, 0, &val))
        return -EINVAL;
    writel((u32)val & 0x7, fir->base + REG_CTRL);
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
static const struct attribute_group fir_attr_group = { .attrs = fir_attrs };

static int fir_probe(struct platform_device *pdev)
{
    struct fir_dev *fir;
    struct resource *res;
    int ret;

    fir = devm_kzalloc(&pdev->dev, sizeof(*fir), GFP_KERNEL);
    if (!fir)
        return -ENOMEM;
    fir->dev = &pdev->dev;
    platform_set_drvdata(pdev, fir);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    fir->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(fir->base))
        return PTR_ERR(fir->base);

    fir->irq = platform_get_irq(pdev, 0);
    if (fir->irq < 0)
        return fir->irq;

    mutex_init(&fir->coeff_lock);
    spin_lock_init(&fir->status_lock);
    init_waitqueue_head(&fir->wait);
    fir->processing = false;
    fir->done = false;
    fir->len = 0;
    fir->in_pos = 0;

    ret = devm_request_irq(&pdev->dev, fir->irq, fir_irq_handler,
                           IRQF_SHARED, dev_name(&pdev->dev), fir);
    if (ret)
        return ret;

    fir->miscdev.minor = MISC_DYNAMIC_MINOR;
    fir->miscdev.name  = "fir0";
    fir->miscdev.fops  = &fir_fops;
    fir->miscdev.mode  = 0660;
    ret = misc_register(&fir->miscdev);
    if (ret)
        return ret;

    /* Create sysfs interface */
    ret = sysfs_create_group(&pdev->dev.kobj, &fir_attr_group);
    if (ret) {
        misc_deregister(&fir->miscdev);
        return ret;
    }

    dev_info(&pdev->dev, "FIR IRQ driver probed: irq=%d\n", fir->irq);
    return 0;
}

static int fir_remove(struct platform_device *pdev)
{
    struct fir_dev *fir = platform_get_drvdata(pdev);
    sysfs_remove_group(&pdev->dev.kobj, &fir_attr_group);
    misc_deregister(&fir->miscdev);
    return 0;
}

static const struct of_device_id fir_of_match[] = {
    { .compatible = "acme,fir-q15-irq-v1" },
    {},
};
MODULE_DEVICE_TABLE(of, fir_of_match);

static struct platform_driver fir_driver = {
    .driver = {
        .name = "fir_irq",
        .of_match_table = fir_of_match,
    },
    .probe = fir_probe,
    .remove = fir_remove,
};
module_platform_driver(fir_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FIR filter with DONE interrupt and blocking read");
