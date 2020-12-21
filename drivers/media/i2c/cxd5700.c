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

#define LOG_TAG			"VSRC:cxd5700"

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
struct cxd5700 {
	struct v4l2_subdev		sd;
	struct v4l2_mbus_framefmt	fmt;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

const char cxd5700_reg_defaults[] = {
	/*{0x0A},*/ {0x01}, {0x07}, {0x02}, {0x01},
	{0x00}, {0x00}, {0x30}, {0x80}, {0xC5}
};

static const struct regmap_config cxd5700_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static void cxd5700_init_format(struct cxd5700 *dev)
{
	dev->fmt.width = 1920;
	dev->fmt.height	= 1080;
	dev->fmt.code = MEDIA_BUS_FMT_UYVY8_1X16;//MEDIA_BUS_FMT_UYVY8_2X8;
	dev->fmt.field = V4L2_FIELD_NONE;
	dev->fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
}
/*
 * Helper fuctions for reflection
 */
static inline struct cxd5700 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct cxd5700, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int cxd5700_init(struct v4l2_subdev *sd, u32 enable)
{
	struct cxd5700		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		ret = regmap_bulk_write(dev->regmap,
				0x0a,
				cxd5700_reg_defaults, ARRAY_SIZE(cxd5700_reg_defaults));
		if (ret < 0)
			loge("regmap_multi_reg_write returned %d\n", ret);
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
static int cxd5700_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct cxd5700		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);


	mutex_unlock(&dev->lock);
	return ret;
}

static int cxd5700_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	struct cxd5700		*dev	= to_dev(sd);
	int			ret	= 0;

	logi("%s call\n", __func__);

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int cxd5700_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *format)
{
	struct cxd5700		*dev	= to_dev(sd);
	int			ret	= 0;

	logi("%s call\n", __func__);

	mutex_lock(&dev->lock);

	memcpy((void *)&dev->fmt, (const void *)&format->format,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_subdev_core_ops cxd5700_core_ops = {
	.init			= cxd5700_init,
};

static const struct v4l2_subdev_video_ops cxd5700_video_ops = {
	.s_stream		= cxd5700_s_stream,
};

static const struct v4l2_subdev_pad_ops cxd5700_pad_ops = {
	.get_fmt		= cxd5700_get_fmt,
	.set_fmt		= cxd5700_set_fmt,
};

static const struct v4l2_subdev_ops cxd5700_ops = {
	.core			= &cxd5700_core_ops,
	.video			= &cxd5700_video_ops,
	.pad			= &cxd5700_pad_ops,
};

struct cxd5700 cxd5700_data = {
};

static const struct i2c_device_id cxd5700_id[] = {
	{ "cxd5700", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cxd5700_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id cxd5700_of_match[] = {
	{
		.compatible	= "sony,cxd5700",
		.data		= &cxd5700_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cxd5700_of_match);
#endif

int cxd5700_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cxd5700 *dev = NULL;
	const struct of_device_id *dev_id = NULL;
	int ret = 0;

	// allocate and clear memory for a device
	dev = devm_kzalloc(&client->dev, sizeof(struct cxd5700), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	mutex_init(&dev->lock);

	// set the specific information
	if (client->dev.of_node) {
		dev_id = of_match_node(cxd5700_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	// Register with V4L2 layer as a slave device
	v4l2_i2c_subdev_init(&dev->sd, client, &cxd5700_ops);

	// register a v4l2 sub device
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	// init regmap
	dev->regmap = devm_regmap_init_i2c(client, &cxd5700_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	/* init format info */
	cxd5700_init_format(dev);

	goto goto_end;

goto_free_device_data:
	// free the videosource data
	kfree(dev);

goto_end:
	return ret;
}

int cxd5700_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct cxd5700		*dev	= to_dev(sd);

	// release regmap
	regmap_exit(dev->regmap);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver cxd5700_driver = {
	.probe		= cxd5700_probe,
	.remove		= cxd5700_remove,
	.driver		= {
		.name		= "cxd5700",
		.of_match_table	= of_match_ptr(cxd5700_of_match),
	},
	.id_table	= cxd5700_id,
};

module_i2c_driver(cxd5700_driver);
