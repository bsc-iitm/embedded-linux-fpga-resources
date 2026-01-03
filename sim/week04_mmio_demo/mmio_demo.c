// Week 4: MMIO-backed smart timer demo (fallback to virtual if phys_base=0)
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BSES Week 4");
MODULE_DESCRIPTION("Week4 MMIO demo: misc dev + sysfs with optional ioremap");

static unsigned long phys_base;
module_param(phys_base, ulong, 0444);
MODULE_PARM_DESC(phys_base, "Physical base of MMIO (0 = virtual registers)");

static unsigned int window_size = 0x1000;
module_param(window_size, uint, 0444);
MODULE_PARM_DESC(window_size, "Size of MMIO window");

#define OFF_CTRL   0x00u
#define OFF_PERIOD 0x04u
#define OFF_DUTY   0x08u
#define OFF_STATUS 0x0Cu

static DEFINE_MUTEX(reg_lock);
static void __iomem *mmio_base;
static struct resource *claimed;

// Virtual fallback state (teaching mode)
static u32 v_ctrl;
static u32 v_period = 0x000000FF;
static u32 v_duty;
static u32 v_status;

static int backend_map(void)
{
    if (!phys_base) return 0;
    claimed = request_mem_region(phys_base, window_size, "smarttimer");
    if (!claimed) return -EBUSY;
    mmio_base = ioremap(phys_base, window_size);
    if (!mmio_base) {
        release_mem_region(phys_base, window_size);
        return -ENOMEM;
    }
    return 0;
}

static void backend_unmap(void)
{
    if (mmio_base) { iounmap(mmio_base); mmio_base = NULL; }
    if (claimed)   { release_mem_region(phys_base, window_size); claimed = NULL; }
}

static inline u32 hw_read(u32 off)
{
    if (mmio_base)
        return readl(mmio_base + off);
    switch (off) {
    case OFF_CTRL:   return v_ctrl & 0x3u; // RAZ/WI for reserved bits
    case OFF_PERIOD: return v_period;
    case OFF_DUTY:   return v_duty;
    case OFF_STATUS: return v_status & 0x3u;
    default:         return 0; // out of range
    }
}

static inline void hw_write(u32 off, u32 val)
{
    // Normalize writes: mask reserved/pulse semantics in software for teaching
    if (off == OFF_CTRL)   val &= 0x3u; // only EN and RST (W1P) exist
    if (off == OFF_STATUS) val &= 0x1u; // only WRAP bit is W1C

    if (mmio_base) {
        if (off == OFF_STATUS) {
            // W1C: read-modify to clear only bits written as 1
            u32 cur = readl(mmio_base + OFF_STATUS);
            cur &= ~val;
            writel(cur, mmio_base + OFF_STATUS);
            return;
        }
        if (off == OFF_CTRL) {
            // W1P for bit1: write 1 triggers, but should read back as 0 later
            u32 cur = readl(mmio_base + OFF_CTRL);
            u32 next = (cur & ~0x3u) | (val & 0x1u); // EN into bit0
            writel(next, mmio_base + OFF_CTRL);
            if (val & BIT(1)) {
                // emulate pulse side-effect: clear status
                writel(0, mmio_base + OFF_STATUS);
            }
            return;
        }
        writel(val, mmio_base + off);
    } else {
        switch (off) {
        case OFF_STATUS:
            if (val & BIT(0)) v_status &= ~BIT(0); // W1C
            break;
        case OFF_CTRL:
            v_ctrl = (v_ctrl & ~0x3u) | (val & 0x1u);
            if (val & BIT(1)) { // W1P
                v_ctrl &= ~BIT(1);
                v_status = 0; // example side effect
            }
            break;
        case OFF_PERIOD:
            v_period = val;
            if (v_ctrl & BIT(0)) v_status |= BIT(1); // UPD_PENDING when enabled
            break;
        case OFF_DUTY:
            v_duty = val;
            if (v_ctrl & BIT(0)) v_status |= BIT(1);
            break;
        }
    }
}

