/*
 * Copyright (C) Telechips Inc.
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
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <asm/system_info.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP
#include <video/tcc/vioc_viqe.h>

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#endif

#ifdef CONFIG_VIOC_MGR
#include <video/tcc/vioc_mgr.h>
extern int vioc_mgr_queue_work(
	unsigned int command, unsigned int blk, unsigned int data0,
	unsigned int data1, unsigned int data2);
#endif

static int debug;
#define dprintk(msg...)						\
	do {							\
		if (debug) {					\
			pr_info("[DBG][VIOC_CONFIG] " msg);	\
		}						\
	} while (0)

int vioc_config_sc_rdma_sel[] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,  0x8,
	0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x11, 0x13,
};

int vioc_config_sc_vin_sel[] = {
	0x10,
	0x12,
	0x0C,
	0x0E,
};

int vioc_config_sc_wdma_sel[] = {
	0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
};

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X)
int vioc_config_viqe_rdma_sel[] = {
	0x00, 0x01, 0x02, 0x03, -1, -1,  0x4, 0x5,  -1,
	-1,   0x06, 0x07, 0x08, -1, 0x9, -1,  0x0B, 0x0D,
};
#elif defined(CONFIG_ARCH_TCC805X)
int vioc_config_viqe_rdma_sel[] = {
	0x00, 0x01, 0x02, 0x03, -1,   -1,  0x4, 0x5,  -1,
	-1,   0x06, 0x07, 0x08, 0x0E, 0x9, 0xF, 0x0B, 0x0D,
};
#else // TCC899X
int vioc_config_viqe_rdma_sel[] = {
	0x00, 0x01, 0x02, 0x03, -1,  -1, -1, 0x5,  -1,
	-1,   0x06, 0x07, 0x08, 0x9, -1, -1, 0x0C, 0x0E,
};
#endif

int vioc_config_viqe_vin_sel[] = {
	0x0A,
	0x0C,
	0x08,
	0x09,
};

int vioc_config_lut_rdma_sel[] = {
	0,    0x1,  0x2,  0x03, 0x04, 0x05, -1, 0x07, 0x08, // rdma 0 ~ 8
	0x09, 0x0A, 0x0B, 0x0C, 0x0D, -1,   -1, 0x11, 0x13,
};

int vioc_config_lut_vin_sel[] = {
	0x10,
	-1,
};
int vioc_config_lut_wdma_sel[] = {
	0x14, 0x15, 0x16, 0x17, -1, 0x19, 0x1A, 0x1B, 0x1C,
};

#ifdef CONFIG_VIOC_CHROMA_INTERPOLATOR
int vioc_config_cin_rdma_sel[] = {
	0x00, 0x01, 0x02, 0x03, -1,   -1, -1, 0x5, -1, // rdma 0 ~ 8
	-1,   0x06, 0x07, 0x08, 0x09, -1, -1, 0xC, -1,
};

int vioc_config_cin_vin_sel[] = {
	0x0B,
	-1,
};
#endif //

#ifdef CONFIG_VIOC_SAR
int vioc_config_sar_rdma_sel[] = {
	0x00, 0x01, 0x02, 0x03, -1,   -1, -1, 0x5, -1, // rdma 0 ~ 8
	-1,   0x06, 0x07, 0x08, 0x09, -1, -1, 0xC, 0xE,
};

int vioc_config_sar_vin_sel[] = {
	0x0B,
	-1,
};
#endif //

#ifdef CONFIG_VIOC_PIXEL_MAPPER
int vioc_config_pixel_mapper_rdma_sel[] = {
	0x00, 0x01, 0x02, 0x03, -1,   -1, -1, 0x5, -1, // rdma 0 ~ 8
	-1,   0x06, 0x07, 0x08, 0x09, -1, -1, 0xC, 0xD,
};

int vioc_config_pixel_mapper_vin_sel[] = {
	0x0B,
	-1,
};
#endif //

#ifdef CONFIG_VIOC_2D_FILTER
int vioc_config_2d_filter_rdma_sel[] = {
	0x0,  0x2,  0x4,  0x6,  0x8,  0xA, -1, 0xD,  0x10,
	0x12, 0x14, 0x16, 0x18, 0x1A, -1,  -1, 0x22, 0x26,
};

int vioc_config_2d_filter_wdma_sel[] = {
	0x28, 0x2A, 0x2C, 0x2E, -1, 0x32, 0x34, 0x36, 0x38,
};

int vioc_config_2d_filter_vin_sel[] = {
	0x20,
	-1,
};

int vioc_config_2d_filter_disp_sel[] = {
	0x3A,
	0x3C,
	-1,
};
#endif // CONFIG_VIOC_2D_FILTER

#ifdef CONFIG_VIOC_CHROMA_INTERPOLATOR
int vioc_config_chroma_interpolator_rdma_sel[] = {
	-1, -1,   0x0,  0x01, -1,   -1, -1, 0x3, -1, // rdma 0 ~ 8
	-1, 0x04, 0x05, 0x06, 0x07, -1, -1, 0x9, 0xA,
};

#endif // CONFIG_VIOC_CHROMA_INTERPOLATOR

#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
int vioc_config_FBCDec_rdma_sel[] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,  0x8,
	0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11,
};
#endif

static volatile void __iomem *pIREQ_reg;

static volatile void __iomem *CalcAddressViocComponent(unsigned int component)
{
	volatile void __iomem *reg;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		switch (get_vioc_index(component)) {
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC898X) \
	|| defined(CONFIG_ARCH_TCC901X)
		case 2:
			reg = (pIREQ_reg + CFG_PATH_RDMA02_OFFSET);
			break;
		case 3:
			reg = (pIREQ_reg + CFG_PATH_RDMA03_OFFSET);
			break;
		case 6:
			reg = (pIREQ_reg + CFG_PATH_RDMA06_OFFSET);
			break;
		case 7:
			reg = (pIREQ_reg + CFG_PATH_RDMA07_OFFSET);
			break;
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		case 10:
			reg = (pIREQ_reg + CFG_PATH_RDMA10_OFFSET);
			break;
#endif
		case 11:
			reg = (pIREQ_reg + CFG_PATH_RDMA11_OFFSET);
			break;
		case 12:
			reg = (pIREQ_reg + CFG_PATH_RDMA12_OFFSET);
			break;
		case 13:
			reg = (pIREQ_reg + CFG_PATH_RDMA13_OFFSET);
			break;
		case 14:
			reg = (pIREQ_reg + CFG_PATH_RDMA14_OFFSET);
			break;
		case 16:
			reg = (pIREQ_reg + CFG_PATH_RDMA16_OFFSET);
			break;
		case 17:
			reg = (pIREQ_reg + CFG_PATH_RDMA17_OFFSET);
			break;
#endif
		default:
			reg = NULL;
			break;
		}
		break;
	case get_vioc_type(VIOC_SCALER):
		switch (get_vioc_index(component)) {
		case 0:
			reg = (pIREQ_reg + CFG_PATH_SC0_OFFSET);
			break;
		case 1:
			reg = (pIREQ_reg + CFG_PATH_SC1_OFFSET);
			break;
		case 2:
			reg = (pIREQ_reg + CFG_PATH_SC2_OFFSET);
			break;
		case 3:
			reg = (pIREQ_reg + CFG_PATH_SC3_OFFSET);
			break;
#if !defined(CONFIG_ARCH_TCC897X)
		case 4:
			reg = (pIREQ_reg + CFG_PATH_SC4_OFFSET);
			break;
#endif
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) \
	|| defined(CONFIG_ARCH_TCC901X) || defined(CONFIG_ARCH_TCC805X)
		case 5:
			reg = (pIREQ_reg + CFG_PATH_SC5_OFFSET);
			break;
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		case 6:
			reg = (pIREQ_reg + CFG_PATH_SC6_OFFSET);
			break;
		case 7:
			reg = (pIREQ_reg + CFG_PATH_SC7_OFFSET);
			break;
#endif
		default:
			reg = NULL;
			break;
		}
		break;
	case get_vioc_type(VIOC_VIQE):
		switch (get_vioc_index(component)) {
		case 0:
			reg = (pIREQ_reg + CFG_PATH_VIQE0_OFFSET);
			break;
#if !(defined(CONFIG_ARCH_TCC897X))
		case 1:
			reg = (pIREQ_reg + CFG_PATH_VIQE1_OFFSET);
			break;
#endif
		default:
			reg = NULL;
			break;
		}
		break;
	case get_vioc_type(VIOC_DEINTLS):
		switch (get_vioc_index(component)) {
		case 0:
			reg = (pIREQ_reg + CFG_PATH_DEINTLS_OFFSET);
			break;
		default:
			reg = NULL;
			break;
		}
		break;

#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		switch (get_vioc_index(component)) {
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC898X) \
	|| defined(CONFIG_ARCH_TCC901X)
		case 0:
			reg = (pIREQ_reg + CFG_PATH_MC0_OFFSET);
			break;
		case 1:
			reg = (pIREQ_reg + CFG_PATH_MC1_OFFSET);
			break;
#endif
		default:
			reg = NULL;
			break;
		}
		break;
#endif
#ifdef CONFIG_VIOC_DTRC_DECOMP
	case get_vioc_type(VIOC_DTRC):
		switch (get_vioc_index(component)) {
		case 0:
			reg = (pIREQ_reg + CFG_PATH_DTRC0_OFFSET);
			break;
		case 1:
			reg = (pIREQ_reg + CFG_PATH_DTRC1_OFFSET);
			break;
		default:
			reg = NULL;
			break;
		}
		break;
#endif

#ifdef CONFIG_VIOC_SAR
	case get_vioc_type(VIOC_SAR):
		switch (get_vioc_index(component)) {
		case 0:
			reg = (pIREQ_reg + CFG_PATH_SAR0_OFFSET);
			break;
		case 1:
			reg = (pIREQ_reg + CFG_PATH_SAR1_OFFSET);
			break;
		default:
			reg = NULL;
			break;
		}
		break;
#endif

#ifdef CONFIG_VIOC_PIXEL_MAPPER
	case get_vioc_type(VIOC_PIXELMAP):
		switch (get_vioc_index(component)) {
		case 0:
			reg = (pIREQ_reg + CFG_PATH_PM0_OFFSET);
			break;
		case 1:
			reg = (pIREQ_reg + CFG_PATH_PM1_OFFSET);
			break;
		default:
			reg = NULL;
			break;
		}
		break;
#endif //

#ifdef CONFIG_VIOC_2D_FILTER
	case get_vioc_type(VIOC_F2D):
		switch (get_vioc_index(component)) {
		case 0:
			reg = (pIREQ_reg + CFG_PATH_F2D0_OFFSET);
			break;
		case 1:
			reg = (pIREQ_reg + CFG_PATH_F2D1_OFFSET);
			break;
		default:
			reg = NULL;
			break;
		}
		break;
#endif //

#ifdef CONFIG_VIOC_CHROMA_INTERPOLATOR
	case get_vioc_type(VIOC_CINTPL):
		switch (get_vioc_index(component)) {
		case 0:
			reg = (pIREQ_reg + CFG_PATH_CIN0_OFFSET);
			break;
		case 1:
			reg = (pIREQ_reg + CFG_PATH_CIN1_OFFSET);
			break;
		default:
			reg = NULL;
			break;
		}
		break;
#endif //

	default:
		pr_err("[ERR][VIOC_CONFIG] %s-%d: wierd component(0x%x) type(0x%x) index(%d)\n",
		       __func__, __LINE__, component, get_vioc_type(component),
		       get_vioc_index(component));
		WARN_ON(1);
		reg = NULL;
		break;
	}

	return reg;
}

static int CheckScalerPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_sc_rdma_sel[get_vioc_index(component)];
		break;
	case get_vioc_type(VIOC_VIN):
		ret = vioc_config_sc_vin_sel[get_vioc_index(component) / 2];
		break;
	case get_vioc_type(VIOC_WDMA):
		ret = vioc_config_sc_wdma_sel[get_vioc_index(component)];
		break;
	default:
		pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}

static int CheckViqePathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_viqe_rdma_sel[get_vioc_index(component)];
		break;
	case get_vioc_type(VIOC_VIN):
		ret = vioc_config_viqe_vin_sel[get_vioc_index(component) / 2];
		break;
	default:
		pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}
#if defined(CONFIG_VIOC_SAR)
int CheckSarPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_sar_rdma_sel[get_vioc_index(component)];
		break;
	case get_vioc_type(VIOC_VIN):
		ret = vioc_config_sar_vin_sel[get_vioc_index(component) / 2];
		break;
	default:
		pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}
#endif //

#ifdef CONFIG_VIOC_PIXEL_MAPPER
int CheckPixelMapPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_pixel_mapper_rdma_sel[get_vioc_index(
			component)];
		break;
	case get_vioc_type(VIOC_VIN):
		ret = vioc_config_pixel_mapper_vin_sel
			[get_vioc_index(component) / 2];
		break;
	default:
		pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}
#endif //

#ifdef CONFIG_VIOC_2D_FILTER
static int Check2dFilterPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_2d_filter_rdma_sel[get_vioc_index(component)];
		break;
	case get_vioc_type(VIOC_VIN):
		ret = vioc_config_2d_filter_vin_sel
			[get_vioc_index(component) / 2];
		break;
	case get_vioc_type(VIOC_WDMA):
		ret = vioc_config_2d_filter_wdma_sel[get_vioc_index(component)];
		break;
	case get_vioc_type(VIOC_DISP):
		ret = vioc_config_2d_filter_disp_sel[get_vioc_index(component)];
		break;
	default:
		pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}
#endif //

#ifdef CONFIG_VIOC_CHROMA_INTERPOLATOR
static int CheckChromaInterpolatorPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_chroma_interpolator_rdma_sel[get_vioc_index(
			component)];
		break;
	default:
		pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}
#endif // CONFIG_VIOC_CHROMA_INTERPOLATOR

#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
static int CheckFBCDecPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_FBCDec_rdma_sel[get_vioc_index(component)];
		break;
	default:
		pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}
#endif

#ifdef CONFIG_VIOC_MAP_DECOMP
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
static int CheckMCPathSelection(unsigned int component, unsigned int mc)
{
	int ret = 0;

	if (get_vioc_type(component) != get_vioc_type(VIOC_WMIX))
		ret = -1;

	if (get_vioc_type(mc) == VIOC_MC) {
		if (get_vioc_index(mc)) {
			if (component == VIOC_WMIX0)
				ret = -2;
		} else {
			if (component > VIOC_WMIX0)
				ret = -2;
		}
	}

	if (ret < 0)
		pr_err("[ERR][VIOC_CONFIG] %s, ret:%d wrong path parameter(Path: 0x%08x MC: 0x%08x)\n",
		       __func__, ret, component, mc);

	return ret;
}
#endif
#endif

int VIOC_AUTOPWR_Enalbe(unsigned int component, unsigned int onoff)
{
	int shift_bit = -1;
	unsigned long value = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		shift_bit = PWR_AUTOPD_RDMA_SHIFT;
		break;

#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		shift_bit = PWR_AUTOPD_MC_SHIFT;
		break;
#endif //
	case get_vioc_type(VIOC_WMIX):
		shift_bit = PWR_AUTOPD_MIX_SHIFT;
		break;
	case get_vioc_type(VIOC_WDMA):
		shift_bit = PWR_AUTOPD_WDMA_SHIFT;
		break;

	case get_vioc_type(VIOC_SCALER):
		shift_bit = PWR_AUTOPD_SC_SHIFT;
		break;
	case get_vioc_type(VIOC_VIQE):
		shift_bit = PWR_AUTOPD_VIQE_SHIFT;
		break;
	case get_vioc_type(VIOC_DEINTLS):
		shift_bit = PWR_AUTOPD_DEINTS_SHIFT;
		break;

#ifdef CONFIG_VIOC_SAR
	case get_vioc_type(VIOC_SAR):
		shift_bit = PWR_AUTOPD_SAR_SHIFT;
		break;
#endif //

#ifdef CONFIG_VIOC_2D_FILTER
	case get_vioc_type(VIOC_F2D):
		shift_bit = PWR_AUTOPD_FILT2D_SHIFT;
		break;
#endif //

#ifdef CONFIG_VIOC_PIXEL_MAPPER
	case get_vioc_type(VIOC_PIXELMAP):
		shift_bit = PWR_AUTOPD_PM_SHIFT;
		break;
#endif
	default:
		goto error;
	}

	if (shift_bit >= 0) {
		if (value)
			value =
				(__raw_readl(pIREQ_reg + PWR_AUTOPD_OFFSET)
				 | ((1) << shift_bit));
		else
			value =
				(__raw_readl(pIREQ_reg + PWR_AUTOPD_OFFSET)
				 & ~((1) << shift_bit));

		__raw_writel(value, pIREQ_reg + PWR_AUTOPD_OFFSET);
	}
	return 0;

error:
	pr_err("[ERR][VIOC_CONFIG] %s, in error, Type :0x%x onoff:%d\n",
	       __func__, component, onoff);

	return -1;
}

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
int VIOC_CONFIG_PM_PlugChange(void)
{
	unsigned long value = 0;
	unsigned int loop = 0;
	bool pluged_pm_num = 0;
	volatile void __iomem *pm0_reg, *pm1_reg;
	volatile void __iomem *plug_out_reg = NULL, *plug_in_reg = NULL;
	int path;

	pm0_reg = CalcAddressViocComponent(VIOC_PIXELMAP0);
	pm1_reg = CalcAddressViocComponent(VIOC_PIXELMAP1);
	if (pm0_reg == NULL || pm1_reg == NULL)
		goto error;

	value =
		((__raw_readl(pm0_reg) & CFG_PATH_STS_MASK)
		 >> CFG_PATH_STS_SHIFT);
	if (value == VIOC_PATH_CONNECTED) {
		pluged_pm_num = 0;
		plug_out_reg = pm0_reg;
		plug_in_reg = pm1_reg;
	}
	value =
		((__raw_readl(pm1_reg) & CFG_PATH_STS_MASK)
		 >> CFG_PATH_STS_SHIFT);
	if (value == VIOC_PATH_CONNECTED) {
		pluged_pm_num = 1;
		plug_out_reg = pm1_reg;
		plug_in_reg = pm0_reg;
	}
	if (plug_out_reg == NULL && plug_in_reg == NULL) {
		pr_warn("%s, all pixel_mapper were plugged-out!!\n", __func__);
		return VIOC_PATH_DISCONNECTED;
	}

	// plug-out
	path = (__raw_readl(plug_out_reg) & CFG_PATH_SEL_MASK);
	value = (__raw_readl(plug_out_reg) & ~(CFG_PATH_EN_MASK));
	__raw_writel(value, plug_out_reg);

	// plug-in
	value =
		(__raw_readl(plug_in_reg)
		 & ~(CFG_PATH_SEL_MASK | CFG_PATH_EN_MASK));
	value |= ((path << CFG_PATH_SEL_SHIFT) | (0x1 << CFG_PATH_EN_SHIFT));
	__raw_writel(value, plug_in_reg);
#if 0
	if (__raw_readl(plug_in_reg) & CFG_PATH_ERR_MASK) {
		pr_err("%s, plug-in path configuration error(ERR_MASK). pixel-mapper-%d device is busy. Path:%d\n",
		       __func__, !pluged_pm_num, path);
		value = (__raw_readl(plug_in_reg) & ~(CFG_PATH_EN_MASK));
		__raw_writel(value, plug_in_reg);
	}
#endif
	loop = 50;
	while (1) {
		mdelay(1);
		loop--;
		value =
			((__raw_readl(plug_in_reg) & CFG_PATH_STS_MASK)
			 >> CFG_PATH_STS_SHIFT);
		if (value == VIOC_PATH_CONNECTED)
			break;
		if (loop < 1) {
			pr_err("%s, plug-in path configuration error(TIMEOUT). pixel-mapper-%d device is busy.Path:%d\n",
			       __func__, !pluged_pm_num, path);
			return VIOC_DEVICE_BUSY;
		}
	}
	__raw_writel(0x00000000, plug_out_reg);
	return VIOC_PATH_CONNECTED;

error:
	pr_err("[ERR][VIOC_CONFIG] %s, in error\n", __func__);
	WARN_ON(1);
	return VIOC_DEVICE_INVALID;
}
EXPORT_SYMBOL(VIOC_CONFIG_PM_PlugChange);
#endif

int VIOC_CONFIG_PlugIn(unsigned int component, unsigned int select)
{
	unsigned long value = 0;
	unsigned int loop = 0;
	volatile void __iomem *reg;
	int path;

	reg = CalcAddressViocComponent(component);
	if (reg == NULL)
		goto error;

	/* Check selection has type value. If has, select value is invalid */
	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_SCALER):
		path = CheckScalerPathSelection(select);
		if (path < 0)
			goto error;
