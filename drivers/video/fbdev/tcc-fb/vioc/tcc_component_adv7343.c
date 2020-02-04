/****************************************************************************
FileName    : kernel/drivers/video/fbdev/tcc-fb/vioc/tcc_component_adv7343.c
Description :

Copyright (C) 2016 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <asm/io.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include "tcc_component.h"

//#define ADV7343_DBG
#ifdef ADV7343_DBG
#define dprintk(fmt, args...)                                                  \
	printk("\e[33m[DBG][ADV7343][%s:%d] \e[0m" fmt, __func__, __LINE__, ##args);
#else
#define dprintk(fmt, args...)
#endif

#define STD_INFO_END 0xFF
#define STD_INFO_DELAY 0xFE

static struct i2c_client *adv7343_i2c_client = NULL;
static const struct i2c_device_id adv7343_i2c_id[] = {
	{
		"component-adv7343",
		0,
	},
	{}};


// static struct adv7343_std_info adv7343_test_pattern[] =
//{	/* Test pattern generation */
//	//{0x02, 0x14},	// Test pattern black bar for RGB
//	//{0x02, 0x24},	// Test pattern black bar for YCbCr
//
//	{0x31, 0x05},	// Test pattern on with Hatch.
//	//{0x31, 0x0D},	// Test pattern on with Field/frame.
//
//	/* red (output blue) */
//	{0x36, 76},		// Y
//	{0x37, 84},		// Cb
//	{0x38, 255},	// Cr
//	/* green (output green) */
//	//{0x36, 149},
//	//{0x37, 43},
//	//{0x38, 21},
//	/* blue (output red) */
//	//{0x36, 29},
//	//{0x37, 255},
//	//{0x38, 107},
//
//	{STD_INFO_END, STD_INFO_END}
//};


/*
 * 24bit RGB Input, RGB Output
 * - need RGB output port
 * - N/A
 */
static struct adv7343_std_info adv7343_720p_24bitRGB_to_RGB[] =
	{/* Table 124. 24-Bit 720p RGB In, RGB Out */
	 {0x17, 0x02},
	 {0x00, 0x1C},
	 {0x01, 0x10},
	 {0x02, 0x10}, // RGB output
	 {0x30, 0x28},
	 {0x31, 0x01},
	 {0x33, 0x28},
	 {0x35, 0x02}, // RGB input
	 {STD_INFO_END, STD_INFO_END}};

static struct adv7343_std_info adv7343_1080i_24bitRGB_to_RGB[] =
	{/* Table 137. 24-Bit 1080i RGB In, RGB Out */
	 {0x17, 0x02},
	 {0x00, 0x1C},
	 {0x01, 0x10},
	 //{0x02, 0x10},	// RGB output (comment-out is YPrPb output)
	 {0x30, 0x68},
	 {0x31, 0x01},
	 {0x33, 0x28},
	 {0x35, 0x02}, // RGB input & DAC2=Pb DAC3=Pr (0x0A is DAC2=Pr DAC3=Pb)
	 {STD_INFO_END, STD_INFO_END}};

/*
 * 24bit YCrCb Input, YPrPb Output
 * - need 24bit (fake) YCrCb Input format (refer to lcdc.c::lcdc_io_init_component)
 * - Test only
 */
static struct adv7343_std_info adv7343_720p_24bitYCrCb_to_YPrPb[] =
	{/* Table 121. 24-Bit 720p YCrCb In, YPrPb Out */
	 {0x17, 0x02},
	 {0x00, 0x1C},
	 {0x01, 0x10},
	 {0x30, 0x28},
	 {0x31, 0x01},
	 {0x33, 0x28},
	 {STD_INFO_END, STD_INFO_END}};

static struct adv7343_std_info adv7343_1080i_24bitYCrCb_to_YPrPb[] =
	{/* Table 134. 24-Bit 1080i YCrCb In, YPrPb Out */
	 {0x17, 0x02},
	 {0x00, 0x1C},
	 {0x01, 0x10},
	 {0x30, 0x68},
	 {0x31, 0x01},
	 {0x33, 0x28},
	 {STD_INFO_END, STD_INFO_END}};

/*
 * 16bit YCrCb Input, YPrPb Output
 */
