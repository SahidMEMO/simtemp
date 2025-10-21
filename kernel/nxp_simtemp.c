/*
 * NXP Simulated Temperature Sensor Driver
 * 
 * This driver simulates a temperature sensor by producing periodic temperature
 * samples and exposing them via a character device with poll/epoll support.
 */

#include <linux/module.h>
#include "nxp_simtemp.h"

/* =============================================================================
 * MACROS definitions
 * ============================================================================= */

#define DRIVER_NAME "nxp_simtemp"
#define DEVICE_NAME "simtemp"
#define CLASS_NAME "simtemp"
#define SIMTEMP_BUFFER_SIZE 1024

/* =============================================================================
 * DATA structures definitions
 * ============================================================================= */


/* Ring buffer for samples */
struct simtemp_buffer {
    struct simtemp_sample samples[SIMTEMP_BUFFER_SIZE];
    unsigned int head;
    unsigned int tail;
    unsigned int count;
    spinlock_t lock;
};

/* Driver private data */
struct nxp_simtemp_data {
    struct platform_device *pdev;
    struct miscdevice miscdev;
    struct device *dev;
    struct class *class;
    dev_t devt;
    
    /* Configuration */
    unsigned int sampling_ms;
    __s32 threshold_mC;
    simtemp_mode_t mode;
    
    /* Statistics */
    unsigned long update_count;
    unsigned long alert_count;
    unsigned long error_count;
    __s32 last_error;
    
    /* Temperature simulation */
    __s32 base_temp_mC;
    __s32 last_temp_mC;
    __s32 ramp_direction;
    unsigned long ramp_counter;
    
    /* Timing */
    struct hrtimer timer;
    ktime_t period;
    
    /* Data buffer */
    struct simtemp_buffer buffer;
    
    /* Wait queues */
    wait_queue_head_t read_wait;
    wait_queue_head_t poll_wait;
    
    /* Synchronization */
    struct mutex config_mutex;
    spinlock_t stats_lock;
    
    /* Sysfs attributes */
    struct device_attribute sampling_ms_attr;
    struct device_attribute threshold_mC_attr;
    struct device_attribute mode_attr;
    struct device_attribute stats_attr;
};


/* =============================================================================
 * GLOBAL variables and structures definitions
 * ============================================================================= */

/* File operations */
static const struct file_operations nxp_simtemp_fops = {
    .owner = THIS_MODULE,
    .open = nxp_simtemp_open,
    .release = nxp_simtemp_release,
    .read = nxp_simtemp_read,
    .poll = nxp_simtemp_poll,
    .unlocked_ioctl = nxp_simtemp_ioctl,
};

/* Device tree match table */
static const struct of_device_id nxp_simtemp_of_match[] = {
    { .compatible = "nxp,simtemp" },
    { }
};
MODULE_DEVICE_TABLE(of, nxp_simtemp_of_match);

/* Platform driver structure */
static struct platform_driver nxp_simtemp_driver = {
    .probe = nxp_simtemp_probe,
    .remove = nxp_simtemp_remove,
    .driver = {
        .name = "nxp_simtemp",
        .of_match_table = nxp_simtemp_of_match,
    },
};

/* Platform device for testing (when no device tree) */
static struct platform_device *test_device;

/* Sysfs attribute definitions */
static DEVICE_ATTR_RW(sampling_ms);
static DEVICE_ATTR_RW(threshold_mC);
static DEVICE_ATTR_RW(mode);
static DEVICE_ATTR_RO(stats);

/* =============================================================================
 * PRIVATE HELPER FUNCTIONS - DECLARATIONS
 * ============================================================================= */

/* Mode parsing helper */
static int nxp_simtemp_parse_mode_string(const char *str, simtemp_mode_t *mode);
/* Ramp mode initialization helper */
static void nxp_simtemp_init_ramp_mode(struct nxp_simtemp_data *data);
/* Data retrieval helper */
static struct nxp_simtemp_data *nxp_simtemp_get_data(struct device *dev);
/* Error logging helper */
static void nxp_simtemp_log_error(const char *action, int error);

/* =============================================================================
 * CHARACTER DEVICE OPERATIONS
 * ============================================================================= */

int
nxp_simtemp_open(
    struct inode *inode,
    struct file *file)
/**
 * @brief Open the character device
 * 
 * This function is called when the character device is opened by user space.
 * It performs minimal initialization and sets the file private data pointer.
 * 
 * @param inode Pointer to the inode structure
 * @param file Pointer to the file structure
 * @return Always returns 0 (success)
 **********************************************************************************/
{
    struct nxp_simtemp_data *data;
    
    data = container_of(file->private_data, struct nxp_simtemp_data, miscdev);
    file->private_data = data;
    
    pr_debug("NXP SimTemp: Device opened\n");
    return 0;
}

/**********************************************************************************/
int
nxp_simtemp_release(
    struct inode *inode,
    struct file *file)
