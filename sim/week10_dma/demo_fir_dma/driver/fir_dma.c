// FIR Filter with AXI DMA demo driver (Week 10)
// - Maps FIR config (AXI-Lite) and AXI DMA regs
// - Allocates coherent input/output buffers
// - Sysfs interface to set coeffs, len, input_data, start, output_data
// - Uses S2MM IOC interrupt to signal completion

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/slab.h>

#define DRV_NAME "fir_dma"

// FIR register offsets (AXI-Lite)
#define FIR_CTRL     0x000
#define FIR_STATUS   0x004
#define FIR_LEN      0x008
#define FIR_COEFF0   0x010
#define FIR_COEFF1   0x014
#define FIR_COEFF2   0x018
#define FIR_COEFF3   0x01C

// AXI DMA registers
#define MM2S_DMACR   0x00
#define MM2S_DMASR   0x04
#define MM2S_SA      0x18
#define MM2S_LENGTH  0x28
#define S2MM_DMACR   0x30
#define S2MM_DMASR   0x34
#define S2MM_DA      0x48
#define S2MM_LENGTH  0x58

#define DMACR_RS         0x00000001
#define DMACR_IOC_IRQ_EN 0x00001000
#define DMASR_IOC_IRQ    0x00001000

#define FIR_MAX_LEN 1024

struct fir_dev {
    struct device *dev;
    void __iomem *base;     // FIR config regs
    void __iomem *dma_base; // AXI DMA regs

    // DMA coherent buffers
    s16 *input_buf;
    dma_addr_t input_dma;
    s16 *output_buf;
    dma_addr_t output_dma;

    u32 len;

    spinlock_t lock;   // protects processing
    bool processing;
    wait_queue_head_t wait;

    struct mutex io_lock; // protects sysfs parsing
};

static irqreturn_t fir_dma_irq(int irq, void *dev_id)
{
    struct fir_dev *fir = dev_id;
    u32 s = readl(fir->dma_base + S2MM_DMASR);
    if (!(s & DMASR_IOC_IRQ))
        return IRQ_NONE;
    writel(DMASR_IOC_IRQ, fir->dma_base + S2MM_DMASR);

    spin_lock(&fir->lock);
    fir->processing = false;
    spin_unlock(&fir->lock);

    wake_up_interruptible(&fir->wait);
    return IRQ_HANDLED;
}

static void fir_start_dma(struct fir_dev *fir)
{
    unsigned long flags;
    u32 len_bytes = fir->len * sizeof(s16);

    spin_lock_irqsave(&fir->lock, flags);
    fir->processing = true;
    spin_unlock_irqrestore(&fir->lock, flags);

    // MM2S: memory -> FIR
    writel(fir->input_dma,  fir->dma_base + MM2S_SA);
    writel(len_bytes,       fir->dma_base + MM2S_LENGTH);

    // S2MM: FIR -> memory
    writel(fir->output_dma, fir->dma_base + S2MM_DA);
    writel(len_bytes,       fir->dma_base + S2MM_LENGTH);
}

// Sysfs: coefficients (write-only)
static ssize_t coefficients_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    s16 c0, c1, c2, c3;
    int n;
    mutex_lock(&fir->io_lock);
    n = sscanf(buf, "%hd %hd %hd %hd", &c0, &c1, &c2, &c3);
    if (n != 4) {
        mutex_unlock(&fir->io_lock);
        return -EINVAL;
    }
    writel((u16)c0, fir->base + FIR_COEFF0);
    writel((u16)c1, fir->base + FIR_COEFF1);
    writel((u16)c2, fir->base + FIR_COEFF2);
    writel((u16)c3, fir->base + FIR_COEFF3);
    mutex_unlock(&fir->io_lock);
    return count;
}
static DEVICE_ATTR_WO(coefficients);

// Sysfs: len (RW)
static ssize_t len_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    return scnprintf(buf, PAGE_SIZE, "%u\n", fir->len);
}

static ssize_t len_store(struct device *dev,
                         struct device_attribute *attr,
                         const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    unsigned int val;
    if (kstrtouint(buf, 0, &val))
        return -EINVAL;
    if (val == 0 || val > FIR_MAX_LEN)
        return -EINVAL;
    fir->len = val;
    writel(val, fir->base + FIR_LEN);
    return count;
}
static DEVICE_ATTR_RW(len);

// Sysfs: input_data (write-only), space/comma separated int16
static ssize_t input_data_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    const char *p = buf;
    int i = 0;
    long v;
    mutex_lock(&fir->io_lock);
    while (*p && i < FIR_MAX_LEN) {
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\t') p++;
        if (!*p) break;
        if (kstrtol(p, 0, &v)) break;
        fir->input_buf[i++] = (s16)v;
        while (*p && *p != ' ' && *p != ',' && *p != '\n' && *p != '\t') p++;
    }
    if (i > 0)
        fir->len = i;
    mutex_unlock(&fir->io_lock);
    return count;
}
static DEVICE_ATTR_WO(input_data);

