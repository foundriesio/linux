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


#define to_tcc_pcie(x)	dev_get_drvdata((x)->dev)

struct tcc_pcie {
	struct dw_pcie	*pci;
	void __iomem		*phy_base;
	void __iomem		*cfg_base;
	void __iomem		*clk_base;
	int			reset_gpio;
	struct clk		*clk_aux;
	struct clk		*clk_apb;
	struct clk		*clk_pcs;
	struct clk		*clk_pci;
	struct clk		*clk_phy;
	struct reset_control	*rst;
	unsigned int		using_ext_ref_clk;
	unsigned int		for_si_test;
	unsigned 		*suspend_regs;
};

#define TCC_PCIE_REG_BACKUP_SIZE	0x90

/* PCIe CFG registers */
#define PCIE_CFG00			0x000
#define PCIE_CFG01			0x004
#define PCIE_CFG02			0x008
#define PCIE_CFG03			0x00c
#define PCIE_CFG04			0x010
#define PCIE_CFG04_APP_LTSSM_ENABLE	(1<<2)
#define PCIE_CFG05			0x014
#define PCIE_CFG06			0x018
#define PCIE_CFG07			0x01c
#define PCIE_CFG08			0x020
#define PCIE_CFG08_PHY_PLL_LOCKED	(1<<1)
#define PCIE_CFG09			0x024
#define PCIE_CFG10			0x028
#define PCIE_CFG11			0x02c
#define PCIE_CFG12			0x030
#define PCIE_CFG13			0x034
#define PCIE_CFG14			0x038
#define PCIE_CFG15			0x03c
#define PCIE_CFG16			0x040
#define PCIE_CFG17			0x044
#define PCIE_CFG18			0x048
#define PCIE_CFG19			0x04c
#define PCIE_CFG20			0x050
#define PCIE_CFG21			0x054
#define PCIE_CFG22			0x058
#define PCIE_CFG23			0x05c
#define PCIE_CFG24			0x060
#define PCIE_CFG25			0x064
#define PCIE_CFG26			0x068
#define PCIE_CFG26_RDLH_LINK_UP		(1<<22)
#define PCIE_CFG27			0x06c
#define PCIE_CFG28			0x070
#define PCIE_CFG29			0x074
#define PCIE_CFG30			0x078
#define PCIE_CFG31			0x07c
#define PCIE_CFG32			0x080
#define PCIE_CFG33			0x084
#define PCIE_CFG34			0x088
#define PCIE_CFG35			0x08c
#define PCIE_CFG36			0x090
#define PCIE_CFG37			0x094
#define PCIE_CFG38			0x098
#define PCIE_CFG39			0x09c
#define PCIE_CFG40			0x0a0
#define PCIE_CFG41			0x0a4
#define PCIE_CFG42			0x0a8
#define PCIE_CFG43			0x0ac
#define PCIE_CFG44			0x0b0
#define PCIE_CFG45			0x0b4
#define PCIE_CFG89			0x164

#define PCIE_PHY_REG08			0x020
#define PCIE_PHY_REG43			0x0AC

#define CLK_CFG0			0x00
#define CLK_CFG1			0x04
#define CLK_CFG2			0x08
#define CLK_CFG3			0x0C
#define CLK_CFG4			0x10

#define LINK_INTR_EN_MASK		0xFFFFF080
#define LINK_INTR_EN_VAL		0xFFFFD080	/* disable [13]INT_DISABLE */
#define PCIE_CFG_MSI_INT_MASK		(1<<30|1<<29|1<<18|1<<16|1<<5)
#define PCIE_CFG_INTX_MASK		(0xFF<<20|0xFF<<8|1<<7)

#define PCIE_DEVICE_RC			0x4
#define PCIE_DEVICE_EP			0x0
#define PCIE_REF_INT			0x0
#define PCIE_REF_EXT			0x1

static void tcc_pcie_writel(void __iomem *base, u32 reg, u32 val)
{
	writel(val, base + reg);
}

#if 0
static u32 tcc_pcie_readl(void __iomem *base, u32 reg)
{
	return readl(base + reg);
}
#endif

