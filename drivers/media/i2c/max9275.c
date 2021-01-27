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

#define LOG_TAG			"VSRC:MAX9275"

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

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct max9275 {
	struct v4l2_subdev		sd;
	struct v4l2_mbus_framefmt	fmt;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

const struct reg_sequence max9275_reg_defaults[] = {
	{0x04, 0x43, 50*1000},

};

static const struct regmap_config max9275_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

/*
 * Helper fuctions for reflection
 */
static inline struct max9275 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max9275, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max9275_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		ret = regmap_multi_reg_write(dev->regmap,
				max9275_reg_defaults,
				ARRAY_SIZE(max9275_reg_defaults));
		if (ret < 0)
			loge("Fail initializing max9275 device\n");
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
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
static int max9275_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->s_cnt == 0) && (enable == 1)) {
		ret = regmap_write(dev->regmap, 0x04, 0x83);
		if (ret < 0)
			loge("Fail Serialization max9275 device\n");
	} else if ((dev->s_cnt == 1) && (enable == 0)) {
		ret = regmap_write(dev->regmap, 0x04, 0x43);
	}

	if (enable)
		dev->s_cnt++;
	else
		dev->s_cnt--;

	mutex_unlock(&dev->lock);
	return ret;
}

static int max9275_get_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *format)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int max9275_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *format)
{
	struct max9275		*dev	= to_dev(sd);
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
static const struct v4l2_subdev_core_ops max9275_core_ops = {
	.init			= max9275_init,
};

static const struct v4l2_subdev_video_ops max9275_video_ops = {
	.s_stream		= max9275_s_stream,
};

static const struct v4l2_subdev_pad_ops max9275_pad_ops = {
	.get_fmt		= max9275_get_fmt,
	.set_fmt		= max9275_set_fmt,
};

static const struct v4l2_subdev_ops max9275_ops = {
	.core			= &max9275_core_ops,
	.video			= &max9275_video_ops,
	.pad			= &max9275_pad_ops,
};

struct max9275 max9275_data = {
};

static const struct i2c_device_id max9275_id[] = {
	{ "max9275", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max9275_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id max9275_of_match[] = {
	{
		.compatible	= "maxim,max9275",
		.data		= &max9275_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max9275_of_match);
#endif

int max9275_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max9275 *dev = NULL;
	const struct of_device_id *dev_id = NULL;
	int ret = 0;

	// allocate and clear memory for a device
	dev = devm_kzalloc(&client->dev, sizeof(struct max9275), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	mutex_init(&dev->lock);

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max9275_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	// Register with V4L2 layer as a slave device
	v4l2_i2c_subdev_init(&dev->sd, client, &max9275_ops);

	// register a v4l2 sub device
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	// init regmap
	dev->regmap = devm_regmap_init_i2c(client, &max9275_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	goto goto_end;

goto_free_device_data:
	// free the videosource data
	kfree(dev);

goto_end:
	return ret;
}

int max9275_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max9275		*dev	= to_dev(sd);

	// release regmap
	regmap_exit(dev->regmap);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max9275_driver = {
	.probe		= max9275_probe,
	.remove		= max9275_remove,
	.driver		= {
		.name		= "max9275",
		.of_match_table	= of_match_ptr(max9275_of_match),
	},
	.id_table	= max9275_id,
};

module_i2c_driver(max9275_driver);

