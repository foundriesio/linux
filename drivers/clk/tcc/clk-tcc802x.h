// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __CLK_TCC802X__H__
#define __CLK_TCC802X__H__

#include <dt-bindings/clock/tcc802x-clks.h>

#ifndef TCC802X_CKC_DRIVER
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
	{ "vpu_bus",	NULL,		FBUS_VBUS,		CLK_SET_RATE_GATE },
	{ "hsio_bus",	NULL,		FBUS_HSIO,		CLK_SET_RATE_GATE },
	{ "smu_bus",	NULL,		FBUS_SMU,		CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
	{ "mali_clk",	NULL,		FBUS_GPU,		0 },
	{ "ddi_bus",	NULL,		FBUS_DDI,		CLK_SET_RATE_GATE },
	{ "g2d_bus",	NULL,		FBUS_G2D,		0 },
	{ "io_bus",	NULL,		FBUS_IO,		CLK_SET_RATE_GATE|CLK_IS_CRITICAL },
	{ "coda",	NULL,		FBUS_CODA,		CLK_SET_RATE_GATE },
};

static struct tcc_clks_type tcc_fbus_clks = {
	NULL, FBUS_MAX, tcc_fbus_data, ARRAY_SIZE(tcc_fbus_data), 0
};

static struct _tcc_clk_data tcc_peri_data[] = {
#ifdef CONFIG_TCC_MICOM
	{ NULL,		NULL,		PERI_SFMC,		CLK_IGNORE_UNUSED },
#endif
#ifdef CONFIG_SND_TCC_AUDIO_DSP
	{ NULL,		NULL,		PERI_MDAI0,		CLK_IGNORE_UNUSED },
	{ NULL,		NULL,		PERI_MFLT0_DAI,		CLK_IGNORE_UNUSED },
#endif
#ifdef CONFIG_TCC_MICOM
	{ NULL,         NULL,           PERI_CAN,               CLK_IGNORE_UNUSED },
#endif
	{ NULL,		NULL,		PERI_CB_STAMP,		CLK_IGNORE_UNUSED },
};
static struct tcc_clks_type tcc_peri_clks = {
	NULL, PERI_MAX, tcc_peri_data, ARRAY_SIZE(tcc_peri_data), 0
};

