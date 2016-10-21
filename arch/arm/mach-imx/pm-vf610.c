/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2014 Toradex AG
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifdef DEBUG
#define pr_pmdebug(fmt, ...) pr_info("PM: VF610: " fmt "\n", ##__VA_ARGS__)
#else
#define pr_pmdebug(fmt, ...)
#endif
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/genalloc.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/imx6q-iomuxc-gpr.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#include <asm/fncpy.h>
#include <asm/proc-fns.h>
#include <asm/suspend.h>
#include <asm/tlb.h>

#include "common.h"

#define DDRMC_PHY_OFFSET		0x400

#define CCR				0x0
#define BM_CCR_FIRC_EN			(0x1 << 16)
#define BM_CCR_FXOSC_EN			(0x1 << 12)

#define CCSR				0x8
#define BM_CCSR_DDRC_CLK_SEL		(0x1 << 6)
#define BM_CCSR_FAST_CLK_SEL		(0x1 << 5)
#define BM_CCSR_SLOW_CLK_SEL		(0x1 << 4)
#define BM_CCSR_SYS_CLK_SEL_MASK	(0x7 << 0)

#define CACRR				0xc

#define CLPCR				0x2c
#define BM_CLPCR_ARM_CLK_DIS_ON_LPM	(0x1 << 5)
#define BM_CLPCR_SBYOS			(0x1 << 6)
#define BM_CLPCR_DIS_REF_OSC		(0x1 << 7)
#define BM_CLPCR_ANADIG_STOP_MODE	(0x1 << 8)
#define BM_CLPCR_FXOSC_BYPSEN		(0x1 << 10)
#define BM_CLPCR_FXOSC_PWRDWN		(0x1 << 11)
#define BM_CLPCR_MASK_CORE0_WFI		(0x1 << 22)
#define BM_CLPCR_MASK_CORE1_WFI		(0x1 << 23)
#define BM_CLPCR_MASK_SCU_IDLE		(0x1 << 24)
#define BM_CLPCR_MASK_L2CC_IDLE		(0x1 << 25)

#define CGPR				0x64
#define BM_CGPR_INT_MEM_CLK_LPM		(0x1 << 17)

#define GPC_PGCR			0x0
#define BM_PGCR_DS_STOP			(0x1 << 7)
#define BM_PGCR_DS_LPSTOP		(0x1 << 6)
#define BM_PGCR_WB_STOP			(0x1 << 4)
#define BM_PGCR_HP_OFF			(0x1 << 3)
#define BM_PGCR_PG_PD1			(0x1 << 0)

#define GPC_LPMR			0x40
#define BM_LPMR_RUN			0x0
#define BM_LPMR_STOP			0x2

#define ANATOP_PLL1_CTRL		0x270
#define ANATOP_PLL2_CTRL		0x30
#define ANATOP_PLL2_PFD			0x100
#define BM_PLL_POWERDOWN	(0x1 << 12)
#define BM_PLL_ENABLE		(0x1 << 13)
#define BM_PLL_BYPASS		(0x1 << 16)
#define BM_PLL_LOCK		(0x1 << 31)
#define BM_PLL_PFD2_CLKGATE	(0x1 << 15)
#define BM_PLL_USB_POWER	(0x1 << 12)
#define BM_PLL_EN_USB_CLKS	(0x1 << 6)

#define VF610_DDRMC_IO_NUM		94
#define VF610_IOMUX_DDR_IO_NUM		48
#define VF610_ANATOP_IO_NUM		2

static struct vf610_cpu_pm_info *pm_info;
static void __iomem *suspend_ocram_base;
static void (*vf610_suspend_in_ocram_fn)(void __iomem *ocram_vbase);
static bool mem_suspend_available;

#ifdef DEBUG
static void __iomem *uart_membase;
static unsigned long uart_clk;
#endif

static const u32 vf610_iomuxc_ddr_io_offset[] __initconst = {
	0x220, 0x224, 0x228, 0x22c, 0x230, 0x234, 0x238, 0x23c,
	0x240, 0x244, 0x248, 0x24c, 0x250, 0x254, 0x258, 0x25c,
	0x260, 0x264, 0x268, 0x26c, 0x270, 0x274, 0x278, 0x27c,
	0x280, 0x284, 0x288, 0x28c, 0x290, 0x294, 0x298, 0x29c,
	0x2a0, 0x2a4, 0x2a8, 0x2ac, 0x2b0, 0x2b4, 0x2b8, 0x2bc,
	0x2c0, 0x2c4, 0x2c8, 0x2cc, 0x2d0, 0x2d4, 0x2d8, 0x21c,
};


