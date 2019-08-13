/*
 * linux/drivers/usb/host/ohci-tcc.c
 *
 * Description: OHCI HCD (Host Controller Driver) for USB.
 *              Bus Glue for Telechips TCCXXX
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2002 David Brownell <dbrownell@users.sourceforge.net>
 * (C) Copyright 2002 Hewlett-Packard Company
 *
 * Bus Glue for Telechips SoC
 *
 * Written by Christopher Hoover <ch@hpl.hp.com>
 * Based on fragments of previous driver by Russell King et al.
 *
 * Modified for LH7A404 from ohci-sa1111.c
 *  by Durgesh Pattamatta <pattamattad@sharpsec.com>
 *
 * Modified for pxa27x from ohci-lh7a404.c
 *  by Nick Bane <nick@cecomputing.co.uk> 26-8-2004
 *
 * Modified for Telechips SoC from ohci-pxa27x.c
 *  by Telechips Linux Team <linux@telechips.com> 03-9-2008
 *
 * This file is licenced under the GPL.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/signal.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <asm/io.h>
#include <asm/irq.h>
#include "tcc-hcd.h"
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <asm/irq.h>
#include <linux/module.h>
//#include <mach/bsp.h>
#include "ohci.h"

#define DRIVER_DESC "USB 2.0 'Enhanced' Host Controller (OHCI-TCC) Driver"
//extern int tcc_ohci_vbus_ctrl(int id, int on);
extern void tcc_ohci_clock_control(int id, int on);
extern int ehci_phy_set;

#define tcc_ohci_readl(r)		readl(r)
#define tcc_ohci_writel(v,r)		writel(v,r)

/* TCC897x USB PHY */
#define TCC_OHCI_PHY_BCFG			0x00
#define TCC_OHCI_PHY_PCFG0			0x04
#define TCC_OHCI_PHY_PCFG1			0x08
#define TCC_OHCI_PHY_PCFG2			0x0C
#define TCC_OHCI_PHY_PCFG3			0x10
#define TCC_OHCI_PHY_PCFG4			0x14
#define TCC_OHCI_PHY_STS			0x18
#define TCC_OHCI_PHY_LCFG0			0x1C
#define TCC_OHCI_HSIO_CFG			0x40

static struct hc_driver __read_mostly ohci_tcc_hc_driver;

struct tcc_ohci_hcd {
	struct device *dev;
	struct usb_hcd *hcd;
	int	hosten_ctrl_able;
	int host_en_gpio;

	int vbus_ctrl_able;
	int vbus_gpio;

	int vbus_source_ctrl;
	struct regulator *vbus_source;

	struct clk *hclk;

	unsigned int			core_clk_rate;

	/* USB PHY */
	void __iomem		*phy_regs;		/* device memory/io */
	resource_size_t		phy_rsrc_start;	/* memory/io resource start */
	resource_size_t		phy_rsrc_len;	/* memory/io resource length */

	unsigned int hcd_tpl_support;	/* TPL support */
};

static int tcc_ohci_parse_dt(struct platform_device *pdev, struct tcc_ohci_hcd *tcc_ohci);

/* TPL Support Attribute */
static ssize_t ohci_tpl_support_show(struct device *_dev,
	          struct device_attribute *attr, char *buf)
{
	struct tcc_ohci_hcd *tcc_ohci =	dev_get_drvdata(_dev);
	struct usb_hcd *hcd = tcc_ohci->hcd;

	return sprintf(buf, "tpl support : %s\n", hcd->tpl_support ? "on" : "off");
}
static ssize_t ohci_tpl_support_store(struct device *_dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tcc_ohci_hcd *tcc_ohci =	dev_get_drvdata(_dev);
	struct usb_hcd *hcd = tcc_ohci->hcd;

	if (!strncmp(buf, "on", 2)) {
		tcc_ohci->hcd_tpl_support = ON;
	}

	if (!strncmp(buf, "off", 3)) {
		tcc_ohci->hcd_tpl_support = OFF;
	}

	hcd->tpl_support = tcc_ohci ->hcd_tpl_support;

	return count;
}
DEVICE_ATTR(ohci_tpl_support, S_IRUGO | S_IWUSR, ohci_tpl_support_show, ohci_tpl_support_store);

