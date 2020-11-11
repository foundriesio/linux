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

#define LOG_TAG			"VSRC:MAX96712"

#define loge(fmt, ...)		pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logw(fmt, ...)		pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logd(fmt, ...)		pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logi(fmt, ...)		pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)

#define WIDTH			1920
#define HEIGHT			(1080 - 2)

#define MAX96712_REG_STATUS_1	0x1E
#define MAX96712_VAL_STATUS_1	0x40

#define TEST_2_INPUT_PORTS

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
 * This object contains essential v4l2 objects such as sub-device and ctrl_handler
 */
struct max96712 {
	struct v4l2_subdev		sd;
	struct v4l2_ctrl_handler	hdl;

	struct power_sequence		gpio;

	/* Regmaps */
	struct regmap			*regmap;
};

const struct reg_sequence max96712_reg_init[] = {
	/* 
	 * MAX96712 (0x52) powers up in GMSL1 mode,
	 * HIM enabled
	 *
	 * 2 MAX9275 power up with I2C address (0x80)
	 * BWS=0, HIBW=1, DRS=0
	 */
	{0x0B05, 0x79, 0},
	{0x0C05, 0x79, 0},
	{0x0D05, 0x79, 0},
	{0x0E05, 0x79, 0},

	/*
	 * choose HVD source
	 */
	{0x0B06, 0xE8, 0},
	{0x0C06, 0xE8, 0},
	{0x0D06, 0xE8, 0},
	{0x0E06, 0xE8, 0},
	// {0x0330,0x04, 0}, Default = 0x04
	
	/*
	 * set output lanes, port A 4 lanes
	 */
	//{0x090A, 0x00, 0},
	{0x094A, 0xC0, 0},
	{0x098A, 0xC0, 0},
	//{0x04CA, 0x00, 0},

	/*
	 * Phy1 set pll (x 100Mbps)
	 */
	{0x0418, 0xEA, 0}, // overide
	{0x041B, 0x2A, 0},

	/*
	 * invert VSYNC for pipe X Y Z U
	 */
	//{0x01D9, 0x59, 0},
	//{0x01F9, 0x59, 0},
	//{0x0219, 0x59, 0},
	//{0x0239, 0x59, 0},

	/*
	 * enable GMSL1 link A B C D
	 */
#ifndef TEST_2_INPUT_PORTS
	{0x0006, 0x01, 0},
#endif
	/*
	 * lane ping for 4-lane port A and B
	 */
	{0x08A3, 0xE4, 0},
	{0x08A4, 0xE4, 0},
	{0x01DA, 0x18, 0},
#ifdef TEST_2_INPUT_PORTS
	{0x01FA, 0x18, 0},
#endif
	/* 
	 * Set DT(0x1E), VC, BPP(0x10)
	 * FOR PIPE X Y Z U setting for RGB888 
	 */
	{0x0411, 0x90, 0}, // 100 1_0000
	{0x0412, 0x40, 0}, // 0 100_00 00
	{0x040B, 0x82, 0}, // 1000_0 010
	{0x040E, 0x5E, 0}, // 01 01_1110
	{0x040F, 0x7E, 0}, // 0111 1110
	{0x0410, 0x7A, 0}, // 0111_10 10
	{0x0415, 0xEF, 0}, // override VC_DT_BPP

	/*
	 * 3 data map en fot pipe x
	 */
	{0x090B, 0x07, 0},
	{0x092D, 0x15, 0},
	{0x090D, 0x1E, 0},
	{0x090E, 0x1E, 0},
	{0x090F, 0x00, 0},
	{0x0910, 0x00, 0},
	{0x0911, 0x01, 0},
	{0x0912, 0x01, 0},

	/*
	 * 3 data map en fot pipe y
	 */
	{0x094B, 0x07, 0},
	{0x096D, 0x15, 0},
	{0x094D, 0x1E, 0},
	{0x094E, 0x5E, 0},
	{0x094F, 0x00, 0},
	{0x0950, 0x40, 0},
	{0x0951, 0x01, 0},
	{0x0952, 0x41, 0},

	/*
	 * HIBW=1
	 */
	{0x0B07, 0x08, 0},
	{0x0C07, 0x08, 0},
	{0x0D07, 0x08, 0},
	{0x0E07, 0x08, 0},

	/*
	 * Enable GMSL1 to GMSL2 color mapping (D1) and
	 * set mapping type (D[7:3]) stored below
	 */
	{0x0B96, 0x9B, 0}, //1001 1011
	{0x0C96, 0x9B, 0},
	{0x0D96, 0x9B, 0},
	{0x0E96, 0x9B, 0},
	
	/*
	 * Shift GMSL1 HVD
	 */
	{0x0BA7, 0x45, 0},
	{0x0CA7, 0x45, 0},
	{0x0DA7, 0x45, 0},
	{0x0EA7, 0x45, 0},

	/*
	 * set HS/VS output on MFP pins for debug
	 */

	/*
	 * enable processing HS and DE signals
	 */
	{0x0B0F, 0x09, 0},
	{0x0C0F, 0x09, 0},
	{0x0D0F, 0x09, 0},
	{0x0E0F, 0x09, 0},

	/*
	 * set local ack
	 */
	{0x0B0D, 0x80, 0},
	{0x0C0D, 0x80, 0},
	{0x0D0D, 0x80, 0},
	{0x0E0D, 0x80, 50},
#ifdef TEST_2_INPUT_PORTS
	{0x0006, 0x03, 50},
#endif
};

