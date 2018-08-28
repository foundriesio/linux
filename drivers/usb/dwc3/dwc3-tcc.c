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
#include "core.h"
#include "io.h"
//#include <asm/system_info.h>

typedef struct _USBPHYCFG
{
	volatile unsigned int       U30_CLKMASK;			// 0x0
	volatile unsigned int       U30_SWRESETN;			// 0x4
	volatile unsigned int       U30_PWRCTRL;			// 0x8
	volatile unsigned int       U30_OVERCRNT;			// 0xC
    volatile unsigned int       U30_PCFG0;              // 0x10  R/W    USB PHY Configuration Register0
    volatile unsigned int       U30_PCFG1;              // 0x14  R/W    USB PHY Configuration Register1
    volatile unsigned int       U30_PCFG2;              // 0x18  R/W    USB PHY Configuration Register2
    volatile unsigned int       U30_PCFG3;              // 0x1c  R/W    USB PHY Configuration Register3
    volatile unsigned int       U30_PCFG4;              // 0x20  R/W    USB PHY Configuration Register4
	volatile unsigned int 		U30_PFLT;				// 0x24  R/W 	 USB PHY Filter Configuration Register
	volatile unsigned int 		U30_PINT;				// 0x28  R/W 	 USB PHY Interrupt Register
	volatile unsigned int       U30_LCFG;               // 0x2C  R/W    USB 3.0 LINK Controller Configuration Register Set 0 
	volatile unsigned int 		U30_PCR0;				//   0x30  USB 3.0 PHY Parameter Control Register Set 0 
	volatile unsigned int   	U30_PCR1;				//   0x34  USB 3.0 PHY Parameter Control Register Set 1 
	volatile unsigned int		U30_PCR2;				//   0x38  USB 3.0 PHY Parameter Control Register Set 2 
	volatile unsigned int 		U30_SWUTMI;				//   0x3C  USB 3.0 UTMI Software Control Register
} USBPHYCFG, *PUSBPHYCFG;

#define PMU_ISOL		0x09C
#define U30_PHY_ISOL	(1<<6)
#ifdef CONFIG_USB_DWC3_HOST        /* 016.03.30 */
static char *vbus = "on";
#else
static char *vbus = "off";
#endif /* CONFIG_USB_DWC3_HOST */
module_param(vbus, charp, 0);
MODULE_PARM_DESC(vbus, "vbus: on, off");

//static char *maximum_speed = "high";
static char *maximum_speed = "super";
module_param(maximum_speed, charp, 0);
MODULE_PARM_DESC(maximum_speed, "Maximum supported speed.");

struct dwc3_tcc {
	struct platform_device	*dwc3;
	struct platform_device	*usb2_phy;
	struct platform_device	*usb3_phy;
	struct device		*dev;
	struct clk		*hclk;
	struct clk		*phy_clk;

	int	usb3en_ctrl_able;
	int host_en_gpio;

	int vbus_ctrl_able;
	int vbus_gpio;

	int vbus_source_ctrl;
	struct regulator *vbus_source;

	int vbus_status;
	int vbus_pm_status;

	void __iomem		*phy_regs;		/* device memory/io */
	void __iomem		*regs;		/* device memory/io */
};

#define TXVRT_SHIFT 6
#define TXVRT_MASK (0xF << TXVRT_SHIFT)
#define TXRISE_SHIFT 10
#define TXRISE_MASK (0x3 << TXRISE_SHIFT)


//#define DWC3_SQ_TEST_MODE

#if defined(DWC3_SQ_TEST_MODE)
#define TX_DEEMPH_SHIFT 		21
#define TX_DEEMPH_MASK  		0x7E00000

#define TX_DEEMPH_6DB_SHIFT 	15
#define TX_DEEMPH_6DB_MASK  	0x1F8000

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

static int dm = 0x15; // default value
//static int dm_device = 0x23;	// device mode
static int dm_host = 0x18;	// host mode
static int dm_6db= 0x20;
static int sw = 0x7F;
static int sel_dm = 0x1;
static int rx_eq = 5; //0xFF;
static int rx_boost = 5; //0xFF;
static int ssc = 0x3200;

module_param(dm, int, S_IRUGO);
MODULE_PARM_DESC (dm, "Tx De-Emphasis at 3.5 dB"); // max: 0x3F

module_param(dm_6db, int, S_IRUGO);
MODULE_PARM_DESC (dm_6db, "Tx De-Emphasis at 6 dB"); // max: 0x3F

module_param(sw, int, S_IRUGO);
MODULE_PARM_DESC (sw, "Tx Amplitude (Full Swing Mode)");

module_param(sel_dm, int, S_IRUGO);
MODULE_PARM_DESC (sel_dm, "Tx De-Emphasis set");

module_param(rx_eq, int, S_IRUGO);
MODULE_PARM_DESC (rx_eq, "Rx Equalization");

module_param(rx_boost, int, S_IRUGO);
MODULE_PARM_DESC (rx_boost, "Rx Boost");
#endif

static int dwc3_tcc_parse_dt(struct platform_device *pdev, struct dwc3_tcc *tcc);
int dwc3_tcc_vbus_ctrl(struct dwc3_tcc *tcc, int on_off);
void dwc3_tcc_free_dt( struct dwc3_tcc *tcc);

void dwc3_bit_set(void __iomem *base, u32 offset, u32 value)
{
	unsigned int uTmp;

	uTmp = dwc3_readl(base, offset);
	dwc3_writel(base, offset, (uTmp|value));
}

void dwc3_bit_clear(void __iomem *base, u32 offset, u32 value)
{
	unsigned int uTmp;

	uTmp = dwc3_readl(base, offset);
	dwc3_writel(base, offset, (uTmp&(~value)));
}

static ssize_t dwc3_tcc_vbus_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc *tcc =	platform_get_drvdata(to_platform_device(dev->parent));

	return sprintf(buf, "dwc3 vbus - %s\n",(tcc->vbus_status) ? "on":"off");
}

static ssize_t dwc3_tcc_vbus_store(struct device *dev, struct device_attribute *attr,  
	const char *buf, size_t count)
{
	struct dwc3_tcc *tcc =	platform_get_drvdata(to_platform_device(dev->parent));

	if (!strncmp(buf, "on", 2)) {
		dwc3_tcc_vbus_ctrl(tcc, ON);
	}

	if (!strncmp(buf, "off", 3)) {
		dwc3_tcc_vbus_ctrl(tcc, OFF);
	}

	return count;
}

static DEVICE_ATTR(vbus, S_IRUGO | S_IWUSR, dwc3_tcc_vbus_show, dwc3_tcc_vbus_store);

