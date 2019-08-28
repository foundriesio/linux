
#ifndef TCC_CKC_REG_H
#define TCC_CKC_REG_H

/*
 * CKC Register Offsets
 */
#define CKC_CLKCTRL	0x000
#define CKC_PLLPMS	0x040
#define CKC_PLLCON	0x060
#define CKC_PLLMON	0x080
#define CKC_CLKDIVC	0x0A0
#define CKC_SWRESET	0x0B0
#define CKC_PCLKCTRL	0x0D0

// Dedicated CLKCTRL Register Offsets
#define DCKC_CLKCTRL	0x000
#define DCKC_CLKCTRL2	0x004
#define DCKC_PLLPMS	0x008
#define DCKC_PLLCON	0x00C
#define DCKC_PLLMON	0x010
#define DCKC_PLLDIVC	0x014

// Video Dedicated CLKCTRL Register Offsets
#define VCKC_VBUS	0x000
#define VCKC_BODA	0x004
#define VCKC_BHEVC	0x008
#define VCKC_CHEVC	0x00c
#define VCKC_PLL0PMS	0x010
#define VCKC_PLL0CON	0x014
#define VCKC_PLL0MON	0x018
#define VCKC_PLL1PMS	0x020
#define VCKC_PLL1CON	0x024
#define VCKC_PLL1MON	0x028
#define VCKC_PLLDIVC	0x030
#define VCKC_PLL0DIV0_OFFSET 0
#define VCKC_PLL0DIV1_OFFSET 8
#define VCKC_PLL1DIV0_OFFSET 16
#define VCKC_PLL1DIV1_OFFSET 24

// I/O Bus configuration Register Offsets
#define IOBUS_PWDN0	0x000000
#define IOBUS_PWDN1	0x000004
#define IOBUS_PWDN2	0x000008
#define IOBUS_PWDN3	0x000000  /* base: 0x16494400 */
#define IOBUS_RESET0	0x00000C
#define IOBUS_RESET1	0x000010
#define IOBUS_RESET2	0x000014
#define IOBUS_RESET3	0x000004  /* base: 0x16494404 */

// HSIO Bus configuration Register Offsets 
#define HSIOBUS_PWDN	0x000000
#define HSIOBUS_RESET	0x000004

/*
 * CLKCTRL Configuration Register
 */
#define CLKCTRL_SEL_MIN		0
#define CLKCTRL_SEL_MAX		15
#define CLKCTRL_SEL_SHIFT	0
#define CLKCTRL_SEL_MASK	0xF
#define CLKCTRL_CONFIG_MIN	1
#define CLKCTRL_CONFIG_MAX	15
#define CLKCTRL_CONFIG_SHIFT	5
#define CLKCTRL_CONFIG_MASK	0xF
#define CLKCTRL_EN_SHIFT	22
#define CLKCTRL_CFGRQ_SHIFT	29
#define CLKCTRL_SYNRQ_SHIFT	30
#define CLKCTRL_CHGRQ_SHIFT	31

enum { /* CLKCTRL SEL */
	CLKCTRL_SEL_PLL0=0,
	CLKCTRL_SEL_PLL1,
	CLKCTRL_SEL_PLL2,
	CLKCTRL_SEL_PLL3,
	CLKCTRL_SEL_PLL4,
	CLKCTRL_SEL_XIN,	// 5
	CLKCTRL_SEL_XTIN,
	CLKCTRL_SEL_PLL0DIV,
	CLKCTRL_SEL_PLL1DIV,
	CLKCTRL_SEL_PLL2DIV,
	CLKCTRL_SEL_PLL3DIV,	// 10
	CLKCTRL_SEL_PLL4DIV,
	CLKCTRL_SEL_XINDIV,
	CLKCTRL_SEL_XTINDIV,
	CLKCTRL_SEL_EXTIN2,
	CLKCTRL_SEL_EXTIN3	// 15
};

/*
 * Dedicated CLKCTRL Configuration Register
 * (CPUQ, CPUS, VBUS, HEVC, CODA, G3D)
 */
#define DCLKCTRL_SEL_MIN	0
#define DCLKCTRL_SEL_MAX	3
#define DCLKCTRL_SEL_SHIFT	0
#define DCLKCTRL_SEL_MASK	0x3
#define DCLKCTRL_XTIN_SEL_SHIFT	2
#define DCLKCTRL_SYNRQ_SHIFT	30
#define DCLKCTRL_CHGRQ_SHIFT	31
#define DCLKCTRL_CLKDIVC_MAX	0x3F
#define DCLKCTRL_CLKDIVC_MIN	0

/* CLKCTRL2 SEL */
enum {
	DCLKCTRL_SEL_XIN=0,
	DCLKCTRL_SEL_PLL,
	DCLKCTRL_SEL_PLLDIV,
	DCLKCTRL_SEL_XTIN,
};