const struct reg_sequence max96712_reg_s_stream[] = {
	{0x0B0D, 0x00, 0},
	{0x0C0D, 0x00, 0},
	{0x0D0D, 0x00, 0},
	{0x0E0D, 0x00, 50},
};

static const struct regmap_config max96712_regmap = {
	.reg_bits		= 16,
	.val_bits		= 8,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

struct v4l2_dv_timings max96712_dv_timings = {
	.type	= V4L2_DV_BT_656_1120,
	.bt	= {
		.width		= WIDTH,
		.height		= HEIGHT,
		.interlaced	= V4L2_DV_PROGRESSIVE,
		.polarities	= 0,//V4L2_DV_HSYNC_POS_POL | V4L2_DV_VSYNC_POS_POL,
	},
};

/*
 * gpio fuctions
 */
int max96712_parse_device_tree(struct max96712 * dev, struct i2c_client *client) {
	struct device_node	*node	= client->dev.of_node;
	int			ret	= 0;

	if(node) {
		dev->gpio.pwr_port = of_get_named_gpio_flags(node, "pwr-gpios", 0, &dev->gpio.pwr_value);
		dev->gpio.pwd_port = of_get_named_gpio_flags(node, "pwd-gpios", 0, &dev->gpio.pwd_value);
		dev->gpio.rst_port = of_get_named_gpio_flags(node, "rst-gpios", 0, &dev->gpio.rst_value);
		dev->gpio.intb_port = of_get_named_gpio_flags(node, "intb-gpios", 0, &dev->gpio.intb_value);
	} else {
		loge("could not find sensor module node!! \n");
		ret = -ENODEV;
	}

	return ret;
}

void max96712_request_gpio(struct max96712 * dev) {
	if(0 < dev->gpio.pwr_port) {
		gpio_request(dev->gpio.pwr_port, "max96712 power");
		gpio_direction_output(dev->gpio.pwr_port, dev->gpio.pwr_value);
		logd("[pwr] gpio: %3d, new val: %d, cur val: %d\n", \
			dev->gpio.pwr_port, dev->gpio.pwr_value, gpio_get_value(dev->gpio.pwr_port));
	}
	if(0 < dev->gpio.pwd_port) {
		gpio_request(dev->gpio.pwd_port, "max96712 power down");
		gpio_direction_output(dev->gpio.pwd_port, dev->gpio.pwd_value);
		logd("[pwd] gpio: %3d, new val: %d, cur val: %d\n", \
			dev->gpio.pwd_port, dev->gpio.pwd_value, gpio_get_value(dev->gpio.pwd_port));
	}
	if(0 < dev->gpio.rst_port) {
		gpio_request(dev->gpio.rst_port, "max96712 reset");
		gpio_direction_output(dev->gpio.rst_port, dev->gpio.rst_value);
		logd("[rst] gpio: %3d, new val: %d, cur val: %d\n", \
			dev->gpio.rst_port, dev->gpio.rst_value, gpio_get_value(dev->gpio.rst_port));
	}
	if(0 < dev->gpio.intb_port) {
		gpio_request(dev->gpio.intb_port, "max96712 interrupt");
		gpio_direction_input(dev->gpio.intb_port);
		logd("[int] gpio: %3d, new val: %d, cur val: %d\n", \
			dev->gpio.intb_port, dev->gpio.intb_value, gpio_get_value(dev->gpio.intb_port));
	}
}

void max96712_free_gpio(struct max96712 * dev) {
	if(0 < dev->gpio.pwr_port)
		gpio_free(dev->gpio.pwr_port);
	if(0 < dev->gpio.pwd_port)
		gpio_free(dev->gpio.pwd_port);
	if(0 < dev->gpio.rst_port)
		gpio_free(dev->gpio.rst_port);
	if(0 < dev->gpio.intb_port)
		gpio_free(dev->gpio.intb_port);
}

/*
 * Helper fuctions for reflection
 */
static inline struct max96712 *to_state(struct v4l2_subdev *sd) {
	return container_of(sd, struct max96712, sd);
}

/*
 * v4l2_ctrl_ops implementations
 */
static int max96712_s_ctrl(struct v4l2_ctrl *ctrl) {
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
static int max96712_init(struct v4l2_subdev *sd, u32 val) {
	struct max96712		* dev	= NULL;
	int			ret	= 0;

	dev = to_state(sd);
	if(!dev) {
		loge("Failed to get video source object by subdev\n");
		ret = -EINVAL;
	} else {
		ret = regmap_multi_reg_write(dev->regmap, max96712_reg_init, ARRAY_SIZE(max96712_reg_init));
		if(ret)
			loge("regmap_multi_reg_write returned %d\n", ret);
	}

	return ret;
}

static int max96712_set_power(struct v4l2_subdev *sd, int on) {
	struct max96712		*dev	= to_state(sd);
	struct power_sequence	*gpio	= &dev->gpio;

	if(on) {
		if(0 < dev->gpio.pwd_port) {
			gpio_set_value_cansleep(gpio->pwd_port, 1);
			msleep(20);
		}
	} else {
		if(0 < dev->gpio.pwd_port)
			gpio_set_value_cansleep(gpio->pwd_port, 0);
		msleep(5);
	}

	return 0;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int max96712_g_input_status(struct v4l2_subdev *sd, u32 *status) {
	struct max96712		*dev	= to_state(sd);
	unsigned int		val	= 0;
	int			ret	= 0;

	// check V4L2_IN_ST_NO_SIGNAL
	ret = regmap_read(dev->regmap, MAX96712_REG_STATUS_1, &val);
	if(ret < 0) {
		loge("failure to check V4L2_IN_ST_NO_SIGNAL\n");
	} else {
		logd("status: 0x%08x\n", val);
		if(val == MAX96712_VAL_STATUS_1) {
			*status &= ~V4L2_IN_ST_NO_SIGNAL;
		} else {
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}
	}

	return ret;
}

static int max96712_s_stream(struct v4l2_subdev *sd, int enable) {
	struct max96712		* dev	= NULL;
	int			ret	= 0;

	dev = to_state(sd);
	if(!dev) {
		loge("Failed to get video source object by subdev\n");
		ret = -EINVAL;
	} else {
		ret = regmap_multi_reg_write(dev->regmap, max96712_reg_s_stream, ARRAY_SIZE(max96712_reg_s_stream));
		if(ret)
			loge("regmap_multi_reg_write returned %d\n", ret);
	}

	return ret;
}

static int max96712_g_dv_timings(struct v4l2_subdev *sd, struct v4l2_dv_timings *timings) {
	memcpy((void *)timings, (const void *)&max96712_dv_timings, sizeof(*timings));

	return 0;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_ctrl_ops max96712_ctrl_ops = {
	.s_ctrl			= max96712_s_ctrl,
};

static const struct v4l2_subdev_core_ops max96712_core_ops = {
	.init			= max96712_init,
	.s_power		= max96712_set_power,
};

static const struct v4l2_subdev_video_ops max96712_video_ops = {
	.g_input_status		= max96712_g_input_status,
	.s_stream		= max96712_s_stream,
	.g_dv_timings		= max96712_g_dv_timings,
};

static const struct v4l2_subdev_ops max96712_ops = {
	.core			= &max96712_core_ops,
	.video			= &max96712_video_ops,
};

struct max96712 max96712_data = {
};

static const struct i2c_device_id max96712_id[] = {
	{ "max96712", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max96712_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id max96712_of_match[] = {
	{
		.compatible	= "maxim,max96712",
		.data		= &max96712_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max96712_of_match);
#endif

int max96712_probe(struct i2c_client * client, const struct i2c_device_id * id) {
	struct max96712		*dev	= NULL;
	struct of_device_id	*dev_id	= NULL; 
	int			ret	= 0;

	// allocate and clear memory for a device
	dev = devm_kzalloc(&client->dev, sizeof(struct max96712), GFP_KERNEL);
	if(dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	// set the specific information
	if(client->dev.of_node) {
		dev_id = of_match_node(max96712_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n", client->name, (client->addr)<<1, client);

	// parse the device tree
	if((ret = max96712_parse_device_tree(dev, client)) < 0) {
		loge("cannot initialize gpio port\n");
		return ret;
	}

	// Register with V4L2 layer as a slave device
	v4l2_i2c_subdev_init(&dev->sd, client, &max96712_ops);

	// regitster v4l2 control handlers
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &max96712_ctrl_ops, V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl, &max96712_ctrl_ops, V4L2_CID_DV_RX_IT_CONTENT_TYPE, V4L2_DV_IT_CONTENT_TYPE_NO_ITC, 0, V4L2_DV_IT_CONTENT_TYPE_NO_ITC);
	dev->sd.ctrl_handler = &dev->hdl;
	if (dev->hdl.error) {
		loge("v4l2_ctrl_handler_init is wrong\n");
		ret = dev->hdl.error;
		goto goto_free_device_data;
	}

	// register a v4l2 sub device
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret) {
		loge("Failed to register subdevice\n");
	} else {
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);
	}

	// request gpio
	max96712_request_gpio(dev);

	// init regmap
        dev->regmap = devm_regmap_init_i2c(client, &max96712_regmap);
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

int max96712_remove(struct i2c_client * client) {
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max96712		*dev	= to_state(sd);

	// release regmap
        regmap_exit(dev->regmap);

	// gree gpio
	max96712_free_gpio(dev);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max96712_driver = {
	.probe		= max96712_probe,
	.remove		= max96712_remove,
	.driver		= {
		.name		= "max96712",
		.of_match_table	= of_match_ptr(max96712_of_match),
	},
	.id_table	= max96712_id,
};

module_i2c_driver(max96712_driver);

