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
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_gpio.h>
#include <dt-bindings/gpio/gpio.h>
#include "../host/tcc-hcd.h"

#define ON      1
#define OFF     0

#ifndef BITSET
#define BITSET(X, MASK)                 ((X) |= (unsigned int)(MASK))
#define BITCLR(X, MASK)                 ((X) &= ~((unsigned int)(MASK)))
#define BITXOR(X, MASK)                 ((X) ^= (unsigned int)(MASK))
#define BITCSET(X, CMASK, SMASK)        ((X) = ((((unsigned int)(X)) &  \
					~((unsigned int)(CMASK))) |     \
					((unsigned int)(SMASK))))
#define BITSCLR(X, SMASK, CMASK)        ((X) = ((((unsigned int)(X)) |  \
					((unsigned int)(SMASK))) &      \
					~((unsigned int)(CMASK))))
#define ISZERO(X, MASK)                 (!(((unsigned int)(X)) &        \
					((unsigned int)(MASK))))
#define ISSET(X, MASK)                  ((unsigned long)(X) &           \
					((unsigned long)(MASK)))
#endif

struct tcc_ehci_device {
	struct device *dev;
	void __iomem *base;	/* Phy base address */
	struct usb_phy phy;
	struct clk *hclk;
	struct clk *isol;
	int mux_port;
	int vbus_gpio_num;
	unsigned long vbus_gpio_flag;
#if defined(CONFIG_TCC_BC_12)
	int irq;
#endif
};

struct ehci_phy_reg {
	volatile uint32_t bcfg;
	volatile uint32_t pcfg0;
	volatile uint32_t pcfg1;
	volatile uint32_t pcfg2;
	volatile uint32_t pcfg3;
	volatile uint32_t pcfg4;
	volatile uint32_t sts;
	volatile uint32_t lcfg0;
	volatile uint32_t lcfg1;
};

static void __iomem *tcc_ehci_get_base(struct usb_phy *phy)
{
	struct tcc_ehci_device *phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);

	return phy_dev->base;
}

/*
 * TOP Isolateion Control function
 */
static int tcc_ehci_phy_isolation(struct usb_phy *phy, int on_off)
{
	struct tcc_ehci_device *phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);

	if (phy_dev->isol == NULL) {
		return -1;
	}

	if (on_off == ON) {
		if (clk_prepare_enable(phy_dev->isol) != 0) {
			dev_err(phy_dev->dev, "[ERROR][USB] can't do usb 2.0 phy enable\n");
		}
	} else if (on_off == OFF) {
		clk_disable_unprepare(phy_dev->isol);
	} else {
		dev_err(phy_dev->dev, "[INFO][USB] bad argument\n");
	}

	return 0;
}

/*
 * Set vbus function
 */
static int tcc_ehci_vbus_set(struct usb_phy *phy, int on_off)
{
	struct tcc_ehci_device *phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct device *dev = phy_dev->dev;
	int retval = 0;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0) == NULL) {
		dev_err(dev, "[ERROR][USB] vbus-ctrl-able property is not declared in device tree.\n");
		return -ENODEV;
	}

	/* Check if the GPIO pin number is within the valid range(0 to 511). */
	if (!gpio_is_valid(phy_dev->vbus_gpio_num)) {
		dev_err(dev, "[ERROR][USB] VBus GPIO pin number is not valid number, errno %d.\n",
				phy_dev->vbus_gpio_num);
		return phy_dev->vbus_gpio_num;
	}

	/* Request a single VBus GPIO with initial configuration. */
	retval = gpio_request_one((unsigned int)phy_dev->vbus_gpio_num,
			phy_dev->vbus_gpio_flag, "vbus_gpio_phy");

	if (retval != 0) {
		dev_err(dev, "[ERROR][USB] VBus GPIO can't be requested, errno %d.\n",
				retval);
		return retval;
	}

	/*
	 * Set the direction of the VBus GPIO passed through the phy_dev
	 * structure to output.
	 */
	retval = gpiod_direction_output(
			gpio_to_desc((unsigned int)phy_dev->vbus_gpio_num),
			on_off);

	if (retval != 0) {
		dev_err(dev, "[ERROR][USB] VBus GPIO direction can't be set to output errno %d.\n",
				retval);
		return retval;
	}

	gpio_free((unsigned int)phy_dev->vbus_gpio_num);

	return retval;
}

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
#define PCFG1_TXVRT_MASK	0xF
#define PCFG1_TXVRT_SHIFT	0x0
#define get_txvrt(x)		((x & PCFG1_TXVRT_MASK) >> PCFG1_TXVRT_SHIFT)

