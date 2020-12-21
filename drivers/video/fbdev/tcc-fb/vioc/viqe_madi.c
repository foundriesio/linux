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
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <video/tcc/viqe_madi.h>
#include <video/tcc/viqe_madi_regs_array.h>


struct device_node *pMADI_np;
volatile void __iomem *pMADI_reg[VMADI_MAX] = {0};

#define MEM_UV422 0 // internal YUV operation format

// simulation 0, real use case 1 :: both even,odd field in one frame.
#define INTERLEAVE 1

#ifdef USE_REG_EXTRACTOR
#include <linux/mm.h>
#include <linux/slab.h>
#endif

unsigned int Mx_Cap_offset[MADI_CAP_MAX] = {
	M4_CAP_OFFSET, // Src-Y
	M5_CAP_OFFSET, // Src-C
	M6_CAP_OFFSET, // Src-Y
	M7_CAP_OFFSET,
	M8_CAP_OFFSET,
	M9_CAP_OFFSET // Src-C
};

unsigned int Mx_Vdeint_offset[MADI_VDEINT_MAX] = {
	M0_VDEINT_OFFSET,  //
	M11_VDEINT_OFFSET,  //0x280 // DDEI
	M2_VDEINT_OFFSET,   //0x2C0 // Movie det mp
	M4_VDEINT_OFFSET,  //0x400 // Src-Y
	M5_VDEINT_OFFSET,  //0x480 // Src-C
	M6_VDEINT_OFFSET,  //0x500 // Src-Y
	M7_VDEINT_OFFSET,  //0x580 // Target-A
	M78_VDEINT_OFFSET, //0x600 // Target-A
	M9_VDEINT_OFFSET,  //0x680 // Target-Y and C
	MA_VDEINT_OFFSET   //0x740 // Target-A
};

#ifdef USE_REG_EXTRACTOR

struct stMADI_Info *pCap_Info;
struct stMADI_Info *pVDEInt_1_Info;
struct stMADI_Info *pVDEInt_lut_Info;
struct stMADI_Info *pVDEInt_2_Info;

static unsigned int _reg_r_ext(volatile void __iomem *reg)
{
	unsigned int szItem = 0x00;
	unsigned int nReg = 0x00;
	unsigned int i = 0;

	nReg = (unsigned int)reg;

	if (nReg & ((unsigned int)pMADI_reg[VMADI_CAP_IF])) {
		szItem = sizeof(Data_Capture_IF) / sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			if ((nReg & 0x00FFFFFF) == pCap_Info[i].offset)
				return pCap_Info[i].value;
		}
	} else if (nReg & ((unsigned int)pMADI_reg[VMADI_DEINT])) {
		szItem = sizeof(CPU_CLK_VDEINT_1) / sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			if ((nReg & 0x00FFFFFF) == pVDEInt_1_Info[i].offset)
				return pVDEInt_1_Info[i].value;
		}
		szItem = sizeof(CPU_CLK_VDEINT_2) / sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			if ((nReg & 0x00FFFFFF) == pVDEInt_2_Info[i].offset)
				return pVDEInt_2_Info[i].value;
		}
	} else if (nReg & ((unsigned int)pMADI_reg[VMADI_DEINT_LUT])) {
		szItem = sizeof(CPU_CLK_VDEINT_LUT)
			/ sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			if ((nReg & 0x00FFFFFF) == pVDEInt_lut_Info[i].offset)
				return pVDEInt_lut_Info[i].value;
		}
	} else {
	}

	return 0x00;
}

static void _reg_w_ext(unsigned int value, volatile void __iomem *reg)
{
	unsigned int szItem = 0x00;
	unsigned int nReg = 0x00;
	unsigned int i = 0;

	nReg = (unsigned int)reg;

	if (nReg & ((unsigned int)pMADI_reg[VMADI_CAP_IF])) {
		szItem = sizeof(Data_Capture_IF) / sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			if ((nReg & 0x00FFFFFF) == pCap_Info[i].offset) {
				pCap_Info[i].value = value;
				return;
			}
		}
	} else if (nReg & ((unsigned int)pMADI_reg[VMADI_DEINT])) {
		szItem = sizeof(CPU_CLK_VDEINT_1) / sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			if ((nReg & 0x00FFFFFF) == pVDEInt_1_Info[i].offset) {
				pVDEInt_1_Info[i].value = value;

				//if (pVDEInt_1_Info[i].offset < 0x80) {
				//	pr_info("[INF][MADI] pVDEINT[0x%x] :: 0x%8x -> 0x%8x\n",
				//		pVDEInt_Info[i].offset,
				//		pVDEInt_Info[i].value, value);
				//	if (pVDEInt_1_Info[i].offset == 0x00) {
				//		pr_info("[INF][MADI] pVDEINT[0x%x] :: 0x%8x -> 0x%8x\n",
				//		       pVDEInt_1_Info[i].offset,
				//		       pVDEInt_1_Info[i].value,
				//		       value);
				//		WARN_ON(1);
				//	}
				//}

				break; // for CPU_CLK_VDEINT_2
			}
		}

		szItem = sizeof(CPU_CLK_VDEINT_2) / sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			if ((nReg & 0x00FFFFFF) == pVDEInt_2_Info[i].offset) {
				pVDEInt_2_Info[i].value = value;
				return;
			}
		}
	} else if (nReg & ((unsigned int)pMADI_reg[VMADI_DEINT_LUT])) {
		szItem = sizeof(CPU_CLK_VDEINT_LUT)
			/ sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			if ((nReg & 0x00FFFFFF) == pVDEInt_lut_Info[i].offset) {
				pVDEInt_lut_Info[i].value = value;
				return;
			}
		}
	} else {
		pr_err("[ERR][MADI] %s-%d => worng setting 0x%p - 0x%x\n",
			__func__, __LINE__, reg, value);
	}
}

void _reg_print_ext(void)
{
	unsigned int szItem = 0x00;
	unsigned int i = 0;

msleep(100);
	szItem = sizeof(Data_Capture_IF) / sizeof(struct stMADI_Info);
	pr_info("[INF][MADI] V_CAP_IF : %d\n", szItem);
	for (i = 0; i < szItem; i++) {
		pr_info("*(volatile unsigned int *)(V_NR + (0x%04x<<2)) = 0x%08x;\n",
			pCap_Info[i].offset >> 2, pCap_Info[i].value);
	}

msleep(100);
	szItem = sizeof(CPU_CLK_VDEINT_1) / sizeof(struct stMADI_Info);
	pr_info("[INF][MADI] V_DEINT-1 : %d\n", szItem);
	for (i = 0; i < szItem; i++) {
		pr_info("*(volatile unsigned int *)(V_DEINT + (0x%04x<<2)) = 0x%08x;\n",
			pVDEInt_1_Info[i].offset >> 2, pVDEInt_1_Info[i].value);
	}

msleep(100);
	szItem = sizeof(CPU_CLK_VDEINT_LUT) / sizeof(struct stMADI_Info);
	pr_info("[INF][MADI] V_DEINT_LUT : %d\n", szItem);
	for (i = 0; i < szItem; i++) {
		pr_info("*(volatile unsigned int *)(V_DEINT_LUT + (0x%04x<<2)) = 0x%08x;\n",
			pVDEInt_lut_Info[i].offset >> 2,
			pVDEInt_lut_Info[i].value);
	}

msleep(100);
	szItem = sizeof(CPU_CLK_VDEINT_2) / sizeof(struct stMADI_Info);
	pr_info("[INF][MADI] V_DEINT-2 : %d\n", szItem);
	for (i = 0; i < szItem; i++) {
		pr_info("*(volatile unsigned int *)(V_DEINT + (0x%04x<<2)) = 0x%08x;\n",
			pVDEInt_2_Info[i].offset >> 2,
			pVDEInt_2_Info[i].value);
	}
}

#define __madi_reg_r _reg_r_ext
#define __madi_reg_w _reg_w_ext
#else
#define __madi_reg_r __raw_readl
#define __madi_reg_w __raw_writel
#endif