int tcc_ohci_clk_ctrl(struct tcc_ohci_hcd *tcc_ohci, int on_off)
{
	if(on_off == ON) {
		if(clk_prepare_enable(tcc_ohci->hclk) != 0) {
			dev_err(tcc_ohci->dev,
				"can't do usb 2.0 hclk clock enable\n");
			return -1;
		}
	} else {
		if(tcc_ohci->hclk) {
			clk_disable_unprepare(tcc_ohci->hclk);
			//clk_put(tcc_ohci->hclk);		
			//tcc_ohci->hclk = NULL;
		}
	}

	//dev_info(tcc_ohci->dev, "clk %s\n", (on_off) ? "enable":"disable");

	return 0;
}

int tcc_ohci_vbus_ctrl(struct tcc_ohci_hcd *tcc_ohci, int on_off)
{
	int err = 0;

	if (tcc_ohci->vbus_source_ctrl == 1) {
		if(on_off == ON) {
			if (tcc_ohci->vbus_source) {
				err = regulator_enable(tcc_ohci->vbus_source);
				if(err) {
					dev_err(tcc_ohci->dev,
						"can't enable vbus source\n");
					return err;
				}
			}
		} else if (on_off == OFF) {
			if (tcc_ohci->vbus_source)
				regulator_disable(tcc_ohci->vbus_source);
		}
	}

	return err;
}

static void tcc_ohci_phy_ctrl(struct tcc_ohci_hcd *tcc_ohci, int on_off)
{
	//PHSIOBUSCFG pEHCIPHYCFG = (PHSIOBUSCFG)tcc_p2v(HwHSIOBUSCFG_BASE);
	if(on_off == ON) {
		if (!ehci_phy_set)
			printk("ehci load first!\n");

	}
	else if(on_off == OFF) {
		tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG0) | Hw8,tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG0);
		tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG1) | Hw28,tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG1);
		//pEHCIPHYCFG->USB20H_PCFG0.nREG |= Hw8;
		//pEHCIPHYCFG->USB20H_PCFG1.nREG |= Hw28; //OTG block Power down
	}
}

static int tcc_ohci_select_pmm(unsigned long reg_base, int mode, int num_port, int *port_mode)
{
	volatile TCC_OHCI *ohci_reg;
	ohci_reg = (volatile TCC_OHCI *)reg_base;

	switch (mode)
	{
	case TCC_OHCI_PPM_NPS:
		/* set NO Power Switching mode */
		ohci_reg->HcRhDescriptorA.nREG |= RH_A_NPS;
		break;
	case TCC_OHCI_PPM_GLOBAL:
		/* make sure the NO Power Switching mode bit is OFF so Power Switching can occur
		 * make sure the PSM bit is CLEAR, which allows all ports to be controlled with
		 * the GLOBAL set and clear power commands
		 */
		ohci_reg->HcRhDescriptorA.nREG &= ~(RH_A_NPS | TCC_OHCI_UHCRHDA_PSM_PERPORT);
		break;
	case TCC_OHCI_PPM_PERPORT:
		/* make sure the NO Power Switching mode bit is OFF so Power Switching can occur
		 * make sure the PSM bit is SET, which allows all ports to be controlled with
		 * the PER PORT set and clear power commands
		 */
		ohci_reg->HcRhDescriptorA.nREG &= ~RH_A_NPS;
		ohci_reg->HcRhDescriptorA.nREG |=  TCC_OHCI_UHCRHDA_PSM_PERPORT;

		/* set the power management mode for each individual port to Per Port. */
		{
			int p;
			for (p = 0; p < num_port; p++)
			{
				ohci_reg->HcRhDescriptorB.nREG |= (unsigned int)(1u << (p + 17));   // port 1 begins at bit 17
			}
		}
		break;
	case TCC_OHCI_PPM_MIXED:
		/* make sure the NO Power Switching mode bit is OFF so Power Switching can occur
		 * make sure the PSM bit is SET, which allows all ports to be controlled with
		 * the PER PORT set and clear power commands
		 */
		ohci_reg->HcRhDescriptorA.nREG &= ~RH_A_NPS;
		ohci_reg->HcRhDescriptorA.nREG |=  TCC_OHCI_UHCRHDA_PSM_PERPORT;

		/* set the power management mode for each individual port to Per Port.
		 * if the value in the PortMode array is non-zero, set Per Port mode for the port.
		 * if the value in the PortMode array is zero, set Global mode for the port
		 */
		{
			int p;
			for (p = 0; p < num_port; p++)
			{
				if (port_mode[p])
				{
					ohci_reg->HcRhDescriptorB.nREG |= (unsigned int)(1u << (p + 17));   // port 1 begins at bit 17
				}
				else
				{
					ohci_reg->HcRhDescriptorB.nREG &= ~(unsigned int)(1u << (p + 17));  // port 1 begins at bit 17
				}
			}
		}
		break;
	default:
		printk(KERN_ERR "Invalid mode %d, set to non-power switch mode.\n", mode);
		ohci_reg->HcRhDescriptorA.nREG |= RH_A_NPS;
	}

	return 0;
}

