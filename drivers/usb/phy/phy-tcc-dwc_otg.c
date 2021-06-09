// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include "../dwc_otg/v3.20a/driver/tcc_otg_regs.h"

#define ON      (1)
#define OFF     (0)

#ifndef BITSET
#define BITSET(X, MASK)                 ((X) |= (uint32_t)(MASK))
#define BITCLR(X, MASK)                 ((X) &= ~((uint32_t)(MASK)))
#define BITXOR(X, MASK)                 ((X) ^= (uint32_t)(MASK))
#define BITCSET(X, CMASK, SMASK)        ((X) = ((((uint32_t)(X)) &	\
				~((uint32_t)(CMASK))) |	\
				((uint32_t)(SMASK))))
#define BITSCLR(X, SMASK, CMASK)        ((X) = ((((uint32_t)(X)) |	\
				((uint32_t)(SMASK))) &	\
				~((uint32_t)(CMASK))))
#define ISZERO(X, MASK)		(!(((uint32_t)(X)) & ((uint32_t)(MASK))))
#define ISSET(X, MASK)		((uint32_t)(X) & ((uint32_t)(MASK)))
#endif

//typedef enum {
enum USBPHY_MODE_T {
	USBPHY_MODE_RESET = 0,
	USBPHY_MODE_OTG,
	USBPHY_MODE_ON,
	USBPHY_MODE_OFF,
	USBPHY_MODE_START,
	USBPHY_MODE_STOP,
	USBPHY_MODE_DEVICE_DETACH
//} USBPHY_DWC_OTG_MODE_T;
};

struct tcc_dwc_otg_device {
	struct device *dev;
	void __iomem *base;
	struct usb_phy phy;
	struct regulator *vbus_supply;
};

struct dwc_otg_phy_reg {
	// 0x100  R/W    USB PHY Configuration Register0
	volatile uint32_t pcfg0;
	// 0x104  R/W    USB PHY Configuration Register1
	volatile uint32_t pcfg1;
	// 0x108  R/W    USB PHY Configuration Register2
	volatile uint32_t pcfg2;
	// 0x10C  R/W    USB PHY Configuration Register3
	volatile uint32_t pcfg3;
	// 0x110  R/W    USB PHY Configuration Register4
	volatile uint32_t pcfg4;
	// 0x114  R/W    USB PHY Status Register
	volatile uint32_t lsts;
	// 0x118  R/W    USB PHY LINK Register0
	volatile uint32_t lcfg0;
	// 0x11C  R/W    USB PHY LINK Register1
	volatile uint32_t lcfg1;
	// 0x120  R/W    USB PHY LINK Register2
	volatile uint32_t lcfg2;
	// 0x124  R/W    USB PHY LINK Register3
	volatile uint32_t lcfg3;
	// 0x128  R/W    USB PHY OTG MUX Register
	volatile uint32_t otgmux;
};

static void __iomem *tcc_dwc_otg_get_base(struct usb_phy *phy)
{
	struct tcc_dwc_otg_device *phy_dev =
		container_of(phy, struct tcc_dwc_otg_device, phy);

	if (phy_dev == NULL) {
		return (void __iomem *)0;
	}

	return phy_dev->base;
}

static int32_t dwc_otg_vbus_set(struct usb_phy *phy, int32_t on_off)
{
	struct tcc_dwc_otg_device *phy_dev =
		container_of(phy, struct tcc_dwc_otg_device, phy);
	struct device *dev;
	int32_t retval = 0;

	if (phy_dev == NULL) {
		return -ENODEV;
	}

	dev = phy_dev->dev;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0) == NULL) {
		dev_err(dev, "[ERROR][USB] vbus-ctrl-able property is not declared in device tree.\n");
		return -ENODEV;
	}

	if (!phy_dev->vbus_supply) {
		dev_err(dev, "[ERROR][USB] Vbus Supply is not valid.\n");
		return -ENODEV;
	}

	if (on_off) {
		retval = regulator_enable(phy_dev->vbus_supply);
		if (retval)
			goto err;

		retval = regulator_set_voltage(phy_dev->vbus_supply,
				5000000, 5000000);
		if (retval)
			goto err;
	} else {
		retval = regulator_set_voltage(phy_dev->vbus_supply,
				1, 1);
		if (retval)
			goto err;

		retval = regulator_disable(phy_dev->vbus_supply);
	}

