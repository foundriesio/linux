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
#include <soc/tcc/irq.h>

#define TCC_PIC_NAME		"tcc_pic"

struct tcc_pic_ops {
	int (*set_polarity)(struct irq_data* d, unsigned int type);
	int (*mask_cm4)(int irq, unsigned int bus);
	int (*unmask_cm4)(int irq, unsigned int bus);
};

struct tcc_pic {
	void __iomem *pol_0_base;
	void __iomem *pol_1_base;
	void __iomem *cmb_cm4_base;
	void __iomem *strgb_cm4_base;
	void __iomem *ca7_irqo_en_base;
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

	if((irqd->hwirq < 32) || (picinfo->pol_0_base == NULL) || (picinfo->pol_1_base == NULL)) {
		printk(KERN_ERR "[ERROR][%s] %s: You have tried the wrong approach:%ld\n", TCC_PIC_NAME, __func__, irqd->hwirq);
		return -EFAULT;
	}
	irq = irqd->hwirq - 32;

	if (irq > (picinfo->max_irq-1)) {
		printk(KERN_ERR "[ERROR][%s] %s: Wrong IRQ:%d\n", TCC_PIC_NAME, __func__, irq);
		return -EFAULT;
	}

	if(irq < picinfo->demarcation) {
		pol = picinfo->pol_0_base + ((irq>>5)<<2);  /* (irq/32)*4 */
	} else {
		pol = picinfo->pol_1_base + (((irq-picinfo->demarcation)>>5)<<2);
	}
	mask = 1 << (irq&0x1F);  /* irq%32 */

