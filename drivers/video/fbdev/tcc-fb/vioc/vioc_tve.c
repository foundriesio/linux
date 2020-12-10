/*
 * Copyright (C) Telechips, Inc.
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
#include <linux/io.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <video/tcc/vioc_tve.h>
#include <video/tcc/vioc_ddicfg.h>

/* Debugging stuff */
//#define DBG_TVE
#ifdef DBG_TVE
#define dprintk(msg...)	pr_info("\e[33m[DBG][TVO]\e[0m " msg)
#else
#define dprintk(msg...)
#endif

static struct clk *tve_clk_ntscpal;
static struct clk *tve_clk_dac;

static volatile void __iomem *pTve;
static volatile void __iomem *pTve_VEN;


void internal_tve_set_mode(unsigned int type)
{
	volatile void __iomem *reg = VIOC_TVE_GetAddress();
	unsigned int val;

	switch (type) {
	case NTSC_M:
		val = ((0x1 << TVE_ECMDA_PWDNENC_SHIFT) |
		       (0x1 << TVE_ECMDA_PED_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case NTSC_M_J:
		val = ((0x1 << TVE_ECMDA_PWDNENC_SHIFT) |
		       (0x1 << TVE_ECMDA_FDRST_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case NTSC_N:
		val = ((0x1 << TVE_ECMDA_PED_SHIFT) |
		       (0x1 << TVE_ECMDA_IFMT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case NTSC_N_J:
		val = (0x1 << TVE_ECMDA_IFMT_SHIFT);
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case NTSC_443:
		val = ((0x1 << TVE_ECMDA_FSCCELL_SHIFT) |
		       (0x1 << TVE_ECMDA_PED_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case PAL_M:
		val = ((0x2 << TVE_ECMDA_FSCCELL_SHIFT) |
		       (0x1 << TVE_ECMDA_PED_SHIFT) |
		       (0x1 << TVE_ECMDA_PHALT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case PAL_N:
		val = ((0x3 << TVE_ECMDA_FSCCELL_SHIFT) |
		       (0x1 << TVE_ECMDA_PED_SHIFT) |
		       (0x1 << TVE_ECMDA_IFMT_SHIFT) |
		       (0x1 << TVE_ECMDA_PHALT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case PAL_B:
	case PAL_G:
	case PAL_H:
	case PAL_I:
		val = ((0x1 << TVE_ECMDA_FSCCELL_SHIFT) |
		       (0x1 << TVE_ECMDA_IFMT_SHIFT) |
		       (0x1 << TVE_ECMDA_PHALT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case PSEUDO_PAL:
		val = ((0x1 << TVE_ECMDA_FSCCELL_SHIFT) |
		       (0x1 << TVE_ECMDA_PED_SHIFT) |
		       (0x1 << TVE_ECMDA_PHALT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case PSEUDO_NTSC:
		val = ((0x1 << TVE_ECMDA_PED_SHIFT) |
		       (0x1 << TVE_ECMDA_IFMT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;
	}
}

void internal_tve_set_config(unsigned int type)
{
	volatile void __iomem *ptve = VIOC_TVE_GetAddress();
	volatile void __iomem *ptve_ven = VIOC_TVE_VEN_GetAddress();
	unsigned int val;

	dprintk("%s\n", __func__);

	// Disconnect LCDC with NTSC/PAL encoder
	val = (__raw_readl(ptve_ven + VENCON) & ~(VENCON_EN_MASK));
	val |= (0x1 << VENCON_EN_SHIFT);
	__raw_writel(val, ptve_ven + VENCON);

	// Set ECMDA Register
	internal_tve_set_mode(type);

	// Set ECMDB Register
	val = (__raw_readl(ptve + TVE_ECMDB) &
	       ~(TVE_ECMDB_CBW_MASK | TVE_ECMDB_YBW_MASK));
	val |= ((0x2 << TVE_ECMDB_CBW_SHIFT) | (0x2 << TVE_ECMDB_YBW_SHIFT));
	__raw_writel(val, ptve + TVE_ECMDB);

	// Set SCH Register
	val = ((0x20 << TVE_SCH_SCH_SHIFT));
	__raw_writel(val, ptve + TVE_SCH);

	// Set SAT Register
#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	if (type == NTSC_M)
		val = (0x10 << TVE_SAT_SAT_SHIFT);
	else
		val = (0x08 << TVE_SAT_SAT_SHIFT);
#else
#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) ||                              \
	defined(CONFIG_TCC_COMPOSITE_COLOR_SPACE_YUV)
	if (type == NTSC_M)
		val = (0x06 << TVE_SAT_SAT_SHIFT);
	else
		val = (0x08 << TVE_SAT_SAT_SHIFT);
#else
	if (type == NTSC_M)
		val = (0x30 << TVE_SAT_SAT_SHIFT);
	else
		val = (0x08 << TVE_SAT_SAT_SHIFT);
#endif
#endif
	__raw_writel(val, ptve + TVE_SAT);

	// Set DACSEL Register
	val = (__raw_readl(ptve + TVE_DACSEL) & ~(TVE_DACSEL_DACSEL_MASK));
	val |= (0x1 << TVE_DACSEL_DACSEL_SHIFT);
	__raw_writel(val, ptve + TVE_DACSEL);

	// Set DACPD Register
	val = (__raw_readl(ptve + TVE_DACPD) & ~(TVE_DACPD_PD_MASK));
	__raw_writel(val, ptve + TVE_DACPD);

	val = (__raw_readl(ptve + TVE_ICNTL) & ~(TVE_ICNTL_VSIP_MASK));
	val |= (0x1 << TVE_ICNTL_VSIP_SHIFT);
	__raw_writel(val, ptve + TVE_ICNTL);

	val = (__raw_readl(ptve + TVE_ICNTL) & ~(TVE_ICNTL_HSVSP_MASK));
	val |= (0x1 << TVE_ICNTL_HSVSP_SHIFT);
	__raw_writel(val, ptve + TVE_ICNTL);

	val = (__raw_readl(ptve + TVE_ICNTL) & ~(TVE_ICNTL_ISYNC_MASK));
#ifdef TCC_COMPOSITE_CCIR656
	val |= (0x6 << TVE_ICNTL_ISYNC_SHIFT);
#else
	val |= (0x2 << TVE_ICNTL_ISYNC_SHIFT);
#endif
	__raw_writel(val, ptve + TVE_ICNTL);

	// Set the Horizontal Offset
	val = (__raw_readl(ptve + TVE_HVOFFSET) & ~(TVE_HVOFFSET_HOFFSET_MASK));
	__raw_writel(val, ptve + TVE_HVOFFSET);

	val = (__raw_readl(ptve + TVE_HOFFSET) & ~(TVE_HOFFSET_HOFFSET_MASK));
	__raw_writel(val, ptve + TVE_HOFFSET);


	// Set the Vertical Offset
	//	BITCSET(pHwTVE->HVOFFST.nREG, 0x08, ((1 & 0x100)>>5));
	val = (__raw_readl(ptve + TVE_HVOFFSET) & ~(TVE_HVOFFSET_VOFFST_MASK));
	// val |= (0x1 << TVE_HVOFFSET_VOFFST_SHIFT);
	__raw_writel(val, ptve + TVE_HVOFFSET);

	val = (__raw_readl(ptve + TVE_VOFFSET) & ~(TVE_VOFFSET_VOFFSET_MASK));
	val |= (0x1 << TVE_VOFFSET_VOFFSET_SHIFT);
	__raw_writel(val, ptve + TVE_VOFFSET);

	// Set the Digital Output Format
	val = (__raw_readl(ptve + TVE_HVOFFSET) & ~(TVE_HVOFFSET_INSEL_MASK));
	val |= (0x2 << TVE_HVOFFSET_INSEL_SHIFT);
	__raw_writel(val, ptve + TVE_HVOFFSET);

	// Set HSVSO Register
	val = (__raw_readl(ptve + TVE_HSVSO) & ~(TVE_HSVSO_HSOE_MASK));
	__raw_writel(val, ptve + TVE_HSVSO);

	val = (__raw_readl(ptve + TVE_HSOE) & ~(TVE_HSOE_HSOE_MASK));
	__raw_writel(val, ptve + TVE_HSOE);

	val = (__raw_readl(ptve + TVE_HSVSO) & ~(TVE_HSVSO_HSOB_MASK));
	__raw_writel(val, ptve + TVE_HSVSO);

	val = (__raw_readl(ptve + TVE_HSOB) & ~(TVE_HSOB_HSOB_MASK));
	__raw_writel(val, ptve + TVE_HSOB);

	val = (__raw_readl(ptve + TVE_HSVSO) & ~(TVE_HSVSO_VSOB_MASK));
	__raw_writel(val, ptve + TVE_HSVSO);

	val = (__raw_readl(ptve + TVE_VSOB) & ~(TVE_VSOB_VSOB_MASK));
	__raw_writel(val, ptve + TVE_VSOB);

	// Set VSOE Register
	val = (__raw_readl(ptve + TVE_VSOE) &
	       ~(TVE_VSOE_VSOE_MASK | TVE_VSOE_NOVRST_MASK |
		 TVE_VSOE_VSOST_MASK));
	__raw_writel(val, ptve + TVE_VSOE);

	// Set the Connection Type
	val = (__raw_readl(ptve_ven + VENCIF) & ~(VENCIF_FMT_MASK));
	val |= (0x1 << VENCIF_FMT_SHIFT);
	__raw_writel(val, ptve_ven + VENCIF);

	val = (__raw_readl(ptve_ven + VENCON) & ~(VENCON_EN_MASK));
	val |= (0x1 << VENCON_EN_SHIFT);
	__raw_writel(val, ptve_ven + VENCON);

	val = (__raw_readl(ptve + TVE_DACPD) & ~(TVE_DACPD_PD_MASK));
	val |= (0x1 << TVE_DACPD_PD_SHIFT);
	__raw_writel(val, ptve + TVE_DACPD);

	val = (__raw_readl(ptve + TVE_ECMDA) & ~(TVE_ECMDA_PWDNENC_MASK));
	val |= (0x1 << TVE_ECMDA_PWDNENC_SHIFT);
	__raw_writel(val, ptve + TVE_ECMDA);
}

void internal_tve_clock_onoff(unsigned int onoff)
{
	dprintk("%s(%d)\n", __func__, onoff);

	if (onoff) {
		// vdac on, display bus isolation
		clk_prepare_enable(tve_clk_dac);
		// tve on, ddi_config
		clk_prepare_enable(tve_clk_ntscpal);
		#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		// dac on, ddi_config
		VIOC_DDICONFIG_DAC_PWDN_Control(NULL, DAC_ON);
		#endif
	} else {
		// vdac off, display bus isolation
		clk_disable_unprepare(tve_clk_dac);
		// tve off, ddi_config
		clk_disable_unprepare(tve_clk_ntscpal);
		#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		// dac off, ddi_config
		VIOC_DDICONFIG_DAC_PWDN_Control(NULL, DAC_OFF);
		#endif
	}
}

void internal_tve_enable(unsigned int type, unsigned int onoff)
{
	volatile void __iomem *ptve = VIOC_TVE_GetAddress();
	volatile void __iomem *ptve_ven = VIOC_TVE_VEN_GetAddress();
	unsigned int val;

	dprintk("%s(%d)\n", __func__, onoff);

	if (onoff) {
		internal_tve_set_config(type);
		val = (__raw_readl(ptve_ven + VENCON) & ~(VENCON_EN_MASK));
		val |= (0x1 << VENCON_EN_SHIFT);
		__raw_writel(val, ptve_ven + VENCON);
		val = (__raw_readl(ptve + TVE_DACPD) & ~(TVE_DACPD_PD_MASK));
		val |= (0x1 << TVE_DACPD_PD_SHIFT);
		__raw_writel(val, ptve + TVE_DACPD);
		val = (__raw_readl(ptve + TVE_ECMDA) &
		       ~(TVE_ECMDA_PWDNENC_MASK));
		__raw_writel(val, ptve + TVE_ECMDA);
	} else {
		val = (__raw_readl(ptve_ven + VENCON) & ~(VENCON_EN_MASK));
		__raw_writel(val, ptve_ven + VENCON);
		val = (__raw_readl(ptve + TVE_DACPD) & ~(TVE_DACPD_PD_MASK));
		__raw_writel(val, ptve + TVE_DACPD);
		val = (__raw_readl(ptve + TVE_ECMDA) &
		       ~(TVE_ECMDA_PWDNENC_MASK));
		val |= (0x1 << TVE_ECMDA_PWDNENC_SHIFT);
		__raw_writel(val, ptve + TVE_ECMDA);
	}
}

void internal_tve_init(void)
{
	struct device_node *ViocTve_np;

	dprintk("%s\n", __func__);

	ViocTve_np = of_find_compatible_node(NULL, NULL, "telechips,tcc-tve");

	if (ViocTve_np) {
		internal_tve_clock_onoff(1);
		internal_tve_enable(0, 0);
		internal_tve_clock_onoff(0);
	} else {
		pr_err("[ERR][TVO] %s: can't find vioc tve\n", __func__);
	}
}

unsigned int internal_tve_calc_cgms_crc(unsigned int data)
{
	int i;
	unsigned int org = data; // 0x000c0;
	unsigned int dat;
	unsigned int tmp;
	unsigned int crc[6] = {1, 1, 1, 1, 1, 1};
	unsigned int crc_val;

	dat = org;
	for (i = 0; i < 14; i++) {
		tmp = crc[5];
		crc[5] = crc[4];
		crc[4] = crc[3];
		crc[3] = crc[2];
		crc[2] = crc[1];
		crc[1] = ((dat & 0x01)) ^ crc[0] ^ tmp;
		crc[0] = ((dat & 0x01)) ^ tmp;
		dat = (dat >> 1);
	}

	crc_val = 0;
	for (i = 0; i < 6; i++) {
		crc_val |= crc[i] << (5 - i);
	}

	dprintk("%s data:0x%x crc=0x%x (%d %d %d %d %d %d)\n", __func__, org,
		crc_val, crc[0], crc[1], crc[2], crc[3], crc[4], crc[5]);

	return crc_val;
}

void internal_tve_set_cgms(unsigned char odd_field_en,
			   unsigned char even_field_en, unsigned int data)
{
	volatile void __iomem *ptve = VIOC_TVE_GetAddress();
	unsigned int val;

	if (odd_field_en) {
		val = (__raw_readl(ptve + CGMS_VCTRL) &
		       ~(CGMS_VCTRL_CGEE_MASK));
		val |= (0x1 << CGMS_VCTRL_CGEE_SHIFT);
		__raw_writel(val, ptve + CGMS_VCTRL);
	} else {
		val = (__raw_readl(ptve + CGMS_VCTRL) &
		       ~(CGMS_VCTRL_CGEE_MASK));
		__raw_writel(val, ptve + CGMS_VCTRL);
	}

	if (odd_field_en) {
		val = (__raw_readl(ptve + CGMS_VCTRL) &
		       ~(CGMS_VCTRL_CGOE_MASK));
		val |= (0x1 << CGMS_VCTRL_CGOE_SHIFT);
		__raw_writel(val, ptve + CGMS_VCTRL);
	} else {
		val = (__raw_readl(ptve + CGMS_VCTRL) &
		       ~(CGMS_VCTRL_CGOE_MASK));
		__raw_writel(val, ptve + CGMS_VCTRL);
	}

	__raw_writel((data & 0x3F), ptve + CGMS_CGMSA);
	__raw_writel(((data >> 6) & 0xFF), ptve + CGMS_CGMSB);
	__raw_writel(((data >> 14) & 0x3F), ptve + CGMS_CGMSC);
	dprintk("%s VCTRL[0x%04x] CGMSA[0x%04x] CGMSB[0x%04x] CGMSC[0x%04x]\n",
		__func__, __raw_readl(ptve + CGMS_VCTRL),
		__raw_readl(ptve + CGMS_CGMSA), __raw_readl(ptve + CGMS_CGMSB),
		__raw_readl(ptve + CGMS_CGMSC));
}

void internal_tve_get_cgms(unsigned char *odd_field_en,
			   unsigned char *even_field_en, unsigned int *data,
			   unsigned char *status)
{
	volatile void __iomem *ptve = VIOC_TVE_GetAddress();

	*status = (unsigned char)(
		(__raw_readl(ptve + CGMS_VSTAT) & CGMS_VSTAT_CGRDY_MASK)
		>> CGMS_VSTAT_CGRDY_SHIFT);
	*odd_field_en = (unsigned char)(
		(__raw_readl(ptve + CGMS_VCTRL) & CGMS_VCTRL_CGOE_MASK)
		>> CGMS_VCTRL_CGOE_SHIFT);
	*even_field_en = (unsigned char)(
		(__raw_readl(ptve + CGMS_VCTRL) & CGMS_VCTRL_CGEE_MASK)
		>> CGMS_VCTRL_CGEE_SHIFT);
	*data = ((__raw_readl(ptve + CGMS_CGMSC) << 14)
		| (__raw_readl(ptve + CGMS_CGMSB) << 6)
		| (__raw_readl(ptve + CGMS_CGMSA)));

	dprintk("%s VCTRL[0x%04x] CGMSA[0x%04x] CGMSB[0x%04x] CGMSC[0x%04x]\n",
		__func__,
		__raw_readl(ptve + CGMS_VCTRL),
		__raw_readl(ptve + CGMS_CGMSA),
		__raw_readl(ptve + CGMS_CGMSB),
		__raw_readl(ptve + CGMS_CGMSC));
}

/*
 * internal_tve_set_cgms_helper(1, 1, 0x00c0);	// force set cgms-a
 */
void internal_tve_set_cgms_helper(unsigned char odd_field_en,
				unsigned char even_field_en, unsigned int key)
{
	unsigned int data, crc;

	crc = internal_tve_calc_cgms_crc(key);
	data = (crc << 14) | key;
	internal_tve_set_cgms(odd_field_en, even_field_en, data);

	pr_info("[INF][TVO] CGMS-A %s - Composite\n",
		(odd_field_en | even_field_en) ? "ON" : "OFF");
}

volatile void __iomem *VIOC_TVE_VEN_GetAddress(void)
{
	if (pTve_VEN == NULL)
		pr_err("[ERR][TVO] %s: ADDRESS NULL\n", __func__);

	return pTve_VEN;
}

volatile void __iomem *VIOC_TVE_GetAddress(void)
{
	if (pTve == NULL)
		pr_err("[ERR][TVO] %s: ADDRESS NULL\n", __func__);

	return pTve;
}

static int __init vioc_tve_init(void)
{
	struct device_node *ViocTve_np;

	ViocTve_np = of_find_compatible_node(NULL, NULL, "telechips,tcc-tve");
	if (ViocTve_np == NULL) {
		pr_info("[INF][TVO] disabled\n");
	} else {
		pTve = (volatile void __iomem *)of_iomap(ViocTve_np, 0);
		if (pTve)
			pr_info("[INF][TVO] 0x%p\n", pTve);

		pTve_VEN = (volatile void __iomem *)of_iomap(ViocTve_np, 1);
		if (pTve_VEN)
			pr_info("[INF][TVO] vioc-pTve_VEN: 0x%p\n", pTve_VEN);

		/* get clock information */
		tve_clk_ntscpal = of_clk_get(ViocTve_np, 0);
		if (tve_clk_ntscpal == NULL)
			pr_err("[ERR][TVO] %s: can not get ntscpal clock\n",
				__func__);

		tve_clk_dac = of_clk_get(ViocTve_np, 1);
		if (tve_clk_dac == NULL)
			pr_err("[ERR][TVO] %s: can not get dac clock\n",
				__func__);
	}
	return 0;
}
arch_initcall(vioc_tve_init);
