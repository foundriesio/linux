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
#include <asm/irq.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <asm/irq.h>
#include <linux/module.h>
#include "ohci.h"
#include "tcc-hcd.h"

#define DRIVER_DESC "USB 2.0 'Enhanced' Host Controller (OHCI-TCC) Driver"

#define tcc_ohci_readl(r)       (readl((r)))
#define tcc_ohci_writel(v, r)   (writel((v), (r)))

/* TCC897x USB PHY */
#define TCC_OHCI_PHY_BCFG       (0x00)
#define TCC_OHCI_PHY_PCFG0      (0x04)
#define TCC_OHCI_PHY_PCFG1      (0x08)
#define TCC_OHCI_PHY_PCFG2      (0x0C)
#define TCC_OHCI_PHY_PCFG3      (0x10)
#define TCC_OHCI_PHY_PCFG4      (0x14)
#define TCC_OHCI_PHY_STS        (0x18)
#define TCC_OHCI_PHY_LCFG0      (0x1C)
#define TCC_OHCI_HSIO_CFG       (0x40)

static struct hc_driver __read_mostly ohci_tcc_hc_driver;

struct tcc_ohci_hcd {
	struct device *dev;
	struct usb_hcd *hcd;
	int32_t hosten_ctrl_able;
	int32_t host_en_gpio;

	int32_t vbus_ctrl_able;
	int32_t vbus_gpio;

	int32_t vbus_source_ctrl;
	struct regulator *vbus_source;

	struct clk *hclk;

	uint32_t core_clk_rate;

	/* USB PHY */
	void __iomem *phy_regs;         /* device memory/io */
	resource_size_t phy_rsrc_start; /* memory/io resource start */
	resource_size_t phy_rsrc_len;   /* memory/io resource length */

	uint32_t hcd_tpl_support;   /* TPL support */
};

static int32_t tcc_ohci_parse_dt(struct platform_device *pdev,
		struct tcc_ohci_hcd *tcc_ohci);

/* TPL Support Attribute */
static ssize_t ohci_tpl_support_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_ohci_hcd *tcc_ohci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = tcc_ohci->hcd;

	return sprintf(buf, "tpl support : %s\n",
			((uint32_t)(hcd->tpl_support) != 0U) ? "on" : "off");
}

static ssize_t ohci_tpl_support_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_ohci_hcd *tcc_ohci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = tcc_ohci->hcd;

	if (strncmp(buf, "on", 2) == 0) {
		tcc_ohci->hcd_tpl_support = ON;
	}

	if (strncmp(buf, "off", 3) == 0) {
		tcc_ohci->hcd_tpl_support = OFF;
	}

	hcd->tpl_support = tcc_ohci->hcd_tpl_support;

	return (ssize_t)count;
}

DEVICE_ATTR(ohci_tpl_support, 0644,
		ohci_tpl_support_show, ohci_tpl_support_store);

int32_t tcc_ohci_clk_ctrl(struct tcc_ohci_hcd *tcc_ohci, int32_t on_off)
{
	if (tcc_ohci == NULL) {
		return -ENODEV;
	}

	if (on_off == ON) {
		if (clk_prepare_enable(tcc_ohci->hclk) != 0) {
			dev_err(tcc_ohci->dev, "[ERROR][USB] can't do usb 2.0 hclk clock enable\n");

			return -1;
		}
	} else {
		if (tcc_ohci->hclk != NULL) {
			clk_disable_unprepare(tcc_ohci->hclk);
		} else {
			/* Nothing to do */
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tcc_ohci_clk_ctrl);

int32_t tcc_ohci_vbus_ctrl(struct tcc_ohci_hcd *tcc_ohci, int32_t on_off)
{
	int32_t err = 0;

	if (tcc_ohci == NULL) {
		return -ENODEV;
	}

	if (tcc_ohci->vbus_source_ctrl == 1) {
		if (on_off == ON) {
			if (tcc_ohci->vbus_source != NULL) {
				err = regulator_enable(tcc_ohci->vbus_source);

				if (err != 0) {
					dev_err(tcc_ohci->dev, "[ERROR][USB] can't enable vbus source\n");

					return err;
				}
			}
		} else if (on_off == OFF) {
			if (tcc_ohci->vbus_source != NULL) {
				regulator_disable(tcc_ohci->vbus_source);
			}
		} else {
			/* Nothing to do */
		}
	}

	return err;
}
EXPORT_SYMBOL_GPL(tcc_ohci_vbus_ctrl);

static void tcc_ohci_phy_ctrl(struct tcc_ohci_hcd *tcc_ohci, int32_t on_off)
{
	if (tcc_ohci == NULL) {
		return;
	}

	if (on_off == ON) {
		if (ehci_phy_set == 0) {
			pr_info("[INFO][USB] ehci load first!\n");
		}

	} else if (on_off == OFF) {
		tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs +
					TCC_OHCI_PHY_PCFG0) | Hw8,
				tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG0);
		tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs +
					TCC_OHCI_PHY_PCFG1) | Hw28,
				tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG1);
	} else {
		/* Nothing to do */
	}
}

