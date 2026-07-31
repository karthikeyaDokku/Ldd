#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
 
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/media-entity.h>
 
#include "myCameraDriver.h"
 
/* ------------------------------------------------------------------ */
/* Supported format: sensor's native raw Bayer output.                 */
/* "RGB" conversion happens downstream in the ISP, not in the sensor.  */
/* ------------------------------------------------------------------ */
#define IMX219_DEFAULT_WIDTH   3280
#define IMX219_DEFAULT_HEIGHT  2464
#define IMX219_DEFAULT_MBUS_CODE MEDIA_BUS_FMT_SRGGB10_1X10
 
static const char * const imx219_supply_names[] = {
    "VANA",
    "VDIG",
    "VDDL",
};
 
struct imx219_dev {
    struct i2c_client *client;
    struct clk *xclk;
    struct gpio_desc *reset_gpio;
    struct regulator_bulk_data supplies[3];
 
    struct v4l2_subdev sd;
    struct media_pad pad;
    struct v4l2_ctrl_handler ctrl_handler;
 
    struct v4l2_ctrl *hflip;
    struct v4l2_ctrl *vflip;
    struct v4l2_ctrl *gain;
    struct v4l2_ctrl *exposure;
    struct v4l2_ctrl *test_pattern;
 
    struct mutex lock; /* serializes format/stream/control access */
    bool streaming;
};
 
static inline struct imx219_dev *to_imx219_dev(struct v4l2_subdev *sd)
{
    return container_of(sd, struct imx219_dev, sd);
}
 
/* ------------------------------------------------------------------ */
/* Low-level register access (unchanged from earlier version)          */
/* ------------------------------------------------------------------ */
 
static int imx219_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
    u8 buf[3];
 
    buf[0] = reg >> 8;
    buf[1] = reg & 0xff;
    buf[2] = val;
 
    dev_info(&client->dev, "WRITE reg=0x%04x val=0x%02x\n", reg, val);
 
    if (i2c_master_send(client, buf, 3) != 3)
        return -EIO;
 
    return 0;
}
 
static int imx219_read_reg(struct i2c_client *client, u16 reg)
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
    int high, low;
 
    high = imx219_read_reg(client, IMX219_REG_CHIP_ID_H);
    if (high < 0)
        return high;
 
    low = imx219_read_reg(client, IMX219_REG_CHIP_ID_L);
    if (low < 0)
        return low;
 
    return (high << 8) | low;
}
 
static int imx219_start_stream(struct i2c_client *client)
{
    dev_info(&client->dev, "STREAM ON\n");
    return imx219_write_reg(client, IMX219_REG_MODE_SELECT, IMX219_STREAM_ON);
}
 
static int imx219_stop_stream(struct i2c_client *client)
{
    dev_info(&client->dev, "STREAM OFF\n");
    return imx219_write_reg(client, IMX219_REG_MODE_SELECT, IMX219_STREAM_OFF);
}
 
static int imx219_set_gain(struct i2c_client *client, u8 gain)
{
    dev_info(&client->dev, "GAIN=%u\n", gain);
    return imx219_write_reg(client, IMX219_ANALOG_GAIN, gain);
}
 
static int imx219_set_exposure(struct i2c_client *client, u16 exposure)
{
    int ret;
 
    dev_info(&client->dev, "EXPOSURE=%u\n", exposure);
 
    ret = imx219_write_reg(client, IMX219_EXPOSURE_H, exposure >> 8);
    if (ret)
        return ret;
 
    return imx219_write_reg(client, IMX219_EXPOSURE_L, exposure & 0xff);
}
 
static int imx219_set_test_pattern(struct i2c_client *client, u8 pattern)
{
    dev_info(&client->dev, "TEST_PATTERN=%u\n", pattern);
    return imx219_write_reg(client, IMX219_TEST_PATTERN, pattern);
}
 
/* orientation register: bit0 = horizontal flip, bit1 = vertical flip.
* Flipping genuinely changes the Bayer pattern order the sensor
* outputs (RGGB <-> GBRG <-> BGGR <-> GRBG), so this is a real,
* accurate use of "changing format" at the sensor level.
*/
static int imx219_set_orientation(struct i2c_client *client, u8 hflip, u8 vflip)
{
    u8 val = (hflip ? 0x01 : 0x00) | (vflip ? 0x02 : 0x00);
 
    dev_info(&client->dev, "ORIENTATION hflip=%u vflip=%u\n", hflip, vflip);
    return imx219_write_reg(client, IMX219_ORIENTATION, val);
}
 
