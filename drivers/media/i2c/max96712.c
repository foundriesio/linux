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
#include "max96712-reg.h"

#define LOG_TAG			"VSRC:MAX96712"

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

#define DEFAULT_WIDTH		(1920)
#define DEFAULT_HEIGHT		(1080)

#define MAX96712_LINK_MODE	MAX96712_GMSL1_4CH

/*
 * TODO
 * The defines below must be modified according to your device
 * The default values are for ar0147(sensor) and max96701(serializer)
 */
/* 1. remote devie info - sensor(ar0147) */
#define AR0147_SLAVE_ADDR			(0x20)

#define MAX96712_SENSOR_SLAVE_ADDR		AR0147_SLAVE_ADDR
#define MAX96712_SENSOR_SLAVE_ADDR_ALIAS0	(AR0147_SLAVE_ADDR + 2)
#define MAX96712_SENSOR_SLAVE_ADDR_ALIAS1	(AR0147_SLAVE_ADDR + 4)
#define MAX96712_SENSOR_SLAVE_ADDR_ALIAS2	(AR0147_SLAVE_ADDR + 6)
#define MAX96712_SENSOR_SLAVE_ADDR_ALIAS3	(AR0147_SLAVE_ADDR + 8)
/* 2. remote devie info - serializer(max96701) */
#define MAX96701_SLAVE_ADDR			(0x80)
#define MAX96701_REG_I2C_SOURCE_B		(0x0B)
#define MAX96701_REG_I2C_DEST_B			(0x0C)

