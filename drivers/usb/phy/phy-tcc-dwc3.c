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
#include <linux/of_gpio.h>
#include "../dwc3/core.h"
#include "../dwc3/io.h"
#include <asm/system_info.h>
#include <dt-bindings/gpio/gpio.h>

#define PHY_RESUME      (2)

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

static const char *maximum_speed = "super";

struct tcc_dwc3_device {
	struct device *dev;
	void __iomem *base;
	void __iomem *h_base;
#if defined(CONFIG_ARCH_TCC803X) || \
	defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC805X)
	void __iomem *ref_base;
#endif
	struct usb_phy phy;
	struct clk *phy_clk;

	int32_t vbus_gpio_num;
	ulong vbus_gpio_flag;
#if defined(CONFIG_ENABLE_BC_30_HOST)
	struct work_struct dwc3_work;
	int32_t irq;
#endif
};

struct PUSBPHYCFG {
	// 0x0
	volatile uint32_t U30_CLKMASK;
	// 0x4
	volatile uint32_t U30_SWRESETN;
	// 0x8
	volatile uint32_t U30_PWRCTRL;
	// 0xC
	volatile uint32_t U30_OVERCRNT;
	// 0x10  USB PHY Config Reg 0
	volatile uint32_t U30_PCFG0;
	// 0x14  USB PHY Config Reg 1
	volatile uint32_t U30_PCFG1;
	// // 0x18  USB PHY Config Reg 2
	volatile uint32_t U30_PCFG2;
	// 0x1c  USB PHY Config Reg 3
	volatile uint32_t U30_PCFG3;
	// 0x20  USB PHY Config Reg 4
	volatile uint32_t U30_PCFG4;
	// 0x24  USB PHY Filter Config Reg
	volatile uint32_t U30_PFLT;
	// 0x28  USB PHY Interrupt Reg
	volatile uint32_t U30_PINT;
	// 0x2C  3.0 LINK Ctrl Conf Reg Set 0
	volatile uint32_t U30_LCFG;
	// 0x30  3.0 PHY Param Ctrl Reg Set 0
	volatile uint32_t U30_PCR0;
	// 0x34  3.0 PHY Param Ctrl Reg Set 1
	volatile uint32_t U30_PCR1;
	// 0x38  3.0 PHY Param Ctrl Reg Set 2
	volatile uint32_t U30_PCR2;
	// 0x3C  3.0 UTMI Software Ctrl Reg
	volatile uint32_t U30_SWUTMI;
};

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
struct PUSBSSPHYCFG {
	// 0x0   USB 3.0 Clk Mask Reg
	volatile uint32_t U30_CLKMASK;
	// 0x4   USB 3.0 S/W Reset Reg
	volatile uint32_t U30_SWRESETN;
	// 0x8   USB 3.0 Power Ctrl Reg
	volatile uint32_t U30_PWRCTRL;
	// 0xC   Overcurrent Select Reg
	volatile uint32_t U30_OVERCRNT;
	// 0x10  USB 3.0 PHY Conf Reg 0
	volatile uint32_t U30_PCFG0;
	// 0x14  USB 3.0 PHY Conf Reg 1
	volatile uint32_t U30_PCFG1;
	// 0x18  USB 3.0 PHY Conf Reg 2
	volatile uint32_t U30_PCFG2;
	// 0x1c  USB 3.0 PHY Conf Reg 3
	volatile uint32_t U30_PCFG3;
	// 0x20  USB 3.0 PHY Conf Reg 4
	volatile uint32_t U30_PCFG4;
	// 0x24  USB 3.0 PHY Conf Reg 5
	volatile uint32_t U30_PCFG5;
	// 0x28  USB 3.0 PHY Conf Reg 6
	volatile uint32_t U30_PCFG6;
	// 0x2C  USB 3.0 PHY Conf Reg 7
	volatile uint32_t U30_PCFG7;
	// 0x30  USB 3.0 PHY Conf Reg 8
	volatile uint32_t U30_PCFG8;
	// 0x34  USB 3.0 PHY Conf Reg 9
	volatile uint32_t U30_PCFG9;
	// 0x38  USB 3.0 PHY Conf Reg 10
	volatile uint32_t U30_PCFG10;
	// 0x3C  USB 3.0 PHY Conf Reg 11
	volatile uint32_t U30_PCFG11;
	// 0x40  USB 3.0 PHY Conf Reg 12
	volatile uint32_t U30_PCFG12;
	// 0x44  USB 3.0 PHY Conf Reg 13
	volatile uint32_t U30_PCFG13;
	// 0x48  USB 3.0 PHY Conf Reg 14
	volatile uint32_t U30_PCFG14;
	// 0x4C  USB 3.0 PHY Conf Reg 15
	volatile uint32_t U30_PCFG15;
	// 0x50~74
	volatile uint32_t reserved0[10];
	// 0x78  USB 3.0 PHY Filter Conf Reg
	volatile uint32_t U30_PFLT;
	// 0x7C  USB 3.0 PHY Interrupt Reg
	volatile uint32_t U30_PINT;
	// 0x80  3.0 LINK Ctrl Conf Reg Set 0
	volatile uint32_t U30_LCFG;
	// 0x84  3.0 PHY Param Ctrl Reg Set 0
	volatile uint32_t U30_PCR0;
	// 0x88  3.0 PHY Param Ctrl Reg Set 1
	volatile uint32_t U30_PCR1;
	// 0x8C  3.0 PHY Param Ctrl Reg Set 2
	volatile uint32_t U30_PCR2;
	// 0x90  USB 3.0 PHY Soft Ctrl Reg
	volatile uint32_t U30_SWUTMI;
	// 0x94  3.0 LINK Ctrl debug Sig 0
	volatile uint32_t U30_DBG0;
	// 0x98  3.0 LINK Ctrl debug Sig 0
	volatile uint32_t U30_DBG1;
	// 0x9C
	volatile uint32_t reserved[1];
	// 0xA0  3.0 HS PHY Conf Reg 0
	volatile uint32_t FPHY_PCFG0;
	// 0xA4  3.0 HS PHY Conf Reg 1
	volatile uint32_t FPHY_PCFG1;
	// 0xA8  3.0 HS PHY Conf Reg 2
	volatile uint32_t FPHY_PCFG2;
	// 0xAc  3.0 HS PHY Conf Reg 3
	volatile uint32_t FPHY_PCFG3;
	// 0xB0  3.0 HS PHY Conf Reg 4
	volatile uint32_t FPHY_PCFG4;
	// 0xB4  3.0 HS LINK Ctrl Conf Reg 0
	volatile uint32_t FPHY_LCFG0;
	// 0xB8  3.0 HS LINK Ctrl Conf Reg 1
	volatile uint32_t FPHY_LCFG1;
};
#endif
static void __iomem *dwc3_get_base(struct usb_phy *phy)
{
	struct tcc_dwc3_device *dwc3_phy_dev =
		container_of(phy, struct tcc_dwc3_device, phy);

	if (dwc3_phy_dev != NULL) {
		return dwc3_phy_dev->base;
	} else {
		return (void __iomem *)0;
	}
}

