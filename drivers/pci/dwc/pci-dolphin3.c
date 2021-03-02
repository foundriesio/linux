/*
 * PCIe host controller driver for Telechips SoCs
 *
 * Copyright (C) 2016 Telechips Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/phy/phy.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "pcie-designware.h"

#define to_tcc_pcie(x)	(dev_get_drvdata((x)->dev))

struct tcc_pcie {
	struct dw_pcie	*pci;
	void __iomem		*phy_base;
	void __iomem		*cfg_base;
	void __iomem		*clk_base;
	s32			reset_gpio;
	s32			ep_pwr_gpio;
	struct clk		*clk_aux;
	struct clk		*clk_apb;
	struct clk		*clk_pcs;
	struct clk		*clk_pci;
	struct clk		*clk_phy;
	struct reset_control	*rst;
	u32			pms;
	u32			pci_gen;
	u32			using_ext_ref_clk;
	u32			for_si_test;
	u32			*suspend_regs;
};

#define TCC_PCIE_REG_BACKUP_SIZE	(0x90)
/* PCIe CFG registers */
#define PCIE_CFG00			(0x000)
#define PCIE_CFG01			(0x004)
#define PCIE_CFG02			(0x008)
#define PCIE_CFG03			(0x00c)
#define PCIE_CFG04			(0x010)
#define PCIE_CFG04_APP_LTSSM_ENABLE	(0x00000004)
#define PCIE_CFG05			(0x014)
#define PCIE_CFG06			(0x018)
#define PCIE_CFG07			(0x01c)
#define PCIE_CFG08			(0x00000020)
#define PCIE_CFG08_PHY_PLL_LOCKED	(0x00000002)
#define PCIE_CFG09			(0x024)
#define PCIE_CFG10			(0x028)
#define PCIE_CFG11			(0x02c)
#define PCIE_CFG12			(0x030)
#define PCIE_CFG13			(0x034)
#define PCIE_CFG14			(0x038)
#define PCIE_CFG15			(0x03c)
#define PCIE_CFG16			(0x040)
#define PCIE_CFG17			(0x044)
#define PCIE_CFG18			(0x048)
#define PCIE_CFG19			(0x04c)
#define PCIE_CFG20			(0x050)
#define PCIE_CFG21			(0x054)
#define PCIE_CFG22			(0x058)
#define PCIE_CFG23			(0x05c)
#define PCIE_CFG24			(0x060)
#define PCIE_CFG25			(0x064)
#define PCIE_CFG26			(0x068)
#define PCIE_CFG26_RDLH_LINK_UP		(0x00400000)
#define PCIE_CFG27			(0x06c)
#define PCIE_CFG28			(0x070)
#define PCIE_CFG29			(0x074)
#define PCIE_CFG30			(0x078)
#define PCIE_CFG31			(0x07c)
#define PCIE_CFG32			(0x080)
#define PCIE_CFG33			(0x084)
#define PCIE_CFG34			(0x088)
#define PCIE_CFG35			(0x08c)
#define PCIE_CFG36			(0x090)
#define PCIE_CFG37			(0x094)
#define PCIE_CFG38			(0x098)
#define PCIE_CFG39			(0x09c)
#define PCIE_CFG40			(0x0a0)
#define PCIE_CFG41			(0x0a4)
#define PCIE_CFG42			(0x0a8)
#define PCIE_CFG43			(0x0ac)
#define PCIE_CFG44			(0x0b0)
#define PCIE_CFG45			(0x0b4)
#define PCIE_CFG89			(0x164)

#define PCIE_PHY_REG08			(0x020)
#define PCIE_PHY_REG43			(0x0AC)

#define CLK_CFG0			(0x00)
#define CLK_CFG1			(0x04)
#define CLK_CFG2			(0x08)
#define CLK_CFG3			(0x0C)
#define CLK_CFG4			(0x10)

#define LINK_INTR_EN_MASK		(0xFFFFF080)
#define LINK_INTR_EN_VAL		(0xFFFFD080)
#define PCIE_CFG_MSI_INT_MASK		(0x60050020)
#define PCIE_CFG_INTX_MASK		(0x0FF0FF80)

#define PCIE_DEVICE_RC			(0x4)
#define PCIE_DEVICE_EP			(0x0)
#define PCIE_REF_INT			(0x0)
#define PCIE_REF_EXT			(0x1)

static void tcc_pcie_layer_set(struct tcc_pcie *tp);
static s32 __init tcc_pcie_init(void);