/**
 * @brief Close the character device
 * 
 * This function is called when the character device is closed by user space.
 * Currently performs minimal cleanup as the device doesn't maintain per-open
 * state that needs to be cleaned up.
 * 
 * @param inode Pointer to the inode structure
 * @param file Pointer to the file structure
 * @return Always returns 0 (success)
 **********************************************************************************/
{
    pr_debug("NXP SimTemp: Device closed\n");
    return 0;
}

/**********************************************************************************/
ssize_t
nxp_simtemp_read(
    struct file *file,
    char __user *buf,
    size_t count,
    loff_t *ppos)
/**
 * @brief Read temperature samples from the device
 * 
 * This function reads temperature samples from the ring buffer. It supports
 * both blocking and non-blocking I/O modes. In blocking mode, the function
 * will wait until data is available. In non-blocking mode, it returns
 * immediately with -EAGAIN if no data is available.
 * 
 * Each read operation returns exactly one temperature sample containing:
 * - Timestamp (nanoseconds)
 * - Temperature in milli-degrees Celsius
 * - Flags indicating sample status and threshold crossing
 * 
 * @param file Pointer to the file structure
 * @param buf User space buffer to copy data to
 * @param count Number of bytes requested (must be >= sizeof(simtemp_sample))
 * @param ppos File position (not used, always reads from current sample)
 * @return Number of bytes read on success, negative error code on failure
 *         -EINVAL: Invalid buffer size
 *         -EAGAIN: No data available (non-blocking mode)
 *         -EFAULT: Failed to copy data to user space
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = file->private_data;
    struct simtemp_sample sample;
    int ret;
    
    if (count < sizeof(sample)) {
        return -EINVAL;
    }
    
    /* Try to get a sample from the buffer */
    ret = nxp_simtemp_get_sample(data, &sample);
    if (ret) {
        if (file->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }
        
        /* Block until data is available */
        ret = wait_event_interruptible(data->read_wait, 
                                      nxp_simtemp_get_sample(data, &sample) == 0);
        if (ret) {
            return ret;
        }
    }
    
    /* Copy sample to user space */
    if (copy_to_user(buf, &sample, sizeof(sample))) {
        return -EFAULT;
    }
    
    return sizeof(sample);
}

/**********************************************************************************/
__poll_t
nxp_simtemp_poll(
    struct file *file,
    poll_table *wait)
/**
 * @brief Poll for data availability on the device
 * 
 * This function implements the poll/epoll interface for the character device.
 * It allows user space applications to check if data is available for reading
 * without blocking. The function registers the wait queues with the poll table
 * and returns the current status of data availability.
 * 
 * @param file Pointer to the file structure
 * @param wait Pointer to the poll table for registering wait queues
 * @return Poll mask indicating available operations:
 *         - POLLIN | POLLRDNORM: Data is available for reading
 *         - 0: No data available
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = file->private_data;
    __poll_t mask = 0;
    unsigned long flags;
    
    poll_wait(file, &data->read_wait, wait);
    poll_wait(file, &data->poll_wait, wait);
    
    spin_lock_irqsave(&data->buffer.lock, flags);
    
    /* Check if data is available for reading */
    if (data->buffer.count > 0) {
        mask |= POLLIN | POLLRDNORM;
    }
    
    /* Check if threshold was crossed (this would be set by the timer callback) */
    /* For now, we'll implement a simple check - in a real implementation,
     * we'd track threshold crossing events separately */
    
    spin_unlock_irqrestore(&data->buffer.lock, flags);
    
    return mask;
}

/**********************************************************************************/
long
nxp_simtemp_ioctl(
    struct file *file,
    unsigned int cmd,
    unsigned long arg)
/**
 * @brief Handle ioctl commands for the device
 * 
 * This function handles ioctl commands for atomic configuration of the device.
 * Currently not implemented - all commands return -ENOTTY.
 * 
 * Future implementation could support:
 * - Atomic configuration changes
 * - Bulk statistics retrieval
 * - Device reset commands
 * 
 * @param file Pointer to the file structure
 * @param cmd Ioctl command number
 * @param arg Command argument (user space pointer)
 * @return -ENOTTY (not implemented)
 **********************************************************************************/
{
    /* TODO: Implement ioctl commands for atomic configuration */
    return -ENOTTY;
}

/* =============================================================================
 * PLATFORM DRIVER FUNCTIONS
 * ============================================================================= */

int
nxp_simtemp_probe(
    struct platform_device *pdev)
