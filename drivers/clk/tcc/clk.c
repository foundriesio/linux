// SPDX-License-Identifier: GPL-2.0+
/*
 * Common clock driver for Telechips SoCs
 *
 * Copyright (C) 2014-2019 Telechips Inc.
 */

#define pr_fmt(fmt)	"tcc_clk: " fmt

#include <linux/version.h>
#include <linux/clkdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clocksource.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>
#include <linux/clk/tcc.h>
#include <dt-bindings/clock/telechips,clk-common.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0))
#define DEFINE_DEBUGFS_ATTRIBUTE DEFINE_SIMPLE_ATTRIBUTE
#endif

struct tcc_clk {
	struct clk_hw hw;
	struct clk_ops *ops;
	int id;
	int clk_src;
	struct list_head list;
};

static LIST_HEAD(tcc_clk_list);

static struct tcc_ckc_ops *ckc_ops = NULL;

#define to_tcc_clk(tcc) container_of(tcc, struct tcc_clk, hw)

#ifdef CONFIG_PLATFORM_AVN
/* do not disable PMU_PWDN & BUS clock & PMU IP ISOL */
#define IGNORE_CLK_DISABLE
#endif

static int tcc_debugfs_clk_enabled(void *data, u64 *val)
{
	struct tcc_clk *tcc = (struct tcc_clk *) data;

	if (!tcc->ops->is_enabled)
		return -ENOENT;

	*val = tcc->ops->is_enabled(&tcc->hw);
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(tcc_clk_enabled_fops, tcc_debugfs_clk_enabled,
		NULL, "%llu\n");

static int debugfs_clk_src_get(void *data, u64 *val)
{
	struct tcc_clk *tcc = (struct tcc_clk *) data;

	*val = tcc->clk_src;
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(clk_src_fops, debugfs_clk_src_get, NULL, "%llu\n");

static int tcc_clk_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct tcc_clk *tcc = to_tcc_clk(hw);

	clk_debugfs_add_file(hw, "clk_enabled", S_IRUGO, tcc,
			&tcc_clk_enabled_fops);

	clk_debugfs_add_file(hw, "clk_source", S_IRUGO, tcc,
			&clk_src_fops);
	return 0;
}

static struct clk *tcc_onecell_get(struct of_phandle_args *clkspec, void *data)
{
	struct clk_onecell_data *clk_data = data;
	unsigned int idx = clkspec->args[0];
	int i;

	for (i = 0; i < clk_data->clk_num; i++) {
		struct clk *clk = clk_data->clks[i];
		struct tcc_clk *tcc_clk;

		tcc_clk = to_tcc_clk(__clk_get_hw(clk));
		if (idx == tcc_clk->id)
			return clk;
	}
	return NULL;
}

static int tcc_clk_register(struct device_node *np, struct clk_ops *ops)
{
	struct clk_onecell_data *clk_data;
	struct tcc_clk *tcc_clk;
	struct clk *clk;
	int num_clks;
	int ret = 0;
	int i;

	num_clks = of_property_count_strings(np, "clock-output-names");
	pr_debug("%pOFfp: %s: # of clks = %d\n", np, __func__, num_clks);

	clk_data = kzalloc(sizeof(struct clk_onecell_data), GFP_KERNEL);
	if (!clk_data) {
		pr_err("failed to allocate clock provider context (%ld)\n",
			PTR_ERR(clk_data));
		return -ENOMEM;
	}

	clk_data->clks = kcalloc(num_clks, sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks) {
		pr_err("failed to allocate clks (%ld)\n",
				PTR_ERR(clk_data->clks));
		kfree(clk_data);
		return -ENOMEM;
	}

	for (i = 0; i < num_clks; i++) {
		struct clk_init_data init = { };
		const char **parent_names;
		const char *clk_name;
		u32 index;
		u32 flags = 0;

		tcc_clk = kzalloc(sizeof(tcc_clk), GFP_KERNEL);
		if (!tcc_clk) {
			pr_err("failed to allocate tcc clk\n");
			ret = PTR_ERR(tcc_clk);
			goto err;
		}

		ret = of_property_read_string_index(np, "clock-output-names",
					i, &clk_name);
		if (ret)
			goto err;

		ret = of_property_read_u32_index(np, "clock-indices",
					i, &index);
		if (ret)
			index = i;

		/* Optional properties for the clock flags */
		of_property_read_u32_index(np, "telechips,clock-flags",
				i, &flags);

		init.name = clk_name;
		init.num_parents = of_clk_get_parent_count(np);
		parent_names = kzalloc(sizeof(char *) * init.num_parents,
				GFP_KERNEL);
		of_clk_parent_fill(np, parent_names, init.num_parents);
		init.parent_names = parent_names;
		init.flags = flags;
#ifdef CONFIG_ARM64
		/* XXX: Do we need it? */
		init.flags |= CLK_IGNORE_UNUSED;
#endif
		init.ops = ops;
		tcc_clk->hw.init = &init;
		tcc_clk->ops = ops;
		tcc_clk->id = index;

		clk = clk_register(NULL, &tcc_clk->hw);
		if (IS_ERR_OR_NULL(clk)) {
			pr_err("failed to register clk '%s'\n", clk_name);
			kfree(tcc_clk);
			continue;
		}
		ret = clk_register_clkdev(clk, clk_name, NULL);
		if (ret) {
			pr_err("failed to register clkdev '%s' (%d)\n",
					clk_name, ret);
			clk_unregister(clk);
			kfree(tcc_clk);
			continue;
		}
		clk_data->clks[i] = clk;

		list_add_tail(&tcc_clk->list, &tcc_clk_list);

		pr_debug("%pOFfp: '%s' registered (index=%d)\n", np, clk_name, index);
	}
	clk_data->clk_num = i;
	return of_clk_add_provider(np, tcc_onecell_get, clk_data);

err:
	return ret;
}

void tcc_ckc_set_ops(struct tcc_ckc_ops *ops)
{
#if !defined(CONFIG_TCC803X_CA7S)
	#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
	ckc_ops = NULL;
	#else
	ckc_ops = ops;
	#endif
#else
	ckc_ops = ops;
#endif
}


/* common function */
static long tcc_round_rate(struct clk_hw *hw, unsigned long rate, unsigned long *best_parent_rate)
{
	return rate;
}


static int tcc_clkctrl_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_pmu_pwdn)
			ckc_ops->ckc_pmu_pwdn(tcc->id, false);
		if (ckc_ops->ckc_swreset)
			ckc_ops->ckc_swreset(tcc->id, false);
		if (ckc_ops->ckc_clkctrl_enable)
			ckc_ops->ckc_clkctrl_enable(tcc->id);
	}
	else {
		arm_smccc_smc(SIP_CLK_ENABLE_CLKCTRL, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}

	return 0;
}

