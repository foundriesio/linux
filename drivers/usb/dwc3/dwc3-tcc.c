/*
 * Copyright (C) 2013 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/platform_data/dwc3-tcc.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/cpufreq.h>
#include <asm/system_info.h>
#include <linux/usb/usb_phy_generic.h>
#include "core.h"

struct USBPHYCFG {
	volatile unsigned int U30_CLKMASK;
	volatile unsigned int U30_SWRESETN;
	volatile unsigned int U30_PWRCTRL;
	volatile unsigned int U30_OVERCRNT;
	volatile unsigned int U30_PCFG0;
	volatile unsigned int U30_PCFG1;
	volatile unsigned int U30_PCFG2;
	volatile unsigned int U30_PCFG3;
	volatile unsigned int U30_PCFG4;
	volatile unsigned int U30_PFLT;
	volatile unsigned int U30_PINT;
	volatile unsigned int U30_LCFG;
	volatile unsigned int U30_PCR0;
	volatile unsigned int U30_PCR1;
	volatile unsigned int U30_PCR2;
	volatile unsigned int U30_SWUTMI;
};

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
struct USBSSPHYCFG {
	volatile unsigned int U30_CLKMASK;
	volatile unsigned int U30_SWRESETN;
	volatile unsigned int U30_PWRCTRL;
	volatile unsigned int U30_OVERCRNT;
	volatile unsigned int U30_PCFG0;
	volatile unsigned int U30_PCFG1;
	volatile unsigned int U30_PCFG2;
	volatile unsigned int U30_PCFG3;
	volatile unsigned int U30_PCFG4;
	volatile unsigned int U30_PCFG5;
	volatile unsigned int U30_PCFG6;
	volatile unsigned int U30_PCFG7;
	volatile unsigned int U30_PCFG8;
	volatile unsigned int U30_PCFG9;
	volatile unsigned int U30_PCFG10;
	volatile unsigned int U30_PCFG11;
	volatile unsigned int U30_PCFG12;
	volatile unsigned int U30_PCFG13;
	volatile unsigned int U30_PCFG14;
	volatile unsigned int U30_PCFG15;
	volatile unsigned int reserved0[10];
	volatile unsigned int U30_PFLT;
	volatile unsigned int U30_PINT;
	volatile unsigned int U30_LCFG;
	volatile unsigned int U30_PCR0;
	volatile unsigned int U30_PCR1;
	volatile unsigned int U30_PCR2;
	volatile unsigned int U30_SWUTMI;
	volatile unsigned int U30_DBG0;
	volatile unsigned int U30_DBG1;
	volatile unsigned int reserved[1];
	volatile unsigned int FPHY_PCFG0;
	volatile unsigned int FPHY_PCFG1;
	volatile unsigned int FPHY_PCFG2;
	volatile unsigned int FPHY_PCFG3;
	volatile unsigned int FPHY_PCFG4;
	volatile unsigned int FPHY_LCFG0;
	volatile unsigned int FPHY_LCFG1;
};
#endif

#define PMU_ISOL		0x09C
#define U30_PHY_ISOL	(1<<6)
#ifdef CONFIG_USB_DWC3_HOST	/* 016.03.30 */
static char *vbus = "on";
#else
static char *vbus = "off";
#endif /* CONFIG_USB_DWC3_HOST */
module_param(vbus, charp, 0);
MODULE_PARM_DESC(vbus, "vbus: on, off");

//static char *maximum_speed = "high";
//char *maximum_speed = "super";
//module_param(maximum_speed, charp, 0);
//MODULE_PARM_DESC(maximum_speed, "Maximum supported speed.");

struct dwc3_tcc {
	struct platform_device *dwc3;
	struct platform_device *usb2_phy;	//this is dummy, maybe
	struct platform_device *usb3_phy;	//this is dummy, maybe
	struct usb_phy *dwc3_phy;	//our dwc3's phy structure
	struct device *dev;
	struct clk *hclk;
	struct clk *phy_clk;

	int usb3en_ctrl_able;
	int host_en_gpio;

	int vbus_ctrl_able;
	int vbus_gpio;

	int vbus_source_ctrl;
	struct regulator *vbus_source;

	int vbus_status;
	int vbus_pm_status;

	void __iomem *phy_regs;	/* device memory/io */
	void __iomem *regs;	/* device memory/io */
};

#define TXVRT_SHIFT 6
#define TXVRT_MASK (0xF << TXVRT_SHIFT)
#define TXRISE_SHIFT 10
#define TXRISE_MASK (0x3 << TXRISE_SHIFT)
#define ISSET(X, MASK) ((unsigned long)(X)&((unsigned long)(MASK)))

//#define DWC3_SQ_TEST_MODE

#if defined(DWC3_SQ_TEST_MODE)
#define TX_DEEMPH_SHIFT			21
#define TX_DEEMPH_MASK			0x7E00000

#define TX_DEEMPH_6DB_SHIFT		15
#define TX_DEEMPH_6DB_MASK		0x1F8000

#define TX_SWING_SHIFT		8
#define TX_SWING_MASK		0x7F00

#define TX_MARGIN_SHIFT		3
#define TX_MARGIN_MASK		0x7

#define TX_DEEMPH_SET_SHIFT	1
#define TX_DEEMPH_SET_MASK	0x3

#define TX_VBOOST_LVL_SHIFT		0
#define TX_VBOOST_LVL_MASK		0x7

#define RX_EQ_SHIFT	8
#define RX_EQ_MASK	0x700

#define RX_BOOST_SHIFT	2
#define RX_BOOST_MASK	0x1C

#define SSC_REF_CLK_SEL_SHIFT 0
#define SSC_REF_CLK_SEL_MASK 0xFF

static int dm = 0x15;		// default value
//static int dm_device = 0x23;  // device mode
static int dm_host = 0x18;	// host mode
static int dm_6db = 0x20;
static int sw = 0x7F;
static int sel_dm = 0x1;
static int rx_eq = 5;		//0xFF;
static int rx_boost = 5;	//0xFF;
static int ssc = 0x3200;

