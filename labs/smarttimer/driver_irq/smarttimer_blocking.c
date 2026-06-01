// Smart Timer blocking-read driver (Week 9)
// Platform driver + misc char device for blocking read on timer wrap

#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>

#define CTRL_OFFSET 0x00
#define STATUS_OFFSET 0x04
#define PERIOD_OFFSET 0x08
#define DUTY_OFFSET 0x0C

#define STATUS_WRAP_BIT (1u << 0)

struct smarttimer_dev {
    struct device* dev;
    void __iomem* base;
    int irq;

    wait_queue_head_t wait;  // for blocking read
    atomic_t wrap_count;     // increments per wrap

    struct miscdevice miscdev;  // char device
};

static irqreturn_t smarttimer_irq_handler(int irq, void* dev_id) {
    struct smarttimer_dev* st = dev_id;
    u32 status = readl(st->base + STATUS_OFFSET);

    if (!(status & STATUS_WRAP_BIT))
        return IRQ_NONE;

    // Ack source and bump count, then wake sleepers
    writel(STATUS_WRAP_BIT, st->base + STATUS_OFFSET);
    atomic_inc(&st->wrap_count);
    wake_up_interruptible(&st->wait);
    dev_info_ratelimited(st->dev, "wrap IRQ, count=%d\n", 
        atomic_read(&st->wrap_count));
    return IRQ_HANDLED;
}

// ---------- sysfs (reuse Week 8 attributes) ----------

static ssize_t ctrl_show(struct device* dev, struct device_attribute* attr, char* buf) {
    struct smarttimer_dev* st = dev_get_drvdata(dev);
    u32 v = readl(st->base + CTRL_OFFSET) & 0x3u;
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t ctrl_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t cnt) {
    struct smarttimer_dev* st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    writel(((u32)val) & 0x3u, st->base + CTRL_OFFSET);
    return cnt;
}
static DEVICE_ATTR_RW(ctrl);

static ssize_t period_show(struct device* dev, struct device_attribute* attr, char* buf) {
    struct smarttimer_dev* st = dev_get_drvdata(dev);
    u32 v = readl(st->base + PERIOD_OFFSET);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t period_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t cnt) {
    struct smarttimer_dev* st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    writel((u32)val, st->base + PERIOD_OFFSET);
    return cnt;
}
static DEVICE_ATTR_RW(period);

static ssize_t duty_show(struct device* dev, struct device_attribute* attr, char* buf) {
    struct smarttimer_dev* st = dev_get_drvdata(dev);
    u32 v = readl(st->base + DUTY_OFFSET);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t duty_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t cnt) {
    struct smarttimer_dev* st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    writel((u32)val, st->base + DUTY_OFFSET);
    return cnt;
}
static DEVICE_ATTR_RW(duty);

static ssize_t status_show(struct device* dev, struct device_attribute* attr, char* buf) {
    struct smarttimer_dev* st = dev_get_drvdata(dev);
    u32 v = readl(st->base + STATUS_OFFSET) & 0x3u;
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t status_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t cnt) {
    struct smarttimer_dev* st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    if (((u32)val) & STATUS_WRAP_BIT)
        writel(STATUS_WRAP_BIT, st->base + STATUS_OFFSET);  // W1C ack
    return cnt;
}
static DEVICE_ATTR_RW(status);

static ssize_t irq_count_show(struct device* dev, struct device_attribute* attr, char* buf) {
    struct smarttimer_dev* st = dev_get_drvdata(dev);
    return scnprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&st->wrap_count));
}
static DEVICE_ATTR_RO(irq_count);

static struct attribute* smarttimer_attrs[] = {
    &dev_attr_ctrl.attr,
    &dev_attr_period.attr,
    &dev_attr_duty.attr,
    &dev_attr_status.attr,
    &dev_attr_irq_count.attr,
    NULL,
};
ATTRIBUTE_GROUPS(smarttimer);

// ---------- misc device (blocking read) ----------

