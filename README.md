

\#  Raspberry Pi 4 GPIO LED Driver (Linux Kernel Module)



A custom \*\*Linux kernel module\*\* to control GPIO pins on Raspberry Pi 4 using \*\*direct memory-mapped I/O\*\*.

This project demonstrates low-level hardware interaction from kernel space and user–kernel communication via `/proc`.



\---



\# Features



\* Loadable Kernel Module (`.ko`)

\* Direct access to BCM2711 GPIO registers

\* `/proc` interface for user-space control

\* Safe data transfer using `copy\_from\_user`

\* GPIO ON/OFF control via simple commands



\---



\# Architecture Overview



```

+-------------------+

|   User Space      |

|-------------------|

| echo "17,1"       |

| /proc/gpio\_ctrl   |

+--------+----------+

&#x20;        |

&#x20;        | write()

&#x20;        ↓

+-------------------+

|   Kernel Space    |

|-------------------|

| GPIO Driver       |

| (/proc interface) |

+--------+----------+

&#x20;        |

&#x20;        | Memory-mapped I/O

&#x20;        ↓

+---------------------------+

| BCM2711 GPIO Registers    |

| (0xfe200000 base addr)    |

+---------------------------+

&#x20;        |

&#x20;        ↓

+-------------------+

|   GPIO Hardware   |

|   (LED ON/OFF)    |

+-------------------+

```



\---



\#  How It Works



1\. User writes command:



&#x20;  ```

&#x20;  echo "17,1" > /proc/gpio\_ctrl

&#x20;  ```



2\. Kernel receives it via `write()`



3\. Driver:



&#x20;  \* Parses input (`pin, value`)

&#x20;  \* Configures GPIO as output

&#x20;  \* Writes to GPIO registers



4\. LED turns ON/OFF



\---



\# GPIO Register Flow



```

Input: "17,1"



→ Parse:

&#x20;  pin = 17

&#x20;  value = 1



→ Configure Pin:

&#x20;  GPFSEL register (set OUTPUT mode)



→ Control Pin:

&#x20;  GPSET → HIGH (ON)

&#x20;  GPCLR → LOW  (OFF)

```



\---



\# Installation \& Usage



1\. Install dependencies



```bash

sudo apt install build-essential linux-headers-$(uname -r)

```



\---



2\. Build the module



```bash

make

```



\---



&#x20;3. Load the driver



```bash

sudo insmod gpio\_driver.ko

```



\---



&#x20;4. Verify



```bash

lsmod | grep gpio\_driver

dmesg | tail

```



\---



5\. Control GPIO



```bash

echo "17,1" > /proc/gpio\_ctrl   # Turn ON

echo "17,0" > /proc/gpio\_ctrl   # Turn OFF

```



\---



&#x20;Blinking Example (User Space)



```bash

while true; do

&#x20; echo "17,1" > /proc/gpio\_ctrl

&#x20; sleep 1

&#x20; echo "17,0" > /proc/gpio\_ctrl

&#x20; sleep 1

done

```



\---



&#x20;Project Structure



```

.

├── gpio\_driver.c

├── gpio\_driver.h

├── Makefile

├── README.md

└── LICENSE

```



\---



\## 🧠 Concepts Demonstrated



\* Linux Kernel Module Development

\* Memory-Mapped I/O (`ioremap`)

\* GPIO Register Manipulation

\* User ↔ Kernel Communication

\* `/proc` filesystem usage

\* Bitwise operations for hardware control



\---



\#Limitations



\* Uses `/proc` (not modern `sysfs`)

\* No concurrency handling (mutex/spinlock)

\* Limited GPIO range validation

\* No interrupt support



\---



\# License



This project is licensed under the \*\*GNU General Public License v2.0 (GPL-2.0)\*\*.



\---





