/*
 * linux/driver/net/tcc_gmac/tca_gmac.h
 *
 * Author : Telechips <linux@telechips.com>
 * Created : June 22, 2010
 * Description : This is the driver for the Telechips
 * MAC 10/100/1000 on-chip Ethernet controllers.
 *               Telechips Ethernet IPs are built around a Synopsys IP Core.
 *
 * Copyright (C) 2010 Telechips
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef _TCA_GMAC_H_
#define _TCA_GMAC_H_

//#include <mach/bsp.h>
//#include <mach/gpio.h>
//#include <mach/iomap.h>

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/phy.h>
#include <linux/delay.h>

#if !defined(CONFIG_ARM64_TCC_BUILD)
#include <asm/mach-types.h>
#include <asm/system_info.h>
#endif
/* Define HSIO GMAC Structure */

typedef struct {
	unsigned VALUE:16;
} TCC_DEF16BIT_IDX_TYPE;

typedef union {
	unsigned short nREG;
	TCC_DEF16BIT_IDX_TYPE bREG;
} TCC_DEF16BIT_TYPE;

typedef struct {
	unsigned VALUE:32;
} TCC_DEF32BIT_IDX_TYPE;

typedef union {
	unsigned long nREG;
	TCC_DEF32BIT_IDX_TYPE bREG;
} TCC_DEF32BIT_TYPE;

typedef struct _GMACDMA {
	volatile TCC_DEF32BIT_TYPE BMODE;
	volatile TCC_DEF32BIT_TYPE TPD;
	volatile TCC_DEF32BIT_TYPE RPD;
	volatile TCC_DEF32BIT_TYPE RDLA;
	volatile TCC_DEF32BIT_TYPE TDLA;
	volatile TCC_DEF32BIT_TYPE STS;
	volatile TCC_DEF32BIT_TYPE OPMODE;
	volatile TCC_DEF32BIT_TYPE IE;
	volatile TCC_DEF32BIT_TYPE MFBOC;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	volatile TCC_DEF32BIT_TYPE CHTD;
	volatile TCC_DEF32BIT_TYPE CHRD;
	volatile TCC_DEF32BIT_TYPE CHTBA;
	volatile TCC_DEF32BIT_TYPE CHRBA;
} GMACDMA, *PGMACDMA;