// Sysfs: start (write-only, any nonzero -> start DMA)
static ssize_t start_store(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    unsigned long v;
    if (kstrtoul(buf, 0, &v))
        return -EINVAL;
    if (v) {
        fir_start_dma(fir);
    }
    return count;
}
static DEVICE_ATTR_WO(start);

// Sysfs: output_data (read-only, blocks until DMA completes)
static ssize_t output_data_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    size_t off = 0;
    int i;
    if (wait_event_interruptible(fir->wait, !READ_ONCE(fir->processing)))
        return -ERESTARTSYS;
    for (i = 0; i < fir->len; i++) {
        off += scnprintf(buf + off, PAGE_SIZE - off, "%d ", fir->output_buf[i]);
        if (off >= PAGE_SIZE - 8) break;
    }
    off += scnprintf(buf + off, PAGE_SIZE - off, "\n");
    return off;
}
static DEVICE_ATTR_RO(output_data);

static struct attribute *fir_attrs[] = {
    &dev_attr_coefficients.attr,
    &dev_attr_len.attr,
    &dev_attr_input_data.attr,
    &dev_attr_start.attr,
    &dev_attr_output_data.attr,
    NULL,
};

static const struct attribute_group fir_attr_group = {
    .attrs = fir_attrs,
};

static int fir_probe(struct platform_device *pdev)
{
    struct fir_dev *fir;
    struct resource *res;
    int irq, ret;

    fir = devm_kzalloc(&pdev->dev, sizeof(*fir), GFP_KERNEL);
    if (!fir)
        return -ENOMEM;
    fir->dev = &pdev->dev;

    // Map FIR config
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    fir->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(fir->base))
        return PTR_ERR(fir->base);

    // Map DMA regs
    res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    fir->dma_base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(fir->dma_base))
        return PTR_ERR(fir->dma_base);

    // Coherent buffers
    fir->input_buf = dma_alloc_coherent(&pdev->dev,
                        FIR_MAX_LEN * sizeof(s16), &fir->input_dma, GFP_KERNEL);
    if (!fir->input_buf)
        return -ENOMEM;
    fir->output_buf = dma_alloc_coherent(&pdev->dev,
                        FIR_MAX_LEN * sizeof(s16), &fir->output_dma, GFP_KERNEL);
    if (!fir->output_buf) {
        dma_free_coherent(&pdev->dev, FIR_MAX_LEN * sizeof(s16),
                          fir->input_buf, fir->input_dma);
        return -ENOMEM;
    }

    spin_lock_init(&fir->lock);
    init_waitqueue_head(&fir->wait);
    mutex_init(&fir->io_lock);
    fir->processing = false;
    fir->len = 32;

    // Enable DMA channels + IRQs
    writel(DMACR_RS | DMACR_IOC_IRQ_EN, fir->dma_base + MM2S_DMACR);
    writel(DMACR_RS | DMACR_IOC_IRQ_EN, fir->dma_base + S2MM_DMACR);

    // IRQ (assume S2MM IRQ provided)
    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        ret = irq;
        goto err_dma;
    }
    ret = devm_request_irq(&pdev->dev, irq, fir_dma_irq, 0, DRV_NAME, fir);
    if (ret)
        goto err_dma;

    // Default enable FIR core
    writel(1, fir->base + FIR_CTRL);
    writel(fir->len, fir->base + FIR_LEN);

    ret = sysfs_create_group(&pdev->dev.kobj, &fir_attr_group);
    if (ret)
        goto err_dma;

    platform_set_drvdata(pdev, fir);
    dev_info(&pdev->dev, "%s: probed\n", DRV_NAME);
    return 0;

err_dma:
    dma_free_coherent(&pdev->dev, FIR_MAX_LEN * sizeof(s16),
                      fir->output_buf, fir->output_dma);
    dma_free_coherent(&pdev->dev, FIR_MAX_LEN * sizeof(s16),
                      fir->input_buf, fir->input_dma);
    return ret;
}

static int fir_remove(struct platform_device *pdev)
{
    struct fir_dev *fir = platform_get_drvdata(pdev);
    sysfs_remove_group(&pdev->dev.kobj, &fir_attr_group);
    dma_free_coherent(&pdev->dev, FIR_MAX_LEN * sizeof(s16),
                      fir->output_buf, fir->output_dma);
    dma_free_coherent(&pdev->dev, FIR_MAX_LEN * sizeof(s16),
                      fir->input_buf, fir->input_dma);
    return 0;
}

static const struct of_device_id fir_of_match[] = {
    { .compatible = "acme,fir-dma-v1" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, fir_of_match);

static struct platform_driver fir_driver = {
    .driver = {
        .name = DRV_NAME,
        .of_match_table = fir_of_match,
    },
    .probe  = fir_probe,
    .remove = fir_remove,
};

module_platform_driver(fir_driver);

MODULE_AUTHOR("BS-ES Week10 Demo");
MODULE_DESCRIPTION("FIR filter with AXI DMA demo driver");
MODULE_LICENSE("GPL");

