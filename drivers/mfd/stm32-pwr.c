// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) STMicroelectronics 2017 - All Rights Reserved
 * Author: Pascal Paillet <p.paillet@st.com> for STMicroelectronics.
 */

#include <linux/arm-smccc.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <asm/exception.h>

#define NB_WAKEUPPINS 6

#define STM32_SVC_PWR 0x82001001
#define STM32_WRITE 0x1
#define STM32_SET_BITS 0x2
#define STM32_CLEAR_BITS 0x3

#define SMC_PWR_BASE 0x50001000
// PWR Registers
#define WKUPCR 0x20
#define WKUPFR 0x24
#define MPUWKUPENR 0x28

#define WKUP_FLAGS_MASK GENMASK(5, 0)

// WKUPCR bits definition
#define WKUP_EDGE_SHIFT 8
#define WKUP_PULL_SHIFT 16
#define WKUP_PULL_MASK GENMASK(1, 0)

enum wkup_pull_setting {
	WKUP_NO_PULL = 0,
	WKUP_PULL_UP,
	WKUP_PULL_DOWN,
	WKUP_PULL_RESERVED
};

#define SMC(class, op, offset, val)				\
{								\
	struct arm_smccc_res res;				\
	arm_smccc_smc(class, op, SMC_PWR_BASE + offset, val,	\
		      0, 0, 0, 0, &res);			\
}								\

struct stm32_pwr_data {
	struct device *dev;		/* self device */
	void __iomem *base;		/* IO Memory base address */
	struct irq_domain *domain;	/* Domain for this controller */
	int irq;			/* Parent interrupt */
};

static void stm32_pwr_irq_ack(struct irq_data *d)
{
	struct stm32_pwr_data *priv = d->domain->host_data;

	dev_dbg(priv->dev, "irq:%lu\n", d->hwirq);
	SMC(STM32_SVC_PWR, STM32_SET_BITS, WKUPCR, BIT(d->hwirq));
}

static void stm32_pwr_irq_mask(struct irq_data *d)
{
	struct stm32_pwr_data *priv = d->domain->host_data;

	dev_dbg(priv->dev, "irq:%lu\n", d->hwirq);
	SMC(STM32_SVC_PWR, STM32_CLEAR_BITS, MPUWKUPENR, BIT(d->hwirq));
}

static void stm32_pwr_irq_unmask(struct irq_data *d)
{
	struct stm32_pwr_data *priv = d->domain->host_data;

	dev_dbg(priv->dev, "irq:%lu\n", d->hwirq);
	SMC(STM32_SVC_PWR, STM32_SET_BITS, MPUWKUPENR, BIT(d->hwirq));
}

static int stm32_pwr_irq_set_wake(struct irq_data *d, unsigned int on)
{
	struct stm32_pwr_data *priv = d->domain->host_data;

	dev_dbg(priv->dev, "irq:%lu on:%d\n", d->hwirq, on);
	if (on)
		enable_irq_wake(priv->irq);
	else
		disable_irq_wake(priv->irq);

	return 0;
}

static int stm32_pwr_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	struct stm32_pwr_data *priv = d->domain->host_data;
	int pin_id = d->hwirq;
	u32 wkupcr;
	int en;

	dev_dbg(priv->dev, "irq:%lu\n", d->hwirq);

	en = readl_relaxed(priv->base + MPUWKUPENR) & BIT(pin_id);
	/* reference manual request to disable the wakeup pin while
	 * changing the edge detection setting
	 */
	if (en)
		stm32_pwr_irq_mask(d);

	wkupcr = readl_relaxed(priv->base + WKUPCR);
	switch (flow_type & IRQ_TYPE_SENSE_MASK) {
	case IRQF_TRIGGER_FALLING:
		wkupcr |= (1 << (WKUP_EDGE_SHIFT + pin_id));
		break;
	case IRQF_TRIGGER_RISING:
		wkupcr &= ~(1 << (WKUP_EDGE_SHIFT + pin_id));
		break;
	default:
		return -EINVAL;
	}
	SMC(STM32_SVC_PWR, STM32_WRITE, WKUPCR, wkupcr);

	if (en)
		stm32_pwr_irq_unmask(d);

	return 0;
}

static struct irq_chip stm32_pwr_irq_chip = {
	.name = "stm32_pwr-irq",
	.irq_ack = stm32_pwr_irq_ack,
	.irq_mask = stm32_pwr_irq_mask,
	.irq_unmask = stm32_pwr_irq_unmask,
	.irq_set_type = stm32_pwr_irq_set_type,
	.irq_set_wake = stm32_pwr_irq_set_wake,
};

