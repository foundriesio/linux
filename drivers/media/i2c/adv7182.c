/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 *the terms of the GNU General Public License as published by the Free Software
 *Foundation; either version 2 of the License, or (at your option) any later
 *version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 *ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 *Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/kdev_t.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-subdev.h>
#include <video/tcc/vioc_vin.h>

#define LOG_TAG			"VSRC:ADV7182"

#define loge(fmt, ...) \
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...) \
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...) \
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...) \
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define WIDTH			720
#define HEIGHT			480

#define ADV7182_REG_STATUS_1	0x10
#define ADV7182_VAL_STATUS_1	0x07

struct power_sequence {
	int			pwr_port;
	int			pwd_port;
	int			rst_port;

	enum of_gpio_flags	pwr_value;
	enum of_gpio_flags	pwd_value;
	enum of_gpio_flags	rst_value;
};

/*
 * This object contains essential v4l2 objects such as sub-device and
 * ctrl_handler
 */
struct adv7182 {
	struct v4l2_subdev		sd;
	struct v4l2_ctrl_handler	hdl;

	struct power_sequence		gpio;

	struct v4l2_mbus_framefmt fmt;

	/* Regmaps */
	struct regmap			*regmap;
};

const struct reg_sequence adv7182_reg_defaults[] = {
	{0x0f, 0x00, 0},	/* Exit Power Down Mode */
	{0x00, 0x00, 0},	/* INSEL = CVBS in on Ain 1 */
	{0x03, 0x0c, 0},	/* Enable Pixel & Sync output drivers */
	{0x04, 0x17, 0},	/* Power-up INTRQ pad & Enable SFL */
	{0x13, 0x00, 0},	/* Enable INTRQ output driver */
	{0x17, 0x41, 0},	/* select SH1 */
	{0x1d, 0x40, 0},	/* Enable LLC output driver */
	{0x52, 0xcb, 0},	/* ADI Recommended Writes */
	{0x0e, 0x80, 0},	/* ADI Recommended Writes */
	{0xd9, 0x44, 0},	/* ADI Recommended Writes */
	{0x0e, 0x00, 0},	/* ADI Recommended Writes */
	{0x0e, 0x40, 0},	/* Select User Sub Map 2 */
	{0xe0, 0x01, 0},	/* Select fast Switching Mode */
	{0x0e, 0x00, 0},	/* Select User Map */
};

static const struct regmap_config adv7182_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

struct v4l2_dv_timings adv7182_dv_timings = {
	.type	= V4L2_DV_BT_656_1120,
	.bt	= {
		.width		= WIDTH,
		.height		= HEIGHT,
		.interlaced	= V4L2_DV_INTERLACED,
		.polarities	= 0,/* V4L2_DV_VSYNC_POS_POL */
	},
};

static u32 adv7182_codes[] = {
	MEDIA_BUS_FMT_UYVY8_2X8,
};

struct v4l2_mbus_config adv7182_mbus_config = {
	.type	= V4L2_MBUS_BT656,
	.flags	=
		V4L2_MBUS_DATA_ACTIVE_LOW	|
		V4L2_MBUS_PCLK_SAMPLE_FALLING	|
		V4L2_MBUS_VSYNC_ACTIVE_LOW	|
		V4L2_MBUS_HSYNC_ACTIVE_LOW	|
		V4L2_MBUS_MASTER,
};

struct v4l2_mbus_framefmt adv7182_mbus_framefmt = {
	.width		= WIDTH,
	.height		= HEIGHT,
	.code		= MEDIA_BUS_FMT_UYVY8_2X8,
};

/*
 * gpio fuctions
 */
int adv7182_parse_device_tree(struct adv7182 *dev, struct i2c_client *client)
{
	struct device_node	*node	= client->dev.of_node;
	int			ret	= 0;

	if (node) {
		dev->gpio.pwr_port = of_get_named_gpio_flags(node,
			"pwr-gpios", 0, &dev->gpio.pwr_value);
		dev->gpio.pwd_port = of_get_named_gpio_flags(node,
			"pwd-gpios", 0, &dev->gpio.pwd_value);
		dev->gpio.rst_port = of_get_named_gpio_flags(node,
			"rst-gpios", 0, &dev->gpio.rst_value);
	} else {
		loge("could not find node!! n");
		ret = -ENODEV;
	}

	return ret;
}

