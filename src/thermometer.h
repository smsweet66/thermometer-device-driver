#include <linux/types.h>
#include <linux/cdev.h>

typedef struct ThermometerDevice
{
    char *temperature;
    struct mutex *device_mutex;
    struct cdev cdev;
} ThermometerDevice;

/// @brief Calculates the resistance based on the time elapsed.
/// @note the equation used in determining the resistance from the time was empirically
/// determined based on my own hardware setup.
/// @param[in] time_elapsed the time that it took for the capaciter to be charged
/// @return the resistance of the variable resistor
int time_to_resistance(u64 time_elapsed);

/// @brief Calculates the temperature based on the resistance of the thermistor
/// @note this is very loosely based on the data sheet for the thermistor I am using.
/// I took some shortcuts since this will only be used around room temperature.
/// @param[in] resistance the resistance of the thermistor
/// @return the temperature of the thermistor
int resistance_to_temperature(int resistance);

/// @brief The open command for this device driver.  Stores the current temperature in a string buffer.
/// @param[in] inode the inode of the device
/// @param[in] filp information about how the file is being accessed
/// @return 0 on success, -E on error
int thermometer_open(struct inode *inode, struct file *filp);

/// @brief The close command for this device driver.  Does nothing
/// @param[in] inode the inode of the device
/// @param[in] filp information about how the file is being accessed
/// @return 0 on success, -E on error
int thermometer_release(struct inode *inode, struct file *filp);

/// @brief The read command for this device driver.  Returns the current temperature as a string.
/// @param[in] filp information about how the file is being accessed
/// @param[out] buf buffer for user data
/// @param[in] count how many bytes to read
/// @param[in,out] f_pos the position to read from
/// @return how many bytes were read
ssize_t thermometer_read(struct file *filp, char __user *buf, size_t count,
                         loff_t *f_pos);

/// @brief Tells linux that the device is ready for use
/// @param[in] dev the device that was created
/// @return 0 on success, -E otherwise
static int thermometer_setup_cdev(ThermometerDevice *dev);

/// @brief Performs the initialization for the device
/// @return 0 on success, -E otherwise
int thermometer_init_module(void);

/// @brief Performs the teardown for the device
void thermometer_cleanup_module(void);