#ifdef DWC3_SQ_TEST_MODE		/* 016.08.26 */
unsigned int dwc3_tcc_read_u30phy_reg(struct dwc3_tcc *tcc, unsigned int address)
{
    PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;
    unsigned int tmp;
    unsigned int read_data;

    // address capture phase
    USBPHYCFG->U30_PCR0 = address;// write addr
    USBPHYCFG->U30_PCR2 |= 0x1; // capture addr

    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==1) break; // check ack == 1
    }

    USBPHYCFG->U30_PCR2 &= ~0x1; // clear capture addr

    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==0) break; // check ack == 0
    }

    // read phase
    USBPHYCFG->U30_PCR2 |= 0x1<<8; // set read
    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==1) break; // check ack == 1
    }

    read_data = USBPHYCFG->U30_PCR1;    // read data

    USBPHYCFG->U30_PCR2 &= ~(0x1<<8); // clear read
    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==0) break; // check ack == 0
    }

    return(read_data);
}

void dwc3_tcc_read_u30phy_reg_all (struct dwc3_tcc	*tcc) {
	unsigned int read_data;
	unsigned int i;

	for (i=0;i<0x37;i++) {
		read_data = dwc3_tcc_read_u30phy_reg(tcc,i);
		printk("addr:0x%08X value:0x%08X\n",i,read_data);
	}
	
	for (i=0x1000;i<0x1030;i++) {
		read_data = dwc3_tcc_read_u30phy_reg(tcc,i);
		printk("addr:0x%08X value:0x%08X\n",i,read_data);
	}
}

unsigned int dwc3_tcc_write_u30phy_reg(struct dwc3_tcc	*tcc, unsigned int address, 
	unsigned int write_data)
{
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;
	unsigned int tmp;
	unsigned int read_data;

	// address capture phase
	USBPHYCFG->U30_PCR0 = address; // write addr
	USBPHYCFG->U30_PCR2 |= 0x1; // capture addr
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	USBPHYCFG->U30_PCR2 &= ~(0x1); // clear capture addr
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}

	// write phase
	USBPHYCFG->U30_PCR0 = write_data; // write data
	USBPHYCFG->U30_PCR2 |= 0x1<<4; // set capture data
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	USBPHYCFG->U30_PCR2 &= ~(0x1<<4); // clear capture data
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}
	USBPHYCFG->U30_PCR2 |= 0x1<<12; // set write
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	USBPHYCFG->U30_PCR2 &= ~(0x1<<12); // clear write
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}

	// read phase
	USBPHYCFG->U30_PCR2 |= 0x1<<8; // set read
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	read_data = USBPHYCFG->U30_PCR1; // read data
	USBPHYCFG->U30_PCR2 &= ~(0x1<<8); // clear read
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}
	if (read_data==write_data) return(1); // success
	else return(0); // fail
}

static ssize_t dwc3_tcc_phy_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));

	dwc3_tcc_read_u30phy_reg_all(tcc);

	return 0;
}

static ssize_t dwc3_tcc_phy_store(struct device *dev, struct device_attribute *attr, 
	const char *buf, size_t count)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));

	dwc3_tcc_read_u30phy_reg_all(tcc);

	return count;
}

static DEVICE_ATTR(phydump, S_IRUGO | S_IWUSR, dwc3_tcc_phy_show, dwc3_tcc_phy_store);

static ssize_t dwc3_tcc_ssc_ovrd_in_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	unsigned int read_data = dwc3_tcc_read_u30phy_reg(tcc, 0x13);

	return sprintf(buf, "ssc_ovrd_in:0x%08x\n",read_data);
}

static ssize_t dwc3_tcc_ssc_ovrd_in_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	unsigned int uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x13);
	uint32_t val = simple_strtoul(buf, NULL, 16);

	BITCSET(uTmp, 0xFF, val);

	dwc3_tcc_write_u30phy_reg(tcc, 0x13, uTmp);

	return count;
}

static DEVICE_ATTR(ssc_ovrd_in, 0644, dwc3_tcc_ssc_ovrd_in_show, dwc3_tcc_ssc_ovrd_in_store);

static ssize_t dwc3_tcc_ssc_asic_in_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	unsigned int read_data = dwc3_tcc_read_u30phy_reg(tcc, 0x16);

	return sprintf(buf, "ssc_ovrd_in:0x%08x\n",read_data);
}

static ssize_t dwc3_tcc_ssc_asic_in_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	unsigned int uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x16);
	uint32_t val = simple_strtoul(buf, NULL, 16);

	BITCSET(uTmp, 0xFF, val);

	dwc3_tcc_write_u30phy_reg(tcc, 0x16, uTmp);

	return count;
}

static DEVICE_ATTR(ssc_asic_in, 0644, dwc3_tcc_ssc_asic_in_show, dwc3_tcc_ssc_asic_in_store);

static ssize_t dwc3_tcc_rxboost_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	unsigned int read_data = dwc3_tcc_read_u30phy_reg(tcc, 0x1024);

	return sprintf(buf, "dwc3 tcc: PHY cfg - RX BOOST: 0x%x\n", ((read_data & RX_BOOST_MASK) >> RX_BOOST_SHIFT));
}

static ssize_t dwc3_tcc_rxboost_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	unsigned int uTmp, tmp;
	uint32_t val = simple_strtoul(buf, NULL, 16);

	uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x1024);	// get the rx_boost value
	tmp = uTmp;

	BITCSET(uTmp, RX_BOOST_MASK, val << RX_BOOST_SHIFT);    // set rx_boost value
	uTmp |= Hw5;

	dwc3_tcc_write_u30phy_reg(tcc, 0x1024, uTmp);

	uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x1024);

	printk("dwc3 tcc: PHY cfg - RX BOOST: 0x%x -> [0x%x]\n"
		, ((tmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT)
		, ((uTmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT));

	return count;
}

static DEVICE_ATTR(rx_boost, 0644, dwc3_tcc_rxboost_show, dwc3_tcc_rxboost_store);

static ssize_t dwc3_tcc_rx_eq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	unsigned int read_data = dwc3_tcc_read_u30phy_reg(tcc, 0x1006);

	return sprintf(buf, "dwc3 tcc: PHY cfg - RX EQ: 0x%x\n", ((read_data & RX_EQ_MASK ) >> RX_EQ_SHIFT));
}