typedef struct _GMAC {
	volatile TCC_DEF32BIT_TYPE MACC;
	volatile TCC_DEF32BIT_TYPE MACFF;
	volatile TCC_DEF32BIT_TYPE HTH;
	volatile TCC_DEF32BIT_TYPE HTL;
	volatile TCC_DEF32BIT_TYPE GMIIA;
	volatile TCC_DEF32BIT_TYPE GMIID;
	volatile TCC_DEF32BIT_TYPE FC;
	volatile TCC_DEF32BIT_TYPE VLANT;
	volatile TCC_DEF32BIT_TYPE VERSION;
	unsigned:32;
	volatile TCC_DEF32BIT_TYPE RWFF;
	volatile TCC_DEF32BIT_TYPE PMTCS;
	unsigned:32;
	unsigned:32;
	volatile TCC_DEF32BIT_TYPE IRQS;
	volatile TCC_DEF32BIT_TYPE IRQM;
	volatile TCC_DEF32BIT_TYPE MACA0H;
	volatile TCC_DEF32BIT_TYPE MACA0L;
	volatile TCC_DEF32BIT_TYPE MACA1H;
	volatile TCC_DEF32BIT_TYPE MACA1L;
	volatile TCC_DEF32BIT_TYPE MACA2H;
	volatile TCC_DEF32BIT_TYPE MACA2L;
	volatile TCC_DEF32BIT_TYPE MACA3H;
	volatile TCC_DEF32BIT_TYPE MACA3L;
	volatile TCC_DEF32BIT_TYPE MACA4H;
	volatile TCC_DEF32BIT_TYPE MACA4L;
	volatile TCC_DEF32BIT_TYPE MACA5H;
	volatile TCC_DEF32BIT_TYPE MACA5L;
	volatile TCC_DEF32BIT_TYPE MACA6H;
	volatile TCC_DEF32BIT_TYPE MACA6L;
	volatile TCC_DEF32BIT_TYPE MACA7H;
	volatile TCC_DEF32BIT_TYPE MACA7L;
	volatile TCC_DEF32BIT_TYPE MACA8H;
	volatile TCC_DEF32BIT_TYPE MACA8L;
	volatile TCC_DEF32BIT_TYPE MACA9H;
	volatile TCC_DEF32BIT_TYPE MACA9L;
	volatile TCC_DEF32BIT_TYPE MACA10H;
	volatile TCC_DEF32BIT_TYPE MACA10L;
	volatile TCC_DEF32BIT_TYPE MACA11H;
	volatile TCC_DEF32BIT_TYPE MACA11L;
	volatile TCC_DEF32BIT_TYPE MACA12H;
	volatile TCC_DEF32BIT_TYPE MACA12L;
	volatile TCC_DEF32BIT_TYPE MACA13H;
	volatile TCC_DEF32BIT_TYPE MACA13L;
	volatile TCC_DEF32BIT_TYPE MACA14H;
	volatile TCC_DEF32BIT_TYPE MACA14L;
	volatile TCC_DEF32BIT_TYPE MACA15H;
	volatile TCC_DEF32BIT_TYPE MACA15L;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	volatile TCC_DEF32BIT_TYPE RGMIIS;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	volatile TCC_DEF32BIT_TYPE MMC_CNTRL;
	volatile TCC_DEF32BIT_TYPE MMC_INTR_RX;
	volatile TCC_DEF32BIT_TYPE MMC_INTR_TX;
	volatile TCC_DEF32BIT_TYPE MMC_INTR_MASK_RX;
	volatile TCC_DEF32BIT_TYPE MMC_INTR_MASK_TX;
	volatile TCC_DEF32BIT_TYPE TXOCTETCOUNT_GB;
	volatile TCC_DEF32BIT_TYPE TXFRAMECOUNT_GB;
	volatile TCC_DEF32BIT_TYPE TXBRADCASTFRAMES_G;
	volatile TCC_DEF32BIT_TYPE TXMULTICASTFRAMES_G;
	volatile TCC_DEF32BIT_TYPE TX64OCTETS_GB;
	volatile TCC_DEF32BIT_TYPE TX65TO127OCTETS_GB;
	volatile TCC_DEF32BIT_TYPE TX128TO255OCTETS_GB;
	volatile TCC_DEF32BIT_TYPE TX256TO511OCTETS_GB;
	volatile TCC_DEF32BIT_TYPE TX512TO1023OCTETS_GB;
	volatile TCC_DEF32BIT_TYPE TX1024TOMAXOCTETS_GB;
	volatile TCC_DEF32BIT_TYPE TXUNICASTFRAMES_GB;
	volatile TCC_DEF32BIT_TYPE TXMULTICASTFRAMES_GB;
	volatile TCC_DEF32BIT_TYPE TXBRADCASTFRAMES_GB;
	volatile TCC_DEF32BIT_TYPE TXUNDERFLOWERRR;
	volatile TCC_DEF32BIT_TYPE TXSINGLECOL_G;
	volatile TCC_DEF32BIT_TYPE TXMULTICOL_G;
	volatile TCC_DEF32BIT_TYPE TXDEFERRED;
	volatile TCC_DEF32BIT_TYPE TXLATECOL;
	volatile TCC_DEF32BIT_TYPE TXEXESSCOL;
	volatile TCC_DEF32BIT_TYPE TXCARRIERERRR;
	volatile TCC_DEF32BIT_TYPE TXOCTETCOUNT_G;
	volatile TCC_DEF32BIT_TYPE TXFRAMECOUNT_G;
	volatile TCC_DEF32BIT_TYPE TXEXCESSDEF;
	volatile TCC_DEF32BIT_TYPE TXPAUSEFRAMES;
	volatile TCC_DEF32BIT_TYPE TXVLANFRAMES_G;
	unsigned:32;
	unsigned:32;
	volatile TCC_DEF32BIT_TYPE RXFRAMECOUNT_GB;
	volatile TCC_DEF32BIT_TYPE RXOCTETCOUNT_GB;
	volatile TCC_DEF32BIT_TYPE RXOCTETCOUNT_G;
	volatile TCC_DEF32BIT_TYPE RXBRADCASTFRAMES_G;
	volatile TCC_DEF32BIT_TYPE RXMULTICASTFRAMES_G;
	volatile TCC_DEF32BIT_TYPE RXCRCERRR;
	volatile TCC_DEF32BIT_TYPE RXALIGNMENTERRR;
	volatile TCC_DEF32BIT_TYPE RXRUNTERRR;
	volatile TCC_DEF32BIT_TYPE RXJABBERERRR;
	volatile TCC_DEF32BIT_TYPE RXUNDERSIZE_G;
	volatile TCC_DEF32BIT_TYPE RXOVERSIZE_G;
	volatile TCC_DEF32BIT_TYPE RX64OCTETS_GB;
	volatile TCC_DEF32BIT_TYPE RX65TO127OCTETS_GB;
	volatile TCC_DEF32BIT_TYPE RX128TO255OCTETS_GB;
	volatile TCC_DEF32BIT_TYPE RX256TO511OCTETS_GB;
	volatile TCC_DEF32BIT_TYPE RX512TO1023OCTETS_GB;
	volatile TCC_DEF32BIT_TYPE RX1024TOMAXOCTETS_GB;
	volatile TCC_DEF32BIT_TYPE RXUNICASTFRAMES_G;
	volatile TCC_DEF32BIT_TYPE RXLENGTHERRR;
	volatile TCC_DEF32BIT_TYPE RXOUTOFRANGETYPE;
	volatile TCC_DEF32BIT_TYPE RXPAUSEFRAMES;
	volatile TCC_DEF32BIT_TYPE rxfifooverflow;
	volatile TCC_DEF32BIT_TYPE rxvlanframes_gb;
	volatile TCC_DEF32BIT_TYPE rxwatchdogerror;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	unsigned:32;
	volatile TCC_DEF32BIT_TYPE mmc_ipc_intr_mask_rx;
	unsigned:32;
	volatile TCC_DEF32BIT_TYPE mmc_ipc_intr_rx;
	unsigned:32;
	volatile TCC_DEF32BIT_TYPE rxipv4_gd_frms;
	volatile TCC_DEF32BIT_TYPE rxipv4_hdrerr_frms;
	volatile TCC_DEF32BIT_TYPE rxipv4_nopay_frms;
	volatile TCC_DEF32BIT_TYPE rxipv4_frag_frms;
	volatile TCC_DEF32BIT_TYPE rxipv4_udsbl_frms;
	volatile TCC_DEF32BIT_TYPE rxipv6_gd_frms;
	volatile TCC_DEF32BIT_TYPE rxipv6_hdrerr_frms;
	volatile TCC_DEF32BIT_TYPE rxipv6_nopay_frms;
	volatile TCC_DEF32BIT_TYPE rxudp_gd_frms;
	volatile TCC_DEF32BIT_TYPE rxudp_err_frms;
	volatile TCC_DEF32BIT_TYPE rxtcp_gd_frms;
	volatile TCC_DEF32BIT_TYPE rxtcp_err_frms;
	volatile TCC_DEF32BIT_TYPE rxicmp_gd_frms;
	volatile TCC_DEF32BIT_TYPE rxicmp_err_frms;
	unsigned:32;
	unsigned:32;
	volatile TCC_DEF32BIT_TYPE rxipv4_gd_octets;
	volatile TCC_DEF32BIT_TYPE rxipv4_hdrerr_octets;
	volatile TCC_DEF32BIT_TYPE rxipv4_nopay_octets;
	volatile TCC_DEF32BIT_TYPE rxipv4_frag_octets;
	volatile TCC_DEF32BIT_TYPE rxipv4_udsbl_octets;
	volatile TCC_DEF32BIT_TYPE rxipv6_gd_octets;
	volatile TCC_DEF32BIT_TYPE rxipv6_hdrerr_octets;
	volatile TCC_DEF32BIT_TYPE rxipv6_nopay_octets;
	volatile TCC_DEF32BIT_TYPE rxudp_gd_octets;
	volatile TCC_DEF32BIT_TYPE rxudp_err_octets;
	volatile TCC_DEF32BIT_TYPE rxtcp_gd_octets;
	volatile TCC_DEF32BIT_TYPE rxtcp_err_octets;
	volatile TCC_DEF32BIT_TYPE rxicmp_gd_octets;
	volatile TCC_DEF32BIT_TYPE rxicmp_err_octets;

	TCC_DEF32BIT_TYPE NOTDEF0[286];

	volatile TCC_DEF32BIT_TYPE TSC;
	volatile TCC_DEF32BIT_TYPE SSI;
	volatile TCC_DEF32BIT_TYPE TSH;
	volatile TCC_DEF32BIT_TYPE TSL;
	volatile TCC_DEF32BIT_TYPE TSHU;
	volatile TCC_DEF32BIT_TYPE TSLU;
	volatile TCC_DEF32BIT_TYPE TSA;
	volatile TCC_DEF32BIT_TYPE TTH;
	volatile TCC_DEF32BIT_TYPE TTL;
	volatile TCC_DEF32BIT_TYPE NOTDEF1[55];
	volatile TCC_DEF32BIT_TYPE MACA16H;
	volatile TCC_DEF32BIT_TYPE MACA16L;
	volatile TCC_DEF32BIT_TYPE MACA17H;
	volatile TCC_DEF32BIT_TYPE MACA17L;
	volatile TCC_DEF32BIT_TYPE MACA18H;
	volatile TCC_DEF32BIT_TYPE MACA18L;
	volatile TCC_DEF32BIT_TYPE MACA19H;
	volatile TCC_DEF32BIT_TYPE MACA19L;
	volatile TCC_DEF32BIT_TYPE MACA20H;
	volatile TCC_DEF32BIT_TYPE MACA20L;
	volatile TCC_DEF32BIT_TYPE MACA21H;
	volatile TCC_DEF32BIT_TYPE MACA21L;
	volatile TCC_DEF32BIT_TYPE MACA22H;
	volatile TCC_DEF32BIT_TYPE MACA22L;
	volatile TCC_DEF32BIT_TYPE MACA23H;
	volatile TCC_DEF32BIT_TYPE MACA23L;
	volatile TCC_DEF32BIT_TYPE MACA24H;
	volatile TCC_DEF32BIT_TYPE MACA24L;
	volatile TCC_DEF32BIT_TYPE MACA25H;
	volatile TCC_DEF32BIT_TYPE MACA25L;
	volatile TCC_DEF32BIT_TYPE MACA26H;
	volatile TCC_DEF32BIT_TYPE MACA26L;
	volatile TCC_DEF32BIT_TYPE MACA27H;
	volatile TCC_DEF32BIT_TYPE MACA27L;
	volatile TCC_DEF32BIT_TYPE MACA28H;
	volatile TCC_DEF32BIT_TYPE MACA28L;
	volatile TCC_DEF32BIT_TYPE MACA29H;
	volatile TCC_DEF32BIT_TYPE MACA29L;
	volatile TCC_DEF32BIT_TYPE MACA30H;
	volatile TCC_DEF32BIT_TYPE MACA30L;
	volatile TCC_DEF32BIT_TYPE MACA31H;
	volatile TCC_DEF32BIT_TYPE MACA31L;
} GMAC, *PGMAC;