static struct _tcc_clk_data tcc_iobus_data[] = {
#ifdef CONFIG_TCC_MICOM
	{ NULL,         NULL,           IOBUS_CAN,              CLK_IGNORE_UNUSED },
#endif
#ifdef CONFIG_SND_TCC_AUDIO_DSP
	{ "asrc_dma",	NULL,	IOBUS_ASRC_DMA,	CLK_IGNORE_UNUSED },
	{ "asrc",		NULL,	IOBUS_ASRC,		CLK_IGNORE_UNUSED },
#endif
	{ "dma0",	"dma",		IOBUS_DMA0,		CLK_IGNORE_UNUSED },
	{ "dma1",	"dma",		IOBUS_DMA1,		CLK_IGNORE_UNUSED },
	{ "dma2",	"dma",		IOBUS_DMA2,		CLK_IGNORE_UNUSED },
	{ "dma",	NULL,		IOBUS_DMA,		CLK_IGNORE_UNUSED },
	{ "i2cs2",	"i2c",		IOBUS_I2C_S2,		0 },
	{ "prt",	NULL,		IOBUS_PRT,		CLK_IGNORE_UNUSED },
	{ "adma0",	"audio0",	IOBUS_ADMA0,		CLK_IGNORE_UNUSED },
	{ "dai0",	"audio0",	IOBUS_DAI0,		CLK_IGNORE_UNUSED },
	{ "spdif0",	"audio0",	IOBUS_SPDIF0,		CLK_IGNORE_UNUSED },
	{ "audio0",	NULL,		IOBUS_AUDIO0,		CLK_IGNORE_UNUSED },
	{ "adma1",	"audio1",	IOBUS_ADMA1,		CLK_IGNORE_UNUSED },
	{ "dai1",	"audio1",	IOBUS_DAI1,		CLK_IGNORE_UNUSED },
	{ "spdif1",	"audio1",	IOBUS_SPDIF1,		CLK_IGNORE_UNUSED },
	{ "audio1",	"audio1p",	IOBUS_AUDIO1,		CLK_IGNORE_UNUSED },
	{ "i2cm0",	"i2c",		IOBUS_I2C_M0,		0 },
	{ "i2cm1",	"i2c",		IOBUS_I2C_M1,		0 },
	{ "i2cm2",	"i2c",		IOBUS_I2C_M2,		0 },
	{ "i2cm3",	"i2c",		IOBUS_I2C_M3,		0 },
	{ "i2cs0",	"i2c",		IOBUS_I2C_S0,		0 },
	{ "i2cs1",	"i2c",		IOBUS_I2C_S1,		0 },
	{ "i2c",	"i2cp",		IOBUS_I2C0,		0 },
	{ "i2cp",	NULL,		IOBUS_I2C1,		0 },
	{ "adma2",	"audio2",	IOBUS_ADMA2,		CLK_IGNORE_UNUSED },
	{ "dai2",	"audio2",	IOBUS_DAI2,		CLK_IGNORE_UNUSED },
	{ "spdif2",	"audio2",	IOBUS_SPDIF2,		CLK_IGNORE_UNUSED },
	{ "audio2",	NULL,		IOBUS_AUDIO2,		CLK_IGNORE_UNUSED },
	{ "adma3",	"audio3",	IOBUS_ADMA3,		CLK_IGNORE_UNUSED },
	{ "dai3",	"audio3",	IOBUS_DAI3,		CLK_IGNORE_UNUSED },
	{ "spdif3",	"audio3",	IOBUS_SPDIF3,		CLK_IGNORE_UNUSED },
	{ "audio3",	NULL,		IOBUS_AUDIO3,		CLK_IGNORE_UNUSED },
//	{ "audio1p",	NULL,		IOBUS_AUDIO1_1,		0 },
	{ "gpsbp",	NULL,		IOBUS_GPSB_2,		0 },
	{ "gpsb0",	"gpsb",		IOBUS_GPSB0,		0 },
	{ "gpsb1",	"gpsb",		IOBUS_GPSB1,		0 },
	{ "gpsb2",	"gpsb",		IOBUS_GPSB2,		0 },
	{ "gpsb3",	"gpsb",		IOBUS_GPSB3,		0 },
	{ "gpsb4",	"gpsb",		IOBUS_GPSB4,		0 },
	{ "gpsb5",	"gpsb",		IOBUS_GPSB5,		0 },
	{ "gpsb",	"gpsbp",	IOBUS_GPSB,		0 },
	{ "i2cs3",	"i2c",		IOBUS_I2C_S3,		0 },
	{ "i2cm4",	"i2c",		IOBUS_I2C_M4,		0 },
	{ "i2cm5",	"i2c",		IOBUS_I2C_M5,		0 },
	{ "i2cm6",	"i2c",		IOBUS_I2C_M6,		0 },
	{ "i2cm7",	"i2c",		IOBUS_I2C_M7,		0 },
	{ "uart0",	NULL,		IOBUS_UART0,		0 },
	{ "uart1",	NULL,		IOBUS_UART1,		0 },
	{ "uart2",	NULL,		IOBUS_UART2,		0 },
	{ "uart3",	NULL,		IOBUS_UART3,		0 },
	{ "uart4",	NULL,		IOBUS_UART4,		0 },
	{ "uart5",	NULL,		IOBUS_UART5,		0 },
	{ "uart6",	NULL,		IOBUS_UART6,		0 },
	{ "uart7",	NULL,		IOBUS_UART7,		0 },
	{ "smartcard0",	NULL,		IOBUS_SMARTCARD0,	0 },
	{ "udma0",	NULL,		IOBUS_UDMA0,		CLK_IGNORE_UNUSED },
	{ "udma1",	NULL,		IOBUS_UDMA1,		CLK_IGNORE_UNUSED },
	{ "udma2",	NULL,		IOBUS_UDMA2,		CLK_IGNORE_UNUSED },
	{ "udma3",	NULL,		IOBUS_UDMA3,		CLK_IGNORE_UNUSED },
	{ "uartsmart0",	NULL,		IOBUS_UART_SMARTCARD0,	CLK_IGNORE_UNUSED },
	{ "uartsmart1",	NULL,		IOBUS_UART_SMARTCARD1,	CLK_IGNORE_UNUSED },
	{ "gdma1",	NULL,		IOBUS_GDMA1,		CLK_IGNORE_UNUSED },
	{ "sdmmc0",	"sdmmc",	IOBUS_SDMMC0,		0 },
	{ "sdmmc1",	"sdmmc",	IOBUS_SDMMC1,		0 },
	{ "sdmmc2",	"sdmmc",	IOBUS_SDMMC2,		0 },
	{ "sdmmc",	NULL,		IOBUS_SDMMC,		0 },
	{ "nfc",	NULL,		IOBUS_NFC,		CLK_IGNORE_UNUSED },
	{ "edicfg",	"edi",		IOBUS_EDICFG,		CLK_IGNORE_UNUSED },
	{ "edi",	NULL,		IOBUS_EDI,		CLK_IGNORE_UNUSED },
};

