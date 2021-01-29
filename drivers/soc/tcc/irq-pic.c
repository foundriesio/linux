// SPDX-License-Identifier: (GPL-2.0-or-later OR MIT)
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/syscore_ops.h>
#include <soc/tcc/irq.h>

#define TCC_PIC_NAME		((const u8 *)"tcc_pic")

/* index of pic registers */
#define POL_0                   ((u32)0)
#define POL_1                   ((u32)1)
#define CMB_CM4                 ((u32)2)
#define STRGB_CM4               ((u32)3)
#define CA7_IRQO_EN             ((u32)4)
#define PIC_REG_MAX             ((u32)5)

struct tcc_pic_ops {
	s32 (*set_polarity)(struct irq_data *d, u32 type);
	s32 (*mask_cm4)(u32 irq, u32 bus);
	s32 (*unmask_cm4)(u32 irq, u32 bus);
};

struct tcc_pic {
	void __iomem *reg[PIC_REG_MAX];
	u32 *reg_save[PIC_REG_MAX];
	u32 reg_size[PIC_REG_MAX];
	u32 demarcation;
	u32 max_irq;
	const struct tcc_pic_ops *ops;
	spinlock_t lock;
};

static struct tcc_pic *picinfo;

static s32 tcc_pic_set_polarity(struct irq_data *irqd, u32 type)
{
	void __iomem *pol;
	u32 mask;
	s32 ret = 0;
	u32 irq;
	unsigned long flags;

	if (irqd == NULL) {
		return -EINVAL;
	}

	if ((irqd->hwirq < (u32)32) || (picinfo->reg[POL_0] == NULL)
				    || (picinfo->reg[POL_1] == NULL)) {
		(void)pr_err(
			     "[ERROR][%s] %s: You have tried the wrong approach:%ld\n",
			     TCC_PIC_NAME, __func__, irqd->hwirq);
		return -EFAULT;
	}
	irq = (u32)irqd->hwirq - (u32)32;

	if (irq > (picinfo->max_irq - (u32)1)) {
		(void)pr_info("[INFO][%s] %s: GIC will be set IRQ:%d\n",
			      TCC_PIC_NAME, __func__, irq);
		return ret;
	}

	if (irq < picinfo->demarcation) {
		pol = picinfo->reg[POL_0] + ((irq >> (u32)5) << (u32)2);
	} else {
		pol = picinfo->reg[POL_1]
		      + (((irq - picinfo->demarcation) >> (u32)5) << (u32)2);
	}
	mask = (u32)1 << (irq & (u32)0x1F);

	spin_lock_irqsave(&picinfo->lock, flags);

	if ((type == (u32)IRQ_TYPE_LEVEL_HIGH) ||
	    (type == (u32)IRQ_TYPE_EDGE_RISING)) {
		writel_relaxed(readl_relaxed(pol) & ~mask, pol);
	} else if ((type == (u32)IRQ_TYPE_LEVEL_LOW) ||
		   (type == (u32)IRQ_TYPE_EDGE_FALLING)) {
		writel_relaxed(readl_relaxed(pol) | mask, pol);
	} else {
		ret = -EINVAL;
	}

	spin_unlock_irqrestore(&picinfo->lock, flags);

	return ret;
}

static s32 tcc_pic_mask_control(u32 irq, u32 bus, u32 enable)
{
	void __iomem *reg = NULL;
	u32 mask;
	unsigned long flags;

	if (irq > (picinfo->max_irq - (u32)1)) {
		(void)pr_err("[ERROR][%s] %s: Wrong IRQ:%d\n",
			     TCC_PIC_NAME, __func__, irq);
		return -EFAULT;
	}

	if (bus == PIC_GATE_CMB) {
		if (picinfo->reg[CMB_CM4] == NULL) {
			(void)pr_err(
				     "[ERROR][%s] %s: You have tried the wrong approach:%d\n",
				     TCC_PIC_NAME, __func__, irq);
			return -EFAULT;
		}
		reg = picinfo->reg[CMB_CM4] + ((irq >> (u32)5) << (u32)2);
	} else if (bus == PIC_GATE_STRGB) {
		if (picinfo->reg[STRGB_CM4] == NULL) {
			(void)pr_err(
				     "[ERROR][%s] %s: You have tried the wrong approach:%d\n",
				     TCC_PIC_NAME, __func__, irq);
			return -EFAULT;
		}
		reg = picinfo->reg[STRGB_CM4] + ((irq >> (u32)5) << (u32)2);
	} else if (bus == SMU_GATE_CA7) {
		if (picinfo->reg[CA7_IRQO_EN] == NULL) {
			(void)pr_err(
				     "[ERROR][%s] %s: You have tried the wrong approach.\n",
				     TCC_PIC_NAME, __func__);
			return -EFAULT;
		}
		reg = picinfo->reg[CA7_IRQO_EN] + ((irq >> (u32)5) << (u32)2);
	} else {
		(void)pr_err("[ERROR][%s] %s: bus type(%d) is wrong.\n",
			     TCC_PIC_NAME, __func__, bus);
		return -EINVAL;
	}

	mask = (u32)1 << (irq & (u32)0x1F);

	spin_lock_irqsave(&picinfo->lock, flags);

	if (enable == (u32)0) {
		/* mask */
		writel_relaxed(readl_relaxed(reg) & ~mask, reg);
	} else {
		/* unmask */
		writel_relaxed(readl_relaxed(reg) | mask, reg);
	}

	spin_unlock_irqrestore(&picinfo->lock, flags);

	return 0;
}

