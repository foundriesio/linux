/****************************************************************************
FileName    : kernel/drivers/video/tcc/vioc/tcc_component_ths8200.c
Description :

Copyright (C) 2013 Telechips Inc.

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

//#include <linux/i2c/ths8200.h>
#include "tcc_component.h"
#include "tcc_component_ths8200.h"

/* Debugging stuff */
static int debug = 0;

#define dprintk(msg...)                                                        \
	if (debug) {                                                           \
		printk("[DBG][ths8200] " msg);                                       \
	}

static struct ths8200_std_info ths8200_video_720p_info[] = {
	/*720p*/
	{THS8200_DTG2_CNTL, THS8200_DTG2_CNTL_720P_DEFAULT},
	{THS8200_DTG1_SPEC_A, THS8200_DTG1_SPEC_A_720P_DEFAULT},
	{THS8200_DTG1_SPEC_B, THS8200_DTG1_SPEC_B_720P_DEFAULT},
	{THS8200_DTG1_SPEC_C, THS8200_DTG1_SPEC_C_720P_DEFAULT},
	{THS8200_DTG1_SPEC_D_LSB, THS8200_DTG1_SPEC_D_LSB_720P_DEFAULT},
	{THS8200_DTG1_SPEC_E_LSB, THS8200_DTG1_SPEC_E_LSB_720P_DEFAULT},
	{THS8200_DTG1_SPEC_DEH_MSB, THS8200_DTG1_SPEC_DEH_MSB_720P_DEFAULT},
	{THS8200_DTG1_SPEC_K_LSB, THS8200_DTG1_SPEC_K_LSB_720P_DEFAULT},
	{THS8200_DTG1_TOT_PIXELS_MSB, THS8200_DTG1_TOT_PIXELS_MSB_720P_DEFAULT},
	{THS8200_DTG1_TOT_PIXELS_LSB, THS8200_DTG1_TOT_PIXELS_LSB_720P_DEFAULT},
	{THS8200_DTG1_MODE, THS8200_DTG1_MODE_720P_DEFAULT},
	{THS8200_DTG1_FRAME_FIELD_SZ_MSB,
	 THS8200_DTG1_FRAME_FIELD_SZ_MSB_720P_DEFAULT},
	{THS8200_DTG1_FRAME_SZ_LSB, THS8200_DTG1_FRAME_SZ_LSB_720P_DEFAULT},
	{THS8200_DTG1_FIELD_SZ_LSB, THS8200_DTG1_FIELD_SZ_LSB_720P_DEFAULT},
	{THS8200_DTG2_HS_IN_DLY_LSB, THS8200_DTG2_HS_IN_DLY_LSB_720P_DEFAULT},
	{THS8200_DTG2_VS_IN_DLY_MSB, THS8200_DTG2_VS_IN_DLY_MSB_720P_DEFAULT},
	{THS8200_DTG2_VS_IN_DLY_LSB, THS8200_DTG2_VS_IN_DLY_LSB_720P_DEFAULT},
	{0, 0},
};