extern int usb_disabled(void);

/*-------------------------------------------------------------------------*/

/*
 * TCC89/91/92 has two down stream ports.
 * Down stream port1 interfaces with USB1.1 transceiver.
 * Down stream port2 interfaces with USB2.0 OTG PHY.
 * port1 should be power off, because OTG use USB2.0 OTG PHY.
 */
void ohci_port_power_control(int id)
{
	if (id == 1)
	{
		;
	}
}

/*-------------------------------------------------------------------------*/
static int ohci_tcc_start(struct usb_hcd *hcd);

/* configure so an HC device and id are always provided */
/* always called with process context; sleeping is OK */

extern unsigned long usb_hcds_loaded;
extern const struct hc_driver* get_ohci_hcd_driver(void);
extern void ohci_hcd_init (struct ohci_hcd *ohci);
extern int ohci_init (struct ohci_hcd *ohci);
extern int ohci_run (struct ohci_hcd *ohci);
extern void ohci_stop (struct usb_hcd *hcd);
/**
 * usb_hcd_tcc_probe - initialize tcc-based HCDs
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 *
 */
static int usb_hcd_tcc_probe(const struct hc_driver *driver, struct platform_device *pdev)
{
	int retval = 0;
	struct usb_hcd *hcd;
	struct tcc_ohci_hcd *tcc_ohci;
	struct resource *res;
	struct resource *res1;
	int irq;

	retval = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (retval)
		return retval;

	tcc_ohci = devm_kzalloc(&pdev->dev, sizeof(struct tcc_ohci_hcd),
						GFP_KERNEL);
	if (!tcc_ohci)
		return -ENOMEM;

	/* Parsing the device table */
	retval = tcc_ohci_parse_dt(pdev, tcc_ohci);
	if(retval != 0){
		if(retval != -1)
			printk(KERN_ERR "ochi-tcc: Device table parsing failed\n");
		retval = -EIO;
		goto err0;
	}
	
	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev,
			"Found HC with no IRQ. Check %s setup!\n",
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto err0;
	}

	/* USB OHCI Base Address*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no register addr. Check %s setup!\n",
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto err1;
	}

	hcd = usb_create_hcd(driver, &pdev->dev, "tcc");
	if (!hcd)
		return -ENOMEM;

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = res->end - res->start + 1;
	hcd->regs = devm_ioremap(&pdev->dev, res->start, hcd->rsrc_len);

	/* USB PHY (UTMI) Base Address*/
	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res1) {
		dev_err(&pdev->dev,
			"Found HC with no register addr. Check %s setup!\n",
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto err1;
	}
	tcc_ohci->hcd = hcd;

	tcc_ohci->phy_rsrc_start = res1->start;
	tcc_ohci->phy_rsrc_len = res1->end - res1->start + 1;
	tcc_ohci->phy_regs = devm_ioremap(&pdev->dev, res1->start, tcc_ohci->phy_rsrc_len);

	platform_set_drvdata(pdev, tcc_ohci);
	tcc_ohci->dev = &(pdev->dev);
	pdev->dev.platform_data = dev_get_platdata(&pdev->dev);

	/* TPL Support Set */
	hcd->tpl_support = tcc_ohci->hcd_tpl_support;
	
	tcc_ohci_clk_ctrl(tcc_ohci, ON);

	/* USB HOST Power Enable */
	if (tcc_ohci_vbus_ctrl(tcc_ohci, ON) != 0)
	{
		printk(KERN_ERR "ohci-tcc: USB HOST VBUS failed\n");
		retval = -EIO;
		goto err2;
	}

	//if ((retval = tcc_start_hc(pdev->id, &pdev->dev)) < 0)
	//{
	//	printk(KERN_ERR "tcc_start_hc failed");
	//	goto err2;
	//}

	tcc_ohci_phy_ctrl(tcc_ohci, ON);

	/* Select Power Management Mode */
	tcc_ohci_select_pmm((unsigned long)hcd->regs, /*inf->port_mode*/TCC_OHCI_PPM_PERPORT, 0, 0);

	ohci_hcd_init(hcd_to_ohci(hcd));

	retval = device_create_file(&pdev->dev, &dev_attr_ohci_tpl_support);
	if (retval < 0) {
		printk(KERN_ERR "Cannot register USB TPL Support attributes: %d\n",
		       retval);
		goto err2;
	}

	retval = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (retval == 0){
	        return retval;
	}

err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
err0:
	dev_err(&pdev->dev, "init %s fail, %d\n",
		dev_name(&pdev->dev), retval);
	
	return retval;
}


