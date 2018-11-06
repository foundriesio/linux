/****************************************************************************
 *
 * clk-tcc897x.h
 * Copyright (C) 2018 Telechips Inc.
 *
 ****************************************************************************/

#ifndef __CLK_TCC897X__H__
#define __CLK_TCC897X__H__

#include <dt-bindings/clock/tcc897x-clks.h>

#ifndef TCC897X_CKC_DRIVER
/*******************************
 * Define special options of clk.
 *   - parents, flags, etc.
 * must keep idx order
 *******************************/
/*	{ name,		parent_name,	idx,		flags },	*/
static struct _tcc_clk_data tcc_fbus_data[] = {
	{ "cpu0",	NULL,		FBUS_CPU0,	0 },
	{ "cpu1",	NULL,		FBUS_CPU1,	0 },
	{ "mem_bus",	NULL,		FBUS_MEM,	CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
	{ "ddi_bus",	NULL,		FBUS_DDI,	CLK_SET_RATE_GATE },
	{ "mali_clk",	NULL,		FBUS_GPU,	0 },
	{ "io_bus",	NULL,		FBUS_IO,	CLK_SET_RATE_GATE|CLK_IS_CRITICAL},
	{ "vpu_bus",	NULL,		FBUS_VBUS,	CLK_SET_RATE_GATE },
	{ "boda",	NULL,		FBUS_CODA,	CLK_SET_RATE_GATE },
	{ "hsio_bus",	NULL,		FBUS_HSIO,	CLK_SET_RATE_GATE },
	{ "smu_bus",	NULL,		FBUS_SMU,	CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
	{ "g2d_bus",	NULL,		FBUS_G2D,	CLK_SET_RATE_GATE },
#ifdef CONFIG_SUPPORT_TCC_NSK
	{ "cm_bus",	NULL,		FBUS_CMBUS,	CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
#else
	{ "cm_bus",	NULL,		FBUS_CMBUS,	CLK_IGNORE_UNUSED },
#endif

	{ "hevc_c",	NULL,		FBUS_HEVC_VCE,	CLK_SET_RATE_GATE },
	{ "hevc_v",	"hevc_c",	FBUS_HEVC_VCPU,	CLK_SET_RATE_GATE },
	{ "hevc_b",	"hevc_c",	FBUS_HEVC_BPU,	CLK_SET_RATE_GATE },
};

static struct _tcc_clk_data tcc_peri_data[] = {
	{ "timerz",	NULL,		PERI_TCZ,	CLK_IGNORE_UNUSED },
#if 0//def CONFIG_SND_TCC_AUDIO_DSP//8971 EVM
	{ "adai3_f",	NULL,		PERI_ADAI3,	CLK_IGNORE_UNUSED },
	{ "adam3_f",	NULL,		PERI_ADAM3,	CLK_IGNORE_UNUSED },
#endif
	{ "cmbus_cnt",	NULL,		PERI_CMBUS_CNT,	CLK_IGNORE_UNUSED },
#if defined(CONFIG_SND_TCC_AUDIO_DSP) || defined(CONFIG_SND_TCC_AUDIO_DSP_MODULE)
	{ "adai1_f",	NULL,		PERI_ADAI1,	CLK_IGNORE_UNUSED },
	{ "adam1_f",	NULL,		PERI_ADAM1,	CLK_IGNORE_UNUSED },
#endif
};

static struct _tcc_clk_data tcc_isoip_top_data[] = {
	{ "otp_phy",	NULL,		ISOIP_TOP_OTP,	CLK_IGNORE_UNUSED },
	{ "rtc_phy",	NULL,		ISOIP_TOP_RTC,	CLK_IGNORE_UNUSED },
	{ "pll_phy",	NULL,		ISOIP_TOP_PLL,	CLK_IGNORE_UNUSED },
};

static struct tcc_clks_type tcc_fbus_clks = {
	NULL, FBUS_MAX, tcc_fbus_data, ARRAY_SIZE(tcc_fbus_data), 0
};

static struct tcc_clks_type tcc_peri_clks = {
	NULL, PERI_MAX, tcc_peri_data, ARRAY_SIZE(tcc_peri_data), 0
};

static struct tcc_clks_type tcc_iobus_clks = {
	"io_bus", IOBUS_MAX, NULL, 0, CLK_IGNORE_UNUSED
};

static struct tcc_clks_type tcc_isoip_top_clks = {
	NULL, ISOIP_TOP_MAX, tcc_isoip_top_data, ARRAY_SIZE(tcc_isoip_top_data), CLK_SET_RATE_GATE
};

static struct tcc_clks_type tcc_hsiobus_clks = {
	"hsio_bus", HSIOBUS_MAX, NULL, 0, CLK_IGNORE_UNUSED
};

static struct tcc_clks_type tcc_ddibus_clks = {
	"ddi_bus", DDIBUS_MAX, NULL, 0, CLK_IGNORE_UNUSED
};

static struct tcc_clks_type tcc_vpubus_clks = {
	"vpu_bus", VIDEOBUS_MAX, NULL, 0, CLK_IGNORE_UNUSED
};
#endif /* TCC897X_CKC_DRIVER */

#define XIN_CLK_RATE	(24*1000*1000)	// 24MHz
#define XTIN_CLK_RATE	32768	// 32.768kHz
#define HDMI_CLK_RATE	(27*1000*1000)
#define HDMI_PCLK_RATE	27	// dummy value for set lcdc peri source to hdmi pclk
#define EXT0_CLK_RATE	(24*1000*1000)	// 24MHz
#define EXT1_CLK_RATE	(24*1000*1000)	// 24MHz

#define CKC_DISABLE 0
#define CKC_ENABLE 1
#define CKC_NOCHANGE 2

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
};

typedef struct {
	unsigned int	fpll;
	unsigned int	en;
	unsigned int	p;
	unsigned int	m;
	unsigned int	s;
	unsigned int	src;
} tPMS;

typedef struct {
	unsigned int	freq;
	unsigned int	en;
	unsigned int	config;
	unsigned int	sel;
} tCLKCTRL;

typedef struct {
	unsigned int	periname;
	unsigned int	freq;
	unsigned int	md;
	unsigned int	en;
	unsigned int	sel;
	unsigned int	div;
} tPCLKCTRL;

extern void tcc_ckc_backup_regs(unsigned int clk_down);
extern void tcc_ckc_restore_regs(void);
extern unsigned long tca_ckc_get_nand_iobus_clk(void);
extern int tcc_ckc_set_hdmi_audio_src(unsigned int src_id);
extern int tcc_ckc_adjust_audio_clk(unsigned int aud_id, int value);
extern void tcc_ckc_restore_audio_clk(unsigned int aud_id);

#endif /* __CLK_TCC897X__H__ */
