#include "myCameraDriver.h"


struct imx219 {
    struct i2c_client *client;
};

/* sensor Detection"*/

static int imx219_write_reg(struct i2c_client *client,
                            u16 reg,
                            u8 value)
{
    int ret;

    ret = i2c_smbus_write_byte_data(client,
                                    reg,
                                    value);

    if (ret < 0)
        dev_err(&client->dev,
                "write failed: 0x%x\n",
                reg);

    return ret;
}

static int imx219_read_reg(struct i2c_client *client,
                           u16 reg)
{
    int ret;

    ret = i2c_smbus_read_byte_data(client,
                                   reg);

    if (ret < 0)
        dev_err(&client->dev,
                "read failed: 0x%x\n",
                reg);

    return ret;
}

/*  Sensor Detection */

static int imx219_detect(struct i2c_client *client)
{
    int chip_id;

    chip_id = imx219_read_reg(client,
                              IMX219_CHIP_ID_REG);

    if (chip_id < 0)
        return chip_id;

    dev_info(&client->dev,
             "chip id = 0x%x\n",
             chip_id);

    if (chip_id != IMX219_CHIP_ID) {
        dev_err(&client->dev,
                "sensor not detected\n");

        return -ENODEV;
    }

    dev_info(&client->dev,
             "IMX219 detected successfully\n");

    return 0;
}

/* ------------------------------------------------ */
/* Streaming                                        */
/* ------------------------------------------------ */

static int imx219_start_stream(struct i2c_client *client)
{
    dev_info(&client->dev,
             "starting stream\n");

    return imx219_write_reg(client,
                            IMX219_MODE_SELECT,
                            IMX219_STREAM_ON);
}

static int imx219_stop_stream(struct i2c_client *client)
{
    dev_info(&client->dev,
             "stopping stream\n");

    return imx219_write_reg(client,
                            IMX219_MODE_SELECT,
                            IMX219_STREAM_OFF);
}

/* ------------------------------------------------ */
/* Probe                                            */
/* ------------------------------------------------ */

static int imx219_probe(struct i2c_client *client)
{
    struct imx219 *sensor;
    int ret;

    dev_info(&client->dev,
             "probe called\n");

    sensor = devm_kzalloc(&client->dev,
                          sizeof(*sensor),
                          GFP_KERNEL);

    if (!sensor)
        return -ENOMEM;

    sensor->client = client;

    ret = imx219_detect(client);

    if (ret)
        return ret;

    /*
     * Temporary test:
     * start streaming for 2 seconds
     */

    imx219_start_stream(client);

    msleep(2000);

    imx219_stop_stream(client);

    dev_info(&client->dev,
             "basic driver loaded\n");

    return 0;
}

/* ------------------------------------------------ */
/* Remove                                           */
/* ------------------------------------------------ */

static void imx219_remove(struct i2c_client *client)
{
    dev_info(&client->dev,
             "driver removed\n");
}


/* Device ID Table */
static const struct i2c_device_id imx219_id[] = {
    { "imx219_basic", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, imx219_id);


/* I2C Driver*/

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
MODULE_DESCRIPTION("Basic IMX219 Driver");
MODULE_AUTHOR("Your Name");
