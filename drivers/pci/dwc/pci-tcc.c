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
	int			reset_gpio;
	struct clk		*clk_aux;
	struct clk		*clk_apb;
	struct clk		*clk_pcs;
	struct clk		*clk_ref_ext;
	struct clk		*clk_hsio;
	struct clk		*clk_phy;
	struct reset_control	*rst;
	bool				using_phy;
	unsigned int		using_ext_ref_clk;
	unsigned int		for_si_test;
	struct phy		*phy;
	unsigned 		*suspend_regs;
};

#define TCC_PCIE_REG_BACKUP_SIZE	0x90

#define PCIE_LINK_WIDTH_SPEED_CONTROL	0x80C
#define PORT_LOGIC_SPEED_CHANGE		(0x1 << 17)

/* PCIe PHY registers */
#define PCIE_PHY_CMN_REG062		0x188
#define PCIE_PHY_CMN_REG064		0x190

/* PCIe CFG registers */
#define PCIE_CFG00			0x000
#define PCIE_CFG00_VEN_MSG_FMT		(1<<26)
#define PCIE_CFG01			0x004
#define PCIE_CFG02			0x008
#define PCIE_CFG03			0x00c
#define PCIE_CFG04			0x010
#define PCIE_CFG04_APP_LTSSM_ENABLE	(1<<2)
#define PCIE_CFG04_APP_INIT_RST		(1<<0)
#define PCIE_CFG05			0x014
#define PCIE_CFG06			0x018
#define PCIE_ELBI_SLV_DBI_ENABLE	(1<<21)
#define PCIE_CFG07			0x01c
#define PCIE_CFG08			0x020
#define PCIE_CFG08_PHY_PLL_LOCKED	(1<<1)
#define PCIE_CFG08_PHY_POWER_OFF	(1<<5)
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
#define PCIE_CFG44_CFG_POWER_UP_RST	(1<<6)
#define PCIE_CFG44_CFG_PERST		(1<<5)
#define PCIE_CFG44_PHY_CMN_REG_RST	(1<<4)
#define PCIE_CFG44_PHY_CMN_RST		(1<<3)
#define PCIE_CFG44_PHY_GLOBAL_RST	(1<<2)
#define PCIE_CFG44_PHY_TRSV_REG_RST	(1<<1)
#define PCIE_CFG44_PHY_TRSV_RST		(1<<0)
#define PCIE_CFG45			0x0b4

#define PCIE_PHY_TRSVRST		0x108

#define LINK_INTR_EN_MASK		0xFFFFE080
#define LINK_INTR_EN_VAL		0xFFFFC080	/* disable [13]INT_DISABLE */
#define PCIE_CFG_MSI_INT_MASK		(1<<31|1<<29|1<<21|1<<5)
#define PCIE_CFG_INTX_MASK		(0xF<<12|0xF<<8)


static inline void tcc_phy_writel(struct tcc_pcie *pcie, u32 val, u32 reg)
{
	writel(val, pcie->phy_base + reg);
}

static inline u32 tcc_phy_readl(struct tcc_pcie *pcie, u32 reg)
{
	return readl(pcie->phy_base + reg);
}

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
	tcc_cfg_write(tp, 0, PCIE_CFG44, PCIE_CFG44_PHY_TRSV_RST);
	tcc_cfg_write(tp, 0, PCIE_CFG44, PCIE_CFG44_PHY_TRSV_REG_RST);
	tcc_cfg_write(tp, 0, PCIE_CFG44, PCIE_CFG44_PHY_CMN_REG_RST);
	tcc_cfg_write(tp, 0, PCIE_CFG44, PCIE_CFG44_PHY_CMN_RST);
	tcc_cfg_write(tp, 0, PCIE_CFG44, PCIE_CFG44_CFG_PERST);
	tcc_cfg_write(tp, 0, PCIE_CFG44, PCIE_CFG44_PHY_GLOBAL_RST);
}

