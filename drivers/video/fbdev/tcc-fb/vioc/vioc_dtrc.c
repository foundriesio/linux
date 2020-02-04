/*
 * vioc_dtrc.c
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
#include <asm/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_dtrc.h>

static volatile void __iomem *pDTRC_reg[VIOC_DTRC_MAX] = {0};
static struct device_node *ViocDTRC_np;

void VIOC_DTRC_RDMA_Setuvintpl(volatile void __iomem *reg, unsigned int uvintpl)
{
	unsigned long val;
	val = (__raw_readl(reg + DTRC_CTRL) & ~(DTRC_CTRL_UVI_MASK));
	val |= ((uvintpl & 0x1) << DTRC_CTRL_UVI_SHIFT);
	__raw_writel(val, reg + DTRC_CTRL);
}

void VIOC_DTRC_RDMA_SetY2R(volatile void __iomem *reg, unsigned int y2r_on,
			   unsigned int y2r_mode)
{
	unsigned long val;
	val = (__raw_readl(reg + DTRC_CTRL) &
	       ~(DTRC_CTRL_Y2RMD_MASK | DTRC_CTRL_Y2R_MASK));
	val |= (((y2r_on & 0x1) << DTRC_CTRL_Y2R_SHIFT) |
		((y2r_mode & 0x3) << DTRC_CTRL_Y2RMD_SHIFT));
	__raw_writel(val, reg + DTRC_CTRL);
}

void VIOC_DTRC_RDMA_SetImageConfig(volatile void __iomem *reg)
{
	__raw_writel(0x00000000, reg + DTRC_CTRL);
}

unsigned int VIOC_DTRC_RDMA_GetIEN(volatile void __iomem *reg)
{
	return (__raw_readl(reg + DTRC_CTRL) & DTRC_CTRL_IEN_MASK) ? 1 : 0;
}

void VIOC_DTRC_RDMA_SetImageUpdate(volatile void __iomem *reg,
				   unsigned int nEnable)
{
	unsigned long val;
	if (__raw_readl(reg + DTRC_CTRL) & DTRC_CTRL_UPD_MASK)
		return;

	val = (__raw_readl(reg + DTRC_CTRL) &
	       ~(DTRC_CTRL_UPD_MASK | DTRC_CTRL_IEN_MASK));
	val |= (((nEnable & 0x1) << DTRC_CTRL_IEN_SHIFT) |
		(0x1 << DTRC_CTRL_UPD_SHIFT));
	__raw_writel(val, reg + DTRC_CTRL);
}

unsigned int VIOC_DTRC_RDMA_GetImageUpdate(volatile void __iomem *reg)
{
	return (__raw_readl(reg + DTRC_CTRL) & DTRC_CTRL_UPD_MASK) ? 1 : 0;
}

void VIOC_DTRC_RDMA_SetImageFormat(volatile void __iomem *reg,
				   unsigned int nFormat)
{
	unsigned long val;
	val = (__raw_readl(reg + DTRC_CTRL) & ~(DTRC_CTRL_FMT_MASK));
	val |= ((nFormat & 0x1) << DTRC_CTRL_FMT_SHIFT);
	__raw_writel(val, reg + DTRC_CTRL);
}

void VIOC_DTRC_RDMA_SetImageAlpha(volatile void __iomem *reg,
				  unsigned int nAlpha0, unsigned int nAlpha1)
{
	unsigned long val;
	val = (__raw_readl(reg + DTRC_ALPHA) &
	       ~(DTRC_ALPHA_A13_MASK | DTRC_ALPHA_A02_MASK));
	val |= (((nAlpha1) << DTRC_ALPHA_A13_SHIFT) |
		((nAlpha0) << DTRC_ALPHA_A02_SHIFT));
	__raw_writel(val, reg + DTRC_ALPHA);
}

void VIOC_DTRC_RDMA_SetImageSize(volatile void __iomem *reg, unsigned int sw,
				 unsigned int sh)
{
	unsigned long val;
	val = (((sh & 0x3FFF) << DTRC_SIZE_HEIGHT_SHIFT) |
	       ((sw & 0x3FFF) << DTRC_SIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + DTRC_SIZE);
}

void VIOC_DTRC_RDMA_GetImageSize(volatile void __iomem *reg, unsigned int *sw,
				 unsigned int *sh)
{
	*sh = ((__raw_readl(reg + DTRC_SIZE) & DTRC_SIZE_HEIGHT_MASK) >>
	       DTRC_SIZE_HEIGHT_SHIFT);
	*sw = ((__raw_readl(reg + DTRC_SIZE) & DTRC_SIZE_WIDTH_MASK) >>
	       DTRC_SIZE_WIDTH_SHIFT);
}

void VIOC_DTRC_RDMA_SetImageBase(volatile void __iomem *reg,
				 unsigned int nBase0,
				 unsigned int nBase1 /*unsigned int nBase2*/)
{
	unsigned long val;
	val = (nBase0 << DTRC_BASE0_BASE0_SHIFT);
	__raw_writel(val, reg + DTRC_BASE0);

	val = (nBase1 << DTRC_BASE1_BASE1_SHIFT);
	__raw_writel(val, reg + DTRC_BASE1);
}