static int32_t tcc_dwc3_vbus_set(struct usb_phy *phy, int32_t on_off)
{
	struct tcc_dwc3_device *phy_dev =
		container_of(phy, struct tcc_dwc3_device, phy);
	struct device *dev;
	struct property *pp;
	int32_t retval = 0;

	if (phy_dev == NULL) {
		return -ENODEV;
	}

	dev = phy_dev->dev;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */

	pp = of_find_property(dev->of_node, "vbus-ctrl-able", NULL);
	if (pp == NULL) {
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
	retval = gpio_request_one((uint32_t)phy_dev->vbus_gpio_num,
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
			gpio_to_desc((uint32_t)phy_dev->vbus_gpio_num),
			on_off);
	if (retval != 0) {
		dev_err(dev, "[ERROR][USB] VBus GPIO direction can't be set to output, errno %d.\n",
				retval);
		return retval;
	}

	gpio_free((uint32_t)phy_dev->vbus_gpio_num);

	return retval;
}

#if defined(CONFIG_ENABLE_BC_30_HOST)
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
static void tcc_dwc3_set_chg_det(struct usb_phy *phy)
{
	struct tcc_dwc3_device *dwc3_phy_dev =
		container_of(phy, struct tcc_dwc3_device, phy);
	struct PUSBSSPHYCFG *USBPHYCFG =
		(struct PUSBSSPHYCFG *)dwc3_phy_dev->base;

	dev_info(dwc3_phy_dev->dev, "[INFO][USB] Charging Detection!!\n");
	writel(readl(&USBPHYCFG->FPHY_PCFG2) | (1 << 8),
			&USBPHYCFG->FPHY_PCFG2); //enable chg det
}

static void tcc_dwc3_set_cdp(struct work_struct *data)
{
	struct tcc_dwc3_device *dwc3_phy_dev =
		container_of(data, struct tcc_dwc3_device, dwc3_work);
	struct PUSBSSPHYCFG *USBPHYCFG =
		(struct PUSBSSPHYCFG *)dwc3_phy_dev->base;
	uint32_t pcfg2 = 0;
	int32_t count = 3;

	while (count > 0) {
		if ((readl(&USBPHYCFG->FPHY_PCFG2) & (1 << 22)) != 0) {
			break;
		}
		mdelay(1);
		count--;
	}

	if (count == 0) {
		dev_info(dwc3_phy_dev->dev, "[INFO][USB] %s : failed to detect charging!!\n",
				__func__);
	} else {
		pcfg2 = readl(&USBPHYCFG->FPHY_PCFG2);
		writel((pcfg2 | (1 << 9)), &USBPHYCFG->FPHY_PCFG2);
		mdelay(100);
		writel(readl(&USBPHYCFG->FPHY_PCFG2) & ~((1 << 9) | (1 << 8)),
				&USBPHYCFG->FPHY_PCFG2);
	}

	writel(readl(&USBPHYCFG->FPHY_PCFG4) | (1 << 31),
			&USBPHYCFG->FPHY_PCFG4); // clear irq
	udelay(10);
	writel(readl(&USBPHYCFG->FPHY_PCFG4) & ~(1 << 31),
			&USBPHYCFG->FPHY_PCFG4); // clear irq
	dev_info(dwc3_phy_dev->dev, "[INFO][USB] %s:Enable chg det!!!\n",
			__func__);
}

static irqreturn_t tcc_dwc3_chg_irq(int32_t irq, void *data)
{
	struct tcc_dwc3_device *dwc3_phy_dev = (struct tcc_dwc3_device *)data;
	struct PUSBSSPHYCFG *USBPHYCFG =
		(struct PUSBSSPHYCFG *)dwc3_phy_dev->base;

	dev_info(dwc3_phy_dev->dev, "[INFO][USB] %s : CHGDET\n", __func__);

	// clear irq
	writel(readl(&USBPHYCFG->U30_PINT) | (1 << 22), &USBPHYCFG->U30_PINT);
	udelay(1);

	// clear irq
	writel(readl(&USBPHYCFG->U30_PINT) & ~(1 << 22), &USBPHYCFG->U30_PINT);

	schedule_work(&dwc3_phy_dev->dwc3_work);

	return IRQ_HANDLED;
}
#else
static void tcc_dwc3_set_chg_det(struct usb_phy *phy)
{
	struct tcc_dwc3_device *dwc3_phy_dev =
		container_of(phy, struct tcc_dwc3_device, phy);
	struct PUSBPHYCFG *USBPHYCFG = (struct PUSBPHYCFG *)dwc3_phy_dev->base;

	dev_info(dwc3_phy_dev->dev, "[INFO][USB] Charging Detection!!\n");

	// enable chg det
	writel(readl(&USBPHYCFG->U30_PCFG1) | (1 << 17), &USBPHYCFG->U30_PCFG1);
}

static void tcc_dwc3_set_cdp(struct work_struct *data)
{
	struct tcc_dwc3_device *dwc3_phy_dev =
		container_of(data, struct tcc_dwc3_device, dwc3_work);
	struct PUSBPHYCFG *USBPHYCFG = (struct PUSBPHYCFG *)dwc3_phy_dev->base;
	uint32_t pcfg = 0;
	int32_t count = 3;

	while (count > 0) {
		if ((readl(&USBPHYCFG->U30_PCFG1) & (1 << 20)) != 0) {
			dev_info(dwc3_phy_dev->dev, "[INFO][USB] Chager Detecttion!!\n");
			break;
		}
		mdelay(1);
		count--;
	}

	if (count == 0) {
		dev_info(dwc3_phy_dev->dev, "[INFO][USB] %s : failed to detect charging!!\n",
				__func__);
	} else {
		pcfg = readl(&USBPHYCFG->U30_PCFG1);
		writel((pcfg | (1 << 18)), &USBPHYCFG->U30_PCFG1);
		mdelay(100);
		writel(readl(&USBPHYCFG->U30_PCFG1) & ~((1 << 18) | (1 << 17)),
				&USBPHYCFG->U30_PCFG1);
	}

	dev_info(dwc3_phy_dev->dev, "[INFO][USB] %s:Enable chg det!!!\n",
			__func__);
}

static irqreturn_t tcc_dwc3_chg_irq(int32_t irq, void *data)
{
	struct tcc_dwc3_device *dwc3_phy_dev = (struct tcc_dwc3_device *)data;
	struct PUSBPHYCFG *USBPHYCFG = (struct PUSBPHYCFG *)dwc3_phy_dev->base;

	dev_info(dwc3_phy_dev->dev, "[INFO][USB] %s : CHGDET\n", __func__);

	// clear irq
	writel(readl(&USBPHYCFG->U30_PINT) | (1 << 22), &USBPHYCFG->U30_PINT);
	udelay(1);

	// clear irq
	writel(readl(&USBPHYCFG->U30_PINT) & ~(1 << 22), &USBPHYCFG->U30_PINT);
	schedule_work(&dwc3_phy_dev->dwc3_work);

	return IRQ_HANDLED;
}
#endif
#endif

static int32_t is_suspend = 1;

#if defined(CONFIG_ARCH_TCC898X)
static void dwc3_tcc898x_swreset(struct PUSBPHYCFG *USBPHYCFG, int32_t on_off)
{
	if (on_off == ON) {
		BITCLR(USBPHYCFG->U30_SWRESETN, Hw1);
	} else if (on_off == OFF) {
		BITSET(USBPHYCFG->U30_SWRESETN, Hw1);
	} else {
		pr_info("[INFO][USB] \x1b[1;31m[%s:%d]Wrong request!!\n",
				__func__, __LINE__);
	}
}
#endif

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
static int32_t dwc3_tcc_ss_phy_ctrl_native(struct usb_phy *phy, int32_t on_off)
{
	struct tcc_dwc3_device *dwc3_phy_dev =
		container_of(phy, struct tcc_dwc3_device, phy);
	struct PUSBSSPHYCFG *USBPHYCFG;
	uint32_t uTmp = 0;
	uint32_t tmp_data = 0;
	int32_t tmp_cnt;
#if defined(CONFIG_ARCH_TCC803X)
	uint32_t cal_value = 0;
#endif

	if (dwc3_phy_dev == NULL) {
		return -ENODEV;
	}

	USBPHYCFG = (struct PUSBSSPHYCFG *)dwc3_phy_dev->base;

	if (USBPHYCFG == NULL) {
		return -ENODEV;
	}

	dev_info(dwc3_phy_dev->dev, "[INFO][USB] %s %s\n", __func__,
			(on_off != 0) ? "ON" : "OFF");

	if ((on_off == (int32_t)ON) && (is_suspend != 0)) {
		// 1.Power-on Reset
		// PHY external Clock Configuration mode selection
		writel((readl(&USBPHYCFG->U30_PCFG4) | (Hw0)),
				&USBPHYCFG->U30_PCFG4);
		writel((readl(&USBPHYCFG->U30_LCFG) & ~Hw31),
				&USBPHYCFG->U30_LCFG); // vcc_reset_n
		writel((readl(&USBPHYCFG->U30_PCFG0) & ~(Hw30)),
				&USBPHYCFG->U30_PCFG0); // phy_reset
		writel((readl(&USBPHYCFG->FPHY_PCFG0) | (Hw31)),
				&USBPHYCFG->FPHY_PCFG0); // USB 2.0 PHY POR Set

		uTmp = readl(&USBPHYCFG->U30_PCFG0);
		uTmp &= ~(Hw25); // turn off SS circuits
		uTmp &= ~(Hw24); // turn on HS circuits
		writel(uTmp, &USBPHYCFG->U30_PCFG0);
		usleep_range(1000, 2000);

		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw30)),
				&USBPHYCFG->U30_PCFG0); // phy_reset

		// External SRAM Init Done Wait
		tmp_data = readl(&USBPHYCFG->U30_PCFG0);
		tmp_data &= (Hw5);

		while (tmp_data == 0U) {
			tmp_data = readl(&USBPHYCFG->U30_PCFG0);
			tmp_data &= (Hw5);
		}

		// External SRAM LD Done Set
		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw3)),
				&USBPHYCFG->U30_PCFG0);

		// Set TXVRT to 0xA
		writel(0xE31C243CU, &USBPHYCFG->FPHY_PCFG1);
		// Set Tx vboost level to 0x7
		writel(0x31C71457, &USBPHYCFG->U30_PCFG13);
		// Set Tx iboost level to 0xA
		writel(0xA4C4302AU, &USBPHYCFG->U30_PCFG15);

		// USB 2.0 PHY POR Release
		writel((readl(&USBPHYCFG->FPHY_PCFG0) & ~(Hw24)),
				&USBPHYCFG->FPHY_PCFG0);
		writel((readl(&USBPHYCFG->FPHY_PCFG0) & ~(Hw31)),
				&USBPHYCFG->FPHY_PCFG0);

		// LINK Reset Release
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw31)),
				&USBPHYCFG->U30_LCFG); // vcc_reset_n

		tmp_cnt = 0;

		while (tmp_cnt < 10000) {
			if ((readl(&USBPHYCFG->U30_PCFG0) &
					0x00000004U) != 0U) {
				break;
			}

			tmp_cnt++;
			udelay(5);
		}

		dev_info(dwc3_phy_dev->dev, "[INFO][USB] XHCI PHY valid check %s\n",
				(tmp_cnt >= 9999) ? "fail!" : "pass.");

		// Initialize all registers
		writel((readl(&USBPHYCFG->U30_LCFG) |
					(Hw27 | Hw26 | Hw19 |
					 Hw18 | Hw17 | Hw7)),
				&USBPHYCFG->U30_LCFG);

		/* Set 2.0phy REXT */
		tmp_cnt = 0;

