// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2014 Renesas Electronics Europe Limited
 *
 * Michel Pollet <michel.pollet@bp.renesas.com>, <buserror@gmail.com>
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>

#include "rzn1-clock.h"
#include <dt-bindings/clock/rzn1-clocks.h>
#include <dt-bindings/soc/rzn1-sysctrl.h>

#define _BIT(_r, _p) { .reg = _r, .pos = _p }

#define _CLK(_n, _clk, _rst, _rdy, _midle, _scon, _mirack, _mistat) \
	{ .name = _n, .clock = _clk, .reset = _rst, \
		.ready = _rdy, .masteridle = _midle }

#include "rzn1-clkctrl-tables.h"

static rzn1_clk_hook hook[RZN1_CLK_COUNT];

void rzn1_clk_set_hook(int clkdesc_id, rzn1_clk_hook clk_hook)
{
	hook[clkdesc_id] = clk_hook;
}

const struct rzn1_clkdesc *rzn1_get_clk_desc(int clkdesc_id)
{
	static struct rzn1_clkdesc zero = _CLK("*unknown*",
				{}, {}, {}, {}, {}, {}, {});

	return clkdesc_id < RZN1_CLK_COUNT ?
			&rzn1_clock_list[clkdesc_id] :
			&zero;
}
EXPORT_SYMBOL_GPL(rzn1_get_clk_desc);

static void clk_mgr_desc_set(const struct rzn1_onereg *one, int on)
{
	u32 *reg = ((u32 *)rzn1_sysctrl_base()) + one->reg;
	u32 val = clk_readl(reg);

	val = (val & ~(1 << one->pos)) | ((!!on) << one->pos);
	clk_writel(val, reg);
}

void rzn1_clk_set_gate(int clkdesc_id, int on)
{
	const struct rzn1_clkdesc *g = rzn1_get_clk_desc(clkdesc_id);

	if (!g->clock.reg) {
		pr_err("%s: No reg for clk ID %d\n", __func__, clkdesc_id);
		return;
	}

	if (hook[clkdesc_id])
		if (hook[clkdesc_id](clkdesc_id, RZN1_CLK_HOOK_GATE_PRE, on))
			return;
	clk_mgr_desc_set(&g->clock, on);

	if (hook[clkdesc_id])
		if (hook[clkdesc_id](clkdesc_id, RZN1_CLK_HOOK_GATE_SET, on))
			return;

	/* De-assert reset */
	if (g->reset.reg)
		clk_mgr_desc_set(&g->reset, 1);

	/* Hardware manual recommends 5us delay after enabling clock & reset */
	udelay(5);

	/* If the peripheral is memory mapped (i.e. an AXI slave), there is an
	 * associated SLVRDY bit in the System Controller that needs to be set
	 * so that the FlexWAY bus fabric passes on the read/write requests.
	 */
	if (g->ready.reg)
		clk_mgr_desc_set(&g->ready, on);

	/* Clear 'Master Idle Request' bit */
	if (g->masteridle.reg)
		clk_mgr_desc_set(&g->masteridle, !on);

	if (hook[clkdesc_id])
		hook[clkdesc_id](clkdesc_id, RZN1_CLK_HOOK_GATE_POST, on);

	/* Note: We don't wait for FlexWAY Socket Connection signal */
}
EXPORT_SYMBOL_GPL(rzn1_clk_set_gate);

static void rzn1_release_reset(int clkdesc_id)
{
	const struct rzn1_clkdesc *g = rzn1_get_clk_desc(clkdesc_id);

	if (!g->reset.reg) {
		pr_err("%s: No reg for clk ID %d\n", __func__, clkdesc_id);
		return;
	}

	clk_mgr_desc_set(&g->reset, 1);

	/* Hardware manual recommends 5us delay after enabling clock & reset */
	udelay(5);
}

int rzn1_clk_is_gate_enabled(int clkdesc_id)
{
	const struct rzn1_clkdesc *g = rzn1_get_clk_desc(clkdesc_id);
	u32 *reg;

	if (!g->clock.reg) {
		pr_err("%s: No reg for clk ID %d\n", __func__, clkdesc_id);
		return 0;
	}

	reg = ((u32 *)rzn1_sysctrl_base()) + g->clock.reg;
	return !!(clk_readl(reg) & (1 << g->clock.pos));
}
EXPORT_SYMBOL_GPL(rzn1_clk_is_gate_enabled);

