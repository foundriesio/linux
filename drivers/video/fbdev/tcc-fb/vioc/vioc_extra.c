/*
 * linux/include/video/tcc/vioc_extra.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/vioc_extra.h>

static volatile void __iomem *pr_reg; // playready register variable
static volatile void __iomem *pr_size_reg; // playready video src size register variable
// static int playready_off = 0;					// static variable sub
// display on/off flag

void VIOC_OUTCFG_set_disp_size(unsigned int width, unsigned int height)
{
	if (pr_size_reg) {
		unsigned long val;
		val = (((height & 0xFFFF) << CH_SIZE_HEIGHT_SHIFT) |
		       ((width & 0xFFFF) << CH_SIZE_WIDTH_SHIFT));
		__raw_writel(val, pr_size_reg + OPL_SIZE);
	}
}

unsigned int VIOC_OUTCFG_get_disp_size(void)
{
	if (pr_size_reg) {
		unsigned int size = __raw_readl(pr_size_reg + OPL_SIZE);
		pr_info("\e[33m[INF][OUTCFG] size %dx%d\e[0m\n",
		       ((size & CH_SIZE_WIDTH_MASK) >> CH_SIZE_WIDTH_SHIFT),
		       ((size & CH_SIZE_HEIGHT_MASK) >> CH_SIZE_HEIGHT_SHIFT));
		return size;
	} else {
		return 0;
	}
}

/*
 * VIOC_OUTCFG_dispStatus
 *
 * 0x1200A808 (HwVIOC_VINDEMUX.uVIN_DEMUX_BLANK1)
 * ----------------------------------------------
 *  bit 13: OPL FixedKey bit (1: Fixed Key channel, 0: clear channel)
 *  bit 12: OPL PlayReady bit (1: PlayReady channel, 0: clear channel)
 *  bit 11: OPL PlayReady & no/fail HDCP (1: PR && no/fail HDCP, 0: !PR || HDCP)
 *  bit 9: force  sub display off (1: display off, 0: display on)
 *  bit 8: force main display off (1: display off, 0: display on)
 *  bit 2: component status (1: detected, 0: not detected, -1: don't care)
 *  bit 1: composite status (1: detected, 0: not detected, -1: don't care)
 *  bit 0:      hdmi status (1: detected, 0: not detected, -1: don't care)
 *
 * return value: current status
 */
int VIOC_OUTCFG_dispStatus(int hdmi, int composite, int component, int sub_off,
			   int main_off)
{
	if (pr_reg) {
		int ret;
		unsigned long val;
		if (hdmi != -1) {
			val = (__raw_readl(pr_reg + OPL_BLANK) &
			       ~(PR_HDMI_MASK));
			val |= ((!!hdmi) << PR_HDMI_SHIFT);
			__raw_writel(val, pr_reg + OPL_BLANK);
		}

		if (composite != -1) {
			val = (__raw_readl(pr_reg + OPL_BLANK) &
			       ~(PR_COMPOSITE_MASK));
			val |= ((!!composite) << PR_COMPOSITE_SHIFT);
			__raw_writel(val, pr_reg + OPL_BLANK);
		}

		if (component != -1) {
			val = (__raw_readl(pr_reg + OPL_BLANK) &
			       ~(PR_COMPONENT_MASK));
			val |= ((!!component) << PR_COMPONENT_SHIFT);
			__raw_writel(val, pr_reg + OPL_BLANK);
		}

		if (main_off != -1) {
			val = (__raw_readl(pr_reg + OPL_BLANK) &
			       ~(PR_MAIN_MASK));
			val |= ((!!main_off) << PR_MAIN_SHIFT);
			__raw_writel(val, pr_reg + OPL_BLANK);
		}

		if (sub_off != -1) {
			val = (__raw_readl(pr_reg + OPL_BLANK) &
			       ~(PR_SUB_MASK));
			val |= ((!!sub_off) << PR_SUB_SHIFT);
			__raw_writel(val, pr_reg + OPL_BLANK);
		}

		ret = (__raw_readl(pr_reg + OPL_BLANK) & PR_CH_STATUS_MASK);
		// pr_debug("\e[33m[DBG][OUTCFG] %s: (b%d%d 0000 0%d%d%d) = 0x%x \e[0m \n",
		// __func__, main_off, sub_off, component, composite, hdmi, ret);

		return ret;
	} else {
		return 0;
	}
}