static void tcc_pcie_deassert_phy_reset(struct tcc_pcie *tp)
{
	tcc_cfg_write(tp, PCIE_CFG44_PHY_GLOBAL_RST, PCIE_CFG44, PCIE_CFG44_PHY_GLOBAL_RST);
	tcc_cfg_write(tp, PCIE_CFG44_CFG_PERST, PCIE_CFG44, PCIE_CFG44_CFG_PERST);
	tcc_cfg_write(tp, PCIE_CFG44_PHY_CMN_RST, PCIE_CFG44, PCIE_CFG44_PHY_CMN_RST);
	tcc_cfg_write(tp, PCIE_CFG44_PHY_CMN_REG_RST, PCIE_CFG44, PCIE_CFG44_PHY_CMN_REG_RST);
	tcc_cfg_write(tp, PCIE_CFG44_PHY_TRSV_REG_RST, PCIE_CFG44, PCIE_CFG44_PHY_TRSV_REG_RST);
	tcc_cfg_write(tp, PCIE_CFG44_PHY_TRSV_RST, PCIE_CFG44, PCIE_CFG44_PHY_TRSV_RST);
}

static void tcc_pcie_power_on_phy(struct tcc_pcie *tp)
{
	tcc_cfg_write(tp, 0, PCIE_CFG08, PCIE_CFG08_PHY_POWER_OFF);
}

static void tcc_pcie_power_off_phy(struct tcc_pcie *tp, int pwdn)
{
	u32 val;

	if (!pwdn)
		return;

	//tcc_pcie_assert_phy_reset(tp);
	tcc_cfg_write(tp, PCIE_CFG08_PHY_POWER_OFF, PCIE_CFG08, PCIE_CFG08_PHY_POWER_OFF);
}


static void tcc_pcie_init_phy(struct tcc_pcie *tp)
{
	int i;

	while(tcc_cfg_read(tp, PCIE_CFG08, 1<<1) == 0); // PLL Lock check
	while(tcc_cfg_read(tp, PCIE_CFG08, 1<<0) == 0); // CDR Lock check

	tcc_cfg_write(tp, 1<<4, PCIE_CFG43, 1<<4);
	udelay(100);


	while(tcc_cfg_read(tp, PCIE_CFG08, 1<<16) != 0) 
	{
		;
	}

	i = 0;
	while(tcc_cfg_read(tp, PCIE_CFG29, 1<<21) == 1) 
	{
		;
	}
	
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

	if(tp->using_ext_ref_clk){
		tcc_cfg_write(tp, 0<<8, PCIE_CFG08, 1<<8);
		/* Enable bit for PHY reference clock from CKC (1: Propagates clock from CKC to PHY) */
		tcc_cfg_write(tp, 1<<7, PCIE_CFG08, 1<<7);
		/* Select reference clock source to PHY (1:CKC(PCLKCTRL15), 0:Oscillator) */
		tcc_cfg_write(tp, 0<<6, PCIE_CFG08, 1<<6);
	}
	else{
	      tcc_cfg_write(tp, 1<<8, PCIE_CFG08, 1<<8);
	      /* Enable bit for PHY reference clock from CKC (1: Propagates clock from CKC to PHY) */
	      tcc_cfg_write(tp, 1<<7, PCIE_CFG08, 1<<7);
	      /* Select reference clock source to PHY (1:CKC(PCLKCTRL15), 0:Oscillator) */
	      tcc_cfg_write(tp, 1<<6, PCIE_CFG08, 1<<6);
	}

	/* assert reset signals */
	tcc_pcie_assert_phy_reset(tp);

	/* de-assert phy reset */
	tcc_pcie_deassert_phy_reset(tp);



	/* pulse for common reset */
	tcc_cfg_write(tp, 0, PCIE_CFG44, PCIE_CFG44_PHY_CMN_RST);
	udelay(500);
	tcc_cfg_write(tp, PCIE_CFG44_PHY_CMN_RST, PCIE_CFG44, PCIE_CFG44_PHY_CMN_RST);

	if(!tp->using_ext_ref_clk){
		writel(readl(tp->phy_base + PCIE_PHY_CMN_REG062)|0x8, tp->phy_base + PCIE_PHY_CMN_REG062);
		writel(readl(tp->phy_base + PCIE_PHY_CMN_REG064)|0x80, tp->phy_base + PCIE_PHY_CMN_REG064);
	}

	/* DBI_RO_WR_EN = 1 */
	writel(readl(pci->dbi_base + 0x8bc)|0x1, pci->dbi_base + 0x8bc);

	/* setup root complex */
	dw_pcie_setup_rc(pp);

	/* assert reset signal */
	tcc_pcie_assert_reset(tp);

	/* initialize phy */
	tcc_pcie_init_phy(tp);
	/* assert LTSSM enable */
	tcc_cfg_write(tp, PCIE_CFG04_APP_LTSSM_ENABLE, PCIE_CFG04, PCIE_CFG04_APP_LTSSM_ENABLE);

	writel(readl(pci->dbi_base + 0x80c)|0x20000,pci->dbi_base + 0x80c); // Set GEN2

	if(tp->for_si_test){
		printk("PHY: REG21:0x%08x, REG22:0x%08x\n", tcc_phy_readl(tp, 0x21*4), tcc_phy_readl(tp, 0x22*4));
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
			printk("PLL Locked: 0x%x\n", val);
			printk("PCIe Link Fail\n");
			return -EINVAL;
		}
	}

	printk("Link up\n");
	clk_disable_unprepare(tp->clk_aux);

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