/* may be called without controller electrically present */
/* may be called with controller, bus, and devices active */

/**
 * usb_hcd_tcc_remove - shutdown processing for tcc-based HCDs
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_tcc_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 *
 */
static void usb_hcd_tcc_remove(struct usb_hcd *hcd, struct platform_device *pdev)
{
	usb_remove_hcd(hcd);

	/* GPIO_EXPAND HVBUS & PWR_GP1 Power-off */
//	tcc_ohci_vbus_ctrl(pdev->id, 0);

	device_remove_file(&pdev->dev, &dev_attr_ohci_tpl_support);

	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
}

/*-------------------------------------------------------------------------*/

static int ohci_tcc_start(struct usb_hcd *hcd)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int ret;
	struct device *dev = hcd->self.controller;
	struct platform_device *pdev = to_platform_device(dev);
		
	/* ignore this - rudals */
	//tcc_set_cken(pdev->id, 1);

	/* The value of NDP in roothub_a is incorrect on this hardware */
	ohci->num_ports = 1;

	if ((ret = ohci_init(ohci)) < 0)
	{
		return ret;
	}
	if ((ret = ohci_run(ohci)) < 0)
	{
		dev_err(dev, "can't start %s", hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}

	ohci_port_power_control(pdev->id);
	return 0;
}


/*-------------------------------------------------------------------------*/

#ifdef	CONFIG_PM
/*-------------------------------------------------------------------------*/
#if !defined (CONFIG_ARCH_TCC896X) && !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC899X) && !defined(CONFIG_ARCH_TCC803X) && !defined(CONFIG_ARCH_TCC901X)
static void tcc_ohci_PHY_cfg(struct device *dev)
{
	//PUSB20PHYCFG	pUSB20PHYCFG =
	//(PUSB20PHYCFG)tcc_p2v(HwUSB20PHYCFG_BASE_0);
	struct tcc_ohci_hcd *tcc_ohci = dev_get_drvdata(dev);

	tcc_ohci_writel(0x03000115, tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG0);
	tcc_ohci_writel(0x0334D143, tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG1);
	tcc_ohci_writel(0x4, tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG2);
	tcc_ohci_writel(0x0, tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG3);
	tcc_ohci_writel(0x0, tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG4);
	tcc_ohci_writel(0x30048020, tcc_ohci->phy_regs+TCC_OHCI_PHY_LCFG0);
	//pUSB20PHYCFG->USB20H0_PCFG0.nREG = 0x03000115;
	//pUSB20PHYCFG->USB20H0_PCFG1.nREG = 0x0334D143;
	//pUSB20PHYCFG->USB20H0_PCFG2.nREG = 0x4;
	//pUSB20PHYCFG->USB20H0_PCFG3.nREG = 0x0;
	//pUSB20PHYCFG->USB20H0_PCFG4.nREG = 0x0;
	//pUSB20PHYCFG->USB20H0_LCFG0.nREG = 0x30048020;

	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG0) | Hw31, tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG0);
	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs+TCC_OHCI_PHY_LCFG0) & 0xCFFFFFFF, tcc_ohci->phy_regs+TCC_OHCI_PHY_LCFG0);
	//pUSB20PHYCFG->USB20H0_PCFG0.bREG.POR = 1;
	//pUSB20PHYCFG->USB20H0_LCFG0.nREG &= 0xCFFFFFFF;

	udelay(10);

	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG0) & ~(Hw31), tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG0);
	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG0) & ~(Hw24), tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG0);
	//pUSB20PHYCFG->USB20H0_PCFG0.bREG.POR = 0;
	//pUSB20PHYCFG->USB20H0_PCFG0.bREG.SIDDQ = 0;
	
	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG4) | Hw30, tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG4);
	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG4) | 0x1400, tcc_ohci->phy_regs+TCC_OHCI_PHY_PCFG4);
	//pUSB20PHYCFG->USB20H0_PCFG4.bREG.IRQ_PHYVALIDEN = 1;
	//pUSB20PHYCFG->USB20H0_PCFG4.nREG |= 0x1400;
	
	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs+TCC_OHCI_PHY_LCFG0) | 0x30000000, tcc_ohci->phy_regs+TCC_OHCI_PHY_LCFG0);
	//pUSB20PHYCFG->USB20H0_LCFG0.nREG |= 0x30000000;
}
#endif

