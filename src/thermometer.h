#include <linux/types.h>
#include <linux/cdev.h>

typedef struct ThermometerDevice
{
    const char temperature[4];
    struct cdev cdev;
} ThermometerDevice;

int thermometer_open(struct inode *inode, struct file *filp);

int thermometer_release(struct inode *inode, struct file *filp);

ssize_t thermometer_read(struct file *filp, char __user *buf, size_t count,
                         loff_t *f_pos);

static int thermometer_setup_cdev(ThermometerDevice *dev);

int thermometer_init_module(void);

void thermometer_cleanup_module(void);