static int tcc_ehci_get_dc_level(struct usb_phy *phy)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg =
		(struct ehci_phy_reg *)ehci_phy_dev->base;
	uint32_t pcfg1_val;

	pcfg1_val = readl(&ehci_pcfg->pcfg1);

	return get_txvrt(pcfg1_val);
}

static int tcc_ehci_set_dc_level(struct usb_phy *phy, unsigned int level)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg =
		(struct ehci_phy_reg *)ehci_phy_dev->base;
	uint32_t pcfg1_val;

	pcfg1_val = readl(&ehci_pcfg->pcfg1);
	BITCSET(pcfg1_val, PCFG1_TXVRT_MASK, level << PCFG1_TXVRT_SHIFT);
	writel(pcfg1_val, &ehci_pcfg->pcfg1);

	dev_info(ehci_phy_dev->dev, "[INFO][USB] cur dc level = %d\n",
			get_txvrt(pcfg1_val));

	return 0;
}
#endif

#if defined(CONFIG_TCC_BC_12)
static void tcc_ehci_set_chg_det(struct usb_phy *phy)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg =
		(struct ehci_phy_reg *)ehci_phy_dev->base;

	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) | (1 << 31), &ehci_pcfg->pcfg4);
	udelay(1);

	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) & ~(1 << 31), &ehci_pcfg->pcfg4);

	// enable chg det
	writel(readl(&ehci_pcfg->pcfg2) | (1 << 8), &ehci_pcfg->pcfg2);
}

static irqreturn_t chg_irq(int irq, void *data)
{
	struct tcc_ehci_device *phy_dev = (struct tcc_ehci_device *)data;
	struct ehci_phy_reg *ehci_pcfg = (struct ehci_phy_reg *)phy_dev->base;
	uint32_t pcfg2 = 0;

	dev_info(phy_dev->dev, "[INFO][USB] Charging Detection!\n");

	pcfg2 = readl(&ehci_pcfg->pcfg2);
	writel((pcfg2 | (1 << 9)), &ehci_pcfg->pcfg2);
	mdelay(50);
	writel(readl(&ehci_pcfg->pcfg2) & ~((1 << 9) | (1 << 8)),
			&ehci_pcfg->pcfg2);

	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) | (1 << 31), &ehci_pcfg->pcfg4);
	udelay(1);

	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) & ~(1 << 31), &ehci_pcfg->pcfg4);

	return IRQ_HANDLED;
}
#endif

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

#define MUX_MODE_HOST		0
#define MUX_MODE_DEVICE		1

static int tcc_ehci_phy_init(struct usb_phy *phy)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg =
		(struct ehci_phy_reg *)ehci_phy_dev->base;
	uint32_t mux_cfg_val;
	int i;

	if (ehci_phy_dev->mux_port != 0) {
		/* get otg control cfg register */
		mux_cfg_val = readl(phy->otg->mux_cfg_addr);
		BITCSET(mux_cfg_val, TCC_MUX_OPSEL, TCC_MUX_H_SELECT);
		writel(mux_cfg_val, phy->otg->mux_cfg_addr);
	}

	if (!__clk_is_enabled(ehci_phy_dev->hclk)) {
		clk_enable(ehci_phy_dev->hclk);
	}

	clk_reset(ehci_phy_dev->hclk, 1);

	// Reset PHY Registers
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
	defined(CONFIG_ARCH_TCC901X) ||	defined(CONFIG_ARCH_TCC805X)
	writel(0x83000025, &ehci_pcfg->pcfg0);

	if (ehci_phy_dev->mux_port != 0) {
		// EHCI MUX Host PHY Configuration
		writel(0xE31C243A, &ehci_pcfg->pcfg1);
	} else {
		// EHCI PHY Configuration
		writel(0xE31C243A, &ehci_pcfg->pcfg1);
	}
#else
	writel(0x03000115, &ehci_pcfg->pcfg0);

	if (ehci_phy_dev->mux_port != 0) {
		// EHCI MUX Host PHY Configuration
		writel(0x0334D175, &ehci_pcfg->pcfg1);
	} else {
		// EHCI PHY Configuration
		writel(0x0334D175, &ehci_pcfg->pcfg1);
	}
#endif

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
	defined(CONFIG_ARCH_TCC901X) || defined(CONFIG_ARCH_TCC805X)
	writel(0x00000000, &ehci_pcfg->pcfg2);
