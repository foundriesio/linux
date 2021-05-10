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

#define LOG_TAG				"VSRC:MAX96705"

#define loge(fmt, ...) \
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...) \
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...) \
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...) \
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)



/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct max96705 {
	struct v4l2_subdev		sd;
	struct v4l2_mbus_framefmt	fmt;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

/* for yuv422 HD camera module*/
const struct reg_sequence max96705_reg_defaults[] = {
};

/* for raw12 ispless camera module */
const struct reg_sequence max96705_reg_defaults_raw12[] = {
};

const struct reg_sequence max96705_reg_defaults_raw8[] = {
	//MAX96705 SER
	{0x04, 0x47, 10*1000},	/* CLINK EN = 1, Delay 10ms */
	{0x07, 0xA4, 0},	/* SER BWS=1, HVEN=1 */
	{0x20, 0x04, 0},	/* Bit Shift */
	{0x21, 0x05, 0},	/* Bit Shift */
	{0x22, 0x06, 0},	/* Bit Shift */
	{0x23, 0x07, 0},	/* Bit Shift */
	{0x24, 0x08, 0},	/* Bit Shift */
	{0x25, 0x09, 0},	/* Bit Shift */
	{0x26, 0x0A, 0},	/* Bit Shift */
	{0x27, 0x0B, 0},	/* Bit Shift */
	{0x30, 0x04, 0},	/* Bit Shift */
	{0x31, 0x05, 0},	/* Bit Shift */
	{0x32, 0x06, 0},	/* Bit Shift */
	{0x33, 0x07, 0},	/* Bit Shift */
	{0x34, 0x08, 0},	/* Bit Shift */
	{0x35, 0x09, 0},	/* Bit Shift */
	{0x36, 0x0A, 0},	/* Bit Shift */
	{0x37, 0x0B, 0},	/* Bit Shift */
	{0x67, 0xc4, 0},	/* DBL Align */
	{0x0f, 0x3c, 10*1000},	/* RESET "L", Delay 10ms */
	{0x0f, 0x3e, 0},	/* RESET "H" */
};

static const struct regmap_config max96705_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static void max96705_init_format(struct max96705 *dev)
{
	dev->fmt.width		= 1280;
	dev->fmt.height		= 800,
	dev->fmt.code		= MEDIA_BUS_FMT_Y8_1X8;
	dev->fmt.field		= V4L2_FIELD_NONE;
	dev->fmt.colorspace	= V4L2_COLORSPACE_SMPTE170M;
}

/*
 * Helper functions for reflection
 */
static inline struct max96705 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max96705, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max96705_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max96705		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		if (dev->fmt.code == MEDIA_BUS_FMT_SGRBG12_1X12) {
			logi("input format is bayer raw\n");
			ret = regmap_multi_reg_write(dev->regmap,
				max96705_reg_defaults_raw12,
				ARRAY_SIZE(max96705_reg_defaults_raw12));
		} else if (dev->fmt.code == MEDIA_BUS_FMT_Y8_1X8) {
			/* format is MEDIA_BUS_FMT_Y8_1X8 */
			logi("input format is RAW8\n");
			ret = regmap_multi_reg_write(dev->regmap,
				max96705_reg_defaults_raw8,
				ARRAY_SIZE(max96705_reg_defaults_raw8));
		} else {
			logi("input format is yuv422\n");
			ret = regmap_multi_reg_write(dev->regmap,
				max96705_reg_defaults,
				ARRAY_SIZE(max96705_reg_defaults));
		}
		if (ret < 0)
			loge("Fail initializing max96705 device\n");
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

/*
 * v4l2_subdev_video_ops implementations
 */
static int max96705_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max96705		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->s_cnt == 0) && (enable == 1)) {
		ret = regmap_write(dev->regmap, 0x04, 0x87);
		if (ret < 0)
			loge("Fail Serialization max96705 device\n");
	} else if ((dev->s_cnt == 1) && (enable == 0)) {
		ret = regmap_write(dev->regmap, 0x04, 0x47);
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

static int max96705_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct max96705		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int max96705_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct max96705		*dev	= to_dev(sd);
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
static const struct v4l2_subdev_core_ops max96705_core_ops = {
	.init			= max96705_init,
};

static const struct v4l2_subdev_video_ops max96705_video_ops = {
	.s_stream		= max96705_s_stream,
};

static const struct v4l2_subdev_pad_ops max96705_pad_ops = {
	.get_fmt		= max96705_get_fmt,
	.set_fmt		= max96705_set_fmt,
};

static const struct v4l2_subdev_ops max96705_ops = {
	.core			= &max96705_core_ops,
	.video			= &max96705_video_ops,
	.pad			= &max96705_pad_ops,
};

struct max96705 max96705_data = {
};

static const struct i2c_device_id max96705_id[] = {
	{ "max96705", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max96705_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id max96705_of_match[] = {
	{
		.compatible	= "maxim,max96705",
		.data		= &max96705_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max96705_of_match);
#endif

int max96705_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max96705			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct max96705), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max96705_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%px\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &max96705_ops);

	/* register a v4l2 sub device */
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &max96705_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	/* init format info */
	max96705_init_format(dev);

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int max96705_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max96705		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max96705_driver = {
	.probe		= max96705_probe,
	.remove		= max96705_remove,
	.driver		= {
		.name		= "max96705",
		.of_match_table	= of_match_ptr(max96705_of_match),
	},
	.id_table	= max96705_id,
};

module_i2c_driver(max96705_driver);