void VIOC_DTRC_RDMA_SetImageOffset(volatile void __iomem *reg,
				   unsigned int nOffset0, unsigned int nOffset1)
{
	unsigned long val;
	val = (((nOffset1 & 0xFFFF) << DTRC_OFS_OFS1_SHIFT) |
	       ((nOffset0 & 0xFFFF) << DTRC_OFS_OFS0_SHIFT));
	__raw_writel(val, reg + DTRC_OFS);
}

void VIOC_DTRC_RDMA_SetIDSEL(volatile void __iomem *reg, uint nIDSEL,
			     uint nFRUS, uint nMISC)
{
	unsigned long val;
	val = (__raw_readl(reg + DTRC_MISC) & ~(DTRC_MISC_IDSEL_MASK));
	val |= (((nIDSEL & 0x3FF) << DTRC_MISC_IDSEL_SHIFT) |
		((nFRUS & 0x3) << DTRC_MISC_F1EN_SHIFT) |
		((nMISC & 0x3) << DTRC_MISC_MISC_SHIFT));
	__raw_writel(val, reg + DTRC_MISC);
}

void VIOC_DTRC_RDMA_F0_ADDR(volatile void __iomem *reg,
			    unsigned int frm0_y_data_addr,
			    unsigned int frm0_y_table_addr,
			    unsigned int frm0_uv_data_addr,
			    unsigned int frm0_uv_table_addr)
{
	__raw_writel((frm0_y_data_addr << FRM_DATA_Y_SADDR_BASE_SHIFT),
		     reg + FRM0_DATA_Y_SADDR);
	__raw_writel((frm0_y_table_addr << FRM_TABLE_Y_SADDR_BASE_SHIFT),
		     reg + FRM0_TABLE_Y_SADDR);
	__raw_writel((frm0_uv_data_addr << FRM_DATA_C_SADDR_BASE_SHIFT),
		     reg + FRM0_DATA_C_SADDR);
	__raw_writel((frm0_uv_table_addr << FRM_TABLE_C_SADDR_BASE_SHIFT),
		     reg + FRM0_TABLE_C_SADDR);
}

void VIOC_DTRC_RDMA_F0_SIZE(volatile void __iomem *reg, unsigned int frm0_width,
			    unsigned int frm0_height)
{
	unsigned long val;
	val = (((frm0_width & 0xFFFF) << FRM_SIZE_WIDTH_SHIFT) |
	       ((frm0_height & 0xFFFFF) << FRM_SIZE_HEIGHT_SHIFT));
	__raw_writel(val, reg + FRM0_SIZE);
}