static const u32 vf610_ddrmc_io_offset[] __initconst = {
	0x00, 0x08, 0x28, 0x2c, 0x30, 0x34, 0x38,
	0x40, 0x44, 0x48, 0x50, 0x54, 0x58, 0x5c,
	0x60, 0x64, 0x68, 0x70, 0x74, 0x78, 0x7c,
	0x84, 0x88, 0x98, 0x9c, 0xa4, 0xc0,
	0x108, 0x10c, 0x114, 0x118, 0x120, 0x124,
	0x128, 0x12c, 0x130, 0x134, 0x138, 0x13c,
	0x148, 0x15c, 0x160, 0x164, 0x16c, 0x180,
	0x184, 0x188, 0x18c, 0x198, 0x1a4, 0x1a8,
	0x1b8, 0x1d4, 0x1d8, 0x1e0, 0x1e4, 0x1e8,
	0x1ec, 0x1f0, 0x1f8, 0x210, 0x224, 0x228,
	0x22c, 0x230, 0x23c, 0x240, 0x244, 0x248,
	0x24c, 0x250, 0x25c, 0x268, 0x26c, 0x278,
	0x268
};

static const u32 vf610_ddrmc_phy_io_offset[] __initconst = {
	0x00, 0x04, 0x08, 0x0c, 0x10,
	0x40, 0x44, 0x48, 0x4c, 0x50,
	0x80, 0x84, 0x88, 0x8c, 0x90,
	0xc4, 0xc8, 0xd0
};

/*
 * suspend ocram space layout:
 * ======================== high address ======================
 *                              .
 *                              .
 *                              .
 *                              ^
 *                              ^
 *                              ^
 *                      vf610_suspend code
 *              PM_INFO structure(vf610_cpu_pm_info)
 * ======================== low address =======================
 */

struct vf610_pm_base {
	phys_addr_t pbase;
	void __iomem *vbase;
};

struct vf610_pm_socdata {
	const char *anatop_compat;
	const char *scsc_compat;
	const char *wkpu_compat;
	const char *ccm_compat;
	const char *gpc_compat;
	const char *src_compat;
	const char *ddrmc_compat;
	const char *iomuxc_compat;
};

static const struct vf610_pm_socdata vf610_pm_data __initconst = {
	.anatop_compat = "fsl,vf610-anatop",
	.scsc_compat = "fsl,vf610-scsc",
	.wkpu_compat = "fsl,vf610-wkpu",
	.ccm_compat = "fsl,vf610-ccm",
	.gpc_compat = "fsl,vf610-gpc",
	.src_compat = "fsl,vf610-src",
	.ddrmc_compat = "fsl,vf610-ddrmc",
	.iomuxc_compat = "fsl,vf610-iomuxc",
};

/*
 * This structure is for passing necessary data for low level ocram
 * suspend code(arch/arm/mach-imx/suspend-vf610.S), if this struct
 * definition is changed, the offset definition in
 * arch/arm/mach-imx/suspend-vf610.S must be also changed accordingly,
 * otherwise, the suspend to ocram function will be broken!
 */
struct vf610_cpu_pm_info {
	phys_addr_t pbase; /* The physical address of pm_info. */
	phys_addr_t resume_addr; /* The physical resume address for asm code */
	u32 cpu_type; /* Currently not used, leave it for alignment */
	u32 pm_info_size; /* Size of pm_info. */
	struct vf610_pm_base anatop_base;
	struct vf610_pm_base scsc_base;
	struct vf610_pm_base wkpu_base;
	struct vf610_pm_base ccm_base;
	struct vf610_pm_base gpc_base;
	struct vf610_pm_base src_base;
	struct vf610_pm_base ddrmc_base;
	struct vf610_pm_base iomuxc_base;
	struct vf610_pm_base l2_base;
	u32 ccm_cacrr;
	u32 ccm_ccsr;
	u32 ddrmc_io_num; /* Number of MMDC IOs which need saved/restored. */
	u32 ddrmc_io_val[VF610_DDRMC_IO_NUM][2]; /* To save offset and value */
	u32 iomux_ddr_io_num;
	u32 iomux_ddr_io_val[VF610_IOMUX_DDR_IO_NUM][2];
} __aligned(8);