#else
	writel(0x00000004, &ehci_pcfg->pcfg2);
#endif
	writel(0x00000000, &ehci_pcfg->pcfg3);
	writel(0x00000000, &ehci_pcfg->pcfg4);
	writel(0x30048020, &ehci_pcfg->lcfg0);

	// Set the POR
	writel(readl(&ehci_pcfg->pcfg0) | Hw31, &ehci_pcfg->pcfg0);
	// Set the Core Reset
	writel(readl(&ehci_pcfg->lcfg0) & 0xCFFFFFFF, &ehci_pcfg->lcfg0);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	// Clear SIDDQ
	writel(readl(&ehci_pcfg->pcfg0) & ~Hw24, &ehci_pcfg->pcfg0);
#endif

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||	\
	defined(CONFIG_ARCH_TCC901X) || defined(CONFIG_ARCH_TCC805X)
	// Wait 30 usec
	udelay(30);
#else
	// Wait 10 usec
	udelay(10);
#endif

	// Release POR
	writel(readl(&ehci_pcfg->pcfg0) & ~Hw31, &ehci_pcfg->pcfg0);
#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X))
	// Clear SIDDQ, moved it before Release POR for stability
	writel(readl(&ehci_pcfg->pcfg0) & ~(1<<24), &ehci_pcfg->pcfg0);
#endif
	// Set Phyvalid en
	writel(readl(&ehci_pcfg->pcfg4) | Hw30, &ehci_pcfg->pcfg4);
	// Set DP/DM (pull down)
	writel(readl(&ehci_pcfg->pcfg4) | 0x1400U, &ehci_pcfg->pcfg4);

	clk_reset(ehci_phy_dev->hclk, 0);

	// Wait Phy Valid Interrupt
	i = 0;

	while (i < 10000) {
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
		defined(CONFIG_ARCH_TCC901X) ||				\
		defined(CONFIG_ARCH_TCC805X)
		if ((readl(&ehci_pcfg->pcfg4) & Hw27) != 0U) {
			break;
		}
#else
		if ((readl(&ehci_pcfg->pcfg0) & Hw21) != 0U) {
			break;
		}
#endif
		i++;
		udelay(5);
	}
	dev_info(ehci_phy_dev->dev, "[INFO][USB] EHCI PHY valid check %s\x1b[0m\n",
			(i >= 9999) ? "fail!" : "pass.");

	// Release Core Reset
	writel(readl(&ehci_pcfg->lcfg0) | 0x30000000U, &ehci_pcfg->lcfg0);

#if defined(CONFIG_USB_HS_DC_VOLTAGE_LEVEL)
	phy->set_dc_voltage_level(phy, CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
#endif

#if defined(CONFIG_TCC_BC_12)
	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) | (1 << 31), &ehci_pcfg->pcfg4);
	dev_info(ehci_phy_dev->dev, "[INFO][USB] %s : Not Mux host\n",
			__func__);

	// Disable VBUS Detect
	writel(readl(&ehci_pcfg->pcfg4) & ~(1 << 30), &ehci_pcfg->pcfg4);

	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) & ~(1 << 31), &ehci_pcfg->pcfg4);
	udelay(1);

	writel(readl(&ehci_pcfg->pcfg2) | ((1 << 8) | (1 << 10)),
			&ehci_pcfg->pcfg2);
	udelay(1);

	// enable CHG_DET interrupt
	writel(readl(&ehci_pcfg->pcfg4) | (1 << 28), &ehci_pcfg->pcfg4);

	enable_irq(ehci_phy_dev->irq);
#endif
	return 0;
}

#define USB20_PCFG0_PHY_POR     (Hw31)
#define USB20_PCFG0_PHY_SIDDQ   (Hw24)

static int tcc_ehci_phy_state_set(struct usb_phy *phy, int on_off)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg =
		(struct ehci_phy_reg *)ehci_phy_dev->base;

	if (on_off == ON) {
		BITCLR(ehci_pcfg->pcfg0,
				(USB20_PCFG0_PHY_POR | USB20_PCFG0_PHY_SIDDQ));
		dev_info(ehci_phy_dev->dev, "[INFO][USB] EHCI PHY start\n");
	} else if (on_off == OFF) {
		BITSET(ehci_pcfg->pcfg0,
				(USB20_PCFG0_PHY_POR | USB20_PCFG0_PHY_SIDDQ));
		dev_info(ehci_phy_dev->dev, "[INFO][USB] EHCI PHY stop\n");
	} else {
		/* Nothing to do */
	}

	dev_info(ehci_phy_dev->dev, "[INFO][USB] EHCI PHY pcfg0 : %08X\n",
			ehci_pcfg->pcfg0);
	return 0;
}

