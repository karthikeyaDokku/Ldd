/* headerfiles inclusion*/
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include "imx219_basic.h"

#ifndef __IMX219_BASIC_H__
#define __IMX219_BASIC_H__

#define IMX219_NAME               "imx219_basic"

#define IMX219_CHIP_ID_REG        0x0000
#define IMX219_CHIP_ID            0x0219

#define IMX219_MODE_SELECT        0x0100
#define IMX219_STREAM_ON          0x01
#define IMX219_STREAM_OFF         0x00

#endif