module_param(dm, int, 0444);
MODULE_PARM_DESC(dm, "Tx De-Emphasis at 3.5 dB");	// max: 0x3F

module_param(dm_6db, int, 0444);
MODULE_PARM_DESC(dm_6db, "Tx De-Emphasis at 6 dB");	// max: 0x3F

module_param(sw, int, 0444);
MODULE_PARM_DESC(sw, "Tx Amplitude (Full Swing Mode)");

module_param(sel_dm, int, 0444);
MODULE_PARM_DESC(sel_dm, "Tx De-Emphasis set");

module_param(rx_eq, int, 0444);
MODULE_PARM_DESC(rx_eq, "Rx Equalization");

module_param(rx_boost, int, 0444);
MODULE_PARM_DESC(rx_boost, "Rx Boost");
#endif

#if 0
static int dwc3_tcc_parse_dt(struct platform_device *pdev,
			     struct dwc3_tcc *tcc);
static void dwc3_tcc_free_dt(struct dwc3_tcc *tcc);
#endif

static int dwc3_tcc_vbus_ctrl(struct dwc3_tcc *tcc, int on_off);

void dwc3_bit_set(void __iomem *base, u32 offset, u32 value)
{
	unsigned int uTmp;

	uTmp = readl(base + offset - DWC3_GLOBALS_REGS_START);
	writel((uTmp | value), base + offset - DWC3_GLOBALS_REGS_START);
}

void dwc3_bit_clear(void __iomem *base, u32 offset, u32 value)
{
	unsigned int uTmp;

	uTmp = readl(base + offset - DWC3_GLOBALS_REGS_START);
	writel((uTmp & (~value)), base + offset - DWC3_GLOBALS_REGS_START);
}

static ssize_t dwc3_tcc_vbus_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "dwc3 vbus - %s\n",
		       (tcc->vbus_status) ? "on" : "off");
}

static ssize_t dwc3_tcc_vbus_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));

	if (!strncmp(buf, "on", 2))
		dwc3_tcc_vbus_ctrl(tcc, ON);

	if (!strncmp(buf, "off", 3))
		dwc3_tcc_vbus_ctrl(tcc, OFF);

	return count;
}

static DEVICE_ATTR(vbus, 0644, dwc3_tcc_vbus_show, dwc3_tcc_vbus_store);

#if defined(DWC3_SQ_TEST_MODE)
static ssize_t dwc3_tcc_phy_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	tcc->dwc3_phy->read_ss_u30phy_reg_all(tcc->dwc3_phy);
#else
	tcc->dwc3_phy->read_u30phy_reg_all(tcc->dwc3_phy);
#endif

	return 0;
}

static ssize_t dwc3_tcc_phy_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	tcc->dwc3_phy->read_ss_u30phy_reg_all(tcc->dwc3_phy);
#else
	tcc->dwc3_phy->read_u30phy_reg_all(tcc->dwc3_phy);
#endif

	return count;
}

static DEVICE_ATTR(phydump, 0644, dwc3_tcc_phy_show, dwc3_tcc_phy_store);

static ssize_t dwc3_tcc_ssc_ovrd_in_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	unsigned int read_data = 0;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	read_data = tcc->dwc3_phy->read_ss_u30phy_reg(tcc->dwc3_phy, 0x13);
#else
	read_data = tcc->dwc3_phy->read_u30phy_reg(tcc->dwc3_phy, 0x13);
#endif

	return sprintf(buf, "ssc_ovrd_in:0x%08x\n", read_data);
}

static ssize_t dwc3_tcc_ssc_ovrd_in_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	unsigned int uTmp = 0;
	uint32_t val = kstrtoul(buf, 0, (unsigned long *)16);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	tcc->dwc3_phy->read_ss_u30phy_reg(tcc->dwc3_phy, 0x13);
#else
	tcc->dwc3_phy->read_u30phy_reg(tcc->dwc3_phy, 0x13);
#endif

	BITCSET(uTmp, 0xFF, val);
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	tcc->dwc3_phy->write_ss_u30phy_reg(tcc->dwc3_phy, 0x13, uTmp);
#else
	tcc->dwc3_phy->write_u30phy_reg(tcc->dwc3_phy, 0x13, uTmp);
#endif

	return count;
}

static DEVICE_ATTR(ssc_ovrd_in, 0644,
		   dwc3_tcc_ssc_ovrd_in_show, dwc3_tcc_ssc_ovrd_in_store);

static ssize_t dwc3_tcc_ssc_asic_in_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	unsigned int read_data = 0;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	tcc->dwc3_phy->read_ss_u30phy_reg(tcc->dwc3_phy, 0x16);
#else
	tcc->dwc3_phy->read_u30phy_reg(tcc->dwc3_phy, 0x16);
#endif

	return sprintf(buf, "ssc_ovrd_in:0x%08x\n", read_data);
}

static ssize_t dwc3_tcc_ssc_asic_in_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	unsigned int uTmp = 0;
	uint32_t val = kstrtoul(buf, 0, (unsigned long *)16);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	uTmp = tcc->dwc3_phy->read_ss_u30phy_reg(tcc->dwc3_phy, 0x16);
#else
	uTmp = tcc->dwc3_phy->read_u30phy_reg(tcc->dwc3_phy, 0x16);
#endif

	BITCSET(uTmp, 0xFF, val);
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	tcc->dwc3_phy->write_ss_u30phy_reg(tcc->dwc3_phy, 0x16, uTmp);
#else
	tcc->dwc3_phy->write_u30phy_reg(tcc->dwc3_phy, 0x16, uTmp);
#endif

	return count;
}

static DEVICE_ATTR(ssc_asic_in, 0644,
		   dwc3_tcc_ssc_asic_in_show, dwc3_tcc_ssc_asic_in_store);

static ssize_t dwc3_tcc_rxboost_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	unsigned int read_data = 0;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	read_data = tcc->dwc3_phy->read_ss_u30phy_reg(tcc->dwc3_phy, 0x1024);
#else
	read_data = tcc->dwc3_phy->read_u30phy_reg(tcc->dwc3_phy, 0x1024);
