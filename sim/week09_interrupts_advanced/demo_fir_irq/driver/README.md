This driver exposes the FIR as a blocking char device using an interrupt on DONE.

Concepts
- Spinlock protects flags shared with the IRQ handler (`done`, `processing`).
- Mutex protects coefficient updates (process-only path).
- IRQ handler clears W1C DONE, sets `done = true`, and wakes the wait queue.
- `/dev/fir0` read blocks until DONE, then copies DATA_OUT[] to userspace.

Minimal logic sketch
```c
// IRQ: clear W1C DONE, set flag, wake
spin_lock(&status_lock); done = true; processing = false; spin_unlock(&status_lock);
writel(STATUS_DONE_BIT, base + REG_STATUS);
wake_up_interruptible(&wait);

// read(): block until DONE
wait_event_interruptible(wait, READ_ONCE(done));
// copy DATA_OUT[] to userspace; then clear done
```

See fir_irq.c for the exact implementation used in this demo.
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    char *token, *buf_copy;
    int i = 0;

    buf_copy = kstrdup(buf, GFP_KERNEL);
    if (!buf_copy)
        return -ENOMEM;

    // Parse space-separated values
    while ((token = strsep(&buf_copy, " \n")) != NULL && i < FIR_MAX_LEN) {
        if (kstrtos16(token, 0, &fir->input_buf[i]))
            continue;
        i++;
    }

    kfree(buf_copy);

    // Write to hardware
    for (int j = 0; j < i; j++)
        writel(fir->input_buf[j], fir->base + FIR_DATA_IN_BASE + j*4);

    return count;
}
static DEVICE_ATTR_WO(input_data);
```

### output_data (read, blocks on wait queue)

```c
static ssize_t output_data_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    ssize_t len = 0;
    int i;

    // Block until processing completes
    if (wait_event_interruptible(fir->wait, !fir->processing))
        return -ERESTARTSYS;  // Interrupted by signal

    // Read output buffer from hardware
    for (i = 0; i < fir->len; i++)
        fir->output_buf[i] = readl(fir->base + FIR_DATA_OUT_BASE + i*4);

    // Format as space-separated values
    for (i = 0; i < fir->len; i++)
        len += scnprintf(buf + len, PAGE_SIZE - len, "%d ", fir->output_buf[i]);

    if (len > 0)
        buf[len - 1] = '\n';  // Replace last space with newline

    return len;
}
static DEVICE_ATTR_RO(output_data);
```

### len (read/write)

```c
static ssize_t len_show(struct device *dev, ...)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", fir->len);
}

static ssize_t len_store(struct device *dev, ...)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u32 val;

    if (kstrtou32(buf, 0, &val) || val > FIR_MAX_LEN)
        return -EINVAL;

    fir->len = val;
    writel(val, fir->base + FIR_LEN_REG);

    return count;
}
static DEVICE_ATTR_RW(len);
```

## Attribute Group

```c
static struct attribute *fir_attrs[] = {
    &dev_attr_coefficients.attr,
    &dev_attr_input_data.attr,
    &dev_attr_output_data.attr,
    &dev_attr_start.attr,
    &dev_attr_len.attr,
    NULL,
};

static const struct attribute_group fir_attr_group = {
    .attrs = fir_attrs,
};
```

## Remove

```c
static int fir_remove(struct platform_device *pdev)
{
    sysfs_remove_group(&pdev->dev.kobj, &fir_attr_group);
    // devm_* handles IRQ and MMIO cleanup
    return 0;
}
```

## Build

```makefile
obj-m := fir_irq.o

KDIR ?= /path/to/linux

all:
	make -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

clean:
	make -C $(KDIR) M=$(PWD) clean
```

## Testing

```bash
insmod fir_irq.ko
echo "8192 16384 16384 8192" > /sys/devices/platform/fir/coefficients
echo "0 0 0 0 0 0 0 0 0 0 32767 0 0 ..." > /sys/devices/platform/fir/input_data
echo 32 > /sys/devices/platform/fir/len
echo 1 > /sys/devices/platform/fir/start
cat /sys/devices/platform/fir/output_data  # Blocks, then prints: 8192 16384 16384 8192 0 ...
```