/* ------------------------------------------------------------------ */
/* Power on/off (unchanged from earlier version)                       */
/* ------------------------------------------------------------------ */
 
static int imx219_power_on(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct imx219_dev *sensor = i2c_get_clientdata(client);
    int ret;
 
    ret = clk_prepare_enable(sensor->xclk);
    if (ret) {
        dev_err(dev, "failed to enable xclk: %d\n", ret);
        return ret;
    }
 
    ret = regulator_bulk_enable(3, sensor->supplies);
    if (ret) {
        dev_err(dev, "failed to enable regulators: %d\n", ret);
        clk_disable_unprepare(sensor->xclk);
        return ret;
    }
 
    usleep_range(6200, 7200);
    gpiod_set_value_cansleep(sensor->reset_gpio, 1);
    usleep_range(100, 200);
    gpiod_set_value_cansleep(sensor->reset_gpio, 0);
    usleep_range(6000, 7000);
 
    return 0;
}
 
static void imx219_power_off(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct imx219_dev *sensor = i2c_get_clientdata(client);
 
    gpiod_set_value_cansleep(sensor->reset_gpio, 1);
    regulator_bulk_disable(3, sensor->supplies);
    clk_disable_unprepare(sensor->xclk);
}
 
static int imx219_get_resources(struct imx219_dev *sensor)
{
    struct device *dev = &sensor->client->dev;
    unsigned int i;
    int ret;
 
    //sensor->xclk = devm_clk_get(dev, NULL);
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
        dev_err(dev, "failed to set xclk rate to %u Hz\n", IMX219_XCLK_FREQ);
        return ret;
    }
 
    sensor->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(sensor->reset_gpio)) {
        dev_err(dev, "failed to get reset gpio\n");
        return PTR_ERR(sensor->reset_gpio);
    }
 
    for (i = 0; i < 3; i++)
        sensor->supplies[i].supply = imx219_supply_names[i];
 
    ret = devm_regulator_bulk_get(dev, 3, sensor->supplies);
    if (ret) {
        dev_err(dev, "failed to get regulators\n");
        return ret;
    }
 
    return 0;
}
 
/* ------------------------------------------------------------------ */
/* NEW: V4L2 subdev pad ops                                             */
/* ------------------------------------------------------------------ */
 
static int imx219_enum_mbus_code(struct v4l2_subdev *sd,
                                  struct v4l2_subdev_state *sd_state,
                                  struct v4l2_subdev_mbus_code_enum *code)
{
    if (code->index > 0)
        return -EINVAL;
 
    code->code = IMX219_DEFAULT_MBUS_CODE;
    return 0;
}
 
static int imx219_enum_frame_size(struct v4l2_subdev *sd,
                                   struct v4l2_subdev_state *sd_state,
                                   struct v4l2_subdev_frame_size_enum *fse)
{
    if (fse->index > 0 || fse->code != IMX219_DEFAULT_MBUS_CODE)
        return -EINVAL;
 
    fse->min_width = IMX219_DEFAULT_WIDTH;
    fse->max_width = IMX219_DEFAULT_WIDTH;
    fse->min_height = IMX219_DEFAULT_HEIGHT;
    fse->max_height = IMX219_DEFAULT_HEIGHT;
 
    return 0;
}
 
static void imx219_fill_default_format(struct v4l2_mbus_framefmt *fmt)
{
    fmt->width = IMX219_DEFAULT_WIDTH;
    fmt->height = IMX219_DEFAULT_HEIGHT;
    fmt->code = IMX219_DEFAULT_MBUS_CODE;
    fmt->field = V4L2_FIELD_NONE;
    fmt->colorspace = V4L2_COLORSPACE_RAW;
    fmt->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
    fmt->quantization = V4L2_QUANTIZATION_DEFAULT;
    fmt->xfer_func = V4L2_XFER_FUNC_DEFAULT;
}
 
static int imx219_get_fmt(struct v4l2_subdev *sd,
                          struct v4l2_subdev_state *sd_state,
                          struct v4l2_subdev_format *format)
{
    struct imx219_dev *sensor = to_imx219_dev(sd);
 
    mutex_lock(&sensor->lock);
 
    if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
        format->format = *v4l2_subdev_state_get_format(sd_state, 0);
    } else {
        imx219_fill_default_format(&format->format);
    }
 
    mutex_unlock(&sensor->lock);
    return 0;
}
 
static int imx219_set_fmt(struct v4l2_subdev *sd,
                          struct v4l2_subdev_state *sd_state,
                          struct v4l2_subdev_format *format)
{
    struct imx219_dev *sensor = to_imx219_dev(sd);
    struct v4l2_mbus_framefmt *fmt;
 