#if defined(CONFIG_ARCH_TCC898X)
	if (system_rev >= 0x0002)
#endif
	{
		val = tcc_cfg_read(tp, PCIE_CFG04, LINK_INTR_EN_MASK);
		val |= LINK_INTR_EN_VAL;
		tcc_cfg_write(tp, val, PCIE_CFG04, LINK_INTR_EN_MASK);
	}

	if (IS_ENABLED(CONFIG_PCI_MSI))
		dw_pcie_msi_init(pp);

	return;
}

static void tcc_pcie_writel_rc(struct dw_pcie *pci, void __iomem *base, u32 reg, size_t size, u32 val)
{
	int ret;

	if ((reg == 0x90c) || (reg == 0x914)) {
		if (((val & 0xFFF00000) == 0x11000000)
		|| ((val & 0xFFF00000) == 0x11600000)
		|| ((val & 0xFFF00000) == 0x11700000)
		|| ((val & 0xFFF00000) == 0x11800000)
		)
			val &= 0x00FFFFFF;
	}

	ret = dw_pcie_write(base + reg, size, val);
}

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

	printk("[][][][] %s [][][][]\n", __func__);

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

	tp->reset_gpio = of_get_named_gpio(pdev->dev.of_node, "reset-gpio", 0);

	ret = of_property_read_u32(pdev->dev.of_node, "using_ext_ref_clk", &tp->using_ext_ref_clk);

	ret = of_property_read_u32(pdev->dev.of_node, "for_si_test", &tp->for_si_test);

	tp->clk_phy = devm_clk_get(&pdev->dev, "pcie_phy");
	if (IS_ERR(tp->clk_phy)) {
		dev_err(&pdev->dev, "Failed to get pcie phy\n");
		return PTR_ERR(tp->clk_phy);
	}
	ret = clk_prepare_enable(tp->clk_phy);
	if (ret)
		return ret;

	tp->rst = devm_reset_control_get(&pdev->dev, NULL);

	tp->clk_aux = devm_clk_get(&pdev->dev, "pcie_aux");
	if (IS_ERR(tp->clk_aux)) {
		dev_err(&pdev->dev, "Failed to get pcie aux clock\n");
		ret = PTR_ERR(tp->clk_aux);
		goto fail_clk_phy;
	}
	clk_set_rate(tp->clk_aux, 125000000);
	ret = clk_prepare_enable(tp->clk_aux);
	if (ret)
		goto fail_clk_phy;

	tp->clk_apb = devm_clk_get(&pdev->dev, "pcie_apb");
	if (IS_ERR(tp->clk_apb)) {
		dev_err(&pdev->dev, "Failed to get pcie apb clock\n");
		ret = PTR_ERR(tp->clk_apb);
		goto fail_clk_aux;
	}
	clk_set_rate(tp->clk_apb, 100000000);
	ret = clk_prepare_enable(tp->clk_apb);
	if (ret)
		goto fail_clk_aux;

	tp->clk_pcs = devm_clk_get(&pdev->dev, "pcie_pcs");
	if (IS_ERR(tp->clk_pcs)) {
		dev_err(&pdev->dev, "Failed to get pcie pcs clock\n");
		ret = PTR_ERR(tp->clk_pcs);
		goto fail_clk_apb;
	}
	clk_set_rate(tp->clk_pcs, 100000000);
	ret = clk_prepare_enable(tp->clk_pcs);
	if (ret)
		goto fail_clk_apb;
	tp->clk_ref_ext = devm_clk_get(&pdev->dev, "pcie_ref_ext");
	if (IS_ERR(tp->clk_ref_ext)) {
		dev_err(&pdev->dev, "Failed to get pcie ref ext clock\n");
		ret = PTR_ERR(tp->clk_ref_ext);
		goto fail_clk_pcs;
	}

	if(!tp->using_ext_ref_clk){
		clk_set_rate(tp->clk_ref_ext, 100000000);
		ret = clk_prepare_enable(tp->clk_ref_ext);
		if (ret)
			goto fail_clk_pcs;
	}

	tp->clk_hsio = devm_clk_get(&pdev->dev, "fbus_hsio");
	if (IS_ERR(tp->clk_hsio)) {
		dev_err(&pdev->dev, "Failed to get hsio clock\n");
		ret = PTR_ERR(tp->clk_hsio);
		goto fail_clk_ref_ext;
	}
	ret = clk_prepare_enable(tp->clk_hsio);
	if (ret)
		goto fail_clk_ref_ext;

	platform_set_drvdata(pdev, tp);

	tcc_pcie_power_on_phy(tp);
	ret = add_pcie_port(tp, pdev);
	if (ret < 0) {
		tcc_pcie_power_off_phy(tp, 1);
		goto fail_clk_hsio;
	}

	tp->suspend_regs = kzalloc(TCC_PCIE_REG_BACKUP_SIZE, GFP_KERNEL);
	if (!tp->suspend_regs)
		dev_err(&pdev->dev, "failed to allocate pcie reg backup table\n");

	printk("[][][][] %s [][][][] end\n", __func__);	
	return 0;

