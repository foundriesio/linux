/****************************************************************************
 *
 * clk-tcc897x.h
 * Copyright (C) 2018 Telechips Inc.
 *
 ****************************************************************************/

#ifndef __CLK_TCC897X__H__
#define __CLK_TCC897X__H__

#include <dt-bindings/clock/tcc897x-clks.h>

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
