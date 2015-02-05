/*
 *  Hisilicon Terminal SoCs drm driver
 *
 *  Copyright (c) 2014-2015 Hisilicon Limited.
 *  Author:
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#ifndef __HISI_ADE_REG_H__
#define __HISI_ADE_REG_H__

/********** ADE Register Offset ***********/
#define ADE_CTRL_REG                (0x4)
#define ADE_CTRL1_REG               (0x8C)
#define ADE_SCL3_MUX_CFG_REG        (0x8)
#define ADE_SCL1_MUX_CFG_REG        (0xC)
#define ADE_ROT_SRC_CFG_REG         (0x10)
#define ADE_SCL2_SRC_CFG_REG        (0x14)
#define ADE_DISP_SRC_CFG_REG        (0x18)
#define ADE_WDMA2_SRC_CFG_REG       (0x1C)
#define ADE_SEC_OVLY_SRC_CFG_REG    (0x20)
#define ADE_WDMA3_SRC_CFG_REG       (0x24)
#define ADE_OVLY1_TRANS_CFG_REG     (0x2C)
#define ADE_CTRAN5_TRANS_CFG_REG    (0x40)
#define ADE_EN_REG                  (0x100)
#define INTR_MASK_CPU_0_REG         (0xC10)
#define INTR_MASK_CPU_1_REG         (0xC14)
#define ADE_FRM_DISGARD_CTRL_REG    (0xA4)
#define ADE_SOFT_RST_SEL0_REG       (0x78)
#define ADE_SOFT_RST_SEL1_REG       (0x7C)
#define ADE_RELOAD_DIS0_REG         (0xAC)
#define ADE_RELOAD_DIS1_REG         (0xB0)
#define ADE_OVLY_CTL_REG            (0x98)
#define RD_CH_DISP_CTRL_REG         (0x1404)
#define RD_CH_DISP_ADDR_REG         (0x1408)
#define RD_CH_DISP_SIZE_REG         (0x140C)
#define RD_CH_DISP_STRIDE_REG       (0x1410)
#define RD_CH_DISP_SPACE_REG        (0x1414)
#define RD_CH_DISP_EN_REG           (0x142C)
#define ADE_CTRAN5_DIS_REG          (0x5404)
#define ADE_CTRAN5_IMAGE_SIZE_REG   (0x543C)
#define ADE_CTRAN5_CFG_OK_REG       (0x5440)
#define ADE_CTRAN6_DIS_REG          (0x5504)
#define ADE_CTRAN6_IMAGE_SIZE_REG   (0x553C)
#define ADE_CTRAN6_CFG_OK_REG       (0x5540)

/********** ADE Register Union Struct ***********/
union U_ADE_CTRL1 {
struct {
	unsigned int	auto_clk_gate_en	:1;
	unsigned int	rot_buf_shr_out		:1;
	unsigned int	Reserved_44		:30;
	} bits;
	unsigned int	u32;
};

union U_ADE_SOFT_RST_SEL0 {
struct {
	unsigned int    ch1_rdma_srst_sel     :1;
	unsigned int    ch2_rdma_srst_sel     :1;
	unsigned int    ch3_rdma_srst_sel     :1;
	unsigned int    ch4_rdma_srst_sel     :1;
	unsigned int    ch5_rdma_srst_sel     :1;
	unsigned int    ch6_rdma_srst_sel     :1;
	unsigned int    disp_rdma_srst_sel    :1;
	unsigned int    cmdq1_rdma_srst_sel   :1;
	unsigned int    cmdq2_rdma_srst_sel   :1;
	unsigned int    Reserved_29           :1;
	unsigned int    ch1_wdma_srst_sel     :1;
	unsigned int    ch2_wdma_srst_sel     :1;
	unsigned int    ch3_wdma_srst_sel     :1;
	unsigned int    Reserved_28           :1;
	unsigned int    cmdq_wdma_srst_sel    :1;
	unsigned int    clip1_srst_sel        :1;
	unsigned int    clip2_srst_sel        :1;
	unsigned int    clip3_srst_sel        :1;
	unsigned int    clip4_srst_sel        :1;
	unsigned int    clip5_srst_sel        :1;
	unsigned int    clip6_srst_sel        :1;
	unsigned int    scl1_srst_sel         :1;
	unsigned int    scl2_srst_sel         :1;
	unsigned int    scl3_srst_sel         :1;
	unsigned int    ctran1_srst_sel       :1;
	unsigned int    ctran2_srst_sel       :1;
	unsigned int    ctran3_srst_sel       :1;
	unsigned int    ctran4_srst_sel       :1;
	unsigned int    ctran5_srst_sel       :1;
	unsigned int    ctran6_srst_sel       :1;
	unsigned int    rot_srst_sel          :1;
	unsigned int    Reserved_27           :1;
	} bits;
	unsigned int	u32;
};

union U_ADE_CTRL {
struct {
	unsigned int    frm_end_start         :2;
	unsigned int    dfs_buf_cfg           :1;
	unsigned int    rot_buf_cfg           :3;
	unsigned int    rd_ch5_nv             :1;
	unsigned int    rd_ch6_nv             :1;
	unsigned int    dfs_buf_unflow_lev1   :13;
	unsigned int    dfs_buf_unflow_lev2   :11;
	} bits;
	unsigned int	u32;
};


/********** ADE Register Write/Read functions ***********/
static inline void set_TOP_CTL_clk_gate_en(u8 *ade_base, u32 val)
{
	volatile union U_ADE_CTRL1   ade_ctrl1;
	u8 *reg_addr = ade_base + ADE_CTRL1_REG;

	ade_ctrl1.u32 = readl(reg_addr);
	ade_ctrl1.bits.auto_clk_gate_en = val;
	writel(ade_ctrl1.u32, reg_addr);
}

static inline void set_TOP_SOFT_RST_SEL0_disp_rdma(u8 *ade_base, u32 val)
{
	volatile union U_ADE_SOFT_RST_SEL0 ade_soft_rst;
	u8 *addr = ade_base + ADE_SOFT_RST_SEL0_REG;

	ade_soft_rst.u32 = readl(addr);
	ade_soft_rst.bits.disp_rdma_srst_sel = val;
	writel(ade_soft_rst.u32, addr);
}

static inline void set_TOP_SOFT_RST_SEL0_ctran5(u8 *ade_base, u32 val)
{
	volatile union U_ADE_SOFT_RST_SEL0 ade_soft_rst;
	u8 *addr = ade_base + ADE_SOFT_RST_SEL0_REG;

	ade_soft_rst.u32 = readl(addr);
	ade_soft_rst.bits.ctran5_srst_sel = val;
	writel(ade_soft_rst.u32, addr);
}

static inline void set_TOP_SOFT_RST_SEL0_ctran6(u8 *ade_base, u32 val)
{
	volatile union U_ADE_SOFT_RST_SEL0 ade_soft_rst;
	u8 *addr = ade_base + ADE_SOFT_RST_SEL0_REG;

	ade_soft_rst.u32 = readl(addr);
	ade_soft_rst.bits.ctran6_srst_sel = val;
	writel(ade_soft_rst.u32, addr);
}

static inline void set_TOP_CTL_frm_end_start(u8 *ade_base, u32 val)
{
	volatile union U_ADE_CTRL  ade_ctrl;
	u8 *reg_addr = ade_base + ADE_CTRL_REG;

	ade_ctrl.u32 = readl(reg_addr);
	ade_ctrl.bits.frm_end_start = val;
	writel(ade_ctrl.u32, reg_addr);
}

#endif
