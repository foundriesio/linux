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

#define LOG_TAG			"VSRC:MAX96705"

#define loge(fmt, ...)		pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logw(fmt, ...)		pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logd(fmt, ...)		pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logi(fmt, ...)		pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)

//#define CONFIG_MIPI_1_CH_CAMERA
//#define CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT

#if defined (CONFIG_MIPI_1_CH_CAMERA) || !defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
#define NTSC_NUM_ACTIVE_PIXELS (1280)
#else
#define NTSC_NUM_ACTIVE_PIXELS (5120)
#endif
#define NTSC_NUM_ACTIVE_LINES (720)

#define WIDTH NTSC_NUM_ACTIVE_PIXELS
#define HEIGHT NTSC_NUM_ACTIVE_LINES

/*
 * This object contains essential v4l2 objects such as sub-device and ctrl_handler
 */
struct max96705 {
	struct v4l2_subdev		sd;

	/* Regmaps */
	struct regmap			*regmap;
};

const struct reg_sequence max96705_reg_defaults[] = {
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

static const struct regmap_config max96705_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

/*
 * Helper fuctions for reflection
 */
static inline struct max96705 *to_state(struct v4l2_subdev *sd) {
	return container_of(sd, struct max96705, sd);
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int max96705_s_stream(struct v4l2_subdev *sd, int enable) {
	struct max96705		* dev	= NULL;
	int			ret	= 0;

	dev = to_state(sd);
	if(!dev) {
		loge("Failed to get video source object by subdev\n");
		ret = -EINVAL;
	} else {
		ret = regmap_multi_reg_write(dev->regmap, max96705_reg_defaults, ARRAY_SIZE(max96705_reg_defaults));
	}

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_subdev_video_ops max96705_video_ops = {
	.s_stream		= max96705_s_stream,
};

static const struct v4l2_subdev_ops max96705_ops = {
	.video			= &max96705_video_ops,
};

struct max96705 max96705_data = {
};

static const struct i2c_device_id max96705_id[] = {
	{ "max96705", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max96705_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id max96705_of_match[] = {
	{
		.compatible	= "maxim,max96705",
		.data		= &max96705_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max96705_of_match);
#endif

int max96705_probe(struct i2c_client * client, const struct i2c_device_id * id) {
	struct max96705		*dev	= NULL;
	struct of_device_id	*dev_id	= NULL;
	int			ret	= 0;

	// allocate and clear memory for a device
	dev = devm_kzalloc(&client->dev, sizeof(struct max96705), GFP_KERNEL);
	if(dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	// set the specific information
	if(client->dev.of_node) {
		dev_id = of_match_node(max96705_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n", client->name, (client->addr)<<1, client);

	// Register with V4L2 layer as a slave device
	v4l2_i2c_subdev_init(&dev->sd, client, &max96705_ops);

	// register a v4l2 sub device
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret) {
		loge("Failed to register subdevice\n");
	} else {
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);
	}


	// init regmap
        dev->regmap = devm_regmap_init_i2c(client, &max96705_regmap);
	if(IS_ERR(dev->regmap)) {
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

int max96705_remove(struct i2c_client * client) {
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max96705		*dev	= to_state(sd);

	// release regmap
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