static ssize_t sys_clk_set_rate_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	strcpy(buf, "Usage: echo <clock name> <rate in hz> >clk_set_rate\n");
	return strlen(buf);
}

static ssize_t sys_clk_set_rate_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	char copy[count + 1];
	char *p = copy;
	const char *trate;
	const char *name;
	long rate;
	struct clk *clk;

	/* the 'buf' is const, so we can't strsep into it */
	strncpy(copy, buf, sizeof(copy));
	name = strsep(&p, " ");
	if (name)
		trate = strsep(&p, " ");
	if (!name || !trate)
		return count;
	pr_devel("%s clock:%s rate:%s\n", __func__, name, trate);
	if (kstrtol(trate, 10, &rate) != 0) {
		pr_err("%s %s, invalid rate %s\n", __func__, name, trate);
		return count;
	}
	pr_devel("%s %s requested rate %ld\n", __func__, name, rate);

	clk = __clk_lookup(name);
	if (IS_ERR(clk)) {
		pr_err("%s %s: clock not found\n", __func__, name);
		return -EINVAL;
	}
	pr_devel("%s clk %s found\n", __func__, name);
	pr_devel("%s %s current rate %lu\n", __func__, name,
		 clk_get_rate(clk));
	if (rate > 0) {
		if (clk_set_rate(clk, rate)) {
			pr_err("%s %s FAILED\n", __func__, name);
		} else {
			pr_info("%s %s set to %lu\n", __func__, name,
				clk_get_rate(clk));
		}
	}
	return count;
}

static struct kobj_attribute clk_set_rate_attribute =
	__ATTR(clk_set_rate, 0644,
	       sys_clk_set_rate_show, sys_clk_set_rate_store);