static struct ths8200_std_info ths8200_video_1080i_info[] =
	{/*1080I*/
	 {THS8200_TST_CNTL1, THS8200_TST_CNTL1_1080I_DEFAULT},
	 {THS8200_TST_CNTL2, THS8200_TST_CNTL2_1080I_DEFAULT},
	 {THS8200_CSM_GY_CNTL_MULT_MSB,
	  THS8200_CSM_GY_CNTL_MULT_MSB_1080I_DEFAULT},
	 {THS8200_DTG2_CNTL, THS8200_DTG2_CNTL_1080I_DEFAULT},
	 {THS8200_DTG1_SPEC_A, THS8200_DTG1_SPEC_A_1080I_DEFAULT},
	 {THS8200_DTG1_SPEC_B, THS8200_DTG1_SPEC_B_1080I_DEFAULT},
	 {THS8200_DTG1_SPEC_C, THS8200_DTG1_SPEC_C_1080I_DEFAULT},
	 {THS8200_DTG1_SPEC_D1, THS8200_DTG1_SPEC_D1_1080I_DEFAULT},
	 {THS8200_DTG1_SPEC_D_LSB, THS8200_DTG1_SPEC_D_LSB_1080I_DEFAULT},
	 {THS8200_DTG1_SPEC_E_LSB, THS8200_DTG1_SPEC_E_LSB_1080I_DEFAULT},
	 {THS8200_DTG1_SPEC_DEH_MSB, THS8200_DTG1_SPEC_DEH_MSB_1080I_DEFAULT},
	 {THS8200_DTG1_SPEC_K_LSB, THS8200_DTG1_SPEC_K_LSB_1080I_DEFAULT},
	 {THS8200_DTG1_SPEC_G_LSB, THS8200_DTG1_SPEC_G_LSB_1080I_DEFAULT},
	 {THS8200_DTG1_SPEC_G_MSB, THS8200_DTG1_SPEC_G_MSB_1080I_DEFAULT},
	 {THS8200_DTG1_TOT_PIXELS_MSB,
	  THS8200_DTG1_TOT_PIXELS_MSB_1080I_DEFAULT},
	 {THS8200_DTG1_TOT_PIXELS_LSB,
	  THS8200_DTG1_TOT_PIXELS_LSB_1080I_DEFAULT},
	 {THS8200_DTG1_MODE, THS8200_DTG1_MODE_1080I_DEFAULT},
	 {THS8200_DTG1_FRAME_FIELD_SZ_MSB,
	  THS8200_DTG1_FRAME_FIELD_SZ_MSB_1080I_DEFAULT},
	 {THS8200_DTG1_FRAME_SZ_LSB, THS8200_DTG1_FRAME_SZ_LSB_1080I_DEFAULT},
	 {THS8200_DTG1_FIELD_SZ_LSB, THS8200_DTG1_FIELD_SZ_LSB_1080I_DEFAULT},
	 {THS8200_DTG2_HLENGTH_LSB, THS8200_DTG2_HLENGTH_LSB_1080I_DEFAULT},
	 {THS8200_DTG2_HLENGTH_LSB_HDLY_MSB,
	  THS8200_DTG2_HLENGTH_LSB_HDLY_MSB_1080I_DEFAULT},
	 {THS8200_DTG2_HLENGTH_HDLY_LSB,
	  THS8200_DTG2_HLENGTH_HDLY_LSB_1080I_DEFAULT},
	 {THS8200_DTG2_VLENGTH1_LSB, THS8200_DTG2_VLENGTH1_LSB_1080I_DEFAULT},
	 {THS8200_DTG2_VLENGTH1_MSB_VDLY1_MSB,
	  THS8200_DTG2_VLENGTH1_MSB_VDLY1_MSB_1080I_DEFAULT},
	 {THS8200_DTG2_VDLY1_LSB, THS8200_DTG2_VDLY1_LSB_1080I_DEFAULT},
	 {THS8200_DTG2_VLENGTH2_LSB, THS8200_DTG2_VLENGTH2_LSB_1080I_DEFAULT},
	 {THS8200_DTG2_VDLY2_LSB, THS8200_DTG2_VDLY2_LSB_1080I_DEFAULT},
	 {THS8200_DTG2_VLENGTH2_MSB_VDLY2_MSB,
	  THS8200_DTG2_VLENGTH2_MSB_VDLY2_MSB_1080I_DEFAULT},
	 {THS8200_DTG2_VDLY1_LSB, THS8200_DTG2_VDLY1_LSB_1080I_DEFAULT},
	 {THS8200_DTG2_HS_IN_DLY_LSB, THS8200_DTG2_HS_IN_DLY_LSB_1080I_DEFAULT},
	 {THS8200_DTG2_VS_IN_DLY_MSB, THS8200_DTG2_VS_IN_DLY_MSB_1080I_DEFAULT},
	 {THS8200_DTG2_VS_IN_DLY_LSB, THS8200_DTG2_VS_IN_DLY_LSB_1080I_DEFAULT},
	 {0, 0}};

static int ths8200_reset_port;
static int ths8200_field_port;

static struct i2c_client *ths8200_i2c_client = NULL;

static const struct i2c_device_id ths8200_i2c_id[] = {
	{
		"component-ths8200",
		0,
	},
	{}};

static int ths8200_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;

	dprintk("%s, addr=0x%02x\n", __func__, client->addr);

	of_property_read_u32(np, "reg", (u32 *)&client->addr);

	if (np) {
		ths8200_reset_port = of_get_named_gpio(np, "rst-gpios", 0);
		if (gpio_is_valid(ths8200_reset_port)) {
			pr_info("[INF][ths8200] %s, reset port: 0x%02x\n", __func__,
			       ths8200_reset_port);
			gpio_request(ths8200_reset_port, "ths8200-reset");
			gpio_direction_output(ths8200_reset_port, 0);
		} else {
			pr_err("[ERR][ths8200] %s, err to get rst-gpios\n", __func__);
			ths8200_reset_port = -1;
		}

		ths8200_field_port = of_get_named_gpio(np, "field-gpios", 0);
		if (gpio_is_valid(ths8200_field_port)) {
			pr_debug("[DBG][ths8200] %s, field port: 0x%02x\n", __func__,
			       ths8200_field_port);
			gpio_request(ths8200_field_port, "ths8200-field");
		} else {
			pr_err("[ERR][ths8200] %s, err to get field-gpios\n", __func__);
			ths8200_field_port = -1;
		}
	}

	ths8200_i2c_client = client;

	return 0;
}