void VIOC_DTRC_RDMA_F0_BYP_DETILE_ADDR(volatile void __iomem *reg,
				       unsigned int frm0_y_byp_detile_addr_s,
				       unsigned int frm0_y_byp_detile_addr_e,
				       unsigned int frm0_uv_byp_detile_addr_s,
				       unsigned int frm0_uv_byp_detile_addr_e)
{
	__raw_writel(
		(frm0_y_byp_detile_addr_s << FRM_BPS_TILE_YADR_S_BASE_SHIFT),
		reg + FRM0_BPS_TILE_YADR_S);
	__raw_writel(
		(frm0_y_byp_detile_addr_e << FRM_BPS_TILE_YADR_E_BASE_SHIFT),
		reg + FRM0_BPS_TILE_YADR_E);
	__raw_writel(
		(frm0_uv_byp_detile_addr_s << FRM_BPS_TILE_CADR_S_BASE_SHIFT),
		reg + FRM0_BPS_TILE_CADR_S);
	__raw_writel(
		(frm0_uv_byp_detile_addr_e << FRM_BPS_TILE_CADR_E_BASE_SHIFT),
		reg + FRM0_BPS_TILE_CADR_E);
}

void VIOC_DTRC_RDMA_DCTL(volatile void __iomem *reg, unsigned int frm0_data_rdy,
			 unsigned int frm0_main8, unsigned int frm0_vinv,
			 unsigned int frm0_byp, unsigned int frm1_data_rdy,
			 unsigned int frm1_main8, unsigned int frm1_vinv,
			 unsigned int frm1_byp)
{
	unsigned long val;
	val = (__raw_readl(reg + FRM_CTRL) &
	       (FRM_CTRL_F0_CROP_MASK | FRM_CTRL_F1_CROP_MASK));
	val |= (((frm0_data_rdy & 0x1) << FRM_CTRL_DR0_SHIFT) |
		((frm0_main8 & 0x1) << FRM_CTRL_M80_SHIFT) |
		((frm0_vinv & 0x1) << FRM_CTRL_VI0_SHIFT) |
		((frm0_byp & 0x1) << FRM_CTRL_BYP0_SHIFT) |
		((frm1_data_rdy & 0x1) << FRM_CTRL_DR1_SHIFT) |
		((frm1_main8 & 0x1) << FRM_CTRL_M81_SHIFT) |
		((frm1_vinv & 0x1) << FRM_CTRL_VI1_SHIFT) |
		((frm1_byp & 0x1) << FRM_CTRL_BYP1_SHIFT));
	__raw_writel(val, reg + FRM_CTRL);
}

void VIOC_DTRC_RDMA_F1_ADDR(volatile void __iomem *reg,
			    unsigned int frm1_y_data_addr,
			    unsigned int frm1_y_table_addr,
			    unsigned int frm1_uv_data_addr,
			    unsigned int frm1_uv_table_addr)
{
	__raw_writel((frm1_y_data_addr << FRM_DATA_Y_SADDR_BASE_SHIFT),
		     reg + FRM1_DATA_Y_SADDR);
	__raw_writel((frm1_y_table_addr << FRM_TABLE_Y_SADDR_BASE_SHIFT),
		     reg + FRM1_TABLE_Y_SADDR);
	__raw_writel((frm1_uv_data_addr << FRM_DATA_C_SADDR_BASE_SHIFT),
		     reg + FRM1_DATA_C_SADDR);
	__raw_writel((frm1_uv_table_addr << FRM_TABLE_C_SADDR_BASE_SHIFT),
		     reg + FRM1_TABLE_C_SADDR);
}

void VIOC_DTRC_RDMA_F1_SIZE(volatile void __iomem *reg, unsigned int frm1_width,
			    unsigned int frm1_height)
{
	unsigned long val;
	val = (((frm1_width & 0xFFFF) << FRM_SIZE_WIDTH_SHIFT) |
	       ((frm1_height & 0xFFFFF) << FRM_SIZE_HEIGHT_SHIFT));
	__raw_writel(val, reg + FRM1_SIZE);
}