err:
	if (retval) {
		dev_err(dev, "[ERROR][USB] Vbus Supply is not controlled.\n");
		regulator_disable(phy_dev->vbus_supply);
		return retval;
	}

	usleep_range(3000, 5000);

	return retval;
}

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)

#define PCFG1_TXVRT_MASK	(0xF)
#define PCFG1_TXVRT_SHIFT	(0x0)
#define get_txvrt(x)		(((x) & PCFG1_TXVRT_MASK) >> PCFG1_TXVRT_SHIFT)

static int32_t tcc_dwc_otg_get_dc_level(struct usb_phy *phy)
{
	struct tcc_dwc_otg_device *dwc_otg_phy_dev =
		container_of(phy, struct tcc_dwc_otg_device, phy);
	struct dwc_otg_phy_reg *dwc_otg_pcfg =
		(struct dwc_otg_phy_reg *)dwc_otg_phy_dev->base;
	uint32_t pcfg1_val;

	pcfg1_val = readl(&dwc_otg_pcfg->pcfg1);

	return get_txvrt(pcfg1_val);
}

static int32_t tcc_dwc_otg_set_dc_level(struct usb_phy *phy, uint32_t level)
{
	struct tcc_dwc_otg_device *dwc_otg_phy_dev =
		container_of(phy, struct tcc_dwc_otg_device, phy);
	struct dwc_otg_phy_reg *dwc_otg_pcfg =
		(struct dwc_otg_phy_reg *)dwc_otg_phy_dev->base;
	uint32_t pcfg1_val;

	pcfg1_val = readl(&dwc_otg_pcfg->pcfg1);
	BITCSET(pcfg1_val, PCFG1_TXVRT_MASK, level << PCFG1_TXVRT_SHIFT);
	writel(pcfg1_val, &dwc_otg_pcfg->pcfg1);

	dev_info(dwc_otg_phy_dev->dev, "[INFO][USB] cur dc level = %d\n",
			get_txvrt(pcfg1_val));

	return 0;
}
#endif

#if defined(CONFIG_TCC_DWC_OTG_HOST_MUX) ||	\
	defined(CONFIG_USB_DWC2_TCC_MUX)
/* Host Controller in OTG MUX S/W Reset */
#define TCC_MUX_H_SWRST		(1 << 4)
/* Host Controller in OTG MUX Clock Enable */
#define TCC_MUX_H_CLKMSK	(1 << 3)
/* OTG Controller in OTG MUX S/W Reset */
#define TCC_MUX_O_SWRST		(1 << 2)
/* OTG Controller in OTG MUX Clock Enable */
#define TCC_MUX_O_CLKMSK	(1 << 1)
/* OTG MUX Controller Select */
#define TCC_MUX_OPSEL		(1 << 0)

#define TCC_MUX_O_SELECT	(TCC_MUX_O_SWRST | TCC_MUX_O_CLKMSK)
#define TCC_MUX_H_SELECT	(TCC_MUX_H_SWRST | TCC_MUX_H_CLKMSK)

#define MUX_MODE_HOST           (0)
#define MUX_MODE_DEVICE         (1)
#endif

#ifndef CONFIG_TCC_EH_ELECT_TST
#define USBPHY_IGNORE_VBUS_COMPARATOR
#else
// ACA ID_ID_OTG Pin Resistance Detection Enable; (1:enable) (0:disable)
#define USBOTG_PCFG2_ACAENB	(1 << 13)
#endif

