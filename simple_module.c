#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>

#include "simple_module.h"

/* Globals */
static int major;
static struct proc_dir_entry *proc_file;
static struct kobject *kobj_ref;
static struct class *cls;

/* ===== Char Driver ===== */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = dev_read,
};

ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    int msg_len = strlen(MSG_BUFFER);
    int bytes = msg_len - *off;

    if (bytes <= 0)
        return 0;

    if (copy_to_user(buf, MSG_BUFFER + *off, bytes))
        return -EFAULT;

    *off += bytes;
    return bytes;
}

/* ===== Procfs ===== */
static const struct proc_ops p_fops = {
    .proc_read = proc_read,
};

ssize_t proc_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    if (*off > 0)
        return 0;

    if (copy_to_user(buf, MSG_BUFFER, strlen(MSG_BUFFER)))
        return -EFAULT;

    *off += strlen(MSG_BUFFER);
    return strlen(MSG_BUFFER);
}

/* ===== Sysfs ===== */
static struct kobj_attribute sys_attr =
    __ATTR(message, 0444, sys_show, NULL);

ssize_t sys_show(struct kobject *kobj,
                 struct kobj_attribute *attr,
                 char *buf)
{
    return sprintf(buf, "%s", MSG_BUFFER);
}

/* ===== Init ===== */
static int __init simple_init(void)
{
    pr_info("module init\n");

    /* Char device */
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        pr_err("char device failed\n");
        return major;
    }

    /* Create /dev node */
    cls = class_create("simple_class");
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    /* Procfs */
    proc_file = proc_create(PROC_NAME, 0444, NULL, &p_fops);
    if (!proc_file)
        pr_err("proc creation failed\n");

    /* Sysfs */
    kobj_ref = kobject_create_and_add(SYS_DIR, kernel_kobj);
    if (!kobj_ref || sysfs_create_file(kobj_ref, &sys_attr.attr))
        pr_err("sysfs creation failed\n");

    pr_info("module loaded. major=%d\n", major);
    return 0;
}

/* ===== Exit ===== */
static void __exit simple_exit(void)
{
    pr_info("module exit\n");

    /* Sysfs */
    sysfs_remove_file(kobj_ref, &sys_attr.attr);
    kobject_put(kobj_ref);

    /* Procfs */
    proc_remove(proc_file);

    /* Char device */
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, DEVICE_NAME);
}

module_init(simple_init);
module_exit(simple_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("karthikeya");
MODULE_DESCRIPTION("Simple char + proc + sysfs driver");