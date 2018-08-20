/****************************************************************************
 *
 * clk-tcc803x.h
 * Copyright (C) 2018 Telechips Inc.
 *
 ****************************************************************************/

#ifndef __CLK_TCC803X__H__
#define __CLK_TCC803X__H__

#include <dt-bindings/clock/tcc803x-clks.h>

#ifndef TCC803X_CKC_DRIVER
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
	{ "io_bus",	NULL,		FBUS_IO,		CLK_SET_RATE_GATE|CLK_IGNORE_UNUSED },
	{ "coda",	NULL,		FBUS_CODA,		CLK_SET_RATE_GATE },
	{ "hevc_c",	NULL,		FBUS_CHEVC,		CLK_SET_RATE_GATE },
	{ "hevc_b",	NULL,		FBUS_BHEVC,		CLK_SET_RATE_GATE },
};

static struct tcc_clks_type tcc_fbus_clks = {
	NULL, FBUS_MAX, tcc_fbus_data, ARRAY_SIZE(tcc_fbus_data), 0
};

/*static struct _tcc_clk_data tcc_peri_data[] = {
	{ NULL,		NULL,		PERI_CB_STAMP,		CLK_IGNORE_UNUSED },
};*/
static struct tcc_clks_type tcc_peri_clks = {
	NULL, PERI_MAX, NULL, 0, CLK_IGNORE_UNUSED
};

/*
static struct _tcc_clk_data tcc_iobus_data[] = {
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
	{ "smartcard1",	NULL,		IOBUS_SMARTCARD1,	0 },
	{ "smartcard2",	NULL,		IOBUS_SMARTCARD2,	0 },
	{ "smartcard3",	NULL,		IOBUS_SMARTCARD3,	0 },
	{ "smartcard4",	NULL,		IOBUS_SMARTCARD4,	0 },
	{ "smartcard5",	NULL,		IOBUS_SMARTCARD5,	0 },
	{ "smartcard6",	NULL,		IOBUS_SMARTCARD6,	0 },
	{ "smartcard7",	NULL,		IOBUS_SMARTCARD7,	0 },
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
*/
static struct tcc_clks_type tcc_iobus_clks = {
	//"io_bus", IOBUS_MAX, tcc_iobus_data, ARRAY_SIZE(tcc_iobus_data), CLK_SET_RATE_GATE
	"io_bus", IOBUS_MAX, NULL, 0, CLK_IGNORE_UNUSED
};

static struct tcc_clks_type tcc_hsiobus_clks = {
	"hsio_bus", HSIOBUS_MAX, NULL, 0, CLK_IGNORE_UNUSED 
};

static struct _tcc_clk_data tcc_ddibus_data[] = {
	{ "dbus_vioc",	"ddi_bus",	DDIBUS_VIOC,		CLK_IGNORE_UNUSED },
};
static struct tcc_clks_type tcc_ddibus_clks = {
	//"ddi_bus", DDIBUS_MAX, tcc_ddibus_data, ARRAY_SIZE(tcc_ddibus_data), CLK_SET_RATE_GATE
	"ddi_bus", DDIBUS_MAX, NULL, 0, CLK_IGNORE_UNUSED
};

static struct _tcc_clk_data tcc_vpubus_data[] = {
	{ "coda_bus",	NULL,		VIDEOBUS_CODA_BUS,	0 },
	{ "hevc_bus",	NULL,		VIDEOBUS_HEVC_BUS,	0 },
	{ "hevc_core",	"hevc_bus",	VIDEOBUS_HEVC_CORE,	0 },
	{ "coda_core",	"coda_bus",	VIDEOBUS_CODA_CORE,	0 },
};
static struct tcc_clks_type tcc_vpubus_clks = {
	//"vpu_bus", VIDEOBUS_MAX, tcc_vpubus_data, ARRAY_SIZE(tcc_vpubus_data), CLK_SET_RATE_GATE
	"vpu_bus", VIDEOBUS_MAX, NULL, 0, CLK_IGNORE_UNUSED
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
	//NULL, ISOIP_TOP_MAX, tcc_isoip_top_data, ARRAY_SIZE(tcc_isoip_top_data), CLK_SET_RATE_GATE
	NULL, ISOIP_TOP_MAX, NULL, 0, CLK_IGNORE_UNUSED
};

static struct tcc_clks_type tcc_isoip_ddi_clks = {
	NULL, ISOIP_DDB_MAX, NULL, 0, CLK_IGNORE_UNUSED
};
#endif /* TCC803X_CKC_DRIVER */

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

typedef struct {
	unsigned int    fpll;
	unsigned int    en;
	unsigned int    p;
	unsigned int    m;
	unsigned int    s;
	unsigned int    src;
} tPMS;

typedef struct {
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
} tDPMS;

typedef struct {
	uint32_t fpll;
	uint32_t pms;
} tPMSValue;

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

typedef struct {
	unsigned int clkctrl0;
	unsigned int clkctrl1;
	unsigned int pms;
	unsigned int con;
	unsigned int mon;
	unsigned int divc;
} tDCKC;

extern unsigned long tca_ckc_get_nand_iobus_clk(void);
extern int tcc_ckc_set_hdmi_audio_src(unsigned int src_id);
extern int tcc_ckc_adjust_audio_clk(unsigned int aud_id, int value);
extern void tcc_ckc_restore_audio_clk(unsigned int aud_id);

#endif /* __CLK_TCC803X__H__ */