static ssize_t dwc3_tcc_rx_eq_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	unsigned int uTmp, tmp;
	uint32_t val = simple_strtoul(buf, NULL, 16);

	uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x1006);
	tmp = uTmp;

	BITCSET(uTmp, RX_EQ_MASK, val << RX_EQ_SHIFT);
	uTmp |= Hw11;

	dwc3_tcc_write_u30phy_reg(tcc, 0x1006, uTmp);

	uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x1006);

	printk("dwc3 tcc: PHY cfg - RX EQ: 0x%x -> [0x%x]\n"
		, ((tmp & RX_EQ_MASK) >> RX_EQ_SHIFT)
		, ((uTmp & RX_EQ_MASK) >> RX_EQ_SHIFT));

	return count;
}

static DEVICE_ATTR(rx_eq, 0644, dwc3_tcc_rx_eq_show, dwc3_tcc_rx_eq_store);

static ssize_t dwc3_tcc_tx_dm_sel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(sel_dm == 0)
		return sprintf(buf,"dwc3 tcc: PHY cfg - 6dB de-emphasis\n");
	else if (sel_dm == 1)
		return sprintf(buf,"dwc3 tcc: PHY cfg - 3.5dB de-emphasis (default)\n");
	else if (sel_dm == 2)
		return sprintf(buf,"dwc3 tcc: PHY cfg - de-emphasis\n");
	else
		return sprintf(buf,"dwc3 tcc: err! invalid value (0: -6dB 1: -3.5dB 2: de-emphasis)\n");
}

static ssize_t dwc3_tcc_tx_dm_sel_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	uint32_t val = simple_strtoul(buf, NULL, 16);

	if(val < 0 || val > 2)
	{
		printk("dwc3 tcc: err! invalid value (0: -6dB 1: -3.5dB 2: de-emphasis)\n");
		return count;
	}

	if(val == 0)
		printk("dwc3 tcc: PHY cfg - 6dB de-emphasis\n");
	else if (val == 1)
		printk("dwc3 tcc: PHY cfg - 3.5dB de-emphasis (default)\n");
	else if (val == 2)
		printk("dwc3 tcc: PHY cfg - de-emphasis\n");

	sel_dm = val;

	dwc3_bit_clear(tcc->regs, DWC3_GUSB3PIPECTL(0), TX_DEEMPH_SET_MASK);
	dwc3_bit_set(tcc->regs, DWC3_GUSB3PIPECTL(0), sel_dm << TX_DEEMPH_SET_SHIFT);

	return count;
}

static DEVICE_ATTR(tx_dm_sel, 0644, dwc3_tcc_tx_dm_sel_show, dwc3_tcc_tx_dm_sel_store);

static ssize_t dwc3_tcc_tx_dm_3p5db_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;

	return sprintf(buf,"dwc3 tcc: TX_DEEMPH 3.5dB : 0x%x\n", (USBPHYCFG->U30_PCFG3 & TX_DEEMPH_MASK) >> TX_DEEMPH_SHIFT);
}

static ssize_t dwc3_tcc_tx_dm_3p5db_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;
	uint32_t val = simple_strtoul(buf, NULL, 16);
	uint32_t tmp = USBPHYCFG->U30_PCFG3;

	dm_host = val;
	BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_MASK, dm_host<< TX_DEEMPH_SHIFT);
	printk("dwc3 tcc: TX_DEEMPH 3.5dB : 0x%x -> [0x%x]\n"
		,(tmp & TX_DEEMPH_MASK) >> TX_DEEMPH_SHIFT
		,(USBPHYCFG->U30_PCFG3 & TX_DEEMPH_MASK) >> TX_DEEMPH_SHIFT);

	return count;
}

static DEVICE_ATTR(tx_dm_3p5db, 0644, dwc3_tcc_tx_dm_3p5db_show, dwc3_tcc_tx_dm_3p5db_store);

static ssize_t dwc3_tcc_tx_dm_6db_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;

	return sprintf(buf,"dwc3 tcc: TX_DEEMPH 6dB : 0x%x\n", (USBPHYCFG->U30_PCFG3 & TX_DEEMPH_6DB_MASK) >> TX_DEEMPH_6DB_SHIFT);
}

static ssize_t dwc3_tcc_tx_dm_6db_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;
	uint32_t val = simple_strtoul(buf, NULL, 16);
	uint32_t tmp = USBPHYCFG->U30_PCFG3;

	dm_6db = val;
	BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_6DB_MASK, dm_6db<< TX_DEEMPH_6DB_SHIFT);
	printk("dwc3 tcc: TX_DEEMPH 6dB : 0x%x -> [0x%x]\n"
		,(tmp & TX_DEEMPH_6DB_MASK) >> TX_DEEMPH_6DB_SHIFT
		,(USBPHYCFG->U30_PCFG3 & TX_DEEMPH_6DB_MASK) >> TX_DEEMPH_6DB_SHIFT);

	return count;
}

static DEVICE_ATTR(tx_dm_6db, 0644, dwc3_tcc_tx_dm_6db_show, dwc3_tcc_tx_dm_6db_store);

static ssize_t dwc3_tcc_tx_swing_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;

	return sprintf(buf,"dwc3 tcc: TX_SWING : 0x%x\n", (USBPHYCFG->U30_PCFG3 & TX_SWING_MASK) >> TX_SWING_SHIFT);
}

static ssize_t dwc3_tcc_tx_swing_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;
	uint32_t val = simple_strtoul(buf, NULL, 16);
	uint32_t tmp = USBPHYCFG->U30_PCFG3;

	sw = val;
	BITCSET(USBPHYCFG->U30_PCFG3, TX_SWING_MASK, sw << TX_SWING_SHIFT);
	printk("dwc3 tcc: TX_SWING : 0x%x -> [0x%x]\n"
		,(tmp & TX_SWING_MASK) >> TX_SWING_SHIFT
		,(USBPHYCFG->U30_PCFG3 & TX_SWING_MASK) >> TX_SWING_SHIFT);

	return count;
}

static DEVICE_ATTR(tx_swing, 0644, dwc3_tcc_tx_swing_show, dwc3_tcc_tx_swing_store);

static ssize_t dwc3_tcc_tx_vboost_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;

	return sprintf(buf,"dwc3 tcc: TX_VBOOST : 0x%x\n", (USBPHYCFG->U30_PCFG2 & TX_VBOOST_LVL_MASK) >> TX_VBOOST_LVL_SHIFT);
}

static ssize_t dwc3_tcc_tx_vboost_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;
	uint32_t val = simple_strtoul(buf, NULL, 16);
	uint32_t tmp = USBPHYCFG->U30_PCFG2;

	BITCSET(USBPHYCFG->U30_PCFG2, TX_VBOOST_LVL_MASK, val << TX_VBOOST_LVL_SHIFT);
	printk("dwc3 tcc: TX_VBOOST : 0x%x -> [0x%x]\n"
		,(tmp & TX_VBOOST_LVL_MASK) >> TX_VBOOST_LVL_SHIFT
		,(USBPHYCFG->U30_PCFG2 & TX_VBOOST_LVL_MASK) >> TX_VBOOST_LVL_SHIFT);

	return count;
}