// ----- sysfs attributes -----
static ssize_t ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{ u32 v; mutex_lock(&reg_lock); v = hw_read(OFF_CTRL); mutex_unlock(&reg_lock); return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v); }
static ssize_t ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{ unsigned long val; if (kstrtoul(buf,0,&val)) return -EINVAL; mutex_lock(&reg_lock); hw_write(OFF_CTRL, (u32)val); mutex_unlock(&reg_lock); return cnt; }
static DEVICE_ATTR_RW(ctrl);

static ssize_t period_show(struct device *dev, struct device_attribute *attr, char *buf)
{ u32 v; mutex_lock(&reg_lock); v = hw_read(OFF_PERIOD); mutex_unlock(&reg_lock); return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v); }
static ssize_t period_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{ unsigned long val; if (kstrtoul(buf,0,&val)) return -EINVAL; mutex_lock(&reg_lock); hw_write(OFF_PERIOD, (u32)val); mutex_unlock(&reg_lock); return cnt; }
static DEVICE_ATTR_RW(period);

static ssize_t duty_show(struct device *dev, struct device_attribute *attr, char *buf)
{ u32 v; mutex_lock(&reg_lock); v = hw_read(OFF_DUTY); mutex_unlock(&reg_lock); return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v); }
static ssize_t duty_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{ unsigned long val; if (kstrtoul(buf,0,&val)) return -EINVAL; mutex_lock(&reg_lock); hw_write(OFF_DUTY, (u32)val); mutex_unlock(&reg_lock); return cnt; }
static DEVICE_ATTR_RW(duty);

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{ u32 v; mutex_lock(&reg_lock); v = hw_read(OFF_STATUS); mutex_unlock(&reg_lock); return scnprintf(buf, PAGE_SIZE, "0x%08x\n", v); }
static ssize_t status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t cnt)
{ unsigned long val; if (kstrtoul(buf,0,&val)) return -EINVAL; mutex_lock(&reg_lock); hw_write(OFF_STATUS, (u32)val); mutex_unlock(&reg_lock); return cnt; }
static DEVICE_ATTR_RW(status);

static struct attribute *attrs[] = {
    &dev_attr_ctrl.attr,
    &dev_attr_period.attr,
    &dev_attr_duty.attr,
    &dev_attr_status.attr,
    NULL,
};
static const struct attribute_group attr_group = { .attrs = attrs };

// ----- misc char device (read snapshot) -----
static ssize_t demo_read(struct file *f, char __user *ubuf, size_t len, loff_t *ppos)
{
    char buf[128]; int n; u32 c,p,d,s;
    mutex_lock(&reg_lock);
    c = hw_read(OFF_CTRL); p = hw_read(OFF_PERIOD); d = hw_read(OFF_DUTY); s = hw_read(OFF_STATUS);
    mutex_unlock(&reg_lock);
    n = scnprintf(buf, sizeof(buf), "CTRL=0x%08x PERIOD=0x%08x DUTY=0x%08x STATUS=0x%08x\n", c,p,d,s);
    return simple_read_from_buffer(ubuf, len, ppos, buf, n);
}
static const struct file_operations demo_fops = { .owner = THIS_MODULE, .read = demo_read };
static struct miscdevice demo_misc = { .minor = MISC_DYNAMIC_MINOR, .name = "mmio_demo", .fops = &demo_fops, .mode = 0660 };

static int __init mmio_demo_init(void)
{
    int ret = backend_map();
    if (ret) return ret;
    ret = misc_register(&demo_misc);
    if (ret) { backend_unmap(); return ret; }
    ret = sysfs_create_group(demo_misc.this_device ? &demo_misc.this_device->kobj : NULL, &attr_group);
    if (ret) { misc_deregister(&demo_misc); backend_unmap(); return ret; }
    pr_info("mmio_demo: loaded (phys_base=%#lx)\n", phys_base);
    return 0;
}

static void __exit mmio_demo_exit(void)
{
    if (demo_misc.this_device)
        sysfs_remove_group(&demo_misc.this_device->kobj, &attr_group);
    misc_deregister(&demo_misc);
    backend_unmap();
    pr_info("mmio_demo: unloaded\n");
}

module_init(mmio_demo_init);
module_exit(mmio_demo_exit);

