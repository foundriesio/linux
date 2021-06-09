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
#include "../host/tcc-hcd.h"
#include <linux/kthread.h>

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

struct tcc_ehci_device {
	struct device *dev;
	void __iomem *base;	/* Phy base address */
	struct usb_phy phy;
	int32_t mux_port;
	struct regulator *vbus_supply;
#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
	int irq;
	struct task_struct *ehci_chgdet_thread;
	struct work_struct chgdet_work;
	bool chg_ready;
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

	if (phy_dev == NULL) {
		return (void __iomem *)0;
	}

	return phy_dev->base;
}

/*
 * Set vbus function
 */
static int32_t tcc_ehci_vbus_set(struct usb_phy *phy, int32_t on_off)
{
	struct tcc_ehci_device *phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
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

static int tcc_ehci_set_dc_level(struct usb_phy *phy, uint32_t level)
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

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
static int ehci_chgdet_thread(void *work)
{
	struct tcc_ehci_device *phy_dev = (struct tcc_ehci_device *)work;
	struct ehci_phy_reg *ehci_pcfg = (struct ehci_phy_reg *)phy_dev->base;
	uint32_t pcfg2 = 0;

	int timeout = 1000;

	dev_dbg(phy_dev->dev, "[DEBUG][USB]Start to check CHGDET\n");
	while (!kthread_should_stop() && timeout > 0) {
		pcfg2 = readl(&ehci_pcfg->pcfg2);
		usleep_range(1000,1100);
		timeout--;
	}

	if ((timeout <= 0)/* || (chgdet == 1)*/) {
		writel(readl(&ehci_pcfg->pcfg2) | (1 << 8), &ehci_pcfg->pcfg2);
		writel(readl(&ehci_pcfg->pcfg4) | (1 << 28), &ehci_pcfg->pcfg4);
		dev_dbg(phy_dev->dev, "[DEBUG][USB]Enable VDATDETENB\n");
	}

	phy_dev->ehci_chgdet_thread = NULL;

	dev_dbg(phy_dev->dev, "[DEBUG][USB]Monitoring is finished(%d)\n",
			timeout);
	return 0;

}

static void chgdet_monitor(struct work_struct *data)
{
	struct tcc_ehci_device *phy_dev = container_of(data,
			struct tcc_ehci_device, chgdet_work);

	struct ehci_phy_reg *ehci_pcfg = (struct ehci_phy_reg *)phy_dev->base;
	uint32_t pcfg2 = 0;
	int32_t timeout_count = 500;

	phy_dev->chg_ready = true;
	pcfg2 = readl(&ehci_pcfg->pcfg2);

	writel((pcfg2 | (1 << 9)), &ehci_pcfg->pcfg2);

	while (timeout_count > 0) {
		pcfg2 = readl(&ehci_pcfg->pcfg2);
		if ((pcfg2 & (1 << 22)) != 0) { //Check VDP_SRC signal
			usleep_range(1000,1100);
			timeout_count--;
		} else {
			break;
		}
	}

	if (timeout_count == 0) {
		dev_dbg(phy_dev->dev, "[DEBUG][USB]Timeout - VDM_SRC is still High\n");
	} else {
		dev_dbg(phy_dev->dev, "[DEBUG][USB]Time count(%d) - VDM_SRC is low\n",
				(500-timeout_count));
	}

	writel(readl(&ehci_pcfg->pcfg2) & ~((1 << 9) | (1 << 8)),
			&ehci_pcfg->pcfg2);

	if (phy_dev->chg_ready == true) {
		dev_dbg(phy_dev->dev, "[DEBUG][USB]Enable chg det monitor!\n");
		if (phy_dev->ehci_chgdet_thread != NULL) {
			kthread_stop(phy_dev->ehci_chgdet_thread);
			phy_dev->ehci_chgdet_thread = NULL;
		}
		dev_dbg(phy_dev->dev, "[DEBUG][USB]start chg det thread!\n");
		phy_dev->ehci_chgdet_thread = kthread_run(ehci_chgdet_thread,
				(void *)phy_dev, "ehci-chgdet");
		if (IS_ERR(phy_dev->ehci_chgdet_thread)) {
			dev_err(phy_dev->dev, "[ERROR][USB]failed to run ehci_chgdet_thread\n");
		}
	} else {
		dev_dbg(phy_dev->dev, "[DEBUG][USB]No need to start chg det monitor\n");
	}
}

static void tcc_ehci_set_chg_det(struct usb_phy *phy)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg =
		(struct ehci_phy_reg *)ehci_phy_dev->base;

	dev_dbg(ehci_phy_dev->dev, "[DEBUG][USB]start chg det!\n");
	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) | (1 << 31), &ehci_pcfg->pcfg4);
	udelay(1);

	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) & ~(1 << 31), &ehci_pcfg->pcfg4);

	// enable chg det
	writel(readl(&ehci_pcfg->pcfg2) | (1 << 8), &ehci_pcfg->pcfg2);
	writel(readl(&ehci_pcfg->pcfg4) | (1 << 28), &ehci_pcfg->pcfg4);
}