static inline void tcc_cfg_write(struct tcc_pcie *pcie, u32 val, u32 reg, u32 mask)
{
	writel((readl(pcie->cfg_base + reg) & ~mask)|val, pcie->cfg_base + reg);
}

static inline u32 tcc_cfg_read(struct tcc_pcie *pcie, u32 reg, u32 mask)
{
	return readl(pcie->cfg_base + reg) & mask;
}

static void tcc_pcie_assert_phy_reset(struct tcc_pcie *tp)
{
	u32 val;

	val = readl(tp->cfg_base + PCIE_CFG44);
	val &= ~(1<<6);
	writel(val, tp->cfg_base + PCIE_CFG44);
}

static void tcc_pcie_deassert_phy_reset(struct tcc_pcie *tp)
{
	u32 val;

	val = readl(tp->cfg_base + PCIE_CFG44);
	val |= (1<<6);
	writel(val, tp->cfg_base + PCIE_CFG44);
}

static void tcc_pcie_power_on_phy(struct tcc_pcie *tp)
{
	tcc_pcie_writel(tp->phy_base, PCIE_PHY_REG43, 0); // PHY POWER DOWN DISABLE
}

static void tcc_pcie_power_off_phy(struct tcc_pcie *tp)
{
	tcc_pcie_writel(tp->phy_base, PCIE_PHY_REG43, 2); // PHY POWER DOWN ENABLE
}

static void tcc_pcie_dev_sel(struct tcc_pcie *tp, u32 device_type)
{
	struct dw_pcie *pci = tp->pci;
	u32 val;

	if((device_type != PCIE_DEVICE_RC) && (device_type != PCIE_DEVICE_EP))
		dev_err(pci->dev, "Device Type Selection Error!\n");

	val = readl(tp->cfg_base + PCIE_CFG89);
	val |= (device_type<<6);
	writel(val, tp->cfg_base + PCIE_CFG89);
}

static int tcc_pcie_ref_clk_set(struct tcc_pcie *tp, u32 pll_val)
{
	u32 val;
	val = readl(tp->clk_base + CLK_CFG0);
	val &= ~(1<<31);
	writel(val, tp->clk_base + CLK_CFG0); // PLL disable
	val = pll_val|0x80000000;
	writel(val, tp->clk_base + CLK_CFG0);

	while((readl(tp->clk_base + CLK_CFG0)&0x800000)!=0x800000);

	val = readl(tp->clk_base + CLK_CFG4);
	val |= ((1<<18)|(1<<5)|(1<<0));
	writel(val, tp->clk_base + CLK_CFG4);

	return 0;
}

static void tcc_pcie_ref_clk(struct tcc_pcie *tp, u32 ref_type)
{
	u32 val;
	val = readl(tp->phy_base + PCIE_PHY_REG08);
	val |= ((ref_type)<<3);
	writel(val, tp->phy_base + PCIE_PHY_REG08);
}

static void tcc_pcie_assert_reset(struct tcc_pcie *tp)
{
	struct dw_pcie *pci = tp->pci;

	if (tp->reset_gpio >= 0) {
		devm_gpio_request_one(pci->dev, tp->reset_gpio,
				GPIOF_OUT_INIT_LOW, "RESET");
		mdelay(1);
		gpio_direction_output(tp->reset_gpio, 1);
	}
	return;
}