typedef struct _GMACDLY {
	volatile union {
		unsigned int bREG;
		struct {
			unsigned TXCLKIDLY:5;
			unsigned:2;
			unsigned TXCLKIINV:1;
			unsigned TXCLKODLY:5;
			unsigned:2;
			unsigned TXCLKOINV:1;
			unsigned TXENDLY:5;
			unsigned:3;
			unsigned TXERDLY:5;
			unsigned:3;
		} nREG;
	} DLY0;
	volatile union {
		unsigned int bREG;
		struct {
			unsigned TXD0DLY:5;
			unsigned:3;
			unsigned TXD1DLY:5;
			unsigned:3;
			unsigned TXD2DLY:5;
			unsigned:3;
			unsigned TXD3DLY:5;
			unsigned:3;
		} nREG;
	} DLY1;
	volatile union {
		unsigned int bREG;
		struct {
			unsigned TXD4DLY:5;
			unsigned:3;
			unsigned TXD5DLY:5;
			unsigned:3;
			unsigned TXD6DLY:5;
			unsigned:3;
			unsigned TXD7DLY:5;
			unsigned:3;
		} nREG;
	} DLY2;
	volatile union {
		unsigned int bREG;
		struct {
			unsigned RXCLKIDLY:5;
			unsigned:2;
			unsigned RXCLKIINV:1;
			unsigned:8;
			unsigned RXDVDLY:5;
			unsigned:3;
			unsigned RXERDLY:5;
			unsigned:3;
		} nREG;
	} DLY3;
	volatile union {
		unsigned int bREG;
		struct {
			unsigned RXD0DLY:5;
			unsigned:3;
			unsigned RXD1DLY:5;
			unsigned:3;
			unsigned RXD2DLY:5;
			unsigned:3;
			unsigned RXD3DLY:5;
			unsigned:3;
		} nREG;
	} DLY4;
	volatile union {
		unsigned int bREG;
		struct {
			unsigned RXD4DLY:5;
			unsigned:3;
			unsigned RXD5DLY:5;
			unsigned:3;
			unsigned RXD6DLY:5;
			unsigned:3;
			unsigned RXD7DLY:5;
			unsigned:3;
		} nREG;
	} DLY5;
	volatile union {
		unsigned int bREG;
		struct {
			unsigned COLDLY:5;
			unsigned:3;
			unsigned CRSDLY:5;
			unsigned:19;
		} nREG;
	} DLY6;
} GMACDLY, *PGMACDLY;

