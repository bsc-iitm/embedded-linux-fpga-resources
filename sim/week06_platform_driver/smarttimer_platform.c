// Week 6: Smart Timer platform driver — cleaned for teaching
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sysfs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BSES Week 6");
MODULE_DESCRIPTION("Week 6: Smart Timer platform driver (DT-bound, simple sysfs)");

#define OFF_CTRL    0x00u
#define OFF_PERIOD  0x04u
#define OFF_DUTY    0x08u
#define OFF_STATUS  0x0Cu

struct smarttimer_dev {
    void __iomem *base;
    struct device *dev;
};

static ssize_t ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v = readl(st->base + OFF_CTRL) & 0x3u; // only bits [1:0]
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    writel(((u32)val) & 0x3u, st->base + OFF_CTRL);
    return cnt;
}
static DEVICE_ATTR_RW(ctrl);

static ssize_t period_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v = readl(st->base + OFF_PERIOD);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t period_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    writel((u32)val, st->base + OFF_PERIOD);
    return cnt;
}
static DEVICE_ATTR_RW(period);

static ssize_t duty_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v = readl(st->base + OFF_DUTY);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t duty_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    writel((u32)val, st->base + OFF_DUTY);
    return cnt;
}
static DEVICE_ATTR_RW(duty);

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v = readl(st->base + OFF_STATUS) & 0x3u;
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val;
    if (kstrtoul(buf, 0, &val)) return -EINVAL;
    if (((u32)val) & 0x1u) {
        u32 cur = readl(st->base + OFF_STATUS) & ~0x1u; // W1C bit0
        writel(cur, st->base + OFF_STATUS);
    }
    return cnt;
}
static DEVICE_ATTR_RW(status);

static struct attribute *st_attrs[] = {
    &dev_attr_ctrl.attr,
    &dev_attr_period.attr,
    &dev_attr_duty.attr,
    &dev_attr_status.attr,
    NULL,
};
static const struct attribute_group st_attr_group = { .attrs = st_attrs };

static int smarttimer_probe(struct platform_device *pdev)
{
    struct smarttimer_dev *st;
    struct resource *res;
    int rc;

    dev_info(&pdev->dev, "smarttimer_probe: entering\n");

    st = devm_kzalloc(&pdev->dev, sizeof(*st), GFP_KERNEL);
    if (!st) {
        dev_err(&pdev->dev, "smarttimer_probe: kzalloc failed\n");
        return -ENOMEM;
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "smarttimer_probe: no memory resource\n");
        return -ENODEV;
    }
    dev_info(&pdev->dev, "smarttimer_probe: resource %pR\n", res);

    st->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(st->base)) {
        dev_err(&pdev->dev, "smarttimer_probe: ioremap failed: %ld\n",
                PTR_ERR(st->base));
        return PTR_ERR(st->base);
    }
    dev_info(&pdev->dev, "smarttimer_probe: mapped to %p\n", st->base);

    st->dev = &pdev->dev;
    platform_set_drvdata(pdev, st);

    rc = sysfs_create_group(&pdev->dev.kobj, &st_attr_group);
    if (rc) {
        dev_err(&pdev->dev, "smarttimer_probe: sysfs_create_group failed: %d\n", rc);
        return rc;
    }

    dev_info(&pdev->dev, "smarttimer bound successfully: %pR\n", res);
    return 0;
}

static int smarttimer_remove(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "smarttimer_remove: unbinding\n");
    sysfs_remove_group(&pdev->dev.kobj, &st_attr_group);
    return 0;
}

static const struct of_device_id smarttimer_of_match[] = {
    { .compatible = "acme,smarttimer-v1" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, smarttimer_of_match);

static struct platform_driver smarttimer_driver = {
    .probe  = smarttimer_probe,
    .remove = smarttimer_remove,
    .driver = {
        .name = "smarttimer",
        .of_match_table = smarttimer_of_match,
    },
};

module_platform_driver(smarttimer_driver);