void adv7182_request_gpio(struct adv7182 *dev)
{
	if (dev->gpio.pwr_port > 0) {
		/* request power */
		gpio_request(dev->gpio.pwr_port, "adv7182 power");
	}
	if (dev->gpio.pwd_port > 0) {
		/* request power-down */
		gpio_request(dev->gpio.pwd_port, "adv7182 power-down");
	}
	if (dev->gpio.rst_port > 0) {
		/* request reset */
		gpio_request(dev->gpio.rst_port, "adv7182 reset");
	}
}

void adv7182_free_gpio(struct adv7182 *dev)
{
	if (dev->gpio.pwr_port > 0)
		gpio_free(dev->gpio.pwr_port);
	if (dev->gpio.pwd_port > 0)
		gpio_free(dev->gpio.pwd_port);
	if (dev->gpio.rst_port > 0)
		gpio_free(dev->gpio.rst_port);
}

/*
 * Helper fuctions for reflection
 */
static inline struct adv7182 *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct adv7182, sd);
}

/*
 * v4l2_ctrl_ops implementations
 */
static int adv7182_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int	ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_HUE:
	case V4L2_CID_DO_WHITE_BALANCE:
	default:
		loge("V4L2_CID_BRIGHTNESS is not implemented yet.\n");
		ret = -EINVAL;
	}

	return ret;
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int adv7182_set_power(struct v4l2_subdev *sd, int on)
{
	struct adv7182		*dev	= to_state(sd);
	struct power_sequence	*gpio	= &dev->gpio;

	if (on) {
		/* port configuration */
		if (dev->gpio.pwr_port > 0) {
			gpio_direction_output(dev->gpio.pwr_port,
				dev->gpio.pwr_value);
			logd("[pwr] gpio: %3d, new val: %d, cur val: %d\n",
				dev->gpio.pwr_port, dev->gpio.pwr_value,
				gpio_get_value(dev->gpio.pwr_port));
		}
		if (dev->gpio.pwd_port > 0) {
			gpio_direction_output(dev->gpio.pwd_port,
				dev->gpio.pwd_value);
			logd("[pwd] gpio: %3d, new val: %d, cur val: %d\n",
				dev->gpio.pwd_port, dev->gpio.pwd_value,
				gpio_get_value(dev->gpio.pwd_port));
		}
		if (dev->gpio.rst_port > 0) {
			gpio_direction_output(dev->gpio.rst_port,
				dev->gpio.rst_value);
			logd("[rst] gpio: %3d, new val: %d, cur val: %d\n",
				dev->gpio.rst_port, dev->gpio.rst_value,
				gpio_get_value(dev->gpio.rst_port));
		}

		/* power-up sequence */
		if (dev->gpio.rst_port > 0) {
			gpio_set_value_cansleep(gpio->rst_port, 1);
			msleep(20);
		}
	} else {
		/* power-down sequence */
		if (dev->gpio.rst_port > 0) {
			gpio_set_value_cansleep(gpio->rst_port, 0);
			msleep(20);
		}
	}

	return 0;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int adv7182_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct adv7182		*dev	= to_state(sd);
	unsigned int		val	= 0;
	int			ret	= 0;

	/* check V4L2_IN_ST_NO_SIGNAL */
	ret = regmap_read(dev->regmap, ADV7182_REG_STATUS_1, &val);
	if (ret < 0) {
		loge("failure to check V4L2_IN_ST_NO_SIGNAL\n");
	} else {
		logd("status: 0x%08x\n", val);
		if (val & ADV7182_VAL_STATUS_1) {
			*status &= ~V4L2_IN_ST_NO_SIGNAL;
		} else {
			logw("V4L2_IN_ST_NO_SIGNAL\n");
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}
	}

	return ret;
}

static int adv7182_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct adv7182		*dev	= NULL;
	struct i2c_client	*client	= NULL;
	struct pinctrl		*pctrl	= NULL;
	int			ret	= 0;

	dev = to_state(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		ret = -EINVAL;
	} else {
		ret = regmap_multi_reg_write(dev->regmap, adv7182_reg_defaults,
			ARRAY_SIZE(adv7182_reg_defaults));
		msleep(50);
	}

	/* pinctrl */
	client = v4l2_get_subdevdata(&dev->sd);
	if (client == NULL) {
		loge("client is NULL\n");
		ret = -1;
	}

	pctrl = pinctrl_get_select_default(&client->dev);
	if (!IS_ERR(pctrl)) {
		pinctrl_put(pctrl);
		ret = -1;
	}

	return ret;
}

