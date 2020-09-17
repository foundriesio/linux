// SPDX-License-Identifier: (GPL-2.0-or-later OR MIT)
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/syscore_ops.h>
#include <soc/tcc/irq.h>

#define TCC_PIC_NAME		"tcc_pic"

/* index of pic registers */
#define POL_0                   0
#define POL_1                   1
#define CMB_CM4                 2
#define STRGB_CM4               3
#define CA7_IRQO_EN             4
#define PIC_REG_MAX             5

struct tcc_pic_ops {
	int (*set_polarity)(struct irq_data *d, unsigned int type);
	int (*mask_cm4)(int irq, unsigned int bus);
	int (*unmask_cm4)(int irq, unsigned int bus);
};

struct tcc_pic {
	void __iomem *reg[PIC_REG_MAX];
	unsigned int *reg_save[PIC_REG_MAX];
	u32 reg_size[PIC_REG_MAX];
	int demarcation;
	int max_irq;
	struct tcc_pic_ops *ops;
	spinlock_t lock;
};

static struct tcc_pic *picinfo;

static int tcc_pic_set_polarity(struct irq_data *irqd, unsigned int type)
{
	void __iomem *pol;
	u32 mask;
	int ret = 0;
	int irq;
	unsigned long flags;

	if ((irqd->hwirq < 32) || (picinfo->reg[POL_0] == NULL)
			       || (picinfo->reg[POL_1] == NULL)) {
		pr_err("[ERROR][%s] %s: You have tried the wrong approach:%ld\n",
		       TCC_PIC_NAME, __func__, irqd->hwirq);
		return -EFAULT;
	}
	irq = irqd->hwirq - 32;

	if (irq > (picinfo->max_irq - 1)) {
		pr_err("[ERROR][%s] %s: Wrong IRQ:%d\n", TCC_PIC_NAME,
		       __func__, irq);
		return -EFAULT;
	}

	if (irq < picinfo->demarcation) {
		pol = picinfo->reg[POL_0] + ((irq >> 5) << 2);
	} else {
		pol = picinfo->reg[POL_1]
		      + (((irq - picinfo->demarcation) >> 5) << 2);
	}
	mask = 1 << (irq & 0x1F);

	spin_lock_irqsave(&picinfo->lock, flags);
	if (type == IRQ_TYPE_LEVEL_HIGH || type == IRQ_TYPE_EDGE_RISING) {
		writel_relaxed(readl_relaxed(pol) & ~mask, pol);
	} else if (type == IRQ_TYPE_LEVEL_LOW
		|| type == IRQ_TYPE_EDGE_FALLING) {
		writel_relaxed(readl_relaxed(pol) | mask, pol);
	} else {
		ret = -EINVAL;
	}
	spin_unlock_irqrestore(&picinfo->lock, flags);

	return ret;
}

static int tcc_pic_mask_control(int irq, unsigned int bus, unsigned int enable)
{
	void __iomem *reg = NULL;
	u32 mask;
	unsigned long flags;

	if (irq > (picinfo->max_irq - 1)) {
		pr_err("[ERROR][%s] %s: Wrong IRQ:%d\n", TCC_PIC_NAME,
		       __func__, irq);
		return -EFAULT;
	}

	if (bus == PIC_GATE_CMB) {
		if (picinfo->reg[CMB_CM4] == NULL) {
			pr_err("[ERROR][%s] %s: You have tried the wrong approach:%d\n",
			       TCC_PIC_NAME, __func__, irq);
			return -EFAULT;
		}
		reg = picinfo->reg[CMB_CM4] + ((irq >> 5) << 2);
	} else if (bus == PIC_GATE_STRGB) {
		if (picinfo->reg[STRGB_CM4] == NULL) {
			pr_err("[ERROR][%s] %s: You have tried the wrong approach:%d\n",
			       TCC_PIC_NAME, __func__, irq);
			return -EFAULT;
		}
		reg = picinfo->reg[STRGB_CM4] + ((irq >> 5) << 2);
	} else if (bus == SMU_GATE_CA7) {
		if (picinfo->reg[CA7_IRQO_EN] == NULL) {
			pr_err("[ERROR][%s] %s: You have tried the wrong approach.\n",
			       TCC_PIC_NAME, __func__);
			return -EFAULT;
		}
		reg = picinfo->reg[CA7_IRQO_EN] + ((irq >> 5) << 2);
	}
	mask = 1 << (irq & 0x1F);

	spin_lock_irqsave(&picinfo->lock, flags);
	if (enable == 0) {
		/* mask */
		writel_relaxed(readl_relaxed(reg) & ~mask, reg);
	} else {
		/* unmask */
		writel_relaxed(readl_relaxed(reg) | mask, reg);
	}
	spin_unlock_irqrestore(&picinfo->lock, flags);

	return 0;
}

