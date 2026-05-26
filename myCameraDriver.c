#include <linux/module.h>
#include <linux/i2c.h>

#include "imx219_basic.h"

static int imx219_read_reg(struct i2c_client *client, u16 reg)
{
    return i2c_smbus_read_byte_data(client, reg);
}

static int imx219_probe(struct i2c_client *client)
{
    int chip_id;

    dev_info(&client->dev, "IMX219 probe\n");

    chip_id = imx219_read_reg(client, IMX219_REG_CHIP_ID);

    if (chip_id < 0) {
        dev_err(&client->dev, "chip id read failed\n");
        return chip_id;
    }

    dev_info(&client->dev,
             "chip id = 0x%x\n",
             chip_id);

    if (chip_id != IMX219_CHIP_ID) {
        dev_err(&client->dev,
                "unexpected sensor id\n");
        return -ENODEV;
    }

    dev_info(&client->dev,
             "IMX219 detected\n");

    return 0;
}

static void imx219_remove(struct i2c_client *client)
{
    dev_info(&client->dev,
             "driver removed\n");
}

static const struct i2c_device_id imx219_id[] = {
    { "imx219_basic", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, imx219_id);

static struct i2c_driver imx219_driver = {
    .driver = {
        .name = IMX219_NAME,
    },
    .probe = imx219_probe,
    .remove = imx219_remove,
    .id_table = imx219_id,
};

module_i2c_driver(imx219_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Minimal IMX219 skeleton driver");