static int adv7182_g_dv_timings(struct v4l2_subdev *sd,
	struct v4l2_dv_timings *timings)
{
	int ret = 0;

	memcpy((void *)timings, (const void *)&adv7182_dv_timings,
		sizeof(*timings));

	return ret;
}

static int adv7182_g_mbus_config(struct v4l2_subdev *sd,
	struct v4l2_mbus_config *cfg)
{
	memcpy((void *)cfg, (const void *)&adv7182_mbus_config,
		sizeof(*cfg));

	return 0;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int adv7182_enum_mbus_code(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_mbus_code_enum *code)
{
	if ((code->pad != 0) || (ARRAY_SIZE(adv7182_codes) <= code->index))
		return -EINVAL;

	code->code = adv7182_codes[code->index];

	return 0;
}

static int adv7182_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct adv7182		*dev = to_state(sd);
	int ret	= 0;

	memcpy((void *)&format->format,
		(const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	return ret;
}

static int adv7182_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct adv7182 *dev = to_state(sd);
	int ret	= 0;

	memcpy((void *)&dev->fmt,
		(const void *)&format->format,
		sizeof(struct v4l2_mbus_framefmt));

	adv7182_dv_timings.bt.width = format->format.width;
	adv7182_dv_timings.bt.height = format->format.height;

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_ctrl_ops adv7182_ctrl_ops = {
	.s_ctrl			= adv7182_s_ctrl,
};

static const struct v4l2_subdev_core_ops adv7182_v4l2_subdev_core_ops = {
	.s_power		= adv7182_set_power,
};

static const struct v4l2_subdev_video_ops adv7182_v4l2_subdev_video_ops = {
	.g_input_status		= adv7182_g_input_status,
	.s_stream		= adv7182_s_stream,
	.g_dv_timings		= adv7182_g_dv_timings,
	.g_mbus_config		= adv7182_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops adv7182_v4l2_subdev_pad_ops = {
	.enum_mbus_code		= adv7182_enum_mbus_code,
	.get_fmt		= adv7182_get_fmt,
	.set_fmt		= adv7182_set_fmt,
};

static const struct v4l2_subdev_ops adv7182_ops = {
	.core			= &adv7182_v4l2_subdev_core_ops,
	.video			= &adv7182_v4l2_subdev_video_ops,
	.pad			= &adv7182_v4l2_subdev_pad_ops,
};

struct adv7182 adv7182_data = {
};

static const struct i2c_device_id adv7182_id[] = {
	{ "adv7182", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, adv7182_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id adv7182_of_match[] = {
	{
		.compatible	= "adi,adv7182",
		.data		= &adv7182_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, adv7182_of_match);
#endif

int adv7182_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct adv7182			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct adv7182), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(adv7182_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	/* parse the device tree */
	ret = adv7182_parse_device_tree(dev, client);
	if (ret < 0) {
		loge("cannot initialize gpio port\n");
		return ret;
	}

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &adv7182_ops);

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &adv7182_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl, &adv7182_ctrl_ops,
		V4L2_CID_DV_RX_IT_CONTENT_TYPE, V4L2_DV_IT_CONTENT_TYPE_NO_ITC,
		0, V4L2_DV_IT_CONTENT_TYPE_NO_ITC);
	dev->sd.ctrl_handler = &dev->hdl;
	if (dev->hdl.error) {
		loge("v4l2_ctrl_handler_init is wrong\n");
		ret = dev->hdl.error;
		goto goto_free_device_data;
	}

	/* register a v4l2 sub device */
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	/* init mbus format */
	memcpy((void *)&dev->fmt, (const void *)&adv7182_mbus_framefmt,
		sizeof(dev->fmt));

	/* request gpio */
	adv7182_request_gpio(dev);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &adv7182_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int adv7182_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct adv7182		*dev	= to_state(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	/* gree gpio */
	adv7182_free_gpio(dev);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver adv7182_driver = {
	.probe		= adv7182_probe,
	.remove		= adv7182_remove,
	.driver		= {
		.name		= "adv7182",
		.of_match_table	= of_match_ptr(adv7182_of_match),
	},
	.id_table	= adv7182_id,
};

module_i2c_driver(adv7182_driver);