static void tcc_ehci_stop_chg_det(struct usb_phy *phy)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg =
		(struct ehci_phy_reg *)ehci_phy_dev->base;

	ehci_phy_dev->chg_ready = false;

	dev_dbg(ehci_phy_dev->dev, "[DEBUG][USB]stop chg det!\n");
	if (ehci_phy_dev->ehci_chgdet_thread != NULL) {
		dev_dbg(ehci_phy_dev->dev, "[DEBUG][USB]kill chg det thread!\n");
		kthread_stop(ehci_phy_dev->ehci_chgdet_thread);
		ehci_phy_dev->ehci_chgdet_thread = NULL;
	}


	dev_dbg(ehci_phy_dev->dev, "[DEBUG][USB]pcfg2= 0x%x/pcfg4=0x%x\n",
			readl(&ehci_pcfg->pcfg2), readl(&ehci_pcfg->pcfg4));
	// disable chg det
	writel(readl(&ehci_pcfg->pcfg4) & ~(1 << 28), &ehci_pcfg->pcfg4);
	writel(readl(&ehci_pcfg->pcfg2) & ~((1 << 9) | (1 << 8)),
			&ehci_pcfg->pcfg2);
	dev_dbg(ehci_phy_dev->dev, "[DEBUG][USB]Disable chg det! pcfg2= 0x%x/pcfg4=0x%x\n",
			readl(&ehci_pcfg->pcfg2), readl(&ehci_pcfg->pcfg4));
}

static irqreturn_t chg_irq(int irq, void *data)
{
	struct tcc_ehci_device *phy_dev = (struct tcc_ehci_device *)data;
	struct ehci_phy_reg *ehci_pcfg = (struct ehci_phy_reg *)phy_dev->base;

	writel(readl(&ehci_pcfg->pcfg4) & ~(1 << 28), &ehci_pcfg->pcfg4);
	dev_dbg(phy_dev->dev, "[DEBUG][USB] Charging Detection!\n");

	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) | (1 << 31), &ehci_pcfg->pcfg4);
	udelay(1);

	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) & ~(1 << 31), &ehci_pcfg->pcfg4);

	schedule_work(&phy_dev->chgdet_work);

	//It's ONLY for PET environment, it affect to normal connection.
	//writel(readl(&ehci_pcfg->pcfg2) |(1<<8),
	//&ehci_pcfg->pcfg2); //enable chg det

	return IRQ_HANDLED;
}
#endif

