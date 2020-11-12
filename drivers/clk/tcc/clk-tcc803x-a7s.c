// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/clocksource.h>
#include <linux/of_address.h>
#include <linux/clk/tcc.h>
#include <linux/clk-provider.h>
#include <linux/irqflags.h>
#include <linux/slab.h>
#if !defined(CONFIG_ARM64_TCC_BUILD)
#include <asm/system_info.h>
#endif

#define TCC803X_CKC_DRIVER
#include "clk-tcc803x.h"
#include "clk-tcc803x-a7s-reg.h"

/**********************************
 *  Pre Defines
 **********************************/
#define ckc_writel	__raw_writel
#define ckc_readl	__raw_readl

#define MEMBUS_PLL	4

#define MAX_TCC_PLL	5
#define MAX_CLK_SRC	(MAX_TCC_PLL*2 + 2)	// XIN, XTIN

#define SWRESET_CA7SP   (1 << 17)

static void __iomem *ckc_base;
static void __iomem *pmu_base;

static void __iomem *membus_cfg_base;
static void __iomem *ddibus_cfg_base;
static void __iomem *iobus_cfg_base;
static void __iomem *iobus_cfg_base1;
static void __iomem *vpubus_cfg_base;
static void __iomem *hsiobus_cfg_base;
static void __iomem *cpubus_cfg_base;
static void __iomem *cpu0_base;
static void __iomem *cpu1_base;
static void __iomem *gpu_3d_base;
static void __iomem *gpu_2d_base;
static void __iomem *vbusckc_base;
static void __iomem *mem_ckc_base;

static unsigned int stClockSource[MAX_CLK_SRC];
static uint32_t stVbusClkSource[VCLKCTRL_SEL_MAX];

static unsigned int stClockG3D;

static struct tcc_ckc_ops tcc803x_ops;
static inline void tcc_ckc_reset_clock_source(int id);

/* PLL Configuration Macro */
#define tcc_pll_write(reg, en, p, m, s, src) { \
	if (en) { \
		volatile unsigned int i; \
		ckc_writel((ckc_readl(reg) & \
			(PLL_CHGPUMP_MASK<<PLL_CHGPUMP_SHIFT))| \
			(1<<PLL_LOCKEN_SHIFT)| \
			((src&PLL_SRC_MASK)<<PLL_SRC_SHIFT)| \
			((s&PLL_S_MASK)<<PLL_S_SHIFT)| \
			((m&PLL_M_MASK)<<PLL_M_SHIFT)| \
			((p&PLL_P_MASK)<<PLL_P_SHIFT), reg); \
		/* need to delay at least 1us. */ \
		for (i = 100; i > 0; i--)\
			;/* if cpu clock is 1GHz then loop 100. */ \
		ckc_writel(ckc_readl(reg) | ((en&1)<<PLL_EN_SHIFT), reg); \
		while ((ckc_readl(reg)&(1<<PLL_LOCKST_SHIFT)) == 0) \
			; \
	} else \
		ckc_writel(ckc_readl(reg) & ~(1<<PLL_EN_SHIFT), reg); \
}

/* Dedicated PLL Configuration Macro (cpu0/cpu1/gpu3d/vbus/hevc/coda/mem) */
#define tcc_dckc_pll_write(reg, en, p, m, s) { \
	if (en) { \
		volatile unsigned int i; \
		ckc_writel((ckc_readl(reg) & \
			(PLL_CHGPUMP_MASK << PLL_CHGPUMP_SHIFT))| \
			(1<<PLL_LOCKEN_SHIFT)| \
			((s&PLL_S_MASK)<<PLL_S_SHIFT)| \
			((m&PLL_M_MASK)<<PLL_M_SHIFT)| \
			((p&PLL_P_MASK)<<PLL_P_SHIFT), reg); \
		/* need to delay at least 1us. */ \
		if (reg == cpu0_base+DCKC_PLLPMS) \
			; /* no need to delay. cpu clock is XIN(24MHz). */ \
		else \
			for (i = 100; i > 0; i--) \
				;/* if cpu clock is 1GHz then loop 100. */ \
		ckc_writel(ckc_readl(reg) | ((en&1)<<PLL_EN_SHIFT), reg); \
		while ((ckc_readl(reg)&(1<<PLL_LOCKST_SHIFT)) == 0) \
			; \
	} else { \
		ckc_writel(ckc_readl(reg) & ~(1<<PLL_EN_SHIFT), reg); \
	} \
}

/* Dedicated CLKCTRL Configuration Macro (CPU0/CPU1/GPU3D/HEVC/CODA/MEM) */
#define tcc_dclkctrl_write(reg, sel) {\
	ckc_writel((sel&DCLKCTRL_SEL_MASK)<<DCLKCTRL_SEL_SHIFT, reg);\
	while (ckc_readl(reg) & (1<<DCLKCTRL_CHGRQ_SHIFT))\
		;\
}

#define tcc_vclkctrl_write(reg, sel) {\
	ckc_writel((sel&VCLKCTRL_SEL_MASK)<<VCLKCTRL_SEL_SHIFT, reg);\
	while (ckc_readl(reg) & (1<<VCLKCTRL_CHGRQ_SHIFT))\
		;\
}

static void tcc_dpll_write(int id, struct tDPMS *dpms)
{
	void __iomem *pms_reg = ckc_base + CKC_PLLPMS + (id * 4);
	void __iomem *dcon_reg = ckc_base + CKC_PLLCON + (id * 4);
	void __iomem *dmon_reg = ckc_base + CKC_PLLMON + (id * 4);

	if (dpms->en == 1) {
		ckc_writel(0, pms_reg);
		// Configuration DPLLPMS Register
		ckc_writel((dpms->p << DPLL_P_SHIFT) |
			   (dpms->m << DPLL_M_SHIFT) |
			   (dpms->s << DPLL_S_SHIFT) |
			   (dpms->sscg_en << DPLL_SSCG_EN_SHIFT) |
			   (dpms->sel_pf << DPLL_SEL_PF_SHIFT) |
			   (dpms->src << DPLL_SRC_SHIFT), pms_reg);

		// Configuration DPLLCON Register for dithered mode
		if (dpms->sscg_en == 1) {
			ckc_writel((dpms->k << DPLL_K_SHIFT) |
				   (dpms->mfr << DPLL_MFR_SHIFT) |
				   (dpms->mrr << DPLL_MRR_SHIFT), dcon_reg);
		}
		ckc_writel(ckc_readl(dmon_reg) | (1 << DPLL_LOCKEN_SHIFT),
			   dmon_reg);
		ckc_writel(ckc_readl(pms_reg) | (dpms->en << DPLL_EN_SHIFT),
			   pms_reg);

		// Waiting Lock
		while ((ckc_readl(dmon_reg) & (1 << DPLL_LOCKST_SHIFT)) == 0)
			;
	} else {
		ckc_writel(ckc_readl(pms_reg) &
			   ~((dpms->en & 1) << DPLL_EN_SHIFT), pms_reg);
	}
}

static inline void tcc_pclkctrl_write(void __iomem *reg, unsigned int md,
				      unsigned int en, unsigned int sel,
				      unsigned int div, unsigned int type)
{
	if (type == PCLKCTRL_TYPE_XXX) {
		ckc_writel(ckc_readl(reg) & ~(1 << PCLKCTRL_OUTEN_SHIFT), reg);
		/*
		 * if (system_rev >= 0x0002)
		 * while(ckc_readl(reg) & (1 << 31));
		 */
		ckc_writel(ckc_readl(reg) & ~(1 << PCLKCTRL_EN_SHIFT), reg);
		ckc_writel((ckc_readl(reg) &
			    ~(PCLKCTRL_DIV_XXX_MASK << PCLKCTRL_DIV_SHIFT)) |
			   ((div & PCLKCTRL_DIV_XXX_MASK) <<
			    PCLKCTRL_DIV_SHIFT), reg);
		ckc_writel((ckc_readl(reg) &
			   ~(PCLKCTRL_SEL_MASK << PCLKCTRL_SEL_SHIFT)) |
			   ((sel & PCLKCTRL_SEL_MASK) << PCLKCTRL_SEL_SHIFT),
			   reg);
		ckc_writel((ckc_readl(reg) & ~(1 << PCLKCTRL_EN_SHIFT))
			   | ((en & 1) << PCLKCTRL_EN_SHIFT), reg);
		ckc_writel((ckc_readl(reg) & ~(1 << PCLKCTRL_OUTEN_SHIFT))
			   | ((en & 1) << PCLKCTRL_OUTEN_SHIFT), reg);
	} else if (type == PCLKCTRL_TYPE_YYY) {
		ckc_writel(ckc_readl(reg) & ~(1 << PCLKCTRL_EN_SHIFT), reg);
		ckc_writel((ckc_readl(reg) &
			    ~(PCLKCTRL_DIV_YYY_MASK << PCLKCTRL_DIV_SHIFT)) |
			   ((div & PCLKCTRL_DIV_YYY_MASK) <<
			    PCLKCTRL_DIV_SHIFT), reg);
		ckc_writel((ckc_readl(reg) &
			    ~(PCLKCTRL_SEL_MASK << PCLKCTRL_SEL_SHIFT)) |
			   ((sel & PCLKCTRL_SEL_MASK) << PCLKCTRL_SEL_SHIFT),
			   reg);
		ckc_writel((ckc_readl(reg) & ~(1 << PCLKCTRL_MD_SHIFT)) |
			   ((md & 1) << PCLKCTRL_MD_SHIFT), reg);
		ckc_writel((ckc_readl(reg) & ~(1 << PCLKCTRL_EN_SHIFT))
			   | ((en & 1) << PCLKCTRL_EN_SHIFT), reg);
	}
}

static inline void tcc_clkctrl_write(void __iomem *reg, unsigned int en,
				     unsigned int config, unsigned int sel)
{
	unsigned int cur_config;

	cur_config =
	    (ckc_readl(reg) >> CLKCTRL_CONFIG_SHIFT) & CLKCTRL_CONFIG_MASK;

	if (config >= cur_config) {
		ckc_writel((ckc_readl(reg) &
			    (~(CLKCTRL_CONFIG_MASK << CLKCTRL_CONFIG_SHIFT)))
			   | ((config & CLKCTRL_CONFIG_MASK) <<
			      CLKCTRL_CONFIG_SHIFT), reg);
		while (ckc_readl(reg) & (1 << CLKCTRL_CFGRQ_SHIFT))
			;
		ckc_writel((ckc_readl(reg) &
			    (~(CLKCTRL_SEL_MASK << CLKCTRL_SEL_SHIFT)))
			   | ((sel & CLKCTRL_SEL_MASK) << CLKCTRL_SEL_SHIFT),
			   reg);
		while (ckc_readl(reg) & (1 << CLKCTRL_CHGRQ_SHIFT))
			;
	} else {
		ckc_writel((ckc_readl(reg) &
			    (~(CLKCTRL_SEL_MASK << CLKCTRL_SEL_SHIFT)))
			   | ((sel & CLKCTRL_SEL_MASK) << CLKCTRL_SEL_SHIFT),
			   reg);
		while (ckc_readl(reg) & (1 << CLKCTRL_CHGRQ_SHIFT))
			;
		ckc_writel((ckc_readl(reg) &
			    (~(CLKCTRL_CONFIG_MASK << CLKCTRL_CONFIG_SHIFT)))
			   | ((config & CLKCTRL_CONFIG_MASK) <<
			      CLKCTRL_CONFIG_SHIFT), reg);
		while (ckc_readl(reg) & (1 << CLKCTRL_CFGRQ_SHIFT))
			;
	}
	ckc_writel((ckc_readl(reg) & (~(1 << CLKCTRL_EN_SHIFT)))
		   | ((en & 1) << CLKCTRL_EN_SHIFT), reg);
	while (ckc_readl(reg) & (1 << CLKCTRL_CFGRQ_SHIFT))
		;
}

static int tcc_find_pms_table(struct tPMSValue *table,
			      uint32_t rate,
			      uint32_t max)
{
	uint32_t i;

	for (i = 0; i < max; i++) {
		if (table[i].fpll == rate) {
			/* if table has request frequency, */
			return i;
		}
	}

	return -1;
}