static int tcc_pcie_establish_link(struct tcc_pcie *tp)
{
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;
	int count = 0;

	tcc_pcie_dev_sel(tp, PCIE_DEVICE_RC);
	if(tp->using_ext_ref_clk)
	    tcc_pcie_ref_clk(tp, PCIE_REF_EXT);
	else
	    tcc_pcie_ref_clk(tp, PCIE_REF_INT);

	/* assert reset signals */
	tcc_pcie_assert_phy_reset(tp);

	/* de-assert phy reset */
	tcc_pcie_deassert_phy_reset(tp);

	udelay(500); //need to find proper delay

	/* DBI_RO_WR_EN = 1 */
	writel(readl(pci->dbi_base + 0x8bc)|0x1, pci->dbi_base + 0x8bc);

	// set target speed GEN4
	writel((readl(pci->dbi_base + 0x30)&~(0xF))|0x4, pci->dbi_base + 0x30);

	// wait clock

	/* setup root complex */
	dw_pcie_setup_rc(pp);

	/* assert reset signal */
	tcc_pcie_assert_reset(tp);

	/* assert LTSSM enable */
	tcc_cfg_write(tp, PCIE_CFG04_APP_LTSSM_ENABLE, PCIE_CFG04, PCIE_CFG04_APP_LTSSM_ENABLE);


	if(tp->for_si_test){
		dev_err(pci->dev, "######## PCI SI TEST MODE ########\n");
		while (1) {
	               wfi();
		}
	}

	/* check if the link is up or not */
	while (!dw_pcie_link_up(pci)) {
		mdelay(100);
		count++;
		if (count == 10) {
			unsigned int val;
			val = tcc_cfg_read(tp, PCIE_CFG08,
				PCIE_CFG08_PHY_PLL_LOCKED) ? 1 : 0;
			dev_err(pci->dev, "PLL Locked: 0x%x\n", val);
			dev_err(pci->dev, "PCIe Link Fail\n");
			return -EINVAL;
		}
	}

	dev_err(pci->dev, "Link up\n");

	return 0;
}

static irqreturn_t tcc_pcie_irq_handler(int irq, void *arg)
{
	struct tcc_pcie *tp = arg;
	irqreturn_t ret = IRQ_NONE;
	unsigned int val;

	val = tcc_cfg_read(tp, PCIE_CFG24, PCIE_CFG_INTX_MASK);
	if (val) {
		tcc_cfg_write(tp, val, PCIE_CFG25, PCIE_CFG_INTX_MASK);
		ret = IRQ_HANDLED;
	}

	return ret;
}

static irqreturn_t tcc_pcie_msi_irq_handler(int irq, void *arg)
{
	struct tcc_pcie *tp = arg;
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;
	irqreturn_t ret = IRQ_NONE;
	unsigned int val;

	val = tcc_cfg_read(tp, PCIE_CFG24, ~(PCIE_CFG_INTX_MASK|(1<<1)));
	if (val) {
		ret = dw_handle_msi_irq(pp);
		tcc_cfg_write(tp, val, PCIE_CFG25, PCIE_CFG_MSI_INT_MASK);
	}

	return ret;
}

static void tcc_pcie_enable_interrupts(struct tcc_pcie *tp)
{
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;
	u32 val;

	/* clear all interrupt signals */
	tcc_cfg_write(tp, 0xFFFFFFFF, PCIE_CFG25, 0xFFFFFFFF);

	val = 0xFFFFFFFD;
	writel(val, tp->cfg_base + 0x16c);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		dw_pcie_msi_init(pp);

	return;
}

static void tcc_pcie_writel_rc(struct dw_pcie *pci, void __iomem *base, u32 reg, size_t size, u32 val)
{
	int ret;

	if((reg == 0x30020c) || (reg == 0x30000C) || (reg == 0x30030C) || (reg == 0x30040C))
		val = 0;

	ret = dw_pcie_write(base + reg, size, val);
}

#if 0
static int tcc_pcie_cfg_read(void __iomem *addr, int where, int size, u32 *val)
{
	*val = readl(addr);

	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;
	else if (size != 4)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	return PCIBIOS_SUCCESSFUL;
}

static int tcc_pcie_cfg_write(void __iomem *addr, int where, int size, u32 val)
{
	if (size == 4)
		writel(val, addr);
	else if (size == 2)
		writew(val, addr + (where & 2));
	else if (size == 1)
		writeb(val, addr + (where & 3));
	else
		return PCIBIOS_BAD_REGISTER_NUMBER;

	return PCIBIOS_SUCCESSFUL;
}
#endif

