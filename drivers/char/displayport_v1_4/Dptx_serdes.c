// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#include "Dptx_v14.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"

#define SER_DES_I2C_REG_ADD_LEN			2
#define SER_DES_I2C_DATA_LEN			1

#define DP0_PANEL_SER_I2C_DEV_ADD		0xC0	/* 0xC0 >> 1 = 0x60 */
#define DP0_PANEL_DES_I2C_DEV_ADD		0x90	/* 0x90 >> 1 = 0x48 */
#define DP1_PANEL_DES_I2C_DEV_ADD		0x94	/* 0x94 >> 1 = 0x4A */
#define DP2_PANEL_DES_I2C_DEV_ADD		0x98	/* 0x98 >> 1 = 0x4C */
#define DP3_PANEL_DES_I2C_DEV_ADD		0xD0	/* 0xD0 >> 1 = 0x68 */

#define	SER_DEV_REV						0x000E
#define SER_REV_ES2						0x01
#define SER_REV_ES4						0x03
#define SER_REV_ALL						0x0F

#define	SER_MISC_CONFIG_B1				0x7019
#define MST_FUNCTION_DISABLE		    0x00
#define MST_FUNCTION_ENABLE		   	 	0x01

#define	SER_LANE_REMAP_B0				0x7030
#define	SER_LANE_REMAP_B1				0x7031

#define	DES_DEV_REV						0x000E
#define DES_REV_ES2						0x01
#define DES_REV_ES3						0x02
#define DES_STREAM_SELECT				0x00A0
#define DES_DROP_VIDEO					0x0307

#define	DES_VIDEO_RX8					0x0108
#define DES_VID_LOCK					0x40
#define DES_VID_PKT_DET					0x20

#define TCC8059_EVB_TYPE				0
#define TCC8050_EVB_TYPE				1
#define TCC_ALL_EVB_TYPE				0x0F

#define DP_SER_DES_DELAY_DEV_ADDR			0xEFFF
#define DP_SER_DES_INVALID_REG_ADDR			0xFFFF


struct Max968xx_dev {
	bool				bActivated;
	bool				bDes_VidLocked;
	bool				bSer_LaneSwap;
	unsigned char		ucSER_Revision_Num;
	unsigned char		ucDES_Revision_Num;
	unsigned char		ucEVB_Type;
    struct i2c_client	*pstClient;
};

struct DP_V14_SER_DES_Reg_Data {
    unsigned int					uiDev_Addr;
    unsigned int					uiReg_Addr;
    unsigned int					uiReg_Val;
    unsigned char					ucDeviceType;
    unsigned char					ucSER_Revision;
};


static struct Max968xx_dev	stMax968xx_dev[SER_DES_INPUT_INDEX_MAX] = {0, };