static struct adv7343_std_info adv7343_720p_16bitYCrCb_to_YPrPb[] =
	{/* Table 117. 16-Bit 720p YCrCb In, YPrPb Out */
	 {0x17, 0x02}, {0x00, 0x1C}, {0x01, 0x10},
	 {0x30, 0x28}, {0x31, 0x01}, {STD_INFO_END, STD_INFO_END}};

static struct adv7343_std_info adv7343_1080i_16bitYCrCb_to_YPrPb[] =
	{/* Table 130. 16-Bit 1080i YCrCb In, YPrPb Out */
	 {0x17, 0x02},
	 {0x00, 0x1C},
	 {0x01, 0x10},
	 {0x30, 0x68},
	 {0x31, 0x01},
	 {0x35, 0x08}, // RGB input disable & (DAC swap) DAC2=Pr DAC3=Pb for TCC8980_STB
	 {STD_INFO_END, STD_INFO_END}};

static int adv7343_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;

	/*
	 * adv7343 slave address
	 * if ALSB=1 then Write=0x56, Read=0x57
	 */
	of_property_read_u32(np, "reg", (u32 *)&client->addr);
	dprintk("addr=0x%02x\n", client->addr);

	adv7343_i2c_client = client;
	if (adv7343_i2c_client == NULL)
		return -ENODEV;

	return 0;
}

static int adv7343_i2c_remove(struct i2c_client *client)
{
	dprintk("\n");
	adv7343_i2c_client = NULL;
	return 0;
}

static int adv7343_i2c_write(unsigned char reg, unsigned char val)
{
	unsigned char bytes;
	unsigned char data[2];
	int ret;

	bytes = 2;
	data[0] = reg;
	data[1] = val;

	ret = i2c_master_send(adv7343_i2c_client, data, bytes);
	if (ret != bytes) {
		pr_err("[ERR][ADV7343] %s failed(%d). %s\n", __func__, ret,
		       adv7343_i2c_client == NULL ? "-ENODEV" : "");
		return -EIO;
	}
	dprintk("reg=0x%02x, val=0x%02x, addr=0x%02x\n", reg, val,
		adv7343_i2c_client->addr);

	return ret;
}

int adv7343_i2c_read(unsigned char reg, unsigned char *val)
{
	unsigned char bytes;

	if (adv7343_i2c_client == NULL)
		return -ENODEV;

	bytes = 1;
	if (i2c_master_send(adv7343_i2c_client, &reg, bytes) != bytes) {
		pr_err("[ERR][ADV7343] %s: read(w) failed.\n", __func__);
		return -EIO;
	}

	bytes = 1;
	if (i2c_master_recv(adv7343_i2c_client, val, bytes) != bytes) {
		pr_err("[ERR][ADV7343] %s: read(r) failed.\n", __func__);
		return -EIO;
	}

	return 0;
}

// void adv7343_reset(void)
//{
//}

// static int adv7343_soft_reset(int mode)
//{
//	int err = 0;
//	return err;
//}

void component_chip_set_cgms(unsigned int enable, unsigned int data)
{
	unsigned char cgms_header, cgms_payload_msb, cgms_payload_lsb;

	if (enable) {
		cgms_header = (unsigned char)(ADV7343_SD_CGMS_WSS0_ENABLE | ((data & 0xf0000) >> 16));
		cgms_payload_msb = (unsigned char)(((data & 0xff00) >> 8));
		cgms_payload_lsb = (unsigned char)((data & 0xff));
	} else {
		cgms_header = ADV7343_SD_CGMS_WSS0_DEFAULT;
		cgms_payload_msb = ADV7343_SD_CGMS_WSS1_DEFAULT;
		cgms_payload_lsb = ADV7343_SD_CGMS_WSS2_DEFAULT;
	}

	adv7343_i2c_write(ADV7343_SD_CGMS_WSS0, cgms_header);
	adv7343_i2c_write(ADV7343_SD_CGMS_WSS1, cgms_payload_msb);
	adv7343_i2c_write(ADV7343_SD_CGMS_WSS2, cgms_payload_lsb);

	dprintk("%s: CGMS %s, 0x%05x (header 0x%02x, payload_msb 0x%02x, payload_lsb 0x%02x\n", __func__,
		enable ? "on" : "off", data, cgms_header, cgms_payload_msb, cgms_payload_lsb);
}