    mutex_lock(&sensor->lock);
 
    /* Only one mode is supported right now: clamp to it regardless of
     * what was requested. Extending this to multiple real sensor
     * modes requires adding the corresponding IMX219 mode-select
     * register tables, which this driver does not yet include.
     */
    imx219_fill_default_format(&format->format);
 
    if (format->which == V4L2_SUBDEV_FORMAT_TRY)
        fmt = v4l2_subdev_state_get_format(sd_state, 0);
    else
        fmt = &format->format;
 
    *fmt = format->format;
 
    mutex_unlock(&sensor->lock);
    return 0;
}
 
static int imx219_init_state(struct v4l2_subdev *sd,
                             struct v4l2_subdev_state *sd_state)
{
    struct v4l2_mbus_framefmt *fmt =
        v4l2_subdev_state_get_format(sd_state, 0);
 
    imx219_fill_default_format(fmt);
    return 0;
}
 
/* ------------------------------------------------------------------ */
/* NEW: V4L2 subdev video ops (streaming)                               */
/* ------------------------------------------------------------------ */
 
static int imx219_s_stream(struct v4l2_subdev *sd, int enable)
{
    struct imx219_dev *sensor = to_imx219_dev(sd);
    struct i2c_client *client = sensor->client;
    int ret;
 
    mutex_lock(&sensor->lock);
 
    if (enable == sensor->streaming) {
        mutex_unlock(&sensor->lock);
        return 0;
    }
 
    if (enable) {
        ret = imx219_start_stream(client);
        if (ret) {
            mutex_unlock(&sensor->lock);
            return ret;
        }
    } else {
        ret = imx219_stop_stream(client);
        if (ret) {
            mutex_unlock(&sensor->lock);
            return ret;
        }
    }
 
    sensor->streaming = enable;
    mutex_unlock(&sensor->lock);
 
    return 0;
}
 
static const struct v4l2_subdev_pad_ops imx219_pad_ops = {
    .enum_mbus_code = imx219_enum_mbus_code,
    .enum_frame_size = imx219_enum_frame_size,
    .get_fmt = imx219_get_fmt,
    .set_fmt = imx219_set_fmt,
};
 
static const struct v4l2_subdev_video_ops imx219_video_ops = {
    .s_stream = imx219_s_stream,
};
 
static const struct v4l2_subdev_ops imx219_subdev_ops = {
    .pad = &imx219_pad_ops,
    .video = &imx219_video_ops,
};
 
static const struct v4l2_subdev_internal_ops imx219_internal_ops = {
    .init_state = imx219_init_state,
};
 
/* ------------------------------------------------------------------ */
/* NEW: V4L2 controls - hflip/vflip actually change the Bayer pattern  */
/* the sensor outputs; gain/exposure/test-pattern map straight onto    */
/* the register writes you already had.                                */
/* ------------------------------------------------------------------ */
 
static int imx219_s_ctrl(struct v4l2_ctrl *ctrl)
{
    struct imx219_dev *sensor =
        container_of(ctrl->handler, struct imx219_dev, ctrl_handler);
    struct i2c_client *client = sensor->client;
 
    switch (ctrl->id) {
    case V4L2_CID_HFLIP:
        return imx219_set_orientation(client, ctrl->val,
                                       sensor->vflip ? sensor->vflip->val : 0);
    case V4L2_CID_VFLIP:
        return imx219_set_orientation(client,
                                       sensor->hflip ? sensor->hflip->val : 0,
                                       ctrl->val);
    case V4L2_CID_ANALOGUE_GAIN:
        return imx219_set_gain(client, (u8)ctrl->val);
    case V4L2_CID_EXPOSURE:
        return imx219_set_exposure(client, (u16)ctrl->val);
    case V4L2_CID_TEST_PATTERN:
        return imx219_set_test_pattern(client, (u8)ctrl->val);
    default:
        return -EINVAL;
    }
}
 
static const struct v4l2_ctrl_ops imx219_ctrl_ops = {
    .s_ctrl = imx219_s_ctrl,
};
 
static const char * const imx219_test_pattern_menu[] = {
    "Disabled",
    "Color Bars",
};
 
