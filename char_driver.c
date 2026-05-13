#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/string.h>

#include "char_driver.h"

/* ================= Globals ================= */

static int major;

static struct class *driver_class;
static struct proc_dir_entry *proc_entry;
static struct kobject *sysfs_kobj;

static DEFINE_MUTEX(buffer_lock);

static char msg_buffer[BUFFER_SIZE] =
    "Hello from Kernel Space!\n";

/* ================= Device Permission ================= */

static char *devnode(const struct device *dev,
                     umode_t *mode)
{
    if (mode)
        *mode = 0666;

    return NULL;
}

/* ================= Device Operations ================= */

static int dev_open(struct inode *inode,
                    struct file *file)
{
    pr_info("device opened\n");
    return 0;
}

static int dev_release(struct inode *inode,
                       struct file *file)
{
    pr_info("device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *file,
                        char __user *buf,
                        size_t len,
                        loff_t *offset)
{
    size_t data_len;
    size_t bytes_to_read;

    mutex_lock(&buffer_lock);

    data_len = strlen(msg_buffer);

    if (*offset >= data_len) {
        mutex_unlock(&buffer_lock);
        return 0;
    }

    bytes_to_read =
        min(len, data_len - (size_t)*offset);

    if (copy_to_user(buf,
                     msg_buffer + *offset,
                     bytes_to_read)) {
        mutex_unlock(&buffer_lock);
        return -EFAULT;
    }

    *offset += bytes_to_read;

    mutex_unlock(&buffer_lock);

    return bytes_to_read;
}

static ssize_t dev_write(struct file *file,
                         const char __user *buf,
                         size_t len,
                         loff_t *offset)
{
    size_t bytes_to_write;

    mutex_lock(&buffer_lock);

    bytes_to_write =
        min(len, (size_t)(BUFFER_SIZE - 1));

    if (copy_from_user(msg_buffer,
                       buf,
                       bytes_to_write)) {
        mutex_unlock(&buffer_lock);
        return -EFAULT;
    }

    msg_buffer[bytes_to_write] = '\0';

    pr_info("received: %s\n", msg_buffer);

    mutex_unlock(&buffer_lock);

    return bytes_to_write;
}

static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .read    = dev_read,
    .write   = dev_write,
};

/* ================= ProcFS ================= */

static ssize_t proc_read(struct file *file,
                         char __user *buf,
                         size_t len,
                         loff_t *offset)
{
    size_t data_len;

    mutex_lock(&buffer_lock);

    data_len = strlen(msg_buffer);

    if (*offset >= data_len) {
        mutex_unlock(&buffer_lock);
        return 0;
    }

    if (copy_to_user(buf,
                     msg_buffer,
                     data_len)) {
        mutex_unlock(&buffer_lock);
        return -EFAULT;
    }

    *offset += data_len;

    mutex_unlock(&buffer_lock);

    return data_len;
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
};

/* ================= SysFS ================= */

static ssize_t sysfs_show(struct kobject *kobj,
                          struct kobj_attribute *attr,
                          char *buf)
{
    ssize_t ret;

    mutex_lock(&buffer_lock);

    ret = scnprintf(buf,
                    BUFFER_SIZE,
                    "%s",
                    msg_buffer);

    mutex_unlock(&buffer_lock);

    return ret;
}

static struct kobj_attribute message_attr =
    __ATTR(message, 0444, sysfs_show, NULL);

/* ================= Module Init ================= */

static int __init char_driver_init(void)
{
    pr_info("loading module\n");

    mutex_init(&buffer_lock);

    major = register_chrdev(0,
                            DEVICE_NAME,
                            &fops);

    if (major < 0) {
        pr_err("failed to register char device\n");
        return major;
    }

    driver_class = class_create("simple_class");

    if (IS_ERR(driver_class)) {
        unregister_chrdev(major,
                           DEVICE_NAME);
        return PTR_ERR(driver_class);
    }

    driver_class->devnode = devnode;

    if (!device_create(driver_class,
                       NULL,
                       MKDEV(major, 0),
                       NULL,
                       DEVICE_NAME)) {

        class_destroy(driver_class);

        unregister_chrdev(major,
                          DEVICE_NAME);

        return -EINVAL;
    }

    proc_entry = proc_create(PROC_NAME,
                             0444,
                             NULL,
                             &proc_fops);

    if (!proc_entry)
        pr_warn("procfs creation failed\n");

    sysfs_kobj =
        kobject_create_and_add(SYS_DIR,
                               kernel_kobj);

    if (!sysfs_kobj ||
        sysfs_create_file(sysfs_kobj,
                          &message_attr.attr)) {

        pr_warn("sysfs creation failed\n");
    }

    pr_info("module loaded major=%d\n",
            major);

    return 0;
}

/* ================= Module Exit ================= */

static void __exit char_driver_exit(void)
{
    pr_info("unloading module\n");

    if (sysfs_kobj) {
        sysfs_remove_file(sysfs_kobj,
                          &message_attr.attr);

        kobject_put(sysfs_kobj);
    }

    if (proc_entry)
        proc_remove(proc_entry);

    device_destroy(driver_class,
                   MKDEV(major, 0));

    class_destroy(driver_class);

    unregister_chrdev(major,
                      DEVICE_NAME);

    mutex_destroy(&buffer_lock);
}

module_init(char_driver_init);
module_exit(char_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Karthikeya");
MODULE_DESCRIPTION("Linux character driver with procfs, sysfs, and mutex support");