#if defined(CONFIG_MC_WORKAROUND)
		if (!system_rev) {
			value =
				(__raw_readl(pIREQ_reg + PWR_AUTOPD_OFFSET)
				 & ~(PWR_AUTOPD_SC_MASK));
			value |= (0x0 << PWR_AUTOPD_SC_SHIFT);
			__raw_writel(value, pIREQ_reg + PWR_AUTOPD_OFFSET);
		}
#endif
		break;
	case get_vioc_type(VIOC_VIQE):
	case get_vioc_type(VIOC_DEINTLS):
		path = CheckViqePathSelection(select);
		if (path < 0)
			goto error;
		break;

#ifdef CONFIG_VIOC_SAR
	case get_vioc_type(VIOC_SAR):
		path = CheckSarPathSelection(select);
		if (path < 0)
			goto error;

		VIOC_AUTOPWR_Enalbe(component, 0);
		break;
#endif //

#ifdef CONFIG_VIOC_PIXEL_MAPPER
	case get_vioc_type(VIOC_PIXELMAP):
		path = CheckPixelMapPathSelection(select);
		if (path < 0)
			goto error;
		VIOC_AUTOPWR_Enalbe(component, 0); // for setting of lut table
		break;
#endif //

#ifdef CONFIG_VIOC_2D_FILTER
	case get_vioc_type(VIOC_F2D):
		path = Check2dFilterPathSelection(select);
		if (path < 0)
			goto error;
		break;