static int ths8200_i2c_remove(struct i2c_client *client)
{
	dprintk("%s\n", __func__);

	ths8200_i2c_client = NULL;

	return 0;
}

int ths8200_i2c_write(unsigned char reg, unsigned char val)
{
	unsigned char data[2];
	unsigned char bytes;

	data[0] = reg;
	data[1] = val;

	bytes = 2;

	if (ths8200_i2c_client == NULL)
		return ENODEV;

	if (i2c_master_send(ths8200_i2c_client, data, bytes) != bytes) {
		dprintk("%s, error!!!!\n", __func__);
		return -EIO;
	}

	// dprintk("ths8200_i2c_write success!!!! \n");

	return 0;
}

int ths8200_i2c_read(unsigned char reg, unsigned char *val)
{
	unsigned char bytes;

	if (ths8200_i2c_client == NULL)
		return ENODEV;

	bytes = 1;
	if (i2c_master_send(ths8200_i2c_client, &reg, bytes) != bytes) {
		dprintk("%s, read(w) error!!!!\n", __func__);
		return -EIO;
	}

	bytes = 1;
	if (i2c_master_recv(ths8200_i2c_client, val, bytes) != bytes) {
		dprintk("%s, read(r) error!!!!\n", __func__);
		return -EIO;
	}

	dprintk("%s, success!!!!\n", __func__);

	return 0;
}

void ths8200_reset(void)
{
	dprintk("%s\n", __func__);

	/* set field port */
	gpio_direction_output(ths8200_field_port, 1);
	mdelay(10);

	/* set reset port */
	gpio_direction_output(ths8200_reset_port, 1);
	mdelay(10);
	gpio_direction_output(ths8200_reset_port, 0);
	mdelay(10);
	gpio_direction_output(ths8200_reset_port, 1);
}

static int ths8200_soft_reset(int mode)
{
	int err = 0;
	unsigned char val;

	err |= ths8200_i2c_read(THS8200_CHIP_CTL, &val);

	dprintk("%s\n", __func__);

	/* reset consists of toggling the reset bit from low to high */

	val &= 0xfe;
	err |= ths8200_i2c_write(THS8200_CHIP_CTL, val);

#if 0
	if(mode == COMPONENT_MODE_1080I)
		val |= 0x11;
	else
#endif
	val |= 0x1;

	err |= ths8200_i2c_write(THS8200_CHIP_CTL, val);

	return err;
}