/* Global setting */
#if defined(CONFIG_ARCH_TCC898X)
#define TCC_PA_HSIOBUSCFG   0x111A0000
#define TCC_PA_GPIO     0x14200000
#elif defined(CONFIG_ARCH_TCC802X)
#define TCC_PA_HSIOBUSCFG   0x119A0000
#define TCC_PA_GPIO     0x14200000
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||\
	defined(CONFIG_ARCH_TCC901X) || defined(CONFIG_ARCH_TCC805X)
#define TCC_PA_HSIOBUSCFG   0x11DA0000
#define TCC_PA_GPIO     0x14200000
#else
#endif

#define Hw31		0x80000000
#define Hw30		0x40000000
#define Hw29		0x20000000
#define Hw28		0x10000000
#define Hw27		0x08000000
#define Hw26		0x04000000
#define Hw25		0x02000000
#define Hw24		0x01000000
#define Hw23		0x00800000
#define Hw22		0x00400000
#define Hw21		0x00200000
#define Hw20		0x00100000
#define Hw19		0x00080000
#define Hw18		0x00040000
#define Hw17		0x00020000
#define Hw16		0x00010000
#define Hw15		0x00008000
#define Hw14		0x00004000
#define Hw13		0x00002000
#define Hw12		0x00001000
#define Hw11		0x00000800
#define Hw10		0x00000400
#define Hw9		0x00000200
#define Hw8		0x00000100
#define Hw7		0x00000080
#define Hw6		0x00000040
#define Hw5		0x00000020
#define Hw4		0x00000010
#define Hw3		0x00000008
#define Hw2		0x00000004
#define Hw1		0x00000002
#define Hw0		0x00000001
#define HwZERO		0x00000000

