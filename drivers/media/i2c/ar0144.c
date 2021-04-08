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

#define LOG_TAG			"VSRC:ar0144"

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
struct ar0144 {
	struct v4l2_subdev		sd;
	struct v4l2_mbus_framefmt	fmt;
	struct v4l2_ctrl_handler	hdl;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

const struct reg_sequence ar0144_reg_init[] = {
	/****** Sensor AR0144 Setting ******/
	{0x301A, 0x00D9, 200*1000},  // RESET_REGISTER
	{0x301A, 0x30D8, 0},  // RESET_REGISTER
	{0x3ED6, 0x3CB5, 0},  // DAC_LD_10_11
	{0x3ED8, 0x8765, 0},  // DAC_LD_12_13
	{0x3EDA, 0x8888, 0},  // DAC_LD_14_15
	{0x3EDC, 0x97FF, 0},  // DAC_LD_16_17
	{0x3EF8, 0x6522, 0},  // DAC_LD_44_45
	{0x3EFA, 0x2222, 0},  // DAC_LD_46_47
	{0x3EFC, 0x6666, 0},  // DAC_LD_48_49
	{0x3F00, 0xAA05, 0},  // DAC_LD_52_53
	{0x3EE2, 0x180E, 0},  // DAC_LD_22_23
	{0x3EE4, 0x0808, 0},  // DAC_LD_24_25
	{0x3EEA, 0x2A09, 0},  // DAC_LD_30_31
	{0x3060, 0x000D, 0},  // ANALOG_GAIN
	{0x3092, 0x00CF, 0},  // ROW_NOISE_CONTROL
	{0x3268, 0x0030, 0},  // SEQUENCER_CONTROL
	{0x3786, 0x0006, 0},  // DIGITAL_CTRL_1
	{0x3F4A, 0x0F70, 0},  // DELTA_DK_PIX_THRES
	{0x306E, 0x4810, 0},  // DATAPATH_SELECT
	{0x3064, 0x1802, 0},  // SMIA_TEST
	{0x3EF6, 0x804D, 0},  // DAC_LD_42_43
	{0x3180, 0xC08F, 0},  // DELTA_DK_CONTROL
	{0x30BA, 0x7623, 0},  // DIGITAL_CTRL
	{0x3176, 0x0480, 0},  // DELTA_DK_ADJUST_GREENR
	{0x3178, 0x0480, 0},  // DELTA_DK_ADJUST_RED
	{0x317A, 0x0480, 0},  // DELTA_DK_ADJUST_BLUE
	{0x317C, 0x0480, 0},  // DELTA_DK_ADJUST_GREENB
	{0x302A, 0x0006, 0},  // VT_PIX_CLK_DIV
	{0x302C, 0x0001, 0},  // VT_SYS_CLK_DIV
	{0x302E, 0x0004, 0},  // PRE_PLL_CLK_DIV
	{0x3030, 0x0042, 0},  // PLL_MULTIPLIER
	{0x3036, 0x000C, 0},  // OP_PIX_CLK_DIV
	{0x3038, 0x0001, 0},  // OP_SYS_CLK_DIV
	{0x30B0, 0x0038, 0},  // DIGITAL_TEST
	{0x3002, 0x0000, 0},  // Y_ADDR_START
	{0x3004, 0x0004, 0},  // X_ADDR_START
	{0x3006, 0x031F, 0},  // Y_ADDR_END
	{0x3008, 0x0503, 0},  // X_ADDR_END
	{0x300A, 0x0339, 0},  // FRAME_LENGTH_LINES
	{0x300C, 0x05D0, 0},  // LINE_LENGTH_PCK
	{0x3012, 0x0064, 0},  // COARSE_INTEGRATION_TIME
	{0x30A2, 0x0001, 0},  // X_ODD_INC
	{0x30A6, 0x0001, 0},  // Y_ODD_INC
	{0x3040, 0x0000, 0},  // READ_MODE
	{0x31AE, 0x0200, 0},  // SERIAL_FORMAT
	{0x3040, 0x0400, 0},  // READ_MODE
	{0x30A8, 0x0003, 0},  // Y_ODD_INC_CB
	{0x3040, 0x0C00, 0},  // READ_MODE
	{0x30AE, 0x0003, 0},  // X_ODD_INC_CB
	{0x3028, 0x0010, 0},  // ROW_SPEED
	{0x301A, 0x30DC, 0},  // RESET_REGISTER
	{0x3064, 0x1802, 0},  // SMIA_TEST
	{0x3064, 0x1802, 0},  // SMIA_TEST
	{0x3100, 0x0000, 0},  // AECTRLREG
	{0x3270, 0x0100, 0},  // flash out enable
};

static const struct regmap_config ar0144_regmap = {
	.reg_bits		= 16,
	.val_bits		= 16,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

static void ar0144_init_format(struct ar0144 *dev)
{
	dev->fmt.width = 1280;
	dev->fmt.height	= 800;
	dev->fmt.code = MEDIA_BUS_FMT_Y8_1X8;
	dev->fmt.field = V4L2_FIELD_NONE;
	dev->fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
}

/*
 * Helper fuctions for reflection
 */
static inline struct ar0144 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ar0144, sd);
}