static int imx219_init_controls(struct imx219_dev *sensor)
{
    struct v4l2_ctrl_handler *hdl = &sensor->ctrl_handler;
    int ret;
 
    v4l2_ctrl_handler_init(hdl, 5);
 
    sensor->hflip = v4l2_ctrl_new_std(hdl, &imx219_ctrl_ops,
                                       V4L2_CID_HFLIP, 0, 1, 1, 0);
    sensor->vflip = v4l2_ctrl_new_std(hdl, &imx219_ctrl_ops,
                                       V4L2_CID_VFLIP, 0, 1, 1, 0);
    sensor->gain = v4l2_ctrl_new_std(hdl, &imx219_ctrl_ops,
                                      V4L2_CID_ANALOGUE_GAIN, 0, 255, 1, 64);
    sensor->exposure = v4l2_ctrl_new_std(hdl, &imx219_ctrl_ops,
                                          V4L2_CID_EXPOSURE, 0, 65535, 1, 2048);
    sensor->test_pattern = v4l2_ctrl_new_std_menu_items(
        hdl, &imx219_ctrl_ops, V4L2_CID_TEST_PATTERN,
        ARRAY_SIZE(imx219_test_pattern_menu) - 1, 0, 0,
        imx219_test_pattern_menu);
 
    if (hdl->error) {
        ret = hdl->error;
        v4l2_ctrl_handler_free(hdl);
        return ret;
    }
 
    sensor->sd.ctrl_handler = hdl;
    return 0;
}
 
/* ------------------------------------------------------------------ */
 
static int imx219_probe(struct i2c_client *client)
{
    struct imx219_dev *sensor;
    int chip_id;
    int ret;
 
    dev_info(&client->dev, "===== IMX219 PROBE =====\n");
 
    sensor = devm_kzalloc(&client->dev, sizeof(*sensor), GFP_KERNEL);
    if (!sensor)
        return -ENOMEM;
 
    sensor->client = client;
    i2c_set_clientdata(client, sensor);
    mutex_init(&sensor->lock);
 
    ret = imx219_get_resources(sensor);
    if (ret)
        return ret;
 
    ret = imx219_power_on(&client->dev);
    if (ret)
        return ret;
 
    msleep(5);
 
    chip_id = imx219_read_chipid(client);
    if (chip_id < 0) {
        dev_err(&client->dev, "chip id read failed\n");
        ret = chip_id;
        goto err_power_off;
    }
 
    dev_info(&client->dev, "chip id = 0x%04x\n", chip_id);
 
    if (chip_id != IMX219_CHIP_ID) {
        dev_err(&client->dev, "wrong sensor\n");
        ret = -ENODEV;
        goto err_power_off;
    }
 
    ret = imx219_stop_stream(client);
    if (ret)
        goto err_power_off;
 
    msleep(100);
 
    /* NEW: register as a V4L2 subdevice + media entity */
    v4l2_i2c_subdev_init(&sensor->sd, client, &imx219_subdev_ops);
    sensor->sd.internal_ops = &imx219_internal_ops;
    sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
 
    sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
    ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
    if (ret)
        goto err_power_off;
 
    ret = v4l2_subdev_init_finalize(&sensor->sd);
    if (ret) {
        dev_err(&client->dev, "failed to init subdev state: %d\n", ret);
        goto err_entity_cleanup;
    }
 
    ret = imx219_init_controls(sensor);
    if (ret)
        goto err_subdev_cleanup;
 
    ret = v4l2_async_register_subdev(&sensor->sd);
    if (ret) {
        dev_err(&client->dev, "failed to register subdev: %d\n", ret);
        goto err_ctrl_free;
    }
 
    dev_info(&client->dev, "IMX219 CUSTOM DRIVER READY (v4l2 subdev registered)\n");
 
    return 0;
 
err_ctrl_free:
    v4l2_ctrl_handler_free(&sensor->ctrl_handler);
err_subdev_cleanup:
    v4l2_subdev_cleanup(&sensor->sd);
err_entity_cleanup:
    media_entity_cleanup(&sensor->sd.entity);
err_power_off:
    imx219_power_off(&client->dev);
    return ret;
}
 
static void imx219_remove(struct i2c_client *client)
{
    struct imx219_dev *sensor = i2c_get_clientdata(client);
 
    v4l2_async_unregister_subdev(&sensor->sd);
    v4l2_ctrl_handler_free(&sensor->ctrl_handler);
    v4l2_subdev_cleanup(&sensor->sd);
    media_entity_cleanup(&sensor->sd.entity);
 
    imx219_stop_stream(client);
    imx219_power_off(&client->dev);
 
    dev_info(&client->dev, "driver removed\n");
}
 
static const struct i2c_device_id imx219_id[] = {
    { "imx219_basic", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, imx219_id);
 
static const struct of_device_id imx219_of_match[] = {
    { .compatible = "custom,imx219_basic" },
    { }
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
MODULE_AUTHOR("Ganesh");
MODULE_DESCRIPTION("Custom IMX219 driver with V4L2 subdev support");