#ifndef IGNORE_CLK_DISABLE
static void tcc_clkctrl_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_clkctrl_disable)
			ckc_ops->ckc_clkctrl_disable(tcc->id);
		if (ckc_ops->ckc_swreset)
			ckc_ops->ckc_swreset(tcc->id, true);
		if (ckc_ops->ckc_pmu_pwdn)
			ckc_ops->ckc_pmu_pwdn(tcc->id, true);
	}
	else {
		arm_smccc_smc(SIP_CLK_DISABLE_CLKCTRL, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}
}
#endif

static unsigned long tcc_clkctrl_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct arm_smccc_res res;
	unsigned long rate = 0;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_clkctrl_get_rate)
			rate = ckc_ops->ckc_clkctrl_get_rate(tcc->id);
	}
	else {
		arm_smccc_smc(SIP_CLK_GET_CLKCTRL, tcc->id, 0, 0, 0, 0, 0, 0, &res);
		rate = res.a0;
	}
	return rate;
}

static int tcc_clkctrl_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_clkctrl_set_rate)
			ckc_ops->ckc_clkctrl_set_rate(tcc->id, rate);
	}
	else {
		arm_smccc_smc(SIP_CLK_SET_CLKCTRL, tcc->id, 1, rate, 0, 0, 0, 0, &res);
	}

	return 0;
}

