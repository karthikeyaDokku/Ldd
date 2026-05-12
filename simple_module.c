#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/mutex.h>

#include "simple_module.h"

/* globals */
static int major;
static struct proc_dir_entry *proc_file;
static struct kobject *kobj_ref;
static struct class *cls;

/* mutex for shared buffer */
static DEFINE_MUTEX(my_mutex);

/* kernel buffer */
static char msg_buffer[BUFFER_SIZE] = "Hello from Kernel Space!\n";

/* ===== open ===== */
static int dev_open(struct inode *inode, struct file *file)
{
    pr_info("device opened\n");
    return 0;
}

/* ===== close ===== */
static int dev_close(struct inode *inode, struct file *file)
{
    pr_info("device closed\n");
    return 0;
}

/* ===== read ===== */
static ssize_t dev_read(struct file *file,
                        char __user *buf,
                        size_t len,
                        loff_t *off)
{
    int bytes;

    mutex_lock(&my_mutex);

    bytes = strlen(msg_buffer) - *off;

    if (bytes <= 0) {
        mutex_unlock(&my_mutex);
        return 0;
    }

    if (bytes > len)
        bytes = len;

    if (copy_to_user(buf, msg_buffer + *off, bytes)) {
        mutex_unlock(&my_mutex);
        return -EFAULT;
    }

    *off += bytes;

    mutex_unlock(&my_mutex);

    return bytes;
}

/* ===== write ===== */
static ssize_t dev_write(struct file *file,
                         const char __user *buf,
                         size_t len,
                         loff_t *off)
{
    mutex_lock(&my_mutex);

    if (len > BUFFER_SIZE - 1)
        len = BUFFER_SIZE - 1;

    if (copy_from_user(msg_buffer, buf, len)) {
        mutex_unlock(&my_mutex);
        return -EFAULT;
    }

    msg_buffer[len] = '\0';

    pr_info("received: %s\n", msg_buffer);

    mutex_unlock(&my_mutex);

    return len;
}

/* ===== char driver ops ===== */
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_close,
    .read    = dev_read,
    .write   = dev_write,
};

/* ===== procfs ===== */
static ssize_t proc_read(struct file *file,
                         char __user *buf,
                         size_t len,
                         loff_t *off)
{
    int bytes;

    mutex_lock(&my_mutex);

    bytes = strlen(msg_buffer);

    if (*off > 0) {
        mutex_unlock(&my_mutex);
        return 0;
    }

    if (copy_to_user(buf, msg_buffer, bytes)) {
        mutex_unlock(&my_mutex);
        return -EFAULT;
    }

    *off += bytes;

    mutex_unlock(&my_mutex);

    return bytes;
}

static const struct proc_ops p_fops = {
    .proc_read = proc_read,
};

/* ===== sysfs ===== */
static ssize_t sys_show(struct kobject *kobj,
                        struct kobj_attribute *attr,
                        char *buf)
{
    ssize_t ret;

    mutex_lock(&my_mutex);

    ret = sprintf(buf, "%s", msg_buffer);

    mutex_unlock(&my_mutex);

    return ret;
}

static struct kobj_attribute sys_attr =
    __ATTR(message, 0444, sys_show, NULL);

/* ===== init ===== */
static int __init simple_init(void)
{
    pr_info("module init\n");

    /* init mutex */
    mutex_init(&my_mutex);

    /* register char device */
    major = register_chrdev(0, DEVICE_NAME, &fops);

    if (major < 0) {
        pr_err("char device failed\n");
        return major;
    }

    /* create /dev/simple_char_dev */
    cls = class_create("simple_class");

    if (IS_ERR(cls)) {
        unregister_chrdev(major, DEVICE_NAME);
        return PTR_ERR(cls);
    }

    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    /* create /proc/simple_proc */
    proc_file = proc_create(PROC_NAME, 0444, NULL, &p_fops);

    if (!proc_file)
        pr_err("proc creation failed\n");

    /* create sysfs entry */
    kobj_ref = kobject_create_and_add(SYS_DIR, kernel_kobj);

    if (!kobj_ref || sysfs_create_file(kobj_ref, &sys_attr.attr))
        pr_err("sysfs creation failed\n");

    pr_info("module loaded major=%d\n", major);

    return 0;
}

/* ===== exit ===== */
static void __exit simple_exit(void)
{
    pr_info("module exit\n");

    /* remove sysfs */
    if (kobj_ref) {
        sysfs_remove_file(kobj_ref, &sys_attr.attr);
        kobject_put(kobj_ref);
    }

    /* remove procfs */
    if (proc_file)
        proc_remove(proc_file);

    /* remove char device */
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);

    unregister_chrdev(major, DEVICE_NAME);

    mutex_destroy(&my_mutex);
}

module_init(simple_init);
module_exit(simple_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("karthikeya");
MODULE_DESCRIPTION("Simple character driver with read write procfs sysfs and mutex");