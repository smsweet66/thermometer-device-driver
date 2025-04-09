/// @file main.c
/// @brief Functions and data related to the thermometer char driver implementation
///
/// @author Sean Sweet
/// @date 2025-4-7

#include "thermometer.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/fs.h> // file_operations

int thermometer_major = 0; // use dynamic major
int thermometer_minor = 0;

#ifdef __KERNEL__
MODULE_AUTHOR("Sean Sweet");
MODULE_LICENSE("Dual BSD/GPL");
#endif

ThermometerDevice thermometer_device = {.temperature = "70"};

int thermometer_open(struct inode *inode, struct file *filp)
{
    ThermometerDevice *device;

    device = container_of(inode->i_cdev, ThermometerDevice, cdev);
    filp->private_data = device;

    return 0;
}

int thermometer_release(struct inode *inode, struct file *filp)
{
    return 0;
}

ssize_t thermometer_read(struct file *filp, char __user *buf, size_t count,
                         loff_t *f_pos)
{
    ssize_t bytes_read = 0;
    size_t str_len = 0;
    size_t copy_len = 0;
    ThermometerDevice *device;

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
    {
        return -EPERM;
    }

    device = (ThermometerDevice *)filp->private_data;
    str_len = strnlen(device->temperature, sizeof(device->temperature));
    if (*f_pos >= str_len)
    {
        goto close_function;
    }

    copy_len = count <= (str_len - *f_pos) ? count : (str_len - *f_pos);

    copy_len -= copy_to_user(buf, device->temperature + *f_pos, copy_len);

close_function:
    return bytes_read;
}

struct file_operations thermometer_fops = {
    .owner = THIS_MODULE,
    .read = thermometer_read,
    .open = thermometer_open,
    .release = thermometer_release,
};

static int thermometer_setup_cdev(ThermometerDevice *dev)
{
    int err, devno = MKDEV(thermometer_major, thermometer_minor);

    cdev_init(&dev->cdev, &thermometer_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &thermometer_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding thermometer cdev\n", err);
    }
    return err;
}

int thermometer_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, thermometer_minor, 1,
                                 "thermometer_char");
    thermometer_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", thermometer_major);
        goto alloc_chrdev_failed;
    }

    result = thermometer_setup_cdev(&thermometer_device);

    if (result)
    {
        goto setup_cdev_failed;
    }

    return 0;

alloc_chrdev_failed:
setup_cdev_failed:

    return result;
}

void thermometer_cleanup_module(void)
{
    dev_t devno = MKDEV(thermometer_major, thermometer_minor);

    cdev_del(&thermometer_device.cdev);

    unregister_chrdev_region(devno, 1);
}

module_init(thermometer_init_module);
module_exit(thermometer_cleanup_module);