static int tcc_clkctrl_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_is_clkctrl_enabled)
			return ckc_ops->ckc_is_clkctrl_enabled(tcc->id) ? 1 : 0;
	}
	else {
		arm_smccc_smc(SIP_CLK_IS_CLKCTRL, tcc->id, 0, 0, 0, 0, 0, 0, &res);
		return res.a0;
	}

	return 0;
}

static struct clk_ops tcc_clkctrl_ops = {
	.enable		= tcc_clkctrl_enable,
#ifndef IGNORE_CLK_DISABLE
	.disable	= tcc_clkctrl_disable,
#endif
	.is_enabled	= tcc_clkctrl_is_enabled,
	.recalc_rate	= tcc_clkctrl_recalc_rate,
	.round_rate	= tcc_round_rate,
	.set_rate	= tcc_clkctrl_set_rate,
	.debug_init	= tcc_clk_debug_init,
};

static void __init tcc_fbus_init(struct device_node *np)
{
	tcc_clk_register(np, &tcc_clkctrl_ops);
}
CLK_OF_DECLARE(tcc_clk_fbus, "telechips,clk-fbus", tcc_fbus_init);


static int tcc_peri_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_peri_enable)
			ckc_ops->ckc_peri_enable(tcc->id);
	}
	else {
		arm_smccc_smc(SIP_CLK_ENABLE_PERI, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}
	return 0;
}

static void tcc_peri_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_peri_disable)
			ckc_ops->ckc_peri_disable(tcc->id);
	}
	else {
		arm_smccc_smc(SIP_CLK_DISABLE_PERI, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}
}

static unsigned long tcc_peri_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct arm_smccc_res res;
	unsigned long rate = 0;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_peri_get_rate)
			rate = ckc_ops->ckc_peri_get_rate(tcc->id);
	}
	else {
		arm_smccc_smc(SIP_CLK_GET_PCLKCTRL, tcc->id, 0, 0, 0, 0, 0, 0, &res);
		return res.a0;
	}
	return rate;
}

static int tcc_peri_set_rate(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);
	unsigned long flags = __clk_get_flags(hw->clk);
	unsigned long vflags = 0;

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_peri_set_rate)
			ckc_ops->ckc_peri_set_rate(tcc->id, rate);
	} else {
		/* We care only about vendor-specific flags */
		if (flags & CLK_F_FIXED)
			vflags |= (flags & (CLK_F_SRC_CLK_MASK << CLK_F_SRC_CLK_SHIFT))
					>> CLK_F_SRC_CLK_SHIFT;
		if (flags & CLK_F_DCO_MODE)
			vflags |= CLK_F_DCO_MODE;
		if (flags & CLK_F_SKIP_SSCG)
			vflags |= CLK_F_SKIP_SSCG;
		if (flags & CLK_F_DIV_MODE)
			vflags |= CLK_F_DIV_MODE;
		arm_smccc_smc(SIP_CLK_SET_PCLKCTRL, tcc->id, 1, rate, vflags,
				0, 0, 0, &res);
	}
	return 0;
}

static int tcc_peri_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_is_peri_enabled)
			return ckc_ops->ckc_is_peri_enabled(tcc->id) ? 1 : 0;
	}
	else {
		arm_smccc_smc(SIP_CLK_IS_PERI, tcc->id, 0, 0, 0, 0, 0, 0, &res);
		return res.a0;
	}
	return 0;
}

