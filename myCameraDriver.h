#ifndef _MY_IMX219_H_
#define _MY_IMX219_H_

#define IMX219_NAME            "imx219_basic"

/* chip id */
#define IMX219_REG_CHIP_ID_H   0x0000
#define IMX219_REG_CHIP_ID_L   0x0001
#define IMX219_CHIP_ID         0x0219

/* stream */
#define IMX219_REG_MODE_SELECT 0x0100
#define IMX219_STREAM_OFF      0x00
#define IMX219_STREAM_ON       0x01

/* gain */
#define IMX219_ANALOG_GAIN     0x0157

/* exposure */
#define IMX219_EXPOSURE_H      0x015A
#define IMX219_EXPOSURE_L      0x015B

/* flip */
#define IMX219_ORIENTATION     0x0172

/* test pattern */
#define IMX219_TEST_PATTERN    0x0600

#endif