/**
 * @brief Probe and initialize the temperature sensor device
 * 
 * This function is called when the platform device is matched with this driver.
 * It performs complete device initialization including:
 * - Memory allocation for driver data
 * - Default configuration setup
 * - Synchronization primitives initialization
 * - Device tree parsing (optional)
 * - High-resolution timer setup
 * - Character device registration
 * - Sysfs attribute creation
 * 
 * The function implements proper error handling with cleanup on failure.
 * 
 * @param pdev Pointer to the platform device structure
 * @return 0 on success, negative error code on failure:
 *         -ENOMEM: Memory allocation failed
 *         -EINVAL: Timer initialization failed
 *         -EBUSY: Character device registration failed
 *         -ENODEV: Sysfs creation failed
 **********************************************************************************/
{
    struct nxp_simtemp_data *data;
    int ret;
    
    pr_info("NXP SimTemp: Probing device\n");
    
    /* Allocate driver data */
    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data) {
        pr_err("Failed to allocate driver data\n");
        return -ENOMEM;
    }
    
    data->pdev = pdev;
    data->dev = &pdev->dev;
    platform_set_drvdata(pdev, data);
    
    /* Initialize default values */
    data->sampling_ms = 100;  /* 100ms default */
    data->threshold_mC = 45000;  /* 45°C default */
    data->mode = SIMTEMP_MODE_NORMAL;
    data->base_temp_mC = 25000;  /* 25°C base temperature */
    data->last_temp_mC = 25000;  /* Initialize to base temperature */
    data->ramp_direction = 1;    /* Will be adjusted when switching to ramp mode */
    data->ramp_counter = 0;
    
    /* Initialize synchronization primitives */
    mutex_init(&data->config_mutex);
    spin_lock_init(&data->stats_lock);
    spin_lock_init(&data->buffer.lock);
    
    /* Initialize wait queues */
    init_waitqueue_head(&data->read_wait);
    init_waitqueue_head(&data->poll_wait);
    
    /* Parse device tree */
    ret = nxp_simtemp_parse_dt(data);
    if (ret) {
        pr_warn("Failed to parse device tree, using defaults: %d\n", ret);
    }
    
    /* Initialize timer */
    ret = nxp_simtemp_init_timer(data);
    if (ret) {
        nxp_simtemp_log_error("initialize timer", ret);
        return ret;
    }
    
    /* Create character device */
    data->miscdev.minor = MISC_DYNAMIC_MINOR;
    data->miscdev.name = DEVICE_NAME;
    data->miscdev.fops = &nxp_simtemp_fops;
    data->miscdev.parent = &pdev->dev;
    data->miscdev.mode = 0666;  /* Set permissions to rw-rw-rw- */
    
    ret = misc_register(&data->miscdev);
    if (ret) {
        nxp_simtemp_log_error("register misc device", ret);
        goto cleanup_timer;
    }
    
    /* Create sysfs attributes */
    ret = nxp_simtemp_create_sysfs(data);
    if (ret) {
        nxp_simtemp_log_error("create sysfs attributes", ret);
        goto cleanup_misc;
    }
    
    pr_info("NXP SimTemp: Device probed successfully\n");
    return 0;
    
cleanup_misc:
    misc_deregister(&data->miscdev);
cleanup_timer:
    nxp_simtemp_cleanup_timer(data);
    return ret;
}

/**********************************************************************************/
void
nxp_simtemp_remove(
    struct platform_device *pdev)
/**
 * @brief Remove and cleanup the temperature sensor device
 * 
 * This function is called when the platform device is removed or the driver
 * is unloaded. It performs complete cleanup in reverse order of initialization:
 * - Stop the high-resolution timer
 * - Remove sysfs attributes
 * - Unregister the character device
 * 
 * The function ensures proper resource cleanup to prevent memory leaks and
 * system instability.
 * 
 * @param pdev Pointer to the platform device structure
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = platform_get_drvdata(pdev);
    
    pr_info("NXP SimTemp: Removing device\n");
    
    /* Cleanup timer first to stop generating samples */
    nxp_simtemp_cleanup_timer(data);
    
    /* Remove sysfs attributes */
    nxp_simtemp_remove_sysfs(data);
    
    /* Unregister misc device */
    misc_deregister(&data->miscdev);
    
    pr_info("NXP SimTemp: Device removed successfully\n");
}

/* =============================================================================
 * DEVICE TREE FUNCTIONS
 * ============================================================================= */

int
nxp_simtemp_parse_dt(
    struct nxp_simtemp_data *data)
/**
 * @brief Parse device tree properties for device configuration
 * 
 * This function parses device tree properties to configure the temperature
 * sensor device. It reads the following optional properties:
 * - sampling-ms: Temperature sampling period (1-10000 ms)
 * - threshold-mC: Temperature threshold in milli-degrees Celsius
 * - mode: Simulation mode ("normal", "noisy", or "ramp")
 * 
 * If properties are missing or invalid, default values are used and warnings
 * are logged. The function is called during device probe.
 * 
 * @param data Pointer to the driver data structure
 * @return 0 on success, -ENODEV if no device tree node found
 **********************************************************************************/
{
    struct device_node *np = data->dev->of_node;
    u32 val;
    const char *mode_str;
    
    if (!np) {
        pr_warn("No device tree node found\n");
        return -ENODEV;
    }
    
    /* Parse sampling-ms property */
    if (of_property_read_u32(np, "sampling-ms", &val) == 0) {
        if (val > 0 && val <= 10000) {  /* Reasonable range: 1ms to 10s */
            data->sampling_ms = val;
            pr_info("DT: sampling-ms = %u\n", val);
        } else {
            pr_warn("DT: Invalid sampling-ms value %u, using default\n", val);
        }
    }
    
    /* Parse threshold-mC property */
    if (of_property_read_u32(np, "threshold-mC", &val) == 0) {
        data->threshold_mC = (__s32)val;
        pr_info("DT: threshold-mC = %d\n", val);
    }
    
    /* Parse mode property */
    if (of_property_read_string(np, "mode", &mode_str) == 0) {
        simtemp_mode_t mode;
        if (nxp_simtemp_parse_mode_string(mode_str, &mode) == 0) {
            data->mode = mode;
        } else {
            pr_warn("DT: Unknown mode '%s', using normal\n", mode_str);
        }
        pr_info("DT: mode = %s\n", mode_str);
    }
    
    return 0;
}

