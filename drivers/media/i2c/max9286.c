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

#define LOG_TAG			"VSRC:MAX9286"

#define loge(fmt, ...)		\
		pr_err("[ERROR][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)		\
		pr_warn("[WARN][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)		\
		pr_debug("[DEBUG][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)		\
		pr_info("[INFO][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define DEFAULT_WIDTH			1280
#define DEFAULT_HEIGHT			720

#define MAX9286_REG_STATUS_1	0x1E
#define MAX9286_VAL_STATUS_1	0x40

struct power_sequence {
	int			pwr_port;
	int			pwd_port;
	int			rst_port;
	int			intb_port;

	enum of_gpio_flags	pwr_value;
	enum of_gpio_flags	pwd_value;
	enum of_gpio_flags	rst_value;
	enum of_gpio_flags	intb_value;
};

/*
 * This object contains essential v4l2 objects such as sub-device and
 * ctrl_handler
 */
struct max9286 {
	struct v4l2_subdev		sd;
	struct v4l2_mbus_framefmt	fmt;

	struct power_sequence		gpio;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

const struct reg_sequence max9286_reg_defaults[] = {
	// init
	// enable 4ch
	{0X0A, 0X0F, 0},	/* Disable all Forward control channel */
#ifdef CONFIG_VIDEO_AR0147
	{0X34, 0Xb5, 0},	 //    Write   Enable auto acknowledge
#else
	{0X34, 0X35, 0},	/* Disable auto acknowledge */
#endif
	{0X15, 0X83, 0},	/*
				 * Select the combined camera line format
				 * for CSI2 output
				 */
#ifdef CONFIG_VIDEO_AR0147
	{0X12, 0Xc7, 0},	// Write DBL OFF, MIPI Output setting(RAW12)
	{0X1C, 0xf6, 5*1000},	 //    BWS: 27bit
#else
	{0X12, 0XF3, 5*1000},	/* MIPI Output setting(DBL ON, YUV422) */
#endif
	{0X63, 0X00, 0},	/* Widows off */
	{0X64, 0X00, 0},	/* Widows off */
	{0X62, 0X1F, 0},	/* FRSYNC Diff off */

#ifdef CONFIG_VIDEO_AR0147
	{0x01, 0xe0, 0}, // manual mode, enable gpi(des) - gpo(ser) transmission
	//{0x01, 0xc0}, // manual mode, enable gpi(des) - gpo(ser) transmission
	{0X00, 0XE1, 0}, // Port 0 used
	{0x0c, 0x11, 5*1000}, // disable HS/VS encoding
#else
	{0x01, 0xc0, 0},	/* manual mode */
	{0X08, 0X25, 0},	/* FSYNC-period-High */
	{0X07, 0XC3, 0},	/* FSYNC-period-Mid */
	{0X06, 0XF8, 5*1000},	/* FSYNC-period-Low */
	{0X00, 0XEF, 5*1000},	/* Port 0~3 used */
#endif
#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
	{0X15, 0X03, 0},	/* (line concatenation) */
#else
	{0X15, 0X93, 0},	/* (line interleave) */
#endif
	{0X69, 0XF0, 0},	/* Auto mask & comabck enable */
#ifdef CONFIG_VIDEO_AR0147
	{0x01, 0xe0, 0},	// enable gpi(des) - gpo(ser) transmission
#else
	{0x01, 0x00, 0},
#endif
	{0X0A, 0XFF, 0},	/* All forward channel enable */
};

static const struct regmap_config max9286_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

/*
 * gpio fuctions
 */
int max9286_parse_device_tree(struct max9286 *dev, struct i2c_client *client)
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
		dev->gpio.intb_port = of_get_named_gpio_flags(node,
			"intb-gpios", 0, &dev->gpio.intb_value);
	} else {
		loge("could not find node!! n");
		ret = -ENODEV;
	}

	return ret;
}

void max9286_request_gpio(struct max9286 *dev)
{
	if (dev->gpio.pwr_port > 0) {
		/* request power */
		gpio_request(dev->gpio.pwr_port, "max9286 power");
	}
	if (dev->gpio.pwd_port > 0) {
		/* request power-down */
		gpio_request(dev->gpio.pwd_port, "max9286 power-down");
	}
	if (dev->gpio.rst_port > 0) {
		/* request reset */
		gpio_request(dev->gpio.rst_port, "max9286 reset");
	}
	if (dev->gpio.intb_port > 0) {
		/* request intb */
		gpio_request(dev->gpio.intb_port, "max9286 interrupt");
	}
}

void max9286_free_gpio(struct max9286 *dev)
{
	if (dev->gpio.pwr_port > 0)
		gpio_free(dev->gpio.pwr_port);
	if (dev->gpio.pwd_port > 0)
		gpio_free(dev->gpio.pwd_port);
	if (dev->gpio.rst_port > 0)
		gpio_free(dev->gpio.rst_port);
	if (dev->gpio.intb_port > 0)
		gpio_free(dev->gpio.intb_port);
}

/*
 * Helper fuctions for reflection
 */
static inline struct max9286 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max9286, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max9286_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		ret = regmap_multi_reg_write(dev->regmap,
				max9286_reg_defaults,
				ARRAY_SIZE(max9286_reg_defaults));
		if (ret < 0) {
			/* failed to write i2c */
			loge("Fail initializing max9286 device\n");
		}
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* ret = regmap_write(dev->regmap, 0x15, 0x93); */
	}

	if (enable)
		dev->i_cnt++;
	else
		dev->i_cnt--;

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9286_set_power(struct v4l2_subdev *sd, int on)
{
	struct max9286		*dev	= to_dev(sd);
	struct power_sequence	*gpio	= &dev->gpio;

	mutex_lock(&dev->lock);

	if (on) {
		if (dev->p_cnt == 0) {
			/* port configuration */
			if (dev->gpio.pwr_port > 0) {
				gpio_direction_output(dev->gpio.pwr_port,
					dev->gpio.pwr_value);
				logd("[pwr] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.pwr_port,
					dev->gpio.pwr_value,
					gpio_get_value(dev->gpio.pwr_port));
			}
			if (dev->gpio.pwd_port > 0) {
				gpio_direction_output(dev->gpio.pwd_port,
					dev->gpio.pwd_value);
				logd("[pwd] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.pwd_port,
					dev->gpio.pwd_value,
					gpio_get_value(dev->gpio.pwd_port));
			}
			if (dev->gpio.rst_port > 0) {
				gpio_direction_output(dev->gpio.rst_port,
					dev->gpio.rst_value);
				logd("[rst] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.rst_port,
					dev->gpio.rst_value,
					gpio_get_value(dev->gpio.rst_port));
			}
			if (dev->gpio.intb_port > 0) {
				gpio_direction_input(dev->gpio.intb_port);
				logd("[int] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.intb_port,
					dev->gpio.intb_value,
					gpio_get_value(dev->gpio.intb_port));
			}

			/* power-up sequence */
			if (dev->gpio.pwd_port > 0) {
				gpio_set_value_cansleep(gpio->pwd_port, 1);
				msleep(20);
			}
			if (dev->gpio.rst_port > 0) {
				gpio_set_value_cansleep(gpio->rst_port, 1);
				msleep(20);
			}
		}
		dev->p_cnt++;
	} else {
		if (dev->p_cnt == 1) {
			/* power-down sequence */
			if (dev->gpio.rst_port > 0) {
				gpio_set_value_cansleep(gpio->rst_port, 0);
				msleep(20);
			}
			if (dev->gpio.pwd_port > 0) {
				gpio_set_value_cansleep(gpio->pwd_port, 0);
				msleep(20);
			}
		}
		dev->p_cnt--;
	}

	mutex_unlock(&dev->lock);

	return 0;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int max9286_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct max9286		*dev	= to_dev(sd);
	unsigned int		val	= 0;
	int			ret	= 0;

	mutex_lock(&dev->lock);

	/* check V4L2_IN_ST_NO_SIGNAL */
	ret = regmap_read(dev->regmap, MAX9286_REG_STATUS_1, &val);
	if (ret < 0) {
		loge("failure to check V4L2_IN_ST_NO_SIGNAL\n");
	} else {
		logd("status: 0x%08x\n", val);
		if (val == MAX9286_VAL_STATUS_1) {
			*status &= ~V4L2_IN_ST_NO_SIGNAL;
		} else {
			logw("V4L2_IN_ST_NO_SIGNAL\n");
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9286_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->s_cnt == 0) && (enable == 1)) {
#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
		ret = regmap_write(dev->regmap, 0x15, 0x0B);
#else
		ret = regmap_write(dev->regmap, 0x15, 0x9B);
#endif
		if (ret < 0) {
			/* failure of enabling output  */
			loge("Fail enable output of max9286 device\n");
		}
	} else if ((dev->s_cnt == 1) && (enable == 0)) {
#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
		ret = regmap_write(dev->regmap, 0x15, 0x03);
#else
		ret = regmap_write(dev->regmap, 0x15, 0x93);
#endif
		if (ret < 0) {
			/* failure of disabling output  */
			loge("Fail disable output of max9286 device\n");
		}
	}

	if (enable) {
		/* count up */
		dev->s_cnt++;
	} else {
		/* count down */
		dev->s_cnt--;
	}

	msleep(30);

	mutex_unlock(&dev->lock);
	return ret;
}

static int max9286_get_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *format)
{
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int max9286_set_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *format)
{
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&dev->fmt, (const void *)&format->format,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_subdev_core_ops max9286_core_ops = {
	.init			= max9286_init,
	.s_power		= max9286_set_power,
};

static const struct v4l2_subdev_video_ops max9286_video_ops = {
	.g_input_status		= max9286_g_input_status,
	.s_stream		= max9286_s_stream,
};

static const struct v4l2_subdev_pad_ops max9286_pad_ops = {
	.get_fmt		= max9286_get_fmt,
	.set_fmt		= max9286_set_fmt,
};

static const struct v4l2_subdev_ops max9286_ops = {
	.core			= &max9286_core_ops,
	.video			= &max9286_video_ops,
	.pad			= &max9286_pad_ops,
};

struct max9286 max9286_data = {
};

static const struct i2c_device_id max9286_id[] = {
	{ "max9286", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max9286_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id max9286_of_match[] = {
	{
		.compatible	= "maxim,max9286",
		.data		= &max9286_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max9286_of_match);
#endif

int max9286_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max9286 *dev = NULL;
	const struct of_device_id *dev_id = NULL;
	int ret = 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct max9286), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	mutex_init(&dev->lock);

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max9286_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	/* parse the device tree */
	ret = max9286_parse_device_tree(dev, client);
	if (ret < 0) {
		loge("cannot initialize gpio port\n");
		return ret;
	}

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &max9286_ops);

	/* register a v4l2 sub device */
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	/* request gpio */
	max9286_request_gpio(dev);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &max9286_regmap);
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

int max9286_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max9286		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	/* gree gpio */
	max9286_free_gpio(dev);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max9286_driver = {
	.probe		= max9286_probe,
	.remove		= max9286_remove,
	.driver		= {
		.name		= "max9286",
		.of_match_table	= of_match_ptr(max9286_of_match),
	},
	.id_table	= max9286_id,
};

module_i2c_driver(max9286_driver);