#define MAX96712_SER_SLAVE_ADDR			MAX96701_SLAVE_ADDR
#define MAX96712_SER_REG_I2C_SOURC		MAX96701_REG_I2C_SOURCE_B
#define MAX96712_SER_REG_I2C_DEST		MAX96701_REG_I2C_DEST_B

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
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct max96712 {
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

/* for yuv422 4ch input */
const struct reg_sequence max96712_reg_defaults[] = {
	/*
	 * DE_SKEW_INIT
	 * Enable auto initial de-skew packets
	 * with the minimum width 32K UI
	 */
	{0x0943, 0x80, 0},
	/*
	 * DE_SKEW_PER
	 * Enable periodic de-skew packets
	 * with width 2K UI every 4 frames
	 */
	{0x0944, 0x91, 0},
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
	{0x0415, 0xEF, 0}, // Override Enable
	{0x0418, 0xF1, 0}, // overide
	{0x041B, 0x2A, 0},

	/*
	 * invert VSYNC for pipe X Y Z U
	 */
	//{0x01D9, 0x59, 0},
	//{0x01F9, 0x59, 0},
	//{0x0219, 0x59, 0},
	//{0x0239, 0x59, 0},

	/*
	 * lane ping for 4-lane port A and B
	 */
	{0x08A3, 0xE4, 0},
	{0x08A4, 0xE4, 0},
	{0x01DA, 0x18, 0},
	{0x01FA, 0x18, 0},
	{0x021A, 0x18, 0},
	{0x023A, 0x18, 0},
	/*
	 * Set DT(0x1E), VC, BPP(0x10)
	 * FOR PIPE X Y Z U setting for RGB888
	 */
	{0x0411, 0x90, 0}, // 100 1_0000
	{0x0412, 0x40, 0}, // 0 100_00 00
	//{0x040B, 0x82, 0}, // 1000_0 010 CSI OUTPUT ENABLE
	{0x040B, 0x80, 0}, // 1000_0 010 CSI OUTPUT DISABLE
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

	// 3 data map en fot pipe x, VC2
	{0x098B, 0x07, 0},
	{0x09AD, 0x15, 0},
	{0x098D, 0x1E, 0},
	{0x098E, 0x9E, 0},
	{0x098F, 0x00, 0},
	{0x0990, 0x80, 0},
	{0x0991, 0x01, 0},
	{0x0992, 0x81, 0},
	// // 3 data map en fot pipe y, VC3
	{0x09CB, 0x07, 0},
	{0x09ED, 0x15, 0},
	{0x09CD, 0x1E, 0},
	{0x09CE, 0xDE, 0},
	{0x09CF, 0x00, 0},
	{0x09D0, 0xC0, 0},
	{0x09D1, 0x01, 0},
	{0x09D2, 0xC1, 0},
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
	{0x0E0D, 0x80, 0},
	/*
	 * enable GMSL1 link A B C D
	 */
	//{0x0006, 0x0f, 50},
	{0x0006, MAX96712_LINK_MODE, 50*1000},
};

/* for raw12 4ch input */
const struct reg_sequence max96712_reg_defaults_raw12[] = {
	//----- DeSerializer MAX96712 GMSL1 mode setting -----
	{0x0006, 0x00, 0}, // GMSL1 mode  All LINK Disable.

	// Pipe line Enable and control
	{0x00F4, 0x0F, 0}, // pipe 0~3 enable
	{0x00F0, 0x62, 0}, // PHY0,1-Z 0110 0010
	{0x00F1, 0xEA, 0}, // PHY0,1-Z 1110 1010

	{0x0B06, 0xEF, 0}, // HIM =1
	{0x0C06, 0xEF, 0}, // HIM =1
	{0x0D06, 0xEF, 0}, // HIM =1
	{0x0E06, 0xEF, 0}, // HIM =1
	{0x0010, 0x11, 0}, // 3G mode
	{0x0011, 0x11, 0}, // 3G mode
	{0x0B0F, 0x01, 0}, // Disable DE_EN
	{0x0C0F, 0x01, 0}, // Disable DE_EN
	{0x0D0F, 0x01, 0}, // Disable DE_EN
	{0x0E0F, 0x01, 0}, // Disable DE_EN

	//0x04,0x90,0x14,0xC4,0x04,  //REV FAST GMSL1(disable)

	{0x0B0D, 0x80, 0}, // Local ACK for pipe 0 EN
	{0x0C0D, 0x80, 0}, // Local ACK for pipe 1 EN
	{0x0D0D, 0x80, 0}, // Local ACK for pipe 2 EN
	{0x0E0D, 0x80, 0}, // Local ACK for pipe 3 EN

	{0x0B07, 0xA4, 0}, // DBL BWS and HVEN =1
	{0x0C07, 0xA4, 0}, // DBL BWS and HVEN =1
	{0x0D07, 0xA4, 0}, // DBL BWS and HVEN =1
	{0x0E07, 0xA4, 0}, // DBL BWS and HVEN =1

	{0x0018, 0x0F, 100*1000}, // ----- One shot reset
	//{0xFF,0x64, 0}, // sleep 100ms

	// Pipe plie Enable and control
	{0x00F4, 0x0F, 0}, // pipe 0~3 enable
	{0x00F0, 0x62, 0}, // PHY0 1-Z 0110 0010
	{0x00F1, 0xEA, 0}, // PHY0 1-Z 1110 1010

	{0x040B, 0x60, 0}, // bpp for pipe0 and CSI_EN 5'b0_1100 0110 0 010

	// Initial VC
	{0x040C, 0x00, 0}, // VC=00 for pipe0 & pipe1
	{0x040D, 0x00, 0}, // VC=00 for pipe2 & pipe3

	//12 =2C = 6'b10_1100
	{0x040E, 0xAC, 0}, // DT for pipe0 & 1 10 10 1100
	{0x040F, 0xBC, 0}, // DT for pipe1 & 2 10 11 1100
	{0x0410, 0xB0, 0}, // DT for pipe2 & 3 10 11 00 00 bpp 12 = 5'b0_1100
	{0x0411, 0x6C, 0}, // bpp for pipe1 & 2 011 0 1100 bpp 12 = 5'b0_1100
	{0x0412, 0x30, 0}, // 0 01100 00 bpp for pipe1 & 2

	//e YU_10_MUX mode for pipe 0-3. It is not needed.
	//0x041A 0xF0

	//PHY ting
	//es ix4 mode
	{0x08A0, 0x04, 0},
	//ane ping for 4-lane port A and B
	{0x08A3, 0xE4, 0},
	{0x08A4, 0xE4, 0},

	// lan-PHY-$ lans setting
	{0x090A, 0xC0, 0},
	{0x094A, 0xC0, 0},
	{0x098A, 0xC0, 0},
	{0x09CA, 0xC0, 0},

	//on M PHYs
	{0x08A2, 0xF4, 0},

	//software override & MIPI data rate : 900Mbps
	{0x0415, 0xE9, 0},
	{0x0418, 0xE9, 0},

	//Pipo MIPI Controller Mapping
	// vi pipe 0  map FS/FE
	{0x090B, 0x07, 0},
	{0x092D, 0x15, 0}, // map to MIPI Controller 1  00 01 01 01
	{0x090D, 0x2C, 0}, // SRC Long packet. 00 10 1100
	{0x090E, 0x2C, 0}, // DST Long packet. Map to VC0. 00 10 1100
	{0x090F, 0x00, 0}, // SRC short packet.
	{0x0910, 0x00, 0}, // DST short packet. 00 00 0000
	{0x0911, 0x01, 0}, // SRC short packet.
	{0x0912, 0x01, 0}, // DST short packet.

	// videpipe 1  map FS/FE
	{0x094B, 0x07, 0},
	{0x096D, 0x15, 0}, // map to MIPI Controller 1  00 01 01 01
	{0x094D, 0x2C, 0}, // SRC Long packet. 00 10 1100
	{0x094E, 0x6C, 0}, // DST Long packet. Map to VC1. 01 10 1100
	{0x094F, 0x00, 0}, // SRC short packet.
	{0x0950, 0x40, 0}, // DST short packet. 01 00 0000
	{0x0951, 0x01, 0}, // SRC short packet.
	{0x0952, 0x41, 0}, // DST short packet.

	// RAW12 video pipe 2  map FS/FE
	{0x098B, 0x07, 0},
	{0x09AD, 0x15, 0}, // map to MIPI Controller 1  00 01 01 01
	{0x098D, 0x2C, 0}, // SRC Long packet. 00 10 1100
	{0x098E, 0xAC, 0}, // DST Long packet. Map to VC2. 10 10 1100
	{0x098F, 0x00, 0}, // SRC short packet.
	{0x0990, 0x80, 0}, // DST short packet. 10 00 0000
	{0x0991, 0x01, 0}, // SRC short packet.
	{0x0992, 0x81, 0}, // DST short packet.

	// vio pipe 3  map FS/FE
	{0x09CB, 0x07, 0},
	{0x09ED, 0x15, 0}, // map to MIPI Controller 1  00 01 01 01
	{0x09CD, 0x2C, 0}, // SRC Long packet. 00 10 1100
	{0x09CE, 0xEC, 0}, // DST Long packet. Map to VC3. 11 10 1100
	{0x09CF, 0x00, 0}, // SRC short packet.
	{0x09D0, 0xC0, 0}, // DST short packet. 11 00 0000
	{0x09D1, 0x01, 0}, // SRC short packet.
	{0x09D2, 0xC1, 0}, // DST short packet.


	//{0x0006,0x0F, 0}, // GMSL1 mode for all Links & All LINK enabled.
	{0x0006, MAX96712_LINK_MODE, 0},

	{0x0018, 0x0F, 100*1000},  //i MAX96712 one shot reset
};

const struct reg_sequence max96712_reg_s_stream[] = {
	/* Local ACK for pipe 0~3 DISEN */
	{0x0B0D, 0x00, 0},
	{0x0C0D, 0x00, 0},
	{0x0D0D, 0x00, 0},
	{0x0E0D, 0x00, 50*1000},
};

static const struct regmap_config max96712_regmap = {
	.reg_bits		= 16,
	.val_bits		= 8,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

/*
 * gpio fuctions
 */
int max96712_parse_device_tree(struct max96712 *dev, struct i2c_client *client)
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

void max96712_request_gpio(struct max96712 *dev)
{
	if (dev->gpio.pwr_port > 0) {
		/* request power */
		gpio_request(dev->gpio.pwr_port, "max96712 power");
	}
	if (dev->gpio.pwd_port > 0) {
		/* request power-down */
		gpio_request(dev->gpio.pwd_port, "max96712 power down");
	}
	if (dev->gpio.rst_port > 0) {
		/* request reset */
		gpio_request(dev->gpio.rst_port, "max96712 reset");
	}
	if (dev->gpio.intb_port > 0) {
		/* request intb */
		gpio_request(dev->gpio.intb_port, "max96712 interrupt");
	}
}

void max96712_free_gpio(struct max96712 *dev)
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
static inline struct max96712 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max96712, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max96712_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max96712		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		if (dev->fmt.code == MEDIA_BUS_FMT_SGRBG12_1X12) {
			logi("input format is bayer raw\n");
			ret = regmap_multi_reg_write(dev->regmap,
				max96712_reg_defaults_raw12,
				ARRAY_SIZE(max96712_reg_defaults_raw12));
		} else {
			logi("input format is yuv422\n");
			ret = regmap_multi_reg_write(dev->regmap,
				max96712_reg_defaults,
				ARRAY_SIZE(max96712_reg_defaults));
		}
		if (ret < 0) {
			/* failed to write i2c */
			loge("regmap_multi_reg_write returned %d\n", ret);
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

static int max96712_set_power(struct v4l2_subdev *sd, int on)
{
	struct max96712		*dev	= to_dev(sd);
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
		}
		dev->p_cnt++;
	} else {
		if (dev->p_cnt == 1) {
			/* power-down sequence */
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
static int max96712_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct max96712		*dev	= to_dev(sd);
	unsigned int		val	= 0;
	int			ret	= 0;

	/* TODO */
	mutex_lock(&dev->lock);

	/* check V4L2_IN_ST_NO_SIGNAL */
	/* ret = regmap_read(dev->regmap, MAX96712_REG_STATUS_1, &val); */
	if (ret < 0) {
		loge("failure to check V4L2_IN_ST_NO_SIGNAL\n");
	} else {
		logd("status: 0x%08x\n", val);
		/* if (val == MAX96712_VAL_STATUS_1) { */
		if (1) {
			*status &= ~V4L2_IN_ST_NO_SIGNAL;
		} else {
			logw("V4L2_IN_ST_NO_SIGNAL\n");
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static inline int max96712_set_fwdcc(struct v4l2_subdev *sd,
				     unsigned int fwdcc, int enable)
{
	struct max96712	*dev = to_dev(sd);
	unsigned int reg_val = 0;
	int ret = 0;

	if (enable) {
		/* Enable Forward Control Channel */
		reg_val = MAX96712_GMSL1_FWDCC_ENABLE;
	} else {
		/* Disable Forward Control Channel */
		reg_val = MAX96712_GMSL1_FWDCC_DISABLE;
	}

	ret = regmap_write(dev->regmap, fwdcc, reg_val);

	return ret;
}

static inline int max96712_set_all_fwdcc(struct v4l2_subdev *sd,
					 int enable)
{
	int ret = 0;

	ret = max96712_set_fwdcc(sd, MAX96712_REG_GMSL1_A_FWDCCEN, enable);
	if (ret < 0) {
		loge("Fail %s FWDCC of LINKE A\n",
			((enable == ON) ? "enable" : "disable"));
		goto e_fwdcc;
	}

	ret = max96712_set_fwdcc(sd, MAX96712_REG_GMSL1_B_FWDCCEN, enable);
	if (ret < 0) {
		loge("Fail %s FWDCC of LINKE B\n",
			((enable == ON) ? "enable" : "disable"));
		goto e_fwdcc;
	}

	ret = max96712_set_fwdcc(sd, MAX96712_REG_GMSL1_C_FWDCCEN, enable);
	if (ret < 0) {
		loge("Fail %s FWDCC of LINKE C\n",
			((enable == ON) ? "enable" : "disable"));
		goto e_fwdcc;
	}

	ret = max96712_set_fwdcc(sd, MAX96712_REG_GMSL1_D_FWDCCEN, enable);
	if (ret < 0) {
		loge("Fail %s FWDCC of LINKE D\n",
			((enable == ON) ? "enable" : "disable"));
		goto e_fwdcc;
	}

e_fwdcc:
	return ret;
}

/**
 * max96712_set_alias - Set alias of remote device's I2C slave address
 *
 * @sd: pointer to &struct v4l2_subdev
 * @target_fwdcc: target input link's FWDCC reg
 * @src: alias address
 * @dst: real I2C slave address
 *
 * I2C master -> (source addr) -> ... -> serializer -> (dest addr) -> device
 */
static int max96712_set_alias(struct v4l2_subdev *sd,
				unsigned int target_fwdcc,
				short src, short dst)
{
	struct max96712 *dev = to_dev(sd);
	struct i2c_client *client = 0;
	unsigned char buf[3] = {0,};
	unsigned short backup_addr = 0;
	int ret = 0;

	client = v4l2_get_subdevdata(sd);
	if (client == NULL) {
		ret = -ENODEV;
		loge("no i2c client info\n");
		goto end;
	}

	/* disable all FWDCC */
	ret = max96712_set_all_fwdcc(sd, OFF);
	if (ret < 0) {
		loge("Fail disable all FWDCC\n");
		goto e_fwdcc;
	}

	/* enable target FWDCC and change remote device's slave address */
	ret = max96712_set_fwdcc(sd, target_fwdcc, ON);
	if (ret < 0) {
		loge("Fail enable target FWDCC\n");
		goto e_fwdcc;
	}

	/* backup deserializer slave address */
	backup_addr = client->addr;

	/*
	 * Set alias.
	 * Src address is a alias.
	 * Dest address is a real slave address of remote device.
	 */
	client->addr = (MAX96712_SER_SLAVE_ADDR >> 1U);

	buf[0] = MAX96712_SER_REG_I2C_SOURC;
	buf[1] = src;

	ret = i2c_master_send(client, buf, 2);
	if (ret < 0) {
		loge("Fail setting source address");
		goto e_fwdcc;
	}

	buf[0] = MAX96712_SER_REG_I2C_DEST;
	buf[1] = dst;

	ret = i2c_master_send(client, buf, 2);
	if (ret < 0) {
		loge("Fail setting destination address");
		goto e_fwdcc;
	}

	/* restore deserializer slave addr disable target FWDCC */
	client->addr = backup_addr;
	ret = max96712_set_fwdcc(sd, target_fwdcc, OFF);
	if (ret < 0) {
		loge("Fail enable target FWDCC\n");
		goto e_fwdcc;
	}

e_fwdcc:
	client->addr = backup_addr;

	/* enable all FWDCC */
	ret = max96712_set_all_fwdcc(sd, ON);
	if (ret < 0) {
		loge("Fail enable all FWDCC\n");
		goto end;
	}

end:
	return ret;
}
/**
 * max96712_set_alias_remote_slave_addr - set alias of remote slave address
 *
 * @sd: pointer to &struct v4l2_subdev
 * @link_mode: link status showing which port is connected to the remote device
 *
 * MAX96712 has 4 input ports.
 * So 4 remote devices(serializer, sensor and etc...) can be connected.
 *
 * If each remote devices are the same, all the I2C salve address will be same.
 * So, the serializer supports the feature translating input I2C slave address.
 */
static int max96712_set_alias_remote_slave_addr(struct v4l2_subdev *sd,
						int link_mode)
{
	int ret = 0;

	if (link_mode & MAX96712_LINK_EN_A) {
		ret = max96712_set_alias(sd,
				MAX96712_REG_GMSL1_A_FWDCCEN,
				MAX96712_SENSOR_SLAVE_ADDR_ALIAS0,
				MAX96712_SENSOR_SLAVE_ADDR);
		if (ret < 0) {
			/* failure of changing remote device address */
			loge("Fail set alias of link a remote address\n");
			goto end;
		}

	}

	if (link_mode & MAX96712_LINK_EN_B) {
		ret = max96712_set_alias(sd,
				MAX96712_REG_GMSL1_B_FWDCCEN,
				MAX96712_SENSOR_SLAVE_ADDR_ALIAS1,
				MAX96712_SENSOR_SLAVE_ADDR);
		if (ret < 0) {
			/* failure of changing remote device address */
			loge("Fail set alias of link b remote address\n");
			goto end;
		}

	}

	if (link_mode & MAX96712_LINK_EN_C) {
		ret = max96712_set_alias(sd,
				MAX96712_REG_GMSL1_C_FWDCCEN,
				MAX96712_SENSOR_SLAVE_ADDR_ALIAS2,
				MAX96712_SENSOR_SLAVE_ADDR);
		if (ret < 0) {
			/* failure of changing remote device address */
			loge("Fail set alias of link c remote address\n");
			goto end;
		}

	}

	if (link_mode & MAX96712_LINK_EN_D) {
		ret = max96712_set_alias(sd,
				MAX96712_REG_GMSL1_D_FWDCCEN,
				MAX96712_SENSOR_SLAVE_ADDR_ALIAS3,
				MAX96712_SENSOR_SLAVE_ADDR);
		if (ret < 0) {
			/* failure of changing remote device address */
			loge("Fail set alias of link d remote address\n");
			goto end;
		}

	}

end:
	return ret;
}

static int max96712_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max96712		*dev	= to_dev(sd);
	unsigned int		reg_val = 0;
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->s_cnt == 0) && (enable == 1)) {
		ret = regmap_multi_reg_write(dev->regmap,
				max96712_reg_s_stream,
				ARRAY_SIZE(max96712_reg_s_stream));
		if (ret < 0) {
			/* failure of enabling output  */
			loge("Fail enable output of max96712 device\n");
		}
		if (dev->fmt.code == MEDIA_BUS_FMT_SGRBG12_1X12) {
			/* format is MEDIA_BUS_FMT_SGRBG12_1X12 */
			ret = max96712_set_alias_remote_slave_addr(sd,
					MAX96712_LINK_MODE);
			if (ret < 0) {
				/* failure of changing remote device address */
				loge("Fail set alias of remote address\n");
			}
			reg_val = 0x62;
		} else {
			/* format is not MEDIA_BUS_FMT_SGRBG12_1X12 */
			reg_val = 0x82;
		}
		ret = regmap_write(dev->regmap, 0x040B, reg_val);
		if (ret < 0) {
			/* failure of enabling output  */
			loge("Fail enable output of max96712 device\n");
		}
	} else if ((dev->s_cnt == 1) && (enable == 0)) {
		if (dev->fmt.code == MEDIA_BUS_FMT_SGRBG12_1X12)
			reg_val = 0x60;
		else
			reg_val = 0x80;
		ret = regmap_write(dev->regmap, 0x040B, reg_val);
		if (ret < 0) {
			/* failure of disabling output  */
			loge("Fail disable output of max96712 device\n");
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

static int max96712_get_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *format)
{
	struct max96712		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int max96712_set_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *format)
{
	struct max96712		*dev	= to_dev(sd);
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
static const struct v4l2_subdev_core_ops max96712_core_ops = {
	.init			= max96712_init,
	.s_power		= max96712_set_power,
};

static const struct v4l2_subdev_video_ops max96712_video_ops = {
	.g_input_status		= max96712_g_input_status,
	.s_stream		= max96712_s_stream,
};

static const struct v4l2_subdev_pad_ops max96712_pad_ops = {
	.get_fmt		= max96712_get_fmt,
	.set_fmt		= max96712_set_fmt,
};

static const struct v4l2_subdev_ops max96712_ops = {
	.core			= &max96712_core_ops,
	.video			= &max96712_video_ops,
	.pad			= &max96712_pad_ops,
};

struct max96712 max96712_data = {
};

static const struct i2c_device_id max96712_id[] = {
	{ "max96712", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max96712_id);

#if IS_ENABLED(CONFIG_OF)
const struct of_device_id max96712_of_match[] = {
	{
		.compatible	= "maxim,max96712",
		.data		= &max96712_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max96712_of_match);
#endif

int max96712_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max96712 *dev = NULL;
	const struct of_device_id *dev_id = NULL;
	int ret = 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct max96712), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max96712_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse the device tree */
	ret = max96712_parse_device_tree(dev, client);
	if (ret < 0) {
		loge("cannot initialize gpio port\n");
		return ret;
	}

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &max96712_ops);

	/* register a v4l2 sub device */
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	/* request gpio */
	max96712_request_gpio(dev);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &max96712_regmap);
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

int max96712_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max96712		*dev	= to_dev(sd);

	// release regmap
	regmap_exit(dev->regmap);

	/* gree gpio */
	max96712_free_gpio(dev);

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