static struct clk_ops tcc_peri_ops = {
	.enable		= tcc_peri_enable,
	.disable	= tcc_peri_disable,
	.recalc_rate	= tcc_peri_recalc_rate,
	.round_rate	= tcc_round_rate,
	.set_rate	= tcc_peri_set_rate,
	.is_enabled	= tcc_peri_is_enabled,
	.debug_init	= tcc_clk_debug_init,
};

static void __init tcc_peri_init(struct device_node *np)
{
	tcc_clk_register(np, &tcc_peri_ops);
}
CLK_OF_DECLARE(tcc_clk_peri, "telechips,clk-peri", tcc_peri_init);

static int tcc_isoip_top_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_isoip_top_pwdn)
			ckc_ops->ckc_isoip_top_pwdn(tcc->id, false);
	}
	else {
		arm_smccc_smc(SIP_CLK_ENABLE_ISOTOP, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}

	return 0;
}

#ifndef IGNORE_CLK_DISABLE
static void tcc_isoip_top_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_isoip_top_pwdn)
			ckc_ops->ckc_isoip_top_pwdn(tcc->id, true);
	}
	else {
		arm_smccc_smc(SIP_CLK_DISABLE_ISOTOP, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}
}
#endif

static int tcc_isoip_top_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_is_isoip_top_pwdn)
			return ckc_ops->ckc_is_isoip_top_pwdn(tcc->id) ? 0 : 1;
	}
	else {
		arm_smccc_smc(SIP_CLK_IS_ISOTOP, tcc->id, 0, 0, 0, 0, 0, 0, &res);
		return (res.a0 == 1) ? 0 : 1;
	}
	return 0;
}

static struct clk_ops tcc_isoip_top_ops = {
	.enable		= tcc_isoip_top_enable,
#ifndef IGNORE_CLK_DISABLE
	.disable	= tcc_isoip_top_disable,
#endif
	.is_enabled	= tcc_isoip_top_is_enabled,
	.debug_init	= tcc_clk_debug_init,
};

static void __init tcc_isoip_top_init(struct device_node *np)
{
	tcc_clk_register(np, &tcc_isoip_top_ops);
}
CLK_OF_DECLARE(tcc_clk_isoip_top, "telechips,clk-isoip_top", tcc_isoip_top_init);

static int tcc_isoip_ddi_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_isoip_ddi_pwdn)
			ckc_ops->ckc_isoip_ddi_pwdn(tcc->id, false);
	}
	else {
		arm_smccc_smc(SIP_CLK_ENABLE_ISODDI, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}

	return 0;
}

#ifndef IGNORE_CLK_DISABLE
static void tcc_isoip_ddi_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_isoip_ddi_pwdn)
			ckc_ops->ckc_isoip_ddi_pwdn(tcc->id, true);
	}
	else {
		arm_smccc_smc(SIP_CLK_DISABLE_ISODDI, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}
}
#endif

static int tcc_isoip_ddi_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_is_isoip_ddi_pwdn)
			return ckc_ops->ckc_is_isoip_ddi_pwdn(tcc->id) ? 0 : 1;
	}
	else {
		arm_smccc_smc(SIP_CLK_IS_ISODDI, tcc->id, 0, 0, 0, 0, 0, 0, &res);
		return (res.a0 == 1) ? 0 : 1;
	}

	return 0;
}

static struct clk_ops tcc_isoip_ddi_ops = {
	.enable		= tcc_isoip_ddi_enable,
#ifndef IGNORE_CLK_DISABLE
	.disable	= tcc_isoip_ddi_disable,
#endif
	.is_enabled	= tcc_isoip_ddi_is_enabled,
	.debug_init	= tcc_clk_debug_init,
};

static void __init tcc_isoip_ddi_init(struct device_node *np)
{
	tcc_clk_register(np, &tcc_isoip_ddi_ops);
}
CLK_OF_DECLARE(tcc_clk_isoip_ddi, "telechips,clk-isoip_ddi", tcc_isoip_ddi_init);

