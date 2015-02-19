/*
 * Hisilicon hi6220 stub clock driver
 *
 * Copyright (c) 2015 Hisilicon Limited.
 * Copyright (c) 2015 Linaro Limited.
 *
 * Author: Leo Yan <leo.yan@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <asm/compiler.h>
#include <linux/clk-provider.h>
#include <linux/clk-private.h>
#include <linux/err.h>
#include <linux/hisi_acpu_cpufreq.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/slab.h>

/* stub clocks */
#define STUB_ACPU0			0
#define STUB_ACPU1			1
#define STUB_GPU			2
#define STUB_DDR_MIN			3
#define STUB_DDR_MAX			4
#define STUB_DDR			5

#define ACPU_DFS_FLAG			0x0
#define ACPU_DFS_FREQ_REQ		0x4
#define ACPU_DFS_TEMP_REQ		0x8

#define ACPU_DFS_LOCK_FLAG		(0xAEAEAEAE)
#define ACPU_DFS_UNLOCK_FLAG		(0x0)

/* Multi-core communication */
#define MC_CORE_ACPU			0x2
#define MC_COM_CPU_RAW_INT_OFFSET(i)	(0x400 + (i << 4))
#define MC_COM_INT_ACPU_DFS		15

#define to_stub_clk(hw) container_of(hw, struct hisi_stub_clk, hw)

struct hisi_stub_clk {
	struct clk_hw	hw;
	void __iomem	*reg;	/* ctrl register */
	void __iomem	*comm;	/* comm register */

	/*
	 * v8r1:
	 *   0: A7;  1: A15;  2: gpu;  3: ddr;
	 *
	 * v8r2:
	 *   0: A53; 1: A53;  2: gpu;  3: ddr;
	 */
	u32		id;
	u32		rate;
	u32		call_fw;

	spinlock_t	*lock;
};

static int initialized_stub_clk = 0;

static void __iomem *mc_base = NULL;
static void __iomem *dfs_base = NULL;
static DEFINE_SPINLOCK(dfs_lock);

static unsigned int set_freq_func_id, get_freq_func_id;

static noinline int __invoke_smc(u64 function_id, u64 arg0, u64 arg1, u64 arg2)
{
	__asm__ volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			__asmeq("%2", "x2")
			__asmeq("%3", "x3")
			"smc	#0\n"
		: "+r" (function_id)
		: "r" (arg0), "r" (arg1), "r" (arg2));

	return function_id;
}

unsigned int hisi_acpu_get_freq(void)
{
	unsigned long flags;
	unsigned int freq, max_freq = UINT_MAX;

	spin_lock_irqsave(&dfs_lock, flags);

	if (readl(dfs_base + ACPU_DFS_FLAG) == ACPU_DFS_LOCK_FLAG)
		max_freq = readl(dfs_base + ACPU_DFS_TEMP_REQ);
	freq = readl(dfs_base + ACPU_DFS_FREQ_REQ);
	freq = min(freq, max_freq);

	pr_debug("%s: flag %x max_freq %d freq %d\n", __func__,
		readl(dfs_base + ACPU_DFS_FLAG), max_freq, freq);

	spin_unlock_irqrestore(&dfs_lock, flags);

	return freq;
}
EXPORT_SYMBOL_GPL(hisi_acpu_get_freq);