	spin_lock_irqsave(&picinfo->lock, flags);
	if (type == IRQ_TYPE_LEVEL_HIGH || type == IRQ_TYPE_EDGE_RISING) {
		writel_relaxed(readl_relaxed(pol) & ~mask, pol);
	} else if (type == IRQ_TYPE_LEVEL_LOW || type == IRQ_TYPE_EDGE_FALLING) {
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

	if (irq > (picinfo->max_irq-1)) {
		printk(KERN_ERR "[ERROR][%s] %s: Wrong IRQ:%d\n", TCC_PIC_NAME, __func__, irq);
		return -EFAULT;
	}

	if (bus == PIC_GATE_CMB) {
		if (picinfo->cmb_cm4_base == NULL) {
			printk(KERN_ERR "[ERROR][%s] %s: You have tried the wrong approach:%d\n", TCC_PIC_NAME, __func__, irq);
			return -EFAULT;
		}
		reg = picinfo->cmb_cm4_base + ((irq>>5)<<2);  /* (irq/32)*4 */
	} else if (bus == PIC_GATE_STRGB) {
		if(picinfo->strgb_cm4_base == NULL) {
			printk(KERN_ERR "[ERROR][%s] %s: You have tried the wrong approach:%d\n", TCC_PIC_NAME, __func__, irq);
			return -EFAULT;
		}
		reg = picinfo->strgb_cm4_base + ((irq>>5)<<2);  /* (irq/32)*4 */
	} else if (bus == SMU_GATE_CA7) {
		if (picinfo->ca7_irqo_en_base == NULL) {
			printk(KERN_ERR "[ERROR][%s] %s: You have tried the wrong approach.\n", TCC_PIC_NAME, __func__);
			return -EFAULT;
		}
		reg = picinfo->ca7_irqo_en_base + ((irq>>5)<<2);  /* (irq/32)*4 */
	}
	mask = 1 << (irq&0x1F);  /* irq%32 */

	spin_lock_irqsave(&picinfo->lock, flags);
	if (enable == 0) {	/* mask */
		writel_relaxed(readl_relaxed(reg) & ~mask, reg);
	} else {	/* unmask */
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

static void tcc_pic_allmask_a7(void)
{
	if (picinfo->ca7_irqo_en_base == NULL) {
		printk(KERN_ERR "[ERROR][%s] %s: You have tried the wrong approach.\n", TCC_PIC_NAME, __func__);
	}

	writel_relaxed(0, picinfo->ca7_irqo_en_base+0x00);
	writel_relaxed(0, picinfo->ca7_irqo_en_base+0x04);
	writel_relaxed(0, picinfo->ca7_irqo_en_base+0x08);
	writel_relaxed(0, picinfo->ca7_irqo_en_base+0x0C);
	writel_relaxed(0, picinfo->ca7_irqo_en_base+0x10);
	writel_relaxed(0, picinfo->ca7_irqo_en_base+0x14);
	writel_relaxed(0, picinfo->ca7_irqo_en_base+0x18);
}

int tcc_irq_set_polarity_ca7(int irq, unsigned int type)
{
	void __iomem *pol;
	u32 mask;
	int ret = 0;
	unsigned long flags;

	if((picinfo->pol_0_base == NULL) || (picinfo->pol_1_base == NULL)) {
		printk(KERN_ERR "[ERROR][%s] %s: You have tried the wrong approach\n", TCC_PIC_NAME, __func__);
		return -EFAULT;
	}

	if (irq > (picinfo->max_irq-1)) {
		printk(KERN_ERR "[ERROR][%s] %s: Wrong IRQ:%d\n", TCC_PIC_NAME, __func__, irq);
		return -EFAULT;
	}

	if(irq < picinfo->demarcation) {
		pol = picinfo->pol_0_base + ((irq>>5)<<2);  /* (irq/32)*4 */
	} else {
		pol = picinfo->pol_1_base + (((irq-picinfo->demarcation)>>5)<<2);
	}
	mask = 1 << (irq&0x1F);  /* irq%32 */

	spin_lock_irqsave(&picinfo->lock, flags);
	if (type == IRQ_TYPE_LEVEL_HIGH || type == IRQ_TYPE_EDGE_RISING) {
		writel_relaxed(readl_relaxed(pol) & ~mask, pol);
	} else if (type == IRQ_TYPE_LEVEL_LOW || type == IRQ_TYPE_EDGE_FALLING) {
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
static struct tcc_pic_ops tcc805x_pic_ops = {
	.set_polarity	= tcc_pic_set_polarity,
	.mask_cm4	= tcc_pic_mask_cm4,
	.unmask_cm4	= tcc_pic_unmask_cm4,
};

static struct tcc_pic_ops tcc803x_pic_ops = {
	.set_polarity	= tcc_pic_set_polarity,
	.mask_cm4	= NULL,
	.unmask_cm4	= NULL,
};

static struct tcc_pic_ops tcc901x_pic_ops = {
	.set_polarity	= tcc_pic_set_polarity,
	.mask_cm4	= NULL,
	.unmask_cm4	= NULL,
};

static struct tcc_pic_ops tcc899x_pic_ops = {
	.set_polarity	= tcc_pic_set_polarity,
	.mask_cm4	= NULL,
	.unmask_cm4	= NULL,
};

static struct tcc_pic_ops tccsub_a7_pic_ops = {
	.set_polarity	= NULL,
	.mask_cm4	= NULL,
	.unmask_cm4	= NULL,
};

static struct tcc_pic_ops tcc897x_pic_ops = {
	.set_polarity	= tcc_pic_set_polarity,
	.mask_cm4	= NULL,
	.unmask_cm4	= NULL,
};

static int tcc_pic_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct tcc_pic *pic;
	int ret;

	pic = devm_kzalloc(dev, sizeof(*pic), GFP_KERNEL);
	if (pic != NULL) {
		memset(pic, 0, sizeof(*pic));
		picinfo = pic;
	} else {
		printk(KERN_ERR "[ERROR][%s] %s: can't alloccate memory.\n", TCC_PIC_NAME, __func__);
		return -ENOMEM;
	}

	/* It'll be set as NULL if the size of reg node is zero. */
	picinfo->pol_0_base = of_iomap(np, 0);
	picinfo->pol_1_base = of_iomap(np, 1);
	picinfo->cmb_cm4_base = of_iomap(np, 2);
	picinfo->strgb_cm4_base = of_iomap(np, 3);
	picinfo->ca7_irqo_en_base = of_iomap(np, 4);
	ret = of_property_read_s32(np, "demarcation", &picinfo->demarcation);
	if (ret != 0) {
		printk(KERN_WARNING "[WARN][%s] %s: can't get the demarcation.\n", TCC_PIC_NAME, __func__);
	}
	ret = of_property_read_s32(np, "max_irq", &picinfo->max_irq);
	if (ret != 0) {
		printk(KERN_WARNING "[WARN][%s] %s: can't get the max_irq.\n", TCC_PIC_NAME, __func__);
	}

	if (of_device_is_compatible(np, "telechips,tcc805x-pic")) {
		picinfo->ops = &tcc805x_pic_ops;
	} else if (of_device_is_compatible(np, "telechips,tcc803x-pic")) {
		picinfo->ops = &tcc803x_pic_ops;
	} else if (of_device_is_compatible(np, "telechips,tccsub-a7-pic")) {
		picinfo->ops = &tccsub_a7_pic_ops;
		tcc_pic_allmask_a7();
	} else if (of_device_is_compatible(np, "telechips,tcc901x-pic")) {
		picinfo->ops = &tcc901x_pic_ops;
	} else if (of_device_is_compatible(np, "telechips,tcc899x-pic")) {
		picinfo->ops = &tcc899x_pic_ops;
	} else if (of_device_is_compatible(np, "telechips,tcc897x-pic")) {
		picinfo->ops = &tcc897x_pic_ops;
	} else {
		printk(KERN_WARNING "[WARN][%s] %s: unable to support the device.\n", TCC_PIC_NAME, __func__);
	}

	spin_lock_init(&picinfo->lock);
	platform_set_drvdata(pdev, picinfo);

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id tcc_pic_of_match[] = {
	{ .compatible = "telechips,tcc805x-pic" },
	{ .compatible = "telechips,tcc803x-pic" },
	{ .compatible = "telechips,tccsub-a7-pic" },
	{ .compatible = "telechips,tcc901x-pic" },
	{ .compatible = "telechips,tcc899x-pic" },
	{ .compatible = "telechips,tcc897x-pic" },
	{ }
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
		printk(KERN_ERR "[ERROR][%s] %s: ret=%d\n", TCC_PIC_NAME, __func__, ret);

	return ret;
}
pure_initcall(tcc_pic_init);
