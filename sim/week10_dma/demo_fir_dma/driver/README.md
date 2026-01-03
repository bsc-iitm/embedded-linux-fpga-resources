# Driver Directory - FIR Filter with DMA

## Purpose

Linux driver for FIR filter with AXI DMA data transfers. Replaces MMIO vector mode with DMA-coherent buffers.

## Files Expected

1. `fir_dma.c` - Platform driver with DMA support
2. `Makefile` - Build configuration

## Key Concepts

### DMA Coherent Buffers

Allocate buffers visible to both CPU and DMA:

```c
struct fir_dev {
    void __iomem *base;           // FIR config registers (COEFF, LEN)
    void __iomem *dma_base;       // AXI DMA registers

    // DMA buffers
    s16 *input_buf;               // Virtual address (CPU access)
    dma_addr_t input_dma;         // Physical address (DMA access)
    s16 *output_buf;              // Virtual address
    dma_addr_t output_dma;        // Physical address

    u32 len;                      // Number of samples
    spinlock_t lock;              // Protects processing flag
    bool processing;              // DMA in progress
    wait_queue_head_t wait;       // Wait for completion
};
```

### Probe Function

```c
static int fir_probe(struct platform_device *pdev)
{
    struct fir_dev *fir;
    struct resource *res;
    int irq, ret;

    fir = devm_kzalloc(&pdev->dev, sizeof(*fir), GFP_KERNEL);
    if (!fir)
        return -ENOMEM;

    // Map FIR config registers
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    fir->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(fir->base))
        return PTR_ERR(fir->base);

    // Map DMA registers
    res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    fir->dma_base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(fir->dma_base))
        return PTR_ERR(fir->dma_base);

    // Allocate DMA-coherent buffers
    fir->input_buf = dma_alloc_coherent(&pdev->dev,
                                         FIR_MAX_LEN * sizeof(s16),
                                         &fir->input_dma, GFP_KERNEL);
    if (!fir->input_buf)
        return -ENOMEM;

    fir->output_buf = dma_alloc_coherent(&pdev->dev,
                                          FIR_MAX_LEN * sizeof(s16),
                                          &fir->output_dma, GFP_KERNEL);
    if (!fir->output_buf) {
        dma_free_coherent(&pdev->dev, FIR_MAX_LEN * sizeof(s16),
                          fir->input_buf, fir->input_dma);
        return -ENOMEM;
    }

    // Initialize synchronization
    spin_lock_init(&fir->lock);
    init_waitqueue_head(&fir->wait);
    fir->processing = false;
    fir->len = 32;  // Default

    // Request DMA completion IRQ
    irq = platform_get_irq(pdev, 0);
    if (irq < 0)
        goto err_free_dma;

    ret = devm_request_irq(&pdev->dev, irq, fir_dma_irq, 0,
                           dev_name(&pdev->dev), fir);
    if (ret)
        goto err_free_dma;

    // Enable DMA channels
    writel(DMACR_RS | DMACR_IOC_IRQ_EN, fir->dma_base + MM2S_DMACR);
    writel(DMACR_RS | DMACR_IOC_IRQ_EN, fir->dma_base + S2MM_DMACR);

    // Create sysfs attributes
    ret = sysfs_create_group(&pdev->dev.kobj, &fir_attr_group);
    if (ret)
        goto err_free_dma;

    platform_set_drvdata(pdev, fir);
    dev_info(&pdev->dev, "FIR DMA driver probed\n");
    return 0;

err_free_dma:
    dma_free_coherent(&pdev->dev, FIR_MAX_LEN * sizeof(s16),
                      fir->output_buf, fir->output_dma);
    dma_free_coherent(&pdev->dev, FIR_MAX_LEN * sizeof(s16),
                      fir->input_buf, fir->input_dma);
    return ret;
}
```

### Start DMA Transfer

```c
static void fir_start_dma(struct fir_dev *fir)
{
    u32 len_bytes = fir->len * sizeof(s16);
    unsigned long flags;

    spin_lock_irqsave(&fir->lock, flags);
    fir->processing = true;
    spin_unlock_irqrestore(&fir->lock, flags);

    // Configure MM2S (memory to FIR)
    writel(fir->input_dma, fir->dma_base + MM2S_SA);
    writel(len_bytes, fir->dma_base + MM2S_LENGTH);  // Triggers transfer

    // Configure S2MM (FIR to memory)
    writel(fir->output_dma, fir->dma_base + S2MM_DA);
    writel(len_bytes, fir->dma_base + S2MM_LENGTH);  // Triggers transfer
}
```

### DMA Completion IRQ Handler

```c
static irqreturn_t fir_dma_irq(int irq, void *dev_id)
{
    struct fir_dev *fir = dev_id;
    u32 status;
    unsigned long flags;

    // Read S2MM status (completion of output transfer)
    status = readl(fir->dma_base + S2MM_DMASR);

    if (!(status & DMASR_IOC_IRQ))
        return IRQ_NONE;  // Not our interrupt

    // Clear interrupt (W1C)
    writel(DMASR_IOC_IRQ, fir->dma_base + S2MM_DMASR);

    // Mark processing complete
    spin_lock_irqsave(&fir->lock, flags);
    fir->processing = false;
    spin_unlock_irqrestore(&fir->lock, flags);

    // Wake waiting readers
    wake_up_interruptible(&fir->wait);

    return IRQ_HANDLED;
}
```