static int tcc_pcie_link_up(struct dw_pcie *pci)
{
	struct tcc_pcie *tp = to_tcc_pcie(pci);

	if (!!tcc_cfg_read(tp, PCIE_CFG26, PCIE_CFG26_RDLH_LINK_UP) &&
	    !!tcc_cfg_read(tp, PCIE_CFG33, (1<<17)))
		return 1;

	return 0;
}

static int tcc_pcie_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tcc_pcie *tp = to_tcc_pcie(pci);

	if (dw_pcie_link_up(pci)) {
		dev_err(pci->dev, "Link already up\n");
	}
	else
		tcc_pcie_establish_link(tp);
	tcc_pcie_enable_interrupts(tp);

	return 0;
}

static const struct dw_pcie_host_ops tcc_pcie_host_ops = {
	.host_init = tcc_pcie_host_init,
};

static const struct dw_pcie_ops dw_pcie_ops = {
	.write_dbi = tcc_pcie_writel_rc,
	.link_up = tcc_pcie_link_up,
};

static int __init add_pcie_port(struct tcc_pcie *tp,
                                struct platform_device *pdev)
{

	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;
	int ret;

	pp->irq = platform_get_irq(pdev, 0);
	if (pp->irq<0) {
		dev_err(&pdev->dev, "failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_irq(&pdev->dev, pp->irq, tcc_pcie_irq_handler,
				IRQF_SHARED, "tcc-pcie", tp);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		pp->msi_irq = platform_get_irq(pdev, 0);
		if (!pp->msi_irq) {
			dev_err(&pdev->dev, "failed to get msi irq\n");
			return -ENODEV;
		}

		ret = devm_request_irq(&pdev->dev, pp->msi_irq,
					tcc_pcie_msi_irq_handler,
					IRQF_SHARED, "tcc-pcie-msi", tp);
		if (ret) {
			dev_err(&pdev->dev, "failed to request msi irq\n");
			return ret;
		}
	}

	pp->root_bus_nr = -1;
	pp->ops = &tcc_pcie_host_ops;

	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize host\n");
		return ret;
	}

	return 0;
}

static int tcc_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct tcc_pcie *tp;
	int ret=0;

	tp = devm_kzalloc(dev, sizeof(*tp), GFP_KERNEL);
	if (!tp)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	tp->pci = pci;

	pci->dbi_base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(pci->dbi_base)) {
		ret = PTR_ERR(pci->dbi_base);
		goto fail_get_address;
	}

	tp->phy_base = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(tp->phy_base)) {
		ret = PTR_ERR(tp->phy_base);
		goto fail_get_address;
	}

	tp->cfg_base = of_iomap(pdev->dev.of_node, 2);
	if (IS_ERR(tp->cfg_base)) {
		ret = PTR_ERR(tp->cfg_base);
		goto fail_get_address;
	}

	tp->clk_base = of_iomap(pdev->dev.of_node, 4);
	if (IS_ERR(tp->clk_base)) {
		ret = PTR_ERR(tp->clk_base);
		goto fail_get_address;
	}

	tp->reset_gpio = of_get_named_gpio(pdev->dev.of_node, "reset-gpio", 0);

	ret = of_property_read_u32(pdev->dev.of_node, "using_ext_ref_clk", &tp->using_ext_ref_clk);

	ret = of_property_read_u32(pdev->dev.of_node, "for_si_test", &tp->for_si_test);

	tp->clk_pci = devm_clk_get(&pdev->dev, "fbus_pci0");
	if (IS_ERR(tp->clk_pci)) {
		dev_err(&pdev->dev, "Failed to get pci0 clock\n");
		ret = PTR_ERR(tp->clk_pci);
		goto fail_clk_pci;
	}
	clk_set_rate(tp->clk_pci, 333333333);
	ret = clk_prepare_enable(tp->clk_pci);
	if (ret)
		goto fail_clk_pci;

	tp->clk_phy = devm_clk_get(&pdev->dev, "pcie_phy");
	if (IS_ERR(tp->clk_phy)) {
		dev_err(&pdev->dev, "Failed to get pcie phy\n");
		return PTR_ERR(tp->clk_phy);
	}
	clk_set_rate(tp->clk_phy, 100000000);
	ret = clk_prepare_enable(tp->clk_phy);
	if (ret)
		goto fail_clk_phy;

	tp->rst = devm_reset_control_get(&pdev->dev, NULL);

	if(!tp->using_ext_ref_clk){
		u32 pms = 0;
		ret = of_property_read_u32(pdev->dev.of_node, "ref_clk_pms", &pms);
                if (ret) {
                        dev_err(&pdev->dev, "PCI Ref. Clock PMS Error!.\n");
                        goto fail_get_address;
                }
		tcc_pcie_ref_clk_set(tp, pms);
	}

	platform_set_drvdata(pdev, tp);

	tcc_pcie_power_on_phy(tp);
	ret = add_pcie_port(tp, pdev);
	if (ret < 0) {
		tcc_pcie_power_off_phy(tp);
		goto fail_clk_pci;
	}

	tp->suspend_regs = kzalloc(TCC_PCIE_REG_BACKUP_SIZE, GFP_KERNEL);
	if (!tp->suspend_regs)
		dev_err(&pdev->dev, "failed to allocate pcie reg backup table\n");

	return 0;