#endif

	return sprintf(buf, "dwc3 tcc: PHY cfg - RX BOOST: 0x%x\n",
		       ((read_data & RX_BOOST_MASK) >> RX_BOOST_SHIFT));
}

static ssize_t dwc3_tcc_rxboost_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	unsigned int uTmp, tmp;
	uint32_t val = kstrtoul(buf, 0, (unsigned long *)16);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	/* get the rx_boost value */
	uTmp = tcc->dwc3_phy->read_ss_u30phy_reg(tcc->dwc3_phy, 0x1024);
#else
	/* get the rx_boost value */
	uTmp = tcc->dwc3_phy->read_u30phy_reg(tcc->dwc3_phy, 0x1024);
#endif
	tmp = uTmp;
	/* set rx_boost value */
	BITCSET(uTmp, RX_BOOST_MASK, val << RX_BOOST_SHIFT);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	tcc->dwc3_phy->write_ss_u30phy_reg(tcc->dwc3_phy, 0x1024, uTmp);
#else
	tcc->dwc3_phy->write_u30phy_reg(tcc->dwc3_phy, 0x1024, uTmp);
#endif

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	/* get the rx_boost value */
	uTmp = tcc->dwc3_phy->read_ss_u30phy_reg(tcc->dwc3_phy, 0x1024);
#else
	/* get the rx_boost value */
	uTmp = tcc->dwc3_phy->read_u30phy_reg(tcc->dwc3_phy, 0x1024);
#endif

	pr_info("[INFO][USB] dwc3 tcc: PHY cfg - RX BOOST: 0x%x -> [0x%x]\n",
		((tmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT),
		((uTmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT));

	return count;
}

static DEVICE_ATTR(rx_boost, 0644,
		   dwc3_tcc_rxboost_show, dwc3_tcc_rxboost_store);

static ssize_t dwc3_tcc_rx_eq_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	unsigned int read_data = 0;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	read_data = tcc->dwc3_phy->read_ss_u30phy_reg(tcc->dwc3_phy, 0x1006);
#else
	read_data = tcc->dwc3_phy->read_u30phy_reg(tcc->dwc3_phy, 0x1006);
#endif

	return sprintf(buf, "dwc3 tcc: PHY cfg - RX EQ: 0x%x\n",
		       ((read_data & RX_EQ_MASK) >> RX_EQ_SHIFT));
}

static ssize_t dwc3_tcc_rx_eq_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	unsigned int uTmp, tmp;
	uint32_t val = kstrtoul(buf, 0, (unsigned long *)16);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	uTmp = tcc->dwc3_phy->read_ss_u30phy_reg(tcc->dwc3_phy, 0x1006);
#else
	uTmp = tcc->dwc3_phy->read_u30phy_reg(tcc->dwc3_phy, 0x1006);
#endif
	tmp = uTmp;

	BITCSET(uTmp, RX_EQ_MASK, val << RX_EQ_SHIFT);
	uTmp |= Hw11;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	tcc->dwc3_phy->write_ss_u30phy_reg(tcc->dwc3_phy, 0x1006, uTmp);
#else
	tcc->dwc3_phy->write_u30phy_reg(tcc->dwc3_phy, 0x1006, uTmp);
#endif

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	uTmp = tcc->dwc3_phy->read_ss_u30phy_reg(tcc->dwc3_phy, 0x1006);
#else
	uTmp = tcc->dwc3_phy->read_u30phy_reg(tcc->dwc3_phy, 0x1006);
#endif

	pr_info("[INFO][USB] dwc3 tcc: PHY cfg - RX EQ: 0x%x -> [0x%x]\n",
		((tmp & RX_EQ_MASK) >> RX_EQ_SHIFT),
		((uTmp & RX_EQ_MASK) >> RX_EQ_SHIFT));

	return count;
}

static DEVICE_ATTR(rx_eq, 0644, dwc3_tcc_rx_eq_show, dwc3_tcc_rx_eq_store);

static ssize_t dwc3_tcc_tx_dm_sel_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	if (sel_dm == 0)
		return sprintf(buf, "dwc3 tcc: PHY cfg - 6dB de-emphasis\n");
	else if (sel_dm == 1)
		return sprintf(buf,
			       "dwc3 tcc: PHY cfg - 3.5dB de-emphasis (default)\n");
	else if (sel_dm == 2)
		return sprintf(buf, "dwc3 tcc: PHY cfg - de-emphasis\n");
	else
		return sprintf(buf,
			       "dwc3 tcc: err! invalid value (0: -6dB 1: -3.5dB 2: de-emphasis)\n");
}

static ssize_t dwc3_tcc_tx_dm_sel_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	uint32_t val = kstrtoul(buf, 0, (unsigned long *)16);

	if (val < 0 || val > 2) {
		pr_info("[INFO][USB] dwc3 tcc: err! invalid value (0: -6dB 1: -3.5dB 2: de-emphasis)\n");
		return count;
	}

	if (val == 0)
		pr_info("[INFO][USB] dwc3 tcc: PHY cfg - 6dB de-emphasis\n");
	else if (val == 1)
		pr_info("[INFO][USB] dwc3 tcc: PHY cfg - 3.5dB de-emphasis (default)\n");
	else if (val == 2)
		pr_info("[INFO][USB] dwc3 tcc: PHY cfg - de-emphasis\n");

	sel_dm = val;

	dwc3_bit_clear(tcc->regs, DWC3_GUSB3PIPECTL(0), TX_DEEMPH_SET_MASK);
	dwc3_bit_set(tcc->regs, DWC3_GUSB3PIPECTL(0),
		     sel_dm << TX_DEEMPH_SET_SHIFT);

	return count;
}

static DEVICE_ATTR(tx_dm_sel, 0644,
		   dwc3_tcc_tx_dm_sel_show, dwc3_tcc_tx_dm_sel_store);