static void
_viqe_madi_set_line_config(volatile void __iomem *reg, unsigned int bcap,
			unsigned int base_offset, unsigned int linebuf_level,
			unsigned int rowbytes, unsigned int linenum)
{
	unsigned int value = 0x00;

	if (bcap) {
		value = (__madi_reg_r(reg + base_offset +
			MADICAP_LINE_BUFFER_LEVEL_OFFSET) &
			~(COMMON_LINE_BUFF_LEVEL_MASK | COMMON_ROWBYTES_MASK));
		value |= (linebuf_level << COMMON_LINE_BUFF_LEVEL_SHIFT);
		value |= (rowbytes << COMMON_ROW_BYTES_SHIFT);
		__madi_reg_w(value, reg + base_offset +
				MADICAP_LINE_BUFFER_LEVEL_OFFSET);

		value = (__madi_reg_r(reg + base_offset +
			MADICAP_LINE_CTRL_OFFSET) & ~(COMMON_LINE_NUM_MASK));
		value |= (linenum << COMMON_LINE_NUM_SHIFT);
		__madi_reg_w(value, reg + base_offset +
			MADICAP_LINE_CTRL_OFFSET);
	} else {
		value = (__madi_reg_r(reg + base_offset +
			MADIVDEINT_LINE_BUFFER_LEVEL_OFFSET) &
			~(COMMON_LINE_BUFF_LEVEL_MASK | COMMON_ROWBYTES_MASK));
		value |= (linebuf_level << COMMON_LINE_BUFF_LEVEL_SHIFT);
		value |= (rowbytes << COMMON_ROW_BYTES_SHIFT);
		__madi_reg_w(value, reg + base_offset +
				MADIVDEINT_LINE_BUFFER_LEVEL_OFFSET);

		value = (__madi_reg_r(reg + base_offset +
				MADIVDEINT_LINE_CTRL_OFFSET) &
			 ~(COMMON_LINE_NUM_MASK));
		value |= (linenum << COMMON_LINE_NUM_SHIFT);
		__madi_reg_w(value,
			reg + base_offset + MADIVDEINT_LINE_CTRL_OFFSET);
	}
}

static void
_viqe_madi_vdeinit_top_bottom_field_offset(volatile void __iomem *reg,
					unsigned int base_offset,
					unsigned int field_offset)
{
	unsigned int value = 0x00;

	value = (__madi_reg_r(reg + base_offset +
		MADIVDEINT_TOP_BOTTOM_OFFSET) & ~(COMMON_LINE_BOTTOM_TOP_MASK));
	value |= ((field_offset << COMMON_LINE_BOTTOM_SHIFT) |
		(0x00 << COMMON_LINE_TOP_SHIFT));
	__madi_reg_w(value, reg + base_offset + MADIVDEINT_TOP_BOTTOM_OFFSET);
}

static void _viqe_madi_set_out_c_line_config(volatile void __iomem *reg,
					unsigned int base_offset,
					unsigned int linebuf_level,
					unsigned int rowbytes,
					unsigned int linenum)
{
	unsigned int value = 0x00;

	value = (__madi_reg_r(reg + base_offset +
		MADIVDEINT_OUT_C0_LINE_BUFFER_LEVEL_OFFSET) &
		~(COMMON_LINE_BUFF_LEVEL_MASK | COMMON_ROWBYTES_MASK));
	value |= (linebuf_level << COMMON_LINE_BUFF_LEVEL_SHIFT);
	value |= (rowbytes << COMMON_ROW_BYTES_SHIFT);
	__madi_reg_w(value, reg + base_offset +
		MADIVDEINT_OUT_C0_LINE_BUFFER_LEVEL_OFFSET);

	value = (__madi_reg_r(reg + base_offset +
		MADIVDEINT_OUT_C0_LINE_CTRL_OFFSET) & ~(COMMON_LINE_NUM_MASK));
	value |= (linenum << COMMON_LINE_NUM_SHIFT);
	__madi_reg_w(value,
		reg + base_offset + MADIVDEINT_OUT_C0_LINE_CTRL_OFFSET);

	value = (__madi_reg_r(reg + base_offset + MADIVDEINT_OUT_C0_BC_OFFSET) &
		~(COMMON_LINE_BUFF_LEVEL_MASK));
	value |= (0x00/*linebuf_level*/ << COMMON_LINE_BOTTOM_SHIFT);
	__madi_reg_w(value, reg + base_offset + MADIVDEINT_OUT_C0_BC_OFFSET);
}

static void _viqe_madi_set_src_crop(volatile void __iomem *reg,
				unsigned int base_offset,
				unsigned int width /*crop_w*/,
				unsigned int height /*crop_h*/)
{
	unsigned int value = 0x00;

	value = (__madi_reg_r(reg + base_offset + MADICAP_DATA_SIZE_OFFSET) &
		~(COMMON_SIZE_MASK));
	value |= ((height << COMMON_SIZE_HEIGHT_SHIFT) |
		(width << COMMON_SIZE_WIDTH_SHIFT));
	__madi_reg_w(value, reg + base_offset + MADICAP_DATA_SIZE_OFFSET);

#ifdef USE_UNDESCRIPTION_REGS
	value = (__madi_reg_r(reg + base_offset +
		MADICAP_80_WIDTH_SIZE_OFFSET) & ~(COMMON_MSB_16_MASK));
	value |= ((width << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + base_offset + MADICAP_80_WIDTH_SIZE_OFFSET);

	value = (__madi_reg_r(reg + base_offset +
		MADICAP_80_HEIGHT_SIZE_OFFSET) & ~(COMMON_MSB_16_MASK));
	value |= ((height << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + base_offset + MADICAP_80_HEIGHT_SIZE_OFFSET);
#endif
}

static void _viqe_madi_set_target_crop(volatile void __iomem *reg,
				       unsigned int base_offset,
				       unsigned int width, unsigned int height)
{
	unsigned int value = 0x00;

	value = (__madi_reg_r(reg + base_offset + MADIVDEINT_DATA_SIZE_OFFSET) &
		~(COMMON_SIZE_MASK));
	value |= ((height << COMMON_SIZE_HEIGHT_SHIFT) |
		(width << COMMON_SIZE_WIDTH_SHIFT));
	__madi_reg_w(value, reg + base_offset + MADIVDEINT_DATA_SIZE_OFFSET);
}

void _viqe_madi_set_pclk_timming(unsigned int out_width /*crop_w*/,
				 unsigned int out_height /*crop_h*/)
{
	unsigned int glb_vde_start_pclk = 0x00;
	unsigned int value_h_w, value_h_h, value_w_w;
	unsigned int value;
	volatile void __iomem *reg = NULL;

	value_h_w = ((out_height << 16) | out_width);
	value_h_h = ((out_height << 16) | out_height);
	value_w_w = ((out_width << 16) | out_width);

	reg = VIQE_MADI_GetAddress(VMADI_CAP_IF);

	// 0x800		- out_width-1 [31:16], out_width[15:0]
	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
		MADICAP_PCLK_00_OFFSET)
		& ~(COMMON_MSB_16_MASK | COMMON_LSB_16_MASK));
	value |= (((out_width - 1) << COMMON_MSB_16_SHIFT) |
		(out_width << COMMON_LSB_16_SHIFT));
	__madi_reg_w(value,
		reg + Mx_Cap_offset[MADI_CAP_8] + MADICAP_PCLK_00_OFFSET);

	// 0x804		- out_width[15:0]
	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
		MADICAP_PCLK_04_OFFSET) & ~(COMMON_LSB_16_MASK));
	value |= ((out_width << COMMON_LSB_16_SHIFT));
	__madi_reg_w(value,
		reg + Mx_Cap_offset[MADI_CAP_8] + MADICAP_PCLK_04_OFFSET);

	// 0x810		- out_height[31:16], out_width[15:0]
	__madi_reg_w(value_h_w,
		reg + Mx_Cap_offset[MADI_CAP_8] + MADICAP_PCLK_10_OFFSET);

	// 0x814		- out_width[31:16], out_width[15:0]
	__madi_reg_w(value_w_w,
		reg + Mx_Cap_offset[MADI_CAP_8] + MADICAP_PCLK_14_OFFSET);

	// 0x818		- out_height[31:16], out_width[15:0]
	__madi_reg_w(value_h_w,
		reg + Mx_Cap_offset[MADI_CAP_8] + MADICAP_PCLK_18_OFFSET);

	// 0x820		- out_height[31:16], out_width[15:0]
	__madi_reg_w(value_h_w,
		reg + Mx_Cap_offset[MADI_CAP_8] + MADICAP_PCLK_20_OFFSET);

	// 0x828/0x82C	- glb_vde_start_pclk[31:16]
	glb_vde_start_pclk = out_height + 0xC;
	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
		MADICAP_PCLK_VDE_OFFSET) & ~(COMMON_MSB_16_MASK));
	value |= ((glb_vde_start_pclk << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value,
		reg + Mx_Cap_offset[MADI_CAP_8] + MADICAP_PCLK_VDE_OFFSET);

	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
		MADICAP_PCLK_82C_OFFSET) & ~(COMMON_MSB_16_MASK));
	value |= ((glb_vde_start_pclk << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value,
		reg + Mx_Cap_offset[MADI_CAP_8] + MADICAP_PCLK_82C_OFFSET);

	// 0x844/0x850	- out_height[31:16], out_height[15:0]
	__madi_reg_w(value_h_h, reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_PCLK_HEIGHT_OFFSET);
	__madi_reg_w(value_h_h, reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_PCLK_LINE_LIMIT_OFFSET);

	// 0x84C	- out_width[31:16], out_width[15:0]
	__madi_reg_w(value_w_w, reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_PCLK_WIDTH_OFFSET);

	// 0x858/868	- out_height[31:16]
	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_PCLK_HEIGHT_OUT_OFFSET) &
		 ~(COMMON_MSB_16_MASK));
	value |= ((out_height << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_PCLK_HEIGHT_OUT_OFFSET);

	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
		MADICAP_PCLK_868_OFFSET) & ~(COMMON_MSB_16_MASK));
	value |= ((out_height << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value,
		reg + Mx_Cap_offset[MADI_CAP_8] + MADICAP_PCLK_868_OFFSET);

	// 0x85C		- out_width[31:16]
	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_PCLK_WIDTH_OUT_OFFSET) &
		 ~(COMMON_MSB_16_MASK));
	value |= ((out_width << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_PCLK_WIDTH_OUT_OFFSET);

#ifdef USE_UNDESCRIPTION_REGS
	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_M8_WIDTH_8C_OFFSET) &
		 ~(COMMON_MSB_16_MASK));
	value |= ((out_width << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_M8_WIDTH_8C_OFFSET);

	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_M8_HEIGHT_90_OFFSET) &
		 ~(COMMON_MSB_16_MASK));
	value |= ((out_height << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_M8_HEIGHT_90_OFFSET);

	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_M8_WIDTH_98_OFFSET) &
		 ~(COMMON_MSB_16_MASK));
	value |= ((out_width << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_M8_WIDTH_98_OFFSET);

	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_M8_HEIGHT_9C_OFFSET) &
		 ~(COMMON_MSB_16_MASK));
	value |= ((out_height << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Cap_offset[MADI_CAP_8] +
			MADICAP_M8_HEIGHT_9C_OFFSET);
#endif
}