static s32 tcc_pic_mask_cm4(u32 irq, u32 bus)
{
	return tcc_pic_mask_control(irq, bus, 0);
}

static s32 tcc_pic_unmask_cm4(u32 irq, u32 bus)
{
	return tcc_pic_mask_control(irq, bus, 1);
}

static void __maybe_unused tcc_pic_allmask_ca7(void)
{
	unsigned long flags;

	if (picinfo->reg[CA7_IRQO_EN] == NULL) {
		(void)pr_err(
			     "[ERROR][%s] %s: You have tried the wrong approach.\n",
			     TCC_PIC_NAME, __func__);
		return;
	}

	spin_lock_irqsave(&picinfo->lock, flags);
	writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x00);
	writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x04);
	writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x08);
	writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x0C);
	writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x10);
	writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x14);
	writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x18);
	spin_unlock_irqrestore(&picinfo->lock, flags);
}

s32 tcc_irq_set_polarity_ca7(u32 irq, u32 type)
{
	void __iomem *pol;
	u32 mask;
	s32 ret = 0;
	unsigned long flags;

	if ((picinfo->reg[POL_0] == NULL) || (picinfo->reg[POL_1] == NULL)) {
		(void)pr_err(
			     "[ERROR][%s] %s: You have tried the wrong approach\n",
			     TCC_PIC_NAME, __func__);
		return -EFAULT;
	}

	if (irq > (picinfo->max_irq - (u32)1)) {
		(void)pr_err("[ERROR][%s] %s: Wrong IRQ:%d\n",
			     TCC_PIC_NAME, __func__, irq);
		return -EFAULT;
	}

	if (irq < picinfo->demarcation) {
		pol = picinfo->reg[POL_0] + ((irq >> (u32)5) << (u32)2);
	} else {
		pol = picinfo->reg[POL_1]
		      + (((irq - picinfo->demarcation) >> (u32)5) << (u32)2);
	}
	mask = (u32)1 << (irq & (u32)0x1F);

	spin_lock_irqsave(&picinfo->lock, flags);

	if ((type == (u32)IRQ_TYPE_LEVEL_HIGH) ||
	    (type == (u32)IRQ_TYPE_EDGE_RISING)) {
		writel_relaxed(readl_relaxed(pol) & ~mask, pol);
	} else if ((type == (u32)IRQ_TYPE_LEVEL_LOW) ||
		   (type == (u32)IRQ_TYPE_EDGE_FALLING)) {
		writel_relaxed(readl_relaxed(pol) | mask, pol);
	} else {
		ret = -EINVAL;
	}

	spin_unlock_irqrestore(&picinfo->lock, flags);

	return ret;
}

s32 tcc_irq_mask_ca7(u32 irq)
{
	return tcc_pic_mask_control(irq, SMU_GATE_CA7, 0);
}

s32 tcc_irq_unmask_ca7(u32 irq)
{
	return tcc_pic_mask_control(irq, SMU_GATE_CA7, 1);
}

s32 tcc_irq_set_polarity(struct irq_data *irqd, u32 type)
{
	s32 ret = 0;

	if (picinfo->ops->set_polarity != NULL) {
		ret = picinfo->ops->set_polarity(irqd, type);
	}

	return ret;
}

s32 tcc_irq_mask_cm4(u32 irq, u32 bus)
{
	s32 ret = 0;

	if (picinfo->ops->mask_cm4 != NULL) {
		ret = picinfo->ops->mask_cm4(irq, bus);
	}

	return ret;
}

s32 tcc_irq_unmask_cm4(u32 irq, u32 bus)
{
	s32 ret = 0;

	if (picinfo->ops->unmask_cm4 != NULL) {
		ret = picinfo->ops->unmask_cm4(irq, bus);
	}

	return ret;
}

#if defined(CONFIG_PM_SLEEP)
#if defined(CONFIG_ARCH_TCC805X)
static s32 tcc_pic_suspend(void)
{
	u32 i, j;

	/* store all registers of pic */
	for (i = 0; i < PIC_REG_MAX; i++) {
		if (picinfo->reg_size[i] == (u32)0) {
			continue;
		}

		picinfo->reg_save[i] =
		    kzalloc(picinfo->reg_size[i], GFP_KERNEL);
		if (picinfo->reg_save[i] != NULL) {
			for (j = 0; (j << 2) < picinfo->reg_size[i]; j++) {
				*(picinfo->reg_save[i] + j) =
				    readl_relaxed(picinfo->reg[i] + (j << 2));
			}
		} else {
			(void)pr_err("[ERROR][%s] %s: can't allocate memory.\n",
				     TCC_PIC_NAME, __func__);
			return -ENOMEM;
		}
	}

	return 0;
}

