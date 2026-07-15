# Custom IMX219 Camera Driver (Raspberry Pi 5)

A standalone Linux kernel driver for the Sony IMX219 camera sensor, written
from scratch and running independently of Raspberry Pi's own built-in driver.

## Why I built this

Raspberry Pi ships a working IMX219 driver out of the box, so this isn't
about making the camera "work" for the first time — it's about understanding
what a camera driver actually has to do under the hood: power sequencing,
device tree binding, I2C register control, and (partially) integration into
the Linux V4L2 camera framework.

## What it actually does

- Powers the sensor on correctly (clock, voltage regulators, reset) instead
  of relying on the stock driver to have already done it
- Talks to the sensor over I2C and confirms it's really an IMX219 by
  reading its chip ID register
- Controls basic sensor settings directly: gain, exposure, test pattern,
  image flip
- Binds to the hardware through a custom Device Tree overlay, so the kernel
  knows which physical clock/GPIO/regulators belong to this sensor

## How the pieces fit together

```
 ┌─────────────────────────┐
 │   Device Tree Overlay    │   tells the kernel: "there's a sensor
 │  (imx219_basic-overlay)  │   at I2C address 0x10, here is its
 └────────────┬─────────────┘   clock / regulators / reset line"
              │
              ▼
 ┌─────────────────────────┐
 │   Kernel matches driver  │   compatible string in the overlay
 │   to the device node     │   matches compatible string in driver
 └────────────┬─────────────┘
              │
              ▼
 ┌─────────────────────────┐
 │        probe()           │
 └────────────┬─────────────┘
              │
              ▼
      ┌───────────────┐
      │ get resources │   clk, reset gpio, 3x regulator (VANA/VDIG/VDDL)
      └───────┬───────┘
              ▼
      ┌───────────────┐
      │   power on     │   enable clock → enable regulators →
      │                │   toggle reset → wait for sensor boot
      └───────┬───────┘
              ▼
      ┌───────────────┐
      │  read chip id  │   confirm register 0x0000/0x0001 == 0x0219
      └───────┬───────┘
              ▼
        chip id wrong? ──yes──► fail probe, power off
              │ no
              ▼
      ┌───────────────┐
      │ configure regs │   test pattern, gain, exposure, flip
      └───────┬───────┘
              ▼
      ┌───────────────┐
      │  start stream  │
      └───────┬───────┘
              ▼
        driver ready
```

## The power-on sequence in detail

This is the part that actually matters — the built-in driver does this for
you, so a naive custom driver "works" only by accident (piggybacking on
power already being on). This driver does it itself:

```
 clk_prepare_enable(xclk)          → turn on the 24MHz clock the sensor needs
        │
        ▼
 regulator_bulk_enable()           → turn on VANA (2.8V), VDIG (1.8V), VDDL (1.2V)
        │
        ▼
 wait ~6-7ms                       → let the clock stabilize
        │
        ▼
 toggle reset gpio (if present)    → most Pi camera modules don't expose one;
        │                            reset happens internally on the sensor
        ▼
 wait ~6ms                          → sensor internal boot time
        │
        ▼
 ready for I2C register access
```

## Files

| File | Purpose |
|---|---|
| `imx219.c` | The driver itself |
| `imx219.h` | Register addresses, clock frequency, delay constants |
| `imx219_basic-overlay.dts` | Device tree overlay binding the driver to CAM0 |
| `Makefile` | Standard out-of-tree kernel module build |

## Building and loading

```bash
make clean
make
sudo insmod imx219.ko
dmesg | tail -30
```

A working load looks like this in the kernel log:

```
===== IMX219 PROBE =====
chip id = 0x0219
STREAM OFF
TEST_PATTERN=1
GAIN=64
EXPOSURE=2048
FLIP=0
STREAM ON
IMX219 CUSTOM DRIVER READY
```

To unload:
```bash
sudo rmmod imx219
```

## The device tree overlay — why it was the hard part

A device tree overlay is essentially a small config patch applied to the
running system's hardware description. The driver's resource lookups
(`devm_clk_get`, `devm_gpiod_get_optional`, `devm_regulator_bulk_get`) don't
invent hardware — they look up real properties on the sensor's device tree
node. Without a matching overlay, every one of those lookups fails.

Getting this right on a Raspberry Pi 5 meant decompiling Raspberry Pi's own
official IMX219 overlay (`/boot/firmware/overlays/imx219.dtbo`) to find the
real label names for CAM0's clock (`cam0_clk`), regulators (`cam0_reg`,
`cam_dummy_reg`), and CSI/I2C routing (`i2c_csi_dsi0`, `csi0`) — these are
specific to how the Pi 5's RP1 I/O chip wires up the two camera ports, and
aren't something you can guess correctly from general Linux driver
documentation.

## Current status

**Working:**
- Full power-on/power-off sequence
- Chip ID verification over I2C
- Register-level control of gain, exposure, test pattern, orientation
- Clean load/unload cycle
- Custom device tree overlay binding

**In progress / not finished:**
- V4L2 subdevice registration (this is what would let standard tools like
  `v4l2-ctl` or camera apps pull actual image frames through the driver).
  The sensor does show up correctly in the kernel's media pipeline graph,
  but full frame streaming through the ISP wasn't completed.
- Real image format/resolution switching — the sensor currently runs at a
  single fixed mode; supporting multiple resolutions needs the IMX219's
  actual mode-select register tables, which this driver doesn't have yet.

## What I learned

The camera "just working" on a stock Raspberry Pi hides a fair amount of
plumbing: clock management, regulator sequencing, device tree binding, and
(if you want real frames) the V4L2 media controller framework. Writing this
from scratch — and hitting real, unglamorous bugs along the way (a wrong
clock label in the overlay, a missing subdevice state initializer, module
reference counting on unload) — was a much more useful way to actually
understand how a Linux camera driver works than reading the finished
mainline driver would have been.