static int tcc_pic_mask_cm4(int irq, unsigned int bus)
{
	return tcc_pic_mask_control(irq, bus, 0);
}

static int tcc_pic_unmask_cm4(int irq, unsigned int bus)
{
	return tcc_pic_mask_control(irq, bus, 1);
}

static void tcc_pic_allmask_ca7(void)
{
	unsigned long flags;

	if (picinfo->reg[CA7_IRQO_EN] == NULL) {
		pr_err("[ERROR][%s] %s: You have tried the wrong approach.\n",
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

int tcc_irq_set_polarity_ca7(int irq, unsigned int type)
{
	void __iomem *pol;
	u32 mask;
	int ret = 0;
	unsigned long flags;

	if ((picinfo->reg[POL_0] == NULL) || (picinfo->reg[POL_1] == NULL)) {
		pr_err("[ERROR][%s] %s: You have tried the wrong approach\n",
		       TCC_PIC_NAME, __func__);
		return -EFAULT;
	}

	if (irq > (picinfo->max_irq - 1)) {
		pr_err("[ERROR][%s] %s: Wrong IRQ:%d\n", TCC_PIC_NAME,
		       __func__, irq);
		return -EFAULT;
	}

	if (irq < picinfo->demarcation) {
		pol = picinfo->reg[POL_0] + ((irq >> 5) << 2);
	} else {
		pol = picinfo->reg[POL_1]
		      + (((irq - picinfo->demarcation) >> 5) << 2);
	}
	mask = 1 << (irq & 0x1F);

	spin_lock_irqsave(&picinfo->lock, flags);
	if (type == IRQ_TYPE_LEVEL_HIGH || type == IRQ_TYPE_EDGE_RISING) {
		writel_relaxed(readl_relaxed(pol) & ~mask, pol);
	} else if (type == IRQ_TYPE_LEVEL_LOW
		|| type == IRQ_TYPE_EDGE_FALLING) {
		writel_relaxed(readl_relaxed(pol) | mask, pol);
	} else {
		ret = -EINVAL;
	}
	spin_unlock_irqrestore(&picinfo->lock, flags);

	return ret;
}

int tcc_irq_mask_ca7(int irq)
{
	return tcc_pic_mask_control(irq, SMU_GATE_CA7, 0);
}

int tcc_irq_unmask_ca7(int irq)
{
	return tcc_pic_mask_control(irq, SMU_GATE_CA7, 1);
}

int tcc_irq_set_polarity(struct irq_data *irqd, unsigned int type)
{
	int ret = 0;

	if (picinfo->ops->set_polarity) {
		ret = picinfo->ops->set_polarity(irqd, type);
	}

	return ret;
}

int tcc_irq_mask_cm4(int irq, unsigned int bus)
{
	int ret = 0;

	if (picinfo->ops->mask_cm4) {
		ret = picinfo->ops->mask_cm4(irq, bus);
	}

	return ret;
}

int tcc_irq_unmask_cm4(int irq, unsigned int bus)
{
	int ret = 0;

	if (picinfo->ops->unmask_cm4) {
		ret = picinfo->ops->unmask_cm4(irq, bus);
	}

	return ret;
}

#if defined(CONFIG_PM_SLEEP)
#if defined(CONFIG_ARCH_TCC805X)
static int tcc_pic_suspend(void)
{
	int i, j;

	/* store all registers of pic */
	for (i = 0; i < PIC_REG_MAX && picinfo->reg_size[i] > 0; i++) {
		picinfo->reg_save[i] =
		    kzalloc(picinfo->reg_size[i], GFP_KERNEL);
		if (picinfo->reg_save[i] != NULL) {
			for (j = 0; (j << 2) < picinfo->reg_size[i]; j++) {
				*(picinfo->reg_save[i] + j) =
				    readl_relaxed(picinfo->reg[i] + (j << 2));
			}
		} else {
			pr_err("[ERROR][%s] %s: can't allocate memory.\n",
			       TCC_PIC_NAME, __func__);
			return -ENOMEM;
		}
	}

	return 0;
}

static void tcc_pic_resume(void)
{
	int i, j;

	/* restore all registers of pic */
	for (i = 0; i < PIC_REG_MAX && picinfo->reg_size[i] > 0; i++) {
		if (picinfo->reg_save[i] != NULL) {
			for (j = 0; (j << 2) < picinfo->reg_size[i]; j++) {
				writel_relaxed(*(picinfo->reg_save[i] + j),
					       picinfo->reg[i] + (j << 2));
			}
			kfree(picinfo->reg_save[i]);
		} else {
			pr_err("[ERROR][%s] %s: can't find memory.\n",
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

static int tcc_pic_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct tcc_pic *pic;
	struct resource res;
	int i, ret;

	pic = devm_kzalloc(dev, sizeof(*pic), GFP_KERNEL);
	if (pic != NULL) {
		memset(pic, 0, sizeof(*pic));
		picinfo = pic;
	} else {
		pr_err("[ERROR][%s] %s: can't alloccate memory.\n",
		       TCC_PIC_NAME, __func__);
		return -ENOMEM;
	}

	/* It'll be set as NULL if the size of reg node is zero. */
	for (i = 0; i < PIC_REG_MAX; i++) {
		picinfo->reg[i] = of_iomap(np, i);
		if (of_address_to_resource(np, i, &res)) {
			picinfo->reg_size[i] = 0;
		} else {
			picinfo->reg_size[i] = resource_size(&res);
		}
	}

	ret = of_property_read_s32(np, "demarcation", &picinfo->demarcation);
	if (ret != 0) {
		pr_err("[WARN][%s] %s: can't get the demarcation.\n",
		       TCC_PIC_NAME, __func__);
	}
	ret = of_property_read_s32(np, "max_irq", &picinfo->max_irq);
	if (ret != 0) {
		pr_err("[WARN][%s] %s: can't get the max_irq.\n",
		       TCC_PIC_NAME, __func__);
	}

	if (of_device_is_compatible(np, "telechips,tcc805x-pic")) {
		picinfo->ops = &tcc805x_pic_ops;
	} else if (of_device_is_compatible(np, "telechips,tcc803x-pic")) {
		picinfo->ops = &tcc803x_pic_ops;
	} else if (of_device_is_compatible(np, "telechips,tccsub-ca7-pic")) {
		picinfo->ops = &tccsub_ca7_pic_ops;
		tcc_pic_allmask_ca7();
	} else if (of_device_is_compatible(np, "telechips,tcc901x-pic")) {
		picinfo->ops = &tcc901x_pic_ops;
	} else if (of_device_is_compatible(np, "telechips,tcc899x-pic")) {
		picinfo->ops = &tcc899x_pic_ops;
	} else if (of_device_is_compatible(np, "telechips,tcc897x-pic")) {
		picinfo->ops = &tcc897x_pic_ops;
	} else {
		pr_err("[WARN][%s] %s: unable to support the device.\n",
		       TCC_PIC_NAME, __func__);
	}

	spin_lock_init(&picinfo->lock);
	platform_set_drvdata(pdev, picinfo);

#if defined(CONFIG_ARCH_TCC805X)
	register_syscore_ops(&tcc_pic_syscore_ops);
#endif

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id tcc_pic_of_match[] = {
	{.compatible = "telechips,tcc805x-pic"},
	{.compatible = "telechips,tcc803x-pic"},
	{.compatible = "telechips,tccsub-ca7-pic"},
	{.compatible = "telechips,tcc901x-pic"},
	{.compatible = "telechips,tcc899x-pic"},
	{.compatible = "telechips,tcc897x-pic"},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_pic_of_match);
#endif

static struct platform_driver tcc_pic_driver = {
	.probe = tcc_pic_probe,
	.driver = {
		   .name = "tcc_pic",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(tcc_pic_of_match),
#endif
		   },
};

static int __init tcc_pic_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&tcc_pic_driver);
	if (ret < 0)
		pr_err("[ERROR][%s] %s: ret=%d\n", TCC_PIC_NAME,
		       __func__, ret);

	return ret;
}

pure_initcall(tcc_pic_init);
