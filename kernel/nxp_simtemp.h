/*
 * NXP Simulated Temperature Sensor Driver - Header File
 * 
 * This header defines the data structures, constants, and function prototypes
 * used by the NXP simulated temperature sensor driver.
 */

#ifndef _NXP_SIMTEMP_H_
#define _NXP_SIMTEMP_H_

/* =============================================================================
 * INCLUDES
 * ============================================================================= */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/string.h>
#include <linux/random.h>

/* =============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================= */

struct nxp_simtemp_data;

/* =============================================================================
 * MACROS definitions
 * ============================================================================= */

/* Flag definitions for temperature samples */
#define SIMTEMP_FLAG_NEW_SAMPLE        0x01
#define SIMTEMP_FLAG_THRESHOLD_CROSSED 0x02

/* IOCTL commands (for future use) */
#define SIMTEMP_IOC_MAGIC 's'
#define SIMTEMP_IOC_GET_CONFIG    _IOR(SIMTEMP_IOC_MAGIC, 1, struct simtemp_config)
#define SIMTEMP_IOC_SET_CONFIG    _IOW(SIMTEMP_IOC_MAGIC, 2, struct simtemp_config)
#define SIMTEMP_IOC_GET_STATS     _IOR(SIMTEMP_IOC_MAGIC, 3, struct simtemp_stats)
#define SIMTEMP_IOC_MAXNR         3

/* =============================================================================
 * DATA TYPES definitions
 * ============================================================================= */

/* Temperature simulation modes */
typedef enum {
    SIMTEMP_MODE_NORMAL,
    SIMTEMP_MODE_NOISY,
    SIMTEMP_MODE_RAMP,
    SIMTEMP_MODE_MAX
} simtemp_mode_t;

/* =============================================================================
 * DATA STRUCTURES definitions
 * ============================================================================= */

/* Binary record format for temperature samples */
struct simtemp_sample {
    __u64 timestamp_ns;   /* monotonic timestamp */
    __s32 temp_mC;        /* milli-degree Celsius (e.g., 44123 = 44.123 °C) */
    __u32 flags;          /* bit0=NEW_SAMPLE, bit1=THRESHOLD_CROSSED */
} __attribute__((packed));

/* Configuration structure for ioctl */
struct simtemp_config {
    __u32 sampling_ms;
    __s32 threshold_mC;
    __u32 mode;
};

/* Statistics structure for ioctl */
struct simtemp_stats {
    __u64 update_count;
    __u64 alert_count;
    __u64 error_count;
    __s32 last_error;
};

/* =============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================= */

/* Platform driver functions */
extern int nxp_simtemp_probe(struct platform_device *pdev);
extern void nxp_simtemp_remove(struct platform_device *pdev);

/* Character device operations */
extern int nxp_simtemp_open(struct inode *inode, struct file *file);
extern int nxp_simtemp_release(struct inode *inode, struct file *file);
extern ssize_t nxp_simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
extern __poll_t nxp_simtemp_poll(struct file *file, poll_table *wait);
extern long nxp_simtemp_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/* Device tree functions */
extern int nxp_simtemp_parse_dt(struct nxp_simtemp_data *data);

/* Timer functions */
extern int nxp_simtemp_init_timer(struct nxp_simtemp_data *data);
extern void nxp_simtemp_cleanup_timer(struct nxp_simtemp_data *data);
extern enum hrtimer_restart nxp_simtemp_timer_callback(struct hrtimer *timer);

/* Temperature simulation functions */
extern __s32 nxp_simtemp_generate_temp(struct nxp_simtemp_data *data);
extern int nxp_simtemp_add_sample(struct nxp_simtemp_data *data, __s32 temp_mC);
extern int nxp_simtemp_get_sample(struct nxp_simtemp_data *data, struct simtemp_sample *sample);

/* Sysfs functions */
extern int nxp_simtemp_create_sysfs(struct nxp_simtemp_data *data);
extern void nxp_simtemp_remove_sysfs(struct nxp_simtemp_data *data);
extern ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf);
extern ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
extern ssize_t threshold_mC_show(struct device *dev, struct device_attribute *attr, char *buf);
extern ssize_t threshold_mC_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
extern ssize_t mode_show(struct device *dev, struct device_attribute *attr, char *buf);
extern ssize_t mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
extern ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf);

#endif /* _NXP_SIMTEMP_H_ */