static void tcc_pic_resume(void)
{
	u32 i, j;

	/* restore all registers of pic */
	for (i = 0; i < PIC_REG_MAX; i++) {
		if (picinfo->reg_size[i] == (u32)0) {
			continue;
		}

		if (picinfo->reg_save[i] != NULL) {
			for (j = 0; (j << (u32)2) < picinfo->reg_size[i]; j++) {
				writel_relaxed(*(picinfo->reg_save[i] + j),
					       picinfo->reg[i] + (j << (u32)2));
			}
			kfree(picinfo->reg_save[i]);
		} else {
			(void)pr_err("[ERROR][%s] %s: can't find memory.\n",
				     TCC_PIC_NAME, __func__);
			return;
		}
	}
}

static struct syscore_ops tcc_pic_syscore_ops = {
	.suspend = tcc_pic_suspend,
	.resume = tcc_pic_resume,
};
#endif
#endif

static struct tcc_pic_ops tcc805x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = tcc_pic_mask_cm4,
	.unmask_cm4 = tcc_pic_unmask_cm4,
};

static struct tcc_pic_ops tcc803x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

static struct tcc_pic_ops tcc901x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

static struct tcc_pic_ops tcc899x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

static struct tcc_pic_ops tccsub_ca7_pic_ops = {
	.set_polarity = NULL,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

static struct tcc_pic_ops tcc897x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

#ifdef CONFIG_OF
static const struct of_device_id tcc_pic_of_match[] = {
	{.compatible = "telechips,tcc805x-pic", .data = &tcc805x_pic_ops},
	{.compatible = "telechips,tcc803x-pic", .data = &tcc803x_pic_ops},
	{.compatible = "telechips,tccsub-ca7-pic", .data = &tccsub_ca7_pic_ops},
	{.compatible = "telechips,tcc901x-pic", .data = &tcc901x_pic_ops},
	{.compatible = "telechips,tcc899x-pic", .data = &tcc899x_pic_ops},
	{.compatible = "telechips,tcc897x-pic", .data = &tcc897x_pic_ops},
	{}
};

static s32 tcc_pic_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct device_node *np;
	const struct of_device_id *match;
	struct tcc_pic *pic;
	struct resource res;
	u32 i;
	s32 ret;

	if (pdev == NULL) {
		return -ENODEV;
	}

	dev = &pdev->dev;
	np = pdev->dev.of_node;

	pic = devm_kzalloc(dev, sizeof(*pic), GFP_KERNEL);
	if (pic != NULL) {
		(void)memset(pic, 0, sizeof(*pic));
		picinfo = pic;
	} else {
		(void)pr_err("[ERROR][%s] %s: can't alloccate memory.\n",
			     TCC_PIC_NAME, __func__);
		return -ENOMEM;
	}

	match = of_match_node(tcc_pic_of_match, np);
	if (match != NULL) {
		picinfo->ops = (const struct tcc_pic_ops *)match->data;
	} else {
		return -ENODEV;
	}

	/* It'll be set as NULL if the size of reg node is zero. */
	for (i = 0; i < PIC_REG_MAX; i++) {
		picinfo->reg[i] = of_iomap(np, (s32)i);
		ret = of_address_to_resource(np, (s32)i, &res);
		if (ret != 0) {
			picinfo->reg_size[i] = 0;
		} else {
			picinfo->reg_size[i] = (u32)resource_size(&res);
		}
	}

	ret = of_property_read_u32(np,
				   "demarcation",
				   (u32 *)&picinfo->demarcation);
	if (ret != 0) {
		(void)pr_err("[WARN][%s] %s: can't get the demarcation.\n",
			     TCC_PIC_NAME, __func__);
	}

	ret = of_property_read_u32(np, "max_irq", (u32 *)&picinfo->max_irq);
	if ((ret != 0) || (picinfo->max_irq < (u32)1)) {
		(void)pr_err("[WARN][%s] %s: max_irq is wrong.\n",
			     TCC_PIC_NAME, __func__);
	}

	ret = of_device_is_compatible(np, "telechips,tccsub-ca7-pic");
	if (ret != 0) {
		tcc_pic_allmask_ca7();
	} else {
		/* no operation */
	}

	spin_lock_init(&picinfo->lock);
	platform_set_drvdata(pdev, picinfo);

#if defined(CONFIG_ARCH_TCC805X)
	register_syscore_ops(&tcc_pic_syscore_ops);
#endif

	return ret;
}

MODULE_DEVICE_TABLE(of, tcc_pic_of_match);
#endif

static struct platform_driver tcc_pic_driver = {
	.probe = &tcc_pic_probe,
	.driver = {
		   .name = "tcc_pic",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(tcc_pic_of_match),
#endif
		   },
};

static s32 __init tcc_pic_init(void)
{
	s32 ret;

	ret = platform_driver_register(&tcc_pic_driver);
	if (ret < 0) {
		(void)pr_err("[ERROR][%s] %s: ret=%d\n",
			     TCC_PIC_NAME, __func__, ret);
	}

	return ret;
}

pure_initcall(tcc_pic_init);