void VIOC_DTRC_RDMA_F1_BYP_DETILE_ADDR(volatile void __iomem *reg,
				       unsigned int frm1_y_byp_detile_addr_s,
				       unsigned int frm1_y_byp_detile_addr_e,
				       unsigned int frm1_uv_byp_detile_addr_s,
				       unsigned int frm1_uv_byp_detile_addr_e)
{
	__raw_writel(
		(frm1_y_byp_detile_addr_s << FRM_BPS_TILE_YADR_S_BASE_SHIFT),
		reg + FRM1_BPS_TILE_YADR_S);
	__raw_writel(
		(frm1_y_byp_detile_addr_e << FRM_BPS_TILE_YADR_E_BASE_SHIFT),
		reg + FRM1_BPS_TILE_YADR_E);
	__raw_writel(
		(frm1_uv_byp_detile_addr_s << FRM_BPS_TILE_CADR_S_BASE_SHIFT),
		reg + FRM1_BPS_TILE_CADR_S);
	__raw_writel(
		(frm1_uv_byp_detile_addr_e << FRM_BPS_TILE_CADR_E_BASE_SHIFT),
		reg + FRM1_BPS_TILE_CADR_E);
}

void VIOC_DTRC_RDM_CTRL(volatile void __iomem *reg, unsigned int arid_reg_ctrl,
			unsigned int clk_dis, unsigned int soft_reset,
			unsigned int detile_by_addr, unsigned int pf_cnt_en)
{
	unsigned long val;
	val = (((arid_reg_ctrl & 0x3) << FRM_MISC_ARID_SHIFT) |
	       ((clk_dis & 0x1) << FRM_MISC_CD_SHIFT) |
	       ((soft_reset & 0x1) << FRM_MISC_SR_SHIFT) |
	       ((detile_by_addr & 0x1) << FRM_MISC_BDA_SHIFT) |
	       ((pf_cnt_en & 0x1) << FRM_MISC_CEN_SHIFT));
	__raw_writel(val, reg + FRM_MISC);
}

void VIOC_DTRC_RDMA_FETCH_ARID(volatile void __iomem *reg,
			       unsigned int data_fetch_arid,
			       unsigned int table_fetch_arid)
{
	unsigned long val;
	val = (((data_fetch_arid & 0xFF) << FETCH_ARID_DATA_ARID_SHIFT) |
	       ((table_fetch_arid & 0xFF) << FETCH_ARID_TABLE_ARID_SHIFT));
	__raw_writel(val, reg + FETCH_ARID);
}

void VIOC_DTRC_RDMA_TIMEOUT(volatile void __iomem *reg, unsigned int timeout)
{
	__raw_writel(((timeout & 0xFFFFFFFF) << TIMEOUT_CNT_SHIFT),
		     reg + TIMEOUT);
}

void VIOC_DTRC_RDMA_ARIDR(volatile void __iomem *reg, unsigned int ARID_REG)
{
	__raw_writel(((ARID_REG & 0xFFFFFFFF) << ARID_ARID_SHIFT), reg + ARID);
}