/* =============================================================================
 * TIMER FUNCTIONS
 * ============================================================================= */

int
nxp_simtemp_init_timer(
    struct nxp_simtemp_data *data)
/**
 * @brief Initialize the high-resolution timer for temperature sampling
 * 
 * This function sets up a high-resolution timer that periodically generates
 * temperature samples. The timer uses CLOCK_MONOTONIC and runs in relative
 * mode, restarting automatically after each callback.
 * 
 * The timer period is calculated from the sampling_ms configuration and
 * converted to nanoseconds for the kernel timer subsystem.
 * 
 * @param data Pointer to the driver data structure
 * @return 0 on success (always succeeds)
 **********************************************************************************/
{
    hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    data->timer.function = nxp_simtemp_timer_callback;
    
    data->period = ktime_set(0, data->sampling_ms * 1000000);  /* Convert ms to ns */
    
    /* Start the timer */
    hrtimer_start(&data->timer, data->period, HRTIMER_MODE_REL);
    
    pr_info("NXP SimTemp: Timer initialized with period %u ms\n", data->sampling_ms);
    return 0;
}

/**********************************************************************************/
void
nxp_simtemp_cleanup_timer(
    struct nxp_simtemp_data *data)
/**
 * @brief Cleanup and stop the high-resolution timer
 * 
 * This function stops the temperature sampling timer and performs cleanup.
 * It is called during device removal to ensure no more samples are generated
 * and to prevent use-after-free issues.
 * 
 * @param data Pointer to the driver data structure
 **********************************************************************************/
{
    hrtimer_cancel(&data->timer);
    pr_info("NXP SimTemp: Timer cleaned up\n");
}

/**********************************************************************************/
enum hrtimer_restart
nxp_simtemp_timer_callback(
    struct hrtimer *timer)
/**
 * @brief High-resolution timer callback for temperature sampling
 * 
 * This function is called periodically by the high-resolution timer to generate
 * temperature samples. It performs the following operations:
 * - Generates a new temperature sample based on the current simulation mode
 * - Adds the sample to the ring buffer
 * - Wakes up any waiting readers (blocking I/O)
 * - Wakes up any polling processes (non-blocking I/O)
 * - Restarts the timer for the next sample
 * 
 * The function always returns HRTIMER_RESTART to continue periodic sampling.
 * 
 * @param timer Pointer to the high-resolution timer structure
 * @return HRTIMER_RESTART to continue periodic sampling
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = container_of(timer, struct nxp_simtemp_data, timer);
    __s32 temp_mC;
    int ret;
    
    /* Generate temperature sample */
    temp_mC = nxp_simtemp_generate_temp(data);
    
    /* Add sample to buffer */
    ret = nxp_simtemp_add_sample(data, temp_mC);
    if (ret) {
        pr_warn("NXP SimTemp: Failed to add sample: %d\n", ret);
    }
    
    /* Wake up any waiting readers */
    wake_up_interruptible(&data->read_wait);
    wake_up_interruptible(&data->poll_wait);
    
    /* Restart timer */
    hrtimer_forward_now(timer, data->period);
    return HRTIMER_RESTART;
}

/* =============================================================================
 * TEMPERATURE SIMULATION FUNCTIONS
 * ============================================================================= */

__s32
nxp_simtemp_generate_temp(
    struct nxp_simtemp_data *data)