enum {
	VCLKCTRL_SEL_XIN=0,
	VCLKCTRL_SEL_PLL0,
	VCLKCTRL_SEL_PLL1,
	VCLKCTRL_SEL_PLL0_DIV0,
	VCLKCTRL_SEL_PLL0_DIV1,
	VCLKCTRL_SEL_PLL1_DIV0,
	VCLKCTRL_SEL_PLL1_DIV1,
	VCLKCTRL_SEL_XTIN,
	VCLKCTRL_SEL_MAX,
};

/*
 * PLL Configuration Register
 */
#define PLL_MAX_RATE		(3200*1000*1000UL)	// Max. 3200MHz
#define PLL_MIN_RATE		(25*1000*1000UL)	// Min.   25MHz
#define PLL_VCO_MAX		(3200*1000*1000UL)	// Max. 3200MHz
#define PLL_VCO_MIN		(1600*1000*1000UL)	// Min. 1600MHz
#define PLL_P_MAX		6	// 63	FREF = FIN/p  (4MHz ~ 12MHz)
#define PLL_P_MIN		2	// 1	FREF = FIN/p  (4MHz ~ 12MHz)
#define PLL_P_SHIFT		0
#define PLL_P_MASK		0x3F
#define PLL_M_MAX		1023
#define PLL_M_MIN		64
#define PLL_M_SHIFT		6
#define PLL_M_MASK		0x3FF
#define PLL_S_MAX		6
#define PLL_S_MIN		0
#define PLL_S_SHIFT		16
#define PLL_S_MASK		0x7
#define PLL_SRC_SHIFT		19
#define PLL_SRC_MASK		0x3
#define PLL_BYPASS_SHIFT	21
#define PLL_LOCKST_SHIFT	23
#define PLL_CHGPUMP_SHIFT	24
#define PLL_CHGPUMP_MASK	0x3
#define PLL_LOCKEN_SHIFT	26
#define PLL_RSEL_SHIFT		27
#define PLL_RSEL_MASK		0xF
#define PLL_EN_SHIFT		31

/*
 * DPLL Configuration Register
 */

// DPLLPMS Register
#define DPLL_MAX_RATE		(3200*1000*1000UL)	// Max. 3200MHz
#define DPLL_MIN_RATE		(25*1000*1000UL)	// Min.   25MHz
#define DPLL_VCO_MAX		(3200*1000*1000UL)	// Max. 3200MHz
#define DPLL_VCO_MIN		(1600*1000*1000UL)	// Min. 1600MHz
#define DPLL_P_MAX		4	// 63	FREF = FIN/p  (6MHz ~ 12MHz)
#define DPLL_P_MIN		1	// 1	FREF = FIN/p  (6MHz ~ 12MHz)
#define DPLL_P_SHIFT		0
#define DPLL_P_MASK		0x3F
#define DPLL_M_MAX		511
#define DPLL_M_MIN		16
#define DPLL_M_SHIFT		6
#define DPLL_M_MASK		0x1FF
#define DPLL_S_MAX		5
#define DPLL_S_MIN		0
#define DPLL_S_SHIFT		16
#define DPLL_S_MASK		0x7
#define DPLL_SRC_SHIFT		19
#define DPLL_SRC_MASK		0x3
#define DPLL_BYPASS_SHIFT	21
#define DPLL_CHGPUMP_SHIFT	24
#define DPLL_CHGPUMP_MASK	0x3
#define DPLL_SSCG_EN_SHIFT	28
#define DPLL_SEL_PF_SHIFT	29
#define DPLL_SEL_PF_MASK	0x3
#define DPLL_SEL_DOWN	0
#define DPLL_SEL_UP	1
#define DPLL_SEL_CENTER 2
#define DPLL_EN_SHIFT		31

// DPLLCON Register
#define DPLL_K_SHIFT	16
#define DPLL_K_MASK	0xFFFF
#define DPLL_MFR_SHIFT	8
#define DPLL_MFR_MASK	0xFF
#define DPLL_MRR_SHIFT	0
#define DPLL_MRR_MASK	0x3F

// DPLLMON register
#define DPLL_LOCKEN_SHIFT	31
#define DPLL_LOCKST_SHIFT	30

/* PLL Clock Source */
enum{
	PLLSRC_XIN=0,
	PLLSRC_HDMIXI,
	PLLSRC_EXTCLK0,
	PLLSRC_EXTCLK1,
	PLLSRC_MAX
};

/*
 * PCLKCTRL Configuration Register
 */