void _viqe_madi_set_vde_m0_init(unsigned int out_width /*crop_w*/,
			unsigned int out_height /*crop_h*/)
{
	unsigned int vde_width = 0x00;
	unsigned int value;
	volatile void __iomem *reg = NULL;

	reg = VIQE_MADI_GetAddress(VMADI_DEINT);

	vde_width = out_height + 0x6;
	// 0x38/0x3C	- out_height+ 0x6 [15:0]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_PFU_VDE_START_OFFSET) &
		 ~(COMMON_LSB_16_MASK));
	value |= ((vde_width << COMMON_LSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_PFU_VDE_START_OFFSET);

	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_PSU_VDE_START_OFFSET) &
		 ~(COMMON_LSB_16_MASK));
	value |= ((vde_width << COMMON_LSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_PSU_VDE_START_OFFSET);

	// 0x40			- out_height [31:16]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_VSIZE_OFFSET) &
		 ~(COMMON_MSB_16_MASK));
	value |= ((out_height << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_VSIZE_OFFSET);

	// 0x4C			- out_width [31:16]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_HSIZE_OFFSET) &
		 ~(COMMON_MSB_16_MASK));
	value |= ((out_width << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_HSIZE_OFFSET);

	// 0x70			- out_height [15:0]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_VSIZE_OUT_OFFSET) &
		 ~(COMMON_LSB_16_MASK));
	value |= ((out_height << COMMON_LSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_VSIZE_OUT_OFFSET);

	// 0x74			- out_width [15:0]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_HSIZE_OUT_OFFSET) &
		 ~(COMMON_LSB_16_MASK));
	value |= ((out_width << COMMON_LSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_HSIZE_OUT_OFFSET);

	// 0x284		- out_width [11:0]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_MEAS_H_OFFSET) &
		 ~(MADIVDEINT_MEAS_MASK));
	value |= ((out_width << MADIVDEINT_MEAS_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_MEAS_H_OFFSET);

	// 0x288		- out_height [11:0]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_MEAS_V_OFFSET) &
		 ~(MADIVDEINT_MEAS_MASK));
	value |= ((out_height << MADIVDEINT_MEAS_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_MEAS_V_OFFSET);

}

void _viqe_madi_set_movie_m2_init(unsigned int out_width /*crop_w*/,
				unsigned int out_height /*crop_h*/)
{
	unsigned int value;
	volatile void __iomem *reg = NULL;

	reg = VIQE_MADI_GetAddress(VMADI_DEINT);

	// 0x2EC		- vwidth [25:13] | hwidth [12:0]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_MV_2EC_OFFSET) &
		 ~(MADIVDEINT_MV_H_MASK | MADIVDEINT_MV_W_MASK));
	value |= (((out_width-2) << MADIVDEINT_MV_W_SHIFT)
		| ((out_height-4) << MADIVDEINT_MV_H_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_MV_2EC_OFFSET);

	// 0x2F4		- vwidth [25:13] | hwidth [12:0]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_MV_2F4_OFFSET) &
		 ~(MADIVDEINT_MV_H_MASK | MADIVDEINT_MV_W_MASK));
	value |= (((out_width-2) << MADIVDEINT_MV_W_SHIFT)
		| ((out_height-4) << MADIVDEINT_MV_H_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_MV_2F4_OFFSET);

	// 0x318		- hwidth_win3 [12:0]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_WIN3_H_OFFSET) &
		 ~(MADIVDEINT_WIN3_SIZE_MASK));
	value |= (((out_width-2) << MADIVDEINT_WIN3_SIZE_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_WIN3_V_OFFSET);

	// 0x31C		- vwidth_win3 [12:0]
	value = (__madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_WIN3_V_OFFSET) &
		 ~(MADIVDEINT_WIN3_SIZE_MASK | 0x7000/*reg_x_scale_win3 = 0*/));
	value |= ((((out_height-4)>>1) << MADIVDEINT_WIN3_SIZE_SHIFT));
	__madi_reg_w(value, reg + Mx_Vdeint_offset[MADI_VDEINT_0] +
			MADIVDEINT_WIN3_V_OFFSET);

}

void _viqe_madi_set_yuv_mode(unsigned int src_yuv420) // 1: yuv420, 0: yuv422
{
	unsigned int value;
	volatile void __iomem *reg = NULL;

	reg = VIQE_MADI_GetAddress(VMADI_CAP_IF);

	value = (__madi_reg_r(reg + MADICAP_M0_YUV420_MODE_OFFSET) &
		~(MADICAP_M0_YUV420_MODE_MASK));
	value |= ((src_yuv420 << MADICAP_M0_YUV420_MODE_SHIFT));
	__madi_reg_w(value, reg + MADICAP_M0_YUV420_MODE_OFFSET);

	reg = VIQE_MADI_GetAddress(VMADI_DEINT);

	value = (__madi_reg_r(reg + MADIVDEINT_M10_YUV_MODE_OFFSET) &
		~(MADIVDEINT_M10_YUV_MODE_MASK));
	value |= ((src_yuv420 << MADIVDEINT_M10_YUV_MODE_SHIFT));
	__madi_reg_w(value, reg + MADIVDEINT_M10_YUV_MODE_OFFSET);

	value = (__madi_reg_r(reg + MADIVDEINT_M11_YUV_MODE_OFFSET) &
		~(MADIVDEINT_M11_YUV_MODE_MASK));
	value |= ((src_yuv420 << MADIVDEINT_M11_YUV_MODE_SHIFT));
	__madi_reg_w(value, reg + MADIVDEINT_M11_YUV_MODE_OFFSET);

}