static struct DP_V14_SER_DES_Reg_Data pstDP_Panel_VIC_1027_DesES3_RegVal[] = {
{0xD0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 10, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  10ms delay	 */

/* AGC CR Init 8G1 */
	{0xC0, 0x60AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* BST CR Init 8G1 */
	{0xC0, 0x60B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* AGC CR Init 5G4 */
	{0xC0, 0x60A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* BST CR Init 5G4 */
	{0xC0, 0x60B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* AGC CR Init 2G7 */
	{0xC0, 0x60A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* BST CR Init 2G7 */
	{0xC0, 0x60B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* Set 8G1 Error Channel Phase */
	{0xC0, 0x6070, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6071, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6170, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6171, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6270, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6271, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6370, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6371, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},

/**********************/
/*  MST Setting      */
/**********************/
	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Turn off video-GM03 */
	{0xC0, 0x7A14, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Disable MST_VS0_DTG_ENABLE */
	{0xC0, 0x7000, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Disable LINK_ENABLE */
	{0xC0, SER_MISC_CONFIG_B1, MST_FUNCTION_ENABLE, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Enable MST */

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 100, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  100ms delay	 */

	{0xC0, 0x70A0, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Set AUX_RD_INTERVAL to 16ms */
	{0xC0, 0x7074, 0x1E, TCC_ALL_EVB_TYPE, SER_REV_ES4}, /* Max rate : 1E -> 8.1Gbps, 14 -> 5.4Gbps, 0A -> 2.7Gbps */
	{0xC0, 0x7074, 0x0A, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x7070, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Max lane count to 4 */
	
	{0xC0, SER_LANE_REMAP_B0, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},{0xC0, SER_LANE_REMAP_B1, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},	// Swap = 0 <-> 2, 1 <-> 3
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 1, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  1ms delay 	 */

	{0xC0, 0x7000, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Enable LINK_ENABLE */

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 50, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  50ms delay	 */

	{0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Enable MST_VS0_DTG_ENABLE */
	{0xC0, 0x7B14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Enable MST_VS1_DTG_ENABLE */
	{0xC0, 0x7C14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Enable MST_VS2_DTG_ENABLE */
	{0xC0, 0x7D14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Enable MST_VS3_DTG_ENABLE */

	{0xC0, 0x7A14, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Disable MST_VS0_DTG_ENABLE */
	{0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Enable MST_VS0_DTG_ENABLE */
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Turn on video */
	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Turn off video */
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Turn on video */
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 100, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*   100ms delay	 */

/* VID_LINK_SEL_X, Y, Z, U of SER will be written 01 */
	{0xC0, 0x0100, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0110, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0120, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL}, 
	{0xC0, 0x0130, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x04CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x05CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x06CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x07CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/*****************************************/
/*	Configure GM03 DP_RX Payload IDs 	 */
/*****************************************/
/* Sets the MST payload ID of the video stream for video output port 0, 1, 2, 3  */
	{0xC0, 0x7904, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7908, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x790C, 0x03, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7910, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL}, 
	{0xC0, 0x7A10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Video FIFO Overflow Clear */
	{0xC0, 0x7A00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* MST virtual sink device 0 enable */
/* DMA mode enable */
	{0xC0, 0x7A18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7A24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7A26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7B00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7B18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7B24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7B14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7C10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7C00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7C24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7C26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7C14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7D00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7D18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7D24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7D14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x6184, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* MAIN_STREAM_ENABLE_MAIN_STREAM_ENABLE will be written 0001  ????????? */

	{0x90, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, DES_STREAM_SELECT, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, DES_STREAM_SELECT, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, DES_STREAM_SELECT, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, DES_STREAM_SELECT, 0x03, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* EDP_VIDEO_CTRL0_VIDEO_OUT_EN of SER will be written 0000  */
	{0xC0, 0x6420, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* EDP_VIDEO_CTRL0_VIDEO_OUT_EN of SER will be written 1111  */
	
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 100, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*   100ms delay	 */

	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* EDP_VIDEO_CTRL0_VIDEO_OUT_EN of SER will be written 0000  */
	{0xC0, 0x6420, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* EDP_VIDEO_CTRL0_VIDEO_OUT_EN of SER will be written 1111  */

/*****************************************/
/*	Des & GPIO & I2C Setting			 */
/*****************************************/
/* Enable Displays on each of the GMSL3 OLDIdes */
	{0x90, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* 1st LCD */
	{0x94, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x94, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* 2nd LCD */
	{0x98, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x98, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},	/* 3rd LCD */
	{0xD0, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xD0, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},	/* 4th LCD */

/* LCD Reset 1 : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset #1 */
	{0xC0, 0x0208, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0209, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0233, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0234, 0xB1, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0235, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Des1 GPIO #17 <-- RX/TX RX ID 1 */

/* LCD Reset 2( TCC8059 ) : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset 1 */
	{0x94, 0x0233, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0234, 0xB1, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0235, 0x61, TCC8059_EVB_TYPE, SER_REV_ALL}, /* Des2 GPIO #17 <-- RX/TX RX ID 1 */

/* LCD Reset 3( TCC8059 ) : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset 3 */
	{0x98, 0x0233, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0234, 0xB1, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0235, 0x61, TCC8059_EVB_TYPE, SER_REV_ALL}, /* Des3 GPIO #17 <-- RX/TX RX ID 1 */
	{0xC0, 0x020B, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x020B, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 2( TCC8050 ) : Ser GPIO #11 RX/TX RX ID 11	--> LCD Reset #2 */
	{0xC0, 0x0258, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0259, 0x0B, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0233, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0234, 0xB1, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0235, 0x6B, TCC8050_EVB_TYPE, SER_REV_ALL}, /* Des2 GPIO #17 <-- RX/TX RX ID 11 */
	{0xC0, 0x025B, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},	{0xC0, 0x025B, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},/* Toggle */

/* LCD Reset 3 : Ser GPIO #15 RX/TX RX ID 15	--> LCD Reset 3 */
	{0xC0, 0x0278, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0279, 0x0F, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0233, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0234, 0xB1, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0235, 0x6F, TCC8050_EVB_TYPE, SER_REV_ALL}, /* Des3 GPIO #17 <-- RX/TX RX ID 15 */
	{0xC0, 0x027B, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x027B, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 4 : Ser GPIO #22 RX/TX RX ID 22	--> LCD Reset #4 */
	{0xC0, 0x02B0, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x02B1, 0x16, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0233, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0234, 0xB1, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0235, 0x6F, TCC8050_EVB_TYPE, SER_REV_ALL}, /* Des1 GPIO #17 <-- RX/TX RX ID 15 */
	{0xC0, 0x02B3, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x02B3, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* LCD on : Ser GPIO #24 RX/TX RX ID 24	--> LCD On #1, 2, 3, 4 */
	{0xC0, 0x02C0, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x02C1, 0x18, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* {0xC0, 0x02C3, 0x21, TCC_ALL_EVB_TYPE}, */
	{0x90, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0237, 0xB2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0238, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Des1 GPIO #18 <-- RX/TX RX ID 24 */
	{0x94, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0237, 0xB2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0238, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Des2 GPIO #18 <-- RX/TX RX ID 24 */
	{0x98, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0237, 0xB2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0238, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Des3 GPIO #18 <-- RX/TX RX ID 24 */
	{0xD0, 0x0236, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0237, 0xB2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0238, 0x78, TCC8050_EVB_TYPE, SER_REV_ALL}, /* Des4 GPIO #18 <-- RX/TX RX ID 24 */
	{0xC0, 0x02C3, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ALL},	{0xC0, 0x02C3, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Toggle */

/* Backlight on 1 : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 1 */
	{0xC0, 0x0200, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0201, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0068, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0069, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0206, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0207, 0xA2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0208, 0x60, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Des1 GPIO #2 <-- RX/TX RX ID 0 */
	{0x90, 0x0048, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0049, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* Backlight on 2( TCC8059 ) : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 2 */
	{0x94, 0x0206, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0207, 0xA2, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0208, 0x60, TCC8059_EVB_TYPE, SER_REV_ALL}, /* Des1 GPIO #2 <-- RX/TX RX ID 0 */
	{0x94, 0x0048, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL}, {0x94, 0x0049, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL},

/* Backlight on 3( TCC8059 ) : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 2 */
	{0x98, 0x0206, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0207, 0xA2, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0208, 0x60, TCC8059_EVB_TYPE, SER_REV_ALL}, /* Des1 GPIO #2 <-- RX/TX RX ID 0 */
	{0x98, 0x0048, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL}, {0x98, 0x0049, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0203, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0203, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* Backlight on 2 : Ser GPIO #0 RX/TX RX ID 5 --> Backlight On 2 */
	{0xC0, 0x0228, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0229, 0x05, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0206, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0207, 0xA2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0208, 0x65, TCC8050_EVB_TYPE, SER_REV_ALL}, /* Des2 GPIO #2 <-- RX/TX RX ID 5 */
	{0x94, 0x0048, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},	{0x94, 0x0049, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x022B, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},	{0xC0, 0x022B, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* Backlight on 3 : Ser GPIO #0 RX/TX RX ID 14 --> Backlight On 3*/
	{0xC0, 0x0270, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0271, 0x0E, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0206, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0207, 0xA2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0208, 0x6E, TCC8050_EVB_TYPE, SER_REV_ALL}, /* Des1 GPIO #2 <-- RX/TX RX ID 14 */
	{0x98, 0x0048, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL}, {0x98, 0x0049, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0273, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0273, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* Backlight on 4 : Ser GPIO #0 RX/TX RX ID 21 --> Backlight On 4*/
	{0xC0, 0x02A8, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x02A9, 0x15, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0206, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0207, 0xA2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0208, 0x75, TCC8050_EVB_TYPE, SER_REV_ALL}, /* Des1 GPIO #2 <-- RX/TX RX ID 21 */
	{0xD0, 0x0048, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL}, {0x98, 0x0049, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0273, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0273, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},
/*****************************************/
/*	I2C Setting							 */
/*****************************************/
/* Des1, 2, 3 GPIO #14 I2C Driving */

	{0x90, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x020C, 0x90, TCC8050_EVB_TYPE, SER_REV_ALL},

	{0x0, 0x0, 0x0, 0, SER_REV_ALL}
};

static struct DP_V14_SER_DES_Reg_Data pstDP_Panel_VIC_1027_DesES2_RegVal[] = { 
	{0x90, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 100, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  100ms delay	 */

	{0x90, 0x0308, 0x03, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x03E0, 0x07, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x14A6, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x1460, 0x87, TCC_ALL_EVB_TYPE, SER_REV_ALL},	{0xC0, 0x141F, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x1431, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x141D, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x14E1, 0x22, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x04D4, 0x43, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0423, 0x47, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x04E1, 0x22, TCC_ALL_EVB_TYPE, SER_REV_ALL},	{0x90, 0x03E0, 0x07, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x0050, 0x66, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x001A, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0022, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x0029, 0x02, TCC_ALL_EVB_TYPE},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 300, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  300ms delay	 */

	{0xC0, 0x6421, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7019, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7A14, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x60AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x61B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x62B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x63B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6070, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6071, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6170, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6171, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6270, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6271, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6370, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2}, {0xC0, 0x6371, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},

	{0xC0, 0x70A0, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6064, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x6065, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x6164, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x6165, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6264, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x6265, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x6364, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x6365, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7000, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7054, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 1, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  1ms delay 	 */

	{0xC0, 0x7074, 0x0A, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Max rate : 1E -> 8.1Gbps, 14 -> 5.4Gbps, 0A -> 2.7Gbps */
	{0xC0, 0x7070, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Max lane count to 4*/

	{0xC0, 0x7030, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},{0xC0, 0x7031, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, 	// Swap = 0 <-> 2, 1 <-> 3
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 1, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  1ms delay 	 */

	{0xC0, 0x7000, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 1, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  1ms delay 	 */

	{0xC0, 0x7A18, 0x05, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7A28, 0xFF, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7A2A, 0xFF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A24, 0xFF, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7A27, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x0210, 0x40, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0211, 0x40, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0212, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0213, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0220, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0221, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0223, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0208, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0209, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x020B, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C0, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x02C1, 0x18, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x02C3, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0200, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0201, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0203, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0068, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0069, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x022D, 0x43, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x022E, 0x6f, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x022F, 0x6f, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0230, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0231, 0xb0, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0232, 0x44, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0233, 0x8c, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0234, 0xb1, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0235, 0x41, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0237, 0xb2, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0238, 0x58, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0206, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0207, 0xA2, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0208, 0x40, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0048, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0049, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x009E, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0079, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0006, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x0071, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0210, 0x60, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0212, 0x4F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0xC0, 0x0213, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022D, 0x63, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x022E, 0x6F, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x022F, 0x2F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022A, 0x18, TCC_ALL_EVB_TYPE, SER_REV_ALL}, {0x90, 0x020C, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 100, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  delay 100ms	 */

	{0x90, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 100, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  delay 100ms	 */
	
	{0x90, 0x020C, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR, 100, TCC_ALL_EVB_TYPE, SER_REV_ALL},/*	  delay 100ms	 */

	{0x90, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x0, 0x0, 0x0, 0, SER_REV_ALL}
};

static int of_parse_serdes_dt( struct Max968xx_dev		*pstDev )
{
	int						iRetVal;
	u32						uiEVB_Type, uiLane_Swap;
	struct device_node 		*pstSerDes_DN;
	
	pstSerDes_DN = of_find_compatible_node( NULL, NULL, "telechips,max968xx_configuration" );
	if( pstSerDes_DN == NULL )
	{
		dptx_warn("Can't find SerDes node \n");
	}

	iRetVal = of_property_read_u32( pstSerDes_DN, "max968xx_evb_type", &uiEVB_Type );
	if( iRetVal < 0)
	{
		dptx_warn("Can't get EVB type.. set to 'TCC8050_SV_01' by default");
		uiEVB_Type = (u32)TCC8050_SV_01;
	}

	iRetVal = of_property_read_u32( pstSerDes_DN, "max96851_lane_02_13_swap", &uiLane_Swap );
	if( iRetVal < 0)
	{
		dptx_warn("Can't max96851 lane swap option.. set to 'Enable by default");
		uiLane_Swap = (u32)1;
	}

	pstDev->ucEVB_Type = (unsigned char)uiEVB_Type;
	pstDev->bSer_LaneSwap = (bool)uiLane_Swap;

	return (0);
}

static int Dptx_Max968XX_I2C_Write( struct i2c_client *client, unsigned short usRegAdd, unsigned char ucValue )
{
	unsigned char			aucWBuf[3]	= {0,};
	int						iRW_Len;
	struct Max968xx_dev		*pstMax968xx_dev;

	pstMax968xx_dev = i2c_get_clientdata( client );

	aucWBuf[0] = (unsigned char)( usRegAdd >> 8 );
	aucWBuf[1] = (unsigned char)( usRegAdd & 0xFF );
	aucWBuf[2] = ucValue;

	iRW_Len = i2c_master_send((const struct i2c_client *)client, (const char *)aucWBuf, (int)( SER_DES_I2C_REG_ADD_LEN + SER_DES_I2C_DATA_LEN ) );
	if( iRW_Len != ( SER_DES_I2C_REG_ADD_LEN + SER_DES_I2C_DATA_LEN ) ) 
	{
		dptx_err("i2c device %s: error to write register address as 0x%x.. w len %d !!!!", client->name, client->addr, iRW_Len );
		return -EIO;
	}

	dptx_dbg("Write I2C Dev 0x%x: Reg address 0x%x -> 0x%x", ( client->addr << 1 ), usRegAdd, ucValue );

	return (0);
}

static unsigned char Dptx_Max968XX_Convert_DevAdd_To_Index( unsigned char ucI2C_DevAdd )
{
	u8						ucElements;
	struct Max968xx_dev		*pstMax968xx_dev;

	for( ucElements = 0; ucElements < SER_DES_INPUT_INDEX_MAX; ucElements++ )
	{
		pstMax968xx_dev = &stMax968xx_dev[ucElements];
		if(( pstMax968xx_dev->pstClient != NULL ) && ( pstMax968xx_dev->pstClient->addr << 1 ) == ucI2C_DevAdd )
		{
			break;
		}
	}

	return (ucElements);
}

bool Dptx_Max968XX_Get_TopologyState( u8 *pucNumOfPluggedPorts )
{
	u8						ucElements;
	struct Max968xx_dev		*pstMax968xx_dev;

	for( ucElements = (u8)DES_INPUT_INDEX_0; ucElements < SER_DES_INPUT_INDEX_MAX; ucElements++ )
	{
		pstMax968xx_dev = &stMax968xx_dev[ucElements];
		if( pstMax968xx_dev->pstClient == NULL )
		{
			break;
		}
	}

	if( ucElements == (u8)SER_DES_INPUT_INDEX_MAX )
	{
		*pucNumOfPluggedPorts = 0;
		return ( DPTX_RETURN_FAIL );
	}
	else
	{
		*pucNumOfPluggedPorts = ( ucElements - 1 );
	}

	return (DPTX_RETURN_SUCCESS);
}

/* B190164 - Update SerDes Register for Touch Init */
bool Touch_Max968XX_update_reg(struct Dptx_Params *pstDptx)
{
	struct Max968xx_dev *pstMax968xx_dev;
	int ret;

	pstMax968xx_dev = &stMax968xx_dev[SER_INPUT_INDEX_0];
	if (pstMax968xx_dev->pstClient == NULL) {
		dptx_err("There is no Serializer connnected ");
		return (bool)true;
	}

#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
	ret = tcc_tsc_serdes_update(
			pstMax968xx_dev->pstClient,
			NULL,
			pstDptx->ucNumOfPorts,
			pstMax968xx_dev->ucEVB_Type,
			pstMax968xx_dev->ucSER_Revision_Num
			);
	if (ret < 0) {
		dptx_err("failed to update register for touch");
		return (bool)true;
	}
#endif
	return (bool)false;
}

bool Dptx_Max968XX_Reset( struct Dptx_Params *pstDptx )
{
	unsigned char	ucSerDes_Index;
	unsigned char	ucSER_Revision_Num = 0, ucDES_Revision_Num = 0, ucRW_Data, ucEVB_Tpye;
	unsigned int	uiElements;
	int				iRetVal;
	bool			bRetVal;
	struct Max968xx_dev		*pstMax968xx_dev;
	struct DP_V14_SER_DES_Reg_Data	*pstSERDES_Reg_Info;

	pstMax968xx_dev = &stMax968xx_dev[SER_INPUT_INDEX_0];
	if( pstMax968xx_dev->pstClient == NULL )
	{
		dptx_err("There is no Serializer connnected ");
		return -EIO;
	}
	ucSER_Revision_Num = pstMax968xx_dev->ucSER_Revision_Num;

	pstMax968xx_dev = &stMax968xx_dev[DES_INPUT_INDEX_0];
	if( pstMax968xx_dev->pstClient == NULL )
	{
		dptx_err("There is no 1st Deserializer connnected ");
		return -EIO;
	}
	ucDES_Revision_Num = pstMax968xx_dev->ucDES_Revision_Num;

	if( ucDES_Revision_Num == DES_REV_ES2 )
	{
		pstSERDES_Reg_Info = pstDP_Panel_VIC_1027_DesES2_RegVal;
		dptx_info("Updating DES ES2 Tables... Revision Num.( %d ) <-> Ser Rev(%d) ", ucDES_Revision_Num, ucSER_Revision_Num );
	}
	else
	{
		pstSERDES_Reg_Info = pstDP_Panel_VIC_1027_DesES3_RegVal;
		dptx_info("Updating DES ES3 Tables... Revision Num.( %d ) <-> Ser Rev(%d) ", ucDES_Revision_Num, ucSER_Revision_Num );
	}

	ucEVB_Tpye = pstMax968xx_dev->ucEVB_Type;
	if( ucEVB_Tpye != (u8)TCC8059_EVB_01 )
	{
		ucEVB_Tpye = (u8)TCC8050_SV_01;
	}

	for( uiElements = 0; !( pstSERDES_Reg_Info[uiElements].uiDev_Addr == 0 && pstSERDES_Reg_Info[uiElements].uiReg_Addr == 0 && pstSERDES_Reg_Info[uiElements].uiReg_Val == 0 ); uiElements++ )
	{
		if( pstSERDES_Reg_Info[uiElements].uiDev_Addr == DP_SER_DES_DELAY_DEV_ADDR )
		{
			mdelay( pstSERDES_Reg_Info[uiElements].uiReg_Val );
			continue;
		}

		ucSerDes_Index = Dptx_Max968XX_Convert_DevAdd_To_Index( (unsigned char)pstSERDES_Reg_Info[uiElements].uiDev_Addr );
		if( ucSerDes_Index >= SER_DES_INPUT_INDEX_MAX )
		{
			dptx_dbg("Invalid SerDes Index returned %d as device address 0x%x", ucSerDes_Index, pstSERDES_Reg_Info[uiElements].uiDev_Addr);
			continue;
		}

		pstMax968xx_dev = &stMax968xx_dev[ucSerDes_Index];
		if( pstMax968xx_dev->pstClient == NULL )
		{
			continue;
		}

		if(( pstSERDES_Reg_Info[uiElements].ucDeviceType != TCC_ALL_EVB_TYPE ) && ( ucEVB_Tpye != pstSERDES_Reg_Info[uiElements].ucDeviceType ))
		{
			dptx_dbg("[%d]EVB Type %d isn't matched with %d", uiElements, ucEVB_Tpye, pstSERDES_Reg_Info[uiElements].ucDeviceType );
			continue;
		}

		if(( pstSERDES_Reg_Info[uiElements].ucSER_Revision != SER_REV_ALL ) && ( ucSER_Revision_Num != pstSERDES_Reg_Info[uiElements].ucSER_Revision ))
		{
			dptx_dbg("[%d]Ser Revision %d isn't matched with %d", uiElements, ucSER_Revision_Num, pstSERDES_Reg_Info[uiElements].ucSER_Revision );
			continue;
		}

		if(( pstDptx->bMultStreamTransport == false ) &&
			(( pstSERDES_Reg_Info[uiElements].uiReg_Addr == DES_DROP_VIDEO ) ||
			( pstSERDES_Reg_Info[uiElements].uiReg_Addr == DES_STREAM_SELECT )))
		{
			continue;
		}

		if(( pstMax968xx_dev->bSer_LaneSwap == false ) &&
			(( pstSERDES_Reg_Info[uiElements].uiReg_Addr == SER_LANE_REMAP_B0 ) ||
			( pstSERDES_Reg_Info[uiElements].uiReg_Addr == SER_LANE_REMAP_B1 )))
		{
			continue;
		}

		ucRW_Data = (unsigned char)pstSERDES_Reg_Info[uiElements].uiReg_Val;

		if(( pstDptx->bMultStreamTransport == false ) && ( pstSERDES_Reg_Info[uiElements].uiReg_Addr == SER_MISC_CONFIG_B1 ))
		{
			dptx_dbg("Set to SST...");
			ucRW_Data = MST_FUNCTION_DISABLE;
		}

		if( pstSERDES_Reg_Info[uiElements].uiReg_Addr == DES_STREAM_SELECT )
		{
			ucRW_Data = ( pstDptx->aucVCP_Id[( ucSerDes_Index - 1 )] - 1 );
			dptx_notice("Set VCP id %d to device 0x%x", ucRW_Data, pstSERDES_Reg_Info[uiElements].uiDev_Addr );
		}

		iRetVal = Dptx_Max968XX_I2C_Write( pstMax968xx_dev->pstClient, pstSERDES_Reg_Info[uiElements].uiReg_Addr, ucRW_Data );
		if( iRetVal )
		{
			continue;
		}
	}

	bRetVal = Touch_Max968XX_update_reg(pstDptx);
	if (bRetVal) {
		dptx_err("failed to update Ser/Des Register for Touch");
		return (bool)true;
	}

	dptx_info("\n[%s:%d]SerDes I2C Resister update is successfully done !!!.. written %d registers \n", __func__, __LINE__, uiElements );

	return (false);
}

static int Dptx_Max968XX_probe( struct i2c_client *client, const struct i2c_device_id *id )
{
	unsigned char			aucAddr_buf[2]	= {0,};
	unsigned char			ucData_buf, ucElements;
	int						iRW_Len;
	struct Max968xx_dev		*pstMax968xx_dev;

	for( ucElements = 0; ucElements < SER_DES_INPUT_INDEX_MAX; ucElements++ )
	{
		pstMax968xx_dev = &stMax968xx_dev[ucElements];

		if( !pstMax968xx_dev->bActivated )
		{
			pstMax968xx_dev->bActivated = true;
			break;
		}
	}

	if( ucElements == SER_DES_INPUT_INDEX_MAX )
	{
		dptx_err("It's limited to 4 SerDes ports connected.. device address as 0x%x", client->addr << 1);
		return ENOMEM;
	}

	aucAddr_buf[0] = (unsigned char)( SER_DEV_REV >> 8 );
	aucAddr_buf[1] = (unsigned char)( SER_DEV_REV & 0xFF );

	iRW_Len = i2c_master_send((const struct i2c_client *)client, (const char *)aucAddr_buf, (int)SER_DES_I2C_REG_ADD_LEN );
	if( iRW_Len != (int)SER_DES_I2C_REG_ADD_LEN )
	{
		dptx_info("DP %d port isn't connected as i2c device name '%s', dev address 0x%x ..", ucElements, client->name, client->addr);
		return 0;
	}

	iRW_Len = i2c_master_recv((const struct i2c_client *)client, &ucData_buf, 1 );
	if( iRW_Len != 1 ) 
	{
		dptx_info("DP %d port isn't connected as i2c device name '%s', dev address 0x%x ..", ucElements, client->name, client->addr);
		return 0;
	}

	if( ucElements == SER_INPUT_INDEX_0 )
	{
		pstMax968xx_dev->ucSER_Revision_Num = ucData_buf;
	}
	else
	{
		pstMax968xx_dev->ucDES_Revision_Num = ucData_buf;
	}

	pstMax968xx_dev->pstClient = client;

	i2c_set_clientdata( client, pstMax968xx_dev );

	of_parse_serdes_dt( pstMax968xx_dev );

	dptx_info("[%d]%s: %s I2C name '%s', address 0x%x : revision is %d.. SerDes Lane %s",
					ucElements,
					( pstMax968xx_dev->ucEVB_Type == TCC8059_EVB_01 ) ? "TCC8059 EVB":( pstMax968xx_dev->ucEVB_Type == TCC8050_SV_01 ) ? "TCC8050/3 sv0.1":( pstMax968xx_dev->ucEVB_Type == TCC8050_SV_10 ) ? "TCC8050/3 sv1.0":"Unknown",
					( client->addr << 1 ) == DP0_PANEL_SER_I2C_DEV_ADD ? "Ser":"Des",
					client->name,
					(( client->addr ) << 1 ),
					ucData_buf,
					( pstMax968xx_dev->bSer_LaneSwap ) ? "is swapped":"is not swapped");

	if( pstMax968xx_dev->ucEVB_Type != TCC8059_EVB_01 )
	{
		pstMax968xx_dev->ucEVB_Type = TCC8050_SV_01;
	}

	return (0);
}

static int Dptx_Max968XX_remove( struct i2c_client *client)
{
	memset( &stMax968xx_dev[SER_INPUT_INDEX_0], 0, ( sizeof( struct Max968xx_dev ) * SER_DES_INPUT_INDEX_MAX ));
	
	return (0);
}

static const struct of_device_id max_96851_78_match[] = {
	{.compatible =	"maxim,serdes"},
	{},
};
MODULE_DEVICE_TABLE(of, max_96851_78_match);


static const struct i2c_device_id max_96851_78_id[] = {
	{ "Max968XX", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, max_96851_78_id);


static struct i2c_driver stMax96851_78_drv = {
	.probe = Dptx_Max968XX_probe,
	.remove = Dptx_Max968XX_remove,
	.id_table = max_96851_78_id,
	.driver = {
			.name = "telechips,Max96851_78",
			.owner = THIS_MODULE,

#if defined(CONFIG_OF)
			.of_match_table = of_match_ptr( max_96851_78_match ),
#endif
		},
};

static int __init Max968XX_Drv_init( void ) 
{
	int	iRetVal;

	iRetVal = i2c_add_driver( &stMax96851_78_drv );
	if( iRetVal != 0 ) 
	{
		pr_err("Max96851_78 I2C registration failed %d\n", iRetVal);
		return iRetVal;
	}
	
	return ( 0 );
}
module_init(Max968XX_Drv_init);

static void __exit Max968XX_Drv_exit(void) 
{
	i2c_del_driver( &stMax96851_78_drv );
}
module_exit(Max968XX_Drv_exit);

