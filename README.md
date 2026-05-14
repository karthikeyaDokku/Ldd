
Linux Characater Device Driver

A Linux kernel module showing communication between user space and kernel space using a shared buffer.
The driver exposes the same data through /dev, procfs, and sysfs with mutex-based synchronization for safe concurrent access.
Built and tested on a Raspberry Pi 5 running Linux kernel v6.x.

