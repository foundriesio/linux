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

#define loge(fmt, ...)		pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logw(fmt, ...)		pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logd(fmt, ...)		pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logi(fmt, ...)		pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)

#define WIDTH			1280
#define HEIGHT			720

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
 * This object contains essential v4l2 objects such as sub-device and ctrl_handler
 */
struct max9286 {
	struct v4l2_subdev		sd;
	struct v4l2_ctrl_handler	hdl;

	struct power_sequence		gpio;

	/* Regmaps */
	struct regmap			*regmap;
};

const struct reg_sequence max9286_reg_defaults[] = {
	// init
	// enable 4ch
	{0X0A, 0X0F, 0},	 //    Write   Disable all Forward control channel
	{0X34, 0X35, 0},	 //    Write   Disable auto acknowledge
	{0X15, 0X83, 0},	 //    Write   Select the combined camera line format for CSI2 output
	{0X12, 0XF3, 5},	 //    Write   MIPI Output setting
//	{0xff, 5},		 //    Write   5mS
	{0X63, 0X00, 0},	 //    Write   Widows off
	{0X64, 0X00, 0},	 //    Write   Widows off
	{0X62, 0X1F, 0},	 //    Write   FRSYNC Diff off
#if 1
	{0x01, 0xc0, 0},	 //    Write   manual mode
	{0X08, 0X25, 0},	 //    Write   FSYNC-period-High
	{0X07, 0XC3, 0},	 //    Write   FSYNC-period-Mid
	{0X06, 0XF8, 5},	 //    Write   FSYNC-period-Low
#endif
	{0X00, 0XEF, 5},	 //    Write   Port 0~3 used
//	  {0x0c, 0x91},
//	{0xff, 5},		 //    Write   5mS
#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
	{0X15, 0X0B, 0},	 //    Write   Enable MIPI output (line concatenation)
#else
	{0X15, 0X9B, 0},	 //    Write   Enable MIPI output (line interleave)
#endif
	{0X69, 0XF0, 0},	 //    Write   Auto mask & comabck enable
	{0x01, 0x00, 0},
	{0X0A, 0XFF, 0},	 //    Write   All forward channel enable
};

static const struct regmap_config max9286_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