## Sysfs Attributes

### coefficients (write)

```c
static ssize_t coefficients_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    s16 coeff[4];
    int ret;

    ret = sscanf(buf, "%hd %hd %hd %hd",
                 &coeff[0], &coeff[1], &coeff[2], &coeff[3]);
    if (ret != 4)
        return -EINVAL;

    // Write to FIR config registers (AXI-Lite)
    writel(coeff[0], fir->base + FIR_COEFF0);
    writel(coeff[1], fir->base + FIR_COEFF1);
    writel(coeff[2], fir->base + FIR_COEFF2);
    writel(coeff[3], fir->base + FIR_COEFF3);

    return count;
}
static DEVICE_ATTR_WO(coefficients);
```

### input_data (write)

```c
static ssize_t input_data_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    char *buf_copy, *token;
    int i = 0;

    buf_copy = kstrdup(buf, GFP_KERNEL);
    if (!buf_copy)
        return -ENOMEM;

    // Parse space-separated Q15 values
    while ((token = strsep(&buf_copy, " \n")) && i < FIR_MAX_LEN) {
        if (kstrtos16(token, 0, &fir->input_buf[i]))
            continue;
        i++;
    }

    kfree(buf_copy);
    return count;
}
static DEVICE_ATTR_WO(input_data);
```

### output_data (read, blocking)

```c
static ssize_t output_data_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    ssize_t len = 0;
    int i;

    // Block until DMA completes
    if (wait_event_interruptible(fir->wait, !fir->processing))
        return -ERESTARTSYS;

    // Copy from DMA buffer (no readl needed)
    for (i = 0; i < fir->len; i++)
        len += scnprintf(buf + len, PAGE_SIZE - len,
                         "%d ", fir->output_buf[i]);

    if (len > 0)
        buf[len - 1] = '\n';

    return len;
}
static DEVICE_ATTR_RO(output_data);
```

### start (write)

```c
static ssize_t start_store(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);

    fir_start_dma(fir);
    return count;
}
static DEVICE_ATTR_WO(start);
```

### len (read/write)

```c
static ssize_t len_show(struct device *dev,
                        struct device_attribute *attr,
                        char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", fir->len);
}

static ssize_t len_store(struct device *dev,
                         struct device_attribute *attr,
                         const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u32 val;

    if (kstrtou32(buf, 0, &val) || val > FIR_MAX_LEN)
        return -EINVAL;

    fir->len = val;
    writel(val, fir->base + FIR_LEN);

    return count;
}
static DEVICE_ATTR_RW(len);
```

## Remove Function

```c
static int fir_remove(struct platform_device *pdev)
{
    struct fir_dev *fir = platform_get_drvdata(pdev);

    sysfs_remove_group(&pdev->dev.kobj, &fir_attr_group);

    // Stop DMA
    writel(0, fir->dma_base + MM2S_DMACR);
    writel(0, fir->dma_base + S2MM_DMACR);

    // Free DMA buffers
    dma_free_coherent(&pdev->dev, FIR_MAX_LEN * sizeof(s16),
                      fir->input_buf, fir->input_dma);
    dma_free_coherent(&pdev->dev, FIR_MAX_LEN * sizeof(s16),
                      fir->output_buf, fir->output_dma);

    return 0;
}
```

## Platform Driver Structure

```c
static const struct of_device_id fir_dma_of_match[] = {
    { .compatible = "acme,fir-dma-v1", },
    { /* end */ }
};
MODULE_DEVICE_TABLE(of, fir_dma_of_match);

static struct platform_driver fir_dma_driver = {
    .probe  = fir_probe,
    .remove = fir_remove,
    .driver = {
        .name = "fir-dma",
        .of_match_table = fir_dma_of_match,
    },
};
module_platform_driver(fir_dma_driver);
```

## Makefile

```makefile
obj-m := fir_dma.o

KDIR ?= /path/to/linux

all:
	make -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

clean:
	make -C $(KDIR) M=$(PWD) clean
```

## Testing

```bash
insmod fir_dma.ko
echo "8192 16384 16384 8192" > /sys/devices/platform/fir/coefficients
echo "0 0 0 0 0 0 0 0 0 0 32767 0 ..." > /sys/devices/platform/fir/input_data
echo 32 > /sys/devices/platform/fir/len
echo 1 > /sys/devices/platform/fir/start
cat /sys/devices/platform/fir/output_data  # Blocks until DMA complete
```

## Key Differences from Week 9

| Aspect                | Week 9 (IRQ)                | Week 10 (DMA)                  |
|-----------------------|-----------------------------|--------------------------------|
| Data input            | writel(DATA_IN[i])          | DMA reads from input_buf       |
| Data output           | readl(DATA_OUT[i])          | DMA writes to output_buf       |
| Buffer allocation     | N/A                         | dma_alloc_coherent()           |
| Address type          | Virtual (ioremap)           | Physical (dma_addr_t)          |
| IRQ source            | FIR STATUS.DONE             | DMA S2MM completion            |
| CPU overhead          | High (64 MMIO ops)          | Minimal (6 DMA config ops)     |

## References

- Linux DMA API: `Documentation/DMA-API.txt`
- Week 9 driver (interrupt-driven MMIO)
- Xilinx AXI DMA driver: `drivers/dma/xilinx/xilinx_dma.c`