static void tcc_pcie_writel(void __iomem *base, u32 reg, u32 val)
{
	iowrite32(val, base + reg);
}

#if 0
static u32 tcc_pcie_readl(void __iomem *base, u32 reg)
{
	return readl(base + reg);
}
#endif

static inline void tcc_cfg_write(struct tcc_pcie *pcie, u32 val, u32 reg,
								u32 mask)
{
	if (pcie != NULL) {
		iowrite32((ioread32(pcie->cfg_base + reg) & ~mask)|val,
						pcie->cfg_base + reg);
	}
}

static inline u32 tcc_cfg_read(struct tcc_pcie *pcie, u32 reg, u32 mask)
{
	if (pcie != NULL) {
		return ioread32(pcie->cfg_base + reg) & mask;
	} else {
		return 0;
	}
}

static void tcc_pcie_assert_phy_reset(struct tcc_pcie *tp)
{
	u32 val;

	val = ioread32(tp->cfg_base + PCIE_CFG44);
	val &= ~(1u<<6);
	iowrite32(val, tp->cfg_base + PCIE_CFG44);
}

static void tcc_pcie_deassert_phy_reset(struct tcc_pcie *tp)
{
	u32 val;

	val = ioread32(tp->cfg_base + PCIE_CFG44);
	val |= (1u<<6);
	iowrite32(val, tp->cfg_base + PCIE_CFG44);
}

static void tcc_pcie_power_on_phy(struct tcc_pcie *tp)
{
	tcc_pcie_writel(tp->phy_base, PCIE_PHY_REG43, 0);
	// PHY POWER DOWN DISABLE
}

static void tcc_pcie_power_off_phy(struct tcc_pcie *tp)
{
	tcc_pcie_writel(tp->phy_base, PCIE_PHY_REG43, 2);
	// PHY POWER DOWN ENABLE
}

static void tcc_pcie_dev_sel(struct tcc_pcie *tp, u32 device_type)
{
	struct dw_pcie *pci = tp->pci;
	u32 val;

	if ((device_type != (u32)PCIE_DEVICE_RC) &&
			(device_type != (u32)PCIE_DEVICE_EP)) {
		dev_err(pci->dev, "Device Type Selection Error!\n");
	}

	val = ioread32(tp->cfg_base + PCIE_CFG89);
	val &= (~(0xFu<<6));
	val |= (device_type<<6);
	iowrite32(val, tp->cfg_base + PCIE_CFG89);
}

static void tcc_pcie_ref_clk_set(struct tcc_pcie *tp, u32 pll_val)
{
	u32 val;

	val = ioread32(tp->clk_base + CLK_CFG0);
	val &= ~(0x80000000u);
	iowrite32(val, tp->clk_base + CLK_CFG0); // PLL disable
	val = pll_val|0x80000000u;
	iowrite32(val, tp->clk_base + CLK_CFG0);

	while ((ioread32(tp->clk_base + CLK_CFG0)&0x800000u) != 0x800000u) {
		;
	}

	val = ioread32(tp->clk_base + CLK_CFG4);
	val |= 0x40021u;
	iowrite32(val, tp->clk_base + CLK_CFG4);

}

static void tcc_pcie_ref_clk(struct tcc_pcie *tp, u32 ref_type)
{
	u32 val;

	val = ioread32(tp->phy_base + PCIE_PHY_REG08);
	val |= ((ref_type)<<3);
	iowrite32(val, tp->phy_base + PCIE_PHY_REG08);
}

static void tcc_pcie_assert_reset(struct tcc_pcie *tp)
{
	struct dw_pcie *pci = tp->pci;
	s32 err;

	if (tp->reset_gpio > 0) {
		if (pci != NULL) {
			devm_gpio_request_one(pci->dev, (u32)tp->reset_gpio,
					GPIOF_OUT_INIT_LOW, "RESET");
		}
		mdelay(1);
		err = gpio_direction_output((u32)tp->reset_gpio, 1);
		if (err != (s32)0) {
			if (pci != NULL) {
				dev_err(pci->dev,
				"PCIE reset pin control Error!\n");
			}
		}
	}
	return;
}

void tcc_pcie_layer_set(struct tcc_pcie *tp)
{
	u32 val;
	struct dw_pcie *pci = tp->pci;

	if (tp->pci_gen >= 2u) {
		val = ioread32(pci->dbi_base + 0x80C);
		val |= 0x20000u;
		iowrite32(val, pci->dbi_base + 0x80C);
	}

	if (tp->pci_gen >= 3u) {
		val = ioread32(pci->dbi_base + 0x890);
		val |= 0x800u;
		iowrite32(val, pci->dbi_base + 0x890);
	}
}