#define PCLKCTRL_MAX_FCKS	(1600*1000*1000)
#define PCLKCTRL_DIV_MIN	0
#define PCLKCTRL_DIV_DCO_MIN	1
#define PCLKCTRL_DIV_SHIFT	0
#define PCLKCTRL_DIV_XXX_MAX	0xFFF
#define PCLKCTRL_DIV_XXX_MASK	PCLKCTRL_DIV_XXX_MAX
#define PCLKCTRL_DIV_YYY_MAX	0xFFFFFF
#define PCLKCTRL_DIV_YYY_MASK	PCLKCTRL_DIV_YYY_MAX
#define PCLKCTRL_SEL_MIN	0
#define PCLKCTRL_SEL_MAX	26
#define PCLKCTRL_SEL_SHIFT	24
#define PCLKCTRL_SEL_MASK	0x1F
#define PCLKCTRL_EN_SHIFT	29
#define PCLKCTRL_OUTEN_SHIFT	30
#define PCLKCTRL_MD_SHIFT	31

/* PCLK Type */
typedef enum {
	PCLKCTRL_TYPE_XXX=0,
	PCLKCTRL_TYPE_YYY,
	PCLKCTRL_TYPE_MAX
} tPCLKTYPE;

/* PCLK Mode Selection */
enum {
	PCLKCTRL_MODE_DCO=0,
	PCLKCTRL_MODE_DIVIDER,
	PCLKCTRL_MODE_MAX
};

/* Peri. Clock Source */
enum{
	PCLKCTRL_SEL_PLL0=0,
	PCLKCTRL_SEL_PLL1,
	PCLKCTRL_SEL_PLL2,
	PCLKCTRL_SEL_PLL3,
	PCLKCTRL_SEL_PLL4,
	PCLKCTRL_SEL_XIN,
	PCLKCTRL_SEL_XTIN,
	PCLKCTRL_SEL_PLL0DIV=10,
	PCLKCTRL_SEL_PLL1DIV,
	PCLKCTRL_SEL_PLL2DIV,
	PCLKCTRL_SEL_PLL3DIV,
	PCLKCTRL_SEL_PLL4DIV,
	PCLKCTRL_SEL_PCIPHY_CLKOUT=18,
	PCLKCTRL_SEL_HDMITMDS,
	PCLKCTRL_SEL_HDMIPCLK,
	PCLKCTRL_SEL_HDMIXIN,	// 27Mhz
	PCLKCTRL_SEL_XINDIV=23,
	PCLKCTRL_SEL_XTINDIV,
	PCLKCTRL_SEL_EXTCLK0,
	PCLKCTRL_SEL_EXTCLK1,
};

/*
 * Memory Bus Configuration Register
 */

// TODO : Need to Check here.

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

// Memory bus clock mask register 1
#define MBUSCLK_OUT_CTRL0         (1 << 0)
#define MBUSCLK_DDR_AXI0          (1 << 1)
#define MBUSCLK_DDR_PERI0         (1 << 2)
#define MBUSCLK_DMC_CH1           (1 << 3)
#define MBUSCLK_SCRBLR_CORE1      (1 << 4)
#define MBUSCLK_SCRBLR_PERI10     (1 << 5)
#define MBUSCLK_DMC_H2H_S1        (1 << 6)
#define MBUSCLK_DMC_H2H_M1        (1 << 7)
#define MBUSCLK_OUT_CTRL1         (1 << 8)
#define MBUSCLK_DDR_AXI1          (1 << 9)
#define MBUSCLK_DDR_PERI1         (1 << 10)
#define MBUSCLK_IMC_TZC_PERI      (1 << 11)
#define MBUSCLK_X2H_TZC_CORE      (1 << 12)
#define MBUSCLK_X2H               (1 << 13)
#define MBUSCLK_AHB_MUX           (1 << 14)
#define MBUSCLK_MISC              (1 << 15)
#define MBUSCLK_MMU               (1 << 16)
#define MBUSCLK_SECURE            (1 << 17)
#define MBUSCLK_TZC_PERI          (1 << 18)
#define MBUSCLK_CFG_CORE          (1 << 19)
#define MBUSCLK_CFG_REG           (1 << 20)
#define MBUSCLK_X2H_CKC           (1 << 21)
#define MBUSCLK_TZMA              (1 << 22)
#define MBUSCLK_TZB               (1 << 23)
#define MBUSCLK_TZB_CFG           (1 << 24)
#define MBUSCLK_NIC               (1 << 25)
#define MBUSCLK_IMC_CORE          (1 << 26)
#define MBUSCLK_IMC_TZC_CORE      (1 << 27)
#define MBUSCLK_IMC_IDF           (1 << 28)
#define MBUSCLK_DDRPHY0           (1 << 29)
#define MBUSCLK_DDRPHY1           (1 << 30)

#endif