/*
 * PlayReady_main_off, PlayReady_sub_off
 *
 * return value:
 *  1: main display off
 *  0: main display on
 */
int PlayReady_main_off(void)
{
	int ret;
	ret = !!(VIOC_OUTCFG_dispStatus(-1, -1, -1, -1, -1) & PR_MAIN_MASK);
	return ret;
}

int PlayReady_sub_off(void)
{
	int ret;
	ret = !!(VIOC_OUTCFG_dispStatus(-1, -1, -1, -1, -1) & PR_SUB_MASK);
	return ret;
}

/*
 * OPL PlayReady && No/Fail HDCP:
 *   0: !PlayReady || HDCP
 *   1: PlayReady && No/Fail HDCP
 */
void OPL_PlayReady_NoHDCP_set(unsigned long val)
{
	if (pr_reg) {
		unsigned long set;
		set = (__raw_readl(pr_reg + OPL_BLANK) & ~(OPL_PR_NoHDCP_MASK));
		set |= ((val & 0x1) << OPL_PR_NoHDCP_SHIFT);
		__raw_writel(set, pr_reg + OPL_BLANK);
	}
}

unsigned long OPL_PlayReady_NoHDCP_get(void)
{
	unsigned long val = 0;
	if (pr_reg)
		val = !!(__raw_readl(pr_reg + OPL_BLANK) & OPL_PR_NoHDCP_MASK);
	return val;
}

/*
 * OPL PlayReady channel:
 *   0: clear channel
 *   1: PlayREady channel
 */
void OPL_PlayReady_set(unsigned long ch)
{
	if (pr_reg) {
		unsigned long val;
		val = (__raw_readl(pr_reg + OPL_BLANK) & ~(OPL_PR_MASK));
		val |= ((ch & 0x1) << OPL_PR_SHIFT);
		__raw_writel(val, pr_reg);
	}
}

unsigned long OPL_PlayReady_get(void)
{
	unsigned long ch = 0;
	if (pr_reg)
		ch = !!(__raw_readl(pr_reg + OPL_BLANK) & OPL_PR_MASK);
	return ch;
}

/*
 * OPL Fixed Key channel:
 *   0: clear channel
 *   1: Fixed Key channel
 */
void OPL_FixedKey_set(unsigned long ch)
{
	if (pr_reg) {
		unsigned long val;
		val = (__raw_readl(pr_reg + OPL_BLANK) & ~(OPL_FK_MASK));
		val |= ((ch & 0x1) << OPL_FK_SHIFT);
		__raw_writel(val, pr_reg + OPL_BLANK);
	}
}

unsigned long OPL_FixedKey_get(void)
{
	unsigned long ch = 0;
	if (pr_reg)
		ch = !!(__raw_readl(pr_reg + OPL_BLANK) & OPL_FK_MASK);
	return ch;
}

static int __init vioc_extra_init(void)
{
	struct device_node *vioc_vin_demux_np, *vioc_vin_np;

	vioc_vin_demux_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_vin_demux");
	if (vioc_vin_demux_np) {
		pr_reg = (volatile void __iomem *)of_iomap(vioc_vin_demux_np,
					is_VIOC_REMAP ? 1 : 0);
		if (pr_reg)
			__raw_writel(0xFFFFFFFF, pr_reg + 0x08);
	}

	vioc_vin_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_vin");
	if (vioc_vin_np) {
		pr_size_reg = (volatile void __iomem *)of_iomap(vioc_vin_np,
						is_VIOC_REMAP ? 0 + VIOC_VIN_MAX : 0);
		if (pr_size_reg)
			__raw_writel(0x00000000, pr_reg + 0x10);
	}

	return 0;
}
arch_initcall(vioc_extra_init);