static DEVICE_ATTR(tx_vboost, 0644, dwc3_tcc_tx_vboost_show, dwc3_tcc_tx_vboost_store);
#endif /* DWC3_SQ_TEST_MODE */


#if 0		/* 014.12.02 */
void dwc3_tcc_phy_software_reset (void)
{
	PUSB3_GLB	gUSB3_GLB = (PUSB3_GLB)tcc_p2v(HwUSBGLOBAL_BASE);
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc_p2v(HwUSBPHYCFG_BASE);
    // Assert reset
    //HwUSB30->sGlobal.uGCTL.bReg.CoreSoftReset = 1;
    BITSET(gUSB3_GLB->GCTL.nREG, Hw11);

    //HwUSB30->sGlobal.uGUSB2PHYCFG.bReg.PHYSoftRst = 1;
    BITSET(gUSB3_GLB->GUSB2PHYCFG.nREG, Hw31);

    //HwUSB30->sGlobal.uGUSB3PIPECTL.bReg.PHYSoftRst = 1;
    BITSET(gUSB3_GLB->GUSB3PIPECTL.nREG, Hw31);

    // turn on all PHY circuits
    //HwHSB_CFG->uUSB30PHY_CFG1.bReg.test_powerdown_hsp = 0; // turn on HS circuit
    //HwHSB_CFG->uUSB30PHY_CFG1.bReg.test_powerdown_ssp = 0; // turn on SS circuit
    BITCLR(USBPHYCFG->UPCR1,Hw9|Hw8);

    // wait T1 (>10us)
	mdelay(10);

    // De-assert reset
    //HwUSB30->sGlobal.uGUSB2PHYCFG.bReg.PHYSoftRst = 0;
    BITCLR(gUSB3_GLB->GUSB2PHYCFG.nREG, Hw31);

    //HwUSB30->sGlobal.uGUSB3PIPECTL.bReg.PHYSoftRst = 0;
    BITCLR(gUSB3_GLB->GUSB3PIPECTL.nREG, Hw31);

    // Wait T3-T1 (>165us)
	mdelay(10);

    //HwUSB30->sGlobal.uGCTL.bReg.CoreSoftReset = 0;
    BITCLR(gUSB3_GLB->GCTL.nREG, Hw11);
}
#endif /* 0 */

#define SOFFN_MASK        0x3FFF
#define SOFFN_SHIFT       3
#define GET_SOFFN_NUM(x)  ((x >> SOFFN_SHIFT)  & SOFFN_MASK)

int dwc3_tcc_get_soffn(void)
{
#if 0		/* 016.02.19 */
	int uTmp;
	uTmp = readl((const volatile void *)tcc_p2v(HwUSBDEVICE_BASE+0x070C));
	return GET_SOFFN_NUM(uTmp);
#endif /* 0 */
	return -1;
}

void dwc3_tcc898x_swreset(struct dwc3_tcc *tcc, int on_off)
{
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;

	if(on_off == ON)
		BITCLR(USBPHYCFG->U30_SWRESETN, Hw1);
	else if(on_off == OFF)
		BITSET(USBPHYCFG->U30_SWRESETN, Hw1);
	else
		printk("\x1b[1;31m[%s:%d]Wrong request!!\x1b[0m\n", __func__, __LINE__);
}

void dwc3_tcc899x_swreset(struct dwc3_tcc *tcc, int on_off)
{
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;

	if(on_off == ON)
		writel((readl(&USBPHYCFG->U30_SWRESETN) & ~Hw1), &USBPHYCFG->U30_SWRESETN);
	else if(on_off == OFF)
		writel((readl(&USBPHYCFG->U30_SWRESETN) | Hw1), &USBPHYCFG->U30_SWRESETN);
	else
		printk("\x1b[1;31m[%s:%d]Wrong request!!\x1b[0m\n", __func__, __LINE__);
}


int dwc3_tcc_phy_ctrl(struct dwc3_tcc *tcc, int on_off)
{
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)tcc->phy_regs;

	unsigned int uTmp = 0;
	int tmp_cnt;

	if(on_off== ON) {
		//======================================================
	    // 1.Power-on Reset
		//======================================================
	//	*(volatile unsigned long *)tcc_p2v(HwCLK_RESET0) &= ~Hw7;
	//	*(volatile unsigned long *)tcc_p2v(HwCLK_RESET0) |= Hw7;
#if defined(CONFIG_ARCH_TCC898X)
		dwc3_tcc898x_swreset(tcc, ON);
		mdelay(1);
		dwc3_tcc898x_swreset(tcc, OFF);
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
		//dwc3_tcc899x_swreset(tcc, ON);
		//mdelay(1);
		//dwc3_tcc899x_swreset(tcc, OFF);
		writel((readl(&USBPHYCFG->U30_LCFG) & ~Hw31), &USBPHYCFG->U30_LCFG);
		writel((readl(&USBPHYCFG->U30_PCFG0) & ~(Hw30)), &USBPHYCFG->U30_PCFG0);

		uTmp = readl(&USBPHYCFG->U30_PCFG0);
		uTmp &= ~(Hw25); // turn off SS circuits
		uTmp &= ~(Hw24); // turn on HS circuits
		writel(uTmp, &USBPHYCFG->U30_PCFG0);
		msleep(1);

		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw30)), &USBPHYCFG->U30_PCFG0);
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw31)), &USBPHYCFG->U30_LCFG);
		
		//printk("PHY = 0x%x \n", USBPHYCFG->U30_PCFG0);
		tmp_cnt = 0;
		while( tmp_cnt < 10000)
		{
			if(readl(&USBPHYCFG->U30_PCFG0) & 0x80000000)
			{
				break;
			}

			tmp_cnt++;
			udelay(5);
		}
		printk("XHCI PHY valid check %s\x1b[0m\n",tmp_cnt>=9999?"fail!":"pass.");
#endif
		//======================================================
		// Initialize all registers
		//======================================================
#if defined(CONFIG_ARCH_TCC898X)		/* 016.09.30 */
		USBPHYCFG->U30_PCFG0 	= (system_rev >= 2)?0x40306228:0x20306228;
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) 
		writel(0x03204208, &USBPHYCFG->U30_PCFG0);
#else
		USBPHYCFG->U30_PCFG0 	= 0x20306228;
