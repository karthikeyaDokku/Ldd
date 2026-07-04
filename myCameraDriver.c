#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include "myCameraDriver.h"

static int imx219_write_reg(struct i2c_client *client,
                            u16 reg,
                            u8 val)
{
    u8 buf[3];

    buf[0] = reg >> 8;
    buf[1] = reg & 0xff;
    buf[2] = val;

    dev_info(&client->dev,
             "WRITE reg=0x%04x val=0x%02x\n",
             reg,
             val);

    if (i2c_master_send(client, buf, 3) != 3)
        return -EIO;

    return 0;
}

static int imx219_read_reg(struct i2c_client *client,
                           u16 reg)
{
    u8 addr[2];
    u8 data;

    addr[0] = reg >> 8;
    addr[1] = reg & 0xff;

    if (i2c_master_send(client, addr, 2) != 2)
        return -EIO;

    if (i2c_master_recv(client, &data, 1) != 1)
        return -EIO;

    return data;
}

static int imx219_read_chipid(struct i2c_client *client)
{
    int high;
    int low;

    high = imx219_read_reg(client,
                           IMX219_REG_CHIP_ID_H);

    if (high < 0)
        return high;

    low = imx219_read_reg(client,
                          IMX219_REG_CHIP_ID_L);

    if (low < 0)
        return low;

    return (high << 8) | low;
}

static int imx219_start_stream(struct i2c_client *client)
{
    dev_info(&client->dev,
             "STREAM ON\n");

    return imx219_write_reg(client,
                            IMX219_REG_MODE_SELECT,
                            IMX219_STREAM_ON);
}

static int imx219_stop_stream(struct i2c_client *client)
{
    dev_info(&client->dev,
             "STREAM OFF\n");

    return imx219_write_reg(client,
                            IMX219_REG_MODE_SELECT,
                            IMX219_STREAM_OFF);
}

static int imx219_set_gain(struct i2c_client *client,
                           u8 gain)
{
    dev_info(&client->dev,
             "GAIN=%u\n",
             gain);

    return imx219_write_reg(client,
                            IMX219_ANALOG_GAIN,
                            gain);
}

static int imx219_set_exposure(struct i2c_client *client,
                               u16 exposure)
{
    int ret;

    dev_info(&client->dev,
             "EXPOSURE=%u\n",
             exposure);

    ret = imx219_write_reg(client,
                           IMX219_EXPOSURE_H,
                           exposure >> 8);

    if (ret)
        return ret;

    return imx219_write_reg(client,
                            IMX219_EXPOSURE_L,
                            exposure & 0xff);
}

static int imx219_set_test_pattern(struct i2c_client *client,
                                   u8 pattern)
{
    dev_info(&client->dev,
             "TEST_PATTERN=%u\n",
             pattern);

    return imx219_write_reg(client,
                            IMX219_TEST_PATTERN,
                            pattern);
}

static int imx219_set_flip(struct i2c_client *client,
                           u8 flip)
{
    dev_info(&client->dev,
             "FLIP=%u\n",
             flip);

    return imx219_write_reg(client,
                            IMX219_ORIENTATION,
                            flip);
}

static int imx219_probe(struct i2c_client *client)
{
    int chip_id;
    int ret;

    dev_info(&client->dev,
             "===== IMX219 PROBE =====\n");

    chip_id = imx219_read_chipid(client);

    if (chip_id < 0) {
        dev_err(&client->dev,
                "chip id read failed\n");
        return chip_id;
    }

    dev_info(&client->dev,
             "chip id = 0x%04x\n",
             chip_id);

    if (chip_id != IMX219_CHIP_ID) {
        dev_err(&client->dev,
                "wrong sensor\n");
        return -ENODEV;
    }

    /* stop sensor */
    ret = imx219_stop_stream(client);
    if (ret)
        return ret;

    msleep(100);

    /*
     * CUSTOM EXPERIMENTS
     */

    /* enable color bars */
    imx219_set_test_pattern(client, 1);

    /* increase gain */
    imx219_set_gain(client, 64);

    /* set exposure */
    imx219_set_exposure(client, 0x0800);

    /* flip image */
    imx219_set_flip(client, 0);

    msleep(100);

    /* start streaming */
    ret = imx219_start_stream(client);

    if (ret)
        return ret;

    dev_info(&client->dev,
             "IMX219 CUSTOM DRIVER READY\n");

    return 0;
}

static void imx219_remove(struct i2c_client *client)
{
    imx219_stop_stream(client);

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
MODULE_AUTHOR("Karthikeya");
MODULE_DESCRIPTION("Custom IMX219 experimentation driver");

