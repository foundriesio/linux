// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __CLK_TCC803X__H__
#define __CLK_TCC803X__H__

#include <dt-bindings/clock/telechips,tcc803x-clks.h>

/************************************
 * Need for pmu&ckc register control
 ************************************/

#define XIN_CLK_RATE    (24*1000*1000)  // 24MHz
#define XTIN_CLK_RATE   32768   // 32.768kHz
#define HDMI_CLK_RATE   (27*1000*1000)
#define HDMI_PCLK_RATE  27
// dummy value for set lcdc peri source to hdmi pclk
#define EXT0_CLK_RATE   (24*1000*1000)  // 24MHz
#define EXT1_CLK_RATE   (24*1000*1000)  // 24MHz

#define CKC_DISABLE     0
#define CKC_ENABLE      1
#define CKC_NOCHANGE    2


/* PLL channel index */
enum {
	PLL_0 = 0,
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

/* IOBUS Peripheral Clock Enable */
enum {/* Peri. Name */
	IOBUS_PERI_UART0 = 8,
	IOBUS_PERI_UART1,
	IOBUS_PERI_UART2,		// 10
	IOBUS_PERI_UART3,
	IOBUS_PERI_UART4,
	IOBUS_PERI_UART5,
	IOBUS_PERI_UART6,
	IOBUS_PERI_UART7,		// 15
	IOBUS_PERI_SMARTCARD0,
	IOBUS_PERI_SMARTCARD1,
	IOBUS_PERI_SMARTCARD2,
	IOBUS_PERI_SMARTCARD3,
	IOBUS_PERI_SMARTCARD4,		// 20
	IOBUS_PERI_SMARTCARD5,
	IOBUS_PERI_SMARTCARD6,
	IOBUS_PERI_SMARTCARD7,
	IOBUS_PERI_I2C_MASTER0,
	IOBUS_PERI_I2C_MASTER1,		// 25
	IOBUS_PERI_I2C_MASTER2,
	IOBUS_PERI_I2C_MASTER3,
	IOBUS_PERI_I2C_MASTER4,
	IOBUS_PERI_I2C_MASTER5,
	IOBUS_PERI_I2C_MASTER6,		// 30
	IOBUS_PERI_I2C_MASTER7,
};

/* SWRESET Register (0x140000B4) */
enum {
	SWRST_CPUMP = 0,    // CPUMP
	SWRST_CPUAP,        // CPUAP
	SWRST_CPUBUS,       // CPU Bus
	SWRST_CMBUS,        // CM Bus
	SWRST_MEMBUS,       // Memory Bus
	SWRST_VBUS,         // Video Bus
	SWRST_HSIOBUS,      // HSIO Bus
	SWRST_SMUBUS,       // SMU Bus
	SWRST_GPU,          // 3D
	SWRST_DDIBUS,       // Display Bus
	SWRST_G2D,          // 2D
	SWRST_IOBUS,        // IO Bus
	SWRST_VCORE,        // Video core
	SWRST_CHEVC,        // CHEVC
	SWRST_VHEVC,        // VHEVC
	SWRST_BHEVC,        // BHEVC
	SWRST_PCIE,         // PCIE
	SWRST_U30,          // U30
	SWRST_SDMA,         // SDMA
};

enum {
	DCKC_GPU = 0,
	DCKC_G2D,
	DCKC_MAX,
};

enum {
	VBUS_VP9 = 1,
	VBUS_HEVC,
	VBUS_CODA
};

struct tPMS {
	unsigned int    fpll;
	unsigned int    en;
	unsigned int    p;
	unsigned int    m;
	unsigned int    s;
	unsigned int    src;
};

struct tDPMS {
	uint32_t	fpll;
	uint32_t	en;
	uint32_t	p;
	uint32_t	m;
	uint32_t	s;
	uint32_t	k;
	uint32_t	mfr;
	uint32_t	mrr;
	uint32_t	sscg_en;
	uint32_t	sel_pf;
	uint32_t	src;
};

struct tPMSValue {
	uint32_t fpll;
	uint32_t pms;
};

struct tCLKCTRL {
	unsigned int    freq;
	unsigned int    en;
	unsigned int    config;
	unsigned int    sel;
};

struct tPCLKCTRL {
	unsigned int    periname;
	unsigned int    freq;
	unsigned int    md;
	unsigned int    en;
	unsigned int    sel;
	unsigned int    div;
};

struct tDCKC {
	unsigned int clkctrl0;
	unsigned int clkctrl1;
	unsigned int pms;
	unsigned int con;
	unsigned int mon;
	unsigned int divc;
};

extern unsigned long tca_ckc_get_nand_iobus_clk(void);
extern int tcc_ckc_set_hdmi_audio_src(unsigned int src_id);
extern int tcc_ckc_adjust_audio_clk(unsigned int aud_id, int value);
extern void tcc_ckc_restore_audio_clk(unsigned int aud_id);

#endif /* __CLK_TCC803X__H__ */