void component_chip_get_cgms(unsigned int *enable, unsigned int *data)
{
	unsigned char cgms_header, cgms_payload_msb, cgms_payload_lsb;

	adv7343_i2c_read(ADV7343_SD_CGMS_WSS0, &cgms_header);
	adv7343_i2c_read(ADV7343_SD_CGMS_WSS1, &cgms_payload_msb);
	adv7343_i2c_read(ADV7343_SD_CGMS_WSS2, &cgms_payload_lsb);

	*enable = !!(cgms_header & 0x60);
	*data = ((cgms_header & 0xf) << 16)
		 | (cgms_payload_msb << 8)
		 | (cgms_payload_lsb);

	dprintk("%s: CGMS %s, 0x%05x (header 0x%02x, payload_msb 0x%02x, payload_lsb 0x%02x\n", __func__,
		*enable ? "on" : "off", *data, cgms_header, cgms_payload_msb, cgms_payload_lsb);
}

static int adv7343_set_mode(int mode, int input, int starter_flag)
{
	int err = 0;
	unsigned char reg, val;
	static struct adv7343_std_info *std_info;

	dprintk("mode(%d) input(%d) starter_flag(%d)\n", mode, input,
		starter_flag);

	switch (mode) {
	case COMPONENT_MODE_1080I:
		if (input == OUTPUT_FORMAT_YCbCr_16bit)
			std_info = adv7343_1080i_16bitYCrCb_to_YPrPb;
		else if (input == OUTPUT_FORMAT_YCbCr_24bit)
			std_info = adv7343_1080i_24bitYCrCb_to_YPrPb;
		else if (input == OUTPUT_FORMAT_RGB888)
			std_info = adv7343_1080i_24bitRGB_to_RGB;
		break;
	case COMPONENT_MODE_720P:
		if (input == OUTPUT_FORMAT_YCbCr_16bit)
			std_info = adv7343_720p_16bitYCrCb_to_YPrPb;
		else if (input == OUTPUT_FORMAT_YCbCr_24bit)
			std_info = adv7343_720p_24bitYCrCb_to_YPrPb;
		else if (input == OUTPUT_FORMAT_RGB888)
			std_info = adv7343_720p_24bitRGB_to_RGB;
		break;
	default:
		break;
	}

	do {
		reg = std_info->reg;
		val = std_info->val;

		if (STD_INFO_DELAY == reg)
			mdelay(val);
		else
			err = adv7343_i2c_write(reg, val);

		if (err < 0) {
			pr_err("[ERR][ADV7343] %s: Set mode i2c write error\n", __func__);
			break;
		}

		#ifdef ADV7343_DBG
		 if (STD_INFO_DELAY == reg) {
			dprintk("delay(%d)\n", val);
		} else {
			unsigned char r;
			adv7343_i2c_read(reg, &r);
			dprintk("0x%02x(0x%02x, 0x%02x)\n", reg, val, r);
		}
		#endif

		std_info++;
	} while (STD_INFO_END != std_info->reg);

	return err;
}

void adv7343_enable(int mode, unsigned int output_format, int starter_flag)
{
	dprintk("mode(%d) starter_flag(%d)\n", mode, starter_flag);

	/*
	 * Reset the device:
	 * If ALSB=1 then the ADV7343 include a power-on-reset circuit
	 * to ensure correct operation after power-up
	 */

	/* Set the mode */
	adv7343_set_mode(mode, output_format, starter_flag);
}

#ifdef CONFIG_OF
static const struct of_device_id adv7343_of_match[] = {
	{
		.compatible = "component-adv7343-i2c",
	},
	{},
};
MODULE_DEVICE_TABLE(of, adv7343_of_match);
#endif

static struct i2c_driver adv7343_i2c_driver = {
	.probe = adv7343_i2c_probe,
	.remove = adv7343_i2c_remove,
	.id_table = adv7343_i2c_id,
	.driver =
		{
			.name = "component-adv7343",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(adv7343_of_match),
#endif
		},
};

static int adv7343_init(void)
{
	dprintk("\n");
	return i2c_add_driver(&adv7343_i2c_driver);
}

static void adv7343_exit(void)
{
	i2c_del_driver(&adv7343_i2c_driver);
}

module_init(adv7343_init);
module_exit(adv7343_exit);

MODULE_DESCRIPTION("ADV7343 Driver");
MODULE_LICENSE("GPL");