#if defined(CONFIG_ARCH_TCC803X)
		do {
			// Read calculated value
			writel(Hw26|Hw25, dwc3_phy_dev->ref_base);
			uTmp = readl(dwc3_phy_dev->ref_base);
			dev_info(dwc3_phy_dev->dev, "[INFO][USB] 2.0H status bus = 0x%08x\n",
					uTmp);
			cal_value = 0x0000F000U & uTmp;

			cal_value = cal_value<<4; // set TESTDATAIN

			//Read Status Bus
			writel(Hw26 | Hw25, &USBPHYCFG->FPHY_PCFG3);
			uTmp = readl(&USBPHYCFG->FPHY_PCFG3);

			//Read Override Bus
			writel(Hw29 | Hw26 | Hw25, &USBPHYCFG->FPHY_PCFG3);
			uTmp = readl(&USBPHYCFG->FPHY_PCFG3);

			//Write Override Bus
			writel(Hw29 | Hw26 | Hw25 | Hw23 |
					Hw22 | Hw21 | Hw20 | cal_value,
					&USBPHYCFG->FPHY_PCFG3);
			udelay(1);

			writel(Hw29 | Hw28 | Hw26 | Hw25 |
					Hw23 | Hw22 | Hw21 | Hw20 | cal_value,
					&USBPHYCFG->FPHY_PCFG3);
			udelay(1);

			writel(Hw29 | Hw26 | Hw25 | Hw23 |
					Hw22 | Hw21 | Hw20 | cal_value,
					&USBPHYCFG->FPHY_PCFG3);
			udelay(1);

			//Read Status Bus
			writel(Hw26 | Hw25, &USBPHYCFG->FPHY_PCFG3);
			uTmp = readl(&USBPHYCFG->FPHY_PCFG3);

			//Read Override Bus
			writel(Hw29 | Hw26 | Hw25, &USBPHYCFG->FPHY_PCFG3);
			uTmp = readl(&USBPHYCFG->FPHY_PCFG3);
			dev_info(dwc3_phy_dev->dev, "[INFO][USB] 2.0 REXT = 0x%08x\n",
					(0x0000F000U & uTmp));

			tmp_cnt++;
		} while (((uTmp & 0x0000F000U) == 0U) && (tmp_cnt < 5));
