/****************************************************************************
 *
 * clk-tcc899x.h
 * Copyright (C) 2018 Telechips Inc.
 *
 ****************************************************************************/

#ifndef __CLK_TCC899X__H__
#define __CLK_TCC899X__H__

#include <dt-bindings/clock/tcc899x-clks.h>

#ifndef TCC899X_CKC_DRIVER
/*******************************
 * Define special options of clk.
 *   - parents, flags, etc.
 * must keep idx order
 *******************************/
/*	{ name,		parent_name,	idx,			flags },	*/
static struct _tcc_clk_data tcc_fbus_data[] = {
	{ "cpu0",	NULL,		FBUS_CPU0,		0 },
	{ "cpu1",	NULL,		FBUS_CPU1,		CLK_IGNORE_UNUSED },
	{ "cpu_bus",	NULL,		FBUS_CBUS,		CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
	{ "cm_bus",	NULL,		FBUS_CMBUS,		CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
	{ "mem_bus",	NULL,		FBUS_MEM,		CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
	{ "vpu_bus",	NULL,		FBUS_VBUS,		0 },
	{ "hsio_bus",	NULL,		FBUS_HSIO,		CLK_SET_RATE_GATE },
	{ "smu_bus",	NULL,		FBUS_SMU,		CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
	{ "mali_clk",	NULL,		FBUS_GPU,		0 },
	{ "ddi_bus",	NULL,		FBUS_DDI,		CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
	{ "io_bus",	NULL,		FBUS_IO,		CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
	{ "boda",	NULL,		FBUS_BODA,		0 },
	{ "hevc_c",	NULL,		FBUS_CHEVC,		0 },
	{ "hevc_b",	NULL,		FBUS_BHEVC,		0 },
};

static struct tcc_clks_type tcc_fbus_clks = {
	NULL, FBUS_MAX, tcc_fbus_data, ARRAY_SIZE(tcc_fbus_data), 0
};

static struct tcc_clks_type tcc_peri_clks = {
	NULL, PERI_MAX, NULL, 0, CLK_IGNORE_UNUSED
};

static struct tcc_clks_type tcc_iobus_clks = {
	"io_bus", IOBUS_MAX, NULL, 0, CLK_IGNORE_UNUSED
};

static struct tcc_clks_type tcc_hsiobus_clks = {
	"hsio_bus", HSIOBUS_MAX, NULL, 0, CLK_IGNORE_UNUSED 
};

static struct tcc_clks_type tcc_ddibus_clks = {
	NULL, DDIBUS_MAX, NULL, 0, 0
};

static struct tcc_clks_type tcc_vpubus_clks = {
	"vpu_bus", VIDEOBUS_MAX, NULL, 0, CLK_SET_RATE_GATE
};

static struct tcc_clks_type tcc_isoip_top_clks = {
	NULL, ISOIP_TOP_MAX, NULL, 0, CLK_IGNORE_UNUSED
};

static struct tcc_clks_type tcc_isoip_ddi_clks = {
	NULL, ISOIP_DDB_MAX, NULL, 0, CLK_IGNORE_UNUSED
};
#endif /* TCC899X_CKC_DRIVER */

/************************************
 * Need for pmu&ckc register control
 ************************************/

#define XIN_CLK_RATE    (24*1000*1000)  // 24MHz
#define XTIN_CLK_RATE   32768   // 32.768kHz
#define HDMI_CLK_RATE   (27*1000*1000)
#define HDMI_PCLK_RATE  27      // dummy value for set lcdc peri source to hdmi pclk
#define EXT0_CLK_RATE   (24*1000*1000)  // 24MHz
#define EXT1_CLK_RATE   (24*1000*1000)  // 24MHz

#define CKC_DISABLE     0
#define CKC_ENABLE      1
#define CKC_NOCHANGE    2


/* PLL channel index */
enum {
	PLL_0=0,
	PLL_1,
	PLL_2,
	PLL_3,
	PLL_4,
	PLL_DIV_0,
	PLL_DIV_1,
	PLL_DIV_2,
	PLL_DIV_3,
	PLL_DIV_4,
	PLL_XIN,
	PLL_XTIN,

	// Video dedicated PLL
	PLL_VIDEO_0,
	PLL_VIDEO_1,
};

#endif /* __CLK_TCC899X__H__ */