static int tcc_ddibus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_ddibus_pwdn)
			ckc_ops->ckc_ddibus_pwdn(tcc->id, false);
		if (ckc_ops->ckc_ddibus_swreset)
			ckc_ops->ckc_ddibus_swreset(tcc->id, false);
	}
	else {
		arm_smccc_smc(SIP_CLK_ENABLE_DDIBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}
	return 0;
}

static void tcc_ddibus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_ddibus_swreset)
			ckc_ops->ckc_ddibus_swreset(tcc->id, true);
		if (ckc_ops->ckc_ddibus_pwdn)
			ckc_ops->ckc_ddibus_pwdn(tcc->id, true);
	}
	else {
		arm_smccc_smc(SIP_CLK_DISABLE_DDIBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}
}

static int tcc_ddibus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_is_ddibus_pwdn)
			return ckc_ops->ckc_is_ddibus_pwdn(tcc->id) ? 0 : 1;
	}
	else {
		arm_smccc_smc(SIP_CLK_IS_DDIBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
		return (res.a0 == 1) ? 0 : 1;
	}

	return 0;
}

static struct clk_ops tcc_ddibus_ops = {
	.enable		= tcc_ddibus_enable,
	.disable	= tcc_ddibus_disable,
	.is_enabled	= tcc_ddibus_is_enabled,
	.debug_init	= tcc_clk_debug_init,
};

static void __init tcc_ddibus_init(struct device_node *np)
{
	tcc_clk_register(np, &tcc_ddibus_ops);
}
CLK_OF_DECLARE(tcc_clk_ddibus, "telechips,clk-ddibus", tcc_ddibus_init);

static int tcc_iobus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_iobus_pwdn)
			ckc_ops->ckc_iobus_pwdn(tcc->id, false);
		if (ckc_ops->ckc_iobus_swreset)
			ckc_ops->ckc_iobus_swreset(tcc->id, false);
	}
	else {
		arm_smccc_smc(SIP_CLK_ENABLE_IOBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}

	return 0;
}

static void tcc_iobus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_iobus_swreset)
			ckc_ops->ckc_iobus_swreset(tcc->id, true);
		if (ckc_ops->ckc_iobus_pwdn)
			ckc_ops->ckc_iobus_pwdn(tcc->id, true);
	}
	else {
		arm_smccc_smc(SIP_CLK_DISABLE_IOBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}
}

static int tcc_iobus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_is_iobus_pwdn)
			return ckc_ops->ckc_is_iobus_pwdn(tcc->id) ? 0 : 1;
	}
	else {
		arm_smccc_smc(SIP_CLK_IS_IOBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
		return (res.a0 == 1) ? 0 : 1;
	}

	return 0;
}

static struct clk_ops tcc_iobus_ops = {
	.enable		= tcc_iobus_enable,
	.disable	= tcc_iobus_disable,
	.is_enabled	= tcc_iobus_is_enabled,
	.debug_init	= tcc_clk_debug_init,
};

static void __init tcc_iobus_init(struct device_node *np)
{
	tcc_clk_register(np, &tcc_iobus_ops);
}
CLK_OF_DECLARE(tcc_clk_iobus, "telechips,clk-iobus", tcc_iobus_init);


static int tcc_vpubus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_vpubus_pwdn)
			ckc_ops->ckc_vpubus_pwdn(tcc->id, false);
		if (ckc_ops->ckc_vpubus_swreset)
			ckc_ops->ckc_vpubus_swreset(tcc->id, false);
	}
	else {
		arm_smccc_smc(SIP_CLK_ENABLE_VPUBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}

	return 0;
}

static void tcc_vpubus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_vpubus_swreset)
			ckc_ops->ckc_vpubus_swreset(tcc->id, true);
		if (ckc_ops->ckc_vpubus_pwdn)
			ckc_ops->ckc_vpubus_pwdn(tcc->id, true);
	} else {
		arm_smccc_smc(SIP_CLK_DISABLE_VPUBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}
}