static s32 tcc_pcie_establish_link(struct tcc_pcie *tp)
{
	struct dw_pcie *pci = NULL;
	struct pcie_port *pp = NULL;
	s32 count = 0;
	s32 err;

	pci = tp->pci;
	if (pci == NULL) {
		return -EINVAL;
	}
	pp = &pci->pp;

	/* EP power control , example : wifi_reg_on */
	if (tp->ep_pwr_gpio > 0) {
		devm_gpio_request_one(pci->dev, (u32)tp->ep_pwr_gpio,
					GPIOF_OUT_INIT_LOW, "EP_PWR");
		mdelay(1);
		err = gpio_direction_output((u32)tp->ep_pwr_gpio, 1);
		if (err != (s32)0) {
			dev_err(pci->dev, "PCIE EP power pin control Error!\n");
			return err;
		}
	}

	tcc_pcie_dev_sel(tp, PCIE_DEVICE_RC);
	if (tp->using_ext_ref_clk == 1u) {
	    tcc_pcie_ref_clk(tp, PCIE_REF_EXT);
	} else {
	    tcc_pcie_ref_clk(tp, PCIE_REF_INT);
	}

	/* assert reset signals */
	tcc_pcie_assert_phy_reset(tp);

	/* de-assert phy reset */
	tcc_pcie_deassert_phy_reset(tp);

	udelay(500); //need to find proper delay

	/* DBI_RO_WR_EN = 1 */
	iowrite32(ioread32(pci->dbi_base + 0x8BCu)|0x1u,
				pci->dbi_base + 0x8BCu);

	// set target speed GEN4
	if (tp->pci_gen > 4u) {
		iowrite32((ioread32(pci->dbi_base + 0xA0u)&~(0xFu))|3u,
						pci->dbi_base + 0xA0u);
	} else {
		iowrite32((ioread32(pci->dbi_base + 0xA0u)&~(0xFu))|tp->pci_gen,
							pci->dbi_base + 0xA0u);
	}

	tcc_pcie_layer_set(tp);

	// wait clock

	/* setup root complex */
	dw_pcie_setup_rc(pp);

	/* assert reset signal */
	tcc_pcie_assert_reset(tp);

	/* assert LTSSM enable */
	tcc_cfg_write(tp, PCIE_CFG04_APP_LTSSM_ENABLE, PCIE_CFG04,
					PCIE_CFG04_APP_LTSSM_ENABLE);

	if (tp->for_si_test == 1u) {
		dev_err(pci->dev, "######## PCI SI TEST MODE ########\n");
		while (1) {
			wfi();
		}
	}

	/* check if the link is up or not */
	err = dw_pcie_link_up(pci);
	while (err == (s32)0) {
		err = dw_pcie_link_up(pci);
		mdelay(100);
		count++;
		if (count == 10) {
			u32 val;

			val = tcc_cfg_read(tp, PCIE_CFG08,
				PCIE_CFG08_PHY_PLL_LOCKED);
			dev_err(pci->dev, "PLL Locked: 0x%x\n", val);
			dev_err(pci->dev, "PCIe Link Fail\n");
			return -EINVAL;
		}
	}

	dev_err(pci->dev, "Link up\n");

	return 0;
}

static irqreturn_t tcc_pcie_irq_handler(s32 irq, void *arg)
{
	struct tcc_pcie *tp = NULL;
	irqreturn_t ret = (irqreturn_t)IRQ_NONE;
	u32 val;

	tp = arg;

	val = tcc_cfg_read(tp, (u32)PCIE_CFG24, (u32)PCIE_CFG_INTX_MASK);
	if (val != 0u) {
		tcc_cfg_write(tp, (u32)val, PCIE_CFG25,
				(u32)PCIE_CFG_INTX_MASK);
		ret = (irqreturn_t)IRQ_HANDLED;
	}

	return ret;
}

