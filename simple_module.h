#ifndef SIMPLE_MODULE_H
#define SIMPLE_MODULE_H

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/kobject.h>

/* Names */
#define DEVICE_NAME "simple_char_dev"
#define PROC_NAME   "simple_proc"
#define SYS_DIR     "simple_sysfs"

/* Message */
#define MSG_BUFFER  "Hello from Kernel Space!\n"

/* Function prototypes (no static here) */
ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off);
ssize_t proc_read(struct file *file, char __user *buf, size_t len, loff_t *off);
ssize_t sys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);

#endif