static ssize_t dwc3_tcc_tx_dm_3p5db_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	struct USBPHYCFG *USBPHYCFG =
	    (struct USBPHYCFG *) (tcc->dwc3_phy->get_base(tcc->dwc3_phy));

	return sprintf(buf,
		       "dwc3 tcc: TX_DEEMPH 3.5dB : 0x%x\n",
		       (USBPHYCFG->U30_PCFG3 & TX_DEEMPH_MASK) >>
		       TX_DEEMPH_SHIFT);
}

static ssize_t dwc3_tcc_tx_dm_3p5db_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	struct USBPHYCFG *USBPHYCFG =
	    (struct USBPHYCFG *) (tcc->dwc3_phy->get_base(tcc->dwc3_phy));
	uint32_t val = kstrtoul(buf, 0, (unsigned long *)16);
	uint32_t tmp = USBPHYCFG->U30_PCFG3;

	dm_host = val;
	BITCSET(USBPHYCFG->U30_PCFG3,
		TX_DEEMPH_MASK, dm_host << TX_DEEMPH_SHIFT);
	pr_info("[INFO][USB] dwc3 tcc: TX_DEEMPH 3.5dB : 0x%x -> [0x%x]\n",
		(tmp & TX_DEEMPH_MASK) >> TX_DEEMPH_SHIFT,
		(USBPHYCFG->U30_PCFG3 & TX_DEEMPH_MASK) >> TX_DEEMPH_SHIFT);

	return count;
}

static DEVICE_ATTR(tx_dm_3p5db, 0644,
		   dwc3_tcc_tx_dm_3p5db_show, dwc3_tcc_tx_dm_3p5db_store);

static ssize_t dwc3_tcc_tx_dm_6db_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	struct USBPHYCFG *USBPHYCFG =
	    (struct USBPHYCFG *) (tcc->dwc3_phy->get_base(tcc->dwc3_phy));

	return sprintf(buf, "dwc3 tcc: TX_DEEMPH 6dB : 0x%x\n",
		       (USBPHYCFG->U30_PCFG3 & TX_DEEMPH_6DB_MASK) >>
		       TX_DEEMPH_6DB_SHIFT);
}

static ssize_t dwc3_tcc_tx_dm_6db_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	struct USBPHYCFG *USBPHYCFG =
	    (struct USBPHYCFG *) (tcc->dwc3_phy->get_base(tcc->dwc3_phy));
	uint32_t val = kstrtoul(buf, 0, (unsigned long *)16);
	uint32_t tmp = USBPHYCFG->U30_PCFG3;

	dm_6db = val;
	BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_6DB_MASK,
		dm_6db << TX_DEEMPH_6DB_SHIFT);
	pr_info("[INFO][USB] dwc3 tcc: TX_DEEMPH 6dB : 0x%x -> [0x%x]\n",
		(tmp & TX_DEEMPH_6DB_MASK) >> TX_DEEMPH_6DB_SHIFT,
		(USBPHYCFG->U30_PCFG3 & TX_DEEMPH_6DB_MASK) >>
		TX_DEEMPH_6DB_SHIFT);

	return count;
}

static DEVICE_ATTR(tx_dm_6db, 0644,
		   dwc3_tcc_tx_dm_6db_show, dwc3_tcc_tx_dm_6db_store);

static ssize_t dwc3_tcc_tx_swing_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	struct USBPHYCFG *USBPHYCFG =
	    (struct USBPHYCFG *) (tcc->dwc3_phy->get_base(tcc->dwc3_phy));

	return sprintf(buf, "dwc3 tcc: TX_SWING : 0x%x\n",
		       (USBPHYCFG->U30_PCFG3 & TX_SWING_MASK) >>
				TX_SWING_SHIFT);
}

static ssize_t dwc3_tcc_tx_swing_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	struct USBPHYCFG *USBPHYCFG =
	    (struct USBPHYCFG *) (tcc->dwc3_phy->get_base(tcc->dwc3_phy));
	uint32_t val = kstrtoul(buf, 0, (unsigned long *)16);
	uint32_t tmp = USBPHYCFG->U30_PCFG3;

	sw = val;
	BITCSET(USBPHYCFG->U30_PCFG3, TX_SWING_MASK, sw << TX_SWING_SHIFT);
	pr_info("[INFO][USB] dwc3 tcc: TX_SWING : 0x%x -> [0x%x]\n",
		(tmp & TX_SWING_MASK) >> TX_SWING_SHIFT,
		(USBPHYCFG->U30_PCFG3 & TX_SWING_MASK) >> TX_SWING_SHIFT);

	return count;
}

static DEVICE_ATTR(tx_swing, 0644,
		   dwc3_tcc_tx_swing_show, dwc3_tcc_tx_swing_store);

static ssize_t dwc3_tcc_tx_vboost_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	struct USBPHYCFG *USBPHYCFG =
	    (struct USBPHYCFG *) (tcc->dwc3_phy->get_base(tcc->dwc3_phy));

	return sprintf(buf, "dwc3 tcc: TX_VBOOST : 0x%x\n",
		       (USBPHYCFG->U30_PCFG2 & TX_VBOOST_LVL_MASK) >>
		       TX_VBOOST_LVL_SHIFT);
}

static ssize_t dwc3_tcc_tx_vboost_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	struct USBPHYCFG *USBPHYCFG =
	    (struct USBPHYCFG *) (tcc->dwc3_phy->get_base(tcc->dwc3_phy));
	uint32_t val = kstrtoul(buf, 0, (unsigned long *)16);
	uint32_t tmp = USBPHYCFG->U30_PCFG2;

	BITCSET(USBPHYCFG->U30_PCFG2, TX_VBOOST_LVL_MASK,
		val << TX_VBOOST_LVL_SHIFT);
	pr_info("[INFO][USB] dwc3 tcc: TX_VBOOST : 0x%x -> [0x%x]\n",
		(tmp & TX_VBOOST_LVL_MASK) >> TX_VBOOST_LVL_SHIFT,
		(USBPHYCFG->U30_PCFG2 & TX_VBOOST_LVL_MASK) >>
		TX_VBOOST_LVL_SHIFT);

	return count;
}

