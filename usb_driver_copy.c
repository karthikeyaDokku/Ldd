#include<linux/usb.h>
#include<linux/module.h>

static int __init usb_skel_init(void){
	int result;
	printk(KERN_INFO "Module is initialised\n");
	return 0;
}

module_init(usb_skel_init);

static void __exit usb_skel_exit(void){
	printk(KERN_INFO "Module is exited\n");
}

module_exit(usb_skel_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Karthikeya");