#endif //

#ifdef CONFIG_VIOC_CHROMA_INTERPOLATOR
	case get_vioc_type(VIOC_CINTPL):
		path = CheckChromaInterpolatorPathSelection(select);
		if (path < 0)
			goto error;
		break;
#endif //

	default:
		pr_err("[ERR][VIOC_CONFIG] %s, wrong component type:(0x%08x)\n",
		       __func__, component);
		goto error;
	}

	value = ((__raw_readl(reg) & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT);
	if (value == VIOC_PATH_CONNECTED) {
		value =
			((__raw_readl(reg) & CFG_PATH_SEL_MASK)
			 >> CFG_PATH_SEL_SHIFT);
		if (value != path) {
			pr_warn("[WAN][VIOC_CONFIG] %s, VIOC(T:%d I:%d) is plugged-out by force (from 0x%08lx to %d)!!\n",
				__func__, get_vioc_type(component),
				get_vioc_index(component), value, path);
			VIOC_CONFIG_PlugOut(component);
		}
	}

	value = (__raw_readl(reg) & ~(CFG_PATH_SEL_MASK | CFG_PATH_EN_MASK));
	value |= ((path << CFG_PATH_SEL_SHIFT) | (0x1 << CFG_PATH_EN_SHIFT));
	__raw_writel(value, reg);

	if (__raw_readl(reg) & CFG_PATH_ERR_MASK) {
		pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(ERR_MASK). device is busy. VIOC(T:%d I:%d) Path:%d\n",
		       __func__, get_vioc_type(component),
		       get_vioc_index(component), path);
		value = (__raw_readl(reg) & ~(CFG_PATH_EN_MASK));
		__raw_writel(value, reg);
	}

	loop = 50;
	while (1) {
		mdelay(1);
		loop--;
		value =
			((__raw_readl(reg) & CFG_PATH_STS_MASK)
			 >> CFG_PATH_STS_SHIFT);
		if (value == VIOC_PATH_CONNECTED)
			break;
		if (loop < 1) {
			pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(TIMEOUT). device is busy. VIOC(T:%d I:%d) Path:%d\n",
			       __func__, get_vioc_type(component),
			       get_vioc_index(component), path);
			return VIOC_DEVICE_BUSY;
		}
	}
	return VIOC_PATH_CONNECTED;

error:
	pr_err("[ERR][VIOC_CONFIG] %s, in error, Type :0x%x Select:0x%x cfg_reg:0x%x\n",
	       __func__, component, select, __raw_readl(reg));
	WARN_ON(1);
	return VIOC_DEVICE_INVALID;
}
EXPORT_SYMBOL(VIOC_CONFIG_PlugIn);

#ifdef CONFIG_VIOC_2D_FILTER
int VIOC_CONFIG_FILTER2D_PlugIn(
	unsigned int component, unsigned int select, unsigned int front)
{
	unsigned long value = 0;
	unsigned int loop = 0;
	volatile void __iomem *reg;
	int path;

	reg = CalcAddressViocComponent(component);
	if (reg == NULL)
		goto error;

	/* Check selection has type value. If has, select value is invalid */
	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_F2D):
		path = Check2dFilterPathSelection(select);
		if (path < 0)
			goto error;
		break;

	default:
		pr_err("[ERR][VIOC_CONFIG] %s, wrong component type:(0x%08x)\n",
		       __func__, component);
		goto error;
	}

	if (front == 0)
		path += 1;

	value = ((__raw_readl(reg) & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT);
	if (value == VIOC_PATH_CONNECTED) {
		value =
			((__raw_readl(reg) & CFG_PATH_SEL_MASK)
			 >> CFG_PATH_SEL_SHIFT);
		if (value != path) {
			pr_warn("[WAN][VIOC_CONFIG] %s, VIOC(T:%d I:%d) is plugged-out by force (from 0x%08lx to %d)!!\n",
				__func__, get_vioc_type(component),
				get_vioc_index(component), value, path);
			VIOC_CONFIG_PlugOut(component);
		}
	}

	value = (__raw_readl(reg) & ~(CFG_PATH_SEL_MASK | CFG_PATH_EN_MASK));
	value |= ((path << CFG_PATH_SEL_SHIFT) | (0x1 << CFG_PATH_EN_SHIFT));
	__raw_writel(value, reg);

	if (__raw_readl(reg) & CFG_PATH_ERR_MASK) {
		pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(ERR_MASK). device is busy. VIOC(T:%d I:%d) Path:%d\n",
		       __func__, get_vioc_type(component),
		       get_vioc_index(component), path);
		value = (__raw_readl(reg) & ~(CFG_PATH_EN_MASK));
		__raw_writel(value, reg);
	}

	loop = 50;
	while (1) {
		mdelay(1);
		loop--;
		value =
			((__raw_readl(reg) & CFG_PATH_STS_MASK)
			 >> CFG_PATH_STS_SHIFT);
		if (value == VIOC_PATH_CONNECTED)
			break;
		if (loop < 1) {
			pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(TIMEOUT). device is busy. VIOC(T:%d I:%d) Path:%d\n",
			       __func__, get_vioc_type(component),
			       get_vioc_index(component), path);
			return VIOC_DEVICE_BUSY;
		}
	}
	return VIOC_PATH_CONNECTED;

error:
	pr_err("[ERR][VIOC_CONFIG] %s, in error, Type :0x%x Select:0x%x cfg_reg:0x%x\n",
	       __func__, component, select, __raw_readl(reg));
	WARN_ON(1);

	return VIOC_DEVICE_INVALID;
}
EXPORT_SYMBOL(VIOC_CONFIG_FILTER2D_PlugIn);
#endif //

int VIOC_CONFIG_PlugOut(unsigned int component)
{
	unsigned long value = 0;
	unsigned int loop = 0;
	volatile void __iomem *reg;

	reg = CalcAddressViocComponent(component);
	if (reg == NULL)
		goto error;

	value = ((__raw_readl(reg) & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT);
	if (value == VIOC_PATH_DISCONNECTED) {
		__raw_writel(0x00000000, reg);
		pr_warn("[WAN][VIOC_CONFIG] %s, VIOC(T:%d I:%d) was already plugged-out!!\n",
			__func__, get_vioc_type(component),
			get_vioc_index(component));
		return VIOC_PATH_DISCONNECTED;
	}

	value = (__raw_readl(reg) & ~(CFG_PATH_EN_MASK));
	__raw_writel(value, reg);

	if (__raw_readl(reg) & CFG_PATH_ERR_MASK) {
		pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(ERR_MASK). device is busy. VIOC(T:%d I:%d)\n",
		       __func__, get_vioc_type(component),
		       get_vioc_index(component));
		value = (__raw_readl(reg) & ~(CFG_PATH_EN_MASK));
		__raw_writel(value, reg);
		return VIOC_DEVICE_BUSY;
	}

	loop = 100;
	while (1) {
		mdelay(1);
		loop--;
		value =
			((__raw_readl(reg) & CFG_PATH_STS_MASK)
			 >> CFG_PATH_STS_SHIFT);
		if (value == VIOC_PATH_DISCONNECTED)
			break;
		if (loop < 1) {
			pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(TIMEOUT). device is busy. VIOC(T:%d I:%d)\n",
			       __func__, get_vioc_type(component),
			       get_vioc_index(component));
			return VIOC_DEVICE_BUSY;
		}
	}
	__raw_writel(0x00000000, reg);
	return VIOC_PATH_DISCONNECTED;
error:
	pr_err("[ERR][VIOC_CONFIG] %s, in error, Type :0x%x cfg_reg:0x%x\n",
	       __func__, component, __raw_readl(reg));
	WARN_ON(1);
	return VIOC_DEVICE_INVALID;
}
EXPORT_SYMBOL(VIOC_CONFIG_PlugOut);