void ths8200_set_mode(int mode, int starter_flag)
{
	int err = 0;
	unsigned char reg, val;
	static struct ths8200_std_info *std_info;

	dprintk("%s : mode=%d\n", __func__, mode);

	switch (mode) {
	case COMPONENT_MODE_1080I:
		std_info = ths8200_video_1080i_info;
		break;
	case COMPONENT_MODE_720P:
		std_info = ths8200_video_720p_info;
		break;
	default:
		std_info = ths8200_video_720p_info;
		break;
	}

	ths8200_soft_reset(mode);

#if 0
    for (i = THS8200_CSC_R11; i <= THS8200_CSC_OFFS3; i++) {
	    /* reset color space conversion registers */
	    err |= ths8200_i2c_write(i, 0x0);
    }

    /* CSC bypassed and Under overflow protection ON */
    err |= ths8200_i2c_write(THS8200_CSC_OFFS3, ((THS8200_CSC_BYPASS << THS8200_CSC_BYPASS_SHIFT) | THS8200_CSC_UOF_CNTL));

	/* 20bit YCbCr 4:2:2 input data format */
    err |= ths8200_i2c_write(THS8200_DATA_CNTL,THS8200_DATA_CNTL_MODE_20BIT_YCBCR);
#else
#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV)
	if (starter_flag == 0) {
		int i;

		for (i = THS8200_CSC_R11; i <= THS8200_CSC_OFFS3; i++) {
			/* reset color space conversion registers */
			err |= ths8200_i2c_write(i, 0x0);
		}

		/* CSC bypassed and Under overflow protection ON */
		err |= ths8200_i2c_write(
			THS8200_CSC_OFFS3,
			((THS8200_CSC_BYPASS << THS8200_CSC_BYPASS_SHIFT) |
			 THS8200_CSC_UOF_CNTL));
	} else
#endif
	{
		/* set color space conversion registers */
		err |= ths8200_i2c_write(THS8200_CSC_R11, 0x00);
		err |= ths8200_i2c_write(THS8200_CSC_R12, 0xDA);
		err |= ths8200_i2c_write(THS8200_CSC_R21, 0x80);
		err |= ths8200_i2c_write(THS8200_CSC_R22, 0x78);
		err |= ths8200_i2c_write(THS8200_CSC_R31, 0x02);
		err |= ths8200_i2c_write(THS8200_CSC_R32, 0x0C);
		err |= ths8200_i2c_write(THS8200_CSC_G11, 0x02);
		err |= ths8200_i2c_write(THS8200_CSC_G12, 0xDC);
		err |= ths8200_i2c_write(THS8200_CSC_G21, 0x81);
		err |= ths8200_i2c_write(THS8200_CSC_G22, 0x94);
		err |= ths8200_i2c_write(THS8200_CSC_G31, 0x81);
		err |= ths8200_i2c_write(THS8200_CSC_G32, 0xDC);
		err |= ths8200_i2c_write(THS8200_CSC_B11, 0x00);
		err |= ths8200_i2c_write(THS8200_CSC_B12, 0x4A);
		err |= ths8200_i2c_write(THS8200_CSC_B21, 0x02);
		err |= ths8200_i2c_write(THS8200_CSC_B22, 0x0C);
		err |= ths8200_i2c_write(THS8200_CSC_B31, 0x80);
		err |= ths8200_i2c_write(THS8200_CSC_B32, 0x30);
		err |= ths8200_i2c_write(THS8200_CSC_OFFS1, 0x00);
		err |= ths8200_i2c_write(THS8200_CSC_OFFS12, 0x08);
		err |= ths8200_i2c_write(THS8200_CSC_OFFS23, 0x02);
		err |= ths8200_i2c_write(THS8200_CSC_OFFS3, 0x00);
	}

	/* 30bit RGB 4:4:4 input data format */
	err |= ths8200_i2c_write(THS8200_DATA_CNTL,
				 THS8200_DATA_CNTL_MODE_30BIT_YCBCR);
#endif

	err |= ths8200_i2c_write(THS8200_DTG1_Y_SYNC1_LSB,
				 THS8200_DTG1_CBCR_SYNC1_LSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_Y_SYNC2_LSB,
				 THS8200_DTG1_Y_SYNC2_LSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_Y_SYNC3_LSB,
				 THS8200_DTG1_Y_SYNC3_LSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_CBCR_SYNC1_LSB,
				 THS8200_DTG1_CBCR_SYNC1_LSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_CBCR_SYNC2_LSB,
				 THS8200_DTG1_CBCR_SYNC2_LSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_CBCR_SYNC3_LSB,
				 THS8200_DTG1_CBCR_SYNC3_LSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_Y_SYNC_MSB,
				 THS8200_DTG1_Y_SYNC_MSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_CBCR_SYNC_MSB,
				 THS8200_DTG1_CBCR_SYNC_MSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_SPEC_H_LSB,
				 THS8200_DTG1_SPEC_H_LSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_SPEC_K_MSB,
				 THS8200_DTG1_SPEC_K_MSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_FLD_FLIP_LINECNT_MSB,
				 THS8200_DTG1_FLD_FLIP_LINECNT_MSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG1_LINECNT_LSB,
				 THS8200_DTG1_LINECNT_LSB_DEFAULT);
	err |= ths8200_i2c_write(THS8200_DTG2_HS_IN_DLY_MSB,
				 THS8200_DTG2_HS_IN_DLY_MSB_DEFAULT);

	do {
		reg = std_info->reg;
		val = std_info->val;

		err |= ths8200_i2c_write(reg, val);

		if (err < 0) {
			dprintk("Set mode i2c write error\n");
			break;
		}

		std_info++;
	} while (std_info->reg);

	if (err < 0) {
		dprintk("Set standard failed\n");
		return;
	}

	ths8200_soft_reset(mode);

	/* this is for testing */
	{
		int i = 0;

		for (i = 0; i < 0x90; i++) {
			err = ths8200_i2c_read(i, &val);

			if (i % 8 == 0)
				dprintk("\n0x%02x : ", i);
			dprintk("0x%02x ", val);
		}
		dprintk("\n\n");
	}

	return;
}

void ths8200_enable(int mode, int stater_flag)
{
	dprintk("ths8200_enable() : type=%d, stater_flag=%d\n", mode,
		stater_flag);

	ths8200_reset();
	ths8200_set_mode(mode, stater_flag);
}

#ifdef CONFIG_OF
static const struct of_device_id ths8200_of_match[] = {
	{
		.compatible = "ti,ths8200",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ths8200_of_match);
#endif

static struct i2c_driver ths8200_i2c_driver = {
	.probe = ths8200_i2c_probe,
	.remove = ths8200_i2c_remove,
	.id_table = ths8200_i2c_id,
	.driver =
		{
			.name = "component-ths8200",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = ths8200_of_match,
#endif
		},
};

static int ths8200_init(void)
{
	return i2c_add_driver(&ths8200_i2c_driver);
}

static void ths8200_exit(void)
{
	i2c_del_driver(&ths8200_i2c_driver);
}

module_init(ths8200_init);
module_exit(ths8200_exit);

MODULE_DESCRIPTION("TI THS8200 Driver");
MODULE_LICENSE("GPL");
