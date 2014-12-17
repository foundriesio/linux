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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqchip/arm-gic.h>
#include "common.h"

#define MSCM_CPxNUM		0x4

#define MSCM_IRCP0IR		0x800
#define MSCM_IRCP1IR		0x804
#define MSCM_IRCPnIR(n)		((n) * 0x4 + MSCM_IRCP0IR)
#define MSCM_IRCPnIR_INT(n)	(0x1 << (n))
#define MSCM_IRCPGIR		0x820

#define MSCM_INTID_MASK		0x3
#define MSCM_INTID(n)		((n) & MSCM_INTID_MASK)
#define MSCM_CPUTL(n)		(((n) == 0 ? 1 : 2) << 16)

#define MSCM_IRSPRC(n)		(0x880 + 2 * (n))
#define MSCM_IRSPRC_CPEN_MASK	0x3

#define MSCM_IRSPRC_NUM		112

#define MSCM_CPU2CPU_NUM	4

#define MSCM_IRQ_OFFSET		32

static void __iomem *mscm_base;
struct device_node *mscm_node;
static u16 mscm_saved_irsprc[MSCM_IRSPRC_NUM];
static u16 cpu_id;

struct mscm_cpu2cpu_irq_data {
	int intid;
	int irq;
	irq_handler_t handler;
	void *priv;
};

static struct mscm_cpu2cpu_irq_data cpu2cpu_irq_data[MSCM_CPU2CPU_NUM];

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

static irqreturn_t mscm_cpu2cpu_irq_handler(int irq, void *dev_id)
{
	irqreturn_t ret;
	struct mscm_cpu2cpu_irq_data *data = dev_id;


	ret = data->handler(data->intid, data->priv);
	if (ret == IRQ_HANDLED)
		writel(MSCM_IRCPnIR_INT(data->intid), mscm_base + MSCM_IRCPnIR(cpu_id));

	return ret;
}

int mscm_request_cpu2cpu_irq(unsigned int intid, irq_handler_t handler,
			     const char *name, void *priv)
{
	int irq;
	struct mscm_cpu2cpu_irq_data *data;


	if (intid >= MSCM_CPU2CPU_NUM)
		return -EINVAL;

	irq = of_irq_get(mscm_node, intid);
	if (irq < 0)
		return irq;

	data = &cpu2cpu_irq_data[intid];
	data->intid = intid;
	data->irq = irq;
	data->handler = handler;
	data->priv = priv;

	return request_irq(irq, mscm_cpu2cpu_irq_handler, 0, name, data);
}
EXPORT_SYMBOL(mscm_request_cpu2cpu_irq);

void mscm_free_cpu2cpu_irq(unsigned int intid, void *priv)
{
	struct mscm_cpu2cpu_irq_data *data;

	if (intid >= MSCM_CPU2CPU_NUM)
		return;

	data = &cpu2cpu_irq_data[intid];

	if (data->irq < 0)
		return;

	free_irq(data->irq, data);
}
EXPORT_SYMBOL(mscm_free_cpu2cpu_irq);

void mscm_trigger_cpu2cpu_irq(unsigned int intid, int cpuid)
{
	writel(MSCM_INTID(intid) | MSCM_CPUTL(cpuid), mscm_base + MSCM_IRCPGIR);
}
EXPORT_SYMBOL(mscm_trigger_cpu2cpu_irq);

void __init vf610_mscm_init(void)
{
	mscm_node = of_find_compatible_node(NULL, NULL, "fsl,vf610-mscm");
	mscm_base = of_iomap(mscm_node, 0);

	if (!mscm_base) {
		WARN_ON(1);
		return;
	}

	cpu_id = readl_relaxed(mscm_base + MSCM_CPxNUM);

	/* Register MSCM as interrupt router */
	register_routable_domain_ops(&routable_irq_domain_ops);

	cpu_pm_register_notifier(&mscm_notifier_block);
}
