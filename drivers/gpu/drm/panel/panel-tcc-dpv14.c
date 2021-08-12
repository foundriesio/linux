// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Telechips.co and/or its affiliates.
 */

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_panel.h>

#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <video/tcc/vioc_lvds.h>

#include <tcc_drm_edid.h>

#include <linux/backlight.h>

#include "panel-tcc.h"

#if defined(CONFIG_DRM_PANEL_MAX968XX)
#include <linux/i2c.h>
#include <linux/regmap.h>

#include "dptx_drm.h"
#include "panel-tcc-dpv14.h"

#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
#include <linux/input/tcc_tsc_serdes.h>
#endif

#endif

#define LOG_DPV14_TAG "DRM_DPV14"

#define DRIVER_DATE	"20210810"
#define DRIVER_MAJOR	2
#define DRIVER_MINOR	0
#define DRIVER_PATCH	0


#if defined(CONFIG_DRM_PANEL_MAX968XX)
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
#define MST_FUNCTION_ENABLE				0x01

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

#define MAX968XX_DELAY_ADDR				0xEFFF
#define MAX968XX_INVALID_REG_ADDR		0xFFFF


enum SER_DES_INPUT_INDEX {
	SER_INPUT_INDEX_0		= 0,
	DES_INPUT_INDEX_0		= 1,
	DES_INPUT_INDEX_1		= 2,
	DES_INPUT_INDEX_2		= 3,
	DES_INPUT_INDEX_3		= 4,
	INPUT_INDEX_MAX			= 5
};

enum TCC805X_EVB_TYPE {
	TCC8059_EVB_01			= 0,
	TCC8050_SV_01			= 1,
	TCC8050_SV_10			= 2,
	TCC805X_EVB_UNKNOWN		= 0xFE
};

enum PHY_INPUT_STREAM_INDEX {
	PHY_INPUT_STREAM_0		= 0,
	PHY_INPUT_STREAM_1		= 1,
	PHY_INPUT_STREAM_2		= 2,
	PHY_INPUT_STREAM_3		= 3,
	PHY_INPUT_STREAM_MAX	= 4
};

struct panel_max968xx {
	uint8_t	activated;
	uint8_t	mst_mode;
	uint8_t	ser_laneswap;
	uint8_t	ser_revision;
	uint8_t	des_revision;
	uint8_t	evb_type;
	uint8_t	vcp_id[4];
	struct i2c_client	*client;
};

struct panel_max968xx_reg_data {
	unsigned int	uiDev_Addr;
	unsigned int	uiReg_Addr;
	unsigned int	uiReg_Val;
	unsigned char	ucDeviceType;
	unsigned char	ucSER_Revision;
};

#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */


struct dp_pins {
	struct pinctrl *p;
	struct pinctrl_state *pwr_port;
	struct pinctrl_state *pwr_on;
	struct pinctrl_state *reset_off;
	struct pinctrl_state *blk_on;
	struct pinctrl_state *blk_off;
	struct pinctrl_state *pwr_off;
};

struct panel_dp14 {
	struct drm_panel panel;
	struct device *dev;
	struct videomode video_mode;

	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	struct backlight_device *backlight;
	#endif

	struct dp_match_data *data;
	struct dp_pins dp_pins;

	/* Version ---------------------*/
	/** @major: driver major number */
	int major;
	/** @minor: driver minor number */
	int minor;
	/** @patchlevel: driver patch level */
	int patchlevel;
	/** @date: driver date */
	char *date;
};

struct dp_match_data {
	char *name;
};

static const struct dp_match_data dpv14_panel_0 = {
	.name = "DP PANEL-0",
};

static const struct dp_match_data dpv14_panel_1 = {
	.name = "DP PANEL-1",
};

static const struct dp_match_data dpv14_panel_2 = {
	.name = "DP PANEL-2",
};

static const struct dp_match_data dpv14_panel_3 = {
	.name = "DP PANEL-3",
};

#if defined(CONFIG_DRM_PANEL_MAX968XX)

static struct panel_max968xx	stpanel_max968xx[INPUT_INDEX_MAX];

