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

#define LOG_TAG				"VSRC:MAX9295"

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
struct max9295 {
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
const struct reg_sequence max9295_reg_defaults[] = {
};

/* for raw12 ispless camera module */
const struct reg_sequence max9295_reg_defaults_raw12[] = {
	/************* Serializer MAX9295 ********************/
	{0x02CA, 0x80, 0},		/* GPIO4 (S_OSC_OE) low */
	{0x02BE, 0x80, 0},		/* GPIO0 (RESET) low */
	{0x02D6, 0x80, 10 * 1000},	/* GPIO8 (TRIGGER) low */

	{0x02CA, 0x90, 10 * 1000},	/* GPIO4 (S_OSC_OE) high */
	{0x02BE, 0x90, 10 * 1000},	/* GPIO0 (RESET) high */
	{0x02D6, 0x90, 10 * 1000},	/* GPIO8 (TRIGGER) High */

	/* Make sure pipelines start transmission */
	{0x0002, 0x33, 0},
	/* Make sure that the SER is in 1x4 mode (phy_config = 0) */
	{0x0330, 0x00, 0},
	/* Set 4 lanes for serializer */
	{0x0331, 0x33, 0},
	/* Lane swapping */
	{0x0332, 0xB0, 0},
	{0x0333, 0x01, 0},
	/* Enable pipe */
	{0x0308, 0x71, 0},
	{0x0311, 0x10, 0},
	/* Route RAW12 to pipe X (MSB is enable, [5:0] = 0x2C for RAW12) */
	{0x0314, 0x6C, 100*1000},
};

const struct reg_sequence max9295_reg_defaults_raw8[] = {
};

static const struct regmap_config max9295_regmap = {
	.reg_bits		= 16,
	.val_bits		= 8,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

static void max9295_init_format(struct max9295 *dev)
{
	dev->fmt.width = 2560;
	dev->fmt.height	= 1440,
	dev->fmt.code = MEDIA_BUS_FMT_UYVY8_1X16;
	dev->fmt.field = V4L2_FIELD_NONE;
	dev->fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
}
/*
 * Helper functions for reflection
 */
static inline struct max9295 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max9295, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max9295_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max9295		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		if (dev->fmt.code == MEDIA_BUS_FMT_SGRBG12_1X12) {
			logi("input format is bayer raw\n");
			ret = regmap_multi_reg_write(dev->regmap,
				max9295_reg_defaults_raw12,
				ARRAY_SIZE(max9295_reg_defaults_raw12));
		} else if (dev->fmt.code == MEDIA_BUS_FMT_Y8_1X8) {
			/* format is MEDIA_BUS_FMT_Y8_1X8 */
			logi("input format is RAW8\n");
			ret = regmap_multi_reg_write(dev->regmap,
				max9295_reg_defaults_raw8,
				ARRAY_SIZE(max9295_reg_defaults_raw8));
		} else {
			logi("input format is yuv422\n");
			ret = regmap_multi_reg_write(dev->regmap,
				max9295_reg_defaults,
				ARRAY_SIZE(max9295_reg_defaults));
		}
		if (ret < 0)
			loge("Fail initializing max9295 device\n");
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* ret = regmap_write(dev->regmap, 0x15, 0x93); */
	}

	if (enable)
		dev->i_cnt++;
	else
		dev->i_cnt--;

	mutex_unlock(&dev->lock);

	msleep(100);

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int max9295_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max9295		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->s_cnt == 0) && (enable == 1)) {
		/* TODO
		 * enable output
		 */
	} else if ((dev->s_cnt == 1) && (enable == 0)) {
		/* TODO
		 * disable output
		 */
	}

	if (enable)
		dev->s_cnt++;
	else
		dev->s_cnt--;

	msleep(30);

	mutex_unlock(&dev->lock);
	return ret;
}

static int max9295_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct max9295		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int max9295_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct max9295		*dev	= to_dev(sd);
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
static const struct v4l2_subdev_core_ops max9295_core_ops = {
	.init			= max9295_init,
};

static const struct v4l2_subdev_video_ops max9295_video_ops = {
	.s_stream		= max9295_s_stream,
};

static const struct v4l2_subdev_pad_ops max9295_pad_ops = {
	.get_fmt		= max9295_get_fmt,
	.set_fmt		= max9295_set_fmt,
};

static const struct v4l2_subdev_ops max9295_ops = {
	.core			= &max9295_core_ops,
	.video			= &max9295_video_ops,
	.pad			= &max9295_pad_ops,
};

struct max9295 max9295_data = {
};

static const struct i2c_device_id max9295_id[] = {
	{ "max9295", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max9295_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id max9295_of_match[] = {
	{
		.compatible	= "maxim,max9295",
		.data		= &max9295_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max9295_of_match);
#endif

int max9295_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max9295		*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct max9295), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max9295_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &max9295_ops);

	/* register a v4l2 sub device */
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &max9295_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	/* init format info */
	max9295_init_format(dev);

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int max9295_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max9295		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max9295_driver = {
	.probe		= max9295_probe,
	.remove		= max9295_remove,
	.driver		= {
		.name		= "max9295",
		.of_match_table	= of_match_ptr(max9295_of_match),
	},
	.id_table	= max9295_id,
};

module_i2c_driver(max9295_driver);