#endif /* CONFIG_ARCH_TCC898X */
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
		writel(0x00000000, &USBPHYCFG->U30_PCFG1);
		writel(0x919E1A04, &USBPHYCFG->U30_PCFG2);
		writel(0x4B8E7F05, &USBPHYCFG->U30_PCFG3);
		writel(0x00200000, &USBPHYCFG->U30_PCFG4);
		writel(0x00000351, &USBPHYCFG->U30_PFLT);
		writel(0x80000000, &USBPHYCFG->U30_PINT);
		//writel(0x007E0080, &USBPHYCFG->U30_LCFG);
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw27 | Hw26 | Hw19 | Hw18 | Hw17 | Hw7)), &USBPHYCFG->U30_LCFG);
	//	writel(0x00000000, &USBPHYCFG->U30_PCR0);
	//	writel(0x00000000, &USBPHYCFG->U30_PCR1);
	//	writel(0x00000000, &USBPHYCFG->U30_PCR2);
	//	writel(0x00000000, &USBPHYCFG->U30_SWUTMI);
#else
		USBPHYCFG->U30_PCFG1 	= 0x00000300;
		USBPHYCFG->U30_PCFG2 	= 0x919f94C4;
		USBPHYCFG->U30_PCFG3 	= 0x4ab07f05;
#endif

#if defined(DWC3_SQ_TEST_MODE)		/* 016.02.22 */
		//BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_MASK, dm << TX_DEEMPH_SHIFT);
		//BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_MASK, dm_device<< TX_DEEMPH_SHIFT);
		BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_MASK, dm_host<< TX_DEEMPH_SHIFT);

		BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_6DB_MASK, dm_6db<< TX_DEEMPH_6DB_SHIFT);
		BITCSET(USBPHYCFG->U30_PCFG3, TX_SWING_MASK, sw << TX_SWING_SHIFT);
		printk("dwc3 tcc: PHY cfg - TX_DEEMPH 3.5dB: 0x%02x, TX_DEEMPH 6dB: 0x%02x, TX_SWING: 0x%02x\n", dm, dm_6db, sw);

		USBPHYCFG->U30_PCFG4 	= 0x00200000;
		USBPHYCFG->U30_PFLT  	= 0x00000351;
		USBPHYCFG->U30_PINT  	= 0x00000000;
		USBPHYCFG->U30_LCFG  	= 0x00C20018;
		USBPHYCFG->U30_PCR0  	= 0x00000000;
		USBPHYCFG->U30_PCR1  	= 0x00000000;
		USBPHYCFG->U30_PCR2  	= 0x00000000;
		USBPHYCFG->U30_SWUTMI	= 0x00000000;
#endif
		// todo ----------------------------------------------------------------------------------------------
		#if defined(CONFIG_ARCH_TCC898X)
		dwc3_tcc898x_swreset(tcc, ON);

		// Link global Reset
		USBPHYCFG->U30_LCFG &= ~(Hw31); // CoreRSTN (Cold Reset), active low
		#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
		//dwc3_tcc899x_swreset(tcc, ON);
		// Link global Reset
		writel((readl(&USBPHYCFG->U30_LCFG) & ~Hw31), &USBPHYCFG->U30_LCFG); // CoreRSTN (Cold Reset), active low
		#endif

#if defined(DWC3_SQ_TEST_MODE)		/* 016.02.22 */
		//*(volatile unsigned long *)tcc_p2v(HwU30GCTL) |= (Hw11); // Link soft reset, active high
	    //*(volatile unsigned long *)tcc_p2v(HwU30GUSB3PIPECTL) |= (Hw31); // Phy SS soft reset, active high
	    //*(volatile unsigned long *)tcc_p2v(HwU30GUSB2PHYCFG) |= (Hw31); //Phy HS soft resetactive high
		dwc3_bit_set(tcc->regs, DWC3_GCTL, DWC3_GCTL_CORESOFTRESET);
		dwc3_bit_set(tcc->regs, DWC3_GUSB3PIPECTL(0), DWC3_GUSB3PIPECTL_PHYSOFTRST);
		dwc3_bit_set(tcc->regs, DWC3_GUSB2PHYCFG(0), DWC3_GUSB2PHYCFG_PHYSOFTRST);
#endif

#if defined(CONFIG_ARCH_TCC898X)
		if (!strncmp("high", maximum_speed, 4)) {
			// USB20 Only Mode
			USBPHYCFG->U30_LCFG |= Hw28; // enable usb20mode -> removed in DWC_usb3 2.60a, but use as interrupt
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp |= Hw25; // turn off SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			USBPHYCFG->U30_PCFG0 = uTmp;
		} else if (!strncmp("super", maximum_speed, 5)) {
			// USB 3.0
			USBPHYCFG->U30_LCFG &= ~(Hw28); // disable usb20mode -> removed in DWC_usb3 2.60a, but use as interrupt
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp &= ~(Hw25); // turn on SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			USBPHYCFG->U30_PCFG0 = uTmp;	
		}

		// Turn Off OTG
	//    USBPHYCFG->U30_PCFG0 |= (Hw21);

		// Release Reset Link global
		USBPHYCFG->U30_LCFG |= (Hw31);
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
		if (!strncmp("high", maximum_speed, 4)) {
			// USB20 Only Mode
			writel((readl(&USBPHYCFG->U30_LCFG) | Hw28), &USBPHYCFG->U30_LCFG); // enable usb20mode -> removed in DWC_usb3 2.60a, but use as interrupt
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp |= Hw25; // turn off SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			writel(uTmp, &USBPHYCFG->U30_PCFG0);
		} else if (!strncmp("super", maximum_speed, 5)) {
			// USB 3.0
			writel((readl(&USBPHYCFG->U30_LCFG) & ~(Hw28)), &USBPHYCFG->U30_LCFG); // disable usb20mode -> removed in DWC_usb3 2.60a, but use as interrupt
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp &= ~(Hw25); // turn on SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			uTmp &= ~(Hw30); // release PHY reset
			writel(uTmp, &USBPHYCFG->U30_PCFG0);
		}
		mdelay(1);
		// Release Reset Link global
		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw30)), &USBPHYCFG->U30_PCFG0);
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw31)), &USBPHYCFG->U30_LCFG);
#endif

