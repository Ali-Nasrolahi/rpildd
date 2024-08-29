#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>


/* Macros */
#define GPIO_DEVNAME ("gpio_dev")
#define GPIO_LOG_PRE "[GPIO Driver]: "
#define BCM2711_GPIO_BASE_ADDR (0xfe200000)
/* Macros */

/******************* Driver variables ********************/
dev_t _dev;
struct cdev _cdev;
struct class *gpio_class;
uint32_t *gpio_base_mmio; /* all access are assumed to be 32-bit */
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

/* Set the pin in output mode */
static void gpio_pin_seto(uint32_t pin)
{
	uint32_t fsel_index = pin/10; 	/* GPIO Function Select 0-5 */
	uint32_t fsel_bitpos = pin%10; 	/* Each FS has 10 pins, FSEL_1(0-9), FSEL_2(10-19), ... */
	uint32_t* gpio_fsel = gpio_base_mmio + fsel_index;

	*gpio_fsel &= ~(7 << (fsel_bitpos*3));
	*gpio_fsel |= (1 << (fsel_bitpos*3));
}

static void gpio_pin_on(uint32_t pin)
{
	gpio_pin_seto(pin);
	uint32_t* gpio_on_register = (uint32_t*)((char*)gpio_base_mmio + 0x1c);
	*gpio_on_register |= (1 << pin);
}

static void gpio_pin_off(uint32_t pin)
{
	gpio_pin_seto(pin);
	uint32_t *gpio_off_register = (uint32_t*)((char*)gpio_base_mmio + 0x28);
	*gpio_off_register |= (1<<pin);
}

static uint32_t gpio_pin_state(void)
{
	return *((uint32_t*)((char*)gpio_base_mmio + 0x34));
}

static ssize_t gpio_read(struct file *filp, char __user *buf, size_t len,loff_t * off) {

	char local_buf[64] = {0};
	sprintf(local_buf, "%u\n", gpio_pin_state());
	copy_to_user(buf, local_buf, strlen(local_buf));
	return strlen(local_buf);
}

static ssize_t gpio_write(struct file *filp, const char __user *buf, size_t len,loff_t * off) {
	unsigned int pin, state;
	char local_buf[20] = {0};

	if(copy_from_user(local_buf, buf, min(len, sizeof(local_buf)))) {
		pr_err("Cannot copy data from userspace\n");
		return -1;
	}

	if(sscanf(local_buf, "pin:%u, state:%u\n", &pin, &state) != 2) {
		pr_err("Wrong input format provided.\n");
		return -1;
	}

	state & 1 ? gpio_pin_on(pin) : gpio_pin_off(pin);

	pr_info("Pin %u state set to %u\n", pin, state);

	return len;
}

static int __init gpio_init(void) {

	gpio_base_mmio = (uint32_t*)ioremap(BCM2711_GPIO_BASE_ADDR, PAGE_SIZE);

	if (gpio_base_mmio == NULL)
	{
		printk("Failed to map GPIO memory to driver\n");
		return -1;
	}

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

r_class:
	class_destroy(gpio_class);
r_cdev:
	cdev_del(&_cdev);
r_chrdev:
	unregister_chrdev_region(_dev, 1);
	iounmap(gpio_base_mmio);
	return -1;
}

static void __exit gpio_exit(void) {

	device_destroy(gpio_class, _dev);
	class_destroy(gpio_class);
	cdev_del(&_cdev);
	unregister_chrdev_region(_dev, 1);
	iounmap(gpio_base_mmio);

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