/**
 * @brief Generate a temperature sample based on the current simulation mode
 * 
 * This function generates temperature samples according to the configured
 * simulation mode. It supports three modes:
 * 
 * - NORMAL: Returns constant base temperature
 * - NOISY: Adds random noise (±1°C) to base temperature
 * - RAMP: Ramps temperature up and down with 0.1°C steps, changing direction
 *   every 20 samples. The initial direction is determined by threshold position
 *   relative to base temperature to ensure threshold crossing.
 * 
 * The function is called by the high-resolution timer callback and updates
 * the sample statistics atomically.
 * 
 * @param data Pointer to the driver data structure
 * @return Temperature in milli-degrees Celsius
 **********************************************************************************/
{
    __s32 temp_mC;
    unsigned long flags;
    
    spin_lock_irqsave(&data->stats_lock, flags);
    
    switch (data->mode) {
    case SIMTEMP_MODE_NORMAL:
        temp_mC = data->base_temp_mC;
        break;
        
    case SIMTEMP_MODE_NOISY:
        /* Add some random noise */
        {
            u32 random_val;
            get_random_bytes(&random_val, sizeof(random_val));
            temp_mC = data->base_temp_mC + (random_val % 2000) - 1000;  /* ±1°C */
        }
        break;
        
    case SIMTEMP_MODE_RAMP:
        /* Ramp temperature up and down */
        data->ramp_counter++;
        if (data->ramp_counter > 10) {  /* Change direction every 10 samples for faster crossing */
            data->ramp_direction *= -1;
            data->ramp_counter = 0;
        }
        
        /* Calculate temperature with ramp step of 0.2°C (200 mC) per sample for faster crossing */
        temp_mC = data->base_temp_mC + (data->ramp_counter * data->ramp_direction * 200);
        
        /* Debug: Print ramp mode info occasionally */
        if (data->ramp_counter % 5 == 0) {
            pr_info("NXP SimTemp RAMP: counter=%lu, direction=%d, temp=%d mC, threshold=%d mC\n", 
                    data->ramp_counter, data->ramp_direction, temp_mC, data->threshold_mC);
        }
        break;
        
    default:
        temp_mC = data->base_temp_mC;
        break;
    }
    
    /* Update statistics */
    data->update_count++;
    
    spin_unlock_irqrestore(&data->stats_lock, flags);
    
    return temp_mC;
}

/**********************************************************************************/
int
nxp_simtemp_add_sample(
    struct nxp_simtemp_data *data,
    __s32 temp_mC)
/**
 * @brief Add a temperature sample to the ring buffer
 * 
 * This function adds a new temperature sample to the ring buffer with proper
 * timestamp and threshold crossing detection. It performs the following:
 * - Creates a sample with current timestamp and temperature
 * - Detects threshold crossing by comparing with previous temperature
 * - Adds the sample to the ring buffer (overwrites oldest if full)
 * - Updates alert statistics if threshold was crossed
 * - Wakes up any waiting readers
 * 
 * The function is thread-safe and uses spinlock protection for buffer access.
 * 
 * @param data Pointer to the driver data structure
 * @param temp_mC Temperature in milli-degrees Celsius
 * @return 0 on success (always succeeds)
 **********************************************************************************/
{
    struct simtemp_sample sample;
    unsigned long flags;
    bool threshold_crossed = false;
    
    /* Prepare sample */
    sample.timestamp_ns = ktime_get_ns();
    sample.temp_mC = temp_mC;
    sample.flags = 0x01;  /* NEW_SAMPLE bit */
    
    /* Check for threshold crossing */
    if ((temp_mC > data->threshold_mC) != (data->last_temp_mC > data->threshold_mC)) {
        sample.flags |= 0x02;  /* THRESHOLD_CROSSED bit */
        threshold_crossed = true;
    }
    data->last_temp_mC = temp_mC;
    
    spin_lock_irqsave(&data->buffer.lock, flags);
    
    /* Add to ring buffer */
    data->buffer.samples[data->buffer.head] = sample;
    data->buffer.head = (data->buffer.head + 1) % SIMTEMP_BUFFER_SIZE;
    
    if (data->buffer.count < SIMTEMP_BUFFER_SIZE) {
        data->buffer.count++;
    } else {
        /* Buffer full, advance tail */
        data->buffer.tail = (data->buffer.tail + 1) % SIMTEMP_BUFFER_SIZE;
    }
    
    if (threshold_crossed) {
        data->alert_count++;
    }
    
    spin_unlock_irqrestore(&data->buffer.lock, flags);
    
    return 0;
}

/**********************************************************************************/
int
nxp_simtemp_get_sample(
    struct nxp_simtemp_data *data,
    struct simtemp_sample *sample)
/**
 * @brief Retrieve a temperature sample from the ring buffer
 * 
 * This function retrieves the oldest temperature sample from the ring buffer
 * in FIFO order. It safely handles the case when no data is available and
 * maintains proper buffer state by advancing the tail pointer and decrementing
 * the count.
 * 
 * The function is thread-safe and uses spinlock protection for buffer access.
 * 
 * @param data Pointer to the driver data structure
 * @param sample Pointer to store the retrieved sample
 * @return 0 on success, -ENODATA if no samples available
 **********************************************************************************/
{
    unsigned long flags;
    int ret = -ENODATA;
    
    spin_lock_irqsave(&data->buffer.lock, flags);
    
    if (data->buffer.count > 0) {
        *sample = data->buffer.samples[data->buffer.tail];
        data->buffer.tail = (data->buffer.tail + 1) % SIMTEMP_BUFFER_SIZE;
        data->buffer.count--;
        ret = 0;
    }
    
    spin_unlock_irqrestore(&data->buffer.lock, flags);
    
    return ret;
}

/* =============================================================================
 * SYSFS FUNCTIONS
 * ============================================================================= */

