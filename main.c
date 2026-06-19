#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
 
#define MaxSize 512
char deviceBuf[MaxSize];

dev_t device_num;


struct cdev mdev_cdev;

struct file_operations mdev_fops;
static int __init mydriverinit(void)
{
	alloc_chrdev_region(&device_num,0,1,"mcb");
	cdev_init(&mdev_cdev,&mdev_fops);
	
	// register cdev structure
	mdev_cdev.owner = THIS_MODULE;
	cdev_add(&mdev_cdev,device_num,1);
	return 0;
}


static void __exit mydrivercleanup(void)
{

}



module_init(mydriverinit);
module_exit(mydrivercleanup);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("KARTHIKEYA");
MODULE_DESCRIPTION("SIMPLE CHAR DRIVER");