#endif

#if defined(CONFIG_ENABLE_BC_30_HOST)
		writel(readl(&USBPHYCFG->FPHY_PCFG4) | (1 << 31),
				&USBPHYCFG->FPHY_PCFG4); // clear irq
		writel(readl(&USBPHYCFG->FPHY_PCFG4) & ~(1 << 30),
				&USBPHYCFG->FPHY_PCFG4); // Disable VBUS Detect

		writel(readl(&USBPHYCFG->FPHY_PCFG4) & ~(1 << 31),
				&USBPHYCFG->FPHY_PCFG4); // clear irq
		udelay(1);

		writel(readl(&USBPHYCFG->FPHY_PCFG2) | ((1 << 8) | (1 << 10)),
				&USBPHYCFG->FPHY_PCFG2);
		udelay(1);

		// enable CHG_DET interrupt
		writel(readl(&USBPHYCFG->FPHY_PCFG4) | (1 << 28),
				&USBPHYCFG->FPHY_PCFG4);

		writel((readl(&USBPHYCFG->U30_PINT) & ~(1 << 6)) |
				((1 << 31) | (1 << 7) | (1 << 5) |
				 (1 << 4) | (1 << 3) | (1 << 2) |
				 (1 << 1) | (1 << 0)), &USBPHYCFG->U30_PINT);

		enable_irq(dwc3_phy_dev->irq);