fail_clk_hsio:
	;//clk_disable_unprepare(tcc_pcie->clk_hsio);
fail_clk_ref_ext:
	clk_disable_unprepare(tp->clk_ref_ext);
fail_clk_pcs:
	clk_disable_unprepare(tp->clk_pcs);
fail_clk_apb:
	clk_disable_unprepare(tp->clk_apb);
fail_clk_aux:
	clk_disable_unprepare(tp->clk_aux);
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
	clk_disable_unprepare(tp->clk_hsio);

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
		printk("%s: failed to allocate pcie reg backup table\n", __func__);
		return -1;
	}

	disable_irq(pp->irq);
	memcpy(tp->suspend_regs, pci->dbi_base, TCC_PCIE_REG_BACKUP_SIZE);

    gpio_direction_output(tp->reset_gpio, 0);
	tcc_pcie_power_off_phy(tp, 1);

	writel(0, tp->cfg_base + 0x1C); // CACTIVE Clear
	mdelay(50);

	if(!tp->using_ext_ref_clk)
	    clk_disable_unprepare(tp->clk_ref_ext);
	clk_disable_unprepare(tp->clk_phy);
	clk_disable_unprepare(tp->clk_hsio);

	return 0;
}

static int tcc_pcie_resume_noirq(struct device *dev)
{
	struct tcc_pcie *tp = dev_get_drvdata(dev);
	struct dw_pcie *pci = tp->pci;
	struct pcie_port *pp = &pci->pp;

	u32 val;

	clk_prepare_enable(tp->clk_hsio);
	clk_prepare_enable(tp->clk_phy);
	if(!tp->using_ext_ref_clk)	
	    clk_prepare_enable(tp->clk_ref_ext);

	clk_prepare_enable(tp->clk_aux);

	if (!IS_ERR(tp->rst)) {
		reset_control_assert(tp->rst);
		mdelay(10);
		reset_control_deassert(tp->rst);
	}

	/* re-establish link */
	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		if (pp->ops->msi_host_init) {
		//	int ret = 0;
		//	ret = pp->ops->msi_host_init(pp, &dw_pcie_msi_chip);
		//	if (ret < 0)
		//		return ret;
		}
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