static int32_t tcc_dwc_otg_phy_init(struct usb_phy *phy)
{
	struct tcc_dwc_otg_device *dwc_otg_phy_dev =
		container_of(phy, struct tcc_dwc_otg_device, phy);
	struct dwc_otg_phy_reg *dwc_otg_pcfg;
#if defined(CONFIG_TCC_DWC_OTG_HOST_MUX) || defined(CONFIG_USB_DWC2_TCC_MUX)
	uint32_t mux_cfg_val;
#endif
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
	defined(CONFIG_ARCH_TCC901X) ||					\
	defined(CONFIG_ARCH_TCC805X)
	int32_t i;
#endif

	if (dwc_otg_phy_dev == NULL) {
		return -ENODEV;
	}

	dwc_otg_pcfg = (struct dwc_otg_phy_reg *)dwc_otg_phy_dev->base;

	dev_info(dwc_otg_phy_dev->dev, "[INFO][USB] dwc_otg PHY init\n");
#if defined(CONFIG_TCC_DWC_OTG_HOST_MUX) || defined(CONFIG_USB_DWC2_TCC_MUX)
	/* get otg control cfg register */
	writel(0x0, &dwc_otg_pcfg->otgmux);
	udelay(10);
	mux_cfg_val = readl(&dwc_otg_pcfg->otgmux);
	BITSET(mux_cfg_val,
			TCC_MUX_OPSEL | TCC_MUX_O_SELECT | TCC_MUX_H_SELECT);
	writel(mux_cfg_val, &dwc_otg_pcfg->otgmux);
#endif

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
	defined(CONFIG_ARCH_TCC901X) ||					\
	defined(CONFIG_ARCH_TCC805X)
	writel(0x83000025U, &dwc_otg_pcfg->pcfg0);
#else
	dwc_otg_pcfg->pcfg0 = 0x83000015U;
#endif

#if defined(CONFIG_ARCH_TCC897X)
	dwc_otg_pcfg->pcfg1 = 0x0330d643;
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
	defined(CONFIG_ARCH_TCC901X) ||					\
	defined(CONFIG_ARCH_TCC805X)
	writel(0xE31C243AU, &dwc_otg_pcfg->pcfg1);
#else
	dwc_otg_pcfg->pcfg1 = 0x0330d645;
#endif

#ifdef CONFIG_TCC_EH_ELECT_TST
	dwc_otg_pcfg->pcfg2 = 0x00000004 | USBOTG_PCFG2_ACAENB;
#else
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
	defined(CONFIG_ARCH_TCC901X) ||					\
	defined(CONFIG_ARCH_TCC805X)
	writel(0x75852004, &dwc_otg_pcfg->pcfg2);
#else
	dwc_otg_pcfg->pcfg2 = 0x00000004;
#endif
#endif /* CONFIG_TCC_EH_ELECT_TST */

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
	defined(CONFIG_ARCH_TCC901X) ||					\
	defined(CONFIG_ARCH_TCC805X)
	writel(0x00000000, &dwc_otg_pcfg->pcfg3);
	writel(0x00000000, &dwc_otg_pcfg->pcfg4);
	writel(0x30000000, &dwc_otg_pcfg->lcfg0);

	// Set the POR
	writel(readl(&dwc_otg_pcfg->pcfg0) | Hw31, &dwc_otg_pcfg->pcfg0);
	// Set the Core Reset
	writel(readl(&dwc_otg_pcfg->lcfg0) & 0x00000000U, &dwc_otg_pcfg->lcfg0);

	udelay(30);

	// Release POR
	writel(readl(&dwc_otg_pcfg->pcfg0) & ~Hw31, &dwc_otg_pcfg->pcfg0);
	// Clear SIDDQ
	writel(readl(&dwc_otg_pcfg->pcfg0) & ~Hw24, &dwc_otg_pcfg->pcfg0);
	// Set Phyvalid en
	writel(readl(&dwc_otg_pcfg->pcfg0) | Hw20, &dwc_otg_pcfg->pcfg0);
	// Set DP/DM (pull down)
	writel(readl(&dwc_otg_pcfg->pcfg4) | 0x1400U, &dwc_otg_pcfg->pcfg4);

	// Wait Phy Valid Interrupt
	i = 0;

	while (i < 10000) {
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
		defined(CONFIG_ARCH_TCC901X) ||				\
		defined(CONFIG_ARCH_TCC805X)
		if ((readl(&dwc_otg_pcfg->pcfg4) & Hw27) != 0U) {
			break;
		}
#else
		if ((readl(&dwc_otg_pcfg->pcfg0) & Hw21) != 0U) {
			break;
		}
#endif
		i++;
		udelay(5);
	}

	dev_info(dwc_otg_phy_dev->dev, "[INFO][USB] OTG PHY valid check %s\n",
			(i >= 9999) ? "fail!" : "pass.");

	// disable PHYVALID_EN -> no irq
	writel(readl(&dwc_otg_pcfg->pcfg0) & ~Hw20, &dwc_otg_pcfg->pcfg0);
	writel(readl(&dwc_otg_pcfg->pcfg0) & ~Hw25, &dwc_otg_pcfg->pcfg0);

	// Release Core Reset
	writel(readl(&dwc_otg_pcfg->lcfg0) | 0x30000000U, &dwc_otg_pcfg->lcfg0);
#else
	dwc_otg_pcfg->pcfg3 = 0x00000000;
	dwc_otg_pcfg->pcfg4 = 0x00000000;
	dwc_otg_pcfg->lcfg0 = 0x30000000;
	dwc_otg_pcfg->lcfg0 = 0x00000000; // assert prstn, adp_reset_n
#if defined(USBPHY_IGNORE_VBUS_COMPARATOR)
	// forced that VBUS status is valid always.
	dwc_otg_pcfg->lcfg0 |= (0x3U << 21U);
#endif
	//disable PHYVALID_EN -> no irq, SIDDQ, POR
	BITCLR(dwc_otg_pcfg->pcfg0,
			((1 << 20) | (1 << 24) | (1 << 25) | (1 << 31)));

	usleep_range(10000, 20000);

	BITSET(dwc_otg_pcfg->lcfg0, (1 << 29));	// prstn; (1<<29)
#endif

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
	tcc_dwc_otg_set_dc_level(phy, CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
	dev_info(dwc_otg_phy_dev->dev, "[INFO][USB] pcfg1: 0x%x txvrt: 0x%x\n",
			dwc_otg_pcfg->pcfg1, CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
#endif

	if (phy->otg != NULL) {
		phy->otg->c_mode = (int32_t)USBPHY_MODE_RESET;
	}

	return 0;
}

// PHY Power-on Reset; (1:Reset)(0:Normal)
#define USBOTG_PCFG0_PHY_POR	(Hw31)
// IDDQ Test Enable; (1:enable)(0:disable)
#define USBOTG_PCFG0_SDI	(Hw24)

/* Phy start/stop */
static int32_t tcc_dwc_otg_set_phy_state(struct usb_phy *phy, int32_t state)
{
	struct tcc_dwc_otg_device *dwc_otg_phy_dev =
	    container_of(phy, struct tcc_dwc_otg_device, phy);
	struct dwc_otg_phy_reg *dwc_otg_pcfg;

	if (dwc_otg_phy_dev == NULL) {
		return -ENODEV;
	}

	dwc_otg_pcfg = (struct dwc_otg_phy_reg *)dwc_otg_phy_dev->base;

	if (state == (int32_t)USBPHY_MODE_START) {
		BITCLR(dwc_otg_pcfg->pcfg0,
		       (USBOTG_PCFG0_PHY_POR | USBOTG_PCFG0_SDI));
		dev_info(dwc_otg_phy_dev->dev, "[INFO][USB] dwc_otg PHY start\n");
		phy->otg->c_mode = (int32_t)USBPHY_MODE_ON;
	} else if (state == (int32_t)USBPHY_MODE_STOP) {
		BITSET(dwc_otg_pcfg->pcfg0,
		       (USBOTG_PCFG0_PHY_POR | USBOTG_PCFG0_SDI));
		dev_info(dwc_otg_phy_dev->dev, "[INFO][USB] dwc_otg PHY stop\n");
		phy->otg->c_mode = (int32_t)USBPHY_MODE_OFF;
	} else {
		dev_info(dwc_otg_phy_dev->dev, "[INFO][USB] [%s:%d]bad argument\n",
		       __func__, __LINE__);
		phy->otg->c_mode = -1;
	}

	return phy->otg->c_mode;
}

static int32_t tcc_dwc_otg_set_vbus_resource(struct usb_phy *phy)
{
	struct tcc_dwc_otg_device *phy_dev =
		container_of(phy, struct tcc_dwc_otg_device, phy);
	struct device *dev = phy_dev->dev;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0) != NULL) {
		/*
		 * Get the vbus regulator declared in the
		 * "vbus-supply" property for the USB PHY driver node.
		 */
		if (phy_dev->vbus_supply)
			goto out;

		phy_dev->vbus_supply = devm_regulator_get_optional(dev, "vbus");
		if (IS_ERR(phy_dev->vbus_supply)) {
			dev_err(dev, "[ERROR][USB] VBus Supply is not valid.\n");
			return -ENODEV;
		}
	} else {
		dev_info(dev, "[INFO][USB] vbus-ctrl-able property is not declared in device tree.\n");
	}

out:

	return 0;
}