static int st_open(struct inode* inode, struct file* file) {
    struct smarttimer_dev* st = container_of(file->private_data, struct smarttimer_dev, miscdev);
    file->private_data = st;
    return 0;
}

static ssize_t st_read(struct file* file, char __user* ubuf, size_t len, loff_t* ppos) {
    struct smarttimer_dev* st = file->private_data;
    int target = atomic_read(&st->wrap_count) + 1;
    char out[4] = "1\n";  // minimal payload

    if (wait_event_interruptible(st->wait, atomic_read(&st->wrap_count) >= target))
        return -ERESTARTSYS;

    return simple_read_from_buffer(ubuf, len, ppos, out, 2);
}

static const struct file_operations st_fops = {
    .owner = THIS_MODULE,
    .open = st_open,
    .read = st_read,
    .llseek = no_llseek,
};

// ---------- platform glue ----------

static int smarttimer_probe(struct platform_device* pdev) {
    struct smarttimer_dev* st;
    struct resource* res;
    int ret;

    st = devm_kzalloc(&pdev->dev, sizeof(*st), GFP_KERNEL);
    if (!st)
        return -ENOMEM;
    st->dev = &pdev->dev;
    platform_set_drvdata(pdev, st);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "no memory resource in device tree\n");
        return -ENODEV;
    }

    st->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(st->base)) {
        dev_err(&pdev->dev, "ioremap failed\n");
        return PTR_ERR(st->base);
    }

    st->irq = platform_get_irq(pdev, 0);
    if (st->irq < 0) {
        dev_err(&pdev->dev, "IRQ not connected - use smarttimer platform driver instead\n");
        return st->irq;
    }

    init_waitqueue_head(&st->wait);
    atomic_set(&st->wrap_count, 0);

    ret = devm_request_irq(&pdev->dev, st->irq, smarttimer_irq_handler,
                           IRQF_SHARED, dev_name(&pdev->dev), st);
    if (ret) {
        dev_err(&pdev->dev, "failed to request IRQ %d: %d\n", st->irq, ret);
        return ret;
    }

    st->miscdev.minor = MISC_DYNAMIC_MINOR;
    st->miscdev.name = "smarttimer0";
    st->miscdev.fops = &st_fops;
    st->miscdev.mode = 0660;

    ret = misc_register(&st->miscdev);
    if (ret) {
        dev_err(&pdev->dev, "misc_register failed: %d\n", ret);
        return ret;
    }

    dev_info(&pdev->dev, "SmartTimer blocking driver probed: base=%pR, irq=%d\n",
             res, st->irq);
    return 0;
}

static int smarttimer_remove(struct platform_device* pdev) {
    struct smarttimer_dev* st = platform_get_drvdata(pdev);
    misc_deregister(&st->miscdev);
    return 0;
}

static const struct of_device_id smarttimer_of_match[] = {
    {.compatible = "acme,smarttimer-v1"},
    {},
};
MODULE_DEVICE_TABLE(of, smarttimer_of_match);

static struct platform_driver smarttimer_driver = {
    .driver = {
        .name = "smarttimer",
        .of_match_table = smarttimer_of_match,
        .dev_groups = smarttimer_groups,
    },
    .probe = smarttimer_probe,
    .remove = smarttimer_remove,
};

static int __init smarttimer_init(void)
{
    int ret;

    pr_info("smarttimer_blocking: loading driver (compatible: \"%s\")\n",
            smarttimer_of_match[0].compatible);

    ret = platform_driver_register(&smarttimer_driver);
    if (ret) {
        pr_err("smarttimer_blocking: platform_driver_register failed: %d\n", ret);
        return ret;
    }

    pr_info("smarttimer_blocking: driver registered, waiting for DT match\n");
    return 0;
}
module_init(smarttimer_init);

static void __exit smarttimer_exit(void)
{
    platform_driver_unregister(&smarttimer_driver);
    pr_info("smarttimer_blocking: driver unloaded\n");
}
module_exit(smarttimer_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Smart Timer blocking-read driver (wait queue)");
