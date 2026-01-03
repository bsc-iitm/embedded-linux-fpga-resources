// Test program for squarer drivers
// Compares MMIO vs DMA performance
//
// Usage: ./test_squarer [num_samples]

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define DEFAULT_SAMPLES 1024
// Note: Both drivers have a 256K sample limit (pre-allocated buffers).
// Drivers will return -EINVAL if this limit is exceeded.

// Get time in nanoseconds
static uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// Test a squarer device
// Returns 0 on success, -1 on error
static int test_device(const char *dev_path, int16_t *input, int32_t *output,
                       size_t count, uint64_t *elapsed_ns)
{
    int fd;
    ssize_t ret;
    uint64_t start, end;

    fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", dev_path, strerror(errno));
        return -1;
    }

    // Write input data
    ret = write(fd, input, count * sizeof(int16_t));
    if (ret != (ssize_t)(count * sizeof(int16_t))) {
        fprintf(stderr, "Write failed: %zd\n", ret);
        close(fd);
        return -1;
    }

    // Time the read (this is where computation happens)
    start = get_time_ns();
    ret = read(fd, output, count * sizeof(int32_t));
    end = get_time_ns();

    if (ret != (ssize_t)(count * sizeof(int32_t))) {
        fprintf(stderr, "Read failed: %zd\n", ret);
        close(fd);
        return -1;
    }

    *elapsed_ns = end - start;
    close(fd);
    return 0;
}

// Verify results
static int verify_results(int16_t *input, int32_t *output, size_t count)
{
    size_t i;
    int errors = 0;

    for (i = 0; i < count; i++) {
        int32_t expected = (int32_t)input[i] * (int32_t)input[i];
        if (output[i] != expected) {
            if (errors < 5) {
                printf("  ERROR at [%zu]: input=%d, expected=%d, got=%d\n",
                       i, input[i], expected, output[i]);
            }
            errors++;
        }
    }
    return errors;
}

int main(int argc, char *argv[])
{
    size_t num_samples = DEFAULT_SAMPLES;
    int16_t *input;
    int32_t *output_mmio, *output_dma;
    uint64_t time_mmio, time_dma;
    size_t i;
    int errors;

    if (argc > 1) {
        num_samples = atoi(argv[1]);
        if (num_samples == 0) {
            fprintf(stderr, "Invalid sample count (must be > 0)\n");
            return 1;
        }
    }

    printf("Squarer Driver Comparison\n");
    printf("=========================\n");
    printf("Samples: %zu\n\n", num_samples);

    // Allocate buffers
    input = malloc(num_samples * sizeof(int16_t));
    output_mmio = malloc(num_samples * sizeof(int32_t));
    output_dma = malloc(num_samples * sizeof(int32_t));
    if (!input || !output_mmio || !output_dma) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    // Initialize input with test pattern
    for (i = 0; i < num_samples; i++) {
        input[i] = (int16_t)(i - num_samples/2);  // range: -N/2 to N/2
    }

    // Test MMIO driver
    printf("Testing MMIO driver (/dev/squarer_mmio)...\n");
    if (test_device("/dev/squarer_mmio", input, output_mmio, num_samples, &time_mmio) == 0) {
        errors = verify_results(input, output_mmio, num_samples);
        printf("  Time: %" PRIu64 " ns (%.2f us)\n", time_mmio, time_mmio / 1000.0);
        printf("  Per sample: %.0f ns\n", (double)time_mmio / num_samples);
        printf("  Errors: %d\n\n", errors);
    } else {
        printf("  SKIPPED (device not available)\n\n");
        time_mmio = 0;
    }

    // Test DMA driver
    printf("Testing DMA driver (/dev/squarer_dma)...\n");
    if (test_device("/dev/squarer_dma", input, output_dma, num_samples, &time_dma) == 0) {
        errors = verify_results(input, output_dma, num_samples);
        printf("  Time: %" PRIu64 " ns (%.2f us)\n", time_dma, time_dma / 1000.0);
        printf("  Per sample: %.0f ns\n", (double)time_dma / num_samples);
        printf("  Errors: %d\n\n", errors);
    } else {
        printf("  SKIPPED (device not available)\n\n");
        time_dma = 0;
    }

    // Summary
    if (time_mmio > 0 && time_dma > 0) {
        printf("Summary\n");
        printf("-------\n");
        printf("MMIO:  %8" PRIu64 " ns  (%zu samples, 2 reg ops each = %zu MMIO ops)\n",
               time_mmio, num_samples, num_samples * 2);
        printf("DMA:   %8" PRIu64 " ns  (%zu samples in single bulk transfer)\n",
               time_dma, num_samples);
        printf("Speedup: %.1fx\n", (double)time_mmio / time_dma);
    }

    free(input);
    free(output_mmio);
    free(output_dma);
    return 0;
}