static int32_t tcc_ohci_select_pmm(ulong reg_base, int32_t mode,
		int32_t num_port, int32_t *port_mode)
{
	volatile struct TCC_OHCI *ohci_reg =
		(volatile struct TCC_OHCI *)reg_base;
	int32_t p;

	if (ohci_reg == NULL) {
		return -ENXIO;
	}

	switch (mode) {
	case TCC_OHCI_PPM_NPS:
		/* set NO Power Switching mode */
		ohci_reg->HcRhDescriptorA |= RH_A_NPS;
		break;
	case TCC_OHCI_PPM_GLOBAL:
		/*
		 * make sure the NO Power Switching mode bit is OFF so Power
		 * Switching can occur make sure the PSM bit is CLEAR, which
		 * allows all ports to be controlled with the GLOBAL set and
		 * clear power commands
		 */
		ohci_reg->HcRhDescriptorA &=
			~(RH_A_NPS | TCC_OHCI_UHCRHDA_PSM_PERPORT);
		break;
	case TCC_OHCI_PPM_PERPORT:
		/*
		 * make sure the NO Power Switching mode bit is OFF so Power
		 * Switching can occur make sure the PSM bit is SET, which
		 * allows all ports to be controlled with the PER PORT set and
		 * clear power commands
		 */
		ohci_reg->HcRhDescriptorA &= ~RH_A_NPS;
		ohci_reg->HcRhDescriptorA |= (ulong)TCC_OHCI_UHCRHDA_PSM_PERPORT;

		/*
		 * set the power management mode for each individual port to Per
		 * Port.
		 */
		for (p = 0; p < num_port; p++) {
			// port 1 begins at bit 1
			ohci_reg->HcRhDescriptorB |=
				(unsigned int)(1u << (p + 17));
		}
		break;
	case TCC_OHCI_PPM_MIXED:
		/*
		 * make sure the NO Power Switching mode bit is OFF so Power
		 * Switching can occur make sure the PSM bit is SET, which
		 * allows all ports to be controlled with the PER PORT set and
		 * clear power commands
		 */
		ohci_reg->HcRhDescriptorA &= ~RH_A_NPS;
		ohci_reg->HcRhDescriptorA |= (ulong)TCC_OHCI_UHCRHDA_PSM_PERPORT;

		/*
		 * set the power management mode for each individual port to Per
		 * Port. if the value in the PortMode array is non-zero, set Per
		 * Port mode for the port. if the value in the PortMode array is
		 * zero, set Global mode for the port
		 */
		for (p = 0; p < num_port; p++) {
			// port 1 begins at bit 17
			if (port_mode[p] != 0) {
				ohci_reg->HcRhDescriptorB |=
					(unsigned int)(1u << (p + 17));
			} else {
				ohci_reg->HcRhDescriptorB &=
					~(unsigned int)(1u << (p + 17));
			}
		}
		break;
	default:
		pr_err("[ERROR][USB] Invalid mode %d, set to non-power switch mode.\n",
				mode);
		ohci_reg->HcRhDescriptorA |= RH_A_NPS;
		break;
	}

	return 0;
}