static int stm32_pwr_irq_map(struct irq_domain *h, unsigned int virq,
			     irq_hw_number_t hw)
{
	irq_set_chip_and_handler(virq, &stm32_pwr_irq_chip, handle_edge_irq);
	return 0;
}

static int stm32_pwr_irq_set_pull_config(struct irq_domain *d, int pin_id,
					 enum wkup_pull_setting config)
{
	struct stm32_pwr_data *priv = d->host_data;
	u32 wkupcr;

	dev_dbg(priv->dev, "irq:%d pull config:0x%x\n", pin_id, config);

	if (config >= WKUP_PULL_RESERVED) {
		dev_err(priv->dev, "%s: bad irq pull config\n", __func__);
		return -EINVAL;
	}

	wkupcr = readl_relaxed(priv->base + WKUPCR);
	wkupcr &= ~((WKUP_PULL_MASK) << (WKUP_PULL_SHIFT + pin_id * 2));
	wkupcr |= (config & WKUP_PULL_MASK) << (WKUP_PULL_SHIFT + pin_id * 2);
	SMC(STM32_SVC_PWR, STM32_WRITE, WKUPCR, wkupcr);

	return 0;
}

static int stm32_pwr_xlate(struct irq_domain *d, struct device_node *ctrlr,
			   const u32 *intspec, unsigned int intsize,
			   irq_hw_number_t *out_hwirq, unsigned int *out_type)
{
	struct stm32_pwr_data *priv = d->host_data;

	if (WARN_ON(intsize < 3)) {
		dev_err(priv->dev, "%s: bad irq config parameters\n", __func__);
		return -EINVAL;
	}

	*out_hwirq = intspec[0];
	*out_type = intspec[1] & (IRQ_TYPE_SENSE_MASK);

	return stm32_pwr_irq_set_pull_config(d, intspec[0], intspec[2]);
}

static const struct irq_domain_ops stm32_pwr_irq_domain_ops = {
	.map = stm32_pwr_irq_map,
	.xlate = stm32_pwr_xlate,
};

/*
 * Handler for the cascaded IRQ.
 */
static void stm32_pwr_handle_irq(struct irq_desc *desc)
{
	struct stm32_pwr_data  *priv = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	u32 wkupfr, wkupenr, i;

	chained_irq_enter(chip, desc);

	wkupfr = readl_relaxed(priv->base + WKUPFR);
	wkupenr = readl_relaxed(priv->base + MPUWKUPENR);
	for (i = 0; i < NB_WAKEUPPINS; i++) {
		if ((wkupfr & BIT(i)) && (wkupenr & BIT(i))) {
			dev_dbg(priv->dev, "handle wkup irq:%d\n", i);
			generic_handle_irq(irq_find_mapping(priv->domain, i));
		}
	}

	chained_irq_exit(chip, desc);
}

static int stm32_pwr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct stm32_pwr_data *priv;

	struct resource *res;
	int ret;

	priv = devm_kzalloc(dev, sizeof(struct stm32_pwr_data), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->base)) {
		dev_err(dev, "%s: Unable to map IO memory\n", __func__);
		return PTR_ERR(priv->base);
	}

	/* Disable all wake-up pins */
	SMC(STM32_SVC_PWR, STM32_WRITE, MPUWKUPENR, 0);
	/* Clear all interrupts flags */
	SMC(STM32_SVC_PWR, STM32_SET_BITS, WKUPCR, WKUP_FLAGS_MASK);

	priv->domain = irq_domain_add_linear(np, NB_WAKEUPPINS,
					     &stm32_pwr_irq_domain_ops, priv);
	if (!priv->domain) {
		dev_err(dev, "%s: Unable to add irq domain!\n", __func__);
		goto out;
	}

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(dev, "failed to get PWR IRQ\n");
		goto err;
	}
	priv->irq = ret;

	irq_set_chained_handler_and_data(priv->irq,
					 stm32_pwr_handle_irq, priv);

out:
	return 0;
err:
	irq_domain_remove(priv->domain);
	return ret;
}

static int stm32_pwr_remove(struct platform_device *pdev)
{
	struct stm32_pwr_data *priv = platform_get_drvdata(pdev);

	irq_domain_remove(priv->domain);
	return 0;
}

static const struct of_device_id stm32_pwr_match[] = {
	{ .compatible = "st,stm32mp1-pwr" },
	{},
};

static struct platform_driver stm32_pwr_driver = {
	.probe = stm32_pwr_probe,
	.remove = stm32_pwr_remove,
	.driver = {
		.name = "stm32-pwr",
		.owner = THIS_MODULE,
		.of_match_table = stm32_pwr_match,
	},
};

static int __init stm32_pwr_init(void)
{
	return platform_driver_register(&stm32_pwr_driver);
}
arch_initcall(stm32_pwr_init);