static DEVICE_ATTR(tx_vboost, 0644,
		   dwc3_tcc_tx_vboost_show, dwc3_tcc_tx_vboost_store);
#endif /* DWC3_SQ_TEST_MODE */

#define SOFFN_MASK        0x3FFF
#define SOFFN_SHIFT       3
#define GET_SOFFN_NUM(x)  ((x >> SOFFN_SHIFT)  & SOFFN_MASK)

/**
 * Show the current value of the USB30 PHY Configuration Register2 (U30_PCFG2)
 */
static ssize_t dwc3_eyep_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	uint32_t val;
	uint32_t reg;
	char str[256] = { 0 };

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	struct USBSSPHYCFG *pUSBPHYCFG =
	    (struct USBSSPHYCFG *)(tcc->dwc3_phy->get_base(tcc->dwc3_phy));
	reg = readl(&pUSBPHYCFG->FPHY_PCFG1);
	val = reg & 0xFU;
#else
	struct USBPHYCFG *pUSBPHYCFG =
	    (struct USBPHYCFG *)(tcc->dwc3_phy->get_base(tcc->dwc3_phy));
	reg = readl(&pUSBPHYCFG->U30_PCFG2);
	val = (ISSET(reg, TXVRT_MASK)) >> TXVRT_SHIFT;
#endif

	if ((val >= 0) && (val <= 0xF)) {
		sprintf(str, "%ld%ld%ld%ld",
			ISSET(val, 0x8) >> 3,
			ISSET(val, 0x4) >> 2,
			ISSET(val, 0x2) >> 1, ISSET(val, 0x1) >> 0);
	}
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	return sprintf(buf, "U30_FPCFG1 = 0x%08X\nTXVRT = %s\n", reg, str);
#else
	return sprintf(buf, "U30_PCFG2 = 0x%08X\nTXVREFTUNE = %s\n", reg, str);
#endif
}

/**
 * HS DC Voltage Level is set
 */
static ssize_t dwc3_eyep_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));
	uint32_t val = kstrtoul(buf, 0, (unsigned long *)2);
	uint32_t reg = 0;

#if defined(CONFIG_ARCH_TCC803X)
	struct USBSSPHYCFG *pUSBPHYCFG =
	    (struct USBSSPHYCFG *)tcc->dwc3_phy->get_base(tcc->dwc3_phy);
	reg = readl(&pUSBPHYCFG->FPHY_PCFG1);
#elif defined(CONFIG_ARCH_TCC805X)
	struct USBSSPHYCFG *pUSBPHYCFG =
	    (struct USBSSPHYCFG *)(tcc->dwc3_phy->get_base(tcc->dwc3_phy));
	reg = readl(&pUSBPHYCFG->FPHY_PCFG1);
#else
	struct USBPHYCFG *pUSBPHYCFG =
	    (struct USBPHYCFG *)(tcc->dwc3_phy->get_base(tcc->dwc3_phy));
	reg = readl(&pUSBPHYCFG->U30_PCFG2);
#endif

	if (count - 1 < 4 || 4 < count - 1) {
		pr_info("[INFO][USB]\nThis argument length is \x1b[1;33mnot 4\x1b[0m\n\n");
		pr_info("[INFO][USB] \tUsage : echo \x1b[1;31mxxxx\x1b[0m > xhci_eyep\n\n");
		pr_info("[INFO][USB] \t\t1) length of \x1b[1;32mxxxx\x1b[0m is 4\n");
		pr_info("[INFO][USB] \t\t2) \x1b[1;32mx\x1b[0m is binary number(\x1b[1;31m0\x1b[0m or \x1b[1;31m1\x1b[0m)\n\n");
		return count;
	}
	if (((buf[0] != '0') && (buf[0] != '1'))
	    || ((buf[1] != '0') && (buf[1] != '1'))
	    || ((buf[2] != '0') && (buf[2] != '1'))
	    || ((buf[3] != '0') && (buf[3] != '1'))) {
		pr_info("[INFO][USB]\necho %c%c%c%c is \x1b[1;33mnot binary value\x1b[0m\n\n",
		     buf[0], buf[1], buf[2], buf[3]);
		pr_info("[INFO][USB] \tUsage : echo \x1b[1;31mxxxx\x1b[0m > xhci_eyep\n\n");
		pr_info("[INFO][USB] \t\t1) \x1b[1;32mx\x1b[0m is binary number(\x1b[1;31m0\x1b[0m or \x1b[1;31m1\x1b[0m)\n\n");
		return count;
	}
#if defined(CONFIG_ARCH_TCC803X)
	BITCSET(reg, 0xF, val); // val range is 0x0 ~ 0xF
	writel(reg, &pUSBPHYCFG->FPHY_PCFG1);
#elif defined(CONFIG_ARCH_TCC805X)
	BITCSET(reg, 0xF, val); // val range is 0x0 ~ 0xF
	writel(reg, &pUSBPHYCFG->FPHY_PCFG1);
#else
	BITCSET(reg, TXVRT_MASK, val << TXVRT_SHIFT); // val range is 0x0 ~ 0xF
	writel(reg, &pUSBPHYCFG->U30_PCFG2);
#endif

	return count;
}

static DEVICE_ATTR(dwc3_eyep, 0644, dwc3_eyep_show,
		   dwc3_eyep_store);

int dwc3_tcc_power_ctrl(struct dwc3_tcc *tcc, int on_off)
{
	int err = 0;

	if ((tcc->vbus_source_ctrl == 1) && (tcc->vbus_source)) {
		if (on_off == ON) {
			err = regulator_enable(tcc->vbus_source);
			if (err) {
				pr_info("[INFO][USB] dwc3 tcc: can't enable vbus source\n");
				return err;
			}
			mdelay(1);
		} else if (on_off == OFF) {
			regulator_disable(tcc->vbus_source);
		}
	}

	return 0;
}