static struct tcc_clks_type tcc_iobus_clks = {
	"io_bus", IOBUS_MAX, tcc_iobus_data, ARRAY_SIZE(tcc_iobus_data), CLK_SET_RATE_GATE
};

static struct tcc_clks_type tcc_hsiobus_clks = {
	"hsio_bus", HSIOBUS_MAX, NULL, 0, CLK_SET_RATE_GATE
};

static struct _tcc_clk_data tcc_ddibus_data[] = {
	{ "dbus_vioc",	"ddi_bus",	DDIBUS_VIOC,		CLK_IGNORE_UNUSED },
};
static struct tcc_clks_type tcc_ddibus_clks = {
	"ddi_bus", DDIBUS_MAX, tcc_ddibus_data, ARRAY_SIZE(tcc_ddibus_data), CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED
};

static struct _tcc_clk_data tcc_vpubus_data[] = {
	{ "coda_bus",	NULL,		VIDEOBUS_CODA_BUS,	0 },
	{ "coda_core",	"coda_bus",	VIDEOBUS_CODA_CORE,	0 },
};
static struct tcc_clks_type tcc_vpubus_clks = {
	"vpu_bus", VIDEOBUS_MAX, tcc_vpubus_data, ARRAY_SIZE(tcc_vpubus_data), CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED
};

static struct _tcc_clk_data tcc_isoip_top_data[] = {
//	{ NULL,		NULL,		ISOIP_TOP_POR,		CLK_IGNORE_UNUSED },
	{ NULL,		NULL,		ISOIP_TOP_RTC,		CLK_IGNORE_UNUSED },
	{ NULL,		NULL,		ISOIP_TOP_OTP,		CLK_IGNORE_UNUSED },
	{ NULL,		NULL,		ISOIP_TOP_DPLL,		CLK_IGNORE_UNUSED },
	{ NULL,		NULL,		ISOIP_TOP_NPLL,		CLK_IGNORE_UNUSED },
	{ NULL,		NULL,		ISOIP_TOP_MBPLL,	CLK_IGNORE_UNUSED },
};
static struct tcc_clks_type tcc_isoip_top_clks = {
	NULL, ISOIP_TOP_MAX, tcc_isoip_top_data, ARRAY_SIZE(tcc_isoip_top_data), CLK_SET_RATE_GATE
};