/* Host Controller in OTG MUX S/W Reset */
#define TCC_MUX_H_SWRST         (uint32_t)(1U << 4U)
/* Host Controller in OTG MUX Clock Enable */
#define TCC_MUX_H_CLKMSK        (uint32_t)(1U << 3U)
/* OTG Controller in OTG MUX S/W Reset */
#define TCC_MUX_O_SWRST         (uint32_t)(1U << 2U)
/* OTG Controller in OTG MUX Clock Enable */
#define TCC_MUX_O_CLKMSK        (uint32_t)(1U << 1U)
/* OTG MUX Controller Select */
#define TCC_MUX_OPSEL           (uint32_t)(1U << 0U)

#define TCC_MUX_O_SELECT	(TCC_MUX_O_SWRST | TCC_MUX_O_CLKMSK)
#define TCC_MUX_H_SELECT	(TCC_MUX_H_SWRST | TCC_MUX_H_CLKMSK)

#define MUX_MODE_HOST           (0)
#define MUX_MODE_DEVICE         (1)

static int32_t tcc_ehci_phy_init(struct usb_phy *phy)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg;
	uint32_t mux_cfg_val;
	int32_t i;

	if (ehci_phy_dev == NULL) {
		return -ENODEV;
	}

	ehci_pcfg = (struct ehci_phy_reg *)ehci_phy_dev->base;

	if (ehci_phy_dev->mux_port != 0) {
		/* get otg control cfg register */
		mux_cfg_val = readl(phy->otg->mux_cfg_addr);
		BITCSET(mux_cfg_val, TCC_MUX_OPSEL, TCC_MUX_H_SELECT);
		writel(mux_cfg_val, phy->otg->mux_cfg_addr);
	}

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
	writel(readl(&ehci_pcfg->lcfg0) & 0xCFFFFFFFU, &ehci_pcfg->lcfg0);

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
	writel(readl(&ehci_pcfg->pcfg0) & (uint32_t)(~(1U<<24U)),
			&ehci_pcfg->pcfg0);
#endif
	// Set Phyvalid en
	writel(readl(&ehci_pcfg->pcfg4) | Hw30, &ehci_pcfg->pcfg4);
	// Set DP/DM (pull down)
	writel(readl(&ehci_pcfg->pcfg4) | 0x1400U, &ehci_pcfg->pcfg4);

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
	dev_info(ehci_phy_dev->dev, "[INFO][USB] EHCI PHY valid check %s\n",
			(i >= 9999) ? "fail!" : "pass.");

	// Release Core Reset
	writel(readl(&ehci_pcfg->lcfg0) | 0x30000000U, &ehci_pcfg->lcfg0);

#if defined(CONFIG_USB_HS_DC_VOLTAGE_LEVEL)
	phy->set_dc_voltage_level(phy, CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
#endif

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
#ifndef CONFIG_ENABLE_BC_20_DRD
	if (ehci_phy_dev->mux_port != 0) {
		dev_info(ehci_phy_dev->dev, "[INFO][USB] Not support BC1.2 for 2.0 DRD port\n");
		return 0;
	}
#endif
#ifndef CONFIG_ENABLE_BC_20_HOST
	if (ehci_phy_dev->mux_port == 0) {
		dev_info(ehci_phy_dev->dev, "[INFO][USB] Not support BC1.2 for 2.0 HOST port\n");
		return 0;
	}
#endif

	dev_info(ehci_phy_dev->dev, "[INFO][USB] %s : Set Charging Detection\n",
			__func__);
	// clear irq
	writel(readl(&ehci_pcfg->pcfg4) | (1 << 31), &ehci_pcfg->pcfg4);

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

	INIT_WORK(&ehci_phy_dev->chgdet_work, chgdet_monitor);

	enable_irq(ehci_phy_dev->irq);
#endif
	return 0;
}

#define USB20_PCFG0_PHY_POR     (Hw31)
#define USB20_PCFG0_PHY_SIDDQ   (Hw24)

