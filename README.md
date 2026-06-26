# IMX219 Camera Driver Learning Project

## Overview
This project is a simple Linux kernel I2C driver created for learning how camera drivers work internally. The target sensor is the Sony IMX219 camera module commonly used with Raspberry Pi boards.

The goal of this project is not to replace the official Raspberry Pi camera driver, but to understand:
* Linux device drivers
* I2C communication architectures
* Camera sensor detection & Register mapping
* Unit testing kernel-space code (White-Box Testing)
* Driver probe and remove lifecycle flows
* Basics of V4L2 camera architecture

---

## Current Features
* **Linux Kernel Module:** Registers as a native standalone I2C client.
* **IMX219 Sensor Detection:** Handles 16-bit register tracking, checking hardware signatures dynamically.
* **Chip ID Verification:** Guards hardware logic integrity during initialization.
* **White-Box Test Suite:** Built using the official Linux **KUnit** framework to intercept code branches (error injection, missing hardware states, context verification) without requiring real camera hardware connected to the host PC.

---

## Learning Objectives
1. How Linux detects camera sensors via the I2C subsystem.
2. How to structure reliable 16-bit word addresses over I2C data transfers.
3. How camera drivers initialize, handle power cycles, and interact with V4L2 subsystems.
4. **How to perform white-box testing on internal `static` kernel functions using modern framework mocking architectures (KUnit).**
5. How image data moves from raw physical sensor buses up to Linux user space.

---

## Testing Framework (White-Box Testing)
To maintain code coverage and isolate logic changes without hitting real hardware, this module includes an integrated unit test file (`myCameraDriver_test.c`).

### 1. Ensure KUnit is Loaded on Host Machine
Before compilation and test execution, load the kernel's native test engine symbols:
```bash
sudo modprobe kunit
```

### 2. Supported Test Scenarios
* **`test_imx219_probe_read_fail`**: Verifies that probe cascades errors cleanly if the I2C read interface blocks.
* **`test_imx219_probe_wrong_chip_id`**: Assures that invalid hardware signatures reject with a strict `-ENODEV` contract.
* **`test_imx219_probe_success`**: Validates the happy path, handling sensor layout registration cleanly.
* **`test_imx219_streaming_controls`**: Isolates independent evaluation over streaming boundaries.

---

## Build

To compile both the primary driver infrastructure and bake in the active KUnit testing harness, run:
```bash
make
```

---

## Load Driver & Run Tests

Because the framework runs out-of-tree, loading the module triggers KUnit to execute the test suite **instantly**:

```bash
# Clear the old kernel print logs
sudo dmesg -C

# Load the compiled driver binary
sudo insmod myCameraDriver.ko

# Inspect test results instantly
dmesg
```

Verify standard registration hooks:
```bash
lsmod | grep myCameraDriver
```

---

## Remove Driver

```bash
sudo rmmod myCameraDriver
```

---

## Project Status & Next Steps
This is an educational project and is currently a verified, self-testing minimal driver used to study camera-driver fundamentals.

Future work:
* Complete implementation of full V4L2 subdev integration.
* Advanced runtime frame layout configurations (resolution, cropping, and format selection).
* Implementing asynchronous subdevice binding rules.