/*
 * usb_hcd_tcc_probe - initialize tcc-based HCDs
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 *
 */
static int32_t usb_hcd_tcc_probe(const struct hc_driver *driver,
		struct platform_device *pdev)
{
	int32_t retval = 0;
	struct usb_hcd *hcd = usb_create_hcd(driver, &pdev->dev, "tcc");
	struct tcc_ohci_hcd *tcc_ohci;
	struct resource *res;
	struct resource *res1;
	int32_t irq;

	retval = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (retval != 0) {
		return retval;
	}

	tcc_ohci = devm_kzalloc(&pdev->dev, sizeof(struct tcc_ohci_hcd),
			GFP_KERNEL);

	if (!tcc_ohci) {
		return -ENOMEM;
	}

	/* Parsing the device table */
	retval = tcc_ohci_parse_dt(pdev, tcc_ohci);

	/*
	 * If the value of the ehci_phy_set variable is -EPROBE_DEFER,
	 * ehci_tcc_drv_probe() in ehci-tcc.c failed because the VBus GPIO pin
	 * number is set to an invalid number. In such a case, the
	 * usb_hcd_tcc_probe() in this ohci-tcc.c also fails.
	 */
	if (ehci_phy_set == -EPROBE_DEFER) {
		pr_err("[ERROR][USB] The tcc-ohci probe will also fail because the VBus GPIO pin number for ehci_phy is not valid.\n");
		retval = -EPROBE_DEFER;
		goto err0;
	} else {
		if (retval != 0) {
			dev_err(&pdev->dev, "[ERROR][USB] Device table parsing failed.\n");
			retval = -EIO;
			goto err0;
		} else {
			/* Nothing to do */
		}
	}

	irq = platform_get_irq(pdev, 0);

	if (irq <= 0) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto err0;
	}

	/* USB OHCI Base Address*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no register addr. Check %s setup!\n",
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto err1;
	}

	if (!hcd) {
		return -ENOMEM;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = res->end - res->start + 1U;
	hcd->regs = devm_ioremap(&pdev->dev, res->start, hcd->rsrc_len);

	/* USB PHY (UTMI) Base Address*/
	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	if (!res1) {
		dev_err(&pdev->dev,
			"[ERROR][USB] Found HC with no register addr. Check %s setup!\n",
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto err1;
	}

	tcc_ohci->hcd = hcd;

	tcc_ohci->phy_rsrc_start = res1->start;
	tcc_ohci->phy_rsrc_len = res1->end - res1->start + 1U;
	tcc_ohci->phy_regs = devm_ioremap(&pdev->dev, res1->start,
			tcc_ohci->phy_rsrc_len);

	platform_set_drvdata(pdev, tcc_ohci);
	tcc_ohci->dev = &(pdev->dev);
	pdev->dev.platform_data = dev_get_platdata(&pdev->dev);

	/* TPL Support Set */
	hcd->tpl_support = tcc_ohci->hcd_tpl_support;

	tcc_ohci_clk_ctrl(tcc_ohci, ON);

	/* USB HOST Power Enable */
	if (tcc_ohci_vbus_ctrl(tcc_ohci, ON) != 0) {
		pr_err("[ERROR][USB] ohci-tcc: USB HOST VBUS failed\n");
		retval = -EIO;
		goto err2;
	}

	tcc_ohci_phy_ctrl(tcc_ohci, ON);

	/* Select Power Management Mode */
	tcc_ohci_select_pmm((ulong)hcd->regs, TCC_OHCI_PPM_PERPORT,
			0, NULL);

	ohci_hcd_init(hcd_to_ohci(hcd));

	retval = device_create_file(&pdev->dev, &dev_attr_ohci_tpl_support);
	if (retval < 0) {
		pr_err("[ERROR][USB] Cannot register USB TPL Support attributes: %d\n",
		       retval);
		goto err2;
	}

	retval = usb_add_hcd(hcd, (uint32_t)irq, IRQF_SHARED);
	if (retval == 0) {
		return retval;
	}

err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
err0:
	dev_err(&pdev->dev, "[ERROR][USB] init %s fail, %d\n",
			dev_name(&pdev->dev), retval);

	return retval;
}