static int tcc_vpubus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_is_vpubus_pwdn)
			return ckc_ops->ckc_is_vpubus_pwdn(tcc->id) ? 0 : 1;
	}
	else {
		arm_smccc_smc(SIP_CLK_IS_VPUBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
		return (res.a0 == 1) ? 0 : 1;
	}

	return 0;
}

static struct clk_ops tcc_vpubus_ops = {
	.enable		= tcc_vpubus_enable,
	.disable	= tcc_vpubus_disable,
	.is_enabled	= tcc_vpubus_is_enabled,
	.debug_init	= tcc_clk_debug_init,
};

static void __init tcc_vpubus_init(struct device_node *np)
{
	tcc_clk_register(np, &tcc_vpubus_ops);
}
CLK_OF_DECLARE(tcc_clk_vpubus, "telechips,clk-vpubus", tcc_vpubus_init);


static int tcc_hsiobus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_hsiobus_pwdn)
			ckc_ops->ckc_hsiobus_pwdn(tcc->id, false);
		if (ckc_ops->ckc_hsiobus_swreset)
			ckc_ops->ckc_hsiobus_swreset(tcc->id, false);
	}
	else {
		arm_smccc_smc(SIP_CLK_ENABLE_HSIOBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}

	return 0;
}

static void tcc_hsiobus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_hsiobus_swreset)
			ckc_ops->ckc_hsiobus_swreset(tcc->id, true);
		if (ckc_ops->ckc_hsiobus_pwdn)
			ckc_ops->ckc_hsiobus_pwdn(tcc->id, true);
	}
	else {
		arm_smccc_smc(SIP_CLK_DISABLE_HSIOBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	}
}

static int tcc_hsiobus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_is_hsiobus_pwdn)
			return ckc_ops->ckc_is_hsiobus_pwdn(tcc->id) ? 0 : 1;
	}
	else {
		arm_smccc_smc(SIP_CLK_IS_HSIOBUS, tcc->id, 0, 0, 0, 0, 0, 0, &res);
		return (res.a0 == 1) ? 0 : 1;
	}

	return 0;
}

static struct clk_ops tcc_hsiobus_ops = {
	.enable		= tcc_hsiobus_enable,
	.disable	= tcc_hsiobus_disable,
	.is_enabled	= tcc_hsiobus_is_enabled,
	.debug_init	= tcc_clk_debug_init,
};

static void __init tcc_hsiobus_init(struct device_node *np)
{
	tcc_clk_register(np, &tcc_hsiobus_ops);
}
CLK_OF_DECLARE(tcc_clk_hsiobus, "telechips,clk-hsiobus", tcc_hsiobus_init);

static int tcc_pll_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_is_pll_enabled)
			return ckc_ops->ckc_is_pll_enabled(tcc->id) ? 0 : 1;
	} else {
		arm_smccc_smc(SIP_CLK_IS_PLL_ENABLED, tcc->id,
				0, 0, 0, 0, 0, 0, &res);
		return res.a0;
	}
	return 0;
}

static unsigned long tcc_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct arm_smccc_res res;
	unsigned long rate = 0;
	struct tcc_clk *tcc = to_tcc_clk(hw);

	if (ckc_ops != NULL) {
		if (ckc_ops->ckc_clkctrl_get_rate)
			rate = ckc_ops->ckc_pll_get_rate(tcc->id);
	} else {
		arm_smccc_smc(SIP_CLK_GET_PLL, tcc->id, 0, 0, 0, 0, 0, 0,
			      &res);
		rate = res.a0;
	}
	return rate;
}

static struct clk_ops tcc_pll_ops = {
	.is_enabled	= tcc_pll_is_enabled,
	.recalc_rate	= tcc_pll_recalc_rate,
	.debug_init	= tcc_clk_debug_init,
};

static void __init tcc_pll_init(struct device_node *np)
{
	tcc_clk_register(np, &tcc_pll_ops);
}
CLK_OF_DECLARE(tcc_clk_pll, "telechips,clk-pll", tcc_pll_init);