static inline int tcc_find_pms(struct tPMS *PLL, unsigned int srcfreq)
{
	u64 u64_pll, u64_src, fvco, sch_p, sch_m, u64_tmp;
	unsigned int srch_pll, err, sch_err;
	int sch_s;

	if (PLL->fpll == 0) {
		PLL->en = 0;
		return 0;
	}

	u64_pll = (u64)PLL->fpll;
	u64_src = (u64)srcfreq;

	err = 0xFFFFFFFF;
	sch_err = 0xFFFFFFFF;

	for (sch_s = PLL_S_MAX, fvco = (u64_pll << PLL_S_MAX);
	     sch_s >= PLL_S_MIN; fvco = (u64_pll << (--sch_s))) {
		if (fvco >= PLL_VCO_MIN && fvco <= PLL_VCO_MAX) {
			for (sch_p = PLL_P_MIN; sch_p <= PLL_P_MAX; sch_p++) {
				sch_m = fvco * sch_p;
				do_div(sch_m, srcfreq);

				if ((sch_m < PLL_M_MIN) || (sch_m > PLL_M_MAX))
					continue;

				u64_tmp = sch_m * u64_src;
				do_div(u64_tmp, sch_p);
				srch_pll = (unsigned int)(u64_tmp >> sch_s);

				if ((srch_pll < PLL_MIN_RATE) ||
				    (srch_pll > PLL_MAX_RATE))
					continue;

				sch_err = (srch_pll > u64_pll) ?
					  (srch_pll - u64_pll) :
					  (u64_pll - srch_pll);
				if (sch_err < err) {
					err = sch_err;
					PLL->p = (unsigned int)sch_p;
					PLL->m = (unsigned int)sch_m;
					PLL->s = (unsigned int)sch_s;
				}
			}
		}
	}
	if (err == 0xFFFFFFFF)
		return -1;

	u64_tmp = u64_src * (unsigned long long)PLL->m;
	do_div(u64_tmp, PLL->p);
	PLL->fpll = (unsigned int)(u64_tmp >> PLL->s);
	PLL->en = 1;
	return 0;
}

static inline int tcc_find_dithered_pms(struct tDPMS *PLL, unsigned int srcfreq)
{
	u64 u64_pll, u64_src, fvco, srch_p, srch_m, u64_tmp;
	unsigned int srch_pll, err, srch_err;
	int srch_s;

	if (PLL->fpll == 0) {
		PLL->en = 0;
		return 0;
	}

	u64_pll = (u64) PLL->fpll;
	u64_src = (u64) srcfreq;

	err = 0xFFFFFFFF;
	srch_err = 0xFFFFFFFF;

	for (srch_s = DPLL_S_MAX, fvco = (u64_pll << DPLL_S_MAX);
	     srch_s >= DPLL_S_MIN; fvco = (u64_pll << (--srch_s))) {
		if ((fvco >= DPLL_VCO_MIN) && (fvco <= DPLL_VCO_MAX)) {
			for (srch_p = DPLL_P_MIN; srch_p <= DPLL_P_MAX;
					srch_p++) {
				srch_m = fvco * srch_p;
				do_div(srch_m, srcfreq);

				if ((srch_m < DPLL_M_MIN) ||
				    (srch_m > DPLL_M_MAX))
					continue;

				u64_tmp = srch_m * u64_src;
				do_div(u64_tmp, srch_p);
				srch_pll = (unsigned int)(u64_tmp >> srch_s);

				if ((srch_pll < DPLL_MIN_RATE) ||
				    (srch_pll > DPLL_MAX_RATE))
					continue;

				srch_err = (srch_pll > u64_pll) ?
					   (srch_pll - u64_pll) :
					   (u64_pll - srch_pll);

				if (srch_err < err) {
					err = srch_err;
					PLL->p = (unsigned int)srch_p;
					PLL->m = (unsigned int)srch_m;
					PLL->s = (unsigned int)srch_s;
				}
			}
		}
	}
	if (err == 0xFFFFFFFF)
		return -1;

	u64_tmp = u64_src * (unsigned long long)PLL->m;
	do_div(u64_tmp, PLL->p);
	PLL->fpll = (unsigned int)(u64_tmp >> PLL->s);
	PLL->en = 1;

	return 0;
}

static int tcc_ckc_dedicated_pll_set_rate(void __iomem *reg,
					  unsigned long rate)
{
	struct tPMS nPLL;

	nPLL.fpll = rate;
	if (tcc_find_pms(&nPLL, XIN_CLK_RATE))
		goto tcc_ckc_setpll2_failed;
	tcc_dckc_pll_write(reg, nPLL.en, nPLL.p, nPLL.m, nPLL.s);
	return 0;

tcc_ckc_setpll2_failed:
	tcc_dckc_pll_write(reg, 0, PLL_P_MIN,
			   ((PLL_P_MIN * PLL_VCO_MIN) +
			    XIN_CLK_RATE) / XIN_CLK_RATE, PLL_S_MIN);
	return -1;
}

static unsigned long tcc_ckc_dedicated_pll_get_rate(void __iomem *reg)
{
	uint32_t reg_values = ckc_readl(reg);
	struct tPMS nPLLCFG;
	u64 u64_tmp;

	nPLLCFG.p = (reg_values >> PLL_P_SHIFT) & (PLL_P_MASK);
	nPLLCFG.m = (reg_values >> PLL_M_SHIFT) & (PLL_M_MASK);
	nPLLCFG.s = (reg_values >> PLL_S_SHIFT) & (PLL_S_MASK);
	nPLLCFG.en = (reg_values >> PLL_EN_SHIFT) & (1);
	nPLLCFG.src = (reg_values >> PLL_SRC_SHIFT) & (PLL_SRC_MASK);

	u64_tmp = (u64)XIN_CLK_RATE * (u64)nPLLCFG.m;
	do_div(u64_tmp, nPLLCFG.p);

	return (unsigned int)((u64_tmp) >> nPLLCFG.s);
}

static int tcc_ckc_dedicated_plldiv_set(int id, uint32_t div)
{
	void __iomem *reg;
	uint32_t reg_values = 0;

	switch (id) {
	case FBUS_CPU0:
		reg = cpu0_base + DCKC_PLLDIVC;
		break;
	case FBUS_CPU1:
		reg = cpu1_base + DCKC_PLLDIVC;
		break;
	default:
		return -1;
	}

	if (div)
		reg_values |= ((1 << 7) | (div & 0x3F));
	else
		reg_values |= 1;

	ckc_writel(reg_values, reg);

	return 0;
}

static int tcc_ckc_dedicated_plldiv_get(int id)
{
	void __iomem *reg;
	uint32_t reg_values = 0;

	switch (id) {
	case FBUS_CPU0:
		reg = cpu0_base + DCKC_PLLDIVC;
		break;
	case FBUS_CPU1:
		reg = cpu1_base + DCKC_PLLDIVC;
		break;
	default:
		return -1;
	}
	reg_values = ckc_readl(reg) & 0x3F;

	return reg_values;
}