int hisi_acpu_set_freq(unsigned int freq)
{
	unsigned long flags;
	unsigned int max_freq = UINT_MAX;

	spin_lock_irqsave(&dfs_lock, flags);

	/* check if beyond max frequency */
	if (readl(dfs_base + ACPU_DFS_FLAG) == ACPU_DFS_LOCK_FLAG)
		max_freq = readl(dfs_base + ACPU_DFS_TEMP_REQ);

	if (freq > max_freq) {
		spin_unlock_irqrestore(&dfs_lock, flags);
		return -EINVAL;
	}

	writel(freq, dfs_base + ACPU_DFS_FREQ_REQ);
	writel((1 << MC_COM_INT_ACPU_DFS),
		mc_base + MC_COM_CPU_RAW_INT_OFFSET(MC_CORE_ACPU));

	spin_unlock_irqrestore(&dfs_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(hisi_acpu_set_freq);

int hisi_acpu_set_max_freq(unsigned int max_freq, unsigned int flag)
{
	unsigned long flags;

	spin_lock_irqsave(&dfs_lock, flags);

	writel(max_freq, dfs_base + ACPU_DFS_TEMP_REQ);

	if (flag == ACPU_LOCK_MAX_FREQ)
		writel(ACPU_DFS_LOCK_FLAG, dfs_base + ACPU_DFS_FLAG);
	else if (flag == ACPU_UNLOCK_MAX_FREQ)
		writel(ACPU_DFS_UNLOCK_FLAG, dfs_base + ACPU_DFS_FLAG);
	else
		return -EINVAL;

	writel((1 << MC_COM_INT_ACPU_DFS),
		mc_base + MC_COM_CPU_RAW_INT_OFFSET(MC_CORE_ACPU));

	pr_debug("%s: flag %x max_freq %d\n", __func__,
		readl(dfs_base + ACPU_DFS_FLAG), max_freq);

	spin_unlock_irqrestore(&dfs_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(hisi_acpu_set_max_freq);

static unsigned long hisi_stub_clk_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	u32 rate = 0;
	struct hisi_stub_clk *stub_clk = to_stub_clk(hw);
	unsigned long flags;

	BUG_ON(!stub_clk->lock);

	spin_lock_irqsave(stub_clk->lock, flags);

	switch (stub_clk->id) {

	case STUB_ACPU0:

		if (stub_clk->call_fw)
			rate = __invoke_smc(get_freq_func_id, stub_clk->id,
					    0, 0);
		else
			rate = hisi_acpu_get_freq();

		/* change from KHz to Hz */
		rate *= 1000;
		break;

	case STUB_ACPU1:
	case STUB_GPU:
	case STUB_DDR_MIN:
	case STUB_DDR_MAX:
	case STUB_DDR:
	default:
		printk(KERN_ERR "un-supported clock id\n");
		break;
	}

	spin_unlock_irqrestore(stub_clk->lock, flags);
	return rate;
}

static int hisi_stub_clk_set_rate(struct clk_hw *hw, unsigned long rate,
                    unsigned long parent_rate)
{
	struct hisi_stub_clk *stub_clk = to_stub_clk(hw);
	unsigned long flags;
	unsigned long new_rate = rate / 1000;  /* Khz */
	int ret = 0;

	BUG_ON(!stub_clk->lock);

	pr_debug("%s: %s set rate=%ldKhz\n", __func__,
			hw->clk->name, new_rate);

	spin_lock_irqsave(stub_clk->lock, flags);

	switch (stub_clk->id) {

	case STUB_ACPU0:

		if (stub_clk->call_fw)
			ret = __invoke_smc(set_freq_func_id, stub_clk->id,
					   new_rate, 0);
		else
			ret = hisi_acpu_set_freq(new_rate);

		break;

	case STUB_ACPU1:
	case STUB_GPU:
	case STUB_DDR_MIN:
	case STUB_DDR_MAX:
	case STUB_DDR:
	default:
		printk(KERN_ERR "un-supported clock id\n");
		break;
	}

	stub_clk->rate = new_rate;

	spin_unlock_irqrestore(stub_clk->lock, flags);
	return ret;
}

static long hisi_stub_clk_round_rate(struct clk_hw *hw,
		unsigned long rate, unsigned long *prate)
{
	return rate;
}

static long hisi_stub_clk_determine_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *best_parent_rate, struct clk **best_parent_clk)
{
	return rate;
}

static struct clk_ops hisi_stub_clk_ops = {
	.recalc_rate    = hisi_stub_clk_recalc_rate,
	.determine_rate = hisi_stub_clk_determine_rate,
	.round_rate     = hisi_stub_clk_round_rate,
	.set_rate       = hisi_stub_clk_set_rate,
};


static int hisi_stub_clk_init(void __iomem *base, void __iomem *comm_base)
{
	int ret = 0;

	dfs_base = base;
	mc_base  = comm_base;

	if (!dfs_base) {
		pr_err("%s: failed to remap dfs region\n", __func__);
		return -EINVAL;
	}

	if (!mc_base) {
		pr_err("%s: failed to remap mc region\n", __func__);
		return -EINVAL;
	}

	/* reset to zero */
	writel(0x0, dfs_base + ACPU_DFS_FLAG);
	writel(0x0, dfs_base + ACPU_DFS_FREQ_REQ);
	writel(0x0, dfs_base + ACPU_DFS_TEMP_REQ);

	/*
	 * FIXME: At boot time, there have no method to read back
	 * current frequency value, so the register ACPU_DFS_FREQ_REQ
	 * is zero; so here just trigger to a fixed frequency firstly.
	 */
	hisi_acpu_set_freq(729000);
	return ret;
}

static struct clk *_register_stub_clk(struct device *dev, unsigned int id,
	const char *name, const char *parent_name, unsigned long flags,
	spinlock_t *lock, unsigned int call_fw)
{
	struct hisi_stub_clk *stub_clk;
	struct clk *clk;
	struct clk_init_data init;

	stub_clk = kzalloc(sizeof(*stub_clk), GFP_KERNEL);
	if (!stub_clk) {
		pr_err("%s: fail to alloc stub clk!\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.ops = &hisi_stub_clk_ops;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.flags = flags;

	stub_clk->hw.init = &init;
	stub_clk->id = id;
	stub_clk->call_fw = call_fw;
	stub_clk->lock = lock;

	clk = clk_register(dev, &stub_clk->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: fail to register stub clk %s!\n", __func__, name);
		kfree(stub_clk);
	}

	return clk;
}

struct clk *hisi_register_stub_clk(struct device *dev, unsigned int id,
	const char *name, const char *parent_name, unsigned long flags,
	void __iomem *reg, void __iomem *comm_reg, spinlock_t *lock)
{
	struct clk *clk;

	pr_debug("[%s]: clk name = %s...\n", __func__, name);

	if (!initialized_stub_clk) {
		hisi_stub_clk_init(reg, comm_reg);
		initialized_stub_clk = 1;
	}

	clk = _register_stub_clk(dev, id, name, parent_name, flags, lock, 0);
	return clk;
}

struct clk *hisi_register_stub_clk_fw(struct device *dev, unsigned int id,
	const char *name, const char *parent_name, unsigned long flags,
	unsigned int set_freq_id, unsigned int get_freq_id, spinlock_t *lock)
{
	struct clk *clk;

	pr_debug("[%s]: clk name = %s...\n", __func__, name);

	if (!initialized_stub_clk) {
		/* init function id for smc call */
		set_freq_func_id = set_freq_id;
		get_freq_func_id = get_freq_id;

		initialized_stub_clk = 1;
	}

	clk = _register_stub_clk(dev, id, name, parent_name, flags, lock, 1);
	return clk;
}
