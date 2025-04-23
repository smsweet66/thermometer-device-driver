/// @file main.c
/// @brief Functions and data related to the thermometer char driver implementation
///
/// @author Sean Sweet
/// @date 2025-4-7

#include "thermometer.h"

#include <linux/delay.h>
#include <linux/fs.h> // file_operations
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/math64.h>

int thermometer_major = 0; // use dynamic major
int thermometer_minor = 0;

#define GPIO_OFFSET 512U
#define INPUT_PIN (GPIO_OFFSET + 18U)  // GPIO 18
#define OUTPUT_PIN (GPIO_OFFSET + 23U) // GPIO 23
#define TEMPURATURE_LENGTH 30U

#ifdef __KERNEL__
MODULE_AUTHOR("Sean Sweet");
MODULE_LICENSE("Dual BSD/GPL");
#endif

ThermometerDevice thermometer_device = {0};

int time_to_resistance(u64 time_elapsed)
{
    return div64_long(time_elapsed, 10) + 600;
}

int resistance_to_tempurature(int resistance)
{
    // the ratio between the current resistance, and the resistance at 25C, times 1000
    int relative_resistance = resistance / 10;

    return (relative_resistance * -10 + 22705) / 463;
}

int thermometer_open(struct inode *inode, struct file *filp)
{
    ThermometerDevice *device;
    int return_val = 0;
    int resistance = 0;
    int tempurature = 0;

    printk(KERN_INFO "Opened\n");

    device = container_of(inode->i_cdev, ThermometerDevice, cdev);
    filp->private_data = device;

    if (mutex_lock_interruptible(device->device_mutex) != 0)
    {
        printk(KERN_WARNING "OPEN: Failed to lock mutex\n");
        return_val = -ERESTARTSYS;
        goto device_mutex_lock_failed;
    }

    gpio_set_value(OUTPUT_PIN, 0);

    msleep(5);

    u64 start = ktime_get_mono_fast_ns();
    gpio_set_value(OUTPUT_PIN, 1);
    while (gpio_get_value(INPUT_PIN) != 1)
        ;

    u64 end = ktime_get_mono_fast_ns();
    resistance = time_to_resistance(end - start);
    tempurature = resistance_to_tempurature(resistance);

    snprintf(device->temperature, TEMPURATURE_LENGTH, "%d\n", tempurature);

    gpio_set_value(OUTPUT_PIN, 0);

    mutex_unlock(device->device_mutex);

device_mutex_lock_failed:
    return return_val;
}

int thermometer_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "Closing\n");

    return 0;
}

ssize_t thermometer_read(struct file *filp, char __user *buf, size_t count,
                         loff_t *f_pos)
{
    size_t str_len = 0;
    size_t copy_len = 0;
    ThermometerDevice *device;
    ssize_t return_val;

    printk(KERN_INFO "Reading\n");

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
    {
        printk(KERN_WARNING "READ: Missing read permissions\n");
        return_val = -EPERM;
        goto insufficient_permissions;
    }

    device = (ThermometerDevice *)filp->private_data;
    if (mutex_lock_interruptible(device->device_mutex) != 0)
    {
        printk(KERN_WARNING "READ: Failed to lock mutex\n");
        return_val = -ERESTARTSYS;
        goto device_mutex_lock_failed;
    }

    str_len = strnlen(device->temperature, sizeof(device->temperature));
    if (*f_pos >= str_len)
    {
        printk(KERN_WARNING "READ: Can't read past EOF\n");
        goto close_function;
    }

    copy_len = count <= (str_len - *f_pos) ? count : (str_len - *f_pos);

    copy_len -= copy_to_user(buf, device->temperature + *f_pos, copy_len);
    *f_pos += copy_len;

close_function:
    mutex_unlock(device->device_mutex);
device_mutex_lock_failed:
insufficient_permissions:
    return copy_len;
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

    printk(KERN_WARNING "Setting up CDEV\n");

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
                                 "thermometer");
    thermometer_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", thermometer_major);
        goto alloc_chrdev_failed;
    }

    thermometer_device.temperature = kmalloc_array(TEMPURATURE_LENGTH, sizeof(char), GFP_KERNEL);
    if (thermometer_device.temperature == NULL)
    {
        printk(KERN_WARNING "INIT: Tempurature buffer malloc failed\n");
        result = -ENOMEM;
        goto tempurature_malloc_failed;
    }

    thermometer_device.device_mutex = kmalloc(sizeof(struct mutex), GFP_KERNEL);
    if (thermometer_device.device_mutex == NULL)
    {
        printk(KERN_WARNING "INIT: Device mutex malloc failed\n");
        result = -ENOMEM;
        goto device_mutex_malloc_failed;
    }

    mutex_init(thermometer_device.device_mutex);

    result = gpio_request_one(OUTPUT_PIN, GPIOF_OUT_INIT_LOW, "OUTPUT_PIN");
    if (result != 0)
    {

        printk(KERN_WARNING "INIT: Output pin config failed: %pe\n", ERR_PTR(result));
        result = -ERESTARTSYS;
        goto request_output_pin_failed;
    }

    result = gpio_request_one(INPUT_PIN, GPIOF_DIR_IN, "INPUT_PIN");
    if (result != 0)
    {
        printk(KERN_WARNING "INIT: Input pin config failed: %pe\n", ERR_PTR(result));
        result = -ERESTARTSYS;
        goto request_input_pin_failed;
    }

    result = thermometer_setup_cdev(&thermometer_device);

    if (result)
    {
        printk(KERN_WARNING "INIT: CDEV setup failed\n");
        goto setup_cdev_failed;
    }

    return 0;
setup_cdev_failed:
    gpio_free(INPUT_PIN);
request_input_pin_failed:
    gpio_free(OUTPUT_PIN);
request_output_pin_failed:
    mutex_destroy(thermometer_device.device_mutex);
    kfree(thermometer_device.device_mutex);
device_mutex_malloc_failed:
    kfree(thermometer_device.temperature);
tempurature_malloc_failed:
alloc_chrdev_failed:

    return result;
}

void thermometer_cleanup_module(void)
{
    dev_t devno = MKDEV(thermometer_major, thermometer_minor);

    cdev_del(&thermometer_device.cdev);

    unregister_chrdev_region(devno, 1);

    gpio_free(INPUT_PIN);
    gpio_free(OUTPUT_PIN);
    mutex_destroy(thermometer_device.device_mutex);
    kfree(thermometer_device.device_mutex);
    kfree(thermometer_device.temperature);
}

module_init(thermometer_init_module);
module_exit(thermometer_cleanup_module);
