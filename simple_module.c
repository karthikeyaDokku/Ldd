#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include "simple_module.h"

static int major;
static struct proc_dir_entry *proc_file;
static struct kobject *kobj_ref;

/* 1. Char Driver Ops */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = dev_read,
};

static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    int bytes = strlen(MSG_BUFFER) - *off;
    if (bytes <= 0) return 0;
    if (copy_to_user(buf, MSG_BUFFER + *off, bytes)) return -EFAULT;
    *off += bytes;
    return bytes;
}

/* 2. Procfs Ops */
static const struct proc_ops p_fops = {
    .proc_read = proc_read,
};

static ssize_t proc_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    if (*off > 0) return 0;
    if (copy_to_user(buf, MSG_BUFFER, strlen(MSG_BUFFER))) return -EFAULT;
    *off += strlen(MSG_BUFFER);
    return strlen(MSG_BUFFER);
}

/* 3. Sysfs Ops */
static struct kobj_attribute sys_attr = __ATTR(message, 0444, sys_show, NULL);

static ssize_t sys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%s", MSG_BUFFER);
}

/* Module Init & Exit */
static int __init simple_init(void) {
    // Char Dev
    major = register_chrdev(0, DEVICE_NAME, &fops);

    // Procfs
    proc_file = proc_create(PROC_NAME, 0444, NULL, &p_fops);

    // Sysfs
    kobj_ref = kobject_create_and_add(SYS_DIR, kernel_kobj);
    if (sysfs_create_file(kobj_ref, &sys_attr.attr)) {
        pr_err("Sysfs creation failed\n");
    }

    pr_info("Module loaded. Char Major: %d\n", major);
    return 0;
}

static void __exit simple_exit(void) {
    sysfs_remove_file(kobj_ref, &sys_attr.attr);
    kobject_put(kobj_ref);
    proc_remove(proc_file);
    unregister_chrdev(major, DEVICE_NAME);
    pr_info("Module unloaded\n");
}

module_init(simple_init);
module_exit(simple_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Developer");
MODULE_DESCRIPTION("A simple hybrid character/proc/sysfs driver");