void VIQE_MADI_Ctrl_Enable(enum MadiEn_Mode mode)
{
	unsigned long value;
	volatile void __iomem *reg = VIQE_MADI_GetAddress(VMADI_CTRL_IF);

	value = __madi_reg_r(reg + MADICTRL_OFFSET);

	value |= ((0x1 << MADICTRL_SW_RESET_DEINT_SHIFT) |
		  (0x1 << MADICTRL_SW_RESET_DCAP_SHIFT));

	if (mode == MADI_ON) {
		value |= ((0x1 << MADICTRL_SW_RESET_NODE_19_SHIFT) |
			(0x1F << MADICTRL_THRESHOLD_NODE_19_SHIFT) |
			(0x1 << MADICTRL_SW_RESET_NODE_18_SHIFT) |
			(0x1F << MADICTRL_THRESHOLD_NODE_18_SHIFT) |
			(0x1 << MADICTRL_SW_RESET_NODE_16_SHIFT) |
			(0x1F << MADICTRL_THRESHOLD_NODE_16_SHIFT));
	}

	__madi_reg_w(value, reg + MADICTRL_OFFSET);
}

void VIQE_MADI_Ctrl_Disable(void)
{
	unsigned long value = 0x00;
	volatile void __iomem *reg = VIQE_MADI_GetAddress(VMADI_CTRL_IF);

	__madi_reg_w(value, reg + MADICTRL_OFFSET);
}

void VIQE_MADI_SetBasicConfiguration(unsigned int odd_first)
{
	volatile void __iomem *reg = NULL;
	unsigned int szItem = 0x00;
	unsigned int i = 0;

	reg = VIQE_MADI_GetAddress(VMADI_CAP_IF);
	szItem = sizeof(Data_Capture_IF) / sizeof(struct stMADI_Info);
	for (i = 0; i < szItem; i++) {
		__madi_reg_w(Data_Capture_IF[i].value,
			reg + Data_Capture_IF[i].offset);
	}

	reg = VIQE_MADI_GetAddress(VMADI_DEINT);
	szItem = sizeof(CPU_CLK_VDEINT_1) / sizeof(struct stMADI_Info);
	for (i = 0; i < szItem; i++) {
		__madi_reg_w(CPU_CLK_VDEINT_1[i].value,
			reg + CPU_CLK_VDEINT_1[i].offset);
	}

	reg = VIQE_MADI_GetAddress(VMADI_DEINT_LUT);
	szItem = sizeof(CPU_CLK_VDEINT_LUT) / sizeof(struct stMADI_Info);
	for (i = 0; i < szItem; i++) {
		__madi_reg_w(CPU_CLK_VDEINT_LUT[i].value,
			reg + CPU_CLK_VDEINT_LUT[i].offset);
	}

	reg = VIQE_MADI_GetAddress(VMADI_DEINT);
	szItem = sizeof(CPU_CLK_VDEINT_2) / sizeof(struct stMADI_Info);
	for (i = 0; i < szItem; i++) {
		__madi_reg_w(CPU_CLK_VDEINT_2[i].value,
			reg + CPU_CLK_VDEINT_2[i].offset);
	}

	if (g_height >= 1080) {
		pr_info("\e[33m [INF][MADI] src(%dx%d), RES_1080i \e[0m\n",
			g_width, g_height);
		reg = VIQE_MADI_GetAddress(VMADI_DEINT);
		szItem = sizeof(RES_1080i) / sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			__madi_reg_w(RES_1080i[i].value,
				reg + RES_1080i[i].offset);
		}
	} else if (g_height >= 576) {
		pr_info("\e[33m [INF][MADI] src(%dx%d), RES_576i \e[0m\n",
			g_width, g_height);
		reg = VIQE_MADI_GetAddress(VMADI_DEINT);
		szItem = sizeof(RES_576i) / sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			__madi_reg_w(RES_576i[i].value,
				reg + RES_576i[i].offset);
		}
	} else if (g_height >= 480) {
		pr_info("\e[33m [INF][MADI] src(%dx%d), RES_480i \e[0m\n",
			g_width, g_height);
		reg = VIQE_MADI_GetAddress(VMADI_DEINT);
		szItem = sizeof(RES_480i) / sizeof(struct stMADI_Info);
		for (i = 0; i < szItem; i++) {
			__madi_reg_w(RES_480i[i].value,
				reg + RES_480i[i].offset);
		}
	} else {
		pr_info("\e[33m [INF][MADI] src(%dx%d) \e[0m\n",
			g_width, g_height);
	}

	//if (odd_first)
		VIQE_MADI_Set_odd_field_first(odd_first);

}

void VIQE_MADI_Set_SrcImgBase(enum MadiADDR_Type type, unsigned int YAddr,
			unsigned int CAddr)
{
	volatile void __iomem *reg = NULL;
	unsigned int y_value = 0x00, u_value = 0x00;

	if (type >= MADI_ADDR_MAX) {
		pr_err("[ERR][MADI] %s: wrong-type(%d)\n", __func__, type);
		return;
	}

	//pr_info("SrcImgBase : type[%d] = 0x%x / 0x%x\n", type, YAddr, CAddr);

	y_value = (YAddr >> COMMON_ADDR_SHIFT) & COMMON_ADDR_MASK;
	u_value = (CAddr >> COMMON_ADDR_SHIFT) & COMMON_ADDR_MASK;

	reg = VIQE_MADI_GetAddress(VMADI_CAP_IF);

	// Y Address for Capture Interface
	__madi_reg_w(y_value, reg + Mx_Cap_offset[MADI_CAP_4] +
			MADICAP_LEFT_Y0_ADDR_OFFSET + (type * 0x4));
	__madi_reg_w(y_value, reg + Mx_Cap_offset[MADI_CAP_6] +
			MADICAP_LEFT_Y0_ADDR_OFFSET + (type * 0x4));

	// C Address for Capture Interface
	__madi_reg_w(u_value, reg + Mx_Cap_offset[MADI_CAP_5] +
			MADICAP_LEFT_Y0_ADDR_OFFSET + (type * 0x4));
	__madi_reg_w(u_value, reg + Mx_Cap_offset[MADI_CAP_9] +
			MADICAP_LEFT_Y0_ADDR_OFFSET + (type * 0x4));

	reg = VIQE_MADI_GetAddress(VMADI_DEINT);

	// Y Address for Capture Interface
	__madi_reg_w(y_value, reg + Mx_Vdeint_offset[MADI_VDEINT_4] +
			MADIVDEINT_Y0_ST_ADDR_OFFSET + (type * 0x4));
	__madi_reg_w(y_value, reg + Mx_Vdeint_offset[MADI_VDEINT_6] +
			MADIVDEINT_Y0_ST_ADDR_OFFSET + (type * 0x4));

	// C Address for Capture Interface
	__madi_reg_w(u_value, reg + Mx_Vdeint_offset[MADI_VDEINT_5] +
			MADIVDEINT_Y0_ST_ADDR_OFFSET + (type * 0x4));
}

void VIQE_MADI_Set_TargetImgBase(enum MadiADDR_Type type, unsigned int Alpha,
				unsigned int YAddr, unsigned int CAddr)
{
	volatile void __iomem *reg = NULL;
	unsigned int a_value = 0x00, y_value = 0x00, u_value = 0x00;

	if (type >= MADI_ADDR_MAX) {
		pr_err("%s ::Error wrong-type(%d)\n", __func__, type);
		return;
	}

	a_value = (Alpha >> COMMON_ADDR_SHIFT) & COMMON_ADDR_MASK;
	y_value = (YAddr >> COMMON_ADDR_SHIFT) & COMMON_ADDR_MASK;
	u_value = (CAddr >> COMMON_ADDR_SHIFT) & COMMON_ADDR_MASK;

	reg = VIQE_MADI_GetAddress(VMADI_DEINT);

	// Alpha Address
	__madi_reg_w(a_value, reg + Mx_Vdeint_offset[MADI_VDEINT_7] +
			MADIVDEINT_Y0_ST_ADDR_OFFSET + (type * 0x4));
	__madi_reg_w(a_value, reg + Mx_Vdeint_offset[MADI_VDEINT_78] +
			MADIVDEINT_Y0_ST_ADDR_OFFSET + (type * 0x4));
	__madi_reg_w(a_value, reg + Mx_Vdeint_offset[MADI_VDEINT_A] +
			MADIVDEINT_Y0_ST_ADDR_OFFSET + (type * 0x4));

	// Y Address
	__madi_reg_w(y_value, reg + Mx_Vdeint_offset[MADI_VDEINT_9] +
			MADIVDEINT_Y0_ST_ADDR_OFFSET + (type * 0x4));

	// C Address
	__madi_reg_w(u_value, reg + Mx_Vdeint_offset[MADI_VDEINT_9] +
			MADIVDEINT_OUT_C0_ST_ADDR_OFFSET + (type * 0x4));
}