static int tcc_ohci_suspend(struct device *dev)
{
	struct tcc_ohci_hcd *tcc_ohci = dev_get_drvdata(dev);
	struct usb_hcd *hcd = tcc_ohci->hcd;
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	unsigned long flags;
	bool do_wakeup = device_may_wakeup(dev);
	int ret = ohci_suspend(hcd, do_wakeup);
	if (ret)
		return ret;
/*
	spin_lock_irqsave(&ohci->lock, flags);
	ohci->flags |= OHCI_QUIRK_TCC_SUSPEND;
	if (time_before(jiffies, ohci->next_statechange))
		mdelay(5);
	ohci->next_statechange = jiffies;

	hcd->state = HC_STATE_SUSPENDED;
*/
	#if defined(CONFIG_ARCH_TCC896X) || defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)

	#else
	tcc_ohci_phy_ctrl(tcc_ohci, OFF);
	tcc_ohci_clk_ctrl(tcc_ohci, OFF);
	tcc_ohci_vbus_ctrl(tcc_ohci, OFF);
	#endif
	//spin_unlock_irqrestore(&ohci->lock, flags);
	
	return 0;
}

static int tcc_ohci_resume(struct device *dev)
{
	struct tcc_ohci_hcd *tcc_ohci = dev_get_drvdata(dev);
	struct usb_hcd *hcd = tcc_ohci->hcd;
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);

	#if defined(CONFIG_ARCH_TCC896X) || defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)

	#else
	tcc_ohci_phy_ctrl(tcc_ohci, ON);
	tcc_ohci_vbus_ctrl(tcc_ohci, ON);
	tcc_ohci_clk_ctrl(tcc_ohci, ON);
	#if !defined(CONFIG_ARCH_TCC898X) && !defined(CONFIG_ARCH_TCC899X) && defined(CONFIG_ARCH_TCC803X) && !defined(CONFIG_ARCH_TCC901X)
	tcc_ohci_PHY_cfg(dev);
	#endif
	#endif
	ohci->flags &= ~OHCI_QUIRK_TCC_SUSPEND;
	
	if (time_before(jiffies, ohci->next_statechange))
		msleep(5);
	ohci->next_statechange = jiffies;

	ohci_resume(hcd, false);

	return 0;
}
#else
#define tcc_ohci_suspend		NULL
#define tcc_ohci_resume			NULL
#endif

static int ohci_hcd_tcc_drv_probe(struct platform_device *pdev)
{
	if (usb_disabled()) {
		return -ENODEV;
	}
	return usb_hcd_tcc_probe(&ohci_tcc_hc_driver, pdev);
}

