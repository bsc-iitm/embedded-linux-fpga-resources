// Week 5: Smart Timer platform driver with DT autoload capability
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BSES Week 5");
MODULE_DESCRIPTION("Week 5: Smart Timer platform driver (DT-bound, minimal sysfs)");

#define OFF_CTRL    0x00u
#define OFF_PERIOD  0x04u
#define OFF_DUTY    0x08u
#define OFF_STATUS  0x0Cu

struct smarttimer_dev {
    void __iomem *base;
    struct device *dev;
    struct mutex lock; // serialize sysfs accesses
};

static inline u32 st_r32(struct smarttimer_dev *st, u32 off)
{ return readl(st->base + off); }

static inline void st_w32(struct smarttimer_dev *st, u32 off, u32 val)
{ writel(val, st->base + off); }

static ssize_t ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v; mutex_lock(&st->lock); v = st_r32(st, OFF_CTRL) & 0x3u; mutex_unlock(&st->lock);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val; if (kstrtoul(buf, 0, &val)) return -EINVAL;
    mutex_lock(&st->lock);
    st_w32(st, OFF_CTRL, (u32)val & 0x3u); // only EN/RST (RST semantics depend on HW)
    mutex_unlock(&st->lock);
    return cnt;
}
static DEVICE_ATTR_RW(ctrl);

static ssize_t period_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v; mutex_lock(&st->lock); v = st_r32(st, OFF_PERIOD); mutex_unlock(&st->lock);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t period_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val; if (kstrtoul(buf, 0, &val)) return -EINVAL;
    mutex_lock(&st->lock); st_w32(st, OFF_PERIOD, (u32)val); mutex_unlock(&st->lock);
    return cnt;
}
static DEVICE_ATTR_RW(period);

static ssize_t duty_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v; mutex_lock(&st->lock); v = st_r32(st, OFF_DUTY); mutex_unlock(&st->lock);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t duty_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val; if (kstrtoul(buf, 0, &val)) return -EINVAL;
    mutex_lock(&st->lock); st_w32(st, OFF_DUTY, (u32)val); mutex_unlock(&st->lock);
    return cnt;
}
static DEVICE_ATTR_RW(duty);

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    u32 v; mutex_lock(&st->lock); v = st_r32(st, OFF_STATUS) & 0x3u; mutex_unlock(&st->lock);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{
    struct smarttimer_dev *st = dev_get_drvdata(dev);
    unsigned long val; if (kstrtoul(buf, 0, &val)) return -EINVAL;
    // W1C on bit0 if HW implements it; perform a read-modify-clear pattern
    mutex_lock(&st->lock);
    if (((u32)val) & 0x1u) {
        u32 cur = st_r32(st, OFF_STATUS);
        cur &= ~0x1u;
        st_w32(st, OFF_STATUS, cur);
    }
    mutex_unlock(&st->lock);
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

    st = devm_kzalloc(&pdev->dev, sizeof(*st), GFP_KERNEL);
    if (!st) return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    st->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(st->base)) return PTR_ERR(st->base);

    st->dev = &pdev->dev;
    mutex_init(&st->lock);
    platform_set_drvdata(pdev, st);

    rc = sysfs_create_group(&pdev->dev.kobj, &st_attr_group);
    if (rc) return rc;

    dev_info(&pdev->dev, "smarttimer bound: %pR\n", res);
    return 0;
}

static int smarttimer_remove(struct platform_device *pdev)
{
    sysfs_remove_group(&pdev->dev.kobj, &st_attr_group);
    return 0; // devm_* handles unmap/free
}

static const struct of_device_id smarttimer_of_match[] = {
    { .compatible = "acme,smart-timer-v1" },
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

