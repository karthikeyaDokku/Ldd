# Linux Device Driver Learning (Raspberry Pi)

This repository contains my work while learning Linux device drivers on Raspberry Pi. I started with a simple GPIO driver to understand how the kernel interacts with hardware, and I am now moving towards writing character device drivers.

The GPIO driver uses memory-mapped I/O to access BCM2711 registers and control pins. It takes input from user space through a `/proc` entry, where I can turn a pin ON or OFF using simple commands like:

echo "17,1" | sudo tee /proc/gpio_ctrl  
echo "17,0" | sudo tee /proc/gpio_ctrl  

While working on this, I learned how kernel modules are loaded, how data moves from user space to kernel space, and how hardware registers are accessed.

I am currently working on a simple character driver that creates a `/dev` device and supports basic `read()` and `write()` operations. This is helping me understand how real device drivers are structured in Linux.

To build and run:

make  
sudo insmod <module>.ko  
dmesg | tail  

This is a learning project and I’m improving it step by step.

License: GPL-2.0