#ifdef CONFIG_VBUS_CTRL_DEF_ENABLE	/* 017.08.30 */
static unsigned int vbus_control_enable = 1;	/* 2017/03/23, */
#else
static unsigned int vbus_control_enable;	/* 2017/03/23, */
#endif /* CONFIG_VBUS_CTRL_DEF_ON */
module_param(vbus_control_enable, uint, 0644);
MODULE_PARM_DESC(vbus_control_enable, "ehci vbus control enable");

static int dwc3_tcc_vbus_ctrl(struct dwc3_tcc *tcc, int on_off)
{
	int err = 0;
	struct usb_phy *phy = tcc->dwc3_phy;

	if (!vbus_control_enable) {
		pr_info("[INFO][USB] dwc3 vbus ctrl disable.\n");
		return -1;
	}

	if (tcc->usb3en_ctrl_able == 1) {
		// host enable
		err = gpio_direction_output(tcc->host_en_gpio, 1);
		if (err) {
			pr_info("[INFO][USB] dwc3 tcc: can't enable host\n");
			return err;
		}
	}

	if (!phy) {
		pr_info("[INFO][USB] [%s:%d]PHY driver is needed\n", __func__,
		       __LINE__);
		return -1;

	}

	tcc->vbus_status = on_off;

	return phy->set_vbus(phy, on_off);
}

static int dwc3_tcc_remove_child(struct device *dev, void *unused)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

#if 0
static int dwc3_tcc_register_phys(struct dwc3_tcc *tcc)
{
	struct usb_phy_generic_platform_data pdata;
	struct platform_device *pdev;
	int ret;

	memset(&pdata, 0x00, sizeof(pdata));

	pdev = platform_device_alloc("usb_phy_generic", PLATFORM_DEVID_AUTO);
	if (!pdev)
		return -ENOMEM;

	tcc->usb2_phy = pdev;
	pdata.type = USB_PHY_TYPE_USB2;
	pdata.gpio_reset = -1;

	ret = platform_device_add_data(tcc->usb2_phy, &pdata, sizeof(pdata));
	if (ret)
		goto err1;

	pdev = platform_device_alloc("usb_phy_generic", PLATFORM_DEVID_AUTO);
	if (!pdev) {
		ret = -ENOMEM;
		goto err1;
	}

	tcc->usb3_phy = pdev;
	pdata.type = USB_PHY_TYPE_USB3;

	ret = platform_device_add_data(tcc->usb3_phy, &pdata, sizeof(pdata));
	if (ret)
		goto err2;

	ret = platform_device_add(tcc->usb2_phy);
	if (ret)
		goto err2;

	ret = platform_device_add(tcc->usb3_phy);
	if (ret)
		goto err3;

	return 0;

err3:
	platform_device_del(tcc->usb2_phy);

err2:
	platform_device_put(tcc->usb3_phy);

err1:
	platform_device_put(tcc->usb2_phy);

	return ret;
}
#endif

static int dwc3_tcc_register_our_phy(struct dwc3_tcc *tcc)
{
	int ret;

	if (of_find_property(tcc->dev->of_node, "telechips,dwc3_phy", 0)) {
		tcc->dwc3_phy =
		    devm_usb_get_phy_by_phandle(tcc->dev, "telechips,dwc3_phy",
						0);
		if (IS_ERR(tcc->dwc3_phy)) {
			tcc->dwc3_phy = NULL;
			pr_info("[INFO][USB] [%s:%d]PHY driver is needed\n",
			       __func__, __LINE__);
			return -1;
		}
	}
#if defined(CONFIG_ARCH_TCC803X) || \
	defined(CONFIG_ARCH_TCC805X)
	ret = tcc->dwc3_phy->set_vbus_resource(tcc->dwc3_phy);
	if (ret != 0) {
		return ret;
	}
#endif

	return tcc->dwc3_phy->init(tcc->dwc3_phy);
}

static void dwc3_tcc_unregister_our_phy(struct dwc3_tcc *tcc)
{
	usb_phy_shutdown(tcc->dwc3_phy);
	usb_phy_set_suspend(tcc->dwc3_phy, 1);
}

#define TCC_MAYBE_NOT_USE
#ifdef TCC_MAYBE_NOT_USE
static int dwc3_tcc_new_probe(struct platform_device *pdev)
{
	struct dwc3_tcc *tcc;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;

	int ret;
#if defined(DWC3_SQ_TEST_MODE)
	struct resource *res;
#endif
	tcc = devm_kzalloc(dev, sizeof(*tcc), GFP_KERNEL);
	if (!tcc)
		return -ENOMEM;

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	platform_set_drvdata(pdev, tcc);

	tcc->dev = dev;

#if 0//defined(DWC3_SQ_TEST_MODE)
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"[ERROR][USB] missing phy memory resource\n");
		return -1;
	}
	res->end += 1;
	tcc->regs = devm_ioremap(&pdev->dev, res->start, res->end - res->start);
	//tcc->regs = 0xf2900000;
#endif

	tcc->hclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(tcc->hclk)) {
		dev_err(dev, "[ERROR][USB] couldn't get h_clock\n");
		return -EINVAL;
	}
	ret = clk_prepare_enable(tcc->hclk);
	if (ret)
		return ret;
#if 0
	tcc->phy_clk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(tcc->phy_clk)) {
		dev_err(dev, "[ERROR][USB] couldn't get phy_clock\n");
		return -EINVAL;
	}
	ret = clk_prepare_enable(tcc->phy_clk);
	if (ret)
		return ret;