ssize_t
sampling_ms_show(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
/**
 * @brief Show the current sampling period via sysfs
 * 
 * This function displays the current sampling period in milliseconds through
 * the sysfs interface. The value is read directly from the driver data.
 * 
 * @param dev Pointer to the device structure
 * @param attr Pointer to the device attribute structure
 * @param buf Buffer to write the sampling period string
 * @return Number of characters written to the buffer
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = nxp_simtemp_get_data(dev);
    return sprintf(buf, "%u\n", data->sampling_ms);
}

/**********************************************************************************/
ssize_t
sampling_ms_store(
    struct device *dev,
    struct device_attribute *attr,
    const char *buf,
    size_t count)
/**
 * @brief Set the sampling period via sysfs
 * 
 * This function allows changing the temperature sampling period through sysfs.
 * It validates the input value (1-10000 ms range) and updates the high-resolution
 * timer accordingly. The timer is restarted with the new period immediately.
 * 
 * The function is thread-safe and uses mutex protection for configuration changes.
 * 
 * @param dev Pointer to the device structure
 * @param attr Pointer to the device attribute structure
 * @param buf Buffer containing the new sampling period string
 * @param count Number of characters in the buffer
 * @return Number of characters processed on success, negative error code on failure:
 *         -EINVAL: Invalid value (out of range 1-10000 ms)
 *         -ERANGE: Conversion error
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = nxp_simtemp_get_data(dev);
    unsigned int val;
    int ret;
    
    ret = kstrtouint(buf, 10, &val);
    if (ret) {
        return ret;
    }
    
    if (val < 1 || val > 10000) {
        return -EINVAL;
    }
    
    mutex_lock(&data->config_mutex);
    data->sampling_ms = val;
    data->period = ktime_set(0, val * 1000000);
    
    /* Restart timer with new period */
    hrtimer_cancel(&data->timer);
    hrtimer_start(&data->timer, data->period, HRTIMER_MODE_REL);
    
    mutex_unlock(&data->config_mutex);
    
    return count;
}

