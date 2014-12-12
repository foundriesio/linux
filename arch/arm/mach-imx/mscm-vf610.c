/*
 * Copyright 2014 Stefan Agner
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/irqchip/arm-gic.h>
#include "common.h"

#define MSCM_CPxNUM		0x4
#define MSCM_IRSPRC(n)		(0x880 + 2 * (n))
#define MSCM_IRSPRC_CPEN_MASK	0x3

#define MSCM_IRSPRC_NUM		112

#define MSCM_IRQ_OFFSET		32

static void __iomem *mscm_base;
static u16 mscm_saved_irsprc[MSCM_IRSPRC_NUM];
static u16 cpu_id;

static int vf610_mscm_notifier(struct notifier_block *self, unsigned long cmd,
			       void *v)
{
	int i;

	/* Only the primary (boot CPU) should do suspend/resume */
	if (cpu_id > 0)
		return NOTIFY_OK;

	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		for (i = 0; i < MSCM_IRSPRC_NUM; i++)
			mscm_saved_irsprc[i] =
				readw_relaxed(mscm_base + MSCM_IRSPRC(i));
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		for (i = 0; i < MSCM_IRSPRC_NUM; i++)
			writew_relaxed(mscm_saved_irsprc[i],
				       mscm_base + MSCM_IRSPRC(i));
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mscm_notifier_block = {
	.notifier_call = vf610_mscm_notifier,
};

static int vf610_mscm_domain_map(struct irq_domain *d, unsigned int irq,
			       irq_hw_number_t hw)
{
	u16 irsprc;

	/* Do not handle non Interrupt Router IRQs */
	if (hw < MSCM_IRQ_OFFSET)
		return 0;

	hw -= MSCM_IRQ_OFFSET;
	irsprc = readw_relaxed(mscm_base + MSCM_IRSPRC(hw));
	irsprc &= MSCM_IRSPRC_CPEN_MASK;

	/* Warn if interrupt is enabled on another CPU */
	WARN_ON(irsprc & ~(0x1 << cpu_id));

	writew_relaxed(0x1 << cpu_id, mscm_base + MSCM_IRSPRC(hw));

	return 0;
}

static void vf610_mscm_domain_unmap(struct irq_domain *d, unsigned int irq)
{
	irq_hw_number_t hw = irq_get_irq_data(irq)->hwirq;
	u16 irsprc;

	/* Do not handle non Interrupt Router IRQs */
	if (hw < MSCM_IRQ_OFFSET)
		return;

	hw -= MSCM_IRQ_OFFSET;
	irsprc = readw_relaxed(mscm_base + MSCM_IRSPRC(hw));
	irsprc &= MSCM_IRSPRC_CPEN_MASK;

	writew_relaxed(0x1 << cpu_id, mscm_base + MSCM_IRSPRC(hw));
}

static int vf610_mscm_domain_xlate(struct irq_domain *d,
				struct device_node *controller,
				const u32 *intspec, unsigned int intsize,
				unsigned long *out_hwirq,
				unsigned int *out_type)
{
	*out_hwirq += 16;
	return 0;
}
static const struct irq_domain_ops routable_irq_domain_ops = {
	.map = vf610_mscm_domain_map,
	.unmap = vf610_mscm_domain_unmap,
	.xlate = vf610_mscm_domain_xlate,
};

void __init vf610_mscm_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "fsl,vf610-mscm");
	mscm_base = of_iomap(np, 0);

	if (!mscm_base) {
		WARN_ON(1);
		return;
	}

	cpu_id = readl_relaxed(mscm_base + MSCM_CPxNUM);

	/* Register MSCM as interrupt router */
	register_routable_domain_ops(&routable_irq_domain_ops);

	cpu_pm_register_notifier(&mscm_notifier_block);
}
