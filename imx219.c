#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include "imx219.h"

/* Names must match the regulator-name properties in the sensor's DT node
 * (same three rails the mainline imx219 driver uses).
 */
static const char * const imx219_supply_names[] = {
    "VANA",  /* 2.8V analog */
    "VDIG",  /* 1.8V digital core */
    "VDDL",  /* 1.2V digital IO */
};

struct imx219_dev {
    struct i2c_client *client;
    struct clk *xclk;
    struct gpio_desc *reset_gpio;
    struct regulator_bulk_data supplies[IMX219_NUM_SUPPLIES];
};

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

/* ------------------------------------------------------------------ */
/* NEW: power-on / power-off sequence, ported from the mainline driver */
/* ------------------------------------------------------------------ */

static int imx219_power_on(struct device *dev)
{
    struct i2c_client *client = to_i1c_client(dev);
    struct imx219_dev *sensor = i2c_get_clientdata(client);
    int ret;

    ret = clk_prepare_enable(sensor->xclk);
    if (ret) {
        dev_err(dev, "failed to enable xclk: %d\n", ret);
        return ret;
    }

    ret = regulator_bulk_enable(IMX219_NUM_SUPPLIES, sensor->supplies);
    if (ret) {
        dev_err(dev, "failed to enable regulators: %d\n", ret);
        clk_disable_unprepare(sensor->xclk);
        return ret;
    }

    /* Sensor needs the clock stable for a bit before reset is released */
    usleep_range(IMX219_XCLK_MIN_DELAY_US,
                 IMX219_XCLK_MIN_DELAY_US + IMX219_XCLK_DELAY_RANGE_US);

    /* Reset is active-low: hold it, then release */
    gpiod_set_value_cansleep(sensor->reset_gpio, 1);
    usleep_range(IMX219_RESET_MIN_DELAY_US,
                 IMX219_RESET_MIN_DELAY_US + IMX219_RESET_DELAY_RANGE_US);
    gpiod_set_value_cansleep(sensor->reset_gpio, 0);

    /* Datasheet t3: time from reset release to first I2C access */
    usleep_range(6000, 7000);

    return 0;
}

static void imx219_power_off(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct imx219_dev *sensor = i2c_get_clientdata(client);

    gpiod_set_value_cansleep(sensor->reset_gpio, 1);
    regulator_bulk_disable(IMX219_NUM_SUPPLIES, sensor->supplies);
    clk_disable_unprepare(sensor->xclk);
}

static int imx219_get_resources(struct imx219_dev *sensor)
{
    struct device *dev = &sensor->client->dev;
    unsigned int i;
    int ret;

    sensor->xclk = devm_clk_get(dev, "xclk");
    if (IS_ERR(sensor->xclk)) {
        //dev_err(dev, "failed to get xclk\n");
        dev_err(dev,
        "failed to get xclk (%ld)\n",
        PTR_ERR(sensor->xclk));
        return PTR_ERR(sensor->xclk);

    }

    ret = clk_set_rate(sensor->xclk, IMX219_XCLK_FREQ);
    if (ret) {
        dev_err(dev, "failed to set xclk rate to %u Hz\n",
                IMX219_XCLK_FREQ);
        return ret;
    }

    /* Reset line: request de-asserted (low) initially */
    sensor->reset_gpio = devm_gpiod_get_optional(dev, "reset",
                                                  GPIOD_OUT_HIGH);
    if (IS_ERR(sensor->reset_gpio)) {
        dev_err(dev, "failed to get reset gpio\n");
        return PTR_ERR(sensor->reset_gpio);
    }

    for (i = 0; i < IMX219_NUM_SUPPLIES; i++)
        sensor->supplies[i].supply = imx219_supply_names[i];

    ret = devm_regulator_bulk_get(dev, IMX219_NUM_SUPPLIES,
                                  sensor->supplies);
    if (ret) {
        dev_err(dev, "failed to get regulators\n");
        return ret;
    }

    return 0;
}

/* ------------------------------------------------------------------ */

static int imx219_probe(struct i2c_client *client)
{
    struct imx219_dev *sensor;
    int chip_id;
    int ret;

    dev_info(&client->dev,
             "===== IMX219 PROBE =====\n");

    sensor = devm_kzalloc(&client->dev, sizeof(*sensor), GFP_KERNEL);
    if (!sensor)
        return -ENOMEM;

    sensor->client = client;
    i2c_set_clientdata(client, sensor);

    ret = imx219_get_resources(sensor);
    if (ret)
        return ret;

    ret = imx219_power_on(&client->dev);
    if (ret)
        return ret;

    /* Give the sensor its full boot time before the first register read.
     * (Mainline driver waits ~5ms minimum here.)
     */
    msleep(5);

    chip_id = imx219_read_chipid(client);

    if (chip_id < 0) {
        dev_err(&client->dev,
                "chip id read failed\n");
        ret = chip_id;
        goto err_power_off;
    }

    dev_info(&client->dev,
             "chip id = 0x%04x\n",
             chip_id);

    if (chip_id != IMX219_CHIP_ID) {
        dev_err(&client->dev,
                "wrong sensor\n");
        ret = -ENODEV;
        goto err_power_off;
    }

    /* stop sensor */
    ret = imx219_stop_stream(client);
    if (ret)
        goto err_power_off;

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
        goto err_power_off;

    dev_info(&client->dev,
             "IMX219 CUSTOM DRIVER READY\n");

    return 0;

err_power_off:
    imx219_power_off(&client->dev);
    return ret;
}

static void imx219_remove(struct i2c_client *client)
{
    imx219_stop_stream(client);
    imx219_power_off(&client->dev);

    dev_info(&client->dev,
             "driver removed\n");
}

static const struct i2c_device_id imx219_id[] = {
    { "imx219_basic", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, imx219_id);

static const struct of_device_id imx219_of_match[] = {
    { .compatible = "custom,imx219_basic" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx219_of_match);

static struct i2c_driver imx219_driver = {
    .driver = {
        .name = IMX219_NAME,
        .of_match_table = imx219_of_match,
    },
    .probe = imx219_probe,
    .remove = imx219_remove,
    .id_table = imx219_id,
};

module_i2c_driver(imx219_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Karthikeya");
MODULE_DESCRIPTION("Custom IMX219 experimentation driver");
