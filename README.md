# IMX219 Camera Driver Learning Project

## Overview

This project is a simple Linux kernel I2C driver created for learning how camera drivers work internally.

The target sensor is the Sony IMX219 camera module commonly used with Raspberry Pi boards.

The goal of this project is not to replace the official Raspberry Pi camera driver, but to understand:

* Linux device drivers
* I2C communication
* Camera sensor detection
* Register read/write operations
* Driver probe and remove flow
* Basics of V4L2 camera architecture

## Current Features

* Linux kernel module
* I2C driver registration
* IMX219 sensor detection
* Chip ID verification
* Kernel log messages for debugging

## Learning Objectives

Through this project I aim to understand:

1. How Linux detects camera sensors
2. How I2C is used to communicate with image sensors
3. How camera drivers initialize hardware
4. How streaming is enabled and disabled
5. How V4L2 camera drivers are structured
6. How image data moves from the sensor to user space

## Build

```bash
make
```

## Load Driver

```bash
sudo insmod myCameraDriver.ko
```

Verify:

```bash
lsmod | grep myCameraDriver
```

Check kernel messages:

```bash
dmesg | tail
```

## Remove Driver

```bash
sudo rmmod myCameraDriver
```

## Project Status

This is an educational project and is currently a minimal driver used to study camera-driver fundamentals.

Future work:

* Sensor register access
* Stream ON/OFF control
* V4L2 integration
* Basic camera controls
* Frame capture path exploration