/* support bypass mode DMA */
const unsigned int bypassDMA[] = {
#if defined(CONFIG_ARCH_TCC897X)
	/* RDMA */
	VIOC_RDMA00, VIOC_RDMA03, VIOC_RDMA04, VIOC_RDMA07, VIOC_RDMA12,
	VIOC_RDMA14,

	/* VIDEO-IN */
	VIOC_VIN00, VIOC_VIN30,
#else
	/* RDMA */
	VIOC_RDMA00, VIOC_RDMA03, VIOC_RDMA04, VIOC_RDMA07,
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	VIOC_RDMA08, VIOC_RDMA14,
#endif
	VIOC_RDMA11, VIOC_RDMA12,

	/* VIDEO-IN */
	VIOC_VIN00,
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC803X) \
	|| defined(CONFIG_ARCH_TCC805X)
	VIOC_VIN10,
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	VIOC_VIN20, VIOC_VIN30,
#endif
#endif

	0x00 // just for final recognition
};

int VIOC_CONFIG_WMIXPath(unsigned int component_num, unsigned int mode)
{
	/* mode - 0: BY-PSSS PATH, 1: WMIX PATH */
	unsigned long value;
	int i, shift_mix_path, shift_vin_rdma_path, support_bypass = 0;
	volatile void __iomem *config_reg = pIREQ_reg;

	for (i = 0; i < (sizeof(bypassDMA) / sizeof(unsigned int)); i++) {
		if (component_num == bypassDMA[i]) {
			support_bypass = 1;
			break;
		}
	}

	if (support_bypass) {
		shift_mix_path = 0xFF; // ignore value
		shift_vin_rdma_path = 0xFF;

		switch (get_vioc_type(component_num)) {
		case get_vioc_type(VIOC_RDMA):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_RDMA00):
				shift_mix_path = CFG_MISC0_MIX00_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA03):
				shift_mix_path = CFG_MISC0_MIX03_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA04):
				shift_mix_path = CFG_MISC0_MIX10_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA07):
				shift_mix_path = CFG_MISC0_MIX13_SHIFT;
				break;
#if !defined(CONFIG_ARCH_TCC897X)
			case get_vioc_index(VIOC_RDMA08):
				shift_mix_path = CFG_MISC0_MIX20_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA11):
				shift_mix_path = CFG_MISC0_MIX23_SHIFT;
				break;
#endif
			case get_vioc_index(VIOC_RDMA12):
				shift_mix_path = CFG_MISC0_MIX30_SHIFT;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
				shift_vin_rdma_path = CFG_MISC0_RD12_SHIFT;
#endif
				break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC802X) \
	|| defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC805X)
			case get_vioc_index(VIOC_RDMA14):
				shift_mix_path = CFG_MISC0_MIX40_SHIFT;
				shift_vin_rdma_path = CFG_MISC0_RD14_SHIFT;
				break;
#endif
			default:
				break;
			}
			break;
		case get_vioc_type(VIOC_VIN):
			switch (get_vioc_index(component_num) / 2) {
			case get_vioc_index(VIOC_VIN00):
				shift_mix_path = CFG_MISC0_MIX50_SHIFT;
				break;
#if !defined(CONFIG_ARCH_TCC897X)
			case get_vioc_index(VIOC_VIN10):
				shift_mix_path = CFG_MISC0_MIX60_SHIFT;
				break;
			case get_vioc_index(VIOC_VIN20):
				shift_mix_path = CFG_MISC0_MIX30_SHIFT;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
				shift_vin_rdma_path = CFG_MISC0_RD12_SHIFT;
#endif
				break;
#endif
			case get_vioc_index(VIOC_VIN30):
				shift_mix_path = CFG_MISC0_MIX40_SHIFT;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X) \
	|| defined(CONFIG_ARCH_TCC805X)
				shift_vin_rdma_path = CFG_MISC0_RD14_SHIFT;
#endif
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}

		if (shift_mix_path != 0xFF) {
#if defined(CONFIG_ARCH_TCC805X)
			VIOC_CONFIG_WMIXPathReset(component_num, 1);
#endif
			value = __raw_readl(config_reg + CFG_MISC0_OFFSET)
				& ~(1 << shift_mix_path);
			if (mode)
				value |= 1 << shift_mix_path;
			__raw_writel(value, config_reg + CFG_MISC0_OFFSET);
#if defined(CONFIG_ARCH_TCC805X)
			VIOC_CONFIG_WMIXPathReset(component_num, 0);
#endif
		}

		if (shift_vin_rdma_path != 0xFF) {
			value = __raw_readl(config_reg + CFG_MISC0_OFFSET)
				& ~(1 << shift_vin_rdma_path);

			if (get_vioc_type(component_num)
			    == get_vioc_type(VIOC_RDMA))
				value |= 0 << shift_vin_rdma_path;
			else
				value |= 1 << shift_vin_rdma_path;
			__raw_writel(value, config_reg + CFG_MISC0_OFFSET);
		}

		return 0;
	}

	dprintk("%s-%d :: ERROR This component(0x%x) doesn't support mixer bypass(%d) mode, %d/%d!!\n",
		__func__, __LINE__, component_num, support_bypass,
		shift_mix_path, shift_vin_rdma_path);
	return -1;
}

#if defined(CONFIG_ARCH_TCC805X)
void VIOC_CONFIG_WMIXPathReset(unsigned int component_num, unsigned int mode)
{
	/* reset - 0 :Normal, 1:Mixing PATH reset */
	unsigned long value;
	int i, shift_mix_path, shift_vin_rdma_path, support_bypass = 0;
	volatile void __iomem *config_reg = pIREQ_reg;

	for (i = 0; i < (sizeof(bypassDMA) / sizeof(unsigned int)); i++) {
		if (component_num == bypassDMA[i]) {
			support_bypass = 1;
			break;
		}
	}

	if (support_bypass) {
		shift_mix_path = 0xFF; // ignore value
		shift_vin_rdma_path = 0xFF;

		switch (get_vioc_type(component_num)) {
		case get_vioc_type(VIOC_RDMA):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_RDMA00):
				shift_mix_path = WMIX_PATH_SWR_MIX00_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA03):
				shift_mix_path = WMIX_PATH_SWR_MIX03_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA04):
				shift_mix_path = WMIX_PATH_SWR_MIX10_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA07):
				shift_mix_path = WMIX_PATH_SWR_MIX13_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA08):
				shift_mix_path = WMIX_PATH_SWR_MIX20_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA11):
				shift_mix_path = WMIX_PATH_SWR_MIX23_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA12):
				shift_mix_path = WMIX_PATH_SWR_MIX30_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA14):
				shift_mix_path = WMIX_PATH_SWR_MIX40_SHIFT;
				break;
			default:
				break;
			}
			break;
		case get_vioc_type(VIOC_VIN):
			switch (get_vioc_index(component_num) / 2) {
			case get_vioc_index(VIOC_VIN00):
				shift_mix_path = WMIX_PATH_SWR_MIX50_SHIFT;
				break;
			case get_vioc_index(VIOC_VIN10):
				shift_mix_path = WMIX_PATH_SWR_MIX60_SHIFT;
				break;
			case get_vioc_index(VIOC_VIN20):
				shift_mix_path = WMIX_PATH_SWR_MIX30_SHIFT;
				break;
			case get_vioc_index(VIOC_VIN30):
				shift_mix_path = WMIX_PATH_SWR_MIX40_SHIFT;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}

		if (shift_mix_path != 0xFF) {
			value = __raw_readl(
					config_reg + CFG_WMIX_PATH_SWR_OFFSET)
				& ~(1 << shift_mix_path);
			if (mode)
				value |= 1 << shift_mix_path;
			__raw_writel(
				value, config_reg + CFG_WMIX_PATH_SWR_OFFSET);
		}
	}
}
#endif

#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
int VIOC_CONFIG_FBCDECPath(
	unsigned int FBCDecPath, unsigned int rdmaPath, unsigned int on)
{
	unsigned long value = 0;
	unsigned int select = 0;
	volatile void __iomem *reg = (pIREQ_reg + CFG_FBC_DEC_SEL_OFFSET);

	/* Check selection has type value. If has, select value is invalid */
	select = CheckFBCDecPathSelection(rdmaPath);
	if (select < 0)
		goto error;

	/*Check RDMA and AFBC_DEC operations.*/
	if (on)
		value =
			(__raw_readl(reg)
			 & ~((0x1 << select)
			     << CFG_FBC_DEC_SEL_AXSM_SEL_SHIFT));
	else
		value =
			(__raw_readl(reg)
			 | ((0x1 << select)
			    << CFG_FBC_DEC_SEL_AXSM_SEL_SHIFT));

	switch (FBCDecPath) {
	case VIOC_FBCDEC0:
		if (on) {
			value &= ~CFG_FBC_DEC_SEL_AD_PATH_SEL00_MASK;
			value |=
				(select
				 << CFG_FBC_DEC_SEL_AD_PATH_SEL00_SHIFT);
		} else {
			value |= (0x1F << CFG_FBC_DEC_SEL_AD_PATH_SEL00_SHIFT);
		}
		break;
	case VIOC_FBCDEC1:
		if (on) {
			value &= ~CFG_FBC_DEC_SEL_AD_PATH_SEL01_MASK;
			value |=
				(select
				 << CFG_FBC_DEC_SEL_AD_PATH_SEL01_SHIFT);
		} else {
			value |= (0x1F << CFG_FBC_DEC_SEL_AD_PATH_SEL01_SHIFT);
		}
		break;
	default:
		goto error;
	};
	// pr_debug("[DBG][VIOC_CONFIG] %s RDMA(%d) with FBC(%d) :: On(%d)-
	// 0x%08lx \n", __func__, get_vioc_index(rdmaPath),
	// get_vioc_index(FBCDecPath), on, value);
	__raw_writel(value, reg);
	return VIOC_PATH_CONNECTED;

error:
	pr_err("[ERR][VIOC_CONFIG] %s, in error, FBCDecPath(%d) :0x%x rdmaPath:0x%x cfg_reg:0x%x\n",
	       __func__, on, get_vioc_index(FBCDecPath), rdmaPath,
	       __raw_readl(reg));
	WARN_ON(1);
	return VIOC_DEVICE_INVALID;
}
EXPORT_SYMBOL(VIOC_CONFIG_FBCDECPath);
#endif