static irqreturn_t tcc_pcie_msi_irq_handler(s32 irq, void *arg)
{
	struct tcc_pcie *tp = NULL;
	struct dw_pcie *pci = NULL;
	struct pcie_port *pp = NULL;
	irqreturn_t ret = (irqreturn_t)IRQ_NONE;
	u32 val;

	tp = arg;
	if (tp != NULL) {
		pci = tp->pci;
	}
	pp = &pci->pp;

	val = tcc_cfg_read(tp, (u32)PCIE_CFG24,
		~((u32)PCIE_CFG_INTX_MASK|(1u<<1)));
	if (val != 0u) {
		ret = dw_handle_msi_irq(pp);
		tcc_cfg_write(tp, val, (u32)PCIE_CFG25,
				(u32)PCIE_CFG_MSI_INT_MASK);
	}

	return ret;
}

static void tcc_pcie_enable_interrupts(struct tcc_pcie *tp)
{
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;
	u32 val;

	/* clear all interrupt signals */
	tcc_cfg_write(tp, 0xFFFFFFFFu, PCIE_CFG25, 0xFFFFFFFFu);

	val = 0xFFFFFFFDu;
	iowrite32(val, tp->cfg_base + 0x16cu);

#ifdef CONFIG_PCI_MSI
		dw_pcie_msi_init(pp);
#endif

	return;
}

static void tcc_pcie_writel_rc(struct dw_pcie *pci, void __iomem *base, u32 reg,
							size_t size, u32 val)
{
	s32 err;
	u32 write_val = val;

	if ((reg == 0x30020Cu) || (reg == 0x30000Cu)
	|| (reg == 0x30030Cu) || (reg == 0x30040Cu)) {
		write_val = 0;
	}
	err = dw_pcie_write(base + reg, (s32)size, write_val);
	if (err != (s32)0) {
		if (pci != NULL) {
			dev_err(pci->dev, "dw_pcie_write fail, base:0x%8p, offset:0x%8x, value:0x%8x \n",
						base, reg, val);
		}
	}
}

static s32 tcc_pcie_link_up(struct dw_pcie *pci)
{
	struct tcc_pcie *tp = NULL;
	u32 ret;

	if (pci != NULL) {
		tp = to_tcc_pcie(pci);
	}

	ret = tcc_cfg_read(tp, (u32)PCIE_CFG26, (u32)PCIE_CFG26_RDLH_LINK_UP);
	if (ret != 0u) {
		ret = tcc_cfg_read(tp, (u32)PCIE_CFG33, 0x20000u);
		if (ret != 0u) {
			return 1;
		}
	}

	return 0;
}

static s32 tcc_pcie_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = NULL;
	struct tcc_pcie *tp = NULL;
	s32 err;

	pci = to_dw_pcie_from_pp(pp);
	if (pci == NULL) {
		return -ENODEV;
	}
	tp = to_tcc_pcie(pci);
	if (tp == NULL) {
		return -ENODEV;
	}

	err = dw_pcie_link_up(pci);
	if (err != (s32)0) {
		dev_err(pci->dev, "Link already up\n");
	} else {
		err = tcc_pcie_establish_link(tp);
		if (err != (s32)0) {
			return -ENODEV;
		}
	}

	if (tp != NULL) {
		tcc_pcie_enable_interrupts(tp);
	}

	return 0;
}

static const struct dw_pcie_host_ops tcc_pcie_host_ops = {
	.host_init = tcc_pcie_host_init,
};

static const struct dw_pcie_ops dw_pcie_ops = {
	.write_dbi = tcc_pcie_writel_rc,
	.link_up = tcc_pcie_link_up,
};

static s32 __init add_pcie_port(struct tcc_pcie *tp,
				struct platform_device *pdev)
{

	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;
	s32 ret;

	/* For core reset */
	/* if it does not use core reset , these codes can be removed */
	ret = gpio_direction_output((u32)tp->reset_gpio, 0);
	if (ret != (s32)0) {
		dev_err(pci->dev, "PCIE reset pin control Error!\n");
		return ret;
	}
	tcc_cfg_write(tp, 0xFFFFFFFFu, PCIE_CFG25, 0xFFFFFFFFu);
	tcc_cfg_write(tp, 0x0, PCIE_CFG44, 0x7F);
	tcc_cfg_write(tp, 0x7F, PCIE_CFG44, 0x7F);
	/* /For core reset */

