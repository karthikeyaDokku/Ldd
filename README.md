## Linux Character Device Driver
A concurrent Linux kernel module demonstrating IPC (Inter-Process Communication) and memory synchronization between user space and kernel space. This driver exposes a unified, thread-safe kernel buffer across three separate kernel interfaces: a standard character device (/dev), a process file system node (procfs), and a subsystem attribute (sysfs).
Designed, developed, and validated on an ARM-based Raspberry Pi 5 running Linux kernel v6.x.
------------------------------
## Architecture & Features
The driver initializes a shared internal memory block exposed concurrently via /dev/simple_char_dev, /proc/simple_proc, and /sys/kernel/simple_sysfs/message.
Key architectural components include:

* Unified State Isolation: All three interfaces point to the same underlying data structure, ensuring state consistency across different virtual file systems.
* Race Condition Mitigation: Multi-process access and concurrent operations are serialized using kernel mutexes (struct mutex) to preserve data integrity during simultaneous read/write cycles.
* Safe Subspace Boundary Copying: Employs copy_to_user() and copy_from_user() to execute hardware-safe, fault-tolerant memory transfers across user/kernel address space boundaries.

------------------------------
## Project Structure

.
├── char_driver.c       # Core driver source implementation
├── char_driver.h       # Header containing structures, macros, and prototypes
├── Makefile            # Kernel build automation script
├── README.md           # Documentation
├── LICENSE             # Project license
└── docs/               # Architecture diagrams and specifications

------------------------------
## Building and Running It## Prerequisites
You need matching kernel system headers to build the out-of-tree kernel module. On Raspberry Pi 5 running Raspberry Pi OS (Bookworm), install the default tracking headers:

sudo apt update
sudo apt install linux-headers-rpi-v8

(Note: If utilizing a 32-bit architecture variant on the Pi 5, substitute with linux-headers-rpi-v7l)
## Compilation
Clone the repository and compile the kernel module utilizing the local kbuild subsystem:

git clone <your-repository-url>
cd Ldd
make

## Module Loading
Insert the compiled kernel object (.ko) into the ring buffer:

sudo insmod char_driver.ko

Verify successful initialization and view the dynamically allocated major number via kernel logs:

dmesg | tail

Expected output structure:

[   ... ] loading module
[   ... ] module loaded major=509

------------------------------
## Interface Verification## 1. Character Device Subsystem
Write payload data into the character device node:

echo "System baseline operational" > /dev/simple_char_dev

Read out the persistent kernel storage buffer:

cat /dev/simple_char_dev

## 2. Process File System (procfs)
Query the read-only inspection node for rapid diagnostic checks:

cat /proc/simple_proc

## 3. Subsystem Kernel Objects (sysfs)
Query the object attribute structure to verify standard sysfs integration:

cat /sys/kernel/simple_sysfs/message

## Module Removal
Unload the driver from the kernel tree:

sudo rmmod char_driver

Review execution cycle metrics and proper teardown allocation sequences:

ls -l /dev/simple_char_dev

------------------------------
## Technical Implementation Details

* Dynamic Character Registration: Utilizes register_chrdev() to reserve major system numbers and assign the custom struct file_operations block to intercept standard system VFS callbacks (open, release, read, write).
* Mutex Synchronization: A centralized DEFINE_MUTEX(buffer_lock) architecture wraps around critical sections, blocking race windows if user-space routines parallelize operations.
* Sysfs Subsystem Mapping: Integrates native kernel attributes (SYSFS_ATTR) to format subsystem entries, matching production-grade driver standards for system configurations.
* Deterministic Resource Teardown: The module exit sequence forces complete hardware-safe registration rollbacks, releasing allocated file nodes, proc hooks, and sysfs attributes to prevent kernel memory leaks.

------------------------------
## Development Roadmap
Planned feature enhancements include:

* Implementing an ioctl() interface for out-of-band device control commands.
* Migrating the flat staging buffer into a circular ring-buffer topology to prevent overflow states.
* Integrating wait queues to transition blocking read() requests into low-overhead sleep states until payload events trigger.
* Introducing multiplexed file monitoring via poll/select API definitions.
* Upgrading the registration sequence from the legacy register_chrdev() mapping to the modern cdev layout structure.

------------------------------
## Environment Specs

* Hardware Platform: Raspberry Pi 5 (ARM Architecture)
* Operating System: Linux Kernel v6.x
* Toolchain: GCC, GNU Make, Kbuild

------------------------------
## Author
Karthikeya
## License
Distributed under the GPL License. See LICENSE for details.