#endif
		// Link global Reset, CoreRSTN (Cold Reset), active low
		writel((readl(&USBPHYCFG->U30_LCFG) & ~Hw31),
				&USBPHYCFG->U30_LCFG);

		if (strncmp("high", maximum_speed, 4) == 0) {
			// USB20 Only Mode
			/*
			 * enable usb20mode -> removed in DWC_usb3 2.60a, but
			 * use as interrupt
			 */
			writel((readl(&USBPHYCFG->U30_LCFG) | Hw28),
					&USBPHYCFG->U30_LCFG);
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp |= Hw25; // turn off SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			writel(uTmp, &USBPHYCFG->U30_PCFG0);
		} else if (strncmp("super", maximum_speed, 5) == 0) {
			// USB 3.0
			/*
			 * disable usb20mode -> removed in DWC_usb3 2.60a, but
			 * use as interrupt
			 */
			writel((readl(&USBPHYCFG->U30_LCFG) & ~(Hw28)),
					&USBPHYCFG->U30_LCFG);
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp &= ~(Hw25); // turn on SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			uTmp &= ~(Hw30); // release PHY reset
			writel(uTmp, &USBPHYCFG->U30_PCFG0);
		} else {
			/* Nothing to do */
		}

		mdelay(1);

		// Release Reset Link global
		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw30)),
				&USBPHYCFG->U30_PCFG0);
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw31)),
				&USBPHYCFG->U30_LCFG);

		mdelay(10);

		if (clk_prepare_enable(dwc3_phy_dev->phy_clk) != 0) {
			dev_err(dwc3_phy_dev->dev, "[ERROR][USB] can't do xhci phy clk enable\n");
		}

		is_suspend = 0;
	} else if ((on_off == (int32_t)OFF) && (is_suspend == 0)) {
		clk_disable_unprepare(dwc3_phy_dev->phy_clk);

		// USB 3.0 PHY Power down
		dev_info(dwc3_phy_dev->dev, "[INFO][USB] dwc3 tcc: PHY power down\n");
		USBPHYCFG->U30_PCFG0 |= (Hw25 | Hw24);

		mdelay(10);

		uTmp = USBPHYCFG->U30_PCFG0;
		is_suspend = 1;
	} else if ((on_off == (int32_t)PHY_RESUME) && (is_suspend != 0)) {
		USBPHYCFG->U30_PCFG0 &= ~(Hw25 | Hw24);
		dev_info(dwc3_phy_dev->dev, "[INFO][USB] dwc3 tcc: PHY power up\n");

		if (clk_prepare_enable(dwc3_phy_dev->phy_clk) != 0) {
			dev_err(dwc3_phy_dev->dev, "[ERROR][USB] can't do xhci phy clk enable\n");
		}

		is_suspend = 0;
	} else {
		/* Nothing to do */
	}

	return 0;
}
#else
static int32_t dwc3_tcc_phy_ctrl_native(struct usb_phy *phy, int32_t on_off)
{
	struct tcc_dwc3_device *dwc3_phy_dev =
		container_of(phy, struct tcc_dwc3_device, phy);
	struct PUSBPHYCFG *USBPHYCFG;
	uint32_t uTmp = 0;
	int32_t tmp_cnt;

	if (dwc3_phy_dev == NULL) {
		return -ENODEV;
	}

	USBPHYCFG = (struct PUSBPHYCFG *)dwc3_phy_dev->base;

	if (USBPHYCFG == NULL) {
		return -ENXIO;
	}

	dev_info(dwc3_phy_dev->dev, "[INFO][USB] %s %s\n", __func__,
			(on_off != 0) ? "ON" : "OFF");
	if ((on_off == (int32_t)ON) && (is_suspend != 0)) {
		// 1.Power-on Reset
#if defined(CONFIG_ARCH_TCC898X)
		dwc3_tcc898x_swreset(USBPHYCFG, ON);
		mdelay(1);
		dwc3_tcc898x_swreset(USBPHYCFG, OFF);
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
		defined(CONFIG_ARCH_TCC901X) ||				\
		defined(CONFIG_ARCH_TCC805X)
		writel((readl(&USBPHYCFG->U30_LCFG) & ~Hw31),
				&USBPHYCFG->U30_LCFG);
		writel((readl(&USBPHYCFG->U30_PCFG0) & ~(Hw30)),
				&USBPHYCFG->U30_PCFG0);

		uTmp = readl(&USBPHYCFG->U30_PCFG0);
		uTmp &= ~(Hw25); // turn off SS circuits
		uTmp &= ~(Hw24); // turn on HS circuits
		writel(uTmp, &USBPHYCFG->U30_PCFG0);
		usleep_range(1000, 2000);

		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw30)),
				&USBPHYCFG->U30_PCFG0);
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw31)),
				&USBPHYCFG->U30_LCFG);
		tmp_cnt = 0;

		while (tmp_cnt < 10000) {
			if ((readl(&USBPHYCFG->U30_PCFG0) &
					0x80000000U) != 0U) {
				break;
			}

			tmp_cnt++;
			udelay(5);
		}

		dev_info(dwc3_phy_dev->dev, "[INFO][USB] XHCI PHY valid check %s\n",
				(tmp_cnt >= 9999) ? "fail!" : "pass.");