#endif
	//===============================================
	// Check Host enable pin
	//===============================================
	if (of_find_property(pdev->dev.of_node, "usb3en-ctrl-able", 0)) {
		tcc->usb3en_ctrl_able = 1;
		tcc->host_en_gpio = of_get_named_gpio(pdev->dev.of_node,
						      "usb3en-gpio", 0);
		if (!gpio_is_valid(tcc->host_en_gpio)) {
			dev_err(&pdev->dev,
				"[ERROR][USB] can't find dev of node: host en gpio\n");
			ret = -ENODEV;
			goto gpios_err;
		}

		ret = gpio_request(tcc->host_en_gpio, "xhci_en_gpio");
		if (ret) {
			dev_err(&pdev->dev,
				"[ERROR][USB] can't requeest xhci_en_gpio\n");
			goto gpios_err;
		}
	} else {
		tcc->usb3en_ctrl_able = 0;
	}

	//===============================================
	// Check Host enable pin
	//===============================================
	if (of_find_property(pdev->dev.of_node, "vbus-ctrl-able", 0)) {
		tcc->vbus_ctrl_able = 1;
#if 0
		tcc->vbus_gpio = of_get_named_gpio(pdev->dev.of_node,
						"vbus-gpio", 0);
		if (!gpio_is_valid(tcc->vbus_gpio)) {
			dev_err(&pdev->dev,
				"[ERROR][USB] can't find dev of node: vbus gpio\n");

			ret = -ENODEV;
			goto gpios_err;
		}

		ret = gpio_request(tcc->vbus_gpio, "usb30_vbus_gpio");
		if (ret) {
			dev_err(&pdev->dev,
				"[ERROR][USB] can't requeest vbus gpio\n");
			goto gpios_err;
		}
#endif
	} else {
		tcc->vbus_ctrl_able = 0;
	}

	//===============================================
	// Check VBUS Source enable
	//===============================================
	if (of_find_property(pdev->dev.of_node, "vbus-source-ctrl", 0)) {
		tcc->vbus_source_ctrl = 1;
		tcc->vbus_source = regulator_get(&pdev->dev, "vdd_dwc");
		if (IS_ERR(tcc->vbus_source)) {
			dev_err(&pdev->dev,
				"[ERROR][USB] regulator get fail!!!\n");
			tcc->vbus_source = NULL;
		}
	} else {
		tcc->vbus_source_ctrl = 0;
	}
#if 0
	dwc3_tcc_power_ctrl(tcc, ON);

	if (!strncmp("on", vbus, 2))
		dwc3_tcc_vbus_ctrl(tcc, ON);
	else if (!strncmp("off", vbus, 3))
		dwc3_tcc_vbus_ctrl(tcc, OFF);

	ret = dwc3_tcc_register_phys(tcc);
	if (ret) {
		dev_err(dev, "[ERROR][USB] couldn't register PHYs\n");
		goto phys_err;
	}
#endif

	ret = dwc3_tcc_register_our_phy(tcc);
	if (ret) {
		dev_err(dev, "[ERROR][USB] couldn't register out PHY\n");
		goto populate_err;
	}
#if defined(CONFIG_DWC3_DUAL_FIRST_HOST) || defined(CONFIG_USB_DWC3_HOST)
#ifndef CONFIG_TCC_EH_ELECT_TST
	dwc3_tcc_vbus_ctrl(tcc, ON);
#endif
#endif

	if (node) {
		ret = of_platform_populate(node, NULL, NULL, dev);
		if (ret) {
			dev_err(dev, "[ERROR][USB] failed to add dwc3 core\n");
			goto populate_err;
		}
	} else {
		dev_err(dev,
			"[ERROR][USB] no device node, failed to add dwc3 core\n");
		ret = -ENODEV;
		goto populate_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_vbus);

	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create vbus\n");
		goto err1;
	}
#if defined(DWC3_SQ_TEST_MODE)
	ret = device_create_file(&pdev->dev, &dev_attr_phydump);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_ssc_ovrd_in);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_ssc_asic_in);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_rx_boost);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_rx_eq);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_tx_dm_sel);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_tx_dm_3p5db);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_tx_dm_6db);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_tx_swing);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_tx_vboost);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create phydump\n");
		goto err1;
	}
#endif
	ret = device_create_file(&pdev->dev, &dev_attr_dwc3_eyep);
	if (ret) {
		dev_err(&pdev->dev,
			"[ERROR][USB] failed to create dwc3_eyep\n");
		goto err1;
	}

	return 0;
err1:
populate_err:
	platform_device_unregister(tcc->usb2_phy);
	platform_device_unregister(tcc->usb3_phy);
	//dwc3_tcc_vbus_ctrl(tcc, OFF);
	dwc3_tcc_power_ctrl(tcc, OFF);
gpios_err:
	clk_disable_unprepare(tcc->hclk);
	return ret;
}
#endif

#ifdef TCC_MAYBE_NOT_USE
static int dwc3_tcc_new_remove(struct platform_device *pdev)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(pdev);

	of_platform_depopulate(tcc->dev);
	device_remove_file(&pdev->dev, &dev_attr_dwc3_eyep);
#if defined(DWC3_SQ_TEST_MODE)
	device_remove_file(&pdev->dev, &dev_attr_tx_vboost);
	device_remove_file(&pdev->dev, &dev_attr_tx_swing);
	device_remove_file(&pdev->dev, &dev_attr_tx_dm_6db);
	device_remove_file(&pdev->dev, &dev_attr_tx_dm_3p5db);
	device_remove_file(&pdev->dev, &dev_attr_tx_dm_sel);
	device_remove_file(&pdev->dev, &dev_attr_rx_eq);
	device_remove_file(&pdev->dev, &dev_attr_rx_boost);
	device_remove_file(&pdev->dev, &dev_attr_ssc_asic_in);
	device_remove_file(&pdev->dev, &dev_attr_ssc_ovrd_in);
	device_remove_file(&pdev->dev, &dev_attr_phydump);
#endif
	device_remove_file(&pdev->dev, &dev_attr_vbus);

	device_for_each_child(&pdev->dev, NULL, dwc3_tcc_remove_child);

	dwc3_tcc_unregister_our_phy(tcc);
	platform_device_unregister(tcc->usb2_phy);
	platform_device_unregister(tcc->usb3_phy);

	clk_disable_unprepare(tcc->hclk);
	dwc3_tcc_vbus_ctrl(tcc, OFF);
	dwc3_tcc_power_ctrl(tcc, OFF);
	return 0;
}
#endif