#define ENABLE		1
#define DISABLE		0

#define ON		1
#define OFF		0

#define FALSE		0
#define TRUE		1

#define BITSET(X, MASK)			((X) |= (unsigned int)(MASK))
#define BITSCLR(X, SMASK, CMASK)	((X) = ((((unsigned int)(X)) |\
				((unsigned int)(SMASK))) & \
			~((unsigned int)(CMASK))))
#define BITCSET(X, CMASK, SMASK)	((X) = ((((unsigned int)(X)) &\
				~((unsigned int)(CMASK))) |\
			((unsigned int)(SMASK))))
#define BITCLR(X, MASK)			((X) &= ~((unsigned int)(MASK)))
#define BITXOR(X, MASK)			((X) ^= (unsigned int)(MASK))
#define ISZERO(X, MASK)			(!(((unsigned int)(X))&\
			((unsigned int)(MASK))))
#define	ISSET(X, MASK)			((unsigned long)(X)&\
		((unsigned long)(MASK)))

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC802X)
#define IO_OFFSET       0xE1000000
#elif defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
#define IO_OFFSET       0x81000000
#else
#endif
#define io_p2v(pa)      ((pa) + IO_OFFSET)
#define tcc_p2v(pa)     io_p2v(pa)

/* Original Code */
#define ECID0_OFFSET		(0x290)
#define ECID1_OFFSET		(0x294)
#define ECID2_OFFSET		(0x298)
#define ECID3_OFFSET		(0x29C)

#define GMACDLY0_OFFSET		(0x2000)
#define GMACDLY1_OFFSET		(0x2004)
#define GMACDLY2_OFFSET		(0x2008)
#define GMACDLY3_OFFSET		(0x200C)
#define GMACDLY4_OFFSET		(0x2010)
#define GMACDLY5_OFFSET		(0x2014)
#define GMACDLY6_OFFSET		(0x2018)

#define GMAC0_CFG0_OFFSET	(0x0060)
#define GMAC0_CFG1_OFFSET	(0x0064)
#define GMAC0_CFG2_OFFSET	(0x0054)
#define GMAC0_CFG3_OFFSET	(0x0058)
#define GMAC0_CFG4_OFFSET	(0x005C)