static void tcc_ehci_phy_mux_sel(struct usb_phy *phy, int is_mux)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	uint32_t mux_cfg_val;

	if (ehci_phy_dev->mux_port != 0) {
		mux_cfg_val = readl(phy->otg->mux_cfg_addr);

		if (is_mux != 0) {
			BITCSET(mux_cfg_val, TCC_MUX_OPSEL, TCC_MUX_H_SELECT);
			writel(mux_cfg_val, phy->otg->mux_cfg_addr);
		} else {
			BITSET(mux_cfg_val, TCC_MUX_OPSEL | TCC_MUX_O_SELECT |
					TCC_MUX_H_SELECT);
			writel(mux_cfg_val, phy->otg->mux_cfg_addr);
		}
	}
}

static int tcc_ehci_phy_set_vbus_resource(struct usb_phy *phy)
{
	struct tcc_ehci_device *phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct device *dev = phy_dev->dev;
	unsigned int gpio_flag;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0) != NULL) {
		/*
		 * Get the GPIO pin number and GPIO flag declared in the
		 * "vbus-gpio" property for the USB PHY driver node.
		 */
		phy_dev->vbus_gpio_num = of_get_named_gpio_flags(dev->of_node,
				"vbus-gpio", 0, &gpio_flag);

		/*
		 * Check if the GPIO pin number is within the valid range(0 to
		 * 511).
		 */
		if (gpio_is_valid(phy_dev->vbus_gpio_num)) {
			/*
			 * The GPIO pin number and GPIO flag are declared in the
			 * "vbus-gpio" property for the USB PHY driver node, as
			 * in the example below.
			 *
			 * e.g.)
			 *     dwc_otg_phy@11DA0100
			 *         ...
			 *         vbus-gpio = <&gpa 27 0>;
			 *
			 * The 3rd argument of the "vbus-gpio" property, which
			 * means the GPIO flag, is declared as default 0 or
			 * GPIO_ACTIVE_HIGH for the TCC USB PHY driver. However,
			 * depending on the hardware configuration, it can be
			 * declared as 1 or GPIO_ACTIVE_LOW. If so, the value of
			 * GPIOF_ACTIVE_LOW is stored in the phy_dev structure
			 * so that it can be used in the set_vbus(). The reason
			 * for saving GPIOF_ACTIVE_LOW rather than
			 * GPIO_ACTIVE_LOW in the phy_dev structure is that the
			 * gpio_request_one() used in the set_vbus() is a legacy
			 * GPIO function using the value defined in linux/gpio.h
			 */
			if (gpio_flag == (unsigned int)GPIO_ACTIVE_LOW) {
				phy_dev->vbus_gpio_flag = GPIOF_ACTIVE_LOW;
			}

			dev_dbg(dev, "[DEBUG][USB] VBus GPIO pin number is %d\n",
					phy_dev->vbus_gpio_num);
			dev_dbg(dev, "[DEBUG][USB] VBus GPIO flag is %s\n",
					gpio_flag ?
					"active low(=1)" : "active high(=0)");
		} else {
			dev_err(dev, "[ERROR][USB] VBus GPIO pin number is not valid number, errno %d.\n",
					phy_dev->vbus_gpio_num);
			return phy_dev->vbus_gpio_num;
		}
	} else {
		dev_info(dev, "[INFO][USB] vbus-ctrl-able property is not declared in device tree.\n");
	}

	return 0;
}

static int tcc_ehci_create_phy(struct device *dev,
		struct tcc_ehci_device *phy_dev)
{
	int retval = 0;

	phy_dev->phy.otg =
		devm_kzalloc(dev, sizeof(*phy_dev->phy.otg), GFP_KERNEL);
	if (phy_dev->phy.otg == NULL) {
		return -ENOMEM;
	}

	phy_dev->mux_port =
		of_find_property(dev->of_node, "mux_port", 0) ? 1 : 0;

	// HCLK
	phy_dev->hclk = of_clk_get(dev->of_node, 0);
	if (IS_ERR(phy_dev->hclk)) {
		phy_dev->hclk = NULL;
	}

	// isol
	phy_dev->isol = of_clk_get(dev->of_node, 1);
	if (IS_ERR(phy_dev->isol)) {
		phy_dev->isol = NULL;
	}

	phy_dev->dev			  = dev;