/*
 * v4l2_ctrl_ops implementations
 */
static int ar0144_s_ctrl(struct v4l2_ctrl *ctrl)
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
static int ar0144_init(struct v4l2_subdev *sd, u32 enable)
{
	struct ar0144		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		/* enable ar0144 */
		ret = regmap_multi_reg_write(dev->regmap,
				ar0144_reg_init,
				ARRAY_SIZE(ar0144_reg_init));
		if (ret) {
			/* err status */
			loge("regmap_multi_reg_write returned %d\n", ret);
		}
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* disable ar0144 */
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
static int ar0144_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ar0144		*dev	= to_dev(sd);
	int			ret	= 0;
	unsigned int readData		= 0;

	mutex_lock(&dev->lock);

	ret = regmap_read(dev->regmap, 0x3040, &readData);
	if (ret < 0) {
		/* read fail */
		loge("Fail ar0144 Read Data\n");
	}

	mutex_unlock(&dev->lock);
	return ret;
}

static int ar0144_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	struct ar0144		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int ar0144_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *format)
{
	struct ar0144		*dev	= to_dev(sd);
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
static const struct v4l2_ctrl_ops ar0144_ctrl_ops = {
	.s_ctrl			= ar0144_s_ctrl,
};

static const struct v4l2_subdev_core_ops ar0144_core_ops = {
	.init			= ar0144_init,
};

static const struct v4l2_subdev_video_ops ar0144_video_ops = {
	.s_stream		= ar0144_s_stream,
};

static const struct v4l2_subdev_pad_ops ar0144_pad_ops = {
	.get_fmt		= ar0144_get_fmt,
	.set_fmt		= ar0144_set_fmt,
};

static const struct v4l2_subdev_ops ar0144_ops = {
	.core			= &ar0144_core_ops,
	.video			= &ar0144_video_ops,
	.pad			= &ar0144_pad_ops,
};

struct ar0144 ar0144_data = {
};

static const struct i2c_device_id ar0144_id[] = {
	{ "ar0144", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ar0144_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ar0144_of_match[] = {
	{
		.compatible	= "onnn,ar0144",
		.data		= &ar0144_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, ar0144_of_match);
#endif

int ar0144_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ar0144			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct ar0144), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(ar0144_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &ar0144_ops);

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &ar0144_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl,
		&ar0144_ctrl_ops,
		V4L2_CID_DV_RX_IT_CONTENT_TYPE,
		V4L2_DV_IT_CONTENT_TYPE_NO_ITC,
		0,
		V4L2_DV_IT_CONTENT_TYPE_NO_ITC);
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

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &ar0144_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	/* init format info */
	ar0144_init_format(dev);

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int ar0144_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct ar0144		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver ar0144_driver = {
	.probe		= ar0144_probe,
	.remove		= ar0144_remove,
	.driver		= {
		.name		= "ar0144",
		.of_match_table	= of_match_ptr(ar0144_of_match),
	},
	.id_table	= ar0144_id,
};

module_i2c_driver(ar0144_driver);