#endif
		// Initialize all registers
#if defined(CONFIG_ARCH_TCC898X)
		USBPHYCFG->U30_PCFG0 =
			(system_rev >= 2) ? 0x40306228 : 0x20306228;
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
		defined(CONFIG_ARCH_TCC901X) ||				\
		defined(CONFIG_ARCH_TCC805X)
		writel(0x03204208, &USBPHYCFG->U30_PCFG0);
#else
		USBPHYCFG->U30_PCFG0 = 0x20306228;
#endif

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
		defined(CONFIG_ARCH_TCC901X) ||				\
		defined(CONFIG_ARCH_TCC805X)
		writel(0x00000000, &USBPHYCFG->U30_PCFG1);
		writel(0x919E1A04U, &USBPHYCFG->U30_PCFG2);
#if defined(CONFIG_ARCH_TCC899X)
		// vboost:4, de-emp:0x1f, swing:0x6f, ios_bias:05
		writel(0x4befef05, &USBPHYCFG->U30_PCFG3);
#else
		writel(0x4B8E7F05, &USBPHYCFG->U30_PCFG3);
#endif
		writel(0x00200000, &USBPHYCFG->U30_PCFG4);
		writel(0x00000351, &USBPHYCFG->U30_PFLT);
		writel(0x80000000U, &USBPHYCFG->U30_PINT);
		writel((readl(&USBPHYCFG->U30_LCFG) |
					(Hw27 | Hw26 | Hw19 |
					 Hw18 | Hw17 | Hw7)),
				&USBPHYCFG->U30_LCFG);
#else
		USBPHYCFG->U30_PCFG1 = 0x00000300;
		USBPHYCFG->U30_PCFG2 = 0x919f94C4;
		USBPHYCFG->U30_PCFG3 = 0x4ab07f05;
#endif

#if defined(CONFIG_ARCH_TCC898X)
		dwc3_tcc898x_swreset(USBPHYCFG, ON);

		// Link global Reset, CoreRSTN (Cold Reset), active low
		USBPHYCFG->U30_LCFG &= ~(Hw31);
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
		defined(CONFIG_ARCH_TCC901X) ||				\
		defined(CONFIG_ARCH_TCC805X)
		// Link global Reset, CoreRSTN (Cold Reset), active low
		writel((readl(&USBPHYCFG->U30_LCFG) & ~Hw31),
				&USBPHYCFG->U30_LCFG);
#endif

#if defined(CONFIG_ARCH_TCC898X)
		if (!strncmp("high", maximum_speed, 4)) {
			// USB20 Only Mode
			/*
			 * enable usb20mode -> removed in DWC_usb3 2.60a, but
			 * use as interrupt
			 */
			USBPHYCFG->U30_LCFG |= Hw28;
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp |= Hw25; // turn off SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			USBPHYCFG->U30_PCFG0 = uTmp;
		} else if (!strncmp("super", maximum_speed, 5)) {
			// USB 3.0
			/*
			 * disable usb20mode -> removed in DWC_usb3 2.60a, but
			 * use as interrupt
			 */
			USBPHYCFG->U30_LCFG &= ~(Hw28);
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp &= ~(Hw25); // turn on SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			USBPHYCFG->U30_PCFG0 = uTmp;
		}

		// Release Reset Link global
		USBPHYCFG->U30_LCFG |= (Hw31);
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||	\
		defined(CONFIG_ARCH_TCC901X) ||				\
		defined(CONFIG_ARCH_TCC805X)
		if (strncmp("high", maximum_speed, 4) == 0) {
			// USB20 Only Mode
			/*
			 * enable usb20mode -> removed in DWC_usb3 2.60a, but
			 * use as interrupt
			 */
			writel((readl(&USBPHYCFG->U30_LCFG) | Hw28),
					&USBPHYCFG->U30_LCFG);
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp |= Hw25; // turn off SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			writel(uTmp, &USBPHYCFG->U30_PCFG0);
		} else if (strncmp("super", maximum_speed, 5) == 0) {
			// USB 3.0
			/*
			 * disable usb20mode -> removed in DWC_usb3 2.60a, but
			 * use as interrupt
			 */
			writel((readl(&USBPHYCFG->U30_LCFG) & ~(Hw28)),
					&USBPHYCFG->U30_LCFG);
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp &= ~(Hw25); // turn on SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			uTmp &= ~(Hw30); // release PHY reset
			writel(uTmp, &USBPHYCFG->U30_PCFG0);
		} else {
			/* Nothing to do */
		}

		mdelay(1);

		// Release Reset Link global
		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw30)),
				&USBPHYCFG->U30_PCFG0);
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw31)),
				&USBPHYCFG->U30_LCFG);