static int ohci_hcd_tcc_drv_remove(struct platform_device *pdev)
{
	struct tcc_ohci_hcd *tcc_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = tcc_ohci->hcd;
	usb_hcd_tcc_remove(hcd, pdev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tcc_ohci_match[] = {
	{ .compatible = "telechips,tcc-ohci" },
	{},
};

#ifdef CONFIG_OF
extern bool of_usb_host_tpl_support(struct device_node *np);
static int tcc_ohci_parse_dt(struct platform_device *pdev, struct tcc_ohci_hcd *tcc_ohci)
{
	int err = 0;

#if 0		/* 014.08.18 */
	//===============================================
	// Check Host enable pin	
	//===============================================
	if (of_find_property(pdev->dev.of_node, "hosten-ctrl-able", 0)) {
		tcc_ohci->hosten_ctrl_able = 1;

		tcc_ohci->host_en_gpio = of_get_named_gpio(pdev->dev.of_node,
						"hosten-gpio", 0);
		if(!gpio_is_valid(tcc_ohci->host_en_gpio)){
			dev_err(&pdev->dev, "can't find dev of node: host en gpio\n");
			return -ENODEV;
		}

		err = gpio_request(tcc_ohci->host_en_gpio, "host_en_gpio");
		if(err) {
			dev_err(&pdev->dev, "can't requeest host_en gpio\n");
			return err;
		}
	} else {
		tcc_ohci->hosten_ctrl_able = 0;	// can not control vbus
	}

	//===============================================
	// Check Host enable pin	
	//===============================================
	if (of_find_property(pdev->dev.of_node, "vbus-ctrl-able", 0)) {
		tcc_ohci->vbus_ctrl_able = 1;

		tcc_ohci->vbus_gpio = of_get_named_gpio(pdev->dev.of_node,
						"vbus-gpio", 0);
		if(!gpio_is_valid(tcc_ohci->vbus_gpio)) {
			dev_err(&pdev->dev, "can't find dev of node: vbus gpio\n");

			return -ENODEV;
		}

		err = gpio_request(tcc_ohci->vbus_gpio, "vbus_gpio");
		if(err) {
			dev_err(&pdev->dev, "can't requeest vbus gpio\n");
			return err;
		}
	} else {
		tcc_ohci->vbus_ctrl_able = 0;	// can not control vbus
	}
#endif /* 0 */

	//===============================================
	// Check VBUS Source enable	
	//===============================================
	if (of_find_property(pdev->dev.of_node, "vbus-source-ctrl", 0)) {
		tcc_ohci->vbus_source_ctrl = 1;
		tcc_ohci->vbus_source = regulator_get(&pdev->dev, "vdd_ohci");
		if (IS_ERR(tcc_ohci->vbus_source)) {
			dev_err(&pdev->dev, "failed to get ohci vdd_source\n");
			tcc_ohci->vbus_source = NULL;
		}
	} else {
		tcc_ohci->vbus_source_ctrl = 0;
	}

	tcc_ohci->hclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(tcc_ohci->hclk))
		tcc_ohci->hclk = NULL;
	
	//===============================================
	// Check TPL Support
	//===============================================
	if(of_usb_host_tpl_support(pdev->dev.of_node))
	{
		//printk("\x1b[1;33m[%s:%d] OHCI Support TPL\x1b[0m\n", __func__, __LINE__);
		tcc_ohci->hcd_tpl_support = 1;
	}
	else
	{
		//printk("\x1b[1;31m[%s:%d] OCHI Not Support TPL\x1b[0m\n", __func__, __LINE__);
		tcc_ohci->hcd_tpl_support = 0;
	}

	return err;
}
#else
static int tcc_ohci_parse_dt(struct platform_device *pdev, struct tcc_ohci_hcd *tcc_ohci)
{
	return 0;
}
#endif

MODULE_DEVICE_TABLE(of, tcc_ohci_match);
#endif
static SIMPLE_DEV_PM_OPS(tcc_ohci_pm_ops, tcc_ohci_suspend,
	tcc_ohci_resume);
//static const struct dev_pm_ops tcc_ohci_pm_ops = {
//	SET_SYSTEM_SLEEP_PM_OPS(tcc_ohci_suspend,tcc_ohci_resume)
//};

static struct platform_driver ohci_hcd_tcc_driver =
{
	.probe			= ohci_hcd_tcc_drv_probe,
	.remove			= ohci_hcd_tcc_drv_remove,
	.driver			=
	{
		.name	= "tcc-ohci",
		.owner	= THIS_MODULE,
		.pm	= &tcc_ohci_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_ohci_match),
#endif

	},
};

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE ("GPL");

extern unsigned long usb_hcds_loaded;

static int __init tcc_ohci_hcd_init(void)
{
	int retval = 0;

	ohci_init_driver(&ohci_tcc_hc_driver, NULL);

	set_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
	retval = platform_driver_register(&ohci_hcd_tcc_driver);
	if (retval < 0)
		clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);

	return retval;
}
module_init(tcc_ohci_hcd_init);

static void __exit tcc_ohci_hcd_cleanup(void)
{
	platform_driver_unregister(&ohci_hcd_tcc_driver);
	clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
}
module_exit(tcc_ohci_hcd_cleanup);
/* work with hotplug and coldplug */
MODULE_ALIAS("platform:tcc-ohci");
