// Smart Timer driver with IRQ support - Linux platform driver with sysfs RW attrs

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>

#define CTRL_OFFSET    0x00
#define STATUS_OFFSET  0x04
#define PERIOD_OFFSET  0x08
#define DUTY_OFFSET    0x0C

#define STATUS_WRAP_BIT  (1 << 0)

struct smarttimer_dev {
    struct device *dev;
    void __iomem *base;
    int irq;
    atomic_t irq_count;
};

static irqreturn_t smarttimer_irq_handler(int irq, void *dev_id)
{
    struct smarttimer_dev *stdev = dev_id;
    u32 status;

    status = readl(stdev->base + STATUS_OFFSET);

    if (status & STATUS_WRAP_BIT) {
        atomic_inc(&stdev->irq_count);
        dev_info_ratelimited(stdev->dev, "Timer wrap IRQ #%d\n",
                            atomic_read(&stdev->irq_count));

        // Clear interrupt (W1C)
        writel(STATUS_WRAP_BIT, stdev->base + STATUS_OFFSET);
        return IRQ_HANDLED;
    }

    return IRQ_NONE;
}

// Sysfs: ctrl (RW) — bit0 EN (RW), bit1 RST (W1P)
static ssize_t ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v = readl(st->base + CTRL_OFFSET) & 0x3u;
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    writel(((u32)val) & 0x3u, st->base + CTRL_OFFSET);
    return cnt;
}
static DEVICE_ATTR_RW(ctrl);

// Sysfs: period (RW)
static ssize_t period_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v = readl(st->base + PERIOD_OFFSET);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t period_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    writel((u32)val, st->base + PERIOD_OFFSET);
    return cnt;
}
static DEVICE_ATTR_RW(period);

// Sysfs: duty (RW)
static ssize_t duty_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v = readl(st->base + DUTY_OFFSET);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t duty_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    writel((u32)val, st->base + DUTY_OFFSET);
    return cnt;
}
static DEVICE_ATTR_RW(duty);

// Sysfs: status (RW) — RO fields; W1C bit0 clears WRAP
static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v = readl(st->base + STATUS_OFFSET) & 0x3u;
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    if (((u32)val) & STATUS_WRAP_BIT) {
        // Acknowledge WRAP via W1C
        writel(STATUS_WRAP_BIT, st->base + STATUS_OFFSET);
    }
    return cnt;
}
static DEVICE_ATTR_RW(status);

// Sysfs: irq_count (RO) — increments in ISR
static ssize_t irq_count_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    return scnprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&st->irq_count));
}
static DEVICE_ATTR_RO(irq_count);

static struct attribute *smarttimer_attrs[] = {
    &dev_attr_ctrl.attr,
    &dev_attr_period.attr,
    &dev_attr_duty.attr,
    &dev_attr_status.attr,
    &dev_attr_irq_count.attr,
    NULL,
};
ATTRIBUTE_GROUPS(smarttimer);

static int smarttimer_probe(struct platform_device *pdev)
{
    struct smarttimer_dev *stdev;
    struct resource *res;
    int ret;

    stdev = devm_kzalloc(&pdev->dev, sizeof(*stdev), GFP_KERNEL);
    if (!stdev)
        return -ENOMEM;

    stdev->dev = &pdev->dev;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    stdev->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(stdev->base))
        return PTR_ERR(stdev->base);

    stdev->irq = platform_get_irq(pdev, 0);
    if (stdev->irq < 0) {
        dev_err(&pdev->dev, "Failed to get IRQ\n");
        return stdev->irq;
    }

    atomic_set(&stdev->irq_count, 0);

    ret = devm_request_irq(&pdev->dev, stdev->irq,
                          smarttimer_irq_handler,
                          IRQF_SHARED,
                          dev_name(&pdev->dev),
                          stdev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to request IRQ %d: %d\n",
                stdev->irq, ret);
        return ret;
    }

    platform_set_drvdata(pdev, stdev);

    dev_info(&pdev->dev, "Probed at 0x%lx, IRQ %d\n",
             (unsigned long)res->start, stdev->irq);

    return 0;
}

static const struct of_device_id smarttimer_of_match[] = {
    { .compatible = "acme,smarttimer-irq-v1" },
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
};
module_platform_driver(smarttimer_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Smart Timer with IRQ support");