#endif

#if defined(CONFIG_ARCH_TCC898X)
		dwc3_tcc898x_swreset(USBPHYCFG, OFF);
#endif
		mdelay(10);

		if (clk_prepare_enable(dwc3_phy_dev->phy_clk) != 0) {
			dev_err(dwc3_phy_dev->dev, "[ERROR][USB] can't do xhci phy clk enable\n");
		}

		is_suspend = 0;
	} else if ((on_off == (int32_t)OFF) && (is_suspend == 0)) {
		clk_disable_unprepare(dwc3_phy_dev->phy_clk);
		// USB 3.0 PHY Power down
		dev_info(dwc3_phy_dev->dev, "[INFO][USB] dwc3 tcc: PHY power down\n");
		USBPHYCFG->U30_PCFG0 |= (Hw25 | Hw24);

		mdelay(10);

		uTmp = USBPHYCFG->U30_PCFG0;
		is_suspend = 1;
	} else if ((on_off == (int32_t)PHY_RESUME) && (is_suspend != 0)) {
		is_suspend = 0;
		dev_info(dwc3_phy_dev->dev, "[INFO][USB] dwc3 tcc: PHY resume\n");
		USBPHYCFG->U30_PCFG0 &= ~(Hw25 | Hw24);

		mdelay(10);

		if (clk_prepare_enable(dwc3_phy_dev->phy_clk) != 0) {
			dev_err(dwc3_phy_dev->dev, "[ERROR][USB] can't do xhci phy clk enable\n");
		}
	} else {
		/* Nothing to do */
	}

	return 0;
}
#endif

static int32_t tcc_dwc3_init_phy(struct usb_phy *phy)
{
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	return dwc3_tcc_ss_phy_ctrl_native(phy, ON);
#else
	return dwc3_tcc_phy_ctrl_native(phy, ON);
#endif
}

static int32_t tcc_dwc3_suspend_phy(struct usb_phy *phy, int32_t suspend)
{
	if (phy == NULL) {
		return -ENODEV;
	}

	if (suspend == 0) {
		return phy->set_phy_state(phy, PHY_RESUME);
	} else {
		return phy->set_phy_state(phy, OFF);
	}
}

static void tcc_dwc3_shutdown_phy(struct usb_phy *phy)
{
	if (phy == NULL) {
		return;
	}

	phy->set_phy_state(phy, OFF);
}

static int32_t tcc_dwc3_set_vbus_resource(struct usb_phy *phy)
{
	struct tcc_dwc3_device *phy_dev =
		container_of(phy, struct tcc_dwc3_device, phy);
	struct device *dev;
	uint32_t gpio_flag;

	if (phy_dev == NULL) {
		return -ENODEV;
	}

	dev = phy_dev->dev;

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
			if (gpio_flag == (uint32_t)GPIO_ACTIVE_LOW) {
				phy_dev->vbus_gpio_flag = GPIOF_ACTIVE_LOW;
			}

			dev_dbg(dev, "[DEBUG][USB] VBus GPIO pin number is %d\n",
					phy_dev->vbus_gpio_num);
			dev_dbg(dev, "[DEBUG][USB] VBus GPIO flag is %s\n",
					(gpio_flag != 0U) ? "active low(=1)" :
					"active high(=0)");
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

/* create phy struct */
static int32_t tcc_dwc3_create_phy(struct device *dev,
		struct tcc_dwc3_device *phy_dev)
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

	// PHY CLK
	phy_dev->phy_clk = of_clk_get(dev->of_node, 0);

	if (IS_ERR(phy_dev->phy_clk)) {
		dev_info(dev, "[INFO][USB] xhci phy clk_get failed\n");
		phy_dev->phy_clk = NULL;
	}

	phy_dev->dev                   = dev;

	phy_dev->phy.dev               = phy_dev->dev;
	phy_dev->phy.label             = "tcc_dwc3_phy";
	phy_dev->phy.state             = OTG_STATE_UNDEFINED;
	phy_dev->phy.type              = USB_PHY_TYPE_USB3;
	phy_dev->phy.init              = &tcc_dwc3_init_phy;
	phy_dev->phy.set_suspend       = &tcc_dwc3_suspend_phy;
	phy_dev->phy.shutdown          = &tcc_dwc3_shutdown_phy;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	phy_dev->phy.set_phy_state     = &dwc3_tcc_ss_phy_ctrl_native;
#else
	phy_dev->phy.set_phy_state     = &dwc3_tcc_phy_ctrl_native;
#endif
#if defined(CONFIG_ENABLE_BC_30_HOST)
	phy_dev->phy.set_chg_det       = &tcc_dwc3_set_chg_det;
#endif
	phy_dev->phy.set_vbus_resource = &tcc_dwc3_set_vbus_resource;
	phy_dev->phy.set_vbus          = &tcc_dwc3_vbus_set;
	phy_dev->phy.get_base          = &dwc3_get_base;

	phy_dev->phy.otg->usb_phy      = &phy_dev->phy;
#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X))
	retval = tcc_dwc3_set_vbus_resource(&phy_dev->phy);
#endif
	return retval;
}