static int32_t tcc_ehci_phy_state_set(struct usb_phy *phy, int32_t on_off)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg = NULL;

	if (ehci_phy_dev == NULL) {
		return -ENODEV;
	}

	ehci_pcfg = (struct ehci_phy_reg *)ehci_phy_dev->base;

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

static void tcc_ehci_phy_mux_sel(struct usb_phy *phy, int32_t is_mux)
{
	struct tcc_ehci_device *ehci_phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
	uint32_t mux_cfg_val;

	if (ehci_phy_dev == NULL) {
		return;
	}

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

static int32_t tcc_ehci_phy_set_vbus_resource(struct usb_phy *phy)
{
	struct tcc_ehci_device *phy_dev =
		container_of(phy, struct tcc_ehci_device, phy);
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

static int32_t tcc_ehci_create_phy(struct device *dev,
		struct tcc_ehci_device *phy_dev)
{
	int32_t retval = 0;

	if ((dev == NULL) || (phy_dev == NULL)) {
		return -ENODEV;
	}

	phy_dev->phy.otg =
		devm_kzalloc(dev, sizeof(*phy_dev->phy.otg), GFP_KERNEL);

	if (phy_dev->phy.otg == NULL) {
		return -ENOMEM;
	}

	phy_dev->mux_port =
		(of_find_property(dev->of_node,
			"mux_port", NULL) != NULL) ? 1 : 0;

	phy_dev->dev			  = dev;

	phy_dev->phy.dev		  = phy_dev->dev;
	phy_dev->phy.label		  = "tcc_ehci_phy";
	phy_dev->phy.state		  = OTG_STATE_UNDEFINED;
	phy_dev->phy.type		  = USB_PHY_TYPE_USB2;
	phy_dev->phy.init		  = tcc_ehci_phy_init;
	phy_dev->phy.set_phy_state	  = tcc_ehci_phy_state_set;
	phy_dev->phy.set_phy_mux_sel	  = tcc_ehci_phy_mux_sel;
	phy_dev->phy.set_vbus_resource	  = tcc_ehci_phy_set_vbus_resource;
	phy_dev->phy.set_vbus		  = tcc_ehci_vbus_set;
	phy_dev->phy.get_base		  = tcc_ehci_get_base;
#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
	phy_dev->phy.get_dc_voltage_level = tcc_ehci_get_dc_level;
	phy_dev->phy.set_dc_voltage_level = tcc_ehci_set_dc_level;
#endif
#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
#ifndef CONFIG_ENABLE_BC_20_DRD
	if (phy_dev->mux_port != 0)
		goto skip;
#endif
#ifndef CONFIG_ENABLE_BC_20_HOST
	if (phy_dev->mux_port == 0)
		goto skip;
#endif
	phy_dev->phy.set_chg_det	  = tcc_ehci_set_chg_det;
	phy_dev->phy.stop_chg_det	  = tcc_ehci_stop_chg_det;
#if !defined(CONFIG_ENABLE_BC_20_DRD) || !defined(CONFIG_ENABLE_BC_20_HOST)
skip:
#endif
#endif
	phy_dev->phy.otg->usb_phy	  = &phy_dev->phy;

#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X))
	retval = tcc_ehci_phy_set_vbus_resource(&phy_dev->phy);
#endif
	return retval;
}

static int32_t tcc_ehci_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_ehci_device *phy_dev;
	int32_t retval;
#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
	int32_t irq, ret = 0;
#endif
	dev_info(dev, "[INFO][USB] %s:%s\n", pdev->dev.kobj.name, __func__);
	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);

	retval = tcc_ehci_create_phy(dev, phy_dev);
	if (retval != 0) {
		return retval;
	}
#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
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
			pdev->resource[0].end - pdev->resource[0].start + 1U);

	phy_dev->phy.base = phy_dev->base;
#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
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

static int32_t tcc_ehci_phy_remove(struct platform_device *pdev)
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

static int32_t __init tcc_ehci_phy_drv_init(void)
{
	int32_t retval = 0;

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