#ifdef DEBUG
static void vf610_uart_reinit(unsigned long int rate, unsigned long int baud)
{
	u8 tmp, c2;
	u16 sbr, brfa;

	/* UART_C2 */
	c2 = __raw_readb(uart_membase + 0x3);
	__raw_writeb(0, uart_membase + 0x3);

	sbr = (u16) (rate / (baud * 16));
	brfa = (rate / baud) - (sbr * 16);

	tmp = ((sbr & 0x1f00) >> 8);
	__raw_writeb(tmp, uart_membase + 0x0);
	tmp = sbr & 0x00ff;
	__raw_writeb(tmp, uart_membase + 0x1);

	/* UART_C4 */
	__raw_writeb(brfa & 0xf, uart_membase + 0xa);

	__raw_writeb(c2, uart_membase + 0x3);
}
#else
#define vf610_uart_reinit(rate, baud)
#endif

static void vf610_set(void __iomem *pll_base, u32 mask)
{
	writel(readl(pll_base) | mask, pll_base);
}

static void vf610_clr(void __iomem *pll_base, u32 mask)
{
	writel(readl(pll_base) & ~mask, pll_base);
}

int vf610_set_lpm(enum vf610_cpu_pwr_mode mode)
{
	void __iomem *ccm_base = pm_info->ccm_base.vbase;
	void __iomem *gpc_base = pm_info->gpc_base.vbase;
	void __iomem *anatop = pm_info->anatop_base.vbase;
	u32 ccr = readl_relaxed(ccm_base + CCR);
	u32 ccsr = readl_relaxed(ccm_base + CCSR);
	u32 cacrr = readl_relaxed(ccm_base + CACRR);
	u32 cclpcr = 0;
	u32 gpc_pgcr = 0;

	switch (mode) {
	case VF610_LP_STOP:
		/* Store clock settings */
		pm_info->ccm_ccsr = ccsr;
		pm_info->ccm_cacrr = cacrr;

		ccr |= BM_CCR_FIRC_EN;
		writel_relaxed(ccr, ccm_base + CCR);

		cclpcr |= BM_CLPCR_ANADIG_STOP_MODE;
		cclpcr |= BM_CLPCR_SBYOS;

		cclpcr |= BM_CLPCR_MASK_SCU_IDLE;
		cclpcr |= BM_CLPCR_MASK_L2CC_IDLE;
		cclpcr |= BM_CLPCR_MASK_CORE1_WFI;
		writel_relaxed(cclpcr, ccm_base + CLPCR);

		gpc_pgcr |= BM_PGCR_DS_STOP;
		gpc_pgcr |= BM_PGCR_DS_LPSTOP;
		gpc_pgcr |= BM_PGCR_WB_STOP;
		gpc_pgcr |= BM_PGCR_HP_OFF;
		gpc_pgcr |= BM_PGCR_PG_PD1;
		writel_relaxed(gpc_pgcr, gpc_base + GPC_PGCR);
		break;
	case VF610_STOP:
		cclpcr &= ~BM_CLPCR_ANADIG_STOP_MODE;
		cclpcr |= BM_CLPCR_ARM_CLK_DIS_ON_LPM;
		cclpcr |= BM_CLPCR_SBYOS;
		writel_relaxed(cclpcr, ccm_base + CLPCR);

		gpc_pgcr |= BM_PGCR_DS_STOP;
		gpc_pgcr |= BM_PGCR_HP_OFF;
		writel_relaxed(gpc_pgcr, gpc_base + GPC_PGCR);

		/* fall-through */
	case VF610_LP_RUN:
		/* Store clock settings */
		pm_info->ccm_ccsr = ccsr;
		pm_info->ccm_cacrr = cacrr;

		ccr |= BM_CCR_FIRC_EN;
		writel_relaxed(ccr, ccm_base + CCR);

		/* Enable PLL2 for DDR clock */
		vf610_set(anatop + ANATOP_PLL2_CTRL, BM_PLL_ENABLE);
		vf610_clr(anatop + ANATOP_PLL2_CTRL, BM_PLL_POWERDOWN);
		vf610_clr(anatop + ANATOP_PLL2_CTRL, BM_PLL_BYPASS);
		while (!(readl(anatop + ANATOP_PLL2_CTRL) & BM_PLL_LOCK));
		vf610_clr(anatop + ANATOP_PLL2_PFD, BM_PLL_PFD2_CLKGATE);

		/* Switch internal OSC's */
		ccsr &= ~BM_CCSR_FAST_CLK_SEL;
		ccsr &= ~BM_CCSR_SLOW_CLK_SEL;

		/* Select PLL2 as DDR clock */
		ccsr &= ~BM_CCSR_DDRC_CLK_SEL;
		writel_relaxed(ccsr, ccm_base + CCSR);

		ccsr &= ~BM_CCSR_SYS_CLK_SEL_MASK;
		writel_relaxed(ccsr, ccm_base + CCSR);
		vf610_uart_reinit(4000000UL, 115200);

		vf610_set(anatop + ANATOP_PLL1_CTRL, BM_PLL_BYPASS);
		writel_relaxed(BM_LPMR_STOP, gpc_base + GPC_LPMR);
		break;
	case VF610_RUN:
		writel_relaxed(BM_LPMR_RUN, gpc_base + GPC_LPMR);

		vf610_clr(anatop + ANATOP_PLL1_CTRL, BM_PLL_BYPASS);
		while(!(readl(anatop + ANATOP_PLL1_CTRL) & BM_PLL_LOCK));

		/* Restore clock settings */
		writel(pm_info->ccm_ccsr, ccm_base + CCSR);

		vf610_uart_reinit(uart_clk, 115200);
		pr_pmdebug("resuming, uart_reinit done");

		/* Disable PLL2 if not needed */
		if (pm_info->ccm_ccsr & BM_CCSR_DDRC_CLK_SEL)
			vf610_set(anatop + ANATOP_PLL2_CTRL, BM_PLL_POWERDOWN);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vf610_suspend_finish(unsigned long val)
{
	if (!vf610_suspend_in_ocram_fn) {
		cpu_do_idle();
	} else {
		/*
		 * call low level suspend function in ocram,
		 * as we need to float DDR IO.
		 */
		local_flush_tlb_all();
		flush_cache_all();
		outer_flush_all();
		vf610_suspend_in_ocram_fn(suspend_ocram_base);
	}

	return 0;
}

static int vf610_pm_enter(suspend_state_t state)
{
	switch (state) {
	case PM_SUSPEND_STANDBY:
		vf610_set_lpm(VF610_STOP);
		flush_cache_all();

		/* zzZZZzzz */
		cpu_do_idle();

		vf610_set_lpm(VF610_RUN);
		break;
	case PM_SUSPEND_MEM:
		vf610_set_lpm(VF610_LP_STOP);

		cpu_suspend(0, vf610_suspend_finish);
		outer_resume();

		vf610_set_lpm(VF610_RUN);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vf610_pm_valid(suspend_state_t state)
{
	return (state == PM_SUSPEND_STANDBY ||
		(state == PM_SUSPEND_MEM && mem_suspend_available));
}

static const struct platform_suspend_ops vf610_pm_ops = {
	.enter = vf610_pm_enter,
	.valid = vf610_pm_valid,
};

static int __init imx_pm_get_base(struct vf610_pm_base *base,
				const char *compat)
{
	struct device_node *node;
	struct resource res;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, compat);
	if (!node) {
		ret = -ENODEV;
		goto out;
	}

	ret = of_address_to_resource(node, 0, &res);
	if (ret)
		goto put_node;

	base->pbase = res.start;
	base->vbase = ioremap(res.start, resource_size(&res));

	if (!base->vbase)
		ret = -ENOMEM;

put_node:
	of_node_put(node);
out:
	return ret;
}

#ifdef DEBUG
static int __init vf610_uart_init(void)
{
	struct device_node *dn;
	const char *name;
	struct clk *clk;
	int ret;

	name = of_get_property(of_chosen, "stdout-path", NULL);
	if (name == NULL)
		return -ENODEV;

	dn = of_find_node_by_path(name);
	if (!dn)
		return -ENODEV;

	clk = of_clk_get(dn, 0);

	if (!clk) {
		ret = PTR_ERR(clk);
		goto put_node;
	}

	uart_clk = clk_get_rate(clk);

	uart_membase = of_iomap(dn, 0);
	if (!clk) {
		ret = -ENOMEM;
		goto put_node;
	}

	ret = 0;

put_node:
	of_node_put(dn);
	return ret;
}
#endif

static void vf610_power_off(void)
{
	void __iomem *gpc_base = pm_info->gpc_base.vbase;
	u32 gpc_pgcr;

	/*
	 * Power gate Power Domain 1
	 */
	gpc_pgcr = readl_relaxed(gpc_base + GPC_PGCR);
	gpc_pgcr |= BM_PGCR_PG_PD1;
	writel_relaxed(gpc_pgcr, gpc_base + GPC_PGCR);

	/* Set low power mode */
	vf610_set_lpm(VF610_STOP);
}

static int __init vf610_suspend_mem_init(const struct vf610_pm_socdata *socdata)
{
	struct device_node *node;
	struct platform_device *pdev;
	bool has_cke_reset_pulls = false;
	phys_addr_t ocram_pbase;
	struct gen_pool *ocram_pool;
	size_t ocram_size;
	unsigned long ocram_base;
	int ret = 0, reg = 0;
	int i;

	node = of_find_compatible_node(NULL, NULL, socdata->ddrmc_compat);
	if (node) {
		has_cke_reset_pulls =
			of_property_read_bool(node, "fsl,has-cke-reset-pulls");

		of_node_put(node);
	}

	if (!has_cke_reset_pulls) {
		pr_info("PM: No CKE/RESET pulls, disable Suspend-to-RAM\n");
		return -ENODEV;
	}

	node = of_find_compatible_node(NULL, NULL, "mmio-sram");
	if (!node) {
		pr_warn("%s: failed to find ocram node!\n", __func__);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		pr_warn("%s: failed to find ocram device!\n", __func__);
		ret = -ENODEV;
		goto put_node;
	}

	ocram_pool = gen_pool_get(&pdev->dev, "stbyram1");
	if (!ocram_pool) {
		pr_warn("%s: ocram pool unavailable!\n", __func__);
		ret = -ENODEV;
		goto put_node;
	}

	ocram_size = gen_pool_size(ocram_pool);
	ocram_base = gen_pool_alloc(ocram_pool, ocram_size);
	if (!ocram_base) {
		pr_warn("%s: unable to alloc ocram!\n", __func__);
		ret = -ENOMEM;
		goto put_node;
	}

	ocram_pbase = gen_pool_virt_to_phys(ocram_pool, ocram_base);

	suspend_ocram_base = __arm_ioremap_exec(ocram_pbase, ocram_size, false);

	pm_info = suspend_ocram_base;
	pm_info->pbase = ocram_pbase;
	pm_info->resume_addr = virt_to_phys(cpu_resume);

	pm_info->ddrmc_io_num = VF610_DDRMC_IO_NUM;

	ret = imx_pm_get_base(&pm_info->ddrmc_base, socdata->ddrmc_compat);
	if (ret) {
		pr_warn("%s: failed to get ddrmc base %d!\n", __func__, ret);
		goto put_node;
	}

	ret = imx_pm_get_base(&pm_info->iomuxc_base, socdata->iomuxc_compat);
	if (ret) {
		pr_warn("%s: failed to get iomuxc base %d!\n", __func__, ret);
		goto iomuxc_map_failed;
	}

	/* Store DDRMC registers */
	for (i = 0; i < ARRAY_SIZE(vf610_ddrmc_io_offset); i++, reg++) {
		pm_info->ddrmc_io_val[reg][0] = vf610_ddrmc_io_offset[i];
		pm_info->ddrmc_io_val[reg][1] =
			readl_relaxed(pm_info->ddrmc_base.vbase +
			vf610_ddrmc_io_offset[i]);
	}

	/* Store DDRMC PHY registers */
	for (i = 0; i < ARRAY_SIZE(vf610_ddrmc_phy_io_offset); i++, reg++) {
		pm_info->ddrmc_io_val[reg][0] = vf610_ddrmc_phy_io_offset[i] +
			DDRMC_PHY_OFFSET;
		pm_info->ddrmc_io_val[reg][1] =
			readl_relaxed(pm_info->ddrmc_base.vbase +
			DDRMC_PHY_OFFSET + vf610_ddrmc_phy_io_offset[i]);
	}

	/* Store IOMUX DDR pad registers */
	pm_info->iomux_ddr_io_num = VF610_IOMUX_DDR_IO_NUM;
	for (i = 0; i < ARRAY_SIZE(vf610_iomuxc_ddr_io_offset); i++) {
		pm_info->iomux_ddr_io_val[i][0] = vf610_iomuxc_ddr_io_offset[i];
		pm_info->iomux_ddr_io_val[i][1] =
			readl_relaxed(pm_info->iomuxc_base.vbase +
			vf610_iomuxc_ddr_io_offset[i]);
	}

	vf610_suspend_in_ocram_fn = fncpy(
		suspend_ocram_base + sizeof(*pm_info),
		&vf610_suspend, ocram_size - sizeof(*pm_info));

	pr_info("PM: CKE/RESET pulls available, enable Suspend-to-RAM\n");
	goto put_node;

iomuxc_map_failed:
	iounmap(pm_info->ddrmc_base.vbase);
put_node:
	of_node_put(node);

	return ret;
}

static int __init vf610_suspend_init(const struct vf610_pm_socdata *socdata)
{
	int ret;

#ifdef DEBUG
	ret = vf610_uart_init();
	if (ret < 0)
		return ret;
#endif

	if (vf610_suspend_mem_init(socdata)) {
		/*
		 * Suspend to memory for some reason not available, use DDR
		 * for standby mode
		 */
		pm_info = kzalloc(sizeof(*pm_info), GFP_KERNEL);
	} else {
		mem_suspend_available = true;
	}

	pm_info->pm_info_size = sizeof(*pm_info);

	ret = imx_pm_get_base(&pm_info->anatop_base, socdata->anatop_compat);
	if (ret) {
		pr_warn("%s: failed to get anatop base %d!\n", __func__, ret);
		return ret;
	}

	ret = imx_pm_get_base(&pm_info->scsc_base, socdata->scsc_compat);
	if (ret) {
		pr_warn("%s: failed to get scsc base %d!\n", __func__, ret);
		goto scsc_map_failed;
	}

	ret = imx_pm_get_base(&pm_info->ccm_base, socdata->ccm_compat);
	if (ret) {
		pr_warn("%s: failed to get ccm base %d!\n", __func__, ret);
		goto ccm_map_failed;
	}

	ret = imx_pm_get_base(&pm_info->gpc_base, socdata->gpc_compat);
	if (ret) {
		pr_warn("%s: failed to get gpc base %d!\n", __func__, ret);
		goto gpc_map_failed;
	}

	ret = imx_pm_get_base(&pm_info->src_base, socdata->src_compat);
	if (ret) {
		pr_warn("%s: failed to get src base %d!\n", __func__, ret);
		goto src_map_failed;
	}

	ret = imx_pm_get_base(&pm_info->l2_base, "arm,pl310-cache");
	if (ret == -ENODEV)
		ret = 0;
	if (ret) {
		pr_warn("%s: failed to get pl310-cache base %d!\n",
			__func__, ret);
		goto pl310_cache_map_failed;
	}

	suspend_set_ops(&vf610_pm_ops);

	return 0;

pl310_cache_map_failed:
	iounmap(pm_info->src_base.vbase);
src_map_failed:
	iounmap(pm_info->gpc_base.vbase);
gpc_map_failed:
	iounmap(pm_info->ccm_base.vbase);
ccm_map_failed:
	iounmap(pm_info->scsc_base.vbase);
scsc_map_failed:
	iounmap(pm_info->anatop_base.vbase);

	if (mem_suspend_available) {
		iounmap(pm_info->ddrmc_base.vbase);
		iounmap(pm_info->iomuxc_base.vbase);
	}

	return ret;
}

void __init vf610_pm_init(void)
{
	int ret;

	if (IS_ENABLED(CONFIG_SUSPEND)) {
		ret = vf610_suspend_init(&vf610_pm_data);
		if (ret)
			pr_warn("%s: No DDR LPM support with suspend %d!\n",
				__func__, ret);

		pm_power_off = vf610_power_off;
	}
}