static int32_t tcc_dwc3_phy_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct tcc_dwc3_device *phy_dev;
	int32_t retval;
#if defined(CONFIG_ENABLE_BC_30_HOST)
	int32_t irq, ret = 0;
#endif
	if (pdev == NULL) {
		return -ENODEV;
	}

	dev = &pdev->dev;
	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);

	retval = tcc_dwc3_create_phy(dev, phy_dev);
	if (retval != 0) {
		dev_err(&pdev->dev, "[ERROR][USB] error create phy\n");
		return retval;
	}
#if defined(CONFIG_ENABLE_BC_30_HOST)
	irq = platform_get_irq(pdev, 0);

	if (irq <= 0) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
				dev_name(&pdev->dev));
		retval = -ENODEV;
	} else {
		dev_info(&pdev->dev, "[INFO][USB] %s: irq=%d\n", __func__, irq);
		phy_dev->irq = irq;
	}
#endif
	if (request_mem_region(pdev->resource[0].start,
				pdev->resource[0].end -
				pdev->resource[0].start + 1,
				"dwc3_phy") == NULL) {
		dev_err(&pdev->dev, "[ERROR][USB] error reserving mapped memory\n");
		retval = -EFAULT;
	}

	phy_dev->base = (void __iomem *)ioremap_nocache(
			(resource_size_t)pdev->resource[0].start,
			(size_t)(pdev->resource[0].end -
				pdev->resource[0].start + 1U));

	phy_dev->phy.base = phy_dev->base;
#if defined(CONFIG_ENABLE_BC_30_HOST)
	ret = devm_request_irq(&pdev->dev, phy_dev->irq, tcc_dwc3_chg_irq,
			IRQF_SHARED, pdev->dev.kobj.name, phy_dev);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][USB] request irq failed\n");
	}

	disable_irq(phy_dev->irq);
	INIT_WORK(&phy_dev->dwc3_work, tcc_dwc3_set_cdp);
#endif

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	phy_dev->ref_base = (void __iomem *)ioremap_nocache(
			(resource_size_t)pdev->resource[1].start,
			pdev->resource[1].end - pdev->resource[1].start + 1);
	phy_dev->phy.ref_base = phy_dev->ref_base;
#endif

	platform_set_drvdata(pdev, phy_dev);

	retval = usb_add_phy_dev(&phy_dev->phy);
	if (retval != 0) {
		dev_err(&pdev->dev, "[ERROR][USB] usb_add_phy failed\n");
		return retval;
	}

	dev_err(&pdev->dev, "[INFO][USB] %s:%s\n", pdev->dev.kobj.name,
			__func__);

	return retval;
}

static int32_t tcc_dwc3_phy_remove(struct platform_device *pdev)
{
	struct tcc_dwc3_device *phy_dev;

	if (pdev == NULL) {
		return -ENODEV;
	}

	phy_dev = platform_get_drvdata(pdev);

	usb_remove_phy(&phy_dev->phy);
	release_mem_region(pdev->resource[0].start, // dwc3 base
			pdev->resource[0].end - pdev->resource[0].start + 1U);
	release_mem_region(pdev->resource[1].start, // dwc3 phy base
			pdev->resource[1].end - pdev->resource[1].start + 1U);
	return 0;
}

static const struct of_device_id tcc_dwc3_phy_match[] = {
	{ .compatible = "telechips,tcc_dwc3_phy" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcc_dwc3_phy_match);

static struct platform_driver tcc_dwc3_phy_driver = {
	.probe  = tcc_dwc3_phy_probe,
	.remove = tcc_dwc3_phy_remove,
	.driver = {
		.name           = "dwc3_phy",
		.owner          = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(tcc_dwc3_phy_match),
#endif
	},
};

static int32_t __init tcc_dwc3_phy_drv_init(void)
{
	int32_t retval = 0;

	retval = platform_driver_register(&tcc_dwc3_phy_driver);
	if (retval < 0) {
		pr_err("[ERROR][USB] %s retval=%d\n", __func__, retval);
	}

	return retval;
}
core_initcall(tcc_dwc3_phy_drv_init);

static void __exit tcc_dwc3_phy_cleanup(void)
{
	platform_driver_unregister(&tcc_dwc3_phy_driver);
}
module_exit(tcc_dwc3_phy_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TCC USB DWC3 transceiver driver");