#ifdef CONFIG_VIOC_MAP_DECOMP
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
/*
 * VIOC_CONFIG_MCPath
 * Connect Mapconverter or RDMA block on component path
 * component : VIOC_RDMA03, VIOC_RDMA07, VIOC_RDMA11, VIOC_RDMA13, VIOC_RDMA15,
 * VIOC_RDMA16, VIOC_RDMA17
 * mc : VIOC_MC0, VIOC_MC1
 */
int VIOC_CONFIG_MCPath(unsigned int component, unsigned int mc)
{
	int ret = 0;
	unsigned long value;
	volatile void __iomem *reg = (pIREQ_reg + CFG_PATH_MC_OFFSET);

	if (CheckMCPathSelection(component, mc) < 0) {
		ret = -1;
		goto error;
	}

	switch (component) {
	case VIOC_WMIX0:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if (__raw_readl(reg) & CFG_PATH_MC_RD03_MASK)
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD03_MASK));
			value |= (0x1 << CFG_PATH_MC_RD03_SHIFT);
		} else {
			if (!(__raw_readl(reg) & CFG_PATH_MC_RD03_MASK))
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD03_MASK));
		}
		break;
	case VIOC_WMIX1:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if (__raw_readl(reg) & CFG_PATH_MC_RD07_MASK)
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD07_MASK));
			value |= (0x1 << CFG_PATH_MC_RD07_SHIFT);
		} else {
			if (!(__raw_readl(reg) & CFG_PATH_MC_RD07_MASK))
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD07_MASK));
		}
		break;
	case VIOC_WMIX2:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if (__raw_readl(reg) & CFG_PATH_MC_RD11_MASK)
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD11_MASK));
			value |=
				((0x1 << CFG_PATH_MC_RD11_SHIFT)
				 | (0x1 << CFG_PATH_MC_MC1_SEL_SHIFT));
		} else {
			if (!(__raw_readl(reg) & CFG_PATH_MC_RD11_MASK))
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD11_MASK));
		}
		break;
	case VIOC_WMIX3:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if (__raw_readl(reg) & CFG_PATH_MC_RD13_MASK)
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD13_MASK));
			value |=
				((0x1 << CFG_PATH_MC_RD13_SHIFT)
				 | (0x2 << CFG_PATH_MC_MC1_SEL_SHIFT));
		} else {
			if (!(__raw_readl(reg) & CFG_PATH_MC_RD13_MASK))
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD13_MASK));
		}
		break;
	case VIOC_WMIX4:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if (__raw_readl(reg) & CFG_PATH_MC_RD15_MASK)
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD15_MASK));
			value |=
				((0x1 << CFG_PATH_MC_RD15_SHIFT)
				 | (0x3 << CFG_PATH_MC_MC1_SEL_SHIFT));
		} else {
			if (!(__raw_readl(reg) & CFG_PATH_MC_RD15_MASK))
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD15_MASK));
		}
		break;
	case VIOC_WMIX5:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if (__raw_readl(reg) & CFG_PATH_MC_RD16_MASK)
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD16_MASK));
			value |=
				((0x1 << CFG_PATH_MC_RD16_SHIFT)
				 | (0x4 << CFG_PATH_MC_MC1_SEL_SHIFT));
		} else {
			if (!(__raw_readl(reg) & CFG_PATH_MC_RD16_MASK))
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD16_MASK));
		}
		break;
	case VIOC_WMIX6:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if (__raw_readl(reg) & CFG_PATH_MC_RD17_MASK)
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD17_MASK));
			value |=
				((0x1 << CFG_PATH_MC_RD17_SHIFT)
				 | (0x5 << CFG_PATH_MC_MC1_SEL_SHIFT));
		} else {
			if (!(__raw_readl(reg) & CFG_PATH_MC_RD17_MASK))
				return ret;

			value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD17_MASK));
		}
		break;
	default:
		goto error;
	}
	__raw_writel(value, reg);

error:
	if (ret < 0) {
		pr_err("[ERR][VIOC_CONFIG] %s, in error, Path: 0x%x MC: %x cfg_reg:0x%x\n",
		       __func__, component, mc, __raw_readl(reg));
	}
	return ret;
}
#endif
#endif

void VIOC_CONFIG_SWReset(unsigned int component, unsigned int mode)
{
#ifdef CONFIG_VIOC_MGR
	if (vioc_mgr_queue_work(VIOC_CMD_RESET, component, mode, 0, 0) < 0)
#endif
	{
		VIOC_CONFIG_SWReset_RAW(component, mode);
	}
}

void VIOC_CONFIG_SWReset_RAW(unsigned int component, unsigned int mode)
{
	unsigned long value;
	volatile void __iomem *reg = pIREQ_reg;

	if (mode != VIOC_CONFIG_RESET && mode != VIOC_CONFIG_CLEAR) {
		pr_err("[ERR][VIOC_CONFIG] %s, in error, invalid mode:%d\n",
		       __func__, mode);
		return;
	}

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_DISP):
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_DEV_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR1_DEV_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;

	case get_vioc_type(VIOC_WMIX):
#if defined(CONFIG_ARCH_TCC805X)
		// read Bypass mode / Mix mode
		value = (__raw_readl(reg + CFG_MISC0_OFFSET) &
			(0x1 << (CFG_MISC0_MIX00_SHIFT + (get_vioc_index(component)*2))));
		if(value){ // Mix Path reset
			value = (__raw_readl(reg + PWR_BLK_SWR1_OFFSET) &
			 ~(PWR_BLK_SWR1_WMIX_MASK));
			value |= (mode << (PWR_BLK_SWR1_WMIX_SHIFT +
				   get_vioc_index(component)));
			__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
			break;
		}else{ // Bypass Path reset
			value = (__raw_readl(reg + CFG_WMIX_PATH_SWR_OFFSET) &
			 ~(0x1 << (get_vioc_index(component)*2)));
			value |= (mode << (get_vioc_index(component)*2));
			__raw_writel(value, (reg + CFG_WMIX_PATH_SWR_OFFSET));
			break;
		}