	pp->irq = platform_get_irq(pdev, 0);
	if (pp->irq < 0) {
		dev_err(&pdev->dev, "failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_irq(&pdev->dev, (unsigned int)pp->irq,
				tcc_pcie_irq_handler,
				IRQF_SHARED, "tcc-pcie", tp);
	if (ret != (s32)0) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}

#ifdef CONFIG_PCI_MSI
	pp->msi_irq = platform_get_irq(pdev, 0);
	if (pp->msi_irq < (s32)0) {
		dev_err(&pdev->dev, "failed to get msi irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(&pdev->dev, (unsigned int)pp->msi_irq,
				tcc_pcie_msi_irq_handler,
				IRQF_SHARED, "tcc-pcie-msi", tp);
	if (ret != (s32)0) {
		dev_err(&pdev->dev, "failed to request msi irq\n");
		return ret;
	}
#endif

	pp->root_bus_nr = 0;
	pp->ops = &tcc_pcie_host_ops;

	ret = dw_pcie_host_init(pp);
	if (ret != (s32)0) {
		dev_err(&pdev->dev, "failed to initialize host\n");
		return ret;
	}

	return 0;
}

static s32 tcc_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct tcc_pcie *tp;
	s32 ret = 0;

	if (pdev == NULL) {
		return -ENOMEM;
	}

	tp = devm_kzalloc(dev, sizeof(*tp), GFP_KERNEL);
	if (tp == NULL) {
		return -ENOMEM;
	}

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (pci == NULL) {
		return -ENOMEM;
	}

	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	tp->pci = pci;

	pci->dbi_base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(pci->dbi_base)) {
		ret = (s32)PTR_ERR(pci->dbi_base);
		goto fail_get_address;
	}

	tp->phy_base = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(tp->phy_base)) {
		ret = (s32)PTR_ERR(tp->phy_base);
		goto fail_get_address;
	}

	tp->cfg_base = of_iomap(pdev->dev.of_node, 2);
	if (IS_ERR(tp->cfg_base)) {
		ret = (s32)PTR_ERR(tp->cfg_base);
		goto fail_get_address;
	}

	tp->clk_base = of_iomap(pdev->dev.of_node, 4);
	if (IS_ERR(tp->clk_base)) {
		ret = (s32)PTR_ERR(tp->clk_base);
		goto fail_get_address;
	}

	tp->reset_gpio = of_get_named_gpio(pdev->dev.of_node, "reset-gpio", 0);
	tp->ep_pwr_gpio = of_get_named_gpio(pdev->dev.of_node,
					"ep_pwr-gpio", 0);

	ret = of_property_read_u32(pdev->dev.of_node, "pci_gen", &tp->pci_gen);
	ret = of_property_read_u32(pdev->dev.of_node, "using_ext_ref_clk",
						&tp->using_ext_ref_clk);
	ret = of_property_read_u32(pdev->dev.of_node, "for_si_test",
						&tp->for_si_test);

	tp->clk_pci = devm_clk_get(&pdev->dev, "fbus_pci0");
	if (IS_ERR(tp->clk_pci)) {
		dev_err(&pdev->dev, "Failed to get pci0 clock\n");
		ret = (s32)PTR_ERR(tp->clk_pci);
		goto fail_clk_pci;
	}
	ret = clk_set_rate(tp->clk_pci, 333333333);
	if (ret != (s32)0) {
		goto fail_clk_pci;
	}
	ret = clk_prepare_enable(tp->clk_pci);
	if (ret != (s32)0) {
		goto fail_clk_pci;
	}

	tp->clk_phy = devm_clk_get(&pdev->dev, "pcie_phy");
	if (IS_ERR(tp->clk_phy)) {
		dev_err(&pdev->dev, "Failed to get pcie phy\n");
		return (s32)PTR_ERR(tp->clk_phy);
	}
	ret = clk_set_rate(tp->clk_phy, 100000000);
	if (ret != (s32)0) {
		goto fail_clk_phy;
	}
	ret = clk_prepare_enable(tp->clk_phy);
	if (ret != (s32)0) {
		goto fail_clk_phy;
	}

	tp->rst = devm_reset_control_get(&pdev->dev, NULL);

	if (tp->using_ext_ref_clk != 1u) {
		ret = of_property_read_u32(pdev->dev.of_node, "ref_clk_pms",
								&tp->pms);
		if (ret != (s32)0) {
			dev_err(&pdev->dev, "PCI Ref. Clock PMS Error!.\n");
			goto fail_get_address;
		}
		tcc_pcie_ref_clk_set(tp, tp->pms);
	}

	platform_set_drvdata(pdev, tp);

	tcc_pcie_power_on_phy(tp);
	ret = add_pcie_port(tp, pdev);
	if (ret < (s32)0) {
		tcc_pcie_power_off_phy(tp);
		goto fail_clk_pci;
	}

	tp->suspend_regs = kzalloc(TCC_PCIE_REG_BACKUP_SIZE, GFP_KERNEL);
	if (tp->suspend_regs == NULL) {
		dev_err(&pdev->dev, "failed to allocate pcie reg backup table\n");
	}

	return 0;