/*
 * usb_hcd_tcc_remove - shutdown processing for tcc-based HCDs
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_tcc_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 *
 */
static void usb_hcd_tcc_remove(struct usb_hcd *hcd,
		struct platform_device *pdev)
{
	if (hcd == NULL) {
		return;
	}

	usb_remove_hcd(hcd);

	device_remove_file(&pdev->dev, &dev_attr_ohci_tpl_support);

	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
}

#if defined(CONFIG_PM)
#if !defined(CONFIG_ARCH_TCC896X) && !defined(CONFIG_ARCH_TCC802X) &&	\
	!defined(CONFIG_ARCH_TCC899X) &&				\
	!defined(CONFIG_ARCH_TCC803X) &&				\
	!defined(CONFIG_ARCH_TCC901X) && !defined(CONFIG_ARCH_TCC805X)
static void tcc_ohci_PHY_cfg(struct device *dev)
{
	struct tcc_ohci_hcd *tcc_ohci = dev_get_drvdata(dev);

	tcc_ohci_writel(0x03000115, tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG0);
	tcc_ohci_writel(0x0334D143, tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG1);
	tcc_ohci_writel(0x4, tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG2);
	tcc_ohci_writel(0x0, tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG3);
	tcc_ohci_writel(0x0, tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG4);
	tcc_ohci_writel(0x30048020, tcc_ohci->phy_regs + TCC_OHCI_PHY_LCFG0);

	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs +
				TCC_OHCI_PHY_PCFG0) | Hw31,
			tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG0);
	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs +
				TCC_OHCI_PHY_LCFG0) & 0xCFFFFFFFU,
			tcc_ohci->phy_regs + TCC_OHCI_PHY_LCFG0);

	udelay(10);

	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs +
				TCC_OHCI_PHY_PCFG0) & ~(Hw31),
			tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG0);
	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs +
				TCC_OHCI_PHY_PCFG0) & ~(Hw24),
			tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG0);

	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs +
				TCC_OHCI_PHY_PCFG4) | Hw30,
			tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG4);
	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs +
				TCC_OHCI_PHY_PCFG4) | 0x1400U,
			tcc_ohci->phy_regs + TCC_OHCI_PHY_PCFG4);

	tcc_ohci_writel(tcc_ohci_readl(tcc_ohci->phy_regs +
				TCC_OHCI_PHY_LCFG0) | 0x30000000U,
			tcc_ohci->phy_regs + TCC_OHCI_PHY_LCFG0);
}
#endif

static int32_t tcc_ohci_suspend(struct device *dev)
{
	struct tcc_ohci_hcd *tcc_ohci = dev_get_drvdata(dev);
	struct usb_hcd *hcd = tcc_ohci->hcd;
	bool do_wakeup = device_may_wakeup(dev);
	int32_t ret = ohci_suspend(hcd, do_wakeup);

	if (ret != 0) {
		return ret;
	}

#if !defined(CONFIG_ARCH_TCC896X) && !defined(CONFIG_ARCH_TCC802X) &&	\
	!defined(CONFIG_ARCH_TCC899X) &&				\
	!defined(CONFIG_ARCH_TCC803X) && !defined(CONFIG_ARCH_TCC901X)
	tcc_ohci_phy_ctrl(tcc_ohci, OFF);
	tcc_ohci_clk_ctrl(tcc_ohci, OFF);
	tcc_ohci_vbus_ctrl(tcc_ohci, OFF);
#endif

	return 0;
}

static int32_t tcc_ohci_resume(struct device *dev)
{
	struct tcc_ohci_hcd *tcc_ohci = dev_get_drvdata(dev);
	struct usb_hcd *hcd = tcc_ohci->hcd;
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);

#if !defined(CONFIG_ARCH_TCC896X) && !defined(CONFIG_ARCH_TCC802X) &&	\
	!defined(CONFIG_ARCH_TCC899X) &&				\
	!defined(CONFIG_ARCH_TCC803X) &&				\
	!defined(CONFIG_ARCH_TCC901X) && !defined(CONFIG_ARCH_TCC805X)
	tcc_ohci_phy_ctrl(tcc_ohci, ON);
	tcc_ohci_vbus_ctrl(tcc_ohci, ON);
	tcc_ohci_clk_ctrl(tcc_ohci, ON);