void VIOC_DTRC_RDMA_FRM0_CROP(volatile void __iomem *reg,
			      unsigned int frm0_crop, unsigned int frm0_xpos,
			      unsigned int frm0_ypos, unsigned int frm0_crop_w,
			      unsigned int frm0_crop_h)
{
	unsigned long val;
	val = (__raw_readl(reg + FRM_CTRL) & ~(FRM_CTRL_F0_CROP_MASK));
	val |= ((frm0_crop & 0x1) << FRM_CTRL_F0_CROP_SHIFT);
	__raw_writel(val, reg + FRM_CTRL);

	val = (((frm0_ypos & 0x3FFF) << FRM_CROPPOS_YPOS_SHIFT) |
	       ((frm0_xpos & 0x3FFF) << FRM_CROPPOS_XPOS_SHIFT));
	__raw_writel(val, reg + FRM0_CROPPOS);

	val = (((frm0_crop_h & 0x3FFF) << FRM_CROPSIZE_HEIGHT_SHIFT) |
	       ((frm0_crop_w & 0x3FFF) << FRM_CROPSIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + FRM0_CROPSIZE);
}

void VIOC_DTRC_RDMA_FRM1_CROP(volatile void __iomem *reg,
			      unsigned int frm1_crop, unsigned int frm1_xpos,
			      unsigned int frm1_ypos, unsigned int frm1_crop_w,
			      unsigned int frm1_crop_h)
{
	unsigned long val;
	val = (__raw_readl(reg + FRM_CTRL) & ~(FRM_CTRL_F1_CROP_MASK));
	val |= ((frm1_crop & 0x1) << FRM_CTRL_F1_CROP_SHIFT);
	__raw_writel(val, reg + FRM_CTRL);

	val = (((frm1_ypos & 0x3FFF) << FRM_CROPPOS_YPOS_SHIFT) |
	       ((frm1_xpos & 0x3FFF) << FRM_CROPPOS_XPOS_SHIFT));
	__raw_writel(val, reg + FRM1_CROPPOS);

	val = (((frm1_crop_h & 0x3FFF) << FRM_CROPSIZE_HEIGHT_SHIFT) |
	       ((frm1_crop_w & 0x3FFF) << FRM_CROPSIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + FRM1_CROPSIZE);
}

void VIOC_DTRC_RDMA_DTCTRL(volatile void __iomem *reg,
			   unsigned int mst_nontile_swap,
			   unsigned int mst_table_swap,
			   unsigned int mst_tile_data_swap,
			   unsigned int slv_rstc_data_swap,
			   unsigned int max_burst_len, unsigned int g1_src,
			   unsigned int g2_hotrst, unsigned int big_endian_en,
			   unsigned int mrg_g_arid)
{
	unsigned long val;
	val = (((mst_tile_data_swap & 0xF) << DTRC_CTRL2_MST_TILE_SWAP_SHIFT) |
	       ((mst_table_swap & 0xF) << DTRC_CTRL2_MST_TABLE_SWAP_SHIFT) |
	       ((mst_tile_data_swap & 0xF) << DTRC_CTRL2_MST_TILE_SWAP_SHIFT) |
	       ((slv_rstc_data_swap & 0xF) << DTRC_CTRL2_SLV_RSC_SWAP_SHIFT) |
	       ((max_burst_len & 0xFF) << DTRC_CTRL2_MAX_BURST_LEN_SHIFT) |
	       ((g1_src & 0x1) << DTRC_CTRL2_G1SRC_SHIFT) |
	       ((g2_hotrst & 0x1) << DTRC_CTRL2_G2HOTRST_SHIFT) |
	       ((big_endian_en & 0x1) << DTRC_CTRL2_10BENDIAN_SHIFT) |
	       ((mrg_g_arid & 0x1) << DTRC_CTRL2_MERGE_G_ARID_SHIFT));
	__raw_writel(val, reg + DTRC_CTRL2);
}

volatile void __iomem *VIOC_DTRC_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_DTRC_MAX)
		goto err;

	if (pDTRC_reg[Num] == NULL) {
		pr_err("[ERR][DTRC] num:%d ADDRESS is Null \n", Num);
		goto err;
	}

	return pDTRC_reg[Num];
err:
	pr_err("[ERR][DTRC] %s Num:%d max:%d \n", __func__, Num, VIOC_DTRC_MAX);
	return NULL;
}

void VIOC_DTRC_DUMP(unsigned int vioc_id, unsigned int size)
{
	volatile void __iomem *pReg;
	unsigned int cnt = 0;
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_DTRC_MAX)
		goto err;

	if (ViocDTRC_np == NULL)
		return;

	pReg = VIOC_DTRC_GetAddress(vioc_id);
	if (pReg == NULL)
		return;

	pr_debug("[DBG][DTRC] DTRC-%d :: 0x%p \n", Num, pReg);
	while (cnt < size) {
		pr_debug("[DBG][DTRC] 0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	return;

err:
	pr_err("[ERR][DTRC] %s Num:%d max:%d \n", __func__, Num, VIOC_DTRC_MAX);
	return;
}

static int __init vioc_dtrc_init(void)
{
	int i;
	ViocDTRC_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_dtrc");
	if (ViocDTRC_np == NULL) {
		pr_info("[INF][DTRC] vioc-dtrc: disabled\n");
	} else {
		for (i = 0; i < VIOC_DTRC_MAX; i++) {
			pDTRC_reg[i] = (volatile void __iomem *)of_iomap(ViocDTRC_np, i);

			if (pDTRC_reg[i])
				pr_info("[INF][DTRC] vioc-dtrc%d: 0x%p\n", i, pDTRC_reg[i]);
		}
	}

	return 0;
}
arch_initcall(vioc_dtrc_init);
