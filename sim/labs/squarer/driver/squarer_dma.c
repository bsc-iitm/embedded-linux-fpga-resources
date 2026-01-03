// Squarer DMA Driver
// Demonstrates bulk DMA transfer (fast path)
//
// Usage:
//   write(fd, input_array, n * sizeof(int16_t))  - provide input samples
//   read(fd, output_array, n * sizeof(int32_t))  - trigger DMA and read results
//
// Each read triggers 1 DMA transfer (just a few register writes)

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>

#define DRV_NAME "squarer_dma"
#define MAX_SAMPLES (256 * 1024)  // 256K samples: 512KB input, 1MB output

// AXI DMA register offsets
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

struct squarer_dma_dev {
    void __iomem *dma_base;
    struct miscdevice misc;
    struct mutex lock;

    // DMA coherent buffers
    s16 *input_buf;
    dma_addr_t input_dma;
    s32 *output_buf;
    dma_addr_t output_dma;

    size_t count;
    bool transfer_done;
    wait_queue_head_t wait;
};

static irqreturn_t squarer_dma_irq(int irq, void *data)
{
    struct squarer_dma_dev *dev = data;
    u32 status = readl(dev->dma_base + S2MM_DMASR);

    if (!(status & DMASR_IOC_IRQ))
        return IRQ_NONE;

    // Clear interrupt
    writel(DMASR_IOC_IRQ, dev->dma_base + S2MM_DMASR);

    dev->transfer_done = true;
    wake_up_interruptible(&dev->wait);
    return IRQ_HANDLED;
}

static void start_dma_transfer(struct squarer_dma_dev *dev, size_t count)
{
    u32 in_bytes = count * sizeof(s16);
    u32 out_bytes = count * sizeof(s32);

    dev->transfer_done = false;

    // MM2S: memory -> squarer (16-bit input)
    writel((u32)dev->input_dma, dev->dma_base + MM2S_SA);
    writel(in_bytes, dev->dma_base + MM2S_LENGTH);

    // S2MM: squarer -> memory (32-bit output)
    writel((u32)dev->output_dma, dev->dma_base + S2MM_DA);
    writel(out_bytes, dev->dma_base + S2MM_LENGTH);
}

static ssize_t squarer_write(struct file *file, const char __user *buf,
                             size_t len, loff_t *off)
{
    struct squarer_dma_dev *dev = container_of(file->private_data,
                                    struct squarer_dma_dev, misc);
    size_t count = len / sizeof(s16);

    if (count == 0 || count > MAX_SAMPLES)
        return -EINVAL;

    mutex_lock(&dev->lock);

    if (copy_from_user(dev->input_buf, buf, count * sizeof(s16))) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }
    dev->count = count;

    mutex_unlock(&dev->lock);
    return count * sizeof(s16);
}

static ssize_t squarer_read(struct file *file, char __user *buf,
                            size_t len, loff_t *off)
{
    struct squarer_dma_dev *dev = container_of(file->private_data,
                                    struct squarer_dma_dev, misc);
    size_t out_bytes;
    int ret;

    mutex_lock(&dev->lock);

    if (dev->count == 0) {
        mutex_unlock(&dev->lock);
        return 0;
    }

    out_bytes = dev->count * sizeof(s32);
    if (len < out_bytes)
        out_bytes = (len / sizeof(s32)) * sizeof(s32);

    // Start DMA transfer
    start_dma_transfer(dev, out_bytes / sizeof(s32));

    // Wait for completion
    ret = wait_event_interruptible_timeout(dev->wait, dev->transfer_done,
                                           msecs_to_jiffies(1000));
    if (ret == 0) {
        mutex_unlock(&dev->lock);
        return -ETIMEDOUT;
    }
    if (ret < 0) {
        mutex_unlock(&dev->lock);
        return ret;
    }

    if (copy_to_user(buf, dev->output_buf, out_bytes)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    mutex_unlock(&dev->lock);
    return out_bytes;
}

static const struct file_operations squarer_fops = {
    .owner = THIS_MODULE,
    .write = squarer_write,
    .read  = squarer_read,
};

static int squarer_dma_probe(struct platform_device *pdev)
{
    struct squarer_dma_dev *dev;
    struct resource *res;
    int irq, ret;

    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    // Map DMA registers
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    dev->dma_base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(dev->dma_base))
        return PTR_ERR(dev->dma_base);

    // Allocate DMA coherent buffers
    dev->input_buf = dma_alloc_coherent(&pdev->dev,
                        MAX_SAMPLES * sizeof(s16), &dev->input_dma, GFP_KERNEL);
    if (!dev->input_buf)
        return -ENOMEM;

    dev->output_buf = dma_alloc_coherent(&pdev->dev,
                        MAX_SAMPLES * sizeof(s32), &dev->output_dma, GFP_KERNEL);
    if (!dev->output_buf) {
        dma_free_coherent(&pdev->dev, MAX_SAMPLES * sizeof(s16),
                          dev->input_buf, dev->input_dma);
        return -ENOMEM;
    }

    mutex_init(&dev->lock);
    init_waitqueue_head(&dev->wait);

    // Enable DMA channels
    writel(DMACR_RS | DMACR_IOC_IRQ_EN, dev->dma_base + MM2S_DMACR);
    writel(DMACR_RS | DMACR_IOC_IRQ_EN, dev->dma_base + S2MM_DMACR);

    // Request IRQ
    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        ret = irq;
        goto err_free_dma;
    }

    ret = devm_request_irq(&pdev->dev, irq, squarer_dma_irq, 0, DRV_NAME, dev);
    if (ret)
        goto err_free_dma;

    dev->misc.minor = MISC_DYNAMIC_MINOR;
    dev->misc.name = "squarer_dma";
    dev->misc.fops = &squarer_fops;

    ret = misc_register(&dev->misc);
    if (ret)
        goto err_free_dma;

    platform_set_drvdata(pdev, dev);
    dev_info(&pdev->dev, "squarer_dma: registered /dev/squarer_dma\n");
    return 0;

err_free_dma:
    dma_free_coherent(&pdev->dev, MAX_SAMPLES * sizeof(s32),
                      dev->output_buf, dev->output_dma);
    dma_free_coherent(&pdev->dev, MAX_SAMPLES * sizeof(s16),
                      dev->input_buf, dev->input_dma);
    return ret;
}

static int squarer_dma_remove(struct platform_device *pdev)
{
    struct squarer_dma_dev *dev = platform_get_drvdata(pdev);

    misc_deregister(&dev->misc);
    dma_free_coherent(&pdev->dev, MAX_SAMPLES * sizeof(s32),
                      dev->output_buf, dev->output_dma);
    dma_free_coherent(&pdev->dev, MAX_SAMPLES * sizeof(s16),
                      dev->input_buf, dev->input_dma);
    return 0;
}

static const struct of_device_id squarer_dma_of_match[] = {
    { .compatible = "demo,squarer-dma" },
    { }
};
MODULE_DEVICE_TABLE(of, squarer_dma_of_match);

static struct platform_driver squarer_dma_driver = {
    .driver = {
        .name = DRV_NAME,
        .of_match_table = squarer_dma_of_match,
    },
    .probe  = squarer_dma_probe,
    .remove = squarer_dma_remove,
};
module_platform_driver(squarer_dma_driver);

MODULE_AUTHOR("Demo");
MODULE_DESCRIPTION("Squarer DMA driver - bulk transfer");
MODULE_LICENSE("GPL");