static struct panel_max968xx_reg_data stPanel_1027_DesES3_RegVal[] = {
	{0xD0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	10, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* AGC CR Init 8G1 */
	{0xC0, 0x60AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* BST CR Init 8G1 */
	{0xC0, 0x60B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* AGC CR Init 5G4 */
	{0xC0, 0x60A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* BST CR Init 5G4 */
	{0xC0, 0x60B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* AGC CR Init 2G7 */
	{0xC0, 0x60A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* BST CR Init 2G7 */
	{0xC0, 0x60B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* Set 8G1 Error Channel Phase */
	{0xC0, 0x6070, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6071, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6170, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6171, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6270, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6271, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6370, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6371, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},

/***** MST Setting *****/
	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A14, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7000, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, SER_MISC_CONFIG_B1, MST_FUNCTION_ENABLE,
	TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x70A0, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7074, 0x1E, TCC_ALL_EVB_TYPE, SER_REV_ES4},
	{0xC0, 0x7074, 0x0A, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x7070, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, SER_LANE_REMAP_B0, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, SER_LANE_REMAP_B1, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7000, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	50, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7A14, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* VID_LINK_SEL_X, Y, Z, U of SER will be written 01 */
	{0xC0, 0x0100, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0110, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0120, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0130, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x04CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x05CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x06CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x07CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/*****************************************/
/* Configure GM03 DP_RX Payload IDs      */
/*****************************************/
/*Sets the MST payload ID of the video stream for video output port 0, 1, 2, 3*/
	{0xC0, 0x7904, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7908, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x790C, 0x03, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7910, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
/* DMA mode enable */
	{0xC0, 0x7A18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x6184, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, DES_STREAM_SELECT, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, DES_STREAM_SELECT, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, DES_STREAM_SELECT, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, DES_STREAM_SELECT, 0x03, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/********** Des & GPIO & I2C Setting *************/
	{0x90, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* 1st LCD */
	{0x94, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* 2nd LCD */
	{0x98, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},/* 3rd LCD */
	{0xD0, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},/* 4th LCD */

/* LCD Reset 1 : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset #1 */
	{0xC0, 0x0208, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0209, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0233, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0234, 0xB1, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0235, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 2( TCC8059 ) : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset 1 */
	{0x94, 0x0233, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0234, 0xB1, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0235, 0x61, TCC8059_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 3( TCC8059 ) : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset 3 */
	{0x98, 0x0233, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0234, 0xB1, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0235, 0x61, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x020B, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x020B, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 2( TCC8050 ) : Ser GPIO #11 RX/TX RX ID 11	--> LCD Reset #2 */
	{0xC0, 0x0258, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0259, 0x0B, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0233, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0234, 0xB1, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0235, 0x6B, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x025B, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x025B, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 3 : Ser GPIO #15 RX/TX RX ID 15	--> LCD Reset 3 */
	{0xC0, 0x0278, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0279, 0x0F, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0233, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0234, 0xB1, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0235, 0x6F, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x027B, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x027B, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 4 : Ser GPIO #22 RX/TX RX ID 22	--> LCD Reset #4 */
	{0xC0, 0x02B0, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02B1, 0x16, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0233, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0234, 0xB1, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0235, 0x6F, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02B3, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02B3, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* LCD on : Ser GPIO #24 RX/TX RX ID 24	--> LCD On #1, 2, 3, 4 */
	{0xC0, 0x02C0, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C1, 0x18, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0237, 0xB2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0238, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0237, 0xB2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0238, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0237, 0xB2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0238, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0236, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0237, 0xB2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0238, 0x78, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C3, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C3, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Toggle */

/* Backlight on 1 : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 1 */
	{0xC0, 0x0200, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0201, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0068, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0069, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0206, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0207, 0xA2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0208, 0x60, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0048, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0049, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* Backlight on 2( TCC8059 ) : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 2 */
	{0x94, 0x0206, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0207, 0xA2, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0208, 0x60, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0048, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0049, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL},

/* Backlight on 3( TCC8059 ) : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 2 */
	{0x98, 0x0206, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0207, 0xA2, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0208, 0x60, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0048, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0049, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0203, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0203, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* Backlight on 2 : Ser GPIO #0 RX/TX RX ID 5 --> Backlight On 2 */
	{0xC0, 0x0228, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0229, 0x05, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0206, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0207, 0xA2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0208, 0x65, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0048, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0049, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x022B, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x022B, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* Backlight on 3 : Ser GPIO #0 RX/TX RX ID 14 --> Backlight On 3*/
	{0xC0, 0x0270, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0271, 0x0E, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0206, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0207, 0xA2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0208, 0x6E, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0048, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0049, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0273, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0273, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* Backlight on 4 : Ser GPIO #0 RX/TX RX ID 21 --> Backlight On 4*/
	{0xC0, 0x02A8, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02A9, 0x15, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0206, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0207, 0xA2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0208, 0x75, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0048, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0049, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0273, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0273, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},
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

static struct panel_max968xx_reg_data stPanel_1027_DesES2_RegVal[] = {
	{0x90, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x0308, 0x03, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x03E0, 0x07, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x14A6, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x1460, 0x87, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x141F, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x1431, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x141D, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x14E1, 0x22, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x04D4, 0x43, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0423, 0x47, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x04E1, 0x22, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x03E0, 0x07, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x0050, 0x66, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x001A, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0022, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x0029, 0x02, TCC_ALL_EVB_TYPE},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	300, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x6421, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7019, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A14, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x60AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6070, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6071, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6170, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6171, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6270, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6271, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6370, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6371, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},

	{0xC0, 0x70A0, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6064, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6065, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6164, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6165, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6264, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6265, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6364, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6365, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7000, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7054, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7074, 0x0A, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7070, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7030, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7031, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7000, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7A18, 0x05, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A28, 0xFF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A2A, 0xFF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A24, 0xFF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A27, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x0210, 0x40, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0211, 0x40, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0212, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0213, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0220, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0221, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0223, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0208, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0209, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x020B, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C0, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C1, 0x18, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C3, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0200, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0201, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0203, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0068, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0069, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x022D, 0x43, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022E, 0x6f, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022F, 0x6f, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0230, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0231, 0xb0, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0232, 0x44, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0233, 0x8c, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0234, 0xb1, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0235, 0x41, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0237, 0xb2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0238, 0x58, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0206, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0207, 0xA2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0208, 0x40, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0048, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0049, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x009E, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0079, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0006, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0071, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0210, 0x60, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0212, 0x4F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0213, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022D, 0x63, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022E, 0x6F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022F, 0x2F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022A, 0x18, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x020C, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x020C, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x0, 0x0, 0x0, 0, SER_REV_ALL}
};
#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */


static inline struct panel_dp14 *to_dp_drv_panel(struct drm_panel *drm_panel)
{
	struct panel_dp14 *dp14 =
		container_of(drm_panel, struct panel_dp14, panel);

	return dp14;
}

static int panel_dpv14_disable(struct drm_panel *drm_panel)
{
	struct panel_dp14	*dp14 = to_dp_drv_panel(drm_panel);

	dev_info(dp14->dev, "[INFO][%s:%s] To %s \r\n",
		LOG_DPV14_TAG, __func__, dp14->data->name);
	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (dp14->backlight) {
		dp14->backlight->props.power = FB_BLANK_POWERDOWN;
		dp14->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(dp14->backlight);
	}
	#else
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.blk_off) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s]%s failed set pinctrl to blk_off\r\n",
			LOG_DPV14_TAG, __func__,
			dp14->data->name);
	}
	#endif
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.pwr_off) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s]%s failed set pinctrl to pwr_off\r\n",
			LOG_DPV14_TAG, __func__,
			dp14->data->name);
	}
	return 0;
}

static int panel_dpv14_enable(struct drm_panel *drm_panel)
{
	struct panel_dp14	*dp14 = to_dp_drv_panel(drm_panel);

	dev_info(
		dp14->dev,
		"[INFO][%s:%s]To %s..\r\n",
		LOG_DPV14_TAG, __func__, dp14->data->name);

	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (dp14->backlight) {
		dp14->backlight->props.state &= ~BL_CORE_FBBLANK;
		dp14->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(dp14->backlight);
	}
	#else
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.blk_on) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s]%s failed set pinctrl to blk_on\r\n",
			LOG_DPV14_TAG, __func__,
			dp14->data->name);
	}
	#endif

	return 0;
}

static int panel_dpv14_unprepare(struct drm_panel *drm_panel)
{
	struct panel_dp14 *dp14 = to_dp_drv_panel(drm_panel);

	dev_info(dp14->dev,
		"[INFO][%s:%s]To %s.. \r\n", LOG_DPV14_TAG, __func__,
		dp14->data->name);

	return 0;
}

static int panel_dpv14_prepare(struct drm_panel *drm_panel)
{
	struct panel_dp14	*dp14 = to_dp_drv_panel(drm_panel);

	dev_info(
		dp14->dev,
		"[INFO][%s:%s]To %s.. \r\n",
		LOG_DPV14_TAG, __func__, dp14->data->name);

	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.pwr_on) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s]%s failed set pinctrl to pwr_on\r\n",
			LOG_DPV14_TAG, __func__,
			dp14->data->name);
	}
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.reset_off) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s]%s failed set pinctrl to reset_off\r\n",
			LOG_DPV14_TAG, __func__,
			dp14->data->name);
	}

	return 0;
}

static int panel_dpv14_get_modes(struct drm_panel *drm_panel)
{
	struct panel_dp14 *dp14 = to_dp_drv_panel(drm_panel);
	struct drm_connector *connector = dp14->panel.connector;
	struct drm_display_mode *mode;
	struct edid *edid;
	int count = 0;

	mode = drm_mode_create(dp14->panel.drm);
	if (!mode)
		goto out;
	if (!connector)
		goto out;
	edid = kzalloc(EDID_LENGTH, GFP_KERNEL);
	if (!edid)
		goto out;

	drm_display_mode_from_videomode(&dp14->video_mode, mode);

	if (tcc_make_edid_from_display_mode(edid, mode))
		goto out_free_edid;
	drm_mode_connector_update_edid_property(connector, edid);
	count = drm_add_edid_modes(connector, edid);
	drm_edid_to_eld(connector, edid);

out_free_edid:
	kfree(edid);
out:
	return count;
}

static const struct drm_panel_funcs panel_dpv14_funcs = {
	.disable = panel_dpv14_disable,
	.unprepare = panel_dpv14_unprepare,
	.prepare = panel_dpv14_prepare,
	.enable = panel_dpv14_enable,
	.get_modes = panel_dpv14_get_modes,
};

static int panel_pinctrl_parse_dt(struct panel_dp14 *dp14)
{
	struct device_node *dn = dp14->dev->of_node;
	struct device_node *np;
	int iret_val = -ENODEV;

	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL) {
		of_node_put(np);
		iret_val = of_get_videomode(
			dn, &dp14->video_mode,
			OF_USE_NATIVE_MODE);
		if (iret_val < 0) {
			dev_err(
				dp14->dev,
				"[ERROR][%s:%s] %s failed to get of_get_videomode\r\n",
				LOG_DPV14_TAG, dp14->data->name, __func__);
			goto err_parse_dt;
		}
	} else {
		dev_err(
			dp14->dev,
			"[ERROR][%s:%s] %s failed to get display-timings property\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
	}

	/* pinctrl */
	dp14->dp_pins.p =
		devm_pinctrl_get(dp14->dev);
	if (IS_ERR(dp14->dp_pins.p)) {
		dev_info(dp14->dev,
			"[INFO][%s:%s] %s There is no pinctrl - May be this panel controls backlight using serializer\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.p = NULL;

		goto err_parse_dt;
	}

	dp14->dp_pins.pwr_port =
		pinctrl_lookup_state(
			dp14->dp_pins.p, "default");
	if (IS_ERR(dp14->dp_pins.pwr_port)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed to find pwr_port\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.pwr_port = NULL;

		goto err_parse_dt;
	}

	dp14->dp_pins.pwr_on =
		pinctrl_lookup_state(dp14->dp_pins.p, "power_on");
	if (IS_ERR(dp14->dp_pins.pwr_on)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %sfailed to find power_on \r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.pwr_on = NULL;
	}

	dp14->dp_pins.reset_off =
		pinctrl_lookup_state(
			dp14->dp_pins.p, "reset_off");
	if (IS_ERR(dp14->dp_pins.reset_off)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed to find reset_off \r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.reset_off = NULL;
	}

	dp14->dp_pins.blk_on =
		pinctrl_lookup_state(
			dp14->dp_pins.p, "blk_on");
	if (IS_ERR(dp14->dp_pins.blk_on)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed to find blk_on \r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.blk_on = NULL;
	}

	dp14->dp_pins.blk_off =
		pinctrl_lookup_state(
			dp14->dp_pins.p, "blk_off");
	if (IS_ERR(dp14->dp_pins.blk_off)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed to find blk_off \r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.blk_off = NULL;
	}

	dp14->dp_pins.pwr_off =
		pinctrl_lookup_state(
			dp14->dp_pins.p, "power_off");
	if (IS_ERR(dp14->dp_pins.pwr_off)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed to find power_off \r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.pwr_off = NULL;
	}

	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	np = of_parse_phandle(dp14->dev->of_node, "backlight", 0);
	if (np != NULL) {
		dp14->backlight = of_find_backlight_by_node(np);
		of_node_put(np);

		if (!dp14->backlight) //backlight node is not valid
			dev_err(
				dp14->dev,
				"[ERROR][%s:%s] %s backlight driver not valid\n",
				LOG_DPV14_TAG, dp14->data->name, __func__);
		else
			dev_info(
				dp14->dev,
				"[INFO][%s:%s] %s External backlight driver : max brightness[%d]\n",
				LOG_DPV14_TAG, dp14->data->name, __func__,
				dp14->backlight->props.max_brightness);
	} else {
		dev_info(
			dp14->dev,
			"[INFO][%s:%s] %s Use pinctrl backlight...\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	#else
	dev_info(
		dp14->dev,
		"[INFO][%s:%s] %s Use pinctrl backlight\n",
		LOG_DPV14_TAG, dp14->data->name, __func__);
	#endif

err_parse_dt:
	return iret_val;
}

static int panel_pinctrl_probe(struct platform_device *pdev)
{
	int iret_val;
	struct panel_dp14 *dp14;

	dp14 = devm_kzalloc(&pdev->dev, sizeof(*dp14), GFP_KERNEL);
	if (!dp14) {
		iret_val = -ENODEV;
		goto err_init;
	}

	dp14->dev = &pdev->dev;
	dp14->data =
		(struct dp_match_data *)of_device_get_match_data(&pdev->dev);
	if (!dp14->data) {
		dev_err(dp14->dev, "[ERROR:%s]Failed to find match_data\r\n", __func__);
		iret_val = -ENODEV;
		goto err_free_mem;
	}
	iret_val = panel_pinctrl_parse_dt(dp14);
	if (iret_val < 0) {
		dev_err(
			dp14->dev,
			"[ERROR][%s:%s] %s failed to parse device tree\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		iret_val = -ENODEV;
		goto err_free_mem;
	}

	/* Register the panel. */
	drm_panel_init(&dp14->panel);

	dp14->panel.dev	= dp14->dev;
	dp14->panel.funcs = &panel_dpv14_funcs;

	iret_val = drm_panel_add(&dp14->panel);
	if (iret_val < 0) {
		dev_err(dp14->dev,
			"[ERROR][%s:%s] %s failed to drm_panel_init\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		goto err_put_dev;
	}
	dev_set_drvdata(dp14->dev, dp14);

	/* Version */
	dp14->major = DRIVER_MAJOR;
	dp14->minor = DRIVER_MINOR;
	dp14->patchlevel = DRIVER_PATCH;
	dp14->date =  kstrdup(DRIVER_DATE, GFP_KERNEL);

	dev_info(dp14->dev, "Initialized %s %d.%d.%d %s for %s\n",
		dp14->data->name, dp14->major, dp14->minor,
		dp14->patchlevel, dp14->date,
		dp14->dev ? dev_name(dp14->dev) : "unknown device");
	return 0;

err_put_dev:
	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	put_device(&dp14->backlight->dev);
	#endif
err_free_mem:
	kfree(dp14);
err_init:
	return iret_val;
}

static int panel_pinctrl_remove(struct platform_device *pdev)
{
	struct panel_dp14	*dp14 = dev_get_drvdata(&pdev->dev);

	devm_pinctrl_put(dp14->dp_pins.p);

	drm_panel_detach(&dp14->panel);
	drm_panel_remove(&dp14->panel);

	panel_dpv14_disable(&dp14->panel);

	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (dp14->backlight)
		put_device(&dp14->backlight->dev);
	#endif

	kfree(dp14);
	return 0;
}

#ifdef CONFIG_PM
static int panel_pinctrl_suspend(struct device *dev)
{
	struct panel_dp14	*dp14 = dev_get_drvdata(dev);

	dev_info(dp14->dev, "[INFO][%s:%s] %s \r\n",
				LOG_DPV14_TAG,
				dp14->data->name,
				__func__);

	return 0;
}

static int panel_pinctrl_resume(struct device *dev)
{
	struct panel_dp14	*dp14 = dev_get_drvdata(dev);

	dev_err(
		dp14->dev,
		"[INFO][%s:%s] %s \r\n",
		LOG_DPV14_TAG, dp14->data->name, __func__);

	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.pwr_port) < 0) {
		dev_warn(
			dp14->dev,
			"[WARN][%s:%s] %s failed set pinctrl to pwr_port\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.pwr_off) < 0) {
		dev_warn(
			dp14->dev,
			"[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.blk_off) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	return 0;
}

static const struct dev_pm_ops panel_pinctrl_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(panel_pinctrl_suspend, panel_pinctrl_resume)
};
#endif


#if defined(CONFIG_DRM_PANEL_MAX968XX)
#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
static int panel_touch_max968XX_update_reg(void)
{
	unsigned char num_of_ports;
	int ret;
	struct panel_max968xx  *pstpanel_max968xx;

	pstpanel_max968xx = &stpanel_max968xx[SER_INPUT_INDEX_0];
	if (pstpanel_max968xx->client == NULL) {
		pr_err("There is no Serializer connnected ");
		return  -ENODEV;
	}

	ret = panel_max968xx_get_topology(&num_of_ports);
	if (ret < 0)
		return  -ENODEV;

	ret = tcc_tsc_serdes_update(
			pstpanel_max968xx->client,
			NULL,
			num_of_ports,
			pstpanel_max968xx->evb_type,
			pstpanel_max968xx->ser_revision
			);
	if (ret < 0) {
		pr_err("failed to update register for touch");
		return -ENODEV;
	}

	return 0;
}
#endif

static int panel_max968xx_i2c_write(
			struct i2c_client *client,
			unsigned short usRegAdd,
			unsigned char ucValue)
{
	uint8_t	wbuf[3]	= {0,};
	int32_t	rw_len;
	struct panel_max968xx	 *pstpanel_max968xx;

	pstpanel_max968xx = i2c_get_clientdata(client);

	wbuf[0] = (u8)(usRegAdd >> 8);
	wbuf[1] = (u8)(usRegAdd & 0xFF);
	wbuf[2] = ucValue;

	rw_len = i2c_master_send(
			(const struct i2c_client *)client,
			(const char *)wbuf,
			(int)(SER_DES_I2C_REG_ADD_LEN + SER_DES_I2C_DATA_LEN));
	if (rw_len != (SER_DES_I2C_REG_ADD_LEN + SER_DES_I2C_DATA_LEN)) {
		pr_err("Fail to write data to add 0x%x.. len %d",
					client->addr << 1,
					rw_len);
		return -ENODEV;
	}
/*
	pr_info("Write 0x%x to add 0x%x, Reg add 0x%x",
				ucValue,
				client->addr << 1,
				usRegAdd);
*/
	return 0;
}

static uint8_t panel_max968xx_convert_add_to_index(uint8_t ucI2C_DevAdd)
{
	u8 elements;
	struct panel_max968xx *pstpanel_max968xx;

	for (elements = 0; elements < INPUT_INDEX_MAX; elements++) {
		pstpanel_max968xx = &stpanel_max968xx[elements];
		if ((pstpanel_max968xx->client != NULL) &&
			(pstpanel_max968xx->client->addr << 1) == ucI2C_DevAdd)
			break;
	}

	return elements;
}

int panel_max968xx_get_topology(unsigned char *num_of_ports)
{
	u8 elements;
	struct panel_max968xx  *pstpanel_max968xx;

	if (num_of_ports == NULL) {
		pr_err("Err [%s:%d]num_of_ports == NULL", __func__, __LINE__);
		return -EINVAL;
	}

	for (elements = (u8)DES_INPUT_INDEX_0;
			elements < INPUT_INDEX_MAX;
			elements++) {
		pstpanel_max968xx = &stpanel_max968xx[elements];
		if (pstpanel_max968xx->client == NULL)
			break;
	}

	if (elements <= (u8)INPUT_INDEX_MAX) {
		*num_of_ports = (elements - 1);
	} else {
		*num_of_ports = 0;
		pr_warn("Warn [%s:%d]Invalid num of ports as %d", __func__, __LINE__, (elements - 1));
	}

	return 0;
}

int panel_max968xx_reset(void)
{
	unsigned char input_index;
	unsigned char ser_revision = 0, des_revision = 0;
	unsigned char dev_add, rw_data, evb_tpye;
	unsigned int elements;
	int ret;
	struct panel_max968xx *pstpanel_max968xx;
	struct panel_max968xx_reg_data *pstpanel_max968xx_reg_data;

	pstpanel_max968xx = &stpanel_max968xx[SER_INPUT_INDEX_0];
	if (pstpanel_max968xx->client == NULL) {
		pr_err("There is no Serializer connnected ");
		return -EIO;
	}
	ser_revision = pstpanel_max968xx->ser_revision;

	pstpanel_max968xx = &stpanel_max968xx[DES_INPUT_INDEX_0];
	if (pstpanel_max968xx->client == NULL) {
		pr_err("There is no 1st Deserializer connnected ");
		return -EIO;
	}
	des_revision = pstpanel_max968xx->des_revision;

	if (des_revision == DES_REV_ES2) {
		pstpanel_max968xx_reg_data = stPanel_1027_DesES2_RegVal;
		pr_info("Updating DES ES2 Tables...");
	} else {
		pstpanel_max968xx_reg_data = stPanel_1027_DesES3_RegVal;
		pr_info("Updating DES ES3 Tables...");
	}

	evb_tpye = pstpanel_max968xx->evb_type;
	if (evb_tpye != (uint8_t)TCC8059_EVB_TYPE)
		evb_tpye = (uint8_t)TCC8050_EVB_TYPE;

	for (elements = 0;
			!(pstpanel_max968xx_reg_data[elements].uiDev_Addr == 0 &&
			pstpanel_max968xx_reg_data[elements].uiReg_Addr == 0 &&
			pstpanel_max968xx_reg_data[elements].uiReg_Val == 0);
			elements++) {
		if (pstpanel_max968xx_reg_data[elements].uiDev_Addr == MAX968XX_DELAY_ADDR) {
			mdelay(pstpanel_max968xx_reg_data[elements].uiReg_Val);
			continue;
		}

		dev_add = (u8)pstpanel_max968xx_reg_data[elements].uiDev_Addr;

		input_index = panel_max968xx_convert_add_to_index(dev_add);
		if (input_index >= INPUT_INDEX_MAX) {
			//pr_info("Dev add 0x%x wasn't connected", dev_add);
			continue;
		}

		pstpanel_max968xx = &stpanel_max968xx[input_index];
		if (pstpanel_max968xx->client == NULL)
			continue;

		if ((pstpanel_max968xx_reg_data[elements].ucDeviceType != TCC_ALL_EVB_TYPE) &&
			(evb_tpye != pstpanel_max968xx_reg_data[elements].ucDeviceType)) {
/*
			pr_info("[%d]EVB %s isn't matched with %d",
					elements,
					(evb_tpye == TCC8059_EVB_TYPE) ? "TCC8059" : "TCC8059/3",
					pstpanel_max968xx_reg_data[elements].ucDeviceType);
*/
			continue;
		}

		if ((pstpanel_max968xx_reg_data[elements].ucSER_Revision != SER_REV_ALL) &&
			(ser_revision != pstpanel_max968xx_reg_data[elements].ucSER_Revision)) {
/*
			pr_info("[%d]Ser Revision %d isn't matched with %d",
						elements,
						ser_revision,
						pstpanel_max968xx_reg_data[elements].ucSER_Revision);
*/
			continue;
		}

		if ((!pstpanel_max968xx->mst_mode) &&
			((pstpanel_max968xx_reg_data[elements].uiReg_Addr == DES_DROP_VIDEO) ||
			(pstpanel_max968xx_reg_data[elements].uiReg_Addr == DES_STREAM_SELECT)))
			continue;

		if ((!pstpanel_max968xx->ser_laneswap) &&
			((pstpanel_max968xx_reg_data[elements].uiReg_Addr == SER_LANE_REMAP_B0) ||
			(pstpanel_max968xx_reg_data[elements].uiReg_Addr == SER_LANE_REMAP_B1)))
			continue;

		rw_data = (uint8_t)pstpanel_max968xx_reg_data[elements].uiReg_Val;

		if ((!pstpanel_max968xx->mst_mode) &&
			(pstpanel_max968xx_reg_data[elements].uiReg_Addr == SER_MISC_CONFIG_B1)) {
			rw_data = MST_FUNCTION_DISABLE;
		}

		if (pstpanel_max968xx_reg_data[elements].uiReg_Addr == DES_STREAM_SELECT) {
			rw_data = (pstpanel_max968xx->vcp_id[(input_index - 1)] - 1);
/*
			pr_notice("Set VCP id %d to device 0x%x",
						rw_data,
						pstpanel_max968xx_reg_data[elements].uiDev_Addr);
*/
		}

		ret = panel_max968xx_i2c_write(
						pstpanel_max968xx->client,
						pstpanel_max968xx_reg_data[elements].uiReg_Addr,
						rw_data);
		if (ret != 0)
			continue;
	}

#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
	ret = panel_touch_max968XX_update_reg();
	if (ret < 0) {
		pr_err("failed to update Ser/Des Register for Touch");
		return ret;
	}
#endif

	pr_info("\n[%s:%d]%d Resisters were successfully updated!!!\n",
				__func__,
				__LINE__,
				elements);

	return 0;
}

static int panel_max968xx_parse_dt(struct panel_max968xx *pstpanel_max968xx)
{
	int ret;
	uint32_t evb_type, ser_laneswap;
	uint32_t vcp_id;
	struct device_node *dn;

	dn = of_find_compatible_node(
						NULL,
						NULL,
						"telechips,max968xx_configuration");
	if (dn == NULL) {
		pr_warn("Can't find SerDes node\n");
		return 0;
	}

	ret = of_property_read_u32(
					dn,
					"max968xx_evb_type",
					&evb_type);
	if (ret < 0) {
		pr_warn("Can't get EVB type.. set to TCC8050_SV_10(default)");
		evb_type = (u32)TCC8050_SV_10;
	}

	ret = of_property_read_u32(
					dn,
					"max96851_lane_02_13_swap",
					&ser_laneswap);
	if (ret < 0) {
		pr_warn("Can't max96851 lane swap option.. set to 1(default)");
		ser_laneswap = (u32)1;
	}

	pstpanel_max968xx->evb_type = (uint8_t)evb_type;
	pstpanel_max968xx->ser_laneswap = (uint8_t)ser_laneswap;

	dn = of_find_compatible_node(
						NULL,
						NULL,
						"telechips,dpv14-tx");
	if (dn == NULL) {
		pr_warn("Can't find dpv14-tx node\n");
		return 0;
	}

	ret = of_property_read_u32(dn, "vcp_id_1st_sink", &vcp_id);
	if (ret < 0) {
		pr_err("Can't get 1st Sink VCP Id.. set to 1(default)");
		vcp_id = (u32)1;
	}
	pstpanel_max968xx->vcp_id[PHY_INPUT_STREAM_0] = (uint8_t)vcp_id;

	ret = of_property_read_u32(dn, "vcp_id_2nd_sink", &vcp_id);
	if (ret < 0) {
		pr_err("Can't get 2nd Sink VCP Id.. set to 2(default)");
		vcp_id = (u32)2;
	}
	pstpanel_max968xx->vcp_id[PHY_INPUT_STREAM_1] = (uint8_t)vcp_id;

	ret = of_property_read_u32(dn, "vcp_id_3rd_sink", &vcp_id);
	if (ret < 0) {
		pr_err("Can't get 3rd Sink VCP Id.. set to 3(default)");
		vcp_id = (u32)3;
	}
	pstpanel_max968xx->vcp_id[PHY_INPUT_STREAM_2] = (uint8_t)vcp_id;

	ret = of_property_read_u32(dn, "vcp_id_4th_sink", &vcp_id);
	if (ret < 0) {
		pr_err("Can't get 4th Sink VCP Id.. set to 4(default)");
		vcp_id = (u32)4;
	}
	pstpanel_max968xx->vcp_id[PHY_INPUT_STREAM_3] = (uint8_t)vcp_id;

	return 0;
}

static int panel_max968xx_i2c_probe(
			struct i2c_client *client,
			const struct i2c_device_id *id)
{
	uint8_t addr_buf[2] = {0,};
	uint8_t data_buf, elements, num_of_ports;
	uint8_t got_rev = 0;
	int32_t ret, rw_len;
	struct panel_max968xx *pstpanel_max968xx;

	for (elements = 0; elements < INPUT_INDEX_MAX; elements++) {
		pstpanel_max968xx = &stpanel_max968xx[elements];

		if (!pstpanel_max968xx->activated) {
			pstpanel_max968xx->activated = true;
			break;
		}
	}
	if (elements == INPUT_INDEX_MAX) {
		pr_err("[%s:%d]No more port for Dev. add 0x%x", 
					__func__,
					__LINE__,
					client->addr << 1);
		return -ENOMEM;
	}

	addr_buf[0] = (uint8_t)(SER_DEV_REV >> 8);
	addr_buf[1] = (uint8_t)(SER_DEV_REV & 0xFF);

	rw_len = i2c_master_send(
				(const struct i2c_client *)client,
				(const char *)addr_buf,
				(int)SER_DES_I2C_REG_ADD_LEN);
	if (rw_len != (int)SER_DES_I2C_REG_ADD_LEN) {
		pr_info("[%s:%d]Dev add 0x%x isn't connected",
					__func__,
					__LINE__,
					client->addr << 1);
		got_rev = 0;
	} else {
		got_rev = 1;
	}

	if (got_rev) {
		rw_len = i2c_master_recv(
					(const struct i2c_client *)client,
					&data_buf,
					1);
		if (rw_len != 1) {
			pr_info("[%s:%d]Dev add 0x%x isn't connected",
						__func__,
						__LINE__,
						client->addr << 1);
			got_rev = 0;
		}
	}

	if (!got_rev) {
		if(elements != DES_INPUT_INDEX_3)
			return 0;

		ret = panel_max968xx_get_topology(&num_of_ports);
		if (ret < 0) {
			pr_err("[%s:%d]Error from panel_max968xx_get_topology()",
						__func__,
						__LINE__);
		} else {
			if (num_of_ports > PHY_INPUT_STREAM_MAX) {
				pr_info("\n[%s:%d]Invalid num_of_ports as %d",
						__func__,
						__LINE__,
						num_of_ports);
			} else {
				pstpanel_max968xx->mst_mode = (num_of_ports > 1) ? true : false;
				dpv14_set_num_of_panels(num_of_ports);
			}
		}

#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
		ret = panel_touch_max968XX_update_reg();
		if (ret < 0) {
			pr_err("failed to update Ser/Des Register for Touch");
			return ret;
		}
#endif

		return 0;
	}

	if (elements == SER_INPUT_INDEX_0)
		pstpanel_max968xx->ser_revision = data_buf;
	else if (elements == DES_INPUT_INDEX_0)
		pstpanel_max968xx->des_revision = data_buf;

	pstpanel_max968xx->client = client;

	i2c_set_clientdata(client, pstpanel_max968xx);

	panel_max968xx_parse_dt(pstpanel_max968xx);

	pr_info("[%d]%s: %s(%s), add 0x%x, rev %d, vcp(%d %d %d %d)..Ser Lane %s",
			elements,
			(pstpanel_max968xx->evb_type == TCC8059_EVB_01) ? "TCC8059 EVB" :
			(pstpanel_max968xx->evb_type == TCC8050_SV_01) ? "TCC8050/3 sv0.1" :
			(pstpanel_max968xx->evb_type == TCC8050_SV_10) ? "TCC8050/3 sv1.0" :
			"Unknown",
			(client->addr << 1) == DP0_PANEL_SER_I2C_DEV_ADD ? "Ser":"Des",
			client->name,
			((client->addr) << 1),
			data_buf,
			pstpanel_max968xx->vcp_id[0],
			pstpanel_max968xx->vcp_id[1],
			pstpanel_max968xx->vcp_id[2],
			pstpanel_max968xx->vcp_id[3],
			(pstpanel_max968xx->ser_laneswap) ? "is swapped":"is not swapped");

	if (pstpanel_max968xx->evb_type != TCC8059_EVB_01)
		pstpanel_max968xx->evb_type = TCC8050_SV_01;

	return 0;
}

static int panel_max968xx_i2c_remove(struct i2c_client *client)
{
	memset(&stpanel_max968xx[SER_INPUT_INDEX_0], 0,
		(sizeof(struct panel_max968xx) * INPUT_INDEX_MAX));

	return 0;
}
#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */



static const struct of_device_id panel_pinctrl_of_table[] = {
	{ .compatible = "telechips,drm-lvds-dpv14-0",
	  .data = &dpv14_panel_0,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-1",
	  .data = &dpv14_panel_1,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-2",
	  .data = &dpv14_panel_2,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-3",
	  .data = &dpv14_panel_3,
	},
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, panel_pinctrl_of_table);

static struct platform_driver pintctrl_driver = {
	.probe		= panel_pinctrl_probe,
	.remove		= panel_pinctrl_remove,
	.driver		= {
		.name	= "panel-dpv14",
#ifdef CONFIG_PM
		.pm	= &panel_pinctrl_pm_ops,
#endif
		.of_match_table = panel_pinctrl_of_table,
	},
};


#if defined(CONFIG_DRM_PANEL_MAX968XX)
static const struct of_device_id max968xx_match[] = {
	{.compatible = "maxim,serdes"},
	{},
};
MODULE_DEVICE_TABLE(of, max968xx_match);


static const struct i2c_device_id max968xx_id[] = {
	{ "Max968XX", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, max968xx_id);


static struct i2c_driver max968xx_i2c_drv = {
	.probe = panel_max968xx_i2c_probe,
	.remove = panel_max968xx_i2c_remove,
	.id_table = max968xx_id,
	.driver = {
			.name = "telechips,Max96851_78",
			.owner = THIS_MODULE,

#if defined(CONFIG_OF)
			.of_match_table = of_match_ptr(max968xx_match),
#endif
		},
};

static int __init panel_dpv14_init(void)
{
	int ret;

	memset(&stpanel_max968xx[SER_INPUT_INDEX_0],
			0,
			(sizeof(struct panel_max968xx) * INPUT_INDEX_MAX));

	ret = i2c_add_driver(&max968xx_i2c_drv);
	if (ret != 0) {
		pr_err("Max96851_78 I2C registration failed %d\n", ret);
		return ret;
	}

	return platform_driver_register(&pintctrl_driver);
}
module_init(panel_dpv14_init);

static void __exit panel_dpv14_exit(void)
{
	i2c_del_driver(&max968xx_i2c_drv);

	return platform_driver_unregister(&pintctrl_driver);
}
module_exit(panel_dpv14_exit);

#else

module_platform_driver(pintctrl_driver);

#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */

MODULE_DESCRIPTION("Display Port Panel Driver");
MODULE_LICENSE("GPL");