struct v4l2_dv_timings max9286_dv_timings = {
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
int max9286_parse_device_tree(struct max9286 * dev, struct i2c_client *client) {
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

void max9286_request_gpio(struct max9286 * dev) {
	if(0 < dev->gpio.pwr_port) {
		gpio_request(dev->gpio.pwr_port, "max9286 power");
		gpio_direction_output(dev->gpio.pwr_port, dev->gpio.pwr_value);
		logd("[pwr] gpio: %3d, new val: %d, cur val: %d\n", \
			dev->gpio.pwr_port, dev->gpio.pwr_value, gpio_get_value(dev->gpio.pwr_port));
	}
	if(0 < dev->gpio.pwd_port) {
		gpio_request(dev->gpio.pwd_port, "max9286 power down");
		gpio_direction_output(dev->gpio.pwd_port, dev->gpio.pwd_value);
		logd("[pwd] gpio: %3d, new val: %d, cur val: %d\n", \
			dev->gpio.pwd_port, dev->gpio.pwd_value, gpio_get_value(dev->gpio.pwd_port));
	}
	if(0 < dev->gpio.rst_port) {
		gpio_request(dev->gpio.rst_port, "max9286 reset");
		gpio_direction_output(dev->gpio.rst_port, dev->gpio.rst_value);
		logd("[rst] gpio: %3d, new val: %d, cur val: %d\n", \
			dev->gpio.rst_port, dev->gpio.rst_value, gpio_get_value(dev->gpio.rst_port));
	}
	if(0 < dev->gpio.intb_port) {
		gpio_request(dev->gpio.intb_port, "max9286 interrupt");
		gpio_direction_input(dev->gpio.intb_port);
		logd("[int] gpio: %3d, new val: %d, cur val: %d\n", \
			dev->gpio.intb_port, dev->gpio.intb_value, gpio_get_value(dev->gpio.intb_port));
	}
}

void max9286_free_gpio(struct max9286 * dev) {
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
static inline struct max9286 *to_state(struct v4l2_subdev *sd) {
	return container_of(sd, struct max9286, sd);
}

/*
 * v4l2_ctrl_ops implementations
 */
static int max9286_s_ctrl(struct v4l2_ctrl *ctrl) {
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
static int max9286_set_power(struct v4l2_subdev *sd, int on) {
	struct max9286		*dev	= to_state(sd);
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
static int max9286_g_input_status(struct v4l2_subdev *sd, u32 *status) {
	struct max9286		*dev	= to_state(sd);
	unsigned int		val	= 0;
	int			ret	= 0;

	// check V4L2_IN_ST_NO_SIGNAL
	ret = regmap_read(dev->regmap, MAX9286_REG_STATUS_1, &val);
	if(ret < 0) {
		loge("failure to check V4L2_IN_ST_NO_SIGNAL\n");
	} else {
		loge("status: 0x%08x\n", val);
		if(val == MAX9286_VAL_STATUS_1) {
			*status &= ~V4L2_IN_ST_NO_SIGNAL;
		} else {
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}
	}

	return ret;
}

static int max9286_s_stream(struct v4l2_subdev *sd, int enable) {
	struct max9286		* dev	= NULL;
	int			ret	= 0;

	dev = to_state(sd);
	if(!dev) {
		loge("Failed to get video source object by subdev\n");
		ret = -EINVAL;
	} else {
		ret = regmap_multi_reg_write(dev->regmap, max9286_reg_defaults, ARRAY_SIZE(max9286_reg_defaults));
	}

	return ret;
}

static int max9286_g_dv_timings(struct v4l2_subdev *sd, struct v4l2_dv_timings *timings) {
	memcpy((void *)timings, (const void *)&max9286_dv_timings, sizeof(*timings));

	return 0;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_ctrl_ops max9286_ctrl_ops = {
	.s_ctrl			= max9286_s_ctrl,
};

static const struct v4l2_subdev_core_ops max9286_core_ops = {
	.s_power		= max9286_set_power,
};

static const struct v4l2_subdev_video_ops max9286_video_ops = {
	.g_input_status		= max9286_g_input_status,
	.s_stream		= max9286_s_stream,
	.g_dv_timings		= max9286_g_dv_timings,
};

static const struct v4l2_subdev_ops max9286_ops = {
	.core			= &max9286_core_ops,
	.video			= &max9286_video_ops,
};

struct max9286 max9286_data = {
};

static const struct i2c_device_id max9286_id[] = {
	{ "max9286", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max9286_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id max9286_of_match[] = {
	{
		.compatible	= "maxim,max9286",
		.data		= &max9286_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max9286_of_match);
#endif

int max9286_probe(struct i2c_client * client, const struct i2c_device_id * id) {
	struct max9286		*dev	= NULL;
	struct of_device_id	*dev_id	= NULL;
	int			ret	= 0;

	// allocate and clear memory for a device
	dev = devm_kzalloc(&client->dev, sizeof(struct max9286), GFP_KERNEL);
	if(dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	// set the specific information
	if(client->dev.of_node) {
		dev_id = of_match_node(max9286_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n", client->name, (client->addr)<<1, client);

	// parse the device tree
	if((ret = max9286_parse_device_tree(dev, client)) < 0) {
		loge("cannot initialize gpio port\n");
		return ret;
	}

	// Register with V4L2 layer as a slave device
	v4l2_i2c_subdev_init(&dev->sd, client, &max9286_ops);

	// regitster v4l2 control handlers
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &max9286_ctrl_ops, V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl, &max9286_ctrl_ops, V4L2_CID_DV_RX_IT_CONTENT_TYPE, V4L2_DV_IT_CONTENT_TYPE_NO_ITC, 0, V4L2_DV_IT_CONTENT_TYPE_NO_ITC);
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
	max9286_request_gpio(dev);

	// init regmap
        dev->regmap = devm_regmap_init_i2c(client, &max9286_regmap);
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

int max9286_remove(struct i2c_client * client) {
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max9286		*dev	= to_state(sd);

	// release regmap
        regmap_exit(dev->regmap);

	// gree gpio
	max9286_free_gpio(dev);

	v4l2_ctrl_handler_free(&dev->hdl);

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