#if !defined(CONFIG_ARCH_TCC898X) && !defined(CONFIG_ARCH_TCC899X) &&	\
	!defined(CONFIG_ARCH_TCC803X) &&				\
	!defined(CONFIG_ARCH_TCC901X) && !defined(CONFIG_ARCH_TCC805X)
	tcc_ohci_PHY_cfg(dev);
#endif
#endif
	if (ohci == NULL) {
		return -ENODEV;
	}

	ohci->flags &= ~TCC_OHCI_QUIRK_SUSPEND;

	if (time_before(jiffies, ohci->next_statechange)) {
		usleep_range(5000, 10000);
	}

	ohci->next_statechange = jiffies;
	ohci_resume(hcd, false);

	return 0;
}
#else
#define tcc_ohci_suspend        (NULL)
#define tcc_ohci_resume         (NULL)
#endif

static int32_t ohci_hcd_tcc_drv_probe(struct platform_device *pdev)
{
	if (usb_disabled() != 0) {
		return -ENODEV;
	}

	return usb_hcd_tcc_probe(&ohci_tcc_hc_driver, pdev);
}

static int32_t ohci_hcd_tcc_drv_remove(struct platform_device *pdev)
{
	struct tcc_ohci_hcd *tcc_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = tcc_ohci->hcd;

	usb_hcd_tcc_remove(hcd, pdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id tcc_ohci_match[] = {
	{ .compatible = "telechips,tcc-ohci" },
	{},
};

#if defined(CONFIG_OF)
static int32_t tcc_ohci_parse_dt(struct platform_device *pdev,
		struct tcc_ohci_hcd *tcc_ohci)
{
	int32_t err = 0;

	// Check VBUS Source enable
	if (of_find_property(pdev->dev.of_node, "vbus-source-ctrl", NULL) != NULL) {
		tcc_ohci->vbus_source_ctrl = 1;
		tcc_ohci->vbus_source = regulator_get(&pdev->dev, "vdd_ohci");

		if (IS_ERR(tcc_ohci->vbus_source)) {
			dev_err(&pdev->dev, "[ERROR][USB] failed to get ohci vdd_source\n");
			tcc_ohci->vbus_source = NULL;
			err = 1;
		}
	} else {
		tcc_ohci->vbus_source_ctrl = 0;
		err = 0;
	}

	tcc_ohci->hclk = of_clk_get(pdev->dev.of_node, 0);

	if (IS_ERR(tcc_ohci->hclk))
		tcc_ohci->hclk = NULL;

	// Check TPL Support
	if (of_usb_host_tpl_support(pdev->dev.of_node)) {
		tcc_ohci->hcd_tpl_support = 1;
	} else {
		tcc_ohci->hcd_tpl_support = 0;
	}

	return err;
}
#else
static int32_t tcc_ohci_parse_dt(struct platform_device *pdev,
		struct tcc_ohci_hcd *tcc_ohci)
{
	return 0;
}
#endif

MODULE_DEVICE_TABLE(of, tcc_ohci_match);
#endif

static SIMPLE_DEV_PM_OPS(tcc_ohci_pm_ops, tcc_ohci_suspend, tcc_ohci_resume);

static struct platform_driver ohci_hcd_tcc_driver = {
	.probe	= ohci_hcd_tcc_drv_probe,
	.remove	= ohci_hcd_tcc_drv_remove,
	.driver	= {
		.name	= "tcc-ohci",
		.owner	= THIS_MODULE,
		.pm	= &tcc_ohci_pm_ops,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_ohci_match),
#endif
	},
};

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

static int32_t __init tcc_ohci_hcd_init(void)
{
	int32_t retval = 0;

	ohci_init_driver(&ohci_tcc_hc_driver, NULL);
	set_bit(USB_EHCI_LOADED, &usb_hcds_loaded);

	retval = platform_driver_register(&ohci_hcd_tcc_driver);
	if (retval < 0) {
		clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
	}

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
