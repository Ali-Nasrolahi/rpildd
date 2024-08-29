#include <asm/io.h>

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>

/* Macros */
#define GPIO_DEVNAME ("gpio_dev")
#define GPIO_LOG_PRE "[GPIO Driver]: "
#define GPIO_BCM2711_PLAT_OFFSET (512)
/* Macros */

/******************* Driver variables ********************/
dev_t _dev;
struct cdev _cdev;
struct class *gpio_class;
/******************* Driver variables ********************/


/*************** Driver functions **********************/
static ssize_t gpio_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t gpio_write(struct file *filp, const char *buf, size_t len, loff_t * off);
/******************************************************/

static struct file_operations fops =
{
  .owner          = THIS_MODULE,
  .read           = gpio_read,
  .write          = gpio_write,
};


static ssize_t gpio_read(struct file *filp, char __user *buf, size_t len,loff_t * off) {

	char local_buf[64] = {0};
	uint32_t pins_value = 0;

	for(uint8_t i = 0; i < 32; ++i) {
		pins_value |=
			((gpiod_get_raw_value_cansleep(gpio_to_desc(GPIO_BCM2711_PLAT_OFFSET + i)) & 1) << i);
	}


	sprintf(local_buf, "%u\n", pins_value);

	if(copy_to_user(buf, local_buf, strlen(local_buf))) {
		pr_err(GPIO_LOG_PRE "Cannot copy data from userspace\n");
		return -1;
	}

	return strlen(local_buf);
}

static ssize_t gpio_write(struct file *filp, const char __user *buf, size_t len,loff_t * off) {

	unsigned int pin, state;
	struct gpio_desc *desc;
	char local_buf[20] = {0};

	if(copy_from_user(local_buf, buf, min(len, sizeof(local_buf)))) {
		pr_err(GPIO_LOG_PRE "Cannot copy data from userspace\n");
		return -1;
	}

	if(sscanf(local_buf, "pin:%u, state:%u\n", &pin, &state) != 2) {
		pr_err(GPIO_LOG_PRE "Wrong input format provided.\n");
		return -1;
	}

	/* Set to output mode then set the value */
	desc = gpio_to_desc(GPIO_BCM2711_PLAT_OFFSET + pin);
	if(gpiod_direction_output(desc, 0)) {
		pr_err(GPIO_LOG_PRE "Cannot change pin %u mode to output\n", pin);
		return -1;
	}
	gpiod_set_raw_value_cansleep(desc, state & 1);

	pr_info(GPIO_LOG_PRE "Pin %u state set to %u\n", pin, state & 1);

	return len;
}

static int __init gpio_init(void) {

	if(alloc_chrdev_region(&_dev, 0, 1, GPIO_DEVNAME)) {
		pr_err(GPIO_LOG_PRE "chrdev failed to allocate device.");
		return -1;
	}

	printk(GPIO_LOG_PRE "Major: %d, Minor: %d\n", MAJOR(_dev), MINOR(_dev));

	cdev_init(&_cdev, &fops);
	if(cdev_add(&_cdev, _dev, 1) < 0) {
		pr_err(GPIO_LOG_PRE "CDev failed to be added.\n");
		goto r_chrdev;
	}

	if(IS_ERR(gpio_class = class_create("gpio_class"))) {
		pr_err(GPIO_LOG_PRE "Class creation failed\n");
		goto r_cdev;
	}

	if(IS_ERR(device_create(gpio_class, NULL, _dev, NULL, GPIO_DEVNAME))) {
		pr_err(GPIO_LOG_PRE "Class creation failed\n");
		goto r_class;
	}

	printk(GPIO_LOG_PRE "Initted\n");

	return 0;

r_device:
	device_destroy(gpio_class, _dev);
r_class:
	class_destroy(gpio_class);
r_cdev:
	cdev_del(&_cdev);
r_chrdev:
	unregister_chrdev_region(_dev, 1);
	return -1;
}

static void __exit gpio_exit(void) {

	device_destroy(gpio_class, _dev);
	class_destroy(gpio_class);
	cdev_del(&_cdev);
	unregister_chrdev_region(_dev, 1);

	printk(GPIO_LOG_PRE "Exited\n");
}

module_init(gpio_init);
module_exit(gpio_exit);

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ali Nasrolahi");
MODULE_DESCRIPTION("Simple GPIO driver.");
MODULE_VERSION("1.0.0");
/* Meta Information */