static struct tcc_clks_type tcc_isoip_ddi_clks = {
	NULL, ISOIP_DDB_MAX, NULL, 0, CLK_SET_RATE_GATE
};
#endif /* TCC802X_CKC_DRIVER */

/************************************
 * Need for pmu&ckc register control
 ************************************/

#define XIN_CLK_RATE    (24*1000*1000)  // 24MHz
#define XTIN_CLK_RATE   32768   // 32.768kHz
#define HDMI_CLK_RATE   (27*1000*1000)
#define HDMI_PCLK_RATE  27      // dummy value for set lcdc peri source to hdmi pclk

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
	PLL_ECLK0,
	PLL_ECLK1,
	PLL_ECLK2,
	PLL_ECLK3,
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
	SWRST_PCIE0 = 16,   // PCIE
	SWRST_PCIE1,	// PCIE
	SWRST_SDMA,         // SDMA
};

// Memory bus clock mask register 0
#define MBUSCLK_CPU0_X2X      (1 << 0)
#define MBUSCLK_CPU1_X2X      (1 << 1)
#define MBUSCLK_VBUS_X2X      (1 << 2)
#define MBUSCLK_GPUBUS_X2X    (1 << 3)
#define MBUSCLK_G2DBUS_X2X    (1 << 4)
#define MBUSCLK_DDIBUS_X2X    (1 << 5)
#define MBUSCLK_HSIOBUS_X2X   (1 << 6)
#define MBUSCLK_IOBUS_X2X     (1 << 7)
#define MBUSCLK_CMBUS_X2X     (1 << 8)
#define MBUSCLK_DAP_H2H_M     (1 << 9)
#define MBUSCLK_GPU_H2H       (1 << 10)
#define MBUSCLK_HSIOBUS_H2H       (1 << 11)
#define MBUSCLK_DDIBUS_H2H    (1 << 12)
#define MBUSCLK_SBUS_H2H      (1 << 13)
#define MBUSCLK_VBUS_H2H      (1 << 14)
#define MBUSCLK_IOBUS_H2H     (1 << 15)
#define MBUSCLK_CPU_H2H       (1 << 16)
#define MBUSCLK_DAP_H2H_S     (1 << 17)
#define MBUSCLK_CMBUS_H2H     (1 << 18)
#define MBUSCLK_G2D_H2H       (1 << 19)
#define MBUSCLK_OTP_X2H       (1 << 20)
#define MBUSCLK_CBC_MAC       (1 << 21)
#define MBUSCLK_SECURE_DMA    (1 << 22)
#define MBUSCLK_AHB_BM        (1 << 23)
#define MBUSCLK_AHB_BM_H2X    (1 << 24)
#define MBUSCLK_AXI_NIC       (1 << 25)
#define MBUSCLK_DMC_PERI      (1 << 26)
#define MBUSCLK_DMC_CH0       (1 << 27)
#define MBUSCLK_SCRBLR_CORE0  (1 << 28)
#define MBUSCLK_SCRBLR_PERI   (1 << 29)
#define MBUSCLK_DMC_H2H_S0    (1 << 30)
#define MBUSCLK_DMC_H2H_M0    (1 << 31)

enum {
	VBUS_CODA = 1,
};

typedef struct {
	unsigned int    fpll;
	unsigned int    en;
	unsigned int    p;
	unsigned int    m;
	unsigned int    s;
	unsigned int    src;
} tPMS;

typedef struct {
	unsigned int    freq;
	unsigned int    en;
	unsigned int    config;
	unsigned int    sel;
} tCLKCTRL;

typedef struct {
	unsigned int    periname;
	unsigned int    freq;
	unsigned int    md;
	unsigned int    en;
	unsigned int    sel;
	unsigned int    div;
} tPCLKCTRL;

extern unsigned long tca_ckc_get_nand_iobus_clk(void);

#endif /* __CLK_TCC802X__H__ */
