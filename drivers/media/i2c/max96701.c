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

#define LOG_TAG			"VSRC:MAX96701"

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


//#define USE_MCNEX_CAM_MODULE

/*
 * This object contains essential v4l2 objects such as sub-device and
 * ctrl_handler
 */
struct max96701 {
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
const struct reg_sequence max96701_reg_defaults[] = {
	{0x04, 0x47, 0},
	// Sensor Data Dout7 -> DIN0
	{0x20, 0x07, 0},
	{0x21, 0x06, 0},
	{0x22, 0x05, 0},
	{0x23, 0x04, 0},
	{0x24, 0x03, 0},
	{0x25, 0x02, 0},
	{0x26, 0x01, 0},
	{0x27, 0x00, 0},
	{0x30, 0x17, 0},
	{0x31, 0x16, 0},
	{0x32, 0x15, 0},
	{0x33, 0x14, 0},
	{0x34, 0x13, 0},
	{0x35, 0x12, 0},
	{0x36, 0x11, 0},
	{0x37, 0x10, 0},

	{0x43, 0x01, 0},
	{0x44, 0x00, 0},
	{0x45, 0x33, 0},
	{0x46, 0x90, 0},
	{0x47, 0x00, 0},
	{0x48, 0x0C, 0},
	{0x49, 0xE4, 0},
	{0x4A, 0x25, 0},
	{0x4B, 0x83, 0},
	{0x4C, 0x84, 0},
	{0x43, 0x21, 0},
};

/* for raw12 ispless camera module */
const struct reg_sequence max96701_reg_defaults_raw12[] = {
#ifdef USE_MCNEX_CAM_MODULE
	{0x04, 0x47, 0}, // configuration mode(in the case of pclk detection fail)
	{0x07, 0x40, 0}, // dbl off, hs/vs encoding off, 27bit
	{0x0f, 0xbe, 300*1000}, // GPO output low to reset sensor
	{0x0f, 0xbf, 100*1000}, // GPO output hight to reset release of sensor

	/* test */
	//{0x07, 0xa4, 0},
	//{0x4d, 0xc0, 0},
//	{0x07, 0xc0, 0}, // high bandwidth mode when BWS, HS/VS encoding disable
	//{0xff,  100, 0},

	// cross bar(Sensor Data Dout7 -> DIN0)
	{0x20, 0x0b, 0},
	{0x21, 0x0a, 0},
	{0x22, 0x09, 0},
	{0x23, 0x08, 0},
	{0x24, 0x07, 0},
	{0x25, 0x06, 0},
	{0x26, 0x05, 0},
	{0x27, 0x04, 0},
	{0x28, 0x03, 0},
	{0x29, 0x02, 0},
	{0x2a, 0x01, 0},
	{0x2b, 0x00, 0},
#else
	{0x04, 0x47, 50*1000},
	{0x07, 0xA4, 0},
#endif
};
static const struct regmap_config max96701_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static void max96701_init_format(struct max96701 *dev)
{
	dev->fmt.width = 1280;
	dev->fmt.height	= 720,
	dev->fmt.code = MEDIA_BUS_FMT_UYVY8_1X16;
	dev->fmt.field = V4L2_FIELD_NONE;
	dev->fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
}
/*
 * Helper fuctions for reflection
 */
static inline struct max96701 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max96701, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max96701_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max96701		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		if (dev->fmt.code == MEDIA_BUS_FMT_SGRBG12_1X12) {
			logi("input format is bayer raw\n");
			ret = regmap_multi_reg_write(dev->regmap,
					max96701_reg_defaults_raw12,
					ARRAY_SIZE(max96701_reg_defaults_raw12));
		}
		else {
			logi("input format is yuv422\n");
			ret = regmap_multi_reg_write(dev->regmap,
					max96701_reg_defaults,
					ARRAY_SIZE(max96701_reg_defaults));
		}
		if (ret < 0)
			loge("Fail initializing max96701 device\n");
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		//ret = regmap_write(dev->regmap, 0x15, 0x93);
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
static int max96701_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max96701		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->s_cnt == 0) && (enable == 1)) {
		ret = regmap_write(dev->regmap, 0x04, 0x87);
		if (ret < 0)
			loge("Fail Serialization max96701 device\n");
	} else if ((dev->s_cnt == 1) && (enable == 0)) {
		ret = regmap_write(dev->regmap, 0x04, 0x47);
	}

	if (enable)
		dev->s_cnt++;
	else
		dev->s_cnt--;

	msleep(30);

	mutex_unlock(&dev->lock);
	return ret;
}

static int max96701_get_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *format)
{
	struct max96701		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int max96701_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *format)
{
	struct max96701		*dev	= to_dev(sd);
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
static const struct v4l2_subdev_core_ops max96701_core_ops = {
	.init			= max96701_init,
};

static const struct v4l2_subdev_video_ops max96701_video_ops = {
	.s_stream		= max96701_s_stream,
};

static const struct v4l2_subdev_pad_ops max96701_pad_ops = {
	.get_fmt		= max96701_get_fmt,
	.set_fmt		= max96701_set_fmt,
};

static const struct v4l2_subdev_ops max96701_ops = {
	.core			= &max96701_core_ops,
	.video			= &max96701_video_ops,
	.pad			= &max96701_pad_ops,
};

struct max96701 max96701_data = {
};

static const struct i2c_device_id max96701_id[] = {
	{ "max96701", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max96701_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id max96701_of_match[] = {
	{
		.compatible	= "maxim,max96701",
		.data		= &max96701_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max96701_of_match);
#endif

int max96701_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max96701		*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int			ret	= 0;

	// allocate and clear memory for a device
	dev = devm_kzalloc(&client->dev, sizeof(struct max96701), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	mutex_init(&dev->lock);

	// set the specific information
	if (client->dev.of_node) {
		dev_id = of_match_node(max96701_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	// Register with V4L2 layer as a slave device
	v4l2_i2c_subdev_init(&dev->sd, client, &max96701_ops);

	// register a v4l2 sub device
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	// init regmap
	dev->regmap = devm_regmap_init_i2c(client, &max96701_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	/* init format info */
	max96701_init_format(dev);

	goto goto_end;

goto_free_device_data:
	// free the videosource data
	kfree(dev);

goto_end:
	return ret;
}

int max96701_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max96701		*dev	= to_dev(sd);

	// release regmap
	regmap_exit(dev->regmap);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max96701_driver = {
	.probe		= max96701_probe,
	.remove		= max96701_remove,
	.driver		= {
		.name		= "max96701",
		.of_match_table	= of_match_ptr(max96701_of_match),
	},
	.id_table	= max96701_id,
};

module_i2c_driver(max96701_driver);
