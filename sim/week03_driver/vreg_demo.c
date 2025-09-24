// Minimal virtual register demo as a Linux kernel module
// Purpose: teaching char devices and sysfs without real hardware

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BSES Week 3");
MODULE_DESCRIPTION("Virtual register demo: /dev + sysfs attributes");

// "Registers" (RAM backed) — mirrors Week 2 names
static u32 reg_ctrl;
static u32 reg_period = 0x000000FF;
static u32 reg_duty;
static u32 reg_status;
static DEFINE_MUTEX(reg_lock);

// Helpers: parse/print u32 as text (accepts decimal or 0x...)
static int parse_u32(const char *buf, size_t count, u32 *out)
{
    int ret;
    unsigned long val;
    char kbuf[32];
    size_t n = min(count, sizeof(kbuf) - 1);
    memcpy(kbuf, buf, n);
    kbuf[n] = '\0';

    // strtoul with base 0: auto 0x... or decimal
    ret = kstrtoul(kbuf, 0, &val);
    if (ret)
        return ret;
    *out = (u32)val;
    return 0;
}

// Character device: read shows a snapshot of all registers as text
static ssize_t vreg_read(struct file *file, char __user *ubuf, size_t len, loff_t *ppos)
{
    char buf[128];
    int n;
    u32 c, p, d, s;

    mutex_lock(&reg_lock);
    c = reg_ctrl; p = reg_period; d = reg_duty; s = reg_status;
    mutex_unlock(&reg_lock);

    n = scnprintf(buf, sizeof(buf),
                  "CTRL=0x%08x PERIOD=0x%08x DUTY=0x%08x STATUS=0x%08x\n",
                  c, p, d, s);

    return simple_read_from_buffer(ubuf, len, ppos, buf, n);
}

// Character device: ignore writes (keep demo simple)
static ssize_t vreg_write(struct file *file, const char __user *ubuf, size_t len, loff_t *ppos)
{
    // No-op; could parse commands here if desired
    return len;
}

static const struct file_operations vreg_fops = {
    .owner = THIS_MODULE,
    .read = vreg_read,
    .write = vreg_write,
//    .llseek = no_llseek,
};

static struct miscdevice vreg_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "vreg_demo",
    .fops  = &vreg_fops,
    .mode  = 0660,
};

// Sysfs attributes
static ssize_t ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u32 v; mutex_lock(&reg_lock); v = reg_ctrl; mutex_unlock(&reg_lock);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    u32 v; int ret = parse_u32(buf, count, &v);
    if (ret) return ret;
    mutex_lock(&reg_lock);
    reg_ctrl = v;
    // Demonstrate a pulse semantics: if bit1 is set (RST pulse), clear immediately
    if (reg_ctrl & BIT(1)) {
        reg_ctrl &= ~BIT(1);
        // Example side effect: clear status
        reg_status = 0;
    }
    mutex_unlock(&reg_lock);
    return count;
}
static DEVICE_ATTR_RW(ctrl);

static ssize_t period_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u32 v; mutex_lock(&reg_lock); v = reg_period; mutex_unlock(&reg_lock);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t period_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    u32 v; int ret = parse_u32(buf, count, &v);
    if (ret) return ret;
    mutex_lock(&reg_lock);
    reg_period = v;
    // Mark update pending (bit1) when enabled (bit0)
    if (reg_ctrl & BIT(0))
        reg_status |= BIT(1);
    mutex_unlock(&reg_lock);
    return count;
}
static DEVICE_ATTR_RW(period);

static ssize_t duty_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u32 v; mutex_lock(&reg_lock); v = reg_duty; mutex_unlock(&reg_lock);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
static ssize_t duty_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    u32 v; int ret = parse_u32(buf, count, &v);
    if (ret) return ret;
    mutex_lock(&reg_lock);
    reg_duty = v;
    if (reg_ctrl & BIT(0))
        reg_status |= BIT(1);
    mutex_unlock(&reg_lock);
    return count;
}
static DEVICE_ATTR_RW(duty);

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u32 v; mutex_lock(&reg_lock); v = reg_status; mutex_unlock(&reg_lock);
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v);
}
// Support W1C for bit0 (clear on write-1); ignore other bits on write
static ssize_t status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    u32 v; int ret = parse_u32(buf, count, &v);
    if (ret) return ret;
    mutex_lock(&reg_lock);
    if (v & BIT(0))
        reg_status &= ~BIT(0);
    mutex_unlock(&reg_lock);
    return count;
}
static DEVICE_ATTR_RW(status);

static struct attribute *vreg_attrs[] = {
    &dev_attr_ctrl.attr,
    &dev_attr_period.attr,
    &dev_attr_duty.attr,
    &dev_attr_status.attr,
    NULL,
};

static const struct attribute_group vreg_attr_group = {
    .attrs = vreg_attrs,
};

static int __init vreg_init(void)
{
    int ret;
    ret = misc_register(&vreg_miscdev);
    if (ret)
        return ret;
    // Create sysfs attributes on the misc device
    ret = sysfs_create_group(&vreg_miscdev.this_device->kobj, &vreg_attr_group);
    if (ret) {
        misc_deregister(&vreg_miscdev);
        return ret;
    }

    pr_info("vreg_demo: loaded (/dev/%s)\n", vreg_miscdev.name);
    return 0;
}

static void __exit vreg_exit(void)
{
    sysfs_remove_group(&vreg_miscdev.this_device->kobj, &vreg_attr_group);
    misc_deregister(&vreg_miscdev);
    pr_info("vreg_demo: unloaded\n");
}

module_init(vreg_init);
module_exit(vreg_exit);