#if defined(DWC3_SQ_TEST_MODE)		/* 016.02.22 */
		//*(volatile unsigned long *)tcc_p2v(HwU30GCTL) &= ~(Hw11); //Release CoreSoftReset
	    //*(volatile unsigned long *)tcc_p2v(HwU30GUSB3PIPECTL) &= ~(Hw31); //Release Phy SS soft reset
	    //*(volatile unsigned long *)tcc_p2v(HwU30GUSB2PHYCFG) &= ~(Hw31); // Release Phy HS soft reset
		//*(volatile unsigned long *)tcc_p2v(HwPMUU30PHY) |= (Hw9); //Release Phy reset
	    //*(volatile unsigned long *)tcc_p2v(HwU30GUSB2PHYCFG) &= ~(Hw6); // Disable Suspend
		dwc3_bit_clear(tcc->regs, DWC3_GCTL, DWC3_GCTL_CORESOFTRESET);
		dwc3_bit_clear(tcc->regs, DWC3_GUSB3PIPECTL(0), DWC3_GUSB3PIPECTL_PHYSOFTRST);
		dwc3_bit_clear(tcc->regs, DWC3_GUSB2PHYCFG(0), DWC3_GUSB2PHYCFG_PHYSOFTRST);
		dwc3_tcc898x_swreset(tcc, OFF);
		dwc3_bit_clear(tcc->regs, DWC3_GUSB2PHYCFG(0), DWC3_GUSB2PHYCFG_SUSPHY);

	    //*(volatile unsigned long *)tcc_p2v(HwU30GCTL) &= ~(Hw11); // Release CoreSoftReset
	    //*(volatile unsigned long *)tcc_p2v(HwU30GSBUSCFG0) &= ~0xff;
	    //*(volatile unsigned long *)tcc_p2v(HwU30GSBUSCFG0) |= Hw1; // INCR4 Burst Enable
	    //*(volatile unsigned long *)tcc_p2v(HwU30GSBUSCFG1) |= 0xf<<8; // 16 burst request limit
		dwc3_bit_clear(tcc->regs, DWC3_GCTL, DWC3_GCTL_CORESOFTRESET);
		dwc3_bit_clear(tcc->regs, DWC3_GSBUSCFG0, 0xff);
		dwc3_bit_set(tcc->regs, DWC3_GSBUSCFG0, Hw1); // INCR4 Burst Enable
		dwc3_bit_set(tcc->regs, DWC3_GSBUSCFG1, (0xf<<8)); // 6 burst request limit

	    // GCTL
	    uTmp = dwc3_readl(tcc->regs, DWC3_GCTL);
	    uTmp &= ~(Hw7|Hw6|Hw5|Hw4|Hw3);
	    uTmp &= ~0xfff80000; // clear 31:19
	    uTmp |= 3<<19; // PwrDnScale = 46.875 / 16 = 3
	//    uTmp |= Hw10; // SOFITPSYNC enable
	    uTmp &= ~Hw10; // SOFITPSYNC disable
	    dwc3_writel(tcc->regs, DWC3_GCTL, uTmp);

	    // Set REFCLKPER
	    uTmp = dwc3_readl(tcc->regs, DWC3_GUCTL);
	    uTmp &= ~0xffc00000; // clear REFCLKPER
	    uTmp |= 41<<22; // REFCLKPER
	    uTmp |= Hw15|Hw14;
	    dwc3_writel(tcc->regs, DWC3_GUCTL, uTmp);

	    // GFLADJ
	    uTmp = dwc3_readl(tcc->regs, 0xc630/*GFLADJ*/);
	    uTmp &= ~0xff7fff80;
	    uTmp |= 10<<24;
	    uTmp |= Hw23; // GFLADJ_REFCLK_LPM_SEL
	    uTmp |= 2032<<8; // GFLADJ_REFCLK_FLADJ
	    uTmp |= Hw7; // GFLADJ_30MHZ_REG_SEL
	    dwc3_writel(tcc->regs, 0xc630/*GFLADJ*/, uTmp);

	    // GUSB2PHYCFG
	    uTmp = dwc3_readl(tcc->regs, DWC3_GUSB2PHYCFG(0));
	    uTmp |= Hw30; // U2_FREECLK_EXISTS
	    uTmp &= ~(Hw4); // UTMI+
	    uTmp &= ~(Hw3); // UTMI+ 8bit
	    uTmp |= 9<<10; // USBTrdTim = 9
	    uTmp |= Hw8; // EnblSlpM
	    dwc3_writel(tcc->regs, DWC3_GUSB2PHYCFG(0), uTmp);

#if 0
		// Set Host mode
		uTmp = *(volatile unsigned long *)tcc_p2v(HwU30GCTL);
		uTmp &= ~(Hw13|Hw12);
		uTmp |= 0x1<<12; // PrtCapDir
		*(volatile unsigned long *)tcc_p2v(HwU30GCTL) = uTmp;