#else
		value = (__raw_readl(reg + PWR_BLK_SWR1_OFFSET) &
			 ~(PWR_BLK_SWR1_WMIX_MASK));
		value |= (mode << (PWR_BLK_SWR1_WMIX_SHIFT +
				   get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;
#endif
	case get_vioc_type(VIOC_WDMA):
#if !defined(CONFIG_ARCH_TCC803X) && !defined(CONFIG_ARCH_TCC805X)
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_WDMA_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR1_WDMA_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
#else /* CONFIG_ARCH_TCC803X || CONFIG_ARCH_TCC805X */
		value =
			(__raw_readl(
				 reg
				 + (component > VIOC_WDMA08 ?
					    PWR_BLK_SWR4_OFFSET :
					    PWR_BLK_SWR1_OFFSET))
			 & ~((component > VIOC_WDMA08 ?
				      PWR_BLK_SWR4_WD_MASK :
				      PWR_BLK_SWR1_WDMA_MASK)));
		value |=
			(mode
			 << (component > VIOC_WDMA08 ? PWR_BLK_SWR4_WD_SHIFT
					     + (get_vioc_index(
						       component
						       - VIOC_WDMA09)) :
						       PWR_BLK_SWR1_WDMA_SHIFT
					     + get_vioc_index(component)));
		__raw_writel(
			value,
			reg
				+ (component > VIOC_WDMA08 ?
					   PWR_BLK_SWR4_OFFSET :
					   PWR_BLK_SWR1_OFFSET));
#endif
		break;

	case get_vioc_type(VIOC_FIFO):
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_FIFO_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR1_FIFO_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;

	case get_vioc_type(VIOC_RDMA):
		value =
			(__raw_readl(reg + PWR_BLK_SWR0_OFFSET)
			 & ~(PWR_BLK_SWR0_RDMA_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR0_RDMA_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR0_OFFSET));
		break;

	case get_vioc_type(VIOC_SCALER):
#ifndef CONFIG_ARCH_TCC897X
#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X))
		value =
			(__raw_readl(reg + PWR_BLK_SWR2_OFFSET)
			 & ~(PWR_BLK_SWR2_SC_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR2_SC_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR2_OFFSET));
#else /* CONFIG_ARCH_TCC803X || CONFIG_ARCH_TCC805X*/
		value =
			(__raw_readl(reg + PWR_BLK_SWR3_OFFSET)
			 & ~(PWR_BLK_SWR3_SC_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR3_SC_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR3_OFFSET));
#endif
#else
		value =
			(__raw_readl(reg + PWR_BLK_SWR0_OFFSET)
			 & ~(PWR_BLK_SWR0_SC_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR0_SC_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR0_OFFSET));
#endif
		break;

	case get_vioc_type(VIOC_VIQE):
		if (get_vioc_index(component) == 0) {
			value =
				(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
				 & ~(PWR_BLK_SWR1_VIQE0_MASK));
			value |= (mode << PWR_BLK_SWR1_VIQE0_SHIFT);
			__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		}
#ifndef CONFIG_ARCH_TCC897X
		else if (get_vioc_index(component) == 1) {
			value =
				(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
				 & ~(PWR_BLK_SWR1_VIQE1_MASK));
			value |= (mode << PWR_BLK_SWR1_VIQE1_SHIFT);
			__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		}
#endif
		break;

	case get_vioc_type(VIOC_VIN):
#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X))
		value =
			(__raw_readl(reg + PWR_BLK_SWR0_OFFSET)
			 & ~(PWR_BLK_SWR0_VIN_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR0_VIN_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR0_OFFSET));
#else /* CONFIG_ARCH_TCC803X || CONFIG_ARCH_TCC805X*/
		value =
			(__raw_readl(
				 reg
				 + (component > VIOC_VIN30 ?
					    PWR_BLK_SWR4_OFFSET :
					    PWR_BLK_SWR0_OFFSET))
			 & ~(component > VIOC_VIN30 ? PWR_BLK_SWR4_VIN_MASK :
						      PWR_BLK_SWR0_VIN_MASK));
		value |=
			(mode
			 << (component > VIOC_VIN30 ? PWR_BLK_SWR4_VIN_SHIFT
					     + (get_vioc_index(
							component - VIOC_VIN40)
						/ 2) :
						      PWR_BLK_SWR0_VIN_SHIFT
					     + (get_vioc_index(component)
						/ 2)));
		__raw_writel(
			value,
			reg
				+ (component > VIOC_VIN30 ?
					   PWR_BLK_SWR4_OFFSET :
					   PWR_BLK_SWR0_OFFSET));
#endif
		break;

#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_MC_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR1_MC_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;
#endif

	case get_vioc_type(VIOC_DEINTLS):
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_DINTS_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR1_DINTS_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;

#ifdef CONFIG_VIOC_DTRC_DECOMP
	case get_vioc_type(VIOC_DTRC):
		if (get_vioc_index(component) == 0) {
			value =
				(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
				 & ~(PWR_BLK_SWR1_DTRC0_MASK));
			value |= (mode << PWR_BLK_SWR1_DTRC0_SHIFT);
			__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		} else if (get_vioc_index(component) == 1) {
			value =
				(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
				 & ~(PWR_BLK_SWR1_DTRC1_MASK));
			value |= (mode << PWR_BLK_SWR1_DTRC1_SHIFT);
			__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		}
		break;
#endif

#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
	case get_vioc_type(VIOC_FBCDEC):
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		value =
			(__raw_readl(reg + PWR_BLK_SWR4_OFFSET)
			 & ~(PWR_BLK_SWR4_AD_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR4_AD_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR4_OFFSET));
#elif defined(CONFIG_ARCH_TCC803X)
		value =
			(__raw_readl(reg + PWR_BLK_SWR2_OFFSET)
			 & ~(PWR_BLK_SWR2_AD_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR2_AD_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR2_OFFSET));
#else /* CONFIG_ARCH_TCC805X */
		value = (__raw_readl(reg + CFG_WMIX_PATH_SWR_OFFSET) &
			 ~(WMIX_PATH_SWR_PF_MASK));
		value |= (mode << (WMIX_PATH_SWR_PF_SHIFT +
				   get_vioc_index(component)));
		__raw_writel(value, (reg + CFG_WMIX_PATH_SWR_OFFSET));

#endif
		break;
#endif

#ifdef CONFIG_VIOC_PIXEL_MAPPER
	case get_vioc_type(VIOC_PIXELMAP):
		value =
			(__raw_readl(reg + PWR_BLK_SWR2_OFFSET)
			 & ~(PWR_BLK_SWR2_PM_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR2_PM_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, reg + PWR_BLK_SWR2_OFFSET);
		break;
#endif

#ifdef CONFIG_VIOC_SAR
	case get_vioc_type(VIOC_SAR):
		value =
			(__raw_readl(reg + PWR_BLK_SWR2_OFFSET)
			 & ~(PWR_BLK_SWR2_SAR_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR2_SAR_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, reg + PWR_BLK_SWR2_OFFSET);
		break;
#endif

#ifdef CONFIG_TCC_DV_IN
	case get_vioc_type(VIOC_DV_IN):
		value =
			(__raw_readl(reg + PWR_BLK_SWR2_OFFSET)
			 & ~(PWR_BLK_SWR2_DV_IN_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR2_DV_IN_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, reg + PWR_BLK_SWR2_OFFSET);
		break;
#endif

#ifdef CONFIG_VIOC_CHROMA_INTERPOLATOR
	case get_vioc_type(VIOC_CINTPL):
		value =
			(__raw_readl(reg + PWR_BLK_SWR3_OFFSET)
			 & ~(PWR_BLK_SWR3_CIN_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR3_CIN_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, reg + PWR_BLK_SWR2_OFFSET);
		break;
#endif

#ifdef CONFIG_VIOC_2D_FILTER
	case get_vioc_type(VIOC_F2D):
		value =
			(__raw_readl(reg + PWR_BLK_SWR3_OFFSET)
			 & ~(PWR_BLK_SWR3_F2D_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR3_F2D_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, reg + PWR_BLK_SWR3_OFFSET);
		break;
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	case get_vioc_type(VIOC_V_DV):
		value =
			(__raw_readl(reg + PWR_BLK_SWR4_OFFSET)
			 & ~(PWR_BLK_SWR4_V_DV_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR4_V_DV_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, reg + PWR_BLK_SWR4_OFFSET);
		break;

	case get_vioc_type(VIOC_DV_DISP):
		value =
			(__raw_readl(reg + PWR_BLK_SWR4_OFFSET)
			 & ~(PWR_BLK_SWR4_OSD_MASK | PWR_BLK_SWR4_EDR_EL_MASK
			     | PWR_BLK_SWR4_EDR_BL_MASK));
		value |=
			(mode
			 << (PWR_BLK_SWR4_EDR_BL_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, reg + PWR_BLK_SWR4_OFFSET);
		break;
#endif

	default:
		pr_err("[ERR][VIOC_CONFIG] %s, wrong component(0x%08x)\n",
		       __func__, component);
		WARN_ON(1);
	}
}
EXPORT_SYMBOL(VIOC_CONFIG_SWReset);

/*
 * VIOC_CONFIG_Device_PlugState
 * Check PlugInOut status of VIOC SCALER, VIQE, DEINTLS.
 * component : VIOC_SC0, VIOC_SC1, VIOC_SC2, VIOC_VIQE, VIOC_DEINTLS
 * pDstatus : Pointer of status value.
 * return value : Device name of Plug in.
 */
int VIOC_CONFIG_Device_PlugState(
	unsigned int component, VIOC_PlugInOutCheck *VIOC_PlugIn)
{
	//	unsigned long value;
	volatile void __iomem *reg = NULL;

	reg = CalcAddressViocComponent(component);
	if (reg == NULL)
		return VIOC_DEVICE_INVALID;

	VIOC_PlugIn->enable =
		(__raw_readl(reg) & CFG_PATH_EN_MASK) >> CFG_PATH_EN_SHIFT;
	VIOC_PlugIn->connect_device =
		(__raw_readl(reg) & CFG_PATH_SEL_MASK) >> CFG_PATH_SEL_SHIFT;
	VIOC_PlugIn->connect_statue =
		(__raw_readl(reg) & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT;

	return VIOC_DEVICE_CONNECTED;
}

static unsigned int CalcPathSelectionInScaler(unsigned int RdmaNum)
{
	unsigned int ret;

	/* In our register, RDMA16/17 offsets are diffrent. */
	if (RdmaNum == VIOC_RDMA16) {
		ret = VIOC_SC_RDMA_16;
#if !defined(CONFIG_ARCH_TCC897X)
	} else if (RdmaNum == VIOC_RDMA17) {
		ret = VIOC_SC_RDMA_17;
#endif
	} else {
		ret = get_vioc_index(RdmaNum);
	}

	return ret;
}

static unsigned int CalcPathSelectionInViqeDeinter(unsigned int RdmaNum)
{
	unsigned int ret;

	/* In our register, RDMA16/17 offsets are diffrent. */
	if (RdmaNum == VIOC_RDMA16) {
		ret = VIOC_VIQE_RDMA_16;
#if !defined(CONFIG_ARCH_TCC897X)
	} else if (RdmaNum == VIOC_RDMA17) {
		ret = VIOC_VIQE_RDMA_17;
#endif
	} else {
		ret = get_vioc_index(RdmaNum);
	}

	return ret;
}

int VIOC_CONFIG_GetScaler_PluginToRDMA(unsigned int RdmaNum)
{
	int i;
	unsigned int rdma_idx;
	VIOC_PlugInOutCheck VIOC_PlugIn;

	rdma_idx = CalcPathSelectionInScaler(RdmaNum);

	for (i = get_vioc_index(VIOC_SCALER0); i <= VIOC_SCALER_MAX; i++) {
		if (VIOC_CONFIG_Device_PlugState(
			    (VIOC_SCALER0 + i), &VIOC_PlugIn)
		    == VIOC_DEVICE_INVALID)
			continue;

		if (VIOC_PlugIn.enable
		    && VIOC_PlugIn.connect_device == rdma_idx)
			return (VIOC_SCALER0 + i);
	}

	return -1;
}

int VIOC_CONFIG_GetViqeDeintls_PluginToRDMA(unsigned int RdmaNum)
{
	int i;
	unsigned int rdma_idx;
	VIOC_PlugInOutCheck VIOC_PlugIn;

	rdma_idx = CalcPathSelectionInViqeDeinter(RdmaNum);

	for (i = get_vioc_index(VIOC_VIQE0); i <= VIOC_VIQE_MAX; i++) {
		if (VIOC_CONFIG_Device_PlugState((VIOC_VIQE0 + i), &VIOC_PlugIn)
		    == VIOC_DEVICE_INVALID)
			continue;

		if (VIOC_PlugIn.enable
		    && VIOC_PlugIn.connect_device == rdma_idx)
			return (VIOC_VIQE0 + i);
	}

	for (i = get_vioc_index(VIOC_DEINTLS0); i <= VIOC_DEINTLS_MAX; i++) {
		if (VIOC_CONFIG_Device_PlugState(
			    (VIOC_DEINTLS0 + i), &VIOC_PlugIn)
		    == VIOC_DEVICE_INVALID)
			continue;

		if (VIOC_PlugIn.enable
		    && VIOC_PlugIn.connect_device == rdma_idx)
			return (VIOC_DEINTLS0 + i);
	}

	return -1;
}

int VIOC_CONFIG_GetRdma_PluginToComponent(
	unsigned int ComponentNum /*Viqe, Mc, Dtrc*/)
{
	VIOC_PlugInOutCheck VIOC_PlugIn;

	if (VIOC_CONFIG_Device_PlugState(ComponentNum, &VIOC_PlugIn)
	    == VIOC_DEVICE_INVALID)
		return -1;

	if (VIOC_PlugIn.enable)
		return (VIOC_RDMA + VIOC_PlugIn.connect_device);

	return -1;
}

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
int VIOC_CONFIG_DV_SET_EDR_PATH(unsigned int edr_on)
{
	unsigned long value;
	volatile void __iomem *reg = pIREQ_reg;

	dprintk_dv_sequence("### EDR Path %s\n", edr_on ? "On" : "Off");
	value =
		(__raw_readl(reg + CFG_PATH_EDR_OFFSET)
		 & ~(CFG_PATH_EDR_EDR_S_MASK));
	value |= (edr_on << CFG_PATH_EDR_EDR_S_SHIFT);

	__raw_writel(value, reg + CFG_PATH_EDR_OFFSET);

	return 0;
}

unsigned int VIOC_CONFIG_DV_GET_EDR_PATH(void)
{
	unsigned long edr_on = 0;
	volatile void __iomem *reg = pIREQ_reg;

	edr_on = (__raw_readl(reg + CFG_PATH_EDR_OFFSET)
		  & CFG_PATH_EDR_EDR_S_MASK)
		>> CFG_PATH_EDR_EDR_S_SHIFT;

	return edr_on;
}

void VIOC_CONFIG_DV_Metadata_Enable(unsigned int addr, unsigned int endian)
{
	unsigned long value;
	volatile void __iomem *reg = pIREQ_reg;

	if (vioc_v_dv_get_mode() != DV_STD)
		return;

	value = (addr << DV_MD_DMA_ADDR_ADDR_SHIFT);
	__raw_writel(value, (reg + DV_MD_DMA_ADDR_OFFSET));

	value =
		(__raw_readl(reg + DV_MD_DMA_CTRL_OFFSET)
		 & ~(DV_MD_DMA_CTRL_ENDIAN_MASK
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		     | DV_MD_DMA_CTRL_UPD_MASK
#endif
		     | DV_MD_DMA_CTRL_EN_MASK));

	value |= ((endian & 0x1) << DV_MD_DMA_CTRL_ENDIAN_SHIFT);

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	value |= (0x1 << DV_MD_DMA_CTRL_UPD_SHIFT);
#endif

	value |= (0x1 << DV_MD_DMA_CTRL_EN_SHIFT);
	__raw_writel(value, (reg + DV_MD_DMA_CTRL_OFFSET));
}

void VIOC_CONFIG_DV_Metadata_Disable(void)
{
	unsigned long value;
	volatile void __iomem *reg = pIREQ_reg;

	value =
		(__raw_readl(reg + DV_MD_DMA_CTRL_OFFSET)
		 & ~(DV_MD_DMA_CTRL_EN_MASK
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		     | DV_MD_DMA_CTRL_UPD_MASK
#endif
		     ));

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	value |=
		((0x0 << DV_MD_DMA_CTRL_EN_SHIFT)
		 | (0x1 << DV_MD_DMA_CTRL_UPD_SHIFT));
#else
	value |= (0x0 << DV_MD_DMA_CTRL_EN_SHIFT);
#endif

	__raw_writel(value, reg + DV_MD_DMA_CTRL_OFFSET);
}

// To avoid displaying specific color to the screen when stopping/starting
// playback on Dolby-Path.
void VIOC_CONFIG_DV_EX_VIOC_PROC(unsigned int component)
{
	unsigned int rdma_component = 0x0;
	int nPlugged_scaler = 0x0;
	int nPlugged_viqe = 0x0;

	if (!VIOC_CONFIG_DV_GET_EDR_PATH())
		return;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		rdma_component = component;
		break;
#ifdef CONFIG_VIOC_DTRC_DECOMP
	case get_vioc_type(VIOC_DTRC):
		rdma_component =
			VIOC_CONFIG_GetRdma_PluginToComponent(component);
		break;
#endif
#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		rdma_component =
			VIOC_CONFIG_GetRdma_PluginToComponent(component);
		break;
#endif
	default:
		pr_info("[INF][VIOC_CONFIG] %s-%d :: not support this component(0x%x)\n",
			__func__, __LINE__, component);
		return;
	}

	if ((rdma_component == (VIOC_RDMA00 + RDMA_VIDEO))
	    || (rdma_component == (VIOC_RDMA00 + RDMA_VIDEO_SUB))) {
		nPlugged_scaler =
			VIOC_CONFIG_GetScaler_PluginToRDMA(rdma_component);
		nPlugged_viqe =
			VIOC_CONFIG_GetViqe_PluginToRDMA(rdma_component);

		if (nPlugged_scaler >= 0) { // SCALER
			volatile void __iomem *pSC =
				VIOC_SC_GetAddress(nPlugged_scaler);

			if (pSC) {
				VIOC_SC_SetBypass(pSC, 1);
				VIOC_SC_SetOutSize(pSC, 0, 0);
				// pr_info("[INF][VIOC_CONFIG] %s-%d scaler(%d)
				// reinit \n", __func__,
				//	__LINE__,
				// nPlugged_scaler-VIOC_SCALER0);
				VIOC_SC_SetUpdate(pSC);
			} else {
				pr_info("[INF][VIOC_CONFIG] Plugged SC(%d)/VIQE(%d)\n",
					nPlugged_scaler, nPlugged_viqe);
			}
		} else if (nPlugged_viqe >= 0) { // VIQE
			/* :: need to test
			 *volatile void __iomem *pViqe = VIOC_VIQE_GetAddress(
			 *		nPlugged_viqe-VIOC_VIQE0);
			 *if(pViqe){
			 *	VIOC_VIQE_SetDeintlSize(pViqe, 0, 0);
			 *	VIOC_VIQE_SetImageSize(pViqe, 0, 0);
			 *	//pr_info("%s-%d viqe(%d) reinit\n", __func__,
			 *__LINE__, nPlugged_viqe-VIOC_VIQE0);
			 *	VIOC_VIQE_SetControlMode(pViqe, OFF, OFF, OFF,
			 *OFF, OFF);
			 *}
			 */
		}
	}
}
#endif

void VIOC_CONFIG_StopRequest(unsigned int en)
{
	unsigned long value;
	volatile void __iomem *reg = pIREQ_reg;

	value = (__raw_readl(reg + CFG_MISC1_OFFSET) & ~(CFG_MISC1_S_REQ_MASK));

	if (en)
		value |= (0x0 << CFG_MISC1_S_REQ_SHIFT);
	else
		value |= (0x1 << CFG_MISC1_S_REQ_SHIFT);

	__raw_writel(value, reg + CFG_MISC1_OFFSET);
}

int VIOC_CONFIG_DMAPath_Select(unsigned int path)
{
	int i;
	unsigned long value;
	volatile void __iomem *reg;

	reg = (void __iomem *)CalcAddressViocComponent(path);
	if (!reg) {
		dprintk("%s-%d :: INFO :: path(0x%x) is not configurable\n",
			__func__, __LINE__, path);
		return path;
	}

	// check vrdma path
	for (i = 0; i < VIOC_RDMA_MAX; i++) {
		reg = (void __iomem *)CalcAddressViocComponent(VIOC_RDMA00 + i);

		if (!reg)
			continue;

		value = __raw_readl(reg);
		if ((value & CFG_PATH_EN_MASK)
		    && ((value & CFG_PATH_SEL_MASK) == (get_vioc_index(path))))
			return VIOC_RDMA00 + i;
	}

#ifdef CONFIG_VIOC_MAP_DECOMP
	// check map converter
	for (i = 0; i < VIOC_MC_MAX; i++) {
		reg = (void __iomem *)CalcAddressViocComponent(VIOC_MC0 + i);
		if (!reg)
			continue;

		value = __raw_readl(reg);

		if ((value & CFG_PATH_EN_MASK)
		    && ((value & CFG_PATH_SEL_MASK) == (get_vioc_index(path))))
			return VIOC_MC0 + i;
	}
#endif
#ifdef CONFIG_VIOC_DTRC_DECOMP
	// check dtrc converter
	for (i = 0; i < VIOC_DTRC_MAX; i++) {
		reg = (void __iomem *)CalcAddressViocComponent(VIOC_DTRC0 + i);
		if (!reg)
			continue;

		value = __raw_readl(reg);
		if ((value & CFG_PATH_EN_MASK)
		    && ((value & CFG_PATH_SEL_MASK) == (get_vioc_index(path))))
			return VIOC_DTRC0 + i;
	}
#endif

	dprintk("%s-%d :: Info path(0x%x) doesn't have plugged-in component\n",
		__func__, __LINE__, path);
	return -1;
}

int VIOC_CONFIG_DMAPath_Set(unsigned int path, unsigned int dma)
{
	int loop = 0;
	unsigned long value = 0x0;
	unsigned int path_sel = 0;
	unsigned int en, sel, status;
	volatile void __iomem *cfg_path_reg;

	cfg_path_reg = CalcAddressViocComponent(dma);
	if (cfg_path_reg == NULL) {
		dprintk("%s-%d :: ERR :: cfg_path_reg for dma(0x%x) is NULL\n",
			__func__, __LINE__, dma);
		return -1;
	}

	switch (get_vioc_type(dma)) {
	case get_vioc_type(VIOC_RDMA):
		break;
#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC): {
#if 1 // Disalbe Auto power-ctrl
		value =
			(__raw_readl(pIREQ_reg + PWR_AUTOPD_OFFSET)
			 & ~(PWR_AUTOPD_MC_MASK));
		value |= (0x0 << PWR_AUTOPD_MC_SHIFT);
		__raw_writel(value, pIREQ_reg + PWR_AUTOPD_OFFSET);
#endif
	} break;
#endif
#ifdef CONFIG_VIOC_DTRC_DECOMP
	case get_vioc_type(VIOC_DTRC):
		break;
#endif
	default:
		pr_info("[INF][VIOC_CONFIG] %s-%d :: ERROR :: cfg_path_reg for dma(0x%x) doesn't support\n",
			__func__, __LINE__, dma);
		return -1;
	}

	// check path status.
	value = __raw_readl(cfg_path_reg);

	en = (value & CFG_PATH_EN_MASK) ? 1 : 0;
	status = (value & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT;
	sel = (value & CFG_PATH_SEL_MASK) >> CFG_PATH_SEL_SHIFT;

	// path selection.
	if (get_vioc_type(path) == get_vioc_type(VIOC_RDMA)) {
		path_sel = get_vioc_index(path);
	} else {
		pr_info("[INF][VIOC_CONFIG] %s-%d :: ERROR :: path(0x%x) is very wierd.\n",
			__func__, __LINE__, path);
		return -1;
	}

	if (en && (sel != path_sel)) {
		loop = 50;
		__raw_writel(value & (~CFG_PATH_EN_MASK), cfg_path_reg);
		while (loop--) {
			value = __raw_readl(cfg_path_reg);
			status = (value & CFG_PATH_STS_MASK)
				>> CFG_PATH_STS_SHIFT;
			if (status == VIOC_PATH_DISCONNECTED)
				break;
			mdelay(1);
		}

		if (loop <= 0)
			goto error;
	}

	loop = 50;
	dprintk("W:: %s CFG_RDMA:0x%p = 0x%x\n", __func__, cfg_path_reg,
		(CFG_PATH_EN_MASK
		 | ((path_sel << CFG_PATH_SEL_SHIFT) & CFG_PATH_SEL_MASK)));
	__raw_writel(
		CFG_PATH_EN_MASK
			| ((path_sel << CFG_PATH_SEL_SHIFT)
			   & CFG_PATH_SEL_MASK),
		cfg_path_reg);
	while (loop--) {
		value = __raw_readl(cfg_path_reg);
		status = (value & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT;
		if (status != VIOC_PATH_DISCONNECTED)
			break;
		mdelay(1);
	}

	if (loop <= 0)
		goto error;

	dprintk("R:: %s CFG_RDMA:0x%p = 0x%x\n", __func__, cfg_path_reg,
		__raw_readl(cfg_path_reg));

	return 0;

error:
	pr_err("[ERR][VIOC_CONFIG] vioc config plug in error : setting path : 0x%x dma:%x cfg_path_reg(0x%p):0x%x  before registe : 0x%08lx\n",
	       path, dma, cfg_path_reg, readl(cfg_path_reg), value);
	return -1;
}

int VIOC_CONFIG_DMAPath_UnSet(int dma)
{
	unsigned int en = 0;
	int loop = 0;
	unsigned long value = 0;
	volatile void __iomem *cfg_path_reg;

	if (dma < 0)
		return -1;

	// select config register.
	cfg_path_reg = CalcAddressViocComponent(dma);
	if (cfg_path_reg == NULL) {
		pr_info("[INF][VIOC_CONFIG] %s-%d :: ERROR :: cfg_path_reg for dma(0x%x) is NULL\n",
			__func__, __LINE__, dma);
		return -1;
	}

	switch (get_vioc_type(dma)) {
	case get_vioc_type(VIOC_RDMA):
		break;
#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		break;
#endif
#ifdef CONFIG_VIOC_DTRC_DECOMP
	case get_vioc_type(VIOC_DTRC):
		break;
#endif
	default:
		pr_info("[INF][VIOC_CONFIG] %s-%d :: ERROR :: cfg_path_reg for dma(0x%x) doesn't support\n",
			__func__, __LINE__, dma);
		return -1;
	}

	// check path status.
	value = __raw_readl(cfg_path_reg);
	en = (value & CFG_PATH_EN_MASK) ? 1 : 0;

	if (en) {
		// disable dma
		loop = 50;
		__raw_writel(value & (~CFG_PATH_EN_MASK), cfg_path_reg);

		// wait dma disconnected status.
		while (loop--) {
			if (((__raw_readl(cfg_path_reg) & CFG_PATH_STS_MASK)
			     >> CFG_PATH_STS_SHIFT)
			    == VIOC_PATH_DISCONNECTED)
				break;
			mdelay(1);
		}

		if (loop <= 0)
			goto error;
	}

	return 0;

error:
	pr_err("[ERR][VIOC_CONFIG] %s  in error : setting  dma:%x cfg_path_cfg_path_reg(0x%p):0x%x  before cfg_path_registe : 0x%08lx\n",
	       __func__, dma, cfg_path_reg, readl(cfg_path_reg), value);
	return -1;
}

extern unsigned int system_rev;
int VIOC_CONFIG_DMAPath_Support(void)
{
#if defined(CONFIG_ARCH_TCC898X)
	if (system_rev >= 0x0002)
		return 1;
	else
		return 0;
#endif

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	return 1;
#else // TCC897X, TCC802X, TCC570X, TCC803X
	return 0;
#endif
}

void VIOC_CONFIG_DMAPath_Iint(void)
{
	int i;
	volatile void __iomem *cfg_path_reg;

	if (VIOC_CONFIG_DMAPath_Support()) {
		for (i = 0; i < VIOC_RDMA_MAX; i++) {
			cfg_path_reg = (void __iomem *)CalcAddressViocComponent(
				VIOC_RDMA00 + i);

			if (!cfg_path_reg)
				continue;
			VIOC_CONFIG_DMAPath_Set(
				VIOC_RDMA00 + i, VIOC_RDMA00 + i);
		}
	}
}

int VIOC_CONFIG_LCDPath_Select(unsigned int lcdx_sel, unsigned int lcdx_if)
{
	int i, ret = 0, cnt;
	unsigned int lcdx_bak;
	uint32_t val;
#if defined(CONFIG_ARCH_TCC805X)
	unsigned int sel_lcd[4];

	cnt = 3;
#else
	unsigned int sel_lcd[3];

	cnt = 2;
#endif

	if (lcdx_sel > 3 || lcdx_if > 3) {
		ret = -EINVAL;
		return ret;
	}

	val = __raw_readl(pIREQ_reg + CFG_MISC1_OFFSET);
	sel_lcd[0] =
		(val & CFG_MISC1_LCD0_SEL_MASK) >> CFG_MISC1_LCD0_SEL_SHIFT;
	sel_lcd[1] =
		(val & CFG_MISC1_LCD1_SEL_MASK) >> CFG_MISC1_LCD1_SEL_SHIFT;
	sel_lcd[2] =
		(val & CFG_MISC1_LCD2_SEL_MASK) >> CFG_MISC1_LCD2_SEL_SHIFT;
#if defined(CONFIG_ARCH_TCC805X)
	sel_lcd[3] =
		(val & CFG_MISC1_LCD3_SEL_MASK) >> CFG_MISC1_LCD3_SEL_SHIFT;
#endif

	if (sel_lcd[lcdx_if] != lcdx_sel) {
		lcdx_bak = sel_lcd[lcdx_if];
		sel_lcd[lcdx_if] = lcdx_sel;
	} else {
		return ret;
	}

	for (i = 0; i <= cnt; i++) {
		if ((i != lcdx_if) && (sel_lcd[i] == lcdx_sel)) {
			sel_lcd[i] = lcdx_bak;
			break;
		}
	}
#if defined(CONFIG_ARCH_TCC805X)
	val &=
		~(CFG_MISC1_LCD3_SEL_MASK | CFG_MISC1_LCD2_SEL_MASK
		  | CFG_MISC1_LCD1_SEL_MASK | CFG_MISC1_LCD0_SEL_MASK);
	val |= ((sel_lcd[3] << CFG_MISC1_LCD3_SEL_SHIFT)
		| (sel_lcd[2] << CFG_MISC1_LCD2_SEL_SHIFT)
		| (sel_lcd[1] << CFG_MISC1_LCD1_SEL_SHIFT)
		| (sel_lcd[0] << CFG_MISC1_LCD0_SEL_SHIFT));
#else
	val &=
		~(CFG_MISC1_LCD2_SEL_MASK | CFG_MISC1_LCD1_SEL_MASK
		  | CFG_MISC1_LCD0_SEL_MASK);
	val |= ((sel_lcd[2] << CFG_MISC1_LCD2_SEL_SHIFT)
		| (sel_lcd[1] << CFG_MISC1_LCD1_SEL_SHIFT)
		| (sel_lcd[0] << CFG_MISC1_LCD0_SEL_SHIFT));
#endif
	__raw_writel(val, pIREQ_reg + CFG_MISC1_OFFSET);

	dprintk("%s, CFG_MISC1: 0x%08x\n", __func__,
		__raw_readl(pIREQ_reg + CFG_MISC1_OFFSET));

	return ret;
}

volatile void __iomem *VIOC_IREQConfig_GetAddress(void)
{
	if (pIREQ_reg == NULL)
		pr_err("[ERR][VIOC_CONFIG] %s VIOC_IREQConfig:%p\n", __func__,
		       pIREQ_reg);

	return pIREQ_reg;
}

void VIOC_IREQConfig_DUMP(unsigned int offset, unsigned int size)
{
	unsigned int cnt = 0;

	if (pIREQ_reg == NULL) {
		pr_err("[ERR][VIOC_CONFIG] %s, VIOC_IREQConfig\n", __func__);
		return;
	}

	pr_debug(
		"[DBG][VIOC_CONFIG] VIOC_IREQConfig :: 0x%p\n",
		pIREQ_reg + offset);
	while (cnt < size) {
		pr_debug(
			"[DBG][VIOC_CONFIG] 0x%p: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			pIREQ_reg + offset + cnt,
			__raw_readl(pIREQ_reg + offset + cnt),
			__raw_readl(pIREQ_reg + offset + cnt + 0x4),
			__raw_readl(pIREQ_reg + offset + cnt + 0x8),
			__raw_readl(pIREQ_reg + offset + cnt + 0xC));
		cnt += 0x10;
	}
}

static int __init vioc_config_init(void)
{
	struct device_node *ViocConfig_np;

	ViocConfig_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_config");

	if (ViocConfig_np == NULL) {
		pr_info("[INF][VIOC_CONFIG] disabled [this is mandatory for vioc display]\n");
	} else {
		pIREQ_reg = of_iomap(ViocConfig_np, is_VIOC_REMAP ? 1 : 0);

		if (pIREQ_reg) {
			pr_info("[INF][VIOC_CONFIG] 0x%p\n", pIREQ_reg);
			VIOC_CONFIG_DMAPath_Iint();
		}
	}
	return 0;
}
arch_initcall(vioc_config_init);