static struct attribute *attrs[] = {
	&clk_set_rate_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *grp_kobj;
static int __init rzn1_clock_sys_init(void)
{
	int retval;

	grp_kobj = kobject_create_and_add("rzn1", kernel_kobj);
	if (!grp_kobj)
		return -ENOMEM;
	retval = sysfs_create_group(grp_kobj, &attr_group);
	if (retval)
		kobject_put(grp_kobj);
	return retval;
}
postcore_initcall(rzn1_clock_sys_init);

#define USBF_EPCTR		(RZN1_USB_DEV_BASE + 0x1000 + 0x10)
#define USBF_EPCTR_EPC_RST	BIT(0)
#define USBF_EPCTR_PLL_RST	BIT(2)
#define USBF_EPCTR_DIRPD	BIT(12)

#define USBH_USBCTR		(RZN1_USB_HOST_BASE + 0x10000 + 0x834)
#define USBH_USBCTR_USBH_RST	BIT(0)
#define USBH_USBCTR_PCICLK_MASK	BIT(1)
#define USBH_USBCTR_PLL_RST	BIT(2)
#define USBH_USBCTR_DIRPD	BIT(9)

#define setbits_le32(addr, bits) \
	writel(readl((void *)(addr)) |  (bits), (void *)(addr))
#define clrbits_le32(addr, bits) \
	writel(readl((void *)(addr)) & ~(bits), (void *)(addr))

static int rzn1_usb_pll_locked(void)
{
	return (rzn1_sysctrl_readl(RZN1_SYSCTRL_REG_USBSTAT) &
			BIT(RZN1_SYSCTRL_REG_USBSTAT_PLL_LOCK));
}

static int rzn1_usb_clock_hook(int clkdesc_id, int operation, u32 value)
{
	u32 val, h2mode = 0;
	struct device_node *np;
	void *epctr = NULL;
	void *usbctr = NULL;
	int hclk_usbh;
	int hclk_usbf;

	if (operation != RZN1_CLK_HOOK_GATE_POST || value != 1)
		return 0;

	np = of_find_node_by_path("/chosen");
	if (np && of_property_read_bool(np, "rzn1,h2mode"))
		h2mode = BIT(RZN1_SYSCTRL_REG_CFG_USB_H2MODE);

	/*
	 * If the PLL is already started and the USB configuration is in the
	 * right mode, we don't need to do anything
	 */
	val = rzn1_sysctrl_readl(RZN1_SYSCTRL_REG_CFG_USB);
	if (((val & BIT(RZN1_SYSCTRL_REG_CFG_USB_H2MODE)) == h2mode) &&
	    rzn1_usb_pll_locked()) {
		pr_info("rzn1: USB PLL already started\n");
		return 0;
	}
	pr_info("rzn1: USB in %s mode\n", h2mode ? "2xHost" : "1xFunc+1xHost");

	/*
	 * When enabling the USB PLL, we need to put both the USB Host and
	 * Function parts into reset. That means we have to enable the clocks to
	 * access these parts, and put the clocks back how they were afterwards.
	 */
	hclk_usbh = rzn1_clk_is_gate_enabled(RZN1_HCLK_USBH_ID);
	hclk_usbf = rzn1_clk_is_gate_enabled(RZN1_HCLK_USBF_ID);
	rzn1_clk_set_gate(RZN1_HCLK_USBH_ID, 1);
	rzn1_clk_set_gate(RZN1_HCLK_USBF_ID, 1);

	epctr = ioremap(USBF_EPCTR, 4);
	usbctr = ioremap(USBH_USBCTR, 4);

	/* Hold USBF and USBH in reset */
	writel(USBH_USBCTR_USBH_RST | USBH_USBCTR_PCICLK_MASK, usbctr);
	writel(USBF_EPCTR_EPC_RST, epctr);
	/* Hold USBPLL in reset */
	setbits_le32(usbctr, USBH_USBCTR_PLL_RST);
	udelay(2);

	/* Power down USB PLL, setting any DIRPD bit will do this */
	setbits_le32(usbctr, USBH_USBCTR_DIRPD);

	/* Stop USB suspend from powering down the USB PLL */
	/* Note: have to update these bits at the same time */
	val |= BIT(RZN1_SYSCTRL_REG_CFG_USB_FRCLK48MOD);
	val &= ~BIT(RZN1_SYSCTRL_REG_CFG_USB_DIRPD);
	val &= ~BIT(RZN1_SYSCTRL_REG_CFG_USB_H2MODE);
	val |= h2mode;
	rzn1_sysctrl_writel(val, RZN1_SYSCTRL_REG_CFG_USB);

	/* Power up USB PLL, all DIRPD bits need to be cleared */
	clrbits_le32(epctr, USBF_EPCTR_DIRPD);
	clrbits_le32(usbctr, USBH_USBCTR_DIRPD);
	usleep_range(1000, 2000);

	/* Release USBPLL reset, either PLL_RST bit will do this */
	clrbits_le32(usbctr, USBH_USBCTR_PLL_RST);

	iounmap(epctr);
	iounmap(usbctr);

	/* Reinstate the USB Func and Host clocks */
	rzn1_clk_set_gate(RZN1_HCLK_USBH_ID, hclk_usbh);
	rzn1_clk_set_gate(RZN1_HCLK_USBF_ID, hclk_usbf);

	while (!rzn1_usb_pll_locked())
		;

	return 0;
}

static int rzn1_eth_reset_hook(int clkdesc_id, int operation, u32 value)
{
	if ((operation != RZN1_CLK_HOOK_GATE_POST) || (value != 1))
		return 0;

	rzn1_release_reset(RZN1_RSTN_CLK25_SWITCHCTRL_ID);
	rzn1_release_reset(RZN1_RSTN_ETH_SWITCHCTRL_ID);

	return 0;
}

static void __init rzn1_clock_init(struct device_node *node)
{
	rzn1_sysctrl_init();

	/*
	 * This hook will decide of the fate of the HOST vs
	 * DEVICE for the usb port -- the board designer has to enable
	 * either of the driver in the DTS, or have the bootloader set
	 * the appropriate DT property before starting the kernel.
	 * It also ensures the USB PLL is enabled.
	 */
	rzn1_clk_set_hook(RZN1_HCLK_USBPM_ID, rzn1_usb_clock_hook);
	rzn1_clk_set_hook(RZN1_CLK_48MHZ_PG_F_ID, rzn1_usb_clock_hook);

	/* This hook releases the RGMII/RMII Converter 50MHz and 25MHz resets */
	rzn1_clk_set_hook(RZN1_HCLK_SWITCH_RG_ID, rzn1_eth_reset_hook);
}

CLK_OF_DECLARE(rzn1_clock, "renesas,rzn1-clock", rzn1_clock_init);
