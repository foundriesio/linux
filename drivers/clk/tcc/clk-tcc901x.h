/****************************************************************************
 *
 * clk-tcc901x.h
 * Copyright (C) 2018 Telechips Inc.
 *
 ****************************************************************************/

#ifndef __CLK_TCC901X__H__
#define __CLK_TCC901X__H__

#include <dt-bindings/clock/telechips,tcc901x-clks.h>

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

#endif /* __CLK_TCC901X__H__ */
