/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <mach/arc_otg.h>
#include <mach/hardware.h>
#include <mach/clock.h>
#include "devices-mvf.h"
#include "crm_regs.h"
#include "regs-anadig.h"
#include "usb.h"

static int usbotg_init_ext(struct platform_device *pdev);
static void usbotg_uninit_ext(struct platform_device *pdev);
static void usbotg_clock_gate(bool on);
static void _dr_discharge_line(bool enable);

/* The usb_phy0_clk do not have enable/disable function at clock.c
 * and PLL output for usb0's phy should be always enabled.
 * usb_phy0_clk only stands for usb uses pll3 as its parent.
 */
static struct clk *usb_phy0_clk;
static u8 otg_used;

/* Beginning of Common operation for DR port */

/*
 * platform data structs
 * - Which one to use is determined by CONFIG options in usb.h
 * - operating_mode plugged at run time
 */
static struct fsl_usb2_platform_data dr_utmi_config = {
	.name              = "DR",
	.init              = usbotg_init_ext,
	.exit              = usbotg_uninit_ext,
	.phy_mode          = FSL_USB2_PHY_UTMI_WIDE,
	.power_budget      = 500,		/* 500 mA max power */
	.usb_clock_for_pm  = usbotg_clock_gate,
	.transceiver       = "utmi",
	.phy_regs = MVF_USBPHY0_BASE_ADDR,
	.dr_discharge_line = _dr_discharge_line,
};

static void usbotg_internal_phy_clock_gate(bool on)
{
	u32 reg;

	void __iomem *phy_reg = MVF_IO_ADDRESS(MVF_USBPHY0_BASE_ADDR);
	reg = __raw_readl(phy_reg + HW_USBPHY_CTRL);

	if (on)
		reg &= ~BM_USBPHY_CTRL_CLKGATE;
	else
		reg |= BM_USBPHY_CTRL_CLKGATE;

	__raw_writel(reg, phy_reg + HW_USBPHY_CTRL);
}

static int usb_phy_enable(struct fsl_usb2_platform_data *pdata)
{
	u32 tmp;
	void __iomem *phy_reg = MVF_IO_ADDRESS(MVF_USBPHY0_BASE_ADDR);
	void __iomem *phy_ctrl;

	/* Stop then Reset */
	UOG_USBCMD &= ~UCMD_RUN_STOP;
	while (UOG_USBCMD & UCMD_RUN_STOP)
		;

	UOG_USBCMD |= UCMD_RESET;
	while ((UOG_USBCMD) & (UCMD_RESET))
		;
	/* Reset USBPHY module */
	phy_ctrl = phy_reg + HW_USBPHY_CTRL;
	tmp = __raw_readl(phy_ctrl);
	tmp |= BM_USBPHY_CTRL_SFTRST;
	__raw_writel(tmp, phy_ctrl);
	udelay(10);

	/* Remove CLKGATE and SFTRST */
	tmp = __raw_readl(phy_ctrl);
	tmp &= ~(BM_USBPHY_CTRL_CLKGATE | BM_USBPHY_CTRL_SFTRST);
	__raw_writel(tmp, phy_ctrl);
	udelay(10);

	/* Power up the PHY */
	__raw_writel(0, phy_reg + HW_USBPHY_PWD);

	return 0;
}
/* Notes: configure USB clock*/
static int usbotg_init_ext(struct platform_device *pdev)
{
	struct clk *usb_clk;
	u32 ret;

	usb_clk = clk_get(NULL, "mvf-usb.0");
	clk_enable(usb_clk);
	usb_phy0_clk = usb_clk;

	ret = usbotg_init(pdev);
	if (ret) {
		printk(KERN_ERR "otg init fails......\n");
		return ret;
	}
	if (!otg_used) {
		usbotg_internal_phy_clock_gate(true);
		usb_phy_enable(pdev->dev.platform_data);
		/*
		 * after the phy reset,can not read the value for id/vbus at
		 * the register of otgsc ,cannot  read at once ,need delay 3 ms
		 */
		mdelay(3);
	}
	otg_used++;

	return ret;
}

static void usbotg_uninit_ext(struct platform_device *pdev)
{
	struct fsl_usb2_platform_data *pdata = pdev->dev.platform_data;

	clk_disable(usb_phy0_clk);
	clk_put(usb_phy0_clk);

	usbotg_uninit(pdata);
	otg_used--;
}

static void usbotg_clock_gate(bool on)
{
	pr_debug("%s: on is %d\n", __func__, on);
	if (on)
		clk_enable(usb_phy0_clk);
	else
		clk_disable(usb_phy0_clk);
}
/*
void mvf_set_otghost_vbus_func(driver_vbus_func driver_vbus)
{
	dr_utmi_config.platform_driver_vbus = driver_vbus;
}
*/
static void _dr_discharge_line(bool enable)
{
	void __iomem *phy_reg = MVF_IO_ADDRESS(MVF_USBPHY0_BASE_ADDR);
	if (enable) {
		__raw_writel(BF_USBPHY_DEBUG_ENHSTPULLDOWN(0x3),
				phy_reg + HW_USBPHY_DEBUG_SET);
		__raw_writel(BF_USBPHY_DEBUG_HSTPULLDOWN(0x3),
				phy_reg + HW_USBPHY_DEBUG_SET);
	} else {
		__raw_writel(BF_USBPHY_DEBUG_ENHSTPULLDOWN(0x3),
				phy_reg + HW_USBPHY_DEBUG_CLR);
		__raw_writel(BF_USBPHY_DEBUG_HSTPULLDOWN(0x3),
				phy_reg + HW_USBPHY_DEBUG_CLR);
	}

}

/* Below two macros are used at otg mode to indicate usb mode*/
#define ENABLED_BY_HOST   (0x1 << 0)
#define ENABLED_BY_DEVICE (0x1 << 1)

#ifdef CONFIG_USB_EHCI_ARC_OTG
#endif /* CONFIG_USB_EHCI_ARC_OTG */


#ifdef CONFIG_USB_GADGET_ARC
#endif /* CONFIG_USB_GADGET_ARC */

void __init mvf_usb_dr_init(void)
{
	struct platform_device *pdev;
	u32 reg;
#ifdef CONFIG_USB_GADGET_ARC
	dr_utmi_config.operating_mode = DR_UDC_MODE;
	dr_utmi_config.platform_suspend = NULL;
	dr_utmi_config.platform_resume  = NULL;

	pdev = mvf_add_fsl_usb2_udc(&dr_utmi_config);

	__raw_writel(((0x01 << 30) | 0x2), ANADIG_USB1_MISC);

	__raw_writel(0x00000894, USBPHY1_CTRL);
	udelay(10);

	__raw_writel(0x0, USBPHY1_PWD);
	__raw_writel(0x1, USBPHY1_IP);
	udelay(20);
	__raw_writel(0x7, USBPHY1_IP);

	reg = __raw_readl(USBPHY1_DEBUG);
	reg &= ~0x40000000; /* clear gate clock */
	__raw_writel(reg, USBPHY1_DEBUG);

	__raw_writel(0x10000007, USBPHY1_TX);
	reg = __raw_readl(USBPHY1_CTRL);
	reg |= ((0xD1 << 11) | 0x6);
	__raw_writel(reg, USBPHY1_CTRL);

	reg = __raw_readl(ANADIG_USB1_VBUS_DETECT);
	reg |= 0xE8;
	__raw_writel(reg, ANADIG_USB1_VBUS_DETECT);
	__raw_writel(0x001c0000, ANADIG_USB1_CHRG_DETECT);
#endif

}