fail_clk_pci:
	clk_disable_unprepare(tp->clk_pci);
fail_clk_phy:
	clk_disable_unprepare(tp->clk_phy);
fail_get_address:
	return ret;
}

static s32 __exit tcc_pcie_remove(struct platform_device *pdev)
{
	struct tcc_pcie *tp = platform_get_drvdata(pdev);
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;
	s32 err;

	disable_irq((u32)(pp->irq));

	clk_disable_unprepare(tp->clk_phy);
	clk_disable_unprepare(tp->clk_pci);

	if (tp->reset_gpio > 0) {
		err = gpio_direction_output((u32)tp->reset_gpio, 0);
		if (err != (s32)0) {
			return err;
		}
		mdelay(1);
		err = gpio_direction_output((u32)tp->reset_gpio, 1);
		if (err != (s32)0) {
			return err;
		}
	}
	return 0;
}

static s32 tcc_pcie_suspend_noirq(struct device *dev)
{
	struct tcc_pcie *tp = dev_get_drvdata(dev);
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;
	s32 err;

	if (tp->suspend_regs == NULL) {
		dev_err(dev, "%s: failed to allocate pcie reg backup table\n"
						, __func__);
		return -1;
	}

	disable_irq((u32)(pp->irq));
	memcpy(tp->suspend_regs, pci->dbi_base, TCC_PCIE_REG_BACKUP_SIZE);

	err = gpio_direction_output((u32)tp->reset_gpio, 0);
	if (err != (s32)0) {
		dev_err(pci->dev, "PCIE reset pin control Error!\n");
		return err;
	}
	tcc_pcie_power_off_phy(tp);

	iowrite32(0, tp->cfg_base + 0x1Cu); // CACTIVE Clear
	mdelay(50);

	clk_disable_unprepare(tp->clk_phy);
	clk_disable_unprepare(tp->clk_pci);

	return 0;
}

static s32 tcc_pcie_resume_noirq(struct device *dev)
{
	struct tcc_pcie *tp = dev_get_drvdata(dev);
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;
	s32 err;

	if (tp->using_ext_ref_clk != 1u) {
		tcc_pcie_ref_clk_set(tp, tp->pms);
	}

	err = clk_prepare_enable(tp->clk_pci);
	if (err != (s32)0) {
		return err;
	}
	err = clk_prepare_enable(tp->clk_phy);
	if (err != (s32)0) {
		return err;
	}

	tcc_pcie_power_on_phy(tp);

	if (!IS_ERR(tp->rst)) {
		err = reset_control_assert(tp->rst);
		if (err != (s32)0) {
			return err;
		}
		mdelay(10);
		err = reset_control_deassert(tp->rst);
		if (err != (s32)0) {
			return err;
		}
	}

	err = tcc_pcie_establish_link(tp);
	if (err != (s32)0) {
		return err;
	}
	tcc_pcie_enable_interrupts(tp);

	if (tp->suspend_regs != NULL) {
		if (pci != NULL) {
			memcpy(pci->dbi_base, tp->suspend_regs,
					TCC_PCIE_REG_BACKUP_SIZE);
		}
	}

	if (pp != NULL) {
		enable_irq((u32)(pp->irq));
	}

	return 0;
}

static const struct dev_pm_ops tcc_pcie_pm = {
	.suspend_noirq	= tcc_pcie_suspend_noirq,
	.resume_noirq	= tcc_pcie_resume_noirq,
	.freeze_noirq	= tcc_pcie_suspend_noirq,
	.thaw_noirq	= tcc_pcie_resume_noirq,
};

static const struct of_device_id tcc_pcie_of_match[] = {
	{ .compatible = "telechips,pcie", },
};

static struct platform_driver tcc_pcie_driver = {
	.remove		= __exit_p(tcc_pcie_remove),
	.driver = {
		.name	= "tcc-pcie",
		.pm	= &tcc_pcie_pm,
		.of_match_table = tcc_pcie_of_match,
	},
};

/* Telechips PCIe driver does not allow module unload */

static s32 __init tcc_pcie_init(void)
{
	return platform_driver_probe(&tcc_pcie_driver, tcc_pcie_probe);
}
subsys_initcall(tcc_pcie_init);