void VIQE_MADI_Get_TargetImgBase(enum MadiADDR_Type type,
				unsigned int *YAddr, unsigned int *CAddr)
{
	volatile void __iomem *reg = NULL;
	unsigned int y_value = 0x00, u_value = 0x00;

	if (type >= MADI_ADDR_MAX) {
		pr_err("[ERR][MADI] %s: wrong-type(%d)\n", __func__, type);
		return;
	}

	reg = VIQE_MADI_GetAddress(VMADI_DEINT);

	// Y Address
	y_value = __madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_9] +
			MADIVDEINT_Y0_ST_ADDR_OFFSET + (type * 0x4));

	// C Address
	u_value = __madi_reg_r(reg + Mx_Vdeint_offset[MADI_VDEINT_9] +
			MADIVDEINT_OUT_C0_ST_ADDR_OFFSET + (type * 0x4));

	*YAddr = (y_value << COMMON_ADDR_SHIFT) & 0xFFFFFFFF;
	*CAddr = (u_value << COMMON_ADDR_SHIFT) & 0xFFFFFFFF;

}

void VIQE_MADI_Set_SrcImgSize(unsigned int width, unsigned int height,
	unsigned int src_yuv420,
	unsigned int bpp, unsigned int crop_sx,
	unsigned int crop_sy, unsigned int crop_w,
	unsigned int crop_h)
{
	volatile void __iomem *reg = NULL;
	unsigned int hwidth_bit = 0x00, acc_hwidth_bit = 0x00, stride = 0x00,
		offset = 0x00, s_offset, rowbytes = 0x00;
	unsigned int y_linebuf_level = 0x00, y_rowbytes = 0x00,
		y_linenum = 0x00;
	unsigned int c_linebuf_level = 0x00, c_rowbytes = 0x00,
		c_linenum = 0x00;
	unsigned int value = 0x00;

	// external control ?
	unsigned int reg_420 = src_yuv420;

	//alank
	//g_width = width;

	// test
	// crop_sx = crop_sy = 0;
	// crop_w = crop_h = 256;

	if (width == 0 || height == 0) {
		pr_err("[ERR][MADI] %s: wrong-size(%d x %d)\n", __func__, width,
		       height);
		return;
	}

	/*
	 * // Common
	 * //glb_source_img_width/height : buffer width/height
	 * //global_pic_h/vwidth : crop width/height
	 * //pix_depth : 8 or 10
	 *
	 *hwidth_bit = (glb_source_img_width*pix_depth);
	 *acc_hwidth_bit = (global_CROP_Hwidth*pix_depth);
	 *stride = (hwidth_bit >> 7) + (hwidth_bit[6:0]!=0);
	 *offset = INTERLEAVE ? stride : 0;
	 */

	hwidth_bit = width * bpp; // 1920*8 = 15360 = 0x3C00
	acc_hwidth_bit = crop_w * bpp; // 1920*8 = 15360 = 0x3C00
	stride = (hwidth_bit >> 7) +
		 (((hwidth_bit & 0x7F) != 0)
			? 1
			: 0); // (0x3C00 >> 7) + (0x3C00&0x7F) ? 1 : 0 = 0x78
	offset = INTERLEAVE ? stride : 0; // 0x78

	/*
	 * // Y
	 *reg_linebuf_level_y_pre[27:16] =
	 *	(acc_hwidth_bit >> 7) + (acc_hwidth_bit[6:0]!=0);
	 *reg_row_byte_y_pre[15:0] = INTERLEAVE ? (stride << 1) : stride;
	 *reg_line_num_y_pre[15:0] = (global_pic_vwidth>>1);
	 */

	/*0x10*/ y_linebuf_level = (acc_hwidth_bit >> 7) +
			(((acc_hwidth_bit & 0x7F) != 0) ? 1 : 0); //0x78
	rowbytes = INTERLEAVE ? (stride << 1) : stride;
	/*0x78*/ y_rowbytes = rowbytes;
	/*0x80*/ y_linenum = (crop_h >> 1);
	s_offset = y_rowbytes * crop_sy;

	/*
	 * // C
	 *reg_linebuf_level_c_pre[27:16] =
	 *	(acc_hwidth_bit >> 7) + (acc_hwidth_bit[6:0]!=0);
	 *reg_row_byte_c_pre[15:0] =
	 *	(MEM_UV422 & reg_420) ? (row_byte << 1) : row_byte;
	 *reg_line_num_c_pre[15:0] =
	 *	(reg_420 ? (global_pic_vwidth >> 2) : (global_pic_vwidth >> 1));
	 */

	/*0x10*/ c_linebuf_level = (acc_hwidth_bit >> 7) +
			(((acc_hwidth_bit & 0x7F) != 0) ? 1 : 0); // 0x78
	/*0xf0*/ c_rowbytes =
		(MEM_UV422 & reg_420) ? (rowbytes << 1) : rowbytes;
	/*0x40*/ c_linenum = (reg_420 ? (crop_h >> 2) : (crop_h >> 1));

	reg = VIQE_MADI_GetAddress(VMADI_CAP_IF);

// Y Configuration for Capture Interface
	_viqe_madi_set_line_config(reg, 1, Mx_Cap_offset[MADI_CAP_4],
			y_linebuf_level, y_rowbytes, y_linenum);
	_viqe_madi_set_line_config(reg, 1, Mx_Cap_offset[MADI_CAP_6],
			y_linebuf_level, y_rowbytes, y_linenum);
	_viqe_madi_set_src_crop(reg, Mx_Cap_offset[MADI_CAP_4], crop_w, crop_h);
	_viqe_madi_set_src_crop(reg, Mx_Cap_offset[MADI_CAP_6], crop_w, crop_h);

// C Configuration for Capture Interface
	_viqe_madi_set_line_config(reg, 1, Mx_Cap_offset[MADI_CAP_5],
			c_linebuf_level, c_rowbytes, c_linenum);
	_viqe_madi_set_line_config(reg, 1, Mx_Cap_offset[MADI_CAP_9],
			c_linebuf_level, c_rowbytes, c_linenum);
	_viqe_madi_set_src_crop(reg, Mx_Cap_offset[MADI_CAP_5], crop_w, crop_h);
	_viqe_madi_set_src_crop(reg, Mx_Cap_offset[MADI_CAP_9], crop_w, crop_h);

#ifdef USE_UNDESCRIPTION_REGS
	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_7] +
			      MADICAP_M7_WIDTH_0C_OFFSET) &
		 ~(COMMON_MSB_16_MASK));
	value |= ((crop_w << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Cap_offset[MADI_CAP_7] +
			MADICAP_M7_WIDTH_0C_OFFSET);

	value = (__madi_reg_r(reg + Mx_Cap_offset[MADI_CAP_7] +
			MADICAP_M7_HEIGHT_10_OFFSET) &
		 ~(COMMON_MSB_16_MASK));
	value |= ((crop_h << COMMON_MSB_16_SHIFT));
	__madi_reg_w(value, reg + Mx_Cap_offset[MADI_CAP_7] +
			MADICAP_M7_HEIGHT_10_OFFSET);
#endif

	reg = VIQE_MADI_GetAddress(VMADI_DEINT);

	// Y Configuration for Capture Interface
	_viqe_madi_set_line_config(reg, 0, Mx_Vdeint_offset[MADI_VDEINT_4],
			y_linebuf_level, y_rowbytes, y_linenum);
	_viqe_madi_set_line_config(reg, 0, Mx_Vdeint_offset[MADI_VDEINT_6],
			y_linebuf_level, y_rowbytes, y_linenum);

	// C Configuration for Capture Interface
	_viqe_madi_set_line_config(reg, 0, Mx_Vdeint_offset[MADI_VDEINT_5],
				   c_linebuf_level, c_rowbytes, c_linenum);
	_viqe_madi_vdeinit_top_bottom_field_offset(
		reg, Mx_Vdeint_offset[MADI_VDEINT_5], 0);

	_viqe_madi_set_yuv_mode(reg_420);
}

