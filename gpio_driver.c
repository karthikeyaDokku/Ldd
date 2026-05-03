#include "gpio_driver.h"

#define LLL_MAX_USER_SIZE 512

static char data_buffer[LLL_MAX_USER_SIZE + 1] = {0};

static unsigned int *gpio_registers = NULL;

/* char driver stuff */
static dev_t dev_num;
static struct cdev gpio_cdev;
static struct class *gpio_class;

static void gpio_pin_on(unsigned int pin) {
	unsigned int fsel_index = pin/10;
	unsigned int fsel_bitpos = pin%10;
	unsigned int* gpio_fsel = gpio_registers + fsel_index;
	unsigned int* gpio_on_register = (unsigned int*)((char*)gpio_registers + 0x1c);

	// set pin as output
	*gpio_fsel &= ~(7 << (fsel_bitpos*3));
	*gpio_fsel |= (1 << (fsel_bitpos*3));

	// turn ON
	*gpio_on_register |= (1 << pin);
}

static void gpio_pin_off(unsigned int pin) {
	unsigned int *gpio_off_register = (unsigned int*)((char*)gpio_registers + 0x28);

	// turn OFF
	*gpio_off_register |= (1<<pin);
}

static ssize_t gpio_write(struct file *file_pointer,
                          const char __user *user_buffer,
                          size_t count, loff_t *offset) {

	unsigned int pin = UINT_MAX;
	unsigned int value = UINT_MAX;

	memset(data_buffer, 0x0, sizeof(data_buffer));

	if (count > LLL_MAX_USER_SIZE)
		count = LLL_MAX_USER_SIZE;

	if (copy_from_user(data_buffer, user_buffer, count))
		return 0;

	// expecting "pin,value"
	if (sscanf(data_buffer, "%d,%d", &pin, &value) != 2) {
		printk(KERN_ALERT "wrong format\n");
		return count;
	}

	// simple check
	if (pin > 27) {
		printk(KERN_ALERT "wrong pin\n");
		return count;
	}

	if (value != 0 && value != 1) {
		printk(KERN_ALERT "wrong value\n");
		return count;
	}

	if (value == 1)
		gpio_pin_on(pin);
	else
		gpio_pin_off(pin);

	return count;
}

/* connect write function */
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.write = gpio_write,
};

static int __init driver_init(void) {
	printk(KERN_INFO "init start\n");

	gpio_registers = (int*)ioremap(BCM2711_GPIO_ADDRESS, PAGE_SIZE);

	if (gpio_registers == NULL) {
		printk(KERN_ALERT "ioremap failed\n");
		return -ENOMEM;
	}

	// creating /dev/gpio
	alloc_chrdev_region(&dev_num, 0, 1, "gpio");
	cdev_init(&gpio_cdev, &fops);
	cdev_add(&gpio_cdev, dev_num, 1);

	gpio_class = class_create("gpio_class");
	device_create(gpio_class, NULL, dev_num, NULL, "gpio");

	printk(KERN_INFO "device created\n");

	return 0;
}

static void __exit driver_exit(void) {
	printk(KERN_INFO "exit\n");

	// remove device
	device_destroy(gpio_class, dev_num);
	class_destroy(gpio_class);
	cdev_del(&gpio_cdev);
	unregister_chrdev_region(dev_num, 1);

	iounmap(gpio_registers);
}

module_init(driver_init);
module_exit(driver_exit);
MODULE_LICENSE("GPL");