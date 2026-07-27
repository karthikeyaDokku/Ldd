# Linux Character Device Driver with ProcFS, SysFS, and Mutex Support

A comprehensive, production-grade Linux kernel module demonstrating a character device driver with concurrent access protection, a dynamic device node permissions policy, and integrations into both **ProcFS** and **SysFS**.

---

## Features

* **Character Device (`/dev`)**: Supports standard file operations (`open`, `release`, `read`, `write`) with safe user-space data copying (`copy_to_user`, `copy_from_user`).
* **Custom Permissions (`devnode`)**: Automatically configures device node permissions to `0666` (read/write access for all users) upon creation, bypassing default root-only restrictions.
* **ProcFS Integration (`/proc`)**: Exposes a read-only virtual file to query the current kernel message buffer.
* **SysFS Integration (`/sys`)**: Implements a custom `kobject` and attribute file (`message`) to expose kernel metrics/buffers via sysfs.
* **Concurrency Control**: Utilizes kernel mutex locks (`DEFINE_MUTEX`) to completely prevent race conditions during simultaneous multi-process reads and writes.
* **Safe Resource Management**: Implements strict, ordered cleanup during module unloading to prevent kernel memory leaks and dangling pointers.

---

## Project Structure

```text
.
├── char_driver.c        # Main kernel module implementation source code
├── char_driver.h        # Header file containing constants, macros, and declarations
└── Makefile             # Kernel module build script

```

---

## Prerequisites & Dependencies

To build and test this kernel module, your system requires:

* A Linux host or virtual machine (with kernel development headers installed).
* `build-essential` tools (`gcc`, `make`).
* Linux kernel headers matching your running kernel version (e.g., `linux-headers-$(uname -r)` on Debian/Ubuntu derivatives).

---

## Building the Module

1. Place `char_driver.c`, `char_driver.h`, and your `Makefile` into the same directory.
2. Compile the kernel module by running:
```bash
make

```


This generates the compiled kernel object file (`char_driver.ko`), along with support files.

---

## Usage Instructions

### 1. Loading the Module

Insert the compiled module into the kernel using `insmod` (requires root privileges):

```bash
sudo insmod char_driver.ko

```

*Check the kernel log to verify successful loading and note the assigned major number:*

```bash
dmesg | tail -n 10

```

### 2. Interacting with the Character Device

Once loaded, the device file will automatically be created under `/dev/` (using the name defined by `DEVICE_NAME` in your headers).

* **Reading from the device:**
```bash
cat /dev/simple_char_device

```


*(Default output: `Hello from Kernel Space!`)*
* **Writing to the device:**
```bash
echo "Hello from User Space!" > /dev/simple_char_device

```


* **Reading back the updated message:**
```bash
cat /dev/simple_char_device

```



### 3. Interacting via ProcFS

Query the driver state using the ProcFS virtual file entry:

```bash
cat /proc/simple_proc_entry

```

### 4. Interacting via SysFS

Read the custom sysfs attribute file managed by the driver's custom `kobject`:

```bash
cat /sys/kernel/simple_sys_dir/message

```

### 5. Unloading the Module

Remove the module from the kernel using `rmmod`:

```bash
sudo rmmod char_driver

```

*Verify complete teardown and resource cleanup:*

```bash
dmesg | tail -n 10

```

---

## Technical Architecture & Flow

### Initialization Flow (`__init`)

1. **Mutex Initialization**: Sets up `buffer_lock` for thread safety.
2. **Char Device Registration**: Calls `register_chrdev()` to acquire a dynamic major number and register file operations (`fops`).
3. **Class Creation**: Calls `class_create()` to set up a sysfs device class and binds the `devnode` callback for permission hardening (`0666`).
4. **Device Node Creation**: Calls `device_create()` to populate the node in `/dev/`.
5. **Virtual Filesystem Setup**: Registers the ProcFS node via `proc_create()` and sets up the SysFS kobject structure via `kobject_create_and_add()` and `sysfs_create_file()`.

### Exit Flow (`__exit`)

Performs a strict, step-by-step teardown in **exact reverse order** of initialization:

1. Removes SysFS attributes and destroys the `kobject`.
2. Removes the ProcFS entry.
3. Destroys the device node and device class.
4. Unregisters the character device major number.
5. Destroys the mutex lock.

---

## License

This project is licensed under the **GNU General Public License (GPL)**. See the `MODULE_LICENSE("GPL")` declaration inside the source code.