#if 0
static int dwc3_tcc_remove(struct platform_device *pdev)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(pdev);
	struct dwc3_tcc_data *pdata = pdev->dev.platform_data;

	clk_disable_unprepare(tcc->phy_clk);

	clk_disable_unprepare(tcc->hclk);

	device_remove_file(&pdev->dev, &dev_attr_vbus);
#if defined(DWC3_SQ_TEST_MODE)
	device_remove_file(&pdev->dev, &dev_attr_phydump);
#endif
	dwc3_tcc_free_dt(tcc);

	platform_device_unregister(tcc->dwc3);

	if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, pdata->phy_type);

	platform_set_drvdata(pdev, NULL);

	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int dwc3_tcc_suspend(struct device *dev)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));

	// usb phy power down

	// vbus current status backup
	tcc->vbus_pm_status = tcc->vbus_status;

	dwc3_tcc_vbus_ctrl(tcc, OFF);
	dwc3_tcc_power_ctrl(tcc, OFF);

	if (tcc->hclk)
		clk_disable_unprepare(tcc->hclk);

	return 0;
}

static int dwc3_tcc_resume(struct device *dev)
{
	struct dwc3_tcc *tcc = platform_get_drvdata(to_platform_device(dev));

	if (tcc->hclk)
		clk_prepare_enable(tcc->hclk);

	dwc3_tcc_power_ctrl(tcc, ON);
	dwc3_tcc_vbus_ctrl(tcc, tcc->vbus_pm_status);

	return 0;
}

static const struct dev_pm_ops dwc3_tcc_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_tcc_suspend, dwc3_tcc_resume)
};

#define DEV_PM_OPS  (&dwc3_tcc_dev_pm_ops)
#else
#define DEV_PM_OPS  NULL
#endif
#ifdef CONFIG_OF
static const struct of_device_id dwc3_tcc_match[] = {
	{.compatible = "telechips,tcc-dwc3"},
	{},
};

MODULE_DEVICE_TABLE(of, dwc3_tcc_match);

#if 0
static int dwc3_tcc_parse_dt(struct platform_device *pdev, struct dwc3_tcc *tcc)
{
	int err = 0;
	struct resource *res;

#if defined(DWC3_SQ_TEST_MODE)
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"[ERROR][USB] missing phy memory resource\n");
		return -1;
	}
	res->end += 1;
	tcc->regs = devm_ioremap(&pdev->dev, res->start, res->end - res->start);
	//tcc->regs = 0xf2900000;
#endif
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev,
			"[ERROR][USB] missing phy memory resource\n");
		return -1;
	}
	res->end += 1;
	tcc->phy_regs =
	    devm_ioremap(&pdev->dev, res->start, res->end - res->start);

	//===============================================
	// Check Host enable pin
	//===============================================
	if (of_find_property(pdev->dev.of_node, "usb3en-ctrl-able", 0)) {
		tcc->usb3en_ctrl_able = 1;
		tcc->host_en_gpio = of_get_named_gpio(pdev->dev.of_node,
						      "usb3en-gpio", 0);
		if (!gpio_is_valid(tcc->host_en_gpio)) {
			dev_err(&pdev->dev,
				"[ERROR][USB] can't find dev of node: host en gpio\n");
			return -ENODEV;
		}

		err = gpio_request(tcc->host_en_gpio, "xhci_en_gpio");
		if (err) {
			dev_err(&pdev->dev,
				"[ERROR][USB] can't requeest xhci_en_gpio\n");
			return err;
		}
	} else {
		tcc->usb3en_ctrl_able = 0;
	}

	//===============================================
	// Check Host enable pin
	//===============================================
	if (of_find_property(pdev->dev.of_node, "vbus-ctrl-able", 0)) {
		tcc->vbus_ctrl_able = 1;
#if 0
		tcc->vbus_gpio = of_get_named_gpio(pdev->dev.of_node,
						"vbus-gpio", 0);
		if (!gpio_is_valid(tcc->vbus_gpio)) {
			dev_err(&pdev->dev,
				"[ERROR][USB] can't find dev of node: vbus gpio\n");

			return -ENODEV;
		}

		err = gpio_request(tcc->vbus_gpio, "usb30_vbus_gpio");
		if (err) {
			dev_err(&pdev->dev,
				"[ERROR][USB] can't requeest vbus gpio\n");
			return err;
		}
#endif
	} else {
		tcc->vbus_ctrl_able = 0;
	}

	//===============================================
	// Check VBUS Source enable
	//===============================================
	if (of_find_property(pdev->dev.of_node, "vbus-source-ctrl", 0)) {
		tcc->vbus_source_ctrl = 1;
		tcc->vbus_source = regulator_get(&pdev->dev, "vdd_dwc");
		if (IS_ERR(tcc->vbus_source)) {
			dev_err(&pdev->dev,
				"[ERROR][USB] regulator get fail!!!\n");
			tcc->vbus_source = NULL;
		}
	} else {
		tcc->vbus_source_ctrl = 0;
	}

	tcc->hclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(tcc->hclk))
		tcc->hclk = NULL;

	tcc->phy_clk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(tcc->phy_clk))
		tcc->phy_clk = NULL;

	return 0;
}

static void dwc3_tcc_free_dt(struct dwc3_tcc *tcc)
{
	if (tcc->vbus_ctrl_able == 1) {
		gpio_free(tcc->host_en_gpio);
		//gpio_free(tcc->vbus_gpio);
		if (tcc->vbus_source)
			regulator_put(tcc->vbus_source);
	}
}
#endif
#endif

static struct platform_driver dwc3_tcc_driver = {
	.probe = dwc3_tcc_new_probe,
	.remove = dwc3_tcc_new_remove,
	.driver = {
		   .name = "tcc-dwc3",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(dwc3_tcc_match),
#endif
		   .pm = DEV_PM_OPS,
		   },
};

module_platform_driver(dwc3_tcc_driver);

MODULE_ALIAS("platform:tcc-dwc3");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DesignWare USB3 TCC Glue Layer");