/* create phy struct */
static int32_t tcc_dwc_otg_create_phy(struct device *dev,
		struct tcc_dwc_otg_device *phy_dev)
{
	int32_t retval = 0;

	if (phy_dev == NULL) {
		return -ENODEV;
	}

	phy_dev->phy.otg =
		devm_kzalloc(dev, sizeof(*phy_dev->phy.otg), GFP_KERNEL);

	if (phy_dev->phy.otg == NULL) {
		return -ENOMEM;
	}

	phy_dev->dev				= dev;

	phy_dev->phy.dev			= phy_dev->dev;
	phy_dev->phy.label			= "tcc_dwc_otg_phy";
	phy_dev->phy.state			= OTG_STATE_UNDEFINED;
	phy_dev->phy.type			= USB_PHY_TYPE_USB2;
	phy_dev->phy.init			= &tcc_dwc_otg_phy_init;
	phy_dev->phy.set_phy_state		= &tcc_dwc_otg_set_phy_state;
	phy_dev->phy.set_vbus_resource	= &tcc_dwc_otg_set_vbus_resource;
	phy_dev->phy.set_vbus			= &dwc_otg_vbus_set;
	phy_dev->phy.get_base			= &tcc_dwc_otg_get_base;
#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
	phy_dev->phy.get_dc_voltage_level	= &tcc_dwc_otg_get_dc_level;
	phy_dev->phy.set_dc_voltage_level	= &tcc_dwc_otg_set_dc_level;
#endif
	phy_dev->phy.otg->usb_phy		= &phy_dev->phy;
#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X))
	retval = tcc_dwc_otg_set_vbus_resource(&phy_dev->phy);
#endif
	return retval;
}

