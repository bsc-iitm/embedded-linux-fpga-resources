# Driver Directory - Performance Comparison

## Implementation

Two drivers using same hardware:

1. **fir_poll.c** - Busy-wait polling (Week 7 style)
2. **fir_irq.c** - Interrupt-driven (Week 9 style, from demo_fir_irq)

## Key Differences

### Polling Driver

```c
// output_data_show() - blocks with busy-wait
writel(CTRL_START, fir->base + CTRL_REG);

ktime_t start = ktime_get();
do {
    status = readl(fir->base + STATUS_REG);
    cpu_relax();
} while (!(status & STATUS_DONE_BIT));
ktime_t end = ktime_get();

fir->stats.last_latency_ns = ktime_to_ns(ktime_sub(end, start));

// Read output buffer
// ...

writel(STATUS_DONE_BIT, fir->base + STATUS_REG);  // Clear
```

### Interrupt Driver

```c
// output_data_show() - blocks on wait queue
ktime_t start = ktime_get();

writel(CTRL_START, fir->base + CTRL_REG);

if (wait_event_interruptible(fir->wait, !fir->processing))
    return -ERESTARTSYS;

ktime_t end = ktime_get();
fir->stats.last_latency_ns = ktime_to_ns(ktime_sub(end, start));

// Read output buffer (already cleared by IRQ handler)
```

## Statistics Structure

Add to both drivers:

```c
struct fir_stats {
    atomic64_t total_ops;
    u64 total_latency_ns;
    u64 min_latency_ns;
    u64 max_latency_ns;
    u64 last_latency_ns;
};

struct fir_dev {
    // ... existing fields ...
    struct fir_stats stats;
};
```

## Statistics Sysfs Attributes

```c
static ssize_t operations_show(struct device *dev, ...)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    return sprintf(buf, "%llu\n", atomic64_read(&fir->stats.total_ops));
}
static DEVICE_ATTR_RO(operations);

static ssize_t avg_latency_us_show(struct device *dev, ...)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    u64 ops = atomic64_read(&fir->stats.total_ops);
    if (ops == 0)
        return sprintf(buf, "0\n");
    return sprintf(buf, "%llu\n", fir->stats.total_latency_ns / ops / 1000);
}
static DEVICE_ATTR_RO(avg_latency_us);

static ssize_t min_latency_us_show(...)
{
    return sprintf(buf, "%llu\n", fir->stats.min_latency_ns / 1000);
}
static DEVICE_ATTR_RO(min_latency_us);

static ssize_t max_latency_us_show(...)
{
    return sprintf(buf, "%llu\n", fir->stats.max_latency_ns / 1000);
}
static DEVICE_ATTR_RO(max_latency_us);

static ssize_t last_latency_us_show(...)
{
    return sprintf(buf, "%llu\n", fir->stats.last_latency_ns / 1000);
}
static DEVICE_ATTR_RO(last_latency_us);

static ssize_t reset_store(struct device *dev, ...)
{
    struct fir_dev *fir = dev_get_drvdata(dev);
    atomic64_set(&fir->stats.total_ops, 0);
    fir->stats.total_latency_ns = 0;
    fir->stats.min_latency_ns = U64_MAX;
    fir->stats.max_latency_ns = 0;
    fir->stats.last_latency_ns = 0;
    return count;
}
static DEVICE_ATTR_WO(reset);

static struct attribute *fir_stats_attrs[] = {
    &dev_attr_operations.attr,
    &dev_attr_avg_latency_us.attr,
    &dev_attr_min_latency_us.attr,
    &dev_attr_max_latency_us.attr,
    &dev_attr_last_latency_us.attr,
    &dev_attr_reset.attr,
    NULL,
};

static const struct attribute_group fir_stats_group = {
    .name = "stats",
    .attrs = fir_stats_attrs,
};
```

## Update Statistics After Each Operation

```c
static void update_stats(struct fir_dev *fir, u64 latency_ns)
{
    atomic64_inc(&fir->stats.total_ops);
    fir->stats.total_latency_ns += latency_ns;
    fir->stats.last_latency_ns = latency_ns;

    if (latency_ns < fir->stats.min_latency_ns)
        fir->stats.min_latency_ns = latency_ns;

    if (latency_ns > fir->stats.max_latency_ns)
        fir->stats.max_latency_ns = latency_ns;
}
```

## Makefile

```makefile
obj-m := fir_poll.o fir_irq.o

KDIR ?= /path/to/linux

all:
	make -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

clean:
	make -C $(KDIR) M=$(PWD) clean
```

## Testing

See main demo README for benchmarking scripts.

Quick test:
```bash
# Test polling
insmod fir_poll.ko
for i in {1..100}; do cat /sys/devices/platform/fir/output_data > /dev/null; done
cat /sys/devices/platform/fir/stats/avg_latency_us
rmmod fir_poll

# Test interrupts
insmod fir_irq.ko
for i in {1..100}; do cat /sys/devices/platform/fir/output_data > /dev/null; done
cat /sys/devices/platform/fir/stats/avg_latency_us
rmmod fir_irq

# Compare results
```