#endif

		//USBPHYCFG->U30_PCFG0 |= Hw30;

	    // Set REFCLKPER
	    uTmp = dwc3_readl(tcc->regs, DWC3_GUCTL);
	    uTmp &= ~0xffc00000; // clear REFCLKPER
	    uTmp |= 41<<22; // REFCLKPER
	    uTmp &= ~(Hw15);
	    dwc3_writel(tcc->regs, DWC3_GUCTL, uTmp);

		//=====================================================================
		// Tx Deemphasis setting
		// Global USB3 PIPE Control Register (GUSB3PIPECTL0): 0x1100C2C0
		//=====================================================================
		dwc3_bit_clear(tcc->regs, DWC3_GUSB3PIPECTL(0), TX_DEEMPH_SET_MASK);
		dwc3_bit_set(tcc->regs, DWC3_GUSB3PIPECTL(0), sel_dm << TX_DEEMPH_SET_SHIFT);

		if(sel_dm == 0)
			printk("dwc3 tcc: PHY cfg - 6dB de-emphasis\n");
		else if (sel_dm == 1)
			printk("dwc3 tcc: PHY cfg - 3.5dB de-emphasis (default)\n");
		else if (sel_dm == 2)
			printk("dwc3 tcc: PHY cfg - de-emphasis\n");

		//=====================================================================
		// Rx EQ setting
		//=====================================================================
		if ( rx_eq != 0xFF)
		{
			//printk("dwc3 tcc: phy cfg - EQ Setting\n");
			//========================================
			//	Read RX_OVER_IN_HI
			//========================================
			uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x1006);
			//printk("Reset value - RX-OVRD: 0x%08X\n", uTmp);
			//printk("Reset value - RX_EQ: 0x%X\n", ((uTmp & 0x00000700 ) >> 8));
			//printk("\x1b[1;33m[%s:%d]rx_eq: %d\x1b[0m\n", __func__, __LINE__, rx_eq);
			
			BITCSET(uTmp, RX_EQ_MASK, rx_eq << RX_EQ_SHIFT);    
			uTmp |= Hw11;
			//printk("Set value - RX-OVRD: 0x%08X\n", uTmp);
			//printk("Set value - RX_EQ: 0x%X\n", ((uTmp & 0x00000700 ) >> 8));

			dwc3_tcc_write_u30phy_reg(tcc, 0x1006, uTmp);

			uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x1006);
			//printk("    Reload - RX-OVRD: 0x%08X\n", uTmp);
			//printk("    Reload - RX_EQ: 0x%X\n", ((uTmp & 0x00000700 ) >> 8));
			printk("dwc3 tcc: PHY cfg - RX EQ: 0x%x\n", ((uTmp & 0x00000700 ) >> 8));
	    }
		
		//=====================================================================
		// Rx Boost setting
		//=====================================================================
		if ( rx_boost != 0xFF)
		{
			//printk("dwc3 tcc: phy cfg - Rx Boost Setting\n");
			//========================================
			//	Read RX_OVER_IN_HI
			//========================================
			uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x1024);
			//printk("Reset value - RX-ENPWR1: 0x%08X\n", uTmp);
			//printk("Reset value - RX_BOOST: 0x%X\n", ((uTmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT));
			//printk("\x1b[1;33m[%s:%d]rx_boost: %d\x1b[0m\n", __func__, __LINE__, rx_boost);
			
			BITCSET(uTmp, RX_BOOST_MASK, rx_boost << RX_BOOST_SHIFT);    
			uTmp |= Hw5;
			//printk("Reset value - RX-ENPWR1: 0x%08X\n", uTmp);
			//printk("Reset value - RX_BOOST: 0x%X\n", ((uTmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT));

			dwc3_tcc_write_u30phy_reg(tcc, 0x1024, uTmp);

			uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x1024);
			//printk("    Reload value - RX-ENPWR1: 0x%08X\n", uTmp);
			//printk("    Reload value - RX_BOOST: 0x%X\n", ((uTmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT));
			printk("dwc3 tcc: PHY cfg - RX BOOST: 0x%x\n", ((uTmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT));
	   	}

		if ( ssc != 0xFF)
		{
			//printk("dwc3 tcc: phy cfg - TSSC Setting\n");
			//========================================
			//	Read SSC_OVER_IN
			//========================================
			uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x13);

			BITCSET(uTmp, SSC_REF_CLK_SEL_MASK, ssc);

			dwc3_tcc_write_u30phy_reg(tcc, 0x13, uTmp);

			uTmp = dwc3_tcc_read_u30phy_reg(tcc, 0x13);
			printk("dwc3 tcc: PHY cfg - SSC_REF_CLK_SEL: 0x%x\n", uTmp & SSC_REF_CLK_SEL_MASK);
		}
#else
		#if defined(CONFIG_ARCH_TCC898X)
		dwc3_tcc898x_swreset(tcc, OFF);
		#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
		//dwc3_tcc899x_swreset(tcc, OFF);
		//msleep(10);
		#endif
#endif /* 0 */
		
		mdelay(10);

	    //while(1) {
		   // uTmp = USBPHYCFG->U30_PCFG0;
		   // uTmp = (uTmp>>31)&0x1;
	    //
		   // if (uTmp){
				//printk("dwc3 tcc: PHY init ok - %s speed support\n", maximum_speed);
				//break;
		   // }
	    //}
	} else if (on_off == OFF) {
		// USB 3.0 PHY Power down
		printk("dwc3 tcc: PHY power down\n");
		USBPHYCFG->U30_PCFG0 |= (Hw25|Hw24);
		mdelay(10);
		uTmp = USBPHYCFG->U30_PCFG0;
	}
	return 0;
}

int dwc3_tcc_power_ctrl(struct dwc3_tcc *tcc, int on_off)
{
	int err = 0;

	if ((tcc->vbus_source_ctrl == 1) && (tcc->vbus_source)) {
		if(on_off == ON) {
			err = regulator_enable(tcc->vbus_source);
			if(err) {
				printk("dwc3 tcc: can't enable vbus source\n");
				return err;
			}
			mdelay(1);
		} else if(on_off == OFF) {
			regulator_disable(tcc->vbus_source);
		}
	}
	
	return 0;
}

#ifdef CONFIG_VBUS_CTRL_DEF_ENABLE		/* 017.08.30 */
static unsigned int vbus_control_enable = 1;  /* 2017/03/23, */
#else
static unsigned int vbus_control_enable = 0;  /* 2017/03/23, */
#endif /* CONFIG_VBUS_CTRL_DEF_ON */
module_param(vbus_control_enable, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vbus_control_enable, "ehci vbus control enable");

int dwc3_tcc_vbus_ctrl(struct dwc3_tcc *tcc, int on_off)
{
	int err = 0;

	if (!vbus_control_enable) {
		printk("ehci vbus ctrl disable.\n");
		return -1;
	}

	if ( tcc->usb3en_ctrl_able == 1) {	
		// host enable
		err = gpio_direction_output(tcc->host_en_gpio, 1);
		if(err) {
			printk("dwc3 tcc: can't enable host\n");
			return err;
		}
	}

	if ( tcc->vbus_ctrl_able == 1) {
		err = gpio_direction_output(tcc->vbus_gpio, on_off);
		if(err) {
			printk("dwc3 tcc: can't enable vbus\n");
			return err;
		}
		tcc->vbus_status = on_off;
		printk("dwc3 tcc: vbus %s\n", (tcc->vbus_status) ? "on":"off");
	} else {
		tcc->vbus_status = OFF;
	}

	return 0;
}

