#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>

#include "simple_module.h"

/* globals */
static int major;
static struct proc_dir_entry *proc_file;
static struct kobject *kobj_ref;
static struct class *cls;

/* kernel buffer */
static char msg_buffer[BUFFER_SIZE] = "Hello from Kernel Space!\n";

/* ===== read ===== */
static ssize_t dev_read(struct file *file,
                        char __user *buf,
                        size_t len,
                        loff_t *off)
{
    int bytes = strlen(msg_buffer) - *off;

    if (bytes <= 0)
        return 0;

    /* don't copy more than requested */
    if (bytes > len)
        bytes = len;

    if (copy_to_user(buf, msg_buffer + *off, bytes))
        return -EFAULT;

    *off += bytes;

    return bytes;
}

/* ===== write ===== */
static ssize_t dev_write(struct file *file,
                         const char __user *buf,
                         size_t len,
                         loff_t *off)
{
    /* avoid overflow */
    if (len > BUFFER_SIZE - 1)
        len = BUFFER_SIZE - 1;

    /* copy user data into kernel buffer */
    if (copy_from_user(msg_buffer, buf, len))
        return -EFAULT;

    msg_buffer[len] = '\0';

    pr_info("received: %s\n", msg_buffer);

    return len;
}

/* ===== char driver ops ===== */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = dev_read,
    .write = dev_write,
};

/* ===== procfs ===== */
static ssize_t proc_read(struct file *file,
                         char __user *buf,
                         size_t len,
                         loff_t *off)
{
    int bytes = strlen(msg_buffer);

    if (*off > 0)
        return 0;

    if (copy_to_user(buf, msg_buffer, bytes))
        return -EFAULT;

    *off += bytes;

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
    return sprintf(buf, "%s", msg_buffer);
}

static struct kobj_attribute sys_attr =
    __ATTR(message, 0444, sys_show, NULL);

/* ===== init ===== */
static int __init simple_init(void)
{
    pr_info("module init\n");

    /* register char device */
    major = register_chrdev(0, DEVICE_NAME, &fops);

    if (major < 0) {
        pr_err("char device failed\n");
        return major;
    }

    /* create /dev/simple_char_dev */
    cls = class_create("simple_class");
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
    sysfs_remove_file(kobj_ref, &sys_attr.attr);
    kobject_put(kobj_ref);

    /* remove procfs */
    proc_remove(proc_file);

    /* remove char device */
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);

    unregister_chrdev(major, DEVICE_NAME);
}

module_init(simple_init);
module_exit(simple_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("karthikeya");
MODULE_DESCRIPTION("Simple character driver with procfs and sysfs");