void VIQE_MADI_Set_TargetImgSize(unsigned int src_width,
				unsigned int src_height,
				unsigned int out_width /*crop_w*/,
				unsigned int out_height /*crop_h*/,
				unsigned int bpp)
{
	volatile volatile void __iomem *reg = NULL;
	unsigned int hwidth_bit = 0x00, acc_hwidth_bit = 0x00;
	unsigned int linebuf_level = 0x00, rowbytes = 0x00, linenum = 0x00;

	// external control ?
	unsigned int reg_frm_access_mode = 1;
	unsigned int reg_420 = 0;

	if (out_width == 0 || out_height == 0) {
		pr_err("[ERR][MADI] %s: wrong-size(%d x %d)\n",
			__func__, out_width, out_height);
		return;
	}

	reg = VIQE_MADI_GetAddress(VMADI_DEINT);

	/*
	 * // Prev/PPrev-Alpha
	 * //glb_source_img_width/height : buffer width/height
	 * //global_pic_h/vwidth : crop width/height
	 * //pix_depth : 8 or 10
	 *
	 *hwidth_bit = (global_pic_hwidth*1);
	 *reg_linebuf_level_a_pre[11:0]	=
	 *	(hwidth_bit >> 7) + (hwidth_bit[6:0]!=0);
	 *reg_row_byte_a_pre[15:0] = reg_frame_access_mode
	 *	? reg_linebuf_level_a_pre : (reg_linebuf_level_a_pre << 1);
	 *reg_line_num_a_pre[15:0] = *reg_frame_access_mode
	 *	? global_pic_vwidth : (global_pic_vwidth>>1);
	 */

	reg_frm_access_mode = 0;
	hwidth_bit = out_width * 1; // 1920 = 0x780
	/*0x0002*/ linebuf_level =
		(hwidth_bit >> 7) + (((hwidth_bit & 0x7F) != 0) ? 1 : 0); // 0xF
	/*0x0004*/ rowbytes = reg_frm_access_mode
		? linebuf_level : (linebuf_level << 1); // 0x1E
	/*0x0080*/ linenum =
		reg_frm_access_mode ? out_height : (out_height >> 1); // 0x870

	_viqe_madi_set_line_config(reg, 0, Mx_Vdeint_offset[MADI_VDEINT_7],
				linebuf_level, rowbytes, linenum);
	_viqe_madi_vdeinit_top_bottom_field_offset(
		reg, Mx_Vdeint_offset[MADI_VDEINT_7], rowbytes);

	_viqe_madi_set_line_config(reg, 0, Mx_Vdeint_offset[MADI_VDEINT_78],
				linebuf_level, rowbytes, linenum);
	_viqe_madi_vdeinit_top_bottom_field_offset(
		reg, Mx_Vdeint_offset[MADI_VDEINT_78], rowbytes);

	/*
	 *hwidth_bit = (global_pic_hwidth*1);
	 *reg_linebuf_level_ca[11:0] = (hwidth_bit >> 7) + (hwidth_bit[6:0]!=0);
	 *reg_row_byte_ca[15:0] = {4'h0, reg_linebuf_level_ca[11:0]}
	 *	<< (reg_frame_access_mode ? 0 : 1);
	 *reg_line_num_ca[15:0] =
	 *	global_pic_vwidth >> (reg_frame_access_mode ? 0 : 1);
	 */

	reg_frm_access_mode = 1;
	hwidth_bit = out_width * 1; // 256 = 0x100
	/*0x0002*/ linebuf_level =
		(hwidth_bit >> 7) + (((hwidth_bit & 0x7F) != 0) ? 1 : 0);
	/*0x0002*/ rowbytes = linebuf_level << (reg_frm_access_mode ? 0 : 1);
	/*0x0100*/ linenum = out_height >> (reg_frm_access_mode ? 0 : 1);

	_viqe_madi_set_line_config(reg, 0, Mx_Vdeint_offset[MADI_VDEINT_A],
				   linebuf_level, rowbytes, linenum);
	_viqe_madi_set_target_crop(reg, Mx_Vdeint_offset[MADI_VDEINT_A],
				   out_width, out_height);

	/*
	 * //Cur-Y/C
	 *acc_hwidth_bit = (global_pic_hwidth*pix_depth);
	 *hwidth_bit = (glb_source_img_width*pix_depth);
	 *
	 *reg_linebuf_level_y[11:0] =
	 *	(acc_hwidth_bit >> 7) + (acc_hwidth_bit[6:0]!=0);
	 *reg_row_byte_y[15:0] = {4'd0, reg_linebuf_level_y[11:0]};
	 *reg_line_num_y[15:0] = global_pic_vwidth;
	 *
	 *reg_linebuf_level_c[11:0] =
	 *	(acc_hwidth_bit >> 7) + (acc_hwidth_bit[6:0]!=0);
	 *reg_row_byte_c[15:0] = {4'd0, reg_linebuf_level_c};
	 *reg_line_num_c[15:0] =
	 *	(reg_420 ? (`global_pic_vwidth >> 1) : `global_pic_vwidth);
	 */

	acc_hwidth_bit = out_width * bpp;
	hwidth_bit = src_width * bpp;

	/*0x0010*/ linebuf_level = (acc_hwidth_bit >> 7) +
				(((acc_hwidth_bit & 0x7F) != 0) ? 1 : 0);
	/*0x0010*/ rowbytes = linebuf_level;
	/*0x0100*/ linenum = out_height;

	_viqe_madi_set_line_config(reg, 0, Mx_Vdeint_offset[MADI_VDEINT_9],
				linebuf_level, rowbytes, linenum);
	_viqe_madi_set_target_crop(reg, Mx_Vdeint_offset[MADI_VDEINT_9],
				out_width, out_height);

	/*0x0010*/ linebuf_level = (acc_hwidth_bit >> 7) +
				(((acc_hwidth_bit & 0x7F) != 0) ? 1 : 0);
	/*0x0010*/ rowbytes = linebuf_level;
	/*0x0100*/ linenum = (reg_420 ? (out_height >> 1) : out_height);

	_viqe_madi_set_out_c_line_config(reg, Mx_Vdeint_offset[MADI_VDEINT_9],
					linebuf_level, rowbytes, linenum);
	_viqe_madi_vdeinit_top_bottom_field_offset(
		reg, Mx_Vdeint_offset[MADI_VDEINT_9], 0x00/*linebuf_level*/);

	_viqe_madi_set_vde_m0_init(out_width, out_height);
	_viqe_madi_set_movie_m2_init(out_width, out_height);
	_viqe_madi_set_pclk_timming(out_width, out_height);
}

void VIQE_MADI_Gen_Timming(unsigned int out_width, unsigned int out_height)
{
	unsigned int h_front_porch = 0x00, h_back_porch = 0x00, h_active = 0x00,
		h_sync_width = 0x00;
	unsigned int v_front_porch = 0x00, v_back_porch = 0x00, v_active = 0x00,
		v_sync_width = 0x00;
	unsigned int h_total = 0x00, v_total = 0x00;
	volatile void __iomem *reg = NULL;

	h_active = out_width;
	v_active = out_height;

	h_front_porch = 32, h_back_porch = 84, h_sync_width = 8;
	v_front_porch = 20, v_back_porch = 65, v_sync_width = 5;

	reg = VIQE_MADI_GetAddress(VMADI_TIMMING);

#if defined(EN_MADI_VERIFICATION)
	__madi_reg_w(MADI_INT_ALL,
		reg + MADITIMMING_GEN_CFG_IREQ_EN_OFFSET); // deactive
#else
	__madi_reg_w(MADI_INT_DEACTIVATED,
		reg + MADITIMMING_GEN_CFG_IREQ_EN_OFFSET); // deactive
#endif

	if ((h_active + 64) < 429)
		h_total = 429;
	else
		h_total = (h_front_porch + h_sync_width + h_back_porch +
			h_active);

	v_total = (v_front_porch + v_sync_width + v_back_porch + v_active);

	__madi_reg_w(h_total, reg + MADITIMMING_GEN_CFG_H_TOTAL_OFFSET);
	__madi_reg_w((h_front_porch), reg + MADITIMMING_GEN_CFG_H_START_OFFSET);
	__madi_reg_w((h_front_porch + h_sync_width),
		reg + MADITIMMING_GEN_CFG_H_END_OFFSET);

	__madi_reg_w(v_total, reg + MADITIMMING_GEN_CFG_V_TOTAL_OFFSET);
	__madi_reg_w((v_front_porch), reg + MADITIMMING_GEN_CFG_V_START_OFFSET);
	__madi_reg_w((v_front_porch + v_sync_width),
		reg + MADITIMMING_GEN_CFG_V_END_OFFSET);
}

void VIQE_MADI_Set_Clk_Gating(void)
{
	volatile void __iomem *reg = NULL;
	unsigned int value = 0x00;

	reg = VIQE_MADI_GetAddress(VMADI_TIMMING);
	value = 1;
	__madi_reg_w(value, reg + MADITIMMING_GEN_CFG_CGLP_CTRL_OFFSET);
}

void VIQE_MADI_Go_Request(void)
{
	volatile void __iomem *reg = NULL;
	unsigned int value = 0x00;

	reg = VIQE_MADI_GetAddress(VMADI_TIMMING);
	value = 1;
	__madi_reg_w(value, reg + MADITIMMING_GEN_GO_REQ_OFFSET);
}

void VIQE_MADI_Set_odd_field_first(unsigned int odd_first)
{
	volatile void __iomem *reg = NULL;
	//unsigned int curr_cfg_field = 0x00;
	//unsigned int next_cfg_field = 0x00;
	//unsigned int curr_cfg_code = 0x00;

	reg = VIQE_MADI_GetAddress(VMADI_TIMMING);

	//curr_cfg_code = __madi_reg_r(reg + MADITIMMING_GEN_CFG_CODE_OFFSET);
	//curr_cfg_field = __madi_reg_r(reg + MADITIMMING_GEN_CFG_FIELD_OFFSET);
	//pr_info("curr_cfg_code = %d, curr_cfg_field = %d\n",
	//	curr_cfg_code, curr_cfg_field);

	__madi_reg_w(0, reg + MADITIMMING_GEN_CFG_CODE_OFFSET);

	if (odd_first) {
		//next_cfg_field = (curr_cfg_field + 1) & 0x1;
		//pr_info("first_field = %d\n", next_cfg_field);
		__madi_reg_w(1, reg + MADITIMMING_GEN_CFG_FIELD_OFFSET);
	} else {
		__madi_reg_w(0, reg + MADITIMMING_GEN_CFG_FIELD_OFFSET);
	}
}

void VIQE_MADI_Change_Cfg(void)
{
	volatile void __iomem *reg = NULL;
	unsigned int curr_cfg_code = 0x00, curr_cfg_field = 0x00;
	unsigned int next_cfg_code = 0x00, next_cfg_field = 0x00;

	reg = VIQE_MADI_GetAddress(VMADI_TIMMING);

	curr_cfg_code = __madi_reg_r(reg + MADITIMMING_GEN_CFG_CODE_OFFSET);
	curr_cfg_field = __madi_reg_r(reg + MADITIMMING_GEN_CFG_FIELD_OFFSET);

	next_cfg_code = (curr_cfg_code + 1) & 0x3;
	next_cfg_field = (curr_cfg_field + 1) & 0x1;

	//pr_info("cfg_field = %d -> %d\n", curr_cfg_field, next_cfg_field);

	__madi_reg_w(next_cfg_code, reg + MADITIMMING_GEN_CFG_CODE_OFFSET);
	__madi_reg_w(next_cfg_field, reg + MADITIMMING_GEN_CFG_FIELD_OFFSET);
}

/*
 *============================================================================
 */
#ifdef DDEI_FIELD_INSERTION

typedef u_int8_t	BYTE;
typedef u_int32_t	DWORD;

#define ReadReg(r)		__madi_reg_r(r)
#define WriteReg(r, v)	__madi_reg_w(v, r)

#define reg_1960_0880	(pMADI_reg[VMADI_DEINT] + 0x880)
#define reg_1960_028C	(pMADI_reg[VMADI_DEINT] + 0x28C)
#define reg_1960_0240	(pMADI_reg[VMADI_DEINT] + 0x240)
#define reg_1960_025C	(pMADI_reg[VMADI_DEINT] + 0x25C)

void FieldInsertionCtrl(void)
{
	DWORD Reg19600880 = ReadReg(reg_1960_0880);
	BYTE FilmMode = (Reg19600880 & 0x180000) >> 19;
	BYTE phase22 = (Reg19600880 & 0x40000000) >> 30;
	BYTE phase32 = (Reg19600880 & 0x38000000) >> 27;

	DWORD Reg1960028C = ReadReg(reg_1960_028C) & 0xFFF3FFFF;
	DWORD Reg19600240 = ReadReg(reg_1960_0240) & 0xF9FFFFFF;
	DWORD Reg1960025C = ReadReg(reg_1960_025C) & 0xDFFFFFFF;

	BYTE phase_offset22 = 1;
	BYTE phase_offset32 = 4;

	if (FilmMode == 3) {  // Wrong detection
		WriteReg(reg_1960_028C, (Reg1960028C | (0x0 << 18)));
		WriteReg(reg_1960_0240, (Reg19600240 | (0x2 << 25)));
		WriteReg(reg_1960_025C, (Reg1960025C | (0x0 << 29)));

	} else if (FilmMode == 1) { // Film 22 mode
		phase22 = (phase22 + phase_offset22) % 2;

		switch (phase22) {
		case 0:
			WriteReg(reg_1960_028C, (Reg1960028C | (0x1 << 18)));
			WriteReg(reg_1960_0240, (Reg19600240 | (0x2 << 25)));
			WriteReg(reg_1960_025C, (Reg1960025C | (0x0 << 29)));
			break;
		case 1:
			WriteReg(reg_1960_028C, (Reg1960028C | (0x2 << 18)));
			WriteReg(reg_1960_0240, (Reg19600240 | (0x2 << 25)));
			WriteReg(reg_1960_025C, (Reg1960025C | (0x0 << 29)));
			break;
		}

	} else if (FilmMode == 2) {  // Film 32 mode
		phase32 = (phase32 + phase_offset32) % 5;

		switch (phase32) {
		case 0:
			WriteReg(reg_1960_028C, (Reg1960028C | (0x1 << 18)));
			WriteReg(reg_1960_0240, (Reg19600240 | (0x2 << 25)));
			WriteReg(reg_1960_025C, (Reg1960025C | (0x0 << 29)));
			break;
		case 1:
			WriteReg(reg_1960_028C, (Reg1960028C | (0x2 << 18)));
			WriteReg(reg_1960_0240, (Reg19600240 | (0x2 << 25)));
			WriteReg(reg_1960_025C, (Reg1960025C | (0x0 << 29)));
			break;
		case 2:
			WriteReg(reg_1960_028C, (Reg1960028C | (0x1 << 18)));
			WriteReg(reg_1960_0240, (Reg19600240 | (0x2 << 25)));
			WriteReg(reg_1960_025C, (Reg1960025C | (0x0 << 29)));
			break;
		case 3:
			WriteReg(reg_1960_028C, (Reg1960028C | (0x1 << 18)));
			WriteReg(reg_1960_0240, (Reg19600240 | (0x2 << 25)));
			WriteReg(reg_1960_025C, (Reg1960025C | (0x0 << 29)));
			break;
		case 4:
			WriteReg(reg_1960_028C, (Reg1960028C | (0x2 << 18)));
			WriteReg(reg_1960_0240, (Reg19600240 | (0x2 << 25)));
			WriteReg(reg_1960_025C, (Reg1960025C | (0x0 << 29)));
			break;
		}
	} else { // Normal video, using Spatial and temporal DDEIN
		WriteReg(reg_1960_028C, (Reg1960028C | (0x0 << 18)));
		WriteReg(reg_1960_0240, (Reg19600240 | (0x2 << 25)));
		WriteReg(reg_1960_025C, (Reg1960025C | (0x0 << 29)));
	}

	#if 0
	{
		DWORD Reg19600880 = ReadReg(reg_1960_0880);
		DWORD Reg1960028C = ReadReg(reg_1960_028C);
		DWORD Reg19600888 = ReadReg((pMADI_reg[VMADI_DEINT] + 0x888));

		pr_info("%d%d, %x | %05x %05x | %x %x\n",
			(Reg19600880 & 0x100000) >> 20, // [20] r32 read back
			(Reg19600880 & 0x80000) >> 19,  // [19] r22 read back
			(Reg1960028C & 0xC0000) >> 18,  // [19:18] field number
			(Reg19600880 & 0x3FFFF),          // field motion
			(Reg19600888 & 0x3FFFF),          // frame motion
			(Reg19600880 & 0x40000000) >> 30, // [30]
			(Reg19600880 & 0x38000000) >> 27  // [29:27]
			);
	}
	#endif
}
#endif	//DDEI_FIELD_INSERTION


#ifdef TEST_REG_RW
struct stMADI_Region {
	unsigned int start;
	unsigned int end;
};

struct stMADI_Region rw_region[] = {
//CTRL
	//can't control.
//CAPTURE
	{0x2000, 0x208C},
	{0x2400, 0x296C},
//	{0x2A00, 0x2A3C}, //Read Only
//DEINT
	{0x4000, 0x4088},
	{0x4200, 0x4204},
	{0x4240, 0x43BC},
	{0x4400, 0x47FC}
//	,{0x4800, 0x495C} //some bits of register should be 0 or 1.
//LUT
//	,{0x6000, 0x6080} // In case of reading, all value is 0x400000
//except TIMMING
//	,{0x8000, 0x8090} //some bits of register should be 0 or 1.
};

void VIQE_MADI_Reg_RW_Test(void)
{
	volatile void __iomem *reg = NULL;
	unsigned int i = 0, st_size = 0;
	unsigned int array_size = 0;
	unsigned int value = 0x0;
	unsigned int fail_count_0x00 = 0, fail_count_0xff = 0, total_count = 0;

	array_size = sizeof(rw_region) / sizeof(struct stMADI_Region);

msleep(500);
	VIQE_MADI_Ctrl_Enable(MADI_READY);
	reg = VIQE_MADI_GetAddress(VMADI_CTRL_IF);

	for (st_size = 0; st_size < array_size; st_size++) {
		for (i = rw_region[st_size].start; i <= rw_region[st_size].end;
			i += 0x4) {
			total_count++;

			//write 0x00000000
			__madi_reg_w(0x00000000, reg+i);
			value = __madi_reg_r(reg+i);
			if (value != 0x00000000) {
				pr_debug("[DBG][MADI] %s :: Error-1 R/W 0x00-test failure => 0x%p(base+0x%x) : 0x%x\n",
					__func__, reg+i, i, value);
				fail_count_0x00++;
				msleep(10);
			}

			//write 0xFFFFFFFF
			__madi_reg_w(0xFFFFFFFF, reg+i);
			value = __madi_reg_r(reg+i);
			if (value != 0xFFFFFFFF) {
				pr_debug("[DBG][MADI] %s :: Error-2 R/W 0xff-test failure => 0x%p(base+0x%x) : 0x%x\n",
					__func__, reg+i, i, value);
				fail_count_0xff++;
				msleep(10);
			}
		}
	}
	pr_debug("[DBG][MADI] %s :: Total failure count => %d/%d : %d\n",
		__func__, fail_count_0xff, fail_count_0x00, total_count);
}
#endif

#if 0
//call sequence!!

VIQE_MADI_Ctrl_Enable(MADI_READY);
VIQE_MADI_Gen_Timming();
VIQE_MADI_SetBaseConfiguration()
VIQE_MADI_Set_SrcImgSize();
VIQE_MADI_Set_TargetImgSize();
VIQE_MADI_Set_SrcImgBase();
VIQE_MADI_Set_TargetImgBase();
VIQE_MADI_Ctrl_Enable(MADI_ON);

// operation~~~

VIQE_MADI_Change_Cfg();
#endif

volatile void __iomem *VIQE_MADI_GetAddress(enum VMADI_TYPE type)
{
	if (type >= VMADI_MAX)
		goto err;

	if (pMADI_reg[type] == NULL)
		pr_err("[ERR][MADI] %s\n", __func__);

	return pMADI_reg[type];

err:
	pr_err("[ERR][MADI] %s type:%d Max madi num:%d\n",
		__func__, type, VMADI_MAX);
	return NULL;
}

void VIQE_MADI_DUMP(enum VMADI_TYPE type, unsigned int size)
{
	volatile void __iomem *pReg;
	unsigned int cnt = 0;

	if (pMADI_np == NULL)
		return;

	pReg = VIQE_MADI_GetAddress(type);

	pr_debug("[DBG][MADI] 0%p\n", pReg);
	while (cnt < size) {
		pr_debug("%p: 0x%08x 0x%08x 0x%08x 0x%08x\n", pReg + cnt,
		       __madi_reg_r(pReg + cnt), __madi_reg_r(pReg + cnt + 0x4),
		       __madi_reg_r(pReg + cnt + 0x8),
		       __madi_reg_r(pReg + cnt + 0xC));
		cnt += 0x10;
	}
}

static int __init viqe_madi_init(void)
{
#ifdef USE_REG_EXTRACTOR
	pCap_Info = kzalloc(PAGE_ALIGN(sizeof(Data_Capture_IF)), GFP_KERNEL);
	if (!pCap_Info)
		pr_err("[ERR][MADI] can not alloc pCap_Info buffer\n");
	else
		memcpy(pCap_Info, Data_Capture_IF, sizeof(Data_Capture_IF));

	pVDEInt_1_Info = kzalloc(PAGE_ALIGN(sizeof(CPU_CLK_VDEINT_1)),
		GFP_KERNEL);
	if (!pVDEInt_1_Info)
		pr_err("[ERR][MADI] can not alloc pVDEInt_1_Info buffer\n");
	else
		memcpy(pVDEInt_1_Info, CPU_CLK_VDEINT_1,
		       sizeof(CPU_CLK_VDEINT_1));

	pVDEInt_lut_Info = kzalloc(PAGE_ALIGN(sizeof(CPU_CLK_VDEINT_LUT)),
		GFP_KERNEL);
	if (!pVDEInt_lut_Info)
		pr_err("[ERR][MADI] can not alloc pVDEInt_lut_Info buffer\n");
	else
		memcpy(pVDEInt_lut_Info, CPU_CLK_VDEINT_LUT,
		       sizeof(CPU_CLK_VDEINT_LUT));

	pVDEInt_2_Info = kzalloc(PAGE_ALIGN(sizeof(CPU_CLK_VDEINT_2)),
		GFP_KERNEL);
	if (!pVDEInt_2_Info) {
		pr_err("[ERR][MADI] can not alloc pVDEInt_2_Info buffer\n");
	} else {
		memcpy(pVDEInt_2_Info, CPU_CLK_VDEINT_2,
		       sizeof(CPU_CLK_VDEINT_2));
	}

	pMADI_reg[VMADI_CTRL_IF] = (void __iomem *)0x01000000;   // 0x12600000
	pMADI_reg[VMADI_CAP_IF] = (void __iomem *)0x02000000;    // 0x12602000
	pMADI_reg[VMADI_DEINT] = (void __iomem *)0x04000000;     // 0x12604000
	pMADI_reg[VMADI_DEINT_LUT] = (void __iomem *)0x08000000; // 0x12606000
	pMADI_reg[VMADI_TIMMING] = (void __iomem *)0x10000000;   // 0x12608000
#else
	pMADI_np = of_find_compatible_node(NULL, NULL, "telechips,viqe_madi");
	if (pMADI_np == NULL) {
		pr_info("[INF][MADI] vioc-madi: disabled\n");
		return 0;
	}

	pMADI_reg[VMADI_CTRL_IF] = of_iomap(pMADI_np, VMADI_CTRL_IF);
	if (!pMADI_reg[VMADI_CTRL_IF]) {
		struct resource res;
		int rc;

		rc = of_address_to_resource(pMADI_np, VMADI_CTRL_IF, &res);
		pr_info("[INF][MADI] %s MADI[%d]: 0x%08lx ~ 0x%08lx\n",
			__func__, VMADI_CTRL_IF,
			(unsigned long)res.start, (unsigned long)res.end);
	} else {
		pMADI_reg[VMADI_CAP_IF] = pMADI_reg[VMADI_CTRL_IF] + 0x2000;
		pMADI_reg[VMADI_DEINT] = pMADI_reg[VMADI_CAP_IF] + 0x2000;
		pMADI_reg[VMADI_DEINT_LUT] = pMADI_reg[VMADI_DEINT] + 0x2000;
		pMADI_reg[VMADI_TIMMING] = pMADI_reg[VMADI_DEINT_LUT] + 0x2000;

		pr_info("[INF][MADI] %s: 0x%p / 0x%p / 0x%p / 0x%p / 0x%p\n",
			__func__,
			pMADI_reg[VMADI_CTRL_IF],
			pMADI_reg[VMADI_CAP_IF],
			pMADI_reg[VMADI_DEINT],
			pMADI_reg[VMADI_DEINT_LUT],
			pMADI_reg[VMADI_TIMMING]);
	}
#endif

	return 0;
}
arch_initcall(viqe_madi_init);