	phy_dev->phy.dev		  = phy_dev->dev;
	phy_dev->phy.label		  = "tcc_ehci_phy";
	phy_dev->phy.state		  = OTG_STATE_UNDEFINED;
	phy_dev->phy.type		  = USB_PHY_TYPE_USB2;
	phy_dev->phy.init		  = tcc_ehci_phy_init;
	phy_dev->phy.set_phy_isol	  = tcc_ehci_phy_isolation;
	phy_dev->phy.set_phy_state	  = tcc_ehci_phy_state_set;
	phy_dev->phy.set_phy_mux_sel	  = tcc_ehci_phy_mux_sel;
	phy_dev->phy.set_vbus_resource	  = tcc_ehci_phy_set_vbus_resource;
	phy_dev->phy.set_vbus		  = tcc_ehci_vbus_set;
	phy_dev->phy.get_base		  = tcc_ehci_get_base;
#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
	phy_dev->phy.get_dc_voltage_level = tcc_ehci_get_dc_level;
	phy_dev->phy.set_dc_voltage_level = tcc_ehci_set_dc_level;
#endif
#if defined(CONFIG_TCC_BC_12)
	phy_dev->phy.set_chg_det	  = tcc_ehci_set_chg_det;
#endif
	phy_dev->phy.otg->usb_phy	  = &phy_dev->phy;

#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X))
	retval = tcc_ehci_phy_set_vbus_resource(&phy_dev->phy);
#endif
	return retval;
}

static int tcc_ehci_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_ehci_device *phy_dev;
	int retval;
#if defined(CONFIG_TCC_BC_12)
	int irq, ret = 0;
#endif
	dev_info(dev, "[INFO][USB] %s:%s\n", pdev->dev.kobj.name, __func__);
	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);

	retval = tcc_ehci_create_phy(dev, phy_dev);
	if (retval != 0) {
		return retval;
	}
#if defined(CONFIG_TCC_BC_12)
	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
				dev_name(&pdev->dev));
		retval = -ENODEV;
	} else {
		dev_info(dev, "[INFO][USB] %s: irq=%d\n", __func__, irq);
		phy_dev->irq = irq;
	}
#endif
	if (request_mem_region(pdev->resource[0].start,
				pdev->resource[0].end -
				pdev->resource[0].start + 1,
				"ehci_phy") == NULL) {
		dev_dbg(&pdev->dev, "[DEBUG][USB] error reserving mapped memory\n");
		retval = -EFAULT;
	}

	phy_dev->base = (void __iomem *)ioremap_nocache(
			(resource_size_t)pdev->resource[0].start,
			pdev->resource[0].end - pdev->resource[0].start + 1);

	phy_dev->phy.base = phy_dev->base;
#if defined(CONFIG_TCC_BC_12)
	ret = devm_request_irq(&pdev->dev, phy_dev->irq, chg_irq,
			IRQF_SHARED, pdev->dev.kobj.name, phy_dev);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] request irq failed\n");
	}

	disable_irq(phy_dev->irq);
#endif
	platform_set_drvdata(pdev, phy_dev);

	retval = usb_add_phy_dev(&phy_dev->phy);
	if (retval != 0) {
		dev_err(&pdev->dev, "[ERROR][USB] usb_add_phy failed\n");
		return retval;
	}

	return retval;
}

static int tcc_ehci_phy_remove(struct platform_device *pdev)
{
	struct tcc_ehci_device *phy_dev = platform_get_drvdata(pdev);

	usb_remove_phy(&phy_dev->phy);

	return 0;
}

static const struct of_device_id tcc_ehci_phy_match[] = {
	{ .compatible = "telechips,tcc_ehci_phy" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcc_ehci_phy_match);

static struct platform_driver tcc_ehci_phy_driver = {
	.probe	= tcc_ehci_phy_probe,
	.remove = tcc_ehci_phy_remove,
	.driver = {
		.name		= "ehci_phy",
		.owner		= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(tcc_ehci_phy_match),
#endif
	},
};

static int __init tcc_ehci_phy_drv_init(void)
{
	int retval = 0;

	retval = platform_driver_register(&tcc_ehci_phy_driver);
	if (retval < 0) {
		pr_err("[ERROR][USB] %s retval=%d\n", __func__, retval);
	}

	return retval;
}
core_initcall(tcc_ehci_phy_drv_init);

static void __exit tcc_ehci_phy_cleanup(void)
{
	platform_driver_unregister(&tcc_ehci_phy_driver);
}
module_exit(tcc_ehci_phy_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TCC USB transceiver driver");