fail_clk_pci:
	clk_disable_unprepare(tp->clk_pci);
fail_clk_phy:
	clk_disable_unprepare(tp->clk_phy);
fail_get_address:
	return ret;
}

static int __exit tcc_pcie_remove(struct platform_device *pdev)
{
	struct tcc_pcie *tp = platform_get_drvdata(pdev);
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;

	disable_irq(pp->irq);

	clk_disable_unprepare(tp->clk_phy);
	clk_disable_unprepare(tp->clk_pci);

	if (tp->reset_gpio >= 0) {
		gpio_direction_output(tp->reset_gpio, 0);
		mdelay(1);
		gpio_direction_output(tp->reset_gpio, 1);
	}
	return 0;
}

static int tcc_pcie_suspend_noirq(struct device *dev)
{
	struct tcc_pcie *tp = dev_get_drvdata(dev);
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;


	if (!tp->suspend_regs) {
		dev_err(dev, "%s: failed to allocate pcie reg backup table\n", __func__);
		return -1;
	}

	disable_irq(pp->irq);
	memcpy(tp->suspend_regs, pci->dbi_base, TCC_PCIE_REG_BACKUP_SIZE);

        gpio_direction_output(tp->reset_gpio, 0);
	tcc_pcie_power_off_phy(tp);

	writel(0, tp->cfg_base + 0x1C); // CACTIVE Clear
	mdelay(50);

	clk_disable_unprepare(tp->clk_phy);
	clk_disable_unprepare(tp->clk_pci);

	return 0;
}

static int tcc_pcie_resume_noirq(struct device *dev)
{
	struct tcc_pcie *tp = dev_get_drvdata(dev);
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;

	clk_prepare_enable(tp->clk_pci);
	clk_prepare_enable(tp->clk_phy);

	if (!IS_ERR(tp->rst)) {
		reset_control_assert(tp->rst);
		mdelay(10);
		reset_control_deassert(tp->rst);
	}

        tcc_pcie_establish_link(tp);
        tcc_pcie_enable_interrupts(tp);

	if (tp->suspend_regs)
		memcpy(pci->dbi_base, tp->suspend_regs, TCC_PCIE_REG_BACKUP_SIZE);

	enable_irq(pp->irq);
	return 0;
}

const struct dev_pm_ops tcc_pcie_pm = {
	.suspend_noirq	= tcc_pcie_suspend_noirq,
	.resume_noirq	= tcc_pcie_resume_noirq,
	.freeze_noirq	= tcc_pcie_suspend_noirq,
	.thaw_noirq	= tcc_pcie_resume_noirq,
};

static const struct of_device_id tcc_pcie_of_match[] = {
	{ .compatible = "telechips,pcie", },
	{},
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

static int __init tcc_pcie_init(void)
{
	return platform_driver_probe(&tcc_pcie_driver, tcc_pcie_probe);
}
subsys_initcall(tcc_pcie_init);