static int tcc_ckc_is_normal_pll(int id)
{
	int ret = 0;

	if (id >= MAX_TCC_PLL) {
		/* 'id' is beyond source clock candidate, */
		return -1;
	}

	switch (id) {
	case PLL_0:
	case PLL_1:
	case PLL_DIV_0:
	case PLL_DIV_1:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static int tcc_ckc_is_dpll_mode(int id)
{
	void __iomem *reg = NULL;
	int ret = 0;

	if (tcc_ckc_is_normal_pll(id) == 1) {
		/* if 'id' is normal pll, */
		return 0;
	}

	switch (id) {
	case PLL_2:
	case PLL_DIV_2:
		reg = ckc_base + CKC_PLLPMS + (PLL_2 * 4);
		break;
	case PLL_3:
	case PLL_DIV_3:
		reg = ckc_base + CKC_PLLPMS + (PLL_3 * 4);
		break;
	case PLL_4:
	case PLL_DIV_4:
		reg = ckc_base + CKC_PLLPMS + (PLL_4 * 4);
		break;
	default:
		ret = 0;
	}
	ret = ((ckc_readl(reg) & (1 << DPLL_SSCG_EN_SHIFT)) != 0) ? 1 : 0;

	return ret;
}

static int tcc_ckc_npll_set_val(int id, uint32_t val)
{
	void __iomem *reg = ckc_base + CKC_PLLPMS + (id * 4);
	struct tPMS nPLL;

	if (tcc_ckc_is_normal_pll(id) != 1) {
		/* if 'id' is normal pll, */
		return -1;
	}

	memset(&nPLL, 0x0, sizeof(struct tPMS));

	nPLL.p = (val >> PLL_P_SHIFT) & PLL_P_MASK;
	nPLL.m = (val >> PLL_M_SHIFT) & PLL_M_MASK;
	nPLL.s = (val >> PLL_S_SHIFT) & PLL_S_MASK;
	nPLL.en = 1;

	tcc_pll_write(reg, nPLL.en, nPLL.p, nPLL.m, nPLL.s, PLLSRC_XIN);
	tcc_ckc_reset_clock_source(id);

	return 0;
}

static int tcc_ckc_dpll_set_val(int id, uint32_t val, uint32_t dcon)
{
	struct tDPMS dpms;

	if (tcc_ckc_is_normal_pll(id) == 1) {
		/* if 'id' is normal pll, */
		return -1;
	}

	memset(&dpms, 0x0, sizeof(struct tDPMS));

	dpms.sscg_en = (val >> DPLL_SSCG_EN_SHIFT) & 0x1;
	dpms.sel_pf = (val >> DPLL_SEL_PF_SHIFT) & DPLL_SEL_PF_MASK;
	dpms.p = (val >> DPLL_P_SHIFT) & DPLL_P_MASK;
	dpms.m = (val >> DPLL_M_SHIFT) & DPLL_M_MASK;
	dpms.s = (val >> DPLL_S_SHIFT) & DPLL_S_MASK;
	dpms.src = PLLSRC_XIN;
	dpms.en = 1;

	if (dpms.sscg_en != 0) {
		dpms.k = (dcon >> DPLL_K_SHIFT) & DPLL_K_MASK;
		dpms.mfr = (dcon >> DPLL_MFR_SHIFT) & DPLL_MFR_MASK;
		dpms.mrr = (dcon >> DPLL_MRR_SHIFT) & DPLL_MRR_MASK;
	}

	tcc_dpll_write(id, &dpms);
	tcc_ckc_reset_clock_source(id);

	return 0;
}

static int tcc_ckc_dpll_set_rate(int id, uint32_t rate)
{
	struct tDPMS dpms;

	if (tcc_ckc_is_normal_pll(id) == 1) {
		/* if 'id' is normal pll, */
		return -1;
	}

	memset(&dpms, 0x0, sizeof(struct tDPMS));

	dpms.fpll = rate;
	dpms.src = PLLSRC_XIN;
	dpms.sel_pf = DPLL_SEL_CENTER;
	if (tcc_find_dithered_pms(&dpms, XIN_CLK_RATE) != 0) {
		dpms.en = 0;
		tcc_dpll_write(id, &dpms);
		tcc_ckc_reset_clock_source(id);
		return -1;
	}
	tcc_dpll_write(id, &dpms);

	tcc_ckc_reset_clock_source(id);

	return 0;
}

static uint32_t tcc_ckc_vpll_get_rate(int id)
{
	void __iomem *reg = NULL;

	if (id == PLL_VIDEO_0) {
		/* 'id' equals to video PLL0 */
		reg = vbusckc_base + VCKC_PLL0PMS;
	} else if (id == PLL_VIDEO_1) {
		/* 'id' equals to video PLL1 */
		reg = vbusckc_base + VCKC_PLL1PMS;
	} else {
		/* error situation */
		return 0;
	}

	return tcc_ckc_dedicated_pll_get_rate(reg);
}

static int tcc_ckc_vpll_set_rate(int id, uint32_t rate)
{
	void __iomem *reg = NULL;
	uint32_t idx = 0;

	if (id == PLL_VIDEO_0) {
		reg = vbusckc_base + VCKC_PLL0PMS;
		idx = VCLKCTRL_SEL_PLL0;
	}

	if (id == PLL_VIDEO_1) {
		reg = vbusckc_base + VCKC_PLL1PMS;
		idx = VCLKCTRL_SEL_PLL1;
	}

	tcc_ckc_dedicated_pll_set_rate(reg, rate);
	stVbusClkSource[idx] = tcc_ckc_dedicated_pll_get_rate(reg);

	return 0;
}

static int tcc_ckc_pll_set_rate(int id, unsigned long rate)
{
	void __iomem *reg = ckc_base + CKC_PLLPMS + (id * 4);
	unsigned int srcfreq = 0;
	unsigned int src = PLLSRC_XIN;
	struct tPMS nPLL;

	if ((id == PLL_VIDEO_0) || (id == PLL_VIDEO_1)) {
		/* if video pll, use corresponding function. */
		tcc_ckc_vpll_set_rate(id, rate);
	}

	if (id >= MAX_TCC_PLL)
		return -1;

	if ((rate < PLL_MIN_RATE) || (rate > PLL_MAX_RATE))
		return -1;

	switch (src) {
	case PLLSRC_XIN:
		srcfreq = XIN_CLK_RATE;
		break;
	case PLLSRC_HDMIXI:
		srcfreq = HDMI_CLK_RATE;
		break;
	case PLLSRC_EXTCLK0:
		srcfreq = EXT0_CLK_RATE;
		break;
	case PLLSRC_EXTCLK1:
		srcfreq = EXT1_CLK_RATE;
		break;
	default:
		goto tcc_ckc_setpll_failed;
	}

	if (srcfreq == 0)
		goto tcc_ckc_setpll_failed;

	if (tcc_ckc_is_normal_pll(id) == 1) {
		memset(&nPLL, 0x0, sizeof(struct tPMS));
		nPLL.fpll = rate;

		if (tcc_find_pms(&nPLL, srcfreq) != 0) {
			/* if finding appropriate p,m,s set failed, */
			goto tcc_ckc_setpll_failed;
		}
		tcc_pll_write(reg, nPLL.en, nPLL.p, nPLL.m, nPLL.s, src);
	} else {
		tcc_ckc_dpll_set_rate(id, rate);
		return 0;
	}
	tcc_ckc_reset_clock_source(id);
	return 0;

tcc_ckc_setpll_failed:
	tcc_pll_write(reg, 0, PLL_P_MIN,
		      ((PLL_P_MIN * PLL_VCO_MIN) + XIN_CLK_RATE) / XIN_CLK_RATE,
		      PLL_S_MIN, src);
	tcc_ckc_reset_clock_source(id);
	return -1;
}

static unsigned long tcc_ckc_pll_get_rate(int id)
{
	void __iomem *reg = ckc_base + CKC_PLLPMS + (id * 4);
	unsigned int reg_values = ckc_readl(reg);
	struct tPMS nPLLCFG;
	unsigned int src_freq;
	u64 u64_tmp;

	if (id >= MAX_TCC_PLL)
		return 0;

	if (tcc_ckc_is_normal_pll(id) == 1) {
		nPLLCFG.p = (reg_values >> PLL_P_SHIFT) & (PLL_P_MASK);
		nPLLCFG.m = (reg_values >> PLL_M_SHIFT) & (PLL_M_MASK);
		nPLLCFG.s = (reg_values >> PLL_S_SHIFT) & (PLL_S_MASK);
		nPLLCFG.en = (reg_values >> PLL_EN_SHIFT) & (1);
		nPLLCFG.src = (reg_values >> PLL_SRC_SHIFT) & (PLL_SRC_MASK);
	} else {
		nPLLCFG.p = (reg_values >> DPLL_P_SHIFT) & (DPLL_P_MASK);
		nPLLCFG.m = (reg_values >> DPLL_M_SHIFT) & (DPLL_M_MASK);
		nPLLCFG.s = (reg_values >> DPLL_S_SHIFT) & (DPLL_S_MASK);
		nPLLCFG.en = (reg_values >> DPLL_EN_SHIFT) & (1);
		nPLLCFG.src = (reg_values >> DPLL_SRC_SHIFT) & (DPLL_SRC_MASK);
	}

	if (nPLLCFG.en == 0)
		return 0;

	switch (nPLLCFG.src) {
	case PLLSRC_XIN:
		src_freq = XIN_CLK_RATE;
		break;
	case PLLSRC_HDMIXI:
		src_freq = HDMI_CLK_RATE;
		break;
	case PLLSRC_EXTCLK0:
		src_freq = EXT0_CLK_RATE;
		break;
	case PLLSRC_EXTCLK1:
		src_freq = EXT1_CLK_RATE;
		break;
	default:
		return 0;
	}

	u64_tmp = (u64)src_freq * (u64)nPLLCFG.m;
	do_div(u64_tmp, nPLLCFG.p);
	return (unsigned int)((u64_tmp) >> nPLLCFG.s);
}

static int tcc_ckc_vpll_div_set(int id, uint32_t div)
{
	void __iomem *div_reg = NULL;
	void __iomem *pms_reg = NULL;
	uint32_t reg_val = 0;
	uint32_t offset0, offset1;
	uint32_t idx = 0;

	if (id == PLL_VIDEO_0) {
		pms_reg = vbusckc_base + VCKC_PLL0PMS;
		offset0 = VCKC_PLL0DIV0_OFFSET;
		offset1 = VCKC_PLL0DIV1_OFFSET;
		idx = VCLKCTRL_SEL_PLL0_DIV0;
	}

	if (id == PLL_VIDEO_1) {
		pms_reg = vbusckc_base + VCKC_PLL1PMS;
		offset0 = VCKC_PLL1DIV0_OFFSET;
		offset1 = VCKC_PLL1DIV1_OFFSET;
		idx = VCLKCTRL_SEL_PLL1_DIV0;
	}
	div_reg = vbusckc_base + VCKC_PLLDIVC;
	reg_val = ckc_readl(div_reg);

	if (div) {
		reg_val |= (0x80 | (div & 0x3F)) << offset0;
		reg_val |= (0x80 | ((div + 1) & 0x3F)) << offset1;
	} else {
		reg_val |= 1 << offset0;
		reg_val |= 1 << offset1;
	}
	ckc_writel(reg_val, div_reg);

	stVbusClkSource[idx] = tcc_ckc_dedicated_pll_get_rate(pms_reg) / div;
	stVbusClkSource[idx + 1] =
	    tcc_ckc_dedicated_pll_get_rate(pms_reg) / (div + 1);

	return 0;
}

static int tcc_ckc_pll_div_set(int id, uint32_t div)
{
	void __iomem *reg;
	uint32_t reg_values;
	uint32_t offset = 0;

	if (id >= MAX_TCC_PLL)
		return 0;

	switch (id) {
	case PLL_0:
	case PLL_1:
	case PLL_2:
	case PLL_3:
		reg = ckc_base + CKC_CLKDIVC;
		offset = (3 - id) * 8;
		break;
	case PLL_4:
		reg = ckc_base + CKC_CLKDIVC + 0x4;
		offset = 24;
		break;
	case PLL_VIDEO_0:
	case PLL_VIDEO_1:
		tcc_ckc_vpll_div_set(id, div);
		return 0;
	default:
		return -1;
	}

	reg_values = ckc_readl(reg) & ~(0xFF << offset);
	ckc_writel(reg_values, reg);

	if (div)
		reg_values |= (0x80 | (div & 0x3F)) << offset;
	else
		reg_values |= 1 << offset;
	ckc_writel(reg_values, reg);

	tcc_ckc_reset_clock_source(id);

	return 0;
}

static unsigned long tcc_ckc_plldiv_get_rate(int id)
{
	void __iomem *reg;
	unsigned int reg_values;
	unsigned int offset = 0, fpll = 0, pdiv = 0;

	if (id >= MAX_TCC_PLL)
		return 0;

	switch (id) {
	case PLL_0:
	case PLL_1:
	case PLL_2:
	case PLL_3:
		reg = ckc_base + CKC_CLKDIVC;
		offset = (3 - id) * 8;
		break;
	case PLL_4:
		reg = ckc_base + CKC_CLKDIVC + 0x4;
		offset = 24;
		break;
	default:
		return 0;
	}

	reg_values = ckc_readl(reg);

	if (((reg_values >> offset) & 0x80) == 0) {
		/* check plldivc enable bit */
		return 0;
	}
	pdiv = (reg_values >> offset) & 0x3F;
	if (!pdiv) {
		/* should not be zero */
		return 0;
	}
	fpll = tcc_ckc_pll_get_rate(id);

	return (unsigned int)fpll / (pdiv + 1);
}

static inline int tcc_find_clkctrl(struct tCLKCTRL *CLKCTRL)
{
	unsigned int i, div[MAX_CLK_SRC], err[MAX_CLK_SRC], searchsrc, clk_rate;

	searchsrc = 0xFFFFFFFF;

	if (CLKCTRL->freq <= (XIN_CLK_RATE / 2)) {
		CLKCTRL->sel = CLKCTRL_SEL_XIN;
		CLKCTRL->freq = XIN_CLK_RATE / 2;
		CLKCTRL->config = 1;
	} else {
		for (i = 0; i < MAX_CLK_SRC; i++) {
			if (stClockSource[i] == 0)
				continue;
			div[i] =
			    (stClockSource[i] + CLKCTRL->freq - 1) /
			    CLKCTRL->freq;

			if (div[i] > (CLKCTRL_CONFIG_MAX + 1))
				div[i] = CLKCTRL_CONFIG_MAX + 1;
			else if (div[i] < (CLKCTRL_CONFIG_MIN + 1))
				div[i] = CLKCTRL_CONFIG_MIN + 1;

			clk_rate = stClockSource[i] / div[i];

			if (CLKCTRL->freq < clk_rate)
				continue;

			err[i] = CLKCTRL->freq - clk_rate;

			if (searchsrc == 0xFFFFFFFF)
				searchsrc = i;
			else {
				/* find similar clock */
				if (err[i] < err[searchsrc])
					searchsrc = i;
				/* find even division vlaue */
				else if (err[i] == err[searchsrc]) {
					if (div[i] % 2 == 0)
						searchsrc = i;
				}
			}
			if (err[searchsrc] == 0)
				break;
		}
		if (searchsrc == 0xFFFFFFFF)
			return -1;

		switch (searchsrc) {
		case PLL_0:
			CLKCTRL->sel = CLKCTRL_SEL_PLL0;
			break;
		case PLL_1:
			CLKCTRL->sel = CLKCTRL_SEL_PLL1;
			break;
		case PLL_2:
			CLKCTRL->sel = CLKCTRL_SEL_PLL2;
			break;
		case PLL_3:
			CLKCTRL->sel = CLKCTRL_SEL_PLL3;
			break;
		case PLL_4:
			CLKCTRL->sel = CLKCTRL_SEL_PLL4;
			break;
		case PLL_DIV_0:
			CLKCTRL->sel = CLKCTRL_SEL_PLL0DIV;
			break;
		case PLL_DIV_1:
			CLKCTRL->sel = CLKCTRL_SEL_PLL1DIV;
			break;
		case PLL_DIV_2:
			CLKCTRL->sel = CLKCTRL_SEL_PLL2DIV;
			break;
		case PLL_DIV_3:
			CLKCTRL->sel = CLKCTRL_SEL_PLL3DIV;
			break;
		case PLL_DIV_4:
			CLKCTRL->sel = CLKCTRL_SEL_PLL4DIV;
			break;
		case PLL_XIN:
			CLKCTRL->sel = CLKCTRL_SEL_XIN;
			break;
		default:
			return -1;
		}

		if (div[searchsrc] > (CLKCTRL_CONFIG_MAX + 1))
			div[searchsrc] = CLKCTRL_CONFIG_MAX + 1;
		else if (div[searchsrc] <= CLKCTRL_CONFIG_MIN)
			div[searchsrc] = CLKCTRL_CONFIG_MIN + 1;

		CLKCTRL->freq = stClockSource[searchsrc] / div[searchsrc];
		CLKCTRL->config = div[searchsrc] - 1;
	}
	return 0;
}

static int tcc_ckc_is_clkctrl_enabled(int id)
{
	void __iomem *reg = ckc_base + CKC_CLKCTRL + (id * 4);
	void __iomem *reg_cpu_pll = NULL;

	switch (id) {
	case FBUS_CPU0:
		return 1;
	case FBUS_CPU1:
		if ((ckc_readl(cpubus_cfg_base + 0x4) & (1 << 17)) == 0)
			return 0;
		return 1;
	case FBUS_MEM:
	case FBUS_MEM_PHY:
		return 1;
	case FBUS_GPU:
		reg_cpu_pll = gpu_3d_base;
		break;
	case FBUS_G2D:
		reg_cpu_pll = gpu_2d_base;
		break;
	case FBUS_VBUS:
	case FBUS_CODA:
	case FBUS_BHEVC:
	case FBUS_CHEVC:
		return 1;
	}

	if (reg_cpu_pll)
		return (ckc_readl(reg_cpu_pll + DCKC_PLLPMS) &
			(1 << PLL_EN_SHIFT)) ? 1 : 0;

	return (ckc_readl(reg) & (1 << CLKCTRL_EN_SHIFT)) ? 1 : 0;
}

static int tcc_ckc_clkctrl_enable(int id)
{
	void __iomem *reg = ckc_base + CKC_CLKCTRL + (id * 4);
	void __iomem *subpwdn_reg = NULL;
	void __iomem *subrst_reg = NULL;

	if (tcc_ckc_is_clkctrl_enabled(id))
		return 0;

	switch (id) {
	case FBUS_CPU0:
	case FBUS_CPU1:
	case FBUS_MEM:
	case FBUS_MEM_PHY:
		return 0;
	}

	ckc_writel(ckc_readl(reg) | (1 << CLKCTRL_EN_SHIFT), reg);

	while (ckc_readl(reg) & (1 << CLKCTRL_CFGRQ_SHIFT))
		;

	return 0;
}

static int tcc_ckc_clkctrl_disable(int id)
{
	void __iomem *reg = ckc_base + CKC_CLKCTRL + (id * 4);
	void __iomem *reg_gpu_pll = NULL;

	if (!tcc_ckc_is_clkctrl_enabled(id)) {
		/* if clkctrl register already disabled. */
		return 0;
	}

	switch (id) {
	case FBUS_CPU0:
	case FBUS_CPU1:
	case FBUS_MEM:
	case FBUS_MEM_PHY:
		return 0;
	case FBUS_GPU:
		reg_gpu_pll = gpu_3d_base;
		break;
	}

	ckc_writel(ckc_readl(reg) & ~(1 << CLKCTRL_EN_SHIFT), reg);
	return 0;
}

static uint32_t tcc_ckc_vclkctrl_get_rate(int id)
{
	void __iomem *reg = NULL;
	uint32_t pll_val = 0;
	uint32_t div_val = 0;
	int sel = 0;

	switch (id) {
	case FBUS_VBUS:
		reg = vbusckc_base + VCKC_VBUS;
		break;
	case FBUS_CODA:
		reg = vbusckc_base + VCKC_CODA;
		break;
	case FBUS_BHEVC:
		reg = vbusckc_base + VCKC_BHEVC;
		break;
	case FBUS_CHEVC:
		reg = vbusckc_base + VCKC_CHEVC;
		break;
	default:
		reg = NULL;
	}

	sel = ckc_readl(reg) & VCLKCTRL_SEL_MASK;

	switch (sel) {
	case VCLKCTRL_SEL_XIN:
		return XIN_CLK_RATE;
	case VCLKCTRL_SEL_PLL0:
		return tcc_ckc_vpll_get_rate(PLL_VIDEO_0);
	case VCLKCTRL_SEL_PLL1:
		return tcc_ckc_vpll_get_rate(PLL_VIDEO_1);
	case VCLKCTRL_SEL_PLL0_DIV0:
		pll_val = tcc_ckc_vpll_get_rate(PLL_VIDEO_0);
		div_val = ((ckc_readl(vbusckc_base + VCKC_PLLDIVC) >>
			    VCKC_PLL0DIV0_OFFSET) & 0x3F) + 1;
		return pll_val / div_val;
	case VCLKCTRL_SEL_PLL0_DIV1:
		pll_val = tcc_ckc_vpll_get_rate(PLL_VIDEO_0);
		div_val = ((ckc_readl(vbusckc_base + VCKC_PLLDIVC) >>
			    VCKC_PLL0DIV1_OFFSET) & 0x3F) + 1;
		return pll_val / div_val;
	case VCLKCTRL_SEL_PLL1_DIV0:
		pll_val = tcc_ckc_vpll_get_rate(PLL_VIDEO_1);
		div_val = ((ckc_readl(vbusckc_base + VCKC_PLLDIVC) >>
			    VCKC_PLL1DIV0_OFFSET) & 0x3F) + 1;
		return pll_val / div_val;
	case VCLKCTRL_SEL_PLL1_DIV1:
		pll_val = tcc_ckc_vpll_get_rate(PLL_VIDEO_1);
		div_val = ((ckc_readl(vbusckc_base + VCKC_PLLDIVC) >>
			    VCKC_PLL1DIV1_OFFSET) & 0x3F) + 1;
		return pll_val / div_val;
	case VCLKCTRL_SEL_XTIN:
		break;
	default:
		return 0;
	}

	return 0;
}

static int tcc_ckc_vclkctrl_set_rate(int id, uint32_t rate)
{
	void __iomem *reg = NULL;
	uint32_t err[VCLKCTRL_SEL_MAX], searchsrc;
	uint32_t i;

	searchsrc = 0xFFFFFFFF;

	for (i = 0; i < VCLKCTRL_SEL_MAX; i++) {
		if (stVbusClkSource[i] == 0) {
			/* if source clock is zero, */
			continue;
		}
		err[i] = rate - stVbusClkSource[i];

		if (searchsrc == 0xFFFFFFFF) {
			searchsrc = i;
		} else {
			/* find similar clock */
			if (err[i] < err[searchsrc])
				searchsrc = i;
		}
		if (err[searchsrc] == 0) {
			/* if difference(error) is zero, */
			break;
		}
	}

	switch (id) {
	case FBUS_VBUS:
		reg = vbusckc_base + VCKC_VBUS;
		break;
	case FBUS_CODA:
		reg = vbusckc_base + VCKC_CODA;
		break;
	case FBUS_BHEVC:
		reg = vbusckc_base + VCKC_BHEVC;
		break;
	case FBUS_CHEVC:
		reg = vbusckc_base + VCKC_CHEVC;
		break;
	default:
		reg = NULL;
	}

	if (reg != NULL) {
		/* if reg is no NULL, */
		tcc_dclkctrl_write(reg, searchsrc);
	}

	return 0;
}

static int tcc_ckc_clkctrl_set_rate(int id, unsigned long rate)
{
	void __iomem *reg = ckc_base + CKC_CLKCTRL + (id * 4);
	struct tCLKCTRL nCLKCTRL;
	unsigned long flags;

	nCLKCTRL.en = (ckc_readl(reg) & (1 << CLKCTRL_EN_SHIFT)) ? 1 : 0;

	switch (id) {
	case FBUS_CPU0:
		// Do not use direct PLL for CPU0
		local_irq_save(flags);
		tcc_dclkctrl_write((cpu0_base + DCKC_CLKCTRL),
				   DCLKCTRL_SEL_XIN);
		tcc_ckc_dedicated_pll_set_rate((cpu0_base + DCKC_PLLPMS),
						rate * 2);
		tcc_ckc_dedicated_plldiv_set(FBUS_CPU0, 1);
		tcc_dclkctrl_write((cpu0_base + DCKC_CLKCTRL),
				   DCLKCTRL_SEL_PLLDIV);
		local_irq_restore(flags);
		return 0;
	case FBUS_CPU1:
		return -1;
	case FBUS_GPU:
		tcc_dclkctrl_write((gpu_3d_base + DCKC_CLKCTRL),
				   DCLKCTRL_SEL_XIN);
		tcc_ckc_dedicated_pll_set_rate((gpu_3d_base + DCKC_PLLPMS),
					       rate);
		tcc_dclkctrl_write((gpu_3d_base + DCKC_CLKCTRL),
				   DCLKCTRL_SEL_PLL);
		return 0;
	case FBUS_G2D:
		tcc_dclkctrl_write((gpu_2d_base + DCKC_CLKCTRL),
				    DCLKCTRL_SEL_XIN);
		tcc_ckc_dedicated_pll_set_rate((gpu_2d_base + DCKC_PLLPMS),
						rate);
		tcc_dclkctrl_write((gpu_2d_base + DCKC_CLKCTRL),
				   DCLKCTRL_SEL_PLL);
		return 0;
	case FBUS_VBUS:
	case FBUS_CHEVC:
	case FBUS_BHEVC:
	case FBUS_CODA:
		tcc_ckc_vclkctrl_set_rate(id, rate);
		return 0;
	case FBUS_MEM:
	case FBUS_MEM_PHY:
		BUG();
		return 0;
	}

	nCLKCTRL.freq = rate;
	if (tcc_find_clkctrl(&nCLKCTRL))
		return -1;

	tcc_clkctrl_write(reg, nCLKCTRL.en, nCLKCTRL.config, nCLKCTRL.sel);
	return 0;
}

static unsigned long tcc_ckc_clkctrl_get_rate(int id)
{
	void __iomem *reg = ckc_base + CKC_CLKCTRL + (id * 4);
	void __iomem *reg_2nd = NULL;
	unsigned int reg_values = ckc_readl(reg);
	struct tCLKCTRL nCLKCTRL;
	unsigned int src_freq = 0;

	switch (id) {
	case FBUS_CPU0:
		return tcc_ckc_dedicated_pll_get_rate(cpu0_base + DCKC_PLLPMS) /
			(tcc_ckc_dedicated_plldiv_get(FBUS_CPU0) + 1);
	case FBUS_CPU1:
		if ((ckc_readl(cpubus_cfg_base + 0x4) & SWRESET_CA7SP) == 0) {
			/* if CA7s already power down state. */
			return 0;
		}
		return tcc_ckc_dedicated_pll_get_rate(cpu1_base + DCKC_PLLPMS) /
			(tcc_ckc_dedicated_plldiv_get(FBUS_CPU1) + 1);
	case FBUS_GPU:
		return tcc_ckc_dedicated_pll_get_rate(gpu_3d_base +
						      DCKC_PLLPMS);
	case FBUS_G2D:
		return tcc_ckc_dedicated_pll_get_rate(gpu_2d_base +
						      DCKC_PLLPMS);
	case FBUS_MEM_PHY:
		return tcc_ckc_dedicated_pll_get_rate(mem_ckc_base +
						      DCKC_PLLPMS);
	case FBUS_VBUS:
	case FBUS_CHEVC:
	case FBUS_BHEVC:
	case FBUS_CODA:
		return tcc_ckc_vclkctrl_get_rate(id);
	case FBUS_CBUS:
	case FBUS_HSIO:
	case FBUS_SMU:
	case FBUS_DDI:
	case FBUS_IO:
	case FBUS_MEM:
	case FBUS_CMBUS:
		break;
	}

	nCLKCTRL.sel =
	    (reg_values & (CLKCTRL_SEL_MASK << CLKCTRL_SEL_SHIFT)) >>
	     CLKCTRL_SEL_SHIFT;

	switch (nCLKCTRL.sel) {
	case CLKCTRL_SEL_PLL0:
		src_freq = tcc_ckc_pll_get_rate(PLL_0);
		break;
	case CLKCTRL_SEL_PLL1:
		src_freq = tcc_ckc_pll_get_rate(PLL_1);
		break;
	case CLKCTRL_SEL_PLL2:
		src_freq = tcc_ckc_pll_get_rate(PLL_2);
		break;
	case CLKCTRL_SEL_PLL3:
		src_freq = tcc_ckc_pll_get_rate(PLL_3);
		break;
	case CLKCTRL_SEL_PLL4:
		src_freq = tcc_ckc_pll_get_rate(PLL_4);
		break;
	case CLKCTRL_SEL_XIN:
		src_freq = XIN_CLK_RATE;
		break;
	case CLKCTRL_SEL_XTIN:
		src_freq = XTIN_CLK_RATE;
		break;
	case CLKCTRL_SEL_PLL0DIV:
		src_freq = tcc_ckc_plldiv_get_rate(PLL_0);
		break;
	case CLKCTRL_SEL_PLL1DIV:
		src_freq = tcc_ckc_plldiv_get_rate(PLL_1);
		break;
	case CLKCTRL_SEL_PLL2DIV:
		src_freq = tcc_ckc_plldiv_get_rate(PLL_2);
		break;
	case CLKCTRL_SEL_PLL3DIV:
		src_freq = tcc_ckc_plldiv_get_rate(PLL_3);
		break;
	case CLKCTRL_SEL_PLL4DIV:
		src_freq = tcc_ckc_plldiv_get_rate(PLL_4);
		break;
	case CLKCTRL_SEL_XINDIV:
	case CLKCTRL_SEL_XTINDIV:
	case CLKCTRL_SEL_EXTIN2:
	case CLKCTRL_SEL_EXTIN3:
	default:
		return 0;
	}

	nCLKCTRL.config =
	    (reg_values & (CLKCTRL_CONFIG_MASK << CLKCTRL_CONFIG_SHIFT)) >>
	     CLKCTRL_CONFIG_SHIFT;
	nCLKCTRL.freq = src_freq / (nCLKCTRL.config + 1);

	return (unsigned long)nCLKCTRL.freq;
}

static inline unsigned int tcc_pclk_support_below_freq(unsigned int periname)
{
	if ((periname == PERI_SDMMC0) || (periname == PERI_SDMMC1) ||
	    (periname == PERI_SDMMC2)) {
		/* calc. freq. must be below(same or under) value */
		return 1;
	}

	return 0;
}

static inline unsigned int tcc_ckc_pclk_divider(struct tPCLKCTRL *PCLKCTRL,
						unsigned int *div,
						const unsigned int src_CLK,
						unsigned int div_min,
						unsigned int div_max)
{
	unsigned int clk_rate1, clk_rate2, err1, err2;

	if (src_CLK <= PCLKCTRL->freq)
		*div = 1;
	else
		*div = src_CLK / PCLKCTRL->freq;

	if (*div > div_max)
		*div = div_max;

	clk_rate1 = src_CLK / (*div);
	clk_rate2 = src_CLK / ((*div < div_max) ? (*div + 1) : *div);

	if (tcc_pclk_support_below_freq(PCLKCTRL->periname)) {
		err1 =  (clk_rate1 > PCLKCTRL->freq) ?
			0xFFFFFFFF :
			(PCLKCTRL->freq - clk_rate1);
		err2 =  (clk_rate2 > PCLKCTRL->freq) ?
			 0xFFFFFFFF :
			 (PCLKCTRL->freq - clk_rate2);
	} else {
		err1 =  (clk_rate1 > PCLKCTRL->freq) ?
			(clk_rate1 - PCLKCTRL->freq) :
			(PCLKCTRL->freq - clk_rate1);
		err2 =  (clk_rate2 > PCLKCTRL->freq) ?
			(clk_rate2 - PCLKCTRL->freq) :
			(PCLKCTRL->freq - clk_rate2);
	}

	if (err1 > err2)
		*div += 1;

	return (err1 < err2) ? err1 : err2;
}

static inline unsigned int tcc_ckc_pclk_dco(struct tPCLKCTRL *PCLKCTRL,
					    unsigned int *div,
					    const unsigned int src_CLK,
					    unsigned int div_min,
					    unsigned int div_max)
{
	unsigned int clk_rate1, clk_rate2, err1, err2;
	u64 u64_tmp;

	if (src_CLK < PCLKCTRL->freq)
		return 0xFFFFFFFF;

	u64_tmp = (unsigned long long)(PCLKCTRL->freq) *
		  (unsigned long long)div_max;

	do_div(u64_tmp, src_CLK);
	*div = (unsigned int)u64_tmp;

	if (*div > (div_max + 1) / 2)
		return 0xFFFFFFFF;

	u64_tmp = (unsigned long long)src_CLK * (unsigned long long)(*div);
	do_div(u64_tmp, (div_max + 1));
	clk_rate1 = (unsigned int)u64_tmp;
	u64_tmp = (unsigned long long)src_CLK * (unsigned long long)(*div + 1);
	do_div(u64_tmp, (div_max + 1));
	clk_rate2 = (unsigned int)u64_tmp;

	if (tcc_pclk_support_below_freq(PCLKCTRL->periname)) {
		err1 =
		    (clk_rate1 > PCLKCTRL->freq) ?
		    0xFFFFFFFF :
		    (PCLKCTRL->freq - clk_rate1);
		err2 =
		    (clk_rate2 > PCLKCTRL->freq) ?
		    0xFFFFFFFF :
		    (PCLKCTRL->freq - clk_rate2);
	} else {
		err1 = (clk_rate1 > PCLKCTRL->freq) ?
		       (clk_rate1 - PCLKCTRL->freq) :
		       (PCLKCTRL->freq - clk_rate1);
		err2 = (clk_rate2 > PCLKCTRL->freq) ?
		       (clk_rate2 - PCLKCTRL->freq) :
		       (PCLKCTRL->freq - clk_rate2);
	}
	if (err1 > err2)
		*div += 1;

	return (err1 < err2) ? err1 : err2;
}

static inline int tcc_find_pclk(struct tPCLKCTRL *PCLKCTRL,
				u32	type,
				u32	flags)
{
	int i;
	unsigned int div_min, div_max, searchsrc, err_dco, err_div, md;
	unsigned int div[MAX_CLK_SRC], err[MAX_CLK_SRC];
	unsigned int div_dco = PCLKCTRL_DIV_DCO_MIN;
	unsigned int div_div = PCLKCTRL_DIV_MIN;
	unsigned int max_src, min_src;

	switch (type) {
	case PCLKCTRL_TYPE_XXX:
		PCLKCTRL->md = PCLKCTRL_MODE_DIVIDER;
		div_max = PCLKCTRL_DIV_XXX_MAX;
		div_min = PCLKCTRL_DIV_MIN;
		break;
	case PCLKCTRL_TYPE_YYY:
		if (flags & CLK_F_DIV_MODE)
			PCLKCTRL->md = PCLKCTRL_MODE_DIVIDER;
		else
			PCLKCTRL->md = PCLKCTRL_MODE_DCO;
		div_max = PCLKCTRL_DIV_YYY_MAX;
		div_min = PCLKCTRL_DIV_DCO_MIN;
		break;
	default:
		return -1;
	}

	if (flags & CLK_F_FIXED) {
		max_src = flags & CLK_F_SRC_CLK_MASK;
		min_src = flags & CLK_F_SRC_CLK_MASK;
	} else {
		max_src = MAX_CLK_SRC - 1;
		min_src = 0;
	}

	searchsrc = 0xFFFFFFFF;
	for (i = (MAX_CLK_SRC - 1); i >= 0; i--) {
		if ((flags & CLK_F_SKIP_SSCG) && tcc_ckc_is_dpll_mode(i)) {
			/* Skip if the source PLL is SSCG enabled */
			continue;
		}

		if (stClockSource[i] == 0) {
			/* source clock is zero. */
			continue;
		}

		if ((stClockSource[i] >= PCLKCTRL_MAX_FCKS) &&
		    (type == PCLKCTRL_TYPE_XXX)) {
			/* source clock is beyond limited value */
			continue;
		}

		/* dco mode */
		if (PCLKCTRL->md == PCLKCTRL_MODE_DIVIDER)
			err_dco = 0xFFFFFFFF;
		else {
			if (i >= (MAX_TCC_PLL * 2))
				continue;

			if ((PCLKCTRL->periname == PERI_MSPDIF0) ||
			    (PCLKCTRL->periname == PERI_MSPDIF1))
				err_dco = 0xFFFFFFFF;
			else
				err_dco =
				    tcc_ckc_pclk_dco(PCLKCTRL, &div_dco,
						     stClockSource[i],
						     div_min,
						     div_max);
		}

		/* divider mode */
		err_div = tcc_ckc_pclk_divider(PCLKCTRL, &div_div,
					       stClockSource[i],
					       div_min + 1,
					       div_max + 1);

		/* common */
		if ((err_dco < err_div) || (flags & CLK_F_DCO_MODE)) {
			err[i] = err_dco;
			div[i] = div_dco;
			md = PCLKCTRL_MODE_DCO;
		} else {
			err[i] = err_div;
			div[i] = div_div;
			md = PCLKCTRL_MODE_DIVIDER;
		}

		if (searchsrc == 0xFFFFFFFF) {
			searchsrc = i;
			PCLKCTRL->md = md;
		} else {
			/* find similar clock */
			if (err[i] < err[searchsrc]) {
				searchsrc = i;
				PCLKCTRL->md = md;
			} else if ((err[i] == err[searchsrc])
				   && (md == PCLKCTRL_MODE_DIVIDER)) {
				searchsrc = i;
				PCLKCTRL->md = md;
			}
			// If err_dco is the same with before search source,
			// Use more lower dco divider source.
			else if ((err[i] == err[searchsrc])
				 && (div[i] < div[searchsrc])) {
				searchsrc = i;
				PCLKCTRL->md = md;
			}
		}
	}

	switch (searchsrc) {
	case PLL_0:
		PCLKCTRL->sel = PCLKCTRL_SEL_PLL0;
		break;
	case PLL_1:
		PCLKCTRL->sel = PCLKCTRL_SEL_PLL1;
		break;
	case PLL_2:
		PCLKCTRL->sel = PCLKCTRL_SEL_PLL2;
		break;
	case PLL_3:
		PCLKCTRL->sel = PCLKCTRL_SEL_PLL3;
		break;
	case PLL_4:
		PCLKCTRL->sel = PCLKCTRL_SEL_PLL4;
		break;
	case PLL_DIV_0:
		PCLKCTRL->sel = PCLKCTRL_SEL_PLL0DIV;
		break;
	case PLL_DIV_1:
		PCLKCTRL->sel = PCLKCTRL_SEL_PLL1DIV;
		break;
	case PLL_DIV_2:
		PCLKCTRL->sel = PCLKCTRL_SEL_PLL2DIV;
		break;
	case PLL_DIV_3:
		PCLKCTRL->sel = PCLKCTRL_SEL_PLL3DIV;
		break;
	case PLL_DIV_4:
		PCLKCTRL->sel = PCLKCTRL_SEL_PLL4DIV;
		break;
	case PLL_XIN:
		PCLKCTRL->sel = PCLKCTRL_SEL_XIN;
		break;
	case PLL_XTIN:
		PCLKCTRL->sel = PCLKCTRL_SEL_XTIN;
		break;
	default:
		return -1;
	}

	if (PCLKCTRL->md == PCLKCTRL_MODE_DCO) {
		u64 u64_tmp;

		PCLKCTRL->div = div[searchsrc];

		if (PCLKCTRL->div > (div_max / 2))
			u64_tmp =
			    (unsigned long long)stClockSource[searchsrc] *
			    (unsigned long long)(div_max - PCLKCTRL->div);
		else
			u64_tmp =
			    (unsigned long long)stClockSource[searchsrc] *
			    (unsigned long long)PCLKCTRL->div;

		do_div(u64_tmp, div_max);
		PCLKCTRL->freq = (unsigned int)u64_tmp;

		if ((PCLKCTRL->div < div_min) ||
		    (PCLKCTRL->div > (div_max - 1)))
			return -1;
	} else {		/* Divider mode */
		PCLKCTRL->div = div[searchsrc];

		if (PCLKCTRL->div >= (PCLKCTRL_DIV_MIN + 1) &&
		    PCLKCTRL->div <= (div_max + 1))
			PCLKCTRL->div -= 1;
		else
			return -1;

		PCLKCTRL->freq = stClockSource[searchsrc] / (PCLKCTRL->div + 1);
	}
	return 0;
}

static inline u32 tcc_check_pclk_type(unsigned int periname)
{
	switch (periname) {
	case PERI_MDAI0:
	case PERI_MSPDIF0:
	case PERI_MDAI1:
	case PERI_MSPDIF1:
	case PERI_MDAI2:
	case PERI_MSPDIF2:
	case PERI_SRCH0_SPDIF:
	case PERI_SRCH0_CORE:
	case PERI_SRCH1_SPDIF:
	case PERI_SRCH1_CORE:
	case PERI_SRCH2_SPDIF:
	case PERI_SRCH2_CORE:
	case PERI_SRCH3_SPDIF:
	case PERI_SRCH3_CORE:
	case PERI_AUX0_INPUT:
	case PERI_AUX1_INPUT:
	case PERI_AUX0_OUTPUT:
	case PERI_AUX1_OUTPUT:
	case PERI_TSRX0:
	case PERI_TSRX1:
	case PERI_TSRX2:
	case PERI_TSRX3:
	case PERI_TSRX4:
	case PERI_TSRX5:
	case PERI_TSRX6:
	case PERI_TSRX7:
	case PERI_TSRX8:
		return PCLKCTRL_TYPE_YYY;
	}
	return PCLKCTRL_TYPE_XXX;
}

static int tcc_ckc_peri_enable(int id)
{
	void __iomem *reg = ckc_base + CKC_PCLKCTRL + (id * 4);

	ckc_writel(ckc_readl(reg) | 1 << PCLKCTRL_EN_SHIFT, reg);
	if (tcc_check_pclk_type(id) == PCLKCTRL_TYPE_XXX)
		ckc_writel(ckc_readl(reg) | (1 << PCLKCTRL_OUTEN_SHIFT), reg);

	return 0;
}

static int tcc_ckc_peri_disable(int id)
{
	void __iomem *reg = ckc_base + CKC_PCLKCTRL + (id * 4);

	if (tcc_check_pclk_type(id) == PCLKCTRL_TYPE_XXX) {
		/* if PCLKCTRL_TYPE_XXX, */
		ckc_writel(ckc_readl(reg) & ~(1 << PCLKCTRL_OUTEN_SHIFT), reg);
	}
	ckc_writel(ckc_readl(reg) & ~(1 << PCLKCTRL_EN_SHIFT), reg);

	return 0;
}

static int tcc_ckc_is_peri_enabled(int id)
{
	void __iomem *reg = ckc_base + CKC_PCLKCTRL + (id * 4);

	return (ckc_readl(reg) & (1 << PCLKCTRL_EN_SHIFT)) ? 1 : 0;
}

static int tcc_ckc_peri_set_rate_common(int md, int id, int sel, int div)
{
	uintptr_t reg = ckc_base + CKC_PCLKCTRL + (id * 4);
	struct tPCLKCTRL nPCLKCTRL;
	u32 type = tcc_check_pclk_type(id);

	nPCLKCTRL.periname = id;
	nPCLKCTRL.div = div;
	nPCLKCTRL.md = md;
	nPCLKCTRL.sel = sel;
	nPCLKCTRL.en = (ckc_readl(reg) & (1<<PCLKCTRL_EN_SHIFT)) ? 1 : 0;
	tcc_pclkctrl_write(reg, nPCLKCTRL.md, nPCLKCTRL.en, nPCLKCTRL.sel,
			   nPCLKCTRL.div, type);

	return 0;
}

static int tcc_ckc_peri_set_rate(int id, unsigned long rate, u32 flags)
{
	struct tPCLKCTRL nPCLKCTRL;
	u32 type = tcc_check_pclk_type(id);

	nPCLKCTRL.freq = rate;
	nPCLKCTRL.periname = id;
	nPCLKCTRL.div = 0;
	nPCLKCTRL.md = PCLKCTRL_MODE_DCO;
	nPCLKCTRL.sel = PCLKCTRL_SEL_XIN;

	/* if input rate are 27(dummy value for set HDMIPCLK)
	 * then set the lcdc pclk source to HDMIPCLK
	 */

	if (((id == PERI_LCD0) || (id == PERI_LCD1) || (id == PERI_LCD2)) &&
	     (nPCLKCTRL.freq == HDMI_PCLK_RATE)) {
		nPCLKCTRL.sel = PCLKCTRL_SEL_HDMIPCLK;
		nPCLKCTRL.div = 0;
		nPCLKCTRL.md = PCLKCTRL_MODE_DIVIDER;
		nPCLKCTRL.freq = HDMI_PCLK_RATE;
	} else {
		if (tcc_find_pclk(&nPCLKCTRL, type, flags))
			goto tcc_ckc_setperi_failed;
	}

	return tcc_ckc_peri_set_rate_common(nPCLKCTRL.md, id, nPCLKCTRL.sel,
					    nPCLKCTRL.div);

tcc_ckc_setperi_failed:
	tcc_ckc_peri_set_rate_common(nPCLKCTRL.md, id, nPCLKCTRL.sel,
				     nPCLKCTRL.div);
	return -1;
}

static int tcc_ckc_peri_set_rate_div(int id, int sel, int div)
{
	return tcc_ckc_peri_set_rate_common(PCLKCTRL_MODE_DIVIDER,
					    id, sel, div);
}

static int tcc_ckc_peri_set_rate_dco(int id, int sel, int div)
{
	return tcc_ckc_peri_set_rate_common(PCLKCTRL_MODE_DCO, id, sel, div);
}

static unsigned long tcc_ckc_peri_get_rate(int id)
{
	void __iomem *reg = ckc_base + CKC_PCLKCTRL + (id * 4);
	unsigned int reg_values = ckc_readl(reg);
	struct tPCLKCTRL nPCLKCTRL;
	u32 type = tcc_check_pclk_type(id);
	unsigned int src_freq = 0, div_mask;

	if (reg_values == 0)
		return 0;

	nPCLKCTRL.md =  reg_values & (1 << PCLKCTRL_MD_SHIFT) ?
			PCLKCTRL_MODE_DIVIDER :
			PCLKCTRL_MODE_DCO;
	nPCLKCTRL.sel = (reg_values &
			(PCLKCTRL_SEL_MASK << PCLKCTRL_SEL_SHIFT)) >>
			PCLKCTRL_SEL_SHIFT;

	switch (nPCLKCTRL.sel) {
	case PCLKCTRL_SEL_PLL0:
		src_freq = tcc_ckc_pll_get_rate(PLL_0);
		break;
	case PCLKCTRL_SEL_PLL1:
		src_freq = tcc_ckc_pll_get_rate(PLL_1);
		break;
	case PCLKCTRL_SEL_PLL2:
		src_freq = tcc_ckc_pll_get_rate(PLL_2);
		break;
	case PCLKCTRL_SEL_PLL3:
		src_freq = tcc_ckc_pll_get_rate(PLL_3);
		break;
	case PCLKCTRL_SEL_PLL4:
		src_freq = tcc_ckc_pll_get_rate(PLL_4);
		break;
	case PCLKCTRL_SEL_XIN:
		src_freq = XIN_CLK_RATE;
		break;
	case PCLKCTRL_SEL_XTIN:
		src_freq = XTIN_CLK_RATE;
		break;
	case PCLKCTRL_SEL_PLL0DIV:
		src_freq = tcc_ckc_plldiv_get_rate(PLL_0);
		break;
	case PCLKCTRL_SEL_PLL1DIV:
		src_freq = tcc_ckc_plldiv_get_rate(PLL_1);
		break;
	case PCLKCTRL_SEL_PLL2DIV:
		src_freq = tcc_ckc_plldiv_get_rate(PLL_2);
		break;
	case PCLKCTRL_SEL_PLL3DIV:
		src_freq = tcc_ckc_plldiv_get_rate(PLL_3);
		break;
	case PCLKCTRL_SEL_PLL4DIV:
		src_freq = tcc_ckc_plldiv_get_rate(PLL_4);
		break;
	case PCLKCTRL_SEL_PCIPHY_CLKOUT:
	case PCLKCTRL_SEL_HDMITMDS:
	case PCLKCTRL_SEL_HDMIPCLK:
	case PCLKCTRL_SEL_HDMIXIN:
	case PCLKCTRL_SEL_XINDIV:
	case PCLKCTRL_SEL_XTINDIV:
	case PCLKCTRL_SEL_EXTCLK0:
	case PCLKCTRL_SEL_EXTCLK1:
	default:
		return 0;
	}

	switch (type) {
	case PCLKCTRL_TYPE_XXX:
		div_mask = PCLKCTRL_DIV_XXX_MASK;
		nPCLKCTRL.md = PCLKCTRL_MODE_DIVIDER;
		break;
	case PCLKCTRL_TYPE_YYY:
		div_mask = PCLKCTRL_DIV_YYY_MASK;
		break;
	default:
		return 0;
	}
	nPCLKCTRL.freq = 0;
	nPCLKCTRL.div = (reg_values &
			(div_mask << PCLKCTRL_DIV_SHIFT)) >>
			 PCLKCTRL_DIV_SHIFT;

	if (nPCLKCTRL.md == PCLKCTRL_MODE_DIVIDER)
		nPCLKCTRL.freq = src_freq / (nPCLKCTRL.div + 1);
	else {
		u64 u64_tmp;

		if (nPCLKCTRL.div > (div_mask + 1) / 2) {
			u64_tmp = (unsigned long long)src_freq *
				  (unsigned long long)((div_mask + 1) -
							nPCLKCTRL.div);
		} else {
			u64_tmp = (unsigned long long)src_freq *
				  (unsigned long long)nPCLKCTRL.div;
		}
		do_div(u64_tmp, div_mask + 1);
		nPCLKCTRL.freq = (unsigned int)u64_tmp;
	}
	return (unsigned long)nPCLKCTRL.freq;
}

static int tcc_ckc_get_peri_div(int id)
{
	uintptr_t reg = ckc_base+CKC_PCLKCTRL + (id * 4);

	if (tcc_check_pclk_type(id) == PCLKCTRL_TYPE_YYY) {
		return (ckc_readl(reg) >> PCLKCTRL_DIV_SHIFT) &
			PCLKCTRL_DIV_YYY_MASK;
	} else {
		return (ckc_readl(reg) >> PCLKCTRL_DIV_SHIFT) &
			PCLKCTRL_DIV_XXX_MASK;
	}
}

static int tcc_ckc_get_peri_src(int id)
{
	uintptr_t reg = ckc_base+CKC_PCLKCTRL + (id * 4);

	return (ckc_readl(reg) >> PCLKCTRL_SEL_SHIFT) & PCLKCTRL_SEL_MASK;
}

static inline void tcc_ckc_reset_clock_source(int id)
{
	if (id >= MAX_CLK_SRC)
		return;

	if (id < MAX_TCC_PLL) {
		stClockSource[id] = tcc_ckc_pll_get_rate(id);
		stClockSource[MAX_TCC_PLL + id] = tcc_ckc_plldiv_get_rate(id);
	}
}

static int tcc_ckc_iobus_pwdn(int id, bool pwdn)
{
	void __iomem *ckc_reg = ckc_base + CKC_CLKCTRL + (FBUS_IO * 0x4);
	void __iomem *reg;

	if ((ckc_readl(ckc_reg) & (1 << CLKCTRL_EN_SHIFT)) == 0)
		return -2;

	if (id < (32 * 1))
		reg = iobus_cfg_base + IOBUS_PWDN0;
	else if (id < (32 * 2))
		reg = iobus_cfg_base + IOBUS_PWDN1;
	else if (id < (32 * 3))
		reg = iobus_cfg_base + IOBUS_PWDN2;
	else if (id < (32 * 4))
		reg = iobus_cfg_base + IOBUS_PWDN3;
	else if (id < (32 * 5))
		reg = iobus_cfg_base1 + IOBUS_PWDN4;
	else
		return -1;

	id %= 32;

	if (pwdn)
		ckc_writel(ckc_readl(reg) & ~(1 << id), reg);
	else
		ckc_writel(ckc_readl(reg) | (1 << id), reg);

	return 0;
}

static int tcc_ckc_is_iobus_pwdn(int id)
{
	void __iomem *ckc_reg = ckc_base + CKC_CLKCTRL + (FBUS_IO * 0x4);
	void __iomem *reg;

	if ((ckc_readl(ckc_reg) & (1 << CLKCTRL_EN_SHIFT)) == 0)
		return -2;

	if (id < (32 * 1))
		reg = iobus_cfg_base + IOBUS_PWDN0;
	else if (id < (32 * 2))
		reg = iobus_cfg_base + IOBUS_PWDN1;
	else if (id < (32 * 3))
		reg = iobus_cfg_base + IOBUS_PWDN2;
	else if (id < (32 * 4))
		reg = iobus_cfg_base + IOBUS_PWDN3;
	else if (id < (32 * 5))
		reg = iobus_cfg_base1 + IOBUS_PWDN4;
	else
		return -1;

	id %= 32;

	return (ckc_readl(reg) & (1 << id)) ? 0 : 1;
}

static int tcc_ckc_iobus_swreset(int id, bool reset)
{
	void __iomem *ckc_reg = ckc_base + CKC_CLKCTRL + (FBUS_IO * 0x4);
	void __iomem *reg;

	if ((ckc_readl(ckc_reg) & (1 << CLKCTRL_EN_SHIFT)) == 0)
		return -2;

	if (id < (32 * 1))
		reg = iobus_cfg_base + IOBUS_RESET0;
	else if (id < (32 * 2))
		reg = iobus_cfg_base + IOBUS_RESET1;
	else if (id < (32 * 3))
		reg = iobus_cfg_base + IOBUS_RESET2;
	else if (id < (32 * 4))
		reg = iobus_cfg_base + IOBUS_RESET3;
	else if (id < (32 * 5))
		reg = iobus_cfg_base1 + IOBUS_RESET4;
	else
		return -1;

	id %= 32;

	if (reset)
		ckc_writel(ckc_readl(reg) & ~(1 << id), reg);
	else
		ckc_writel(ckc_readl(reg) | (1 << id), reg);

	return 0;
}

static int tcc_ckc_ddibus_pwdn(int id, bool pwdn)
{
	void __iomem *ckc_reg = ckc_base + CKC_CLKCTRL + (FBUS_DDI * 0x4);
	void __iomem *reg = ddibus_cfg_base + DDIBUS_PWDN;

	if (id >= DDIBUS_MAX)
		return -1;

	if ((ckc_readl(ckc_reg) & (1 << CLKCTRL_EN_SHIFT)) == 0)
		return -2;

	if (pwdn)
		ckc_writel(ckc_readl(reg) & ~(1 << id), reg);
	else
		ckc_writel(ckc_readl(reg) | (1 << id), reg);

	return 0;
}

static int tcc_ckc_is_ddibus_pwdn(int id)
{
	void __iomem *ckc_reg = ckc_base + CKC_CLKCTRL + (FBUS_DDI * 0x4);
	void __iomem *reg = ddibus_cfg_base + DDIBUS_PWDN;

	if (id >= DDIBUS_MAX)
		return -1;

	if ((ckc_readl(ckc_reg) & (1 << CLKCTRL_EN_SHIFT)) == 0)
		return -2;

	return (ckc_readl(reg) & (0x1 << id)) ? 0 : 1;
}

static int tcc_ckc_ddibus_swreset(int id, bool reset)
{
	void __iomem *ckc_reg = ckc_base + CKC_CLKCTRL + (FBUS_DDI * 0x4);
	void __iomem *reg = ddibus_cfg_base + DDIBUS_RESET;

	if (id >= DDIBUS_MAX)
		return -1;

	if ((ckc_readl(ckc_reg) & (1 << CLKCTRL_EN_SHIFT)) == 0)
		return -2;

	if (reset)
		ckc_writel(ckc_readl(reg) & ~(1 << id), reg);
	else
		ckc_writel(ckc_readl(reg) | (1 << id), reg);

	return 0;
}

static int tcc_ckc_vpubus_pwdn(int id, bool pwdn)
{
	void __iomem *reg = vpubus_cfg_base + VPUBUS_PWDN;
	u32 val = 1 << id;

	if (id >= VIDEOBUS_MAX)
		return -1;

	/*
	 * XXX : WAVE410/CODA960 core/bus clocks should be set simultaneously.
	 */

	if (id == VIDEOBUS_HEVC_BUS || id == VIDEOBUS_CODA_BUS)
		return 0;
	if (id == VIDEOBUS_HEVC_CORE)
		val |= 1 << VIDEOBUS_HEVC_BUS;
	if (id == VIDEOBUS_CODA_CORE)
		val |= 1 << VIDEOBUS_CODA_BUS;

	if (pwdn)
		ckc_writel(ckc_readl(reg) & ~(val), reg);
	else
		ckc_writel(ckc_readl(reg) | (val), reg);

	return 0;
}

static int tcc_ckc_is_vpubus_pwdn(int id)
{
	void __iomem *reg = vpubus_cfg_base + VPUBUS_PWDN;

	if (id >= VIDEOBUS_MAX)
		return -1;

	return (ckc_readl(reg) & (0x1 << id)) ? 0 : 1;
}

static int tcc_ckc_vpubus_swreset(int id, bool reset)
{
	void __iomem *reg = vpubus_cfg_base + VPUBUS_RESET;
	u32 val = 1 << id;

	if (id >= VIDEOBUS_MAX)
		return -1;

	/*
	 * XXX : WAVE410/CODA960 core/bus resets should be set simultaneously.
	 */

	if (id == VIDEOBUS_HEVC_BUS || id == VIDEOBUS_CODA_BUS)
		return 0;
	if (id == VIDEOBUS_HEVC_CORE)
		val |= 1 << VIDEOBUS_HEVC_BUS;
	if (id == VIDEOBUS_CODA_CORE)
		val |= 1 << VIDEOBUS_CODA_BUS;

	if (reset)
		ckc_writel(ckc_readl(reg) & ~(val), reg);
	else
		ckc_writel(ckc_readl(reg) | (val), reg);

	return 0;
}

static int tcc_ckc_hsiobus_pwdn(int id, bool pwdn)
{
	void __iomem *ckc_reg = ckc_base + CKC_CLKCTRL + (FBUS_HSIO * 0x4);
	void __iomem *reg = hsiobus_cfg_base + HSIOBUS_PWDN;

	if (id >= HSIOBUS_MAX)
		return -1;

	if ((ckc_readl(ckc_reg) & (1 << CLKCTRL_EN_SHIFT)) == 0)
		return -2;

	if (pwdn)
		/* do not disable trng */
		if (id != HSIOBUS_TRNG)
			ckc_writel(ckc_readl(reg) & ~(1 << id), reg);
	else
		ckc_writel(ckc_readl(reg) | (1 << id), reg);

	return 0;
}

static int tcc_ckc_is_hsiobus_pwdn(int id)
{
	void __iomem *ckc_reg = ckc_base + CKC_CLKCTRL + (FBUS_HSIO * 0x4);
	void __iomem *reg = hsiobus_cfg_base + HSIOBUS_PWDN;

	if (id >= HSIOBUS_MAX)
		return -1;

	if ((ckc_readl(ckc_reg) & (1 << CLKCTRL_EN_SHIFT)) == 0)
		return -2;

	return (ckc_readl(reg) & (0x1 << id)) ? 0 : 1;
}

static int tcc_ckc_hsiobus_swreset(int id, bool reset)
{
	void __iomem *ckc_reg = ckc_base + CKC_CLKCTRL + (FBUS_HSIO * 0x4);
	void __iomem *reg = hsiobus_cfg_base + HSIOBUS_RESET;

	if (id >= HSIOBUS_MAX)
		return -1;

	if ((ckc_readl(ckc_reg) & (1 << CLKCTRL_EN_SHIFT)) == 0)
		return -2;

	if (reset)
		/* do not disable trng */
		if (id != HSIOBUS_TRNG)
			ckc_writel(ckc_readl(reg) & ~(1 << id), reg);
	else
		ckc_writel(ckc_readl(reg) | (1 << id), reg);

	return 0;
}

static int tcc_ckc_swreset(unsigned int id, bool reset)
{
	void __iomem *reg = ckc_base + CKC_SWRESET;

	if (reg == NULL) {
		/* if reg is NULL */
		return -1;
	}

	if (reset)
		ckc_writel(ckc_readl(reg) & ~id, reg);
	else
		ckc_writel(ckc_readl(reg) | id, reg);

	return 0;
}

static int tcc_ckc_ip_pwdn(int id, bool pwdn)
{
#if 0
	void __iomem *reg = pmu_base + PMU_ISOIP_TOP;
	unsigned int value;

	if (id >= ISOIP_TOP_MAX)
		return -1;

	value = ckc_readl(reg);
	if (pwdn)
		value |= (1 << id);
	else
		value &= ~(1 << id);
	ckc_writel(value, reg);
#endif

	return 0;
}

static int tcc_ckc_is_ip_pwdn(int id)
{
#if 0
	void __iomem *reg = pmu_base + PMU_ISOIP_TOP;

	if (id >= ISOIP_TOP_MAX)
		return -1;

	return (ckc_readl(reg) & (1 << id)) ? 1 : 0;
#endif
	return 0;
}

static int tcc_ckc_isoip_ddi_pwdn(int id, bool pwdn)
{
#if 0
	void __iomem *reg = pmu_base + PMU_ISOIP_DDI;
	unsigned int value;

	if (id >= ISOIP_DDB_MAX)
		return -1;

	value = ckc_readl(reg);
	if (pwdn)
		value |= (1 << id);
	else
		value &= ~(1 << id);
	ckc_writel(value, reg);
#endif

	return 0;
}

static int tcc_ckc_is_isoip_ddi_pwdn(int id)
{
#if 0
	void __iomem *reg = pmu_base + PMU_ISOIP_DDI;

	if (id >= ISOIP_DDB_MAX)
		return -1;

	return (ckc_readl(reg) & (1 << id)) ? 1 : 0;
#endif
	return 0;
}

/**********************************
 *  MISC. Functions
 **********************************/
struct ckc_backup_sts {
	unsigned long rate;
	unsigned long en;
};

struct ckc_backup_reg {
	struct ckc_backup_sts pll[MAX_TCC_PLL];
	struct ckc_backup_sts clk[FBUS_MAX];
	struct ckc_backup_sts peri[PERI_MAX];
	unsigned long display_sub[2];
	unsigned long io_sub[8];
	unsigned long hsio_sub[2];
	unsigned long video_sub[2];
	unsigned long plldiv[2];
	unsigned long isoip_pwdn[2];
};

static struct ckc_backup_reg *ckc_backup;

void tcc_ckc_save(unsigned int clk_down)
{
	void __iomem *peri_reg = ckc_base + CKC_PCLKCTRL;
	int i;

#if 0
	BUG_ON(ckc_backup);
	ckc_backup = kzalloc(sizeof(struct ckc_backup_reg), GFP_KERNEL);
	BUG_ON(!ckc_backup);

	/* PCLKCTRL */
	for (i = 0; i < PERI_MAX; i++) {
		ckc_backup->peri[i].rate = tcc_ckc_peri_get_rate(i);
		ckc_backup->peri[i].en = tcc_ckc_is_peri_enabled(i);
#ifdef CONFIG_TCC_WAKE_ON_LAN
		if (i == PERI_GMAC || i == PERI_GMAC_PTP)
			continue;
#endif
		if (clk_down)
			tcc_pclkctrl_write((peri_reg + (i * 4)), 0,
					   ckc_backup->peri[i].en,
					   PCLKCTRL_SEL_XIN, 1,
					   tcc_check_pclk_type(i));
	}

	/* CLKCTRL */
	for (i = 0; i < FBUS_MAX; i++) {
		ckc_backup->clk[i].rate = tcc_ckc_clkctrl_get_rate(i);
		ckc_backup->clk[i].en = tcc_ckc_is_clkctrl_enabled(i);
		/* save sub-block pwdn/swreset */
		if (ckc_backup->clk[i].en) {
			switch (i) {
			case FBUS_VBUS:
				ckc_backup->video_sub[0] =
				    ckc_readl(vpubus_cfg_base + VPUBUS_PWDN);
				ckc_backup->video_sub[1] =
				    ckc_readl(vpubus_cfg_base + VPUBUS_RESET);
				break;
			case FBUS_HSIO:
				ckc_backup->hsio_sub[0] =
				    ckc_readl(hsiobus_cfg_base + HSIOBUS_PWDN);
				ckc_backup->hsio_sub[1] =
				    ckc_readl(hsiobus_cfg_base + HSIOBUS_RESET);
				break;
			case FBUS_DDI:
				ckc_backup->display_sub[0] =
				    ckc_readl(ddibus_cfg_base + DDIBUS_PWDN);
				ckc_backup->display_sub[1] =
				    ckc_readl(ddibus_cfg_base + DDIBUS_RESET);
				break;
			case FBUS_IO:
				ckc_backup->io_sub[0] =
				    ckc_readl(iobus_cfg_base + IOBUS_PWDN0);
				ckc_backup->io_sub[1] =
				    ckc_readl(iobus_cfg_base + IOBUS_RESET0);
				ckc_backup->io_sub[2] =
				    ckc_readl(iobus_cfg_base + IOBUS_PWDN1);
				ckc_backup->io_sub[3] =
				    ckc_readl(iobus_cfg_base + IOBUS_RESET1);
				ckc_backup->io_sub[4] =
				    ckc_readl(iobus_cfg_base + IOBUS_PWDN2);
				ckc_backup->io_sub[5] =
				    ckc_readl(iobus_cfg_base + IOBUS_RESET2);
				ckc_backup->io_sub[6] =
				    ckc_readl(iobus_cfg_base1 + IOBUS_PWDN3);
				ckc_backup->io_sub[7] =
				    ckc_readl(iobus_cfg_base1 + IOBUS_RESET3);
				break;
			}
		}

		if (i == FBUS_CPU0 || i == FBUS_CPU1 || i == FBUS_CBUS
		    || i == FBUS_CMBUS || i == FBUS_MEM || i == FBUS_SMU
		    || i == FBUS_IO || i == FBUS_MEM_PHY)
			;
#ifdef CONFIG_TCC_WAKE_ON_LAN
		else if (i == FBUS_HSIO)
			continue;
#endif
		else if (clk_down) {
			tcc_ckc_clkctrl_disable(i);
			if (tcc803x_ops.ckc_pmu_pwdn)
				tcc803x_ops.ckc_pmu_pwdn(i, true);
		}
	}

	/* PLL */
	ckc_backup->plldiv[0] = ckc_readl(ckc_base + CKC_CLKDIVC);
	ckc_backup->plldiv[1] = ckc_readl(ckc_base + CKC_CLKDIVC + 0x4);
	for (i = 0; i < MAX_TCC_PLL; i++)
		ckc_backup->pll[i].rate = tcc_ckc_pll_get_rate(i);

	/* PMU IP */
	ckc_backup->isoip_pwdn[0] = ckc_readl(pmu_base + PMU_ISOIP_TOP);
	ckc_backup->isoip_pwdn[0] = ckc_readl(pmu_base + PMU_ISOIP_DDI);

	if (clk_down) {
		ckc_writel(ckc_backup->isoip_pwdn[0] | 0xFFFFFFF9,
			pmu_base+PMU_ISOL);
		/* phys pwdn except (PLL, RTC) */
		ckc_writel(ckc_backup->isoip_pwdn[1] | 0xFFFFFFF9,
			pmu_base+PMU_ISOL);
		/* phys pwdn except (PLL, RTC) */
	}
#endif
}

#ifdef CONFIG_SNAPSHOT_BOOT
unsigned int do_hibernate_boot;
#endif //CONFIG_SNAPSHOT_BOOT
void tcc_ckc_restore(void)
{
#if 0
	void __iomem *clk_reg = ckc_base + CKC_CLKCTRL;
	int i;

	tcc_clkctrl_write((clk_reg + (FBUS_IO * 4)), 1, 1, CLKCTRL_SEL_XIN);
	tcc_clkctrl_write((clk_reg + (FBUS_SMU * 4)), 1, 1, CLKCTRL_SEL_XIN);
	tcc_clkctrl_write((clk_reg + (FBUS_HSIO * 4)), 1, 1, CLKCTRL_SEL_XIN);
	tcc_clkctrl_write((clk_reg + (FBUS_CMBUS * 4)), 1, 1, CLKCTRL_SEL_XIN);

	BUG_ON(!ckc_backup);

	/* PMU IP */
	ckc_writel(ckc_backup->isoip_pwdn[0], pmu_base + PMU_ISOIP_TOP);
	ckc_writel(ckc_backup->isoip_pwdn[1], pmu_base + PMU_ISOIP_DDI);

#ifdef CONFIG_SNAPSHOT_BOOT
	if (!do_hibernate_boot)
#endif
	{
		/* PLL */
		ckc_writel(ckc_backup->plldiv[0], ckc_base + CKC_CLKDIVC);
		ckc_writel(ckc_backup->plldiv[1], ckc_base + CKC_CLKDIVC + 0x4);
		for (i = 0; i < (MAX_TCC_PLL); i++) {
			if (i == MEMBUS_PLL)
				continue;
			tcc_ckc_pll_set_rate(i, ckc_backup->pll[i].rate);
		}
	}

	/* CLKCTRL */
	for (i = 0; i < FBUS_MAX; i++) {
		if (i == FBUS_MEM || i == FBUS_MEM_PHY)
			continue;

		if (ckc_backup->clk[i].en) {
			if (tcc803x_ops.ckc_pmu_pwdn)
				tcc803x_ops.ckc_pmu_pwdn(i, false);
			tcc_ckc_clkctrl_enable(i);

			/* restore sub-block pwdn/swreset */
			switch (i) {
			case FBUS_DDI:
				ckc_writel(ckc_backup->display_sub[1],
					   ddibus_cfg_base + DDIBUS_RESET);
				ckc_writel(ckc_backup->display_sub[0],
					   ddibus_cfg_base + DDIBUS_PWDN);
				break;
			case FBUS_IO:
				ckc_writel(ckc_backup->io_sub[1],
					   iobus_cfg_base + IOBUS_RESET0);
				ckc_writel(ckc_backup->io_sub[0],
					   iobus_cfg_base + IOBUS_PWDN0);
				ckc_writel(ckc_backup->io_sub[3],
					   iobus_cfg_base + IOBUS_RESET1);
				ckc_writel(ckc_backup->io_sub[2],
					   iobus_cfg_base + IOBUS_PWDN1);
				ckc_writel(ckc_backup->io_sub[5],
					   iobus_cfg_base + IOBUS_RESET2);
				ckc_writel(ckc_backup->io_sub[4],
					   iobus_cfg_base + IOBUS_PWDN2);
				ckc_writel(ckc_backup->io_sub[7],
					   iobus_cfg_base1 + IOBUS_RESET3);
				ckc_writel(ckc_backup->io_sub[6],
					   iobus_cfg_base1 + IOBUS_PWDN3);
				break;
			case FBUS_VBUS:
				ckc_writel(ckc_backup->video_sub[1],
					   vpubus_cfg_base + VPUBUS_RESET);
				ckc_writel(ckc_backup->video_sub[0],
					   vpubus_cfg_base + VPUBUS_PWDN);
				break;
			case FBUS_HSIO:
				ckc_writel(ckc_backup->hsio_sub[1],
					   hsiobus_cfg_base + HSIOBUS_RESET);
				ckc_writel(ckc_backup->hsio_sub[0],
					   hsiobus_cfg_base + HSIOBUS_PWDN);
				break;
			}
		} else {
			tcc_ckc_clkctrl_disable(i);
			if (tcc803x_ops.ckc_pmu_pwdn)
				tcc803x_ops.ckc_pmu_pwdn(i, true);
		}
		// except dedicated clocks
		if (i == FBUS_GPU || i == FBUS_G2D)
			continue;
		tcc_ckc_clkctrl_set_rate(i, ckc_backup->clk[i].rate);
		if (tcc_ckc_clkctrl_get_rate(i) < ckc_backup->clk[i].rate)
			tcc_ckc_clkctrl_set_rate(i,
						 ckc_backup->clk[i].rate + 1);
		/* Add 1Hz? */
	}

	/* PCLKCTRL */
	for (i = 0; i < PERI_MAX; i++) {
#ifdef CONFIG_SNAPSHOT_BOOT
		if (do_hibernate_boot && (i == PERI_LCD0 || i == PERI_LCD1))
			continue;
#endif
		tcc_ckc_peri_set_rate(i, ckc_backup->peri[i].rate ?
				      : XIN_CLK_RATE);

		if (ckc_backup->peri[i].en)
			tcc_ckc_peri_enable(i);
		else
			tcc_ckc_peri_disable(i);
	}

	kfree(ckc_backup);
	ckc_backup = NULL;
#endif
}

static struct tcc_ckc_ops tcc803x_ops = {
	.ckc_pmu_pwdn		 = NULL,
	.ckc_is_pmu_pwdn	 = NULL,
	.ckc_swreset		 = NULL,
	.ckc_isoip_top_pwdn	 = &tcc_ckc_ip_pwdn,
	.ckc_is_isoip_top_pwdn	 = &tcc_ckc_is_ip_pwdn,
	.ckc_isoip_ddi_pwdn	 = &tcc_ckc_isoip_ddi_pwdn,
	.ckc_is_isoip_ddi_pwdn	 = &tcc_ckc_is_isoip_ddi_pwdn,
	.ckc_pll_set_rate	 = &tcc_ckc_pll_set_rate,
	.ckc_pll_get_rate	 = &tcc_ckc_pll_get_rate,
	.ckc_clkctrl_enable	 = &tcc_ckc_clkctrl_enable,
	.ckc_clkctrl_disable	 = &tcc_ckc_clkctrl_disable,
	.ckc_clkctrl_set_rate	 = &tcc_ckc_clkctrl_set_rate,
	.ckc_clkctrl_get_rate	 = &tcc_ckc_clkctrl_get_rate,
	.ckc_is_clkctrl_enabled	 = &tcc_ckc_is_clkctrl_enabled,
	.ckc_peri_enable	 = &tcc_ckc_peri_enable,
	.ckc_peri_disable	 = &tcc_ckc_peri_disable,
	.ckc_peri_set_rate	 = &tcc_ckc_peri_set_rate,
	.ckc_peri_get_rate	 = &tcc_ckc_peri_get_rate,
	.ckc_is_peri_enabled	 = &tcc_ckc_is_peri_enabled,
	.ckc_ddibus_pwdn	 = &tcc_ckc_ddibus_pwdn,
	.ckc_is_ddibus_pwdn	 = &tcc_ckc_is_ddibus_pwdn,
	.ckc_ddibus_swreset	 = &tcc_ckc_ddibus_swreset,
	.ckc_gpubus_pwdn	 = NULL,
	.ckc_is_gpubus_pwdn	 = NULL,
	.ckc_gpubus_swreset	 = NULL,
	.ckc_iobus_pwdn		 = &tcc_ckc_iobus_pwdn,
	.ckc_is_iobus_pwdn	 = &tcc_ckc_is_iobus_pwdn,
	.ckc_iobus_swreset	 = &tcc_ckc_iobus_swreset,
	.ckc_vpubus_pwdn	 = &tcc_ckc_vpubus_pwdn,
	.ckc_is_vpubus_pwdn	 = &tcc_ckc_is_vpubus_pwdn,
	.ckc_vpubus_swreset	 = &tcc_ckc_vpubus_swreset,
	.ckc_hsiobus_pwdn	 = &tcc_ckc_hsiobus_pwdn,
	.ckc_is_hsiobus_pwdn	 = &tcc_ckc_is_hsiobus_pwdn,
	.ckc_hsiobus_swreset	 = &tcc_ckc_hsiobus_swreset,
	.ckc_g2dbus_pwdn	 = NULL,
	.ckc_is_g2dbus_pwdn	 = NULL,
	.ckc_g2dbus_swreset	 = NULL,
	.ckc_cmbus_pwdn		 = NULL,
	.ckc_is_cmbus_pwdn	 = NULL,
	.ckc_cmbus_swreset	 = NULL,
};

static int __init tcc803x_ckc_init(struct device_node *np)
{
	int i;

	ckc_base = of_iomap(np, 0);
	BUG_ON(!ckc_base);
	pmu_base = of_iomap(np, 1);
	BUG_ON(!pmu_base);
	membus_cfg_base = of_iomap(np, 2);
	BUG_ON(!membus_cfg_base);
	ddibus_cfg_base = of_iomap(np, 3);
	BUG_ON(!ddibus_cfg_base);
	iobus_cfg_base = of_iomap(np, 5);
	BUG_ON(!iobus_cfg_base);
	iobus_cfg_base1 = of_iomap(np, 6);
	BUG_ON(!iobus_cfg_base1);
	vpubus_cfg_base = of_iomap(np, 7);
	BUG_ON(!vpubus_cfg_base);
	hsiobus_cfg_base = of_iomap(np, 8);
	BUG_ON(!hsiobus_cfg_base);
	cpubus_cfg_base = of_iomap(np, 9);
	BUG_ON(!cpubus_cfg_base);
	gpu_3d_base = of_iomap(np, 10);
	BUG_ON(!gpu_3d_base);
	gpu_2d_base = of_iomap(np, 11);
	BUG_ON(!gpu_2d_base);
	mem_ckc_base = of_iomap(np, 12);
	BUG_ON(!mem_ckc_base);

	cpu0_base = cpubus_cfg_base + 0x00110000;
	cpu1_base = cpubus_cfg_base + 0x00210000;
	vbusckc_base = vpubus_cfg_base + 0x180000;

	/*
	 * get public PLLs, XIN and XTIN rates
	 */
	for (i = 0; i < MAX_TCC_PLL; i++)
		tcc_ckc_reset_clock_source(i);
	stClockSource[MAX_TCC_PLL * 2] = XIN_CLK_RATE;	/* XIN */

	if (!of_property_read_bool(np, "except-src-xtin"))
		stClockSource[MAX_TCC_PLL * 2 + 1] = XTIN_CLK_RATE;  /* XTIN */

	for (i = 0; i < MAX_TCC_PLL; i++) {
		if (stClockSource[i])
			pr_info("PLL_%d    : %10u Hz\n", i, stClockSource[i]);
	}
	for (; i < (MAX_TCC_PLL * 2); i++) {
		if (stClockSource[i])
			pr_info("PLLDIV_%d : %10u Hz\n", i - MAX_TCC_PLL,
			       stClockSource[i]);
	}
	pr_info("XIN      : %10u Hz\n", stClockSource[i++]);
	pr_info("XTIN     : %10u Hz\n", stClockSource[i++]);

	stClockG3D = tcc803x_ops.ckc_clkctrl_get_rate(FBUS_GPU);

	tcc_ckc_set_ops(&tcc803x_ops);
	//tcc_ckctest_set_ops(&tcc803x_ops);

	return 0;
}

CLK_OF_DECLARE(tcc_ckc, "telechips,ckc", tcc803x_ckc_init);

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static void *tcc_clk_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *tcc_clk_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void tcc_clk_stop(struct seq_file *m, void *v)
{
}

static int tcc_clk_show(struct seq_file *m, void *v)
{
	seq_printf(m, "cpu(a53_mp) : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_CPU0),
		   tcc_ckc_is_clkctrl_enabled(FBUS_CPU0) ? "" :
		   "(disabled)");
	seq_printf(m, "cpu(a7_sp)  : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_CPU1),
		   tcc_ckc_is_clkctrl_enabled(FBUS_CPU1) ? "" :
		   "(disabled)");
	seq_printf(m, "cpu(m4)     : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_CMBUS),
		   tcc_ckc_is_clkctrl_enabled(FBUS_CMBUS) ? "" :
		   "(disabled)");
	seq_printf(m, "cpu bus     : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_CBUS),
		   tcc_ckc_is_clkctrl_enabled(FBUS_CBUS) ? "" :
		   "(disabled)");
	seq_printf(m, "memory bus  : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_MEM),
		   tcc_ckc_is_clkctrl_enabled(FBUS_MEM) ? "" :
		   "(disabled)");
	seq_printf(m, "memory phy  : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_MEM_PHY),
		   tcc_ckc_is_clkctrl_enabled(FBUS_MEM_PHY) ? "" :
		   "(disabled)");
	seq_printf(m, "smu bus     : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_SMU),
		   tcc_ckc_is_clkctrl_enabled(FBUS_SMU) ? "" :
		   "(disabled)");
	seq_printf(m, "i/o bus     : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_IO),
		   tcc_ckc_is_clkctrl_enabled(FBUS_IO) ? "" :
		   "(disabled)");
	seq_printf(m, "hsio bus    : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_HSIO),
		   tcc_ckc_is_clkctrl_enabled(FBUS_HSIO) ? "" :
		   "(disabled)");
	seq_printf(m, "display bus : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_DDI),
		   tcc_ckc_is_clkctrl_enabled(FBUS_DDI) ? "" :
		   "(disabled)");
	seq_printf(m, "graphic 3d  : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_GPU),
		   tcc_ckc_is_clkctrl_enabled(FBUS_GPU) ? "" :
		   "(disabled)");
	seq_printf(m, "graphic 2d  : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_G2D),
		   tcc_ckc_is_clkctrl_enabled(FBUS_G2D) ? "" :
		   "(disabled)");
	seq_printf(m, "video bus   : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_VBUS),
		   tcc_ckc_is_clkctrl_enabled(FBUS_VBUS) ? "" :
		   "(disabled)");
	seq_printf(m, "codec(CODA) : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_CODA),
		   tcc_ckc_is_clkctrl_enabled(FBUS_CODA) ? "" :
		   "(disabled)");
	seq_printf(m, "hevc_c      : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_CHEVC),
		   tcc_ckc_is_clkctrl_enabled(FBUS_CHEVC) ? "" :
		   "(disabled)");
	seq_printf(m, "hevc_b      : %10lu Hz %s\n",
		   tcc_ckc_clkctrl_get_rate(FBUS_BHEVC),
		   tcc_ckc_is_clkctrl_enabled(FBUS_BHEVC) ? "" :
		   "(disabled)");
	return 0;
}

static const struct seq_operations tcc_clk_op = {
	.start = tcc_clk_start,
	.next = tcc_clk_next,
	.stop = tcc_clk_stop,
	.show = tcc_clk_show
};

static int tcc_clk_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &tcc_clk_op);
}

static const struct file_operations proc_tcc_clk_operations = {
	.open = tcc_clk_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int __init tcc_clk_proc_init(void)
{
	proc_create("clocks", 0000, NULL, &proc_tcc_clk_operations);
	return 0;
}

device_initcall(tcc_clk_proc_init);