static int32_t tcc_dwc_otg_phy_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct tcc_dwc_otg_device *phy_dev;
	int32_t retval;

	if (pdev == NULL) {
		return -ENODEV;
	}

	dev = &pdev->dev;

	dev_info(dev, "[INFO][USB] %s:%s\n", pdev->dev.kobj.name, __func__);
	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);

	retval = tcc_dwc_otg_create_phy(dev, phy_dev);
	if (retval != 0) {
		return retval;
	}

	if (request_mem_region(pdev->resource[0].start,
				pdev->resource[0].end -
				pdev->resource[0].start + 1,
				"dwc_otg_phy") == NULL) {
		dev_dbg(&pdev->dev, "[DEBUG][USB] error reserving mapped memory\n");
		retval = -EFAULT;
	}

	phy_dev->base = (void __iomem *)ioremap_nocache(
			(resource_size_t)pdev->resource[0].start,
			pdev->resource[0].end - pdev->resource[0].start + 1U);
	phy_dev->phy.base = phy_dev->base;

	platform_set_drvdata(pdev, phy_dev);

	retval = usb_add_phy_dev(&phy_dev->phy);
	if (retval != 0) {
		dev_err(&pdev->dev, "[ERROR][USB] usb_add_phy failed\n");
		return retval;
	}

	return retval;
}

static int32_t tcc_dwc_otg_phy_remove(struct platform_device *pdev)
{
	struct tcc_dwc_otg_device *phy_dev = platform_get_drvdata(pdev);

	usb_remove_phy(&phy_dev->phy);

	return 0;
}

static const struct of_device_id tcc_dwc_otg_phy_match[] = {
	{ .compatible = "telechips,tcc_dwc_otg_phy" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcc_dwc_otg_phy_match);

static struct platform_driver tcc_dwc_otg_phy_driver = {
	.probe  = tcc_dwc_otg_phy_probe,
	.remove = tcc_dwc_otg_phy_remove,
	.driver = {
		.name           = "dwc_otg_phy",
		.owner          = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(tcc_dwc_otg_phy_match),
#endif
	},
};

static int32_t __init tcc_dwc_otg_phy_drv_init(void)
{
	int32_t retval = 0;

	retval = platform_driver_register(&tcc_dwc_otg_phy_driver);
	if (retval < 0) {
		pr_err("[ERROR][USB] %s retval=%d\n", __func__, retval);
	}

	return retval;
}
core_initcall(tcc_dwc_otg_phy_drv_init);

static void __exit tcc_dwc_otg_phy_cleanup(void)
{
	platform_driver_unregister(&tcc_dwc_otg_phy_driver);
}
module_exit(tcc_dwc_otg_phy_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TCC USB transceiver driver");