static int  dwc3_tcc_probe(struct platform_device *pdev)
{
	struct dwc3_tcc  		*tcc;
	struct dwc3_tcc_data	*pdata = pdev->dev.platform_data;
	struct platform_device	*dwc3;
	struct device	   		*dev = &pdev->dev;
	//unsigned int			core_clk_rate;
	int		   				ret = -ENOMEM;

	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
	{
		printk("%s : failed to mask dma\n", __func__);
		return ret;
	}

	tcc = devm_kzalloc(dev, sizeof(*tcc), GFP_KERNEL);
	if (!tcc) {
		dev_err(dev, "not enough memory\n");
		goto err0;
	}

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;
	if (!dev->coherent_dma_mask)
		dev->coherent_dma_mask = DMA_BIT_MASK(32);


	platform_set_drvdata(pdev, tcc);

	dwc3 = platform_device_alloc("dwc3", PLATFORM_DEVID_AUTO);
	if (!dwc3) {
		dev_err(&pdev->dev, "couldn't allocate dwc3 device\n");
		goto err0;
	}

	memcpy(&dwc3->dev.archdata, &dev->archdata, sizeof(dev->archdata)); //fix issue - failed to alloc dma buffer 
	dwc3_tcc_parse_dt(pdev, tcc);

	if (tcc->hclk)
		clk_prepare_enable(tcc->hclk);
	
	dwc3_tcc_power_ctrl(tcc, ON);

	if (!strncmp("on", vbus, 2)) {
		dwc3_tcc_vbus_ctrl(tcc, ON);
	} else if (!strncmp("off", vbus, 3)) {
		dwc3_tcc_vbus_ctrl(tcc, OFF);
	}

	if (tcc->phy_clk)
		clk_prepare_enable(tcc->phy_clk);

	dwc3->dev.parent = &pdev->dev;
	tcc->dwc3	= dwc3;
	tcc->dev	= &pdev->dev;
	/* PHY initialization */
	dwc3_tcc_phy_ctrl(tcc, ON);
#ifdef DWC3_SQ_TEST_MODE		/* 016.08.26 */
	devm_iounmap(&pdev->dev, tcc->regs);
#endif /* DWC3_SQ_TEST_MODE */
	ret = platform_device_add_resources(dwc3, pdev->resource,
			pdev->num_resources);
	if (ret) {
		dev_err(&pdev->dev, "couldn't add resources to dwc3 device\n");
		goto err1;
	}

	ret = platform_device_add(dwc3);
	if (ret) {
		dev_err(&pdev->dev, "failed to register dwc3 device\n");
		goto err1;
	}

	ret = device_create_file(&dwc3->dev, &dev_attr_vbus);
	
	if (ret) {
		dev_err(&pdev->dev, "failed to create vbus\n");
		goto err1;
	}

#if defined(DWC3_SQ_TEST_MODE)
	ret = device_create_file(&pdev->dev, &dev_attr_phydump);
	if (ret) {
		dev_err(&pdev->dev, "failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_ssc_ovrd_in);
	if (ret) {
		dev_err(&pdev->dev, "failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_ssc_asic_in);
	if (ret) {
		dev_err(&pdev->dev, "failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_rx_boost);
	if (ret) {
		dev_err(&pdev->dev, "failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_rx_eq);
	if (ret) {
		dev_err(&pdev->dev, "failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_tx_dm_sel);
	if (ret) {
		dev_err(&pdev->dev, "failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_tx_dm_3p5db);
	if (ret) {
		dev_err(&pdev->dev, "failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_tx_dm_6db);
	if (ret) {
		dev_err(&pdev->dev, "failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_tx_swing);
	if (ret) {
		dev_err(&pdev->dev, "failed to create phydump\n");
		goto err1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_tx_vboost);
	if (ret) {
		dev_err(&pdev->dev, "failed to create phydump\n");
		goto err1;
	}
#endif
	printk("%s:%d end\n", __func__, __LINE__);
	return 0;

err1:
	printk("%s:%d \n", __func__, __LINE__);
	if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, pdata->phy_type);
err0:
	printk("%s:%d error\n", __func__, __LINE__);
	return ret;
}

static int dwc3_tcc_remove(struct platform_device *pdev)
{
	struct dwc3_tcc	*tcc = platform_get_drvdata(pdev);
	struct dwc3_tcc_data *pdata = pdev->dev.platform_data;

	clk_disable_unprepare(tcc->phy_clk);

	dwc3_tcc_phy_ctrl(tcc, OFF);

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

#ifdef CONFIG_PM_SLEEP
static int dwc3_tcc_suspend(struct device *dev)
{
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));

	// usb phy power down
	dwc3_tcc_phy_ctrl(tcc, OFF);
	clk_disable_unprepare(tcc->phy_clk);

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
	struct dwc3_tcc	*tcc =	platform_get_drvdata(to_platform_device(dev));

	if (tcc->hclk)
		clk_prepare_enable(tcc->hclk);

	dwc3_tcc_power_ctrl(tcc, ON);
	dwc3_tcc_vbus_ctrl(tcc, tcc->vbus_pm_status);

	clk_prepare_enable(tcc->phy_clk);

	dwc3_tcc_phy_ctrl(tcc, ON);

//   writel(0x1, tcc_p2v(0x16370004)); // uart: suspend debug
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
	{ .compatible = "telechips,tcc-dwc3" },
	{},
};
MODULE_DEVICE_TABLE(of, dwc3_tcc_match);

static int dwc3_tcc_parse_dt(struct platform_device *pdev, struct dwc3_tcc *tcc)
{
	int err= 0;
	struct resource	*res;

#if defined(DWC3_SQ_TEST_MODE)
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing phy memory resource\n");
		return -1;
	}
	res->end += 1;
	tcc->regs = devm_ioremap(&pdev->dev, res->start, res->end-res->start);
	//tcc->regs = 0xf2900000;
#endif
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "missing phy memory resource\n");
		return -1;
	}
	res->end += 1;
	tcc->phy_regs = devm_ioremap(&pdev->dev, res->start, res->end-res->start);

	//===============================================
	// Check Host enable pin
	//===============================================
	if (of_find_property(pdev->dev.of_node, "usb3en-ctrl-able", 0)) {
		tcc->usb3en_ctrl_able = 1;
		tcc->host_en_gpio = of_get_named_gpio(pdev->dev.of_node,
						"usb3en-gpio", 0);
		if(!gpio_is_valid(tcc->host_en_gpio)){
			dev_err(&pdev->dev, "can't find dev of node: host en gpio\n");
			return -ENODEV;
		}
	
		err = gpio_request(tcc->host_en_gpio, "xhci_en_gpio");
		if(err) {
			dev_err(&pdev->dev, "can't requeest xhci_en_gpio\n");
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
		tcc->vbus_gpio = of_get_named_gpio(pdev->dev.of_node,
						"vbus-gpio", 0);
		if(!gpio_is_valid(tcc->vbus_gpio)) {
			dev_err(&pdev->dev, "can't find dev of node: vbus gpio\n");
	
			return -ENODEV;
		}
	
		err = gpio_request(tcc->vbus_gpio, "usb30_vbus_gpio");
		if(err) {
			dev_err(&pdev->dev, "can't requeest vbus gpio\n");
			return err;
		}		
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
			dev_err(&pdev->dev, "regulator get fail!!!\n");
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

void dwc3_tcc_free_dt( struct dwc3_tcc *tcc)
{
	if ( tcc->vbus_ctrl_able == 1) {
		gpio_free(tcc->host_en_gpio);
		gpio_free(tcc->vbus_gpio);
		if (tcc->vbus_source)
			regulator_put(tcc->vbus_source);
	}
}

#endif

static struct platform_driver dwc3_tcc_driver = {
	.probe		= dwc3_tcc_probe,
	.remove		= dwc3_tcc_remove,
	.driver		= {
		.name	= "tcc-dwc3",			
		.owner	= THIS_MODULE,
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