#define GMAC1_CFG0_OFFSET	(0x0068)
#define GMAC1_CFG1_OFFSET	(0x006C)
#define GMAC1_CFG2_OFFSET	(0x0070)
#define GMAC1_CFG3_OFFSET	(0x0074)
#define GMAC1_CFG4_OFFSET	(0x0078)

// GMAC CFG0
#define CFG0_TR					((unsigned int)1<<\
		(unsigned int)31)
#define CFG0_TXDIV_SHIFT		(19)
#define CFG0_TXDIV_MASK			((unsigned int)0x7f<<\
		(unsigned int)CFG0_TXDIV_SHIFT)
#define CFG0_TXCLK_SEL_SHIFT	(16)
#define CFG0_TXCLK_SEL_MASK		((unsigned int)0x3<<\
		(unsigned int)CFG0_TXCLK_SEL_SHIFT)
#define CFG0_RR					((unsigned int)1<<\
		(unsigned int)15)
#define CFG0_RXDIV_SHIFT		(4)
#define CFG0_RXDIV_MASK			((unsigned int)0x3f<<\
		(unsigned int)CFG0_RXDIV_SHIFT)
#define CFG0_RXCLK_SEL_SHIFT	(0)
#define CFG0_RXCLK_SEL_MASK		((unsigned int)0x3<<\
		(unsigned int)CFG0_RXCLK_SEL_SHIFT)

// GMAC CFG1
#define CFG1_CE					((unsigned int)1<<\
		(unsigned int)31)
#define CFG1_CC					((unsigned int)1<<\
		(unsigned int)30)
#define CFG1_PHY_INFSEL_SHIFT	(18)
#define CFG1_PHY_INFSEL_MASK	((unsigned int)((unsigned int)0x7<<\
			(unsigned int)CFG1_PHY_INFSEL_SHIFT))
#define CFG1_FCTRL				((unsigned int)1<<\
	(unsigned int)17)
#define CFG1_TCO				((unsigned int)1<<\
		(unsigned int)16)

// GMAC CFG2
#define GPI_SHIFT				(0)
#define GPO_SHIFT				(4)
#define PTP_AUX_TS_TRIG_SHIFT	(8)
#define PTP_PPS_TRIG_SHIFT		(12)

struct gmac_dt_info_t {
	unsigned int index;
	phy_interface_t phy_inf;
	struct clk *hsio_clk;
	struct clk *gmac_clk;
	struct clk *ptp_clk;
	struct clk *gmac_hclk;
	unsigned int phy_on;
	unsigned int phy_rst;
	u32 txclk_i_dly;
	u32 txclk_i_inv;
	u32 txclk_o_dly;
	u32 txclk_o_inv;
	u32 txen_dly;
	u32 txer_dly;
	u32 txd0_dly;
	u32 txd1_dly;
	u32 txd2_dly;
	u32 txd3_dly;
	u32 txd4_dly;
	u32 txd5_dly;
	u32 txd6_dly;
	u32 txd7_dly;
	u32 rxclk_i_dly;
	u32 rxclk_i_inv;
	u32 rxdv_dly;
	u32 rxer_dly;
	u32 rxd0_dly;
	u32 rxd1_dly;
	u32 rxd2_dly;
	u32 rxd3_dly;
	u32 rxd4_dly;
	u32 rxd5_dly;
	u32 rxd6_dly;
	u32 rxd7_dly;
	u32 crs_dly;
	u32 col_dly;
};

int tca_gmac_init(struct device_node *np, struct gmac_dt_info_t *dt_info);
void tca_gmac_clk_enable(struct gmac_dt_info_t *dt_info);
void tca_gmac_clk_disable(struct gmac_dt_info_t *dt_info);
unsigned long tca_gmac_get_hsio_clk(struct gmac_dt_info_t *dt_info);
phy_interface_t tca_gmac_get_phy_interface(struct gmac_dt_info_t *dt_info);
void tca_gmac_phy_pwr_on(struct gmac_dt_info_t *dt_info);
void tca_gmac_phy_pwr_off(struct gmac_dt_info_t *dt_info);
void tca_gmac_phy_reset(struct gmac_dt_info_t *dt_info);
void tca_gmac_tunning_timing(struct gmac_dt_info_t *dt_info,
			     void __iomem *ioaddr);
void tca_gmac_portinit(struct gmac_dt_info_t *dt_info, void __iomem *ioaddr);
void IO_UTIL_ReadECID(unsigned int ecid[]);
int tca_get_mac_addr_from_ecid(unsigned char *mac_addr);

#endif /*_TCA_GMAC_H_*/