/**********************************************************************************/
ssize_t
threshold_mC_show(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
/**
 * @brief Show the current temperature threshold via sysfs
 * 
 * This function displays the current temperature threshold in milli-degrees
 * Celsius through the sysfs interface.
 * 
 * @param dev Pointer to the device structure
 * @param attr Pointer to the device attribute structure
 * @param buf Buffer to write the threshold string
 * @return Number of characters written to the buffer
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = nxp_simtemp_get_data(dev);
    return sprintf(buf, "%d\n", data->threshold_mC);
}

/**********************************************************************************/
ssize_t
threshold_mC_store(
    struct device *dev,
    struct device_attribute *attr,
    const char *buf,
    size_t count)
/**
 * @brief Set the temperature threshold via sysfs
 * 
 * This function allows changing the temperature threshold through sysfs.
 * The threshold is used to detect temperature crossing events and trigger
 * alerts. The value is stored in milli-degrees Celsius.
 * 
 * The function is thread-safe and uses mutex protection for configuration changes.
 * 
 * @param dev Pointer to the device structure
 * @param attr Pointer to the device attribute structure
 * @param buf Buffer containing the new threshold string
 * @param count Number of characters in the buffer
 * @return Number of characters processed on success, negative error code on failure:
 *         -ERANGE: Conversion error
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = nxp_simtemp_get_data(dev);
    __s32 val;
    int ret;
    
    ret = kstrtos32(buf, 10, &val);
    if (ret) {
        return ret;
    }
    
    mutex_lock(&data->config_mutex);
    data->threshold_mC = val;
    mutex_unlock(&data->config_mutex);
    
    return count;
}

/**********************************************************************************/
ssize_t
mode_show(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
/**
 * @brief Show the current simulation mode via sysfs
 * 
 * This function displays the current temperature simulation mode through
 * the sysfs interface. Returns one of: "normal", "noisy", "ramp", or "unknown".
 * 
 * @param dev Pointer to the device structure
 * @param attr Pointer to the device attribute structure
 * @param buf Buffer to write the mode string
 * @return Number of characters written to the buffer
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = nxp_simtemp_get_data(dev);
    const char *mode_str;
    
    switch (data->mode) {
    case SIMTEMP_MODE_NORMAL:
        mode_str = "normal";
        break;
    case SIMTEMP_MODE_NOISY:
        mode_str = "noisy";
        break;
    case SIMTEMP_MODE_RAMP:
        mode_str = "ramp";
        break;
    default:
        mode_str = "unknown";
        break;
    }
    
    return sprintf(buf, "%s\n", mode_str);
}

/**********************************************************************************/
ssize_t
mode_store(
    struct device *dev,
    struct device_attribute *attr,
    const char *buf,
    size_t count)
/**
 * @brief Set the temperature simulation mode via sysfs
 * 
 * This function allows changing the temperature simulation mode through sysfs.
 * It supports three modes: "normal", "noisy", and "ramp". When switching to
 * ramp mode, it intelligently sets the initial ramp direction based on the
 * threshold position relative to the base temperature to ensure threshold
 * crossing occurs quickly.
 * 
 * The function is thread-safe and uses mutex protection for configuration
 * changes.
 * 
 * @param dev Pointer to the device structure
 * @param attr Pointer to the device attribute structure
 * @param buf Buffer containing the mode string ("normal", "noisy", or "ramp")
 * @param count Number of characters in the buffer
 * @return Number of characters processed on success, -EINVAL on invalid mode
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = nxp_simtemp_get_data(dev);
    simtemp_mode_t mode;
    
    /* Parse mode string - need to null-terminate for strcmp */
    char mode_str[16];
    strncpy(mode_str, buf, sizeof(mode_str) - 1);
    mode_str[sizeof(mode_str) - 1] = '\0';
    
    /* Remove trailing newline if present */
    char *newline = strchr(mode_str, '\n');
    if (newline) {
        *newline = '\0';
    }
    
    if (nxp_simtemp_parse_mode_string(mode_str, &mode) != 0) {
        return -EINVAL;
    }
    
    mutex_lock(&data->config_mutex);
    
    /* Reset ramp variables only when switching TO ramp mode from a different mode */
    if (mode == SIMTEMP_MODE_RAMP && data->mode != SIMTEMP_MODE_RAMP) {
        pr_info("NXP SimTemp: Switching to RAMP mode, initializing ramp variables\n");
        nxp_simtemp_init_ramp_mode(data);
        pr_info("NXP SimTemp: Ramp initialized - direction=%d, threshold=%d mC, base=%d mC\n", 
                data->ramp_direction, data->threshold_mC, data->base_temp_mC);
    }
    
    data->mode = mode;
    pr_info("NXP SimTemp: Mode set to %d\n", mode);
    mutex_unlock(&data->config_mutex);
    
    return count;
}

/**********************************************************************************/
ssize_t
stats_show(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
/**
 * @brief Show device statistics via sysfs
 * 
 * This function displays comprehensive device statistics through the sysfs
 * interface. The statistics include:
 * - Total number of temperature updates
 * - Number of threshold crossing alerts
 * - Number of errors encountered
 * - Last error code
 * 
 * The function is thread-safe and uses spinlock protection for statistics access.
 * 
 * @param dev Pointer to the device structure
 * @param attr Pointer to the device attribute structure
 * @param buf Buffer to write the statistics string
 * @return Number of characters written to the buffer
 **********************************************************************************/
{
    struct nxp_simtemp_data *data = nxp_simtemp_get_data(dev);
    unsigned long flags;
    unsigned long updates, alerts, errors;
    __s32 last_error;
    
    spin_lock_irqsave(&data->stats_lock, flags);
    updates = data->update_count;
    alerts = data->alert_count;
    errors = data->error_count;
    last_error = data->last_error;
    spin_unlock_irqrestore(&data->stats_lock, flags);
    
    return sprintf(buf, "updates=%lu alerts=%lu errors=%lu last_error=%d\n",
                   updates, alerts, errors, last_error);
}

/**********************************************************************************/
int
nxp_simtemp_create_sysfs(
    struct nxp_simtemp_data *data)
/**
 * @brief Create sysfs attributes for device configuration and monitoring
 * 
 * This function creates the sysfs interface for the temperature sensor device.
 * It performs the following operations:
 * - Creates a device class for sysfs organization
 * - Creates a device instance in the class
 * - Creates individual sysfs attributes for:
 *   - sampling_ms: Temperature sampling period
 *   - threshold_mC: Temperature threshold
 *   - mode: Simulation mode
 *   - stats: Device statistics
 * 
 * The function implements proper error handling with cleanup on failure.
 * 
 * @param data Pointer to the driver data structure
 * @return 0 on success, negative error code on failure
 **********************************************************************************/
{
    int ret;
    
    /* Create device class */
    data->class = class_create(CLASS_NAME);
    if (IS_ERR(data->class)) {
        ret = PTR_ERR(data->class);
        pr_err("Failed to create device class: %d\n", ret);
        return ret;
    }
    
    /* Create device */
    data->devt = MKDEV(MAJOR(data->miscdev.minor), data->miscdev.minor);
    data->dev = device_create(data->class, NULL, data->devt, data, DEVICE_NAME);
    if (IS_ERR(data->dev)) {
        ret = PTR_ERR(data->dev);
        pr_err("Failed to create device: %d\n", ret);
        goto cleanup_class;
    }
    
    /* Create sysfs attributes */
    ret = device_create_file(data->dev, &dev_attr_sampling_ms);
    if (ret) {
        pr_err("Failed to create sampling_ms attribute: %d\n", ret);
        goto cleanup_device;
    }
    
    ret = device_create_file(data->dev, &dev_attr_threshold_mC);
    if (ret) {
        pr_err("Failed to create threshold_mC attribute: %d\n", ret);
        goto cleanup_sampling_ms;
    }
    
    ret = device_create_file(data->dev, &dev_attr_mode);
    if (ret) {
        pr_err("Failed to create mode attribute: %d\n", ret);
        goto cleanup_threshold_mC;
    }
    
    ret = device_create_file(data->dev, &dev_attr_stats);
    if (ret) {
        pr_err("Failed to create stats attribute: %d\n", ret);
        goto cleanup_mode;
    }
    
    pr_info("NXP SimTemp: Sysfs attributes created successfully\n");
    return 0;
    
cleanup_mode:
    device_remove_file(data->dev, &dev_attr_mode);
cleanup_threshold_mC:
    device_remove_file(data->dev, &dev_attr_threshold_mC);
cleanup_sampling_ms:
    device_remove_file(data->dev, &dev_attr_sampling_ms);
cleanup_device:
    device_destroy(data->class, data->devt);
cleanup_class:
    class_destroy(data->class);
    return ret;
}

/**********************************************************************************/
void
nxp_simtemp_remove_sysfs(
    struct nxp_simtemp_data *data)
/**
 * @brief Remove sysfs attributes and cleanup sysfs interface
 * 
 * This function removes all sysfs attributes and cleans up the sysfs interface.
 * It performs cleanup in reverse order of creation:
 * - Removes all device attributes
 * - Destroys the device instance
 * - Destroys the device class
 * 
 * The function is safe to call even if some resources were not created.
 * 
 * @param data Pointer to the driver data structure
 **********************************************************************************/
{
    if (data->dev) {
        device_remove_file(data->dev, &dev_attr_stats);
        device_remove_file(data->dev, &dev_attr_mode);
        device_remove_file(data->dev, &dev_attr_threshold_mC);
        device_remove_file(data->dev, &dev_attr_sampling_ms);
        device_destroy(data->class, data->devt);
    }
    
    if (data->class) {
        class_destroy(data->class);
    }
    
    pr_info("NXP SimTemp: Sysfs attributes removed\n");
}

/* =============================================================================
 * MODULE INITIALIZATION AND CLEANUP
 * ============================================================================= */

static int __init nxp_simtemp_init(void)
{
    int ret;
    
    pr_info("NXP Simulated Temperature Driver: Initializing\n");
    
    ret = platform_driver_register(&nxp_simtemp_driver);
    if (ret) {
        nxp_simtemp_log_error("register platform driver", ret);
        return ret;
    }
    
    /* Create a test platform device if no device tree binding exists */
    test_device = platform_device_alloc("nxp_simtemp", -1);
    if (!test_device) {
        pr_warn("Failed to allocate test platform device\n");
        /* Continue anyway - driver might be bound via device tree */
    } else {
        ret = platform_device_add(test_device);
        if (ret) {
            pr_warn("Failed to add test platform device: %d\n", ret);
            platform_device_put(test_device);
            test_device = NULL;
        } else {
            pr_info("Test platform device created successfully\n");
        }
    }
    
    pr_info("NXP Simulated Temperature Driver: Registered successfully\n");
    return 0;
}

/**********************************************************************************/
static void __exit nxp_simtemp_exit(void)
{
    pr_info("NXP Simulated Temperature Driver: Unregistering\n");
    
    /* Clean up test device if it was created */
    if (test_device) {
        platform_device_del(test_device);
        platform_device_put(test_device);
        test_device = NULL;
        pr_info("Test platform device removed\n");
    }
    
    platform_driver_unregister(&nxp_simtemp_driver);
    pr_info("NXP Simulated Temperature Driver: Unregistered\n");
}

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("NXP Systems Software Engineer Challenge");
MODULE_DESCRIPTION("NXP Simulated Temperature Sensor Driver");
MODULE_VERSION("1.0");

/* =============================================================================
 * PRIVATE HELPER FUNCTIONS - IMPLEMENTATIONS
 * ============================================================================= */

/**
 * @brief Parse mode string and convert to mode enum
 * 
 * @param str String to parse ("normal", "noisy", "ramp")
 * @param mode Pointer to store the parsed mode
 * @return 0 on success, -EINVAL on invalid string
 */
 static int
 nxp_simtemp_parse_mode_string(
     const char *str,
     simtemp_mode_t *mode)
 {
     if (strcmp(str, "normal") == 0) {
         *mode = SIMTEMP_MODE_NORMAL;
         return 0;
     } else if (strcmp(str, "noisy") == 0) {
         *mode = SIMTEMP_MODE_NOISY;
         return 0;
     } else if (strcmp(str, "ramp") == 0) {
         *mode = SIMTEMP_MODE_RAMP;
         return 0;
     }
     return -EINVAL;
 }
 
 /**
  * @brief Initialize ramp mode variables
  * 
  * @param data Pointer to driver data structure
  */
 static void
 nxp_simtemp_init_ramp_mode(
     struct nxp_simtemp_data *data)
 {
     data->ramp_counter = 0;
     if (data->threshold_mC < data->base_temp_mC) {
         data->ramp_direction = -1;  /* Start going down to cross threshold */
     } else {
         data->ramp_direction = 1;   /* Start going up to cross threshold */
     }
 }
 
 /**
  * @brief Get driver data from device
  * 
  * @param dev Pointer to device structure
  * @return Pointer to driver data structure
  */
 static struct nxp_simtemp_data *
 nxp_simtemp_get_data(
     struct device *dev)
 {
     return dev_get_drvdata(dev);
 }
 
 /**
  * @brief Log error message with consistent format
  * 
  * @param action Description of the action that failed
  * @param error Error code
  */
 static void
 nxp_simtemp_log_error(
     const char *action,
     int error)
 {
     pr_err("Failed to %s: %d\n", action, error);
 }
 