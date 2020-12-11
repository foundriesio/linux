// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __DPTX_REG_H__
#define __DPTX_REG_H__

#include <linux/bitops.h>

/* Constants */
#define DPTX_MP_SINGLE_PIXEL		0
#define DPTX_MP_DUAL_PIXEL		1
#define DPTX_MP_QUAD_PIXEL		2

/* Controller Version Registers */
#define DPTX_VER_NUMBER			0x00
#define DPTX_VER_100a			0x31303061

#define DPTX_VER_TYPE			0x04

#define DPTX_ID					0x08
#define DPTX_ID_DEVICE_ID		0x9001
#define DPTX_ID_VENDOR_ID		0x16c3

/* DPTX Configuration Register */
#define DPTX_CONFIG1			0x100

/* CoreControl Register */
#define DPTX_CCTL				0x200
#define DPTX_SRST_CTRL			0x204

/* MST */
#define DPTX_MST_VCP_TABLE_REG_N(n)	(0x210 + (n) * 4)

#define DP_DPRX_ESI_LEN 14

/* Video Registers. N=0-3 */
#define DPTX_VSAMPLE_CTRL_N(n)		(0x300  + 0x10000 * (n))
#define DPTX_VSAMPLE_RESERVED1_N(n)	(0x304  + 0x10000 * (n))
#define DPTX_VSAMPLE_RESERVED2_N(n)	(0x308  + 0x10000 * (n))
#define DPTX_VSAMPLE_POLARITY_CTRL_N(n)	(0x30C  + 0x10000 * (n))
#define DPTX_VIDEO_CONFIG1_N(n)		(0x310  + 0x10000 * (n))
#define DPTX_VIDEO_CONFIG2_N(n)		(0x314  + 0x10000 * (n))
#define DPTX_VIDEO_CONFIG3_N(n)		(0x318  + 0x10000 * (n))
#define DPTX_VIDEO_CONFIG4_N(n)		(0x31c  + 0x10000 * (n))
#define DPTX_VIDEO_CONFIG5_N(n)		(0x320  + 0x10000 * (n))
#define DPTX_VIDEO_MSA1_N(n)		(0x324  + 0x10000 * (n))
#define DPTX_VIDEO_MSA2_N(n)		(0x328  + 0x10000 * (n))
#define DPTX_VIDEO_MSA3_N(n)		(0x32C  + 0x10000 * (n))
#define DPTX_VG_CONFIG1_N(n)		(0x3804 + 0x10000 * (n))
#define DPTX_VG_CONFIG2_N(n)		(0x3808 + 0x10000 * (n))
#define DPTX_VG_CONFIG3_N(n)		(0x380C + 0x10000 * (n))
#define DPTX_VG_CONFIG4_N(n)		(0x3810 + 0x10000 * (n))
#define DPTX_VG_CONFIG5_N(n)		(0x3814 + 0x10000 * (n))
#define DPTX_VG_SWRST_N(n)		(0x3800 + 0x10000 * (n))

#define DPTX_VIDEO_HBLANK_INTERVAL	0x330
#define DPTX_VIDEO_HBLANK_INTERVAL_N(n)		( 0x330 + 0x10000 * (n) )

#define DPTX_VIDEO_HBLANK_INTERVAL_MASK       GENMASK(15, 0)

#define DPTX_VIDEO_HBLANK_INTERVAL_SHIFT 16
#define DPTX_VIDEO_HBLANK_INTERVAL_ENABLE 1

/* Audio Registers */
#define DPTX_AUD_CONFIG1		0x400
#define DPTX_AUD_CONFIG1_N(n)	(0x400 + 0x10000 * (n))
#define DPTX_AG_CONFIG1			0x3904
#define DPTX_AG_CONFIG2			0x3908
#define DPTX_AG_CONFIG3			0x390C
#define DPTX_AG_CONFIG4			0x3910
#define DPTX_AG_CONFIG5			0x3914
#define DPTX_AG_CONFIG6			0x3918

/* SecondaryDataPacket Registers */
#define DPTX_SDP_VERTICAL_CTRL			0x500
#define DPTX_SDP_VERTICAL_CTRL_N(n)		(0x500 + 0x10000 * (n))
#define DPTX_SDP_HORIZONTAL_CTRL        0x504
#define DPTX_SDP_HORIZONTAL_CTRL_N(n)	(0x504 + 0x10000 * (n))
#define DPTX_SDP_STATUS			0x508
#define DPTX_SDP_BANK			0x600

/* PHY Layer Control Registers */
#define DPTX_PHYIF_CTRL			0xA00
#define DPTX_PHY_TX_EQ			0xA04
#define DPTX_CUSTOMPAT0			0xA08
#define DPTX_CUSTOMPAT1			0xA0C
#define DPTX_CUSTOMPAT2			0xA10
#define DPTX_PHYREG_CMDADDR		0xC00
#define DPTX_PHYREG_DATA		0xC04
#define DPTX_TYPE_C_CTRL		0xC08

/* AUX Channel Interface Registers */
#define DPTX_AUX_CMD			0xB00
#define DPTX_AUX_STS			0xB04
#define DPTX_AUX_DATA0			0xB08
#define DPTX_AUX_DATA1			0xB0C
#define DPTX_AUX_DATA2			0xB10
#define DPTX_AUX_DATA3			0xB14

/* Interrupt Registers */
#define DPTX_ISTS				0xD00
#define DPTX_IEN				0xD04
#define DPTX_HPDSTS				0xD08
#define DPTX_HPD_IEN			0xD0C

/* HDCP Registers */
#define DPTX_HDCP_CONFIG			0xE00
#define DPTX_HDCP_API_INT_MSK		0xE10

/* I2C APB Registers */
#define DPTX_I2C_ENABLE			0xc006c
#define DPTX_I2C_CTRL			0xc0000
#define DPTX_I2C_TARGET			0xc0004
#define DPTX_I2C_DATA_CMD		0xc0010
#define DPTX_PLUG_ORIENTATION_MASK	BIT(4)

/* RAM Registers */

#define DPTX_VG_RAM_ADDR_N(n)		(0x381C + 0x10000 * (n))
#define DPTX_VG_WRT_RAM_CTR_N(n)	(0x3820 + 0x10000 * (n))
#define DPTX_VG_WRT_RAM_DATA_N(n)	(0x3824 + 0x10000 * (n))

#define DPTX_VG_RAM_ADDR_START_SHIFT	12
#define DPTX_VG_RAM_ADDR_START_MASK	GENMASK(0, 12)
#define DPTX_VG_RAM_CTR_START_SHIFT	0
#define DPTX_VG_RAM_CTR_START_MASK	BIT(0)
#define DPTX_VG_WRT_RAM_DATA_SHIFT	0
#define DPTX_VG_WRT_RAM_DATA_MASK	GENMASK(0, 7)

/* Register Bitfields */
#define DPTX_ID_DEVICE_ID_SHIFT		16
#define DPTX_ID_DEVICE_ID_MASK		GENMASK(31, 16)
#define DPTX_ID_VENDOR_ID_SHIFT		0
#define DPTX_ID_VENDOR_ID_MASK		GENMASK(15, 0)

#define DPTX_CONFIG1_HDCP_EN					BIT(0)				/* 1 : Enable, 0 : Disable */
#define DPTX_CONFIG1_AUIDO_SELECTED_MASK		GENMASK(1, 2)
#define DPTX_CONFIG1_NUM_STREAMS_SHIFT		16
#define DPTX_CONFIG1_NUM_STREAMS_MASK		GENMASK(18, 16)
#define DPTX_CONFIG1_MP_MODE_SHIFT			19
#define DPTX_CONFIG1_MP_MODE_MASK			GENMASK(21, 19)		/* 1 : Single Pixel Mode, 2 : Dual Pixel Mode, 3 : Quard Pixel Mode */
#define DPTX_CONFIG1_MP_MODE_SINGLE			1
#define DPTX_CONFIG1_MP_MODE_DUAL			2
#define DPTX_CONFIG1_MP_MODE_QUAD			4
#define DPTX_CONFIG1_DSC_EN					BIT(22)				/* 1 : Display Stream Compression Enable, 0 : Disable */
#define DPTX_CONFIG1_FEC_EN					BIT(24)				/* 1 : Enable, 0: Disable */
#define DPTX_CONFIG1_NUM_DSC_ENC_SHIFT		25
#define DPTX_CONFIG1_NUM_DSC_ENC_MASK		GENMASK(28, 25)
#define DPTX_CONFIG1_GEN2_PHY				BIT(29)

#define DPTX_CCTL_SCRAMBLE_DIS				BIT(0)
#define DPTX_CCTL_ENH_FRAME_EN				BIT(1)
#define DPTX_CCTL_FAST_LINK_TRAINED_EN		BIT(2)
#define DPTX_CCTL_SCALE_DOWN_MODE			BIT(3)
#define DPTX_CCTL_ENABLE_MST_MODE			BIT(25)
#define DPTX_CCTL_ENABLE_FEC				BIT(26)
#define DPTX_CCTL_INITIATE_MST_ACT			BIT(28)
#define DPTX_CCTL_ENH_FRAME_FEC_EN			BIT(29)

#define DPTX_SRST_CTRL_CONTROLLER			BIT(0)
#define DPTX_SRST_CTRL_PHY					BIT(1)
#define DPTX_SRST_CTRL_HDCP					BIT(2)
#define DPTX_SRST_CTRL_AUDIO_SAMPLER		BIT(3)
#define DPTX_SRST_CTRL_AUX					BIT(4)
#define DPTX_SRST_VIDEO_RESET_0				BIT(5)
#define DPTX_SRST_VIDEO_RESET_1				BIT(6)
#define DPTX_SRST_VIDEO_RESET_2				BIT(7)
#define DPTX_SRST_VIDEO_RESET_3				BIT(8)
#define DPTX_SRST_AUDIO_SAMPLER_EAM1		BIT(9)
#define DPTX_SRST_AUDIO_SAMPLER_EAM2		BIT(10)
#define DPTX_SRST_AUDIO_SAMPLER_EAM3		BIT(11)

#define DPTX_SRST_VIDEO_RESET_N(n)			BIT(5 + n)

#if 1
#define DPTX_SRST_CTRL_ALL ( DPTX_SRST_CTRL_CONTROLLER |	DPTX_SRST_CTRL_PHY | DPTX_SRST_CTRL_HDCP | DPTX_SRST_CTRL_AUDIO_SAMPLER | DPTX_SRST_CTRL_AUX | \
								DPTX_SRST_VIDEO_RESET_0 | DPTX_SRST_VIDEO_RESET_1 | DPTX_SRST_VIDEO_RESET_2 | DPTX_SRST_VIDEO_RESET_3 | \
								DPTX_SRST_AUDIO_SAMPLER_EAM1 | DPTX_SRST_AUDIO_SAMPLER_EAM2 | DPTX_SRST_AUDIO_SAMPLER_EAM3 )
#else
#define DPTX_SRST_CTRL_ALL (DPTX_SRST_CTRL_CONTROLLER |		\
			    DPTX_SRST_CTRL_HDCP |		\
			    DPTX_SRST_CTRL_AUDIO_SAMPLER |	\
			    DPTX_SRST_CTRL_AUX)
#endif

#define DPTX_PHYIF_CTRL_TPS_SEL_SHIFT	0
#define DPTX_PHYIF_CTRL_TPS_SEL_MASK	GENMASK(3, 0)
#define DPTX_PHYIF_CTRL_TPS_NONE	0
#define DPTX_PHYIF_CTRL_TPS_1		1
#define DPTX_PHYIF_CTRL_TPS_2		2
#define DPTX_PHYIF_CTRL_TPS_3		3
#define DPTX_PHYIF_CTRL_TPS_4		4
#define DPTX_PHYIF_CTRL_TPS_SYM_ERM	5
#define DPTX_PHYIF_CTRL_TPS_PRBS7	6
#define DPTX_PHYIF_CTRL_TPS_CUSTOM80	7
#define DPTX_PHYIF_CTRL_TPS_CP2520_1	8
#define DPTX_PHYIF_CTRL_TPS_CP2520_2	9
#define DPTX_PHYIF_CTRL_RATE_SHIFT	4
#define DPTX_PHYIF_CTRL_RATE_MASK	GENMASK(5, 4)
#define DPTX_PHYIF_CTRL_LANES_SHIFT	6
#define DPTX_PHYIF_CTRL_LANES_MASK	GENMASK(7, 6)
#define DPTX_PHYIF_CTRL_RATE_RBR		0x0
#define DPTX_PHYIF_CTRL_RATE_HBR		0x1
#define DPTX_PHYIF_CTRL_RATE_HBR2		0x2
#define DPTX_PHYIF_CTRL_RATE_HBR3		0x3
#define DPTX_PHYIF_CTRL_XMIT_EN_SHIFT	8
#define DPTX_PHYIF_CTRL_XMIT_EN_MASK					GENMASK(11, 8)
#define DPTX_PHYIF_CTRL_XMIT_EN(lane)					BIT(8 + lane)
#define DPTX_PHYIF_CTRL_BUSY(lane)						BIT(12 + lane)
#define DPTX_PHYIF_CTRL_SSC_DIS							BIT(16)
#define DPTX_PHYIF_CTRL_LANE_PWRDOWN_SHIFT				17
#define DPTX_PHYIF_CTRL_LANE_PWRDOWN_MASK 				GENMASK(20, 17)		/* 0x001e 0000 ==> (( ~0UL << 17 ) & (~0UL >> ( 32 - 1 - 20 )))  ==> 0xffffffff << 17 -> 0xfffe 0000 &  0xffffffff >> 11 -> 0x001f ffff  ==> 001e 0000 */
#define DPTX_PHYIF_CTRL_WIDTH							BIT(25)

#define DPTX_PHY_TX_EQ_PREEMP_SHIFT(lane)	(6 * lane)
#define DPTX_PHY_TX_EQ_PREEMP_MASK(lane)	GENMASK(6 * lane + 1, 6 * lane)
#define DPTX_PHY_TX_EQ_VSWING_SHIFT(lane)	(6 * lane + 2)
#define DPTX_PHY_TX_EQ_VSWING_MASK(lane)	GENMASK(6 * lane + 3, \
							6 * lane + 2)

#define DPTX_AUX_CMD_REQ_LEN_SHIFT	0
#define DPTX_AUX_CMD_REQ_LEN_MASK	GENMASK(3, 0)
#define DPTX_AUX_CMD_I2C_ADDR_ONLY	BIT(4)
#define DPTX_AUX_CMD_ADDR_SHIFT		8
#define DPTX_AUX_CMD_ADDR_MASK		GENMASK(27, 8)
#define DPTX_AUX_CMD_TYPE_SHIFT		28
#define DPTX_AUX_CMD_TYPE_MASK		GENMASK(31, 28)
#define DPTX_AUX_CMD_TYPE_WRITE		0x0
#define DPTX_AUX_CMD_TYPE_READ		0x1
#define DPTX_AUX_CMD_TYPE_WSU		0x2
#define DPTX_AUX_CMD_TYPE_MOT		0x4
#define DPTX_AUX_CMD_TYPE_NATIVE	0x8

#define DPTX_AUX_STS_STATUS_SHIFT	4
#define DPTX_AUX_STS_STATUS_MASK	GENMASK(7, 4)
#define DPTX_AUX_STS_STATUS_ACK		0x0
#define DPTX_AUX_STS_STATUS_NACK	0x1
#define DPTX_AUX_STS_STATUS_DEFER	0x2
#define DPTX_AUX_STS_STATUS_I2C_NACK	0x4
#define DPTX_AUX_STS_STATUS_I2C_DEFER	0x8
#define DPTX_AUX_STS_AUXM_SHIFT		8
#define DPTX_AUX_STS_AUXM_MASK		GENMASK(15, 8)
#define DPTX_AUX_STS_REPLY_RECEIVED	BIT(16)
#define DPTX_AUX_STS_TIMEOUT		BIT(17)
#define DPTX_AUX_STS_REPLY_ERR		BIT(18)
#define DPTX_AUX_STS_BYTES_READ_SHIFT	19
#define DPTX_AUX_STS_BYTES_READ_MASK	GENMASK(23, 19)
#define DPTX_AUX_STS_SINK_DWA		BIT(24)

#define DPTX_TYPEC_DISABLE_ACK				BIT(0)
#define DPTX_TYPEC_DISABLE_STATUS			BIT(1)
#define DPTX_TYPEC_INTRURPPT_STATUS			BIT(2)

#define DPTX_ISTS_HPD			BIT(0)
#define DPTX_ISTS_AUX_REPLY		BIT(1)
#define DPTX_ISTS_HDCP			BIT(2)
#define DPTX_ISTS_AUX_CMD_INVALID	BIT(3)
#define DPTX_ISTS_SDP			BIT(4)
#define DPTX_ISTS_AUDIO_FIFO_OVERFLOW	BIT(5)
#define DPTX_ISTS_VIDEO_FIFO_OVERFLOW	BIT(6)
#define DPTX_ISTS_TYPE_C		BIT(7)
#define DPTX_ISTS_ALL_INTR	(DPTX_ISTS_HPD |			\
				 DPTX_ISTS_AUX_REPLY |			\
				 DPTX_ISTS_HDCP |			\
				 DPTX_ISTS_AUX_CMD_INVALID |		\
				 DPTX_ISTS_SDP |			\
				 DPTX_ISTS_AUDIO_FIFO_OVERFLOW |	\
				 DPTX_ISTS_VIDEO_FIFO_OVERFLOW |	\
				 DPTX_ISTS_TYPE_C)
#define DPTX_IEN_HPD			BIT(0)
#define DPTX_IEN_AUX_REPLY		BIT(1)
#define DPTX_IEN_HDCP			BIT(2)
#define DPTX_IEN_AUX_CMD_INVALID	BIT(3)
#define DPTX_IEN_SDP			BIT(4)
#define DPTX_IEN_AUDIO_FIFO_OVERFLOW	BIT(5)
#define DPTX_IEN_VIDEO_FIFO_OVERFLOW	BIT(6)
#define DPTX_IEN_TYPE_C			BIT(7)

#define DPTX_IEN_ALL_INTR	(DPTX_IEN_HPD |			\
				 DPTX_IEN_AUX_REPLY |		\
				 DPTX_IEN_HDCP |		\
				 DPTX_IEN_AUX_CMD_INVALID |	\
				 DPTX_IEN_SDP |			\
				 DPTX_IEN_AUDIO_FIFO_OVERFLOW | \
				 DPTX_IEN_VIDEO_FIFO_OVERFLOW | \
				 DPTX_IEN_TYPE_C)

#define DPTX_HPDSTS_IRQ			BIT(0)
#define DPTX_HPDSTS_HOT_PLUG		BIT(1)
#define DPTX_HPDSTS_HOT_UNPLUG		BIT(2)
#define DPTX_HPDSTS_UNPLUG_ERR_EN		BIT(3)
#define DPTX_HPDSTS_STATUS		BIT(8)
#define DPTX_HPDSTS_STATE_SHIFT		4
#define DPTX_HPDSTS_STATE_MASK		GENMASK(6, 4)
#define DPTX_HPDSTS_TIMER_SHIFT		16
#define DPTX_HPDSTS_TIMER_MASK		GENMASK(31, 16)

#define DPTX_HPD_IEN_IRQ_EN						DPTX_HPDSTS_IRQ
#define DPTX_HPD_IEN_HOT_PLUG_EN				DPTX_HPDSTS_HOT_PLUG
#define DPTX_HPD_IEN_HOT_UNPLUG_EN				DPTX_HPDSTS_HOT_UNPLUG

#define DPTX_AG_CONFIG1_WORD_WIDTH_SHIFT		6
#define DPTX_AG_CONFIG1_WORD_WIDTH_MASK			GENMASK(9, 6)
#define DPTX_AG_CONFIG1_USE_LUT_SHIFT			14
#define DPTX_AG_CONFIG1_USE_LUT_MASK			BIT(14)
#define DPTX_AG_CONFIG3_CH_NUMCL0_SHIFT			24
#define DPTX_AG_CONFIG3_CH_NUMCL0_MASK			GENMASK(27, 24)
#define DPTX_AG_CONFIG3_CH_NUMCR0_SHIFT			28
#define DPTX_AG_CONFIG3_CH_NUMCR0_MASK			GENMASK(31, 28)
#define DPTX_AG_CONFIG4_SAMP_FREQ_SHIFT			0
#define DPTX_AG_CONFIG4_SAMP_FREQ_MASK			GENMASK(3, 0)
#define DPTX_AG_CONFIG4_WORD_LENGTH_SHIFT		8
#define DPTX_AG_CONFIG4_WORD_LENGTH_MASK		GENMASK(11, 8)
#define DPTX_AG_CONFIG4_ORIG_SAMP_FREQ_SHIFT		12
#define DPTX_AG_CONFIG4_ORIG_SAMP_FREQ_MASK		GENMASK(15, 12)
#define DPTX_AUD_CONFIG1_INF_TYPE_SHIFT			0
#define DPTX_AUD_CONFIG1_INF_TYPE_MASK			BIT(0)
#define DPTX_AUD_CONFIG1_NCH_SHIFT				12
#define DPTX_AUD_CONFIG1_NCH_MASK				GENMASK(14, 12)
#define DPTX_AUD_CONFIG1_DATA_WIDTH_SHIFT		5
#define DPTX_AUD_CONFIG1_DATA_WIDTH_MASK		GENMASK(9, 5)
#define DPTX_AUD_CONFIG1_DATA_EN_IN_SHIFT		1
#define DPTX_AUD_CONFIG1_DATA_EN_IN_MASK		GENMASK(4, 1)
#define DPTX_AUD_CONFIG1_HBR_MODE_ENABLE_SHIFT			10
#define DPTX_AUD_CONFIG1_HBR_MODE_ENABLE_MASK			BIT(10)
#define DPTX_AUD_CONFIG1_ATS_VER_SHFIT			24
#define DPTX_AUD_CONFIG1_ATS_VER_MASK			GENMASK(29, 24)

#define DPTX_VSAMPLE_CTRL_STREAM_EN					BIT(5)
#define DPTX_VSAMPLE_CTRL_VMAP_BPC_SHIFT            16
#define DPTX_VSAMPLE_CTRL_VMAP_BPC_MASK             GENMASK(20, 16)
#define DPTX_VSAMPLE_CTRL_MULTI_PIXEL_SHIFT			21
#define DPTX_VSAMPLE_CTRL_MULTI_PIXEL_MASK			GENMASK(22, 21)
#define DPTX_VSAMPLE_CTRL_ENABLE_DSC				BIT(23)
#define DPTX_VSAMPLE_CTRL_QUAD_PIXEL				DPTX_MP_QUAD_PIXEL
#define DPTX_VSAMPLE_CTRL_DUAL_PIXEL				DPTX_MP_DUAL_PIXEL
#define DPTX_VSAMPLE_CTRL_SINGLE_PIXEL				DPTX_MP_SINGLE_PIXEL

#define DPTX_VIDEO_VMSA2_BPC_SHIFT                      29
#define DPTX_VIDEO_VMSA2_BPC_MASK                       GENMASK(31, 29)
#define DPTX_VIDEO_VMSA2_COL_SHIFT                      25
#define DPTX_VIDEO_VMSA2_COL_MASK                       GENMASK(28, 25)
#define DPTX_VIDEO_VMSA3_PIX_ENC_SHIFT                  31
#define DPTX_VIDEO_VMSA3_PIX_ENC_YCBCR420_SHIFT           30 // ignore MSA
#define DPTX_VIDEO_VMSA3_PIX_ENC_MASK                   GENMASK(31, 30)
#define DPTX_POL_CTRL_V_SYNC_POL_EN						BIT(0)
#define DPTX_POL_CTRL_H_SYNC_POL_EN						BIT(1)
#define DPTX_POL_CTRL_DE_IN_POL_EN						BIT(2)
#define DPTX_VIDEO_CONFIG1_IN_OSC_EN					BIT(0)
#define DPTX_VIDEO_CONFIG1_O_IP_EN						BIT(1)
#define DPTX_VIDEO_H_BLANK_SHIFT						2
#define DPTX_VIDEO_H_BLANK_MASK                 	    GENMASK(15, 2)
#define DPTX_VIDEO_H_ACTIVE_SHIFT						16
#define DPTX_VIDEO_H_ACTIVE_MASK                     	GENMASK(31, 16)
#define DPTX_VIDEO_V_BLANK_SHIFT						16
#define DPTX_VIDEO_V_BLANK_MASK                     	GENMASK(31, 16)
#define DPTX_VIDEO_V_ACTIVE_SHIFT						0
#define DPTX_VIDEO_V_ACTIVE_MASK                 	    GENMASK(15, 0)
#define DPTX_VIDEO_H_FRONT_PORCH_SHIFT					0
#define DPTX_VIDEO_H_FRONTE_PORCH_MASK            	    GENMASK(15, 0)
#define DPTX_VIDEO_H_SYNC_WIDTH_SHIFT					16
#define DPTX_VIDEO_H_SYNC_WIDTH_MASK                  	GENMASK(31, 16)
#define DPTX_VIDEO_V_FRONT_PORCH_SHIFT					0
#define DPTX_VIDEO_V_FRONTE_PORCH_MASK            	    GENMASK(15, 0)
#define DPTX_VIDEO_V_SYNC_WIDTH_SHIFT					16
#define DPTX_VIDEO_V_SYNC_WIDTH_MASK                  	GENMASK(31, 16)
#define DPTX_VIDEO_MSA1_H_START_SHIFT					0
#define DPTX_VIDEO_MSA1_V_START_SHIFT					16
#define DPTX_VIDEO_CONFIG5_TU_SHIFT						0
#define DPTX_VIDEO_CONFIG5_TU_MASK						GENMASK(6, 0)
#define DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_MST            14
#define DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_MST             GENMASK(19, 14)
#define DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_SST            16
#define DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_SST             GENMASK(19, 16)
#define DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_SHIFT			7
#define DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_MASK			GENMASK(13, 7)

#define DPTX_VG_CONFIG1_BPC_SHIFT			12
#define DPTX_VG_CONFIG1_BPC_MASK			GENMASK(14, 12)
#define DPTX_VG_CONFIG1_PATTERN_SHIFT		17
#define DPTX_VG_CONFIG1_PATTERN_MASK		GENMASK(18, 17)
#define DPTX_VG_CONFIG1_MULTI_PIXEL_SHIFT	19
#define DPTX_VG_CONFIG1_MULTI_PIXEL_MASK	GENMASK(20, 19)
#define DPTX_VG_CONFIG1_QUAD_PIXEL			DPTX_MP_QUAD_PIXEL
#define DPTX_VG_CONFIG1_DUAL_PIXEL			DPTX_MP_DUAL_PIXEL
#define DPTX_VG_CONFIG1_SINGLE_PIXEL		DPTX_MP_SINGLE_PIXEL
#define DPTX_VG_CONFIG1_ODE_POL_EN			BIT(0)
#define DPTX_VG_CONFIG1_OH_SYNC_POL_EN		BIT(1)
#define DPTX_VG_CONFIG1_OV_SYNC_POL_EN		BIT(2)
#define DPTX_VG_CONFIG1_OIP_EN				BIT(3)
#define DPTX_VG_CONFIG1_BLANK_IN_OSC_EN		BIT(5)
#define DPTX_VG_CONFIG1_YCC_422_EN			BIT(6)
#define DPTX_VG_CONFIG1_YCC_PATTERN_GEN_EN	BIT(7)
#define DPTX_VG_CONFIG1_YCC_420_EN			BIT(15)
#define DPTX_VG_CONFIG1_PIXEL_REP_SHIFT		12
#define DPTX_VG_CONFIG2_H_ACTIVE_SHIFT		0
#define DPTX_VG_CONFIG2_H_BLANK_SHIFT		16

#define DPTX_EN_AUDIO_TIMESTAMP_SDP			BIT(0)
#define DPTX_EN_AUDIO_STREAM_SDP			BIT(1)
#define DPTX_EN_AUDIO_INFOFRAME_SDP			BIT(2)
#define DPTX_DISABLE_EXT_SDP					BIT(30)
#define DPTX_FIXED_PRIORITY_ARBITRATION			BIT(31)

#define DPTX_EN_AUDIO_CH_1					1
#define DPTX_EN_AUDIO_CH_2					1
#define DPTX_EN_AUDIO_CH_3              	3
#define DPTX_EN_AUDIO_CH_4					9
#define DPTX_EN_AUDIO_CH_5					7
#define DPTX_EN_AUDIO_CH_6					7
#define DPTX_EN_AUDIO_CH_7					0xF
#define DPTX_EN_AUDIO_CH_8					0xF
#define DPTX_AUDIO_MUTE						BIT(15)

#define DPTX_HDCP_REG_RMLCTL				0x3614
#define DPTX_HDCP_CFG						0xE00
#define DPTX_HDCP_REG_RMLSTS				0x3618
#define DPTX_HDCP_REG_DPK_CRC				0x3630
#define DPTX_HDCP_REG_DPK0					0x3620
#define DPTX_HDCP_REG_DPK1					0x3624
#define DPTX_HDCP_REG_SEED					0x361C
#define DPTX_HDCP_INT_STS					0xE0C
#define DPTX_HDCP_INT_CLR					0xE08
#define DPTX_HDCP_OBS						0xE04
#define DPTX_HDCP22GPIOSTS          	    0x3628
#define DPTX_HDCP22GPIOOUTCHNGSTS			0x362c

#define DPTX_HDCP_CFG_DPCD_PLUS_SHIFT			7
#define DPTX_HDCP_CFG_DPCD_PLUS_MASK			BIT(7)
#define DPTX_HDCP_KSV_ACCESS_INT				BIT(0)
#define DPTX_HDCP_AUX_RESP_TIMEOUT				BIT(3)
#define DPTX_HDCP_KSV_SHA1						BIT(5)
#define DPTX_HDCP_FAILED						BIT(6)
#define DPTX_HDCP_ENGAGED						BIT(7)
#define DPTX_HDCP22_GPIOINT						BIT(8)
#define DPTX_REVOC_LIST_MASK					GENMASK(31, 24)
#define DPTX_REVOC_SIZE_SHIFT					23
#define DPTX_REVOC_SIZE_MASK					GENMASK(23, 8)
#define DPTX_ODPK_DECRYPT_ENABLE_SHIFT			0
#define DPTX_ODPK_DECRYPT_ENABLE_MASK			BIT(0)
#define DPTX_IDPK_DATA_INDEX_SHIFT				0
#define DPTX_IDPK_DATA_INDEX_MASK				GENMASK(5, 0)
#define DPTX_IDPK_WR_OK_STS_SHIFT				6
#define DPTX_IDPK_WR_OK_STS_MASK				BIT(6)
#define DPTX_CFG_EN_HDCP13						BIT(2)
#define DPTX_CFG_CP_IRQ							BIT(6)
#define DPTX_CFG_EN_HDCP						BIT(1)
#define DPTX_HDCP22_BOOTED						BIT(23)
#define DPTX_HDCP22_SINK_CAP_CHECK_COMPLETE		BIT(27)
#define DPTX_HDCP22_SINK_CAPABLE_SINK			BIT(28)
#define DPTX_HDCP22_AUTHENTICATION_SUCCESS		BIT(29)
#define DPTX_HDCP22_AUTHENTICATION_FAILED		BIT(30)

#define DPTX_DSC_HWCFG							0x0230
#define DPTX_DSC_NUM_ENC_MSK					GENMASK(3,0)

#define DPTX_DSC_CTL							0x0234
#define DPTX_DSC_STREAM0_LOG2_NUM_SLICES_MSK	GENMASK(23, 22)
#define DPTX_DSC_STREAM0_LOG2_NUM_SLICES_SHIFT	22

#define DPTX_DSC_STREAM0_ENC_CLK_DIV_MSK		BIT(18)
#define DPTX_DSC_STREAM0_ENC_CLK_DIV_SHIFT		18

#define DPTX_DSC_ENC_STREAM_SEL(i)				GENMASK((2*i+1),(2*i))
#define DPTX_DSC_ENC_STREAM_SEL_SHIFT(i)		((i)*2)
#define DPTX_DSC_STREAM0_NUM_SLICES_SHIFT		22

// Stream should be [1 .. 4],  SST = 1
#define DPTX_DSC_PPS(stream, i)					((0x3a00 + ( stream - 1 ) * 0x10000) + ( i * 0x4 ))
#define DPTX_SST_MODE							1

// PPS SDPs
#define DPTX_DSC_VER_MIN_SHIFT				0
#define DPTX_DSC_VER_MAJ_SHIFT				4
#define DPTX_DSC_BUF_DEPTH_SHIFT			24
#define DPTX_DSC_BLOCK_PRED_SHIFT			5
#define DPTX_DSC_BPP_HIGH_MASK				GENMASK(9,8)
#define DPTX_DSC_BPP_LOW_MASK				GENMASK(7,0)
#define DPTX_DSC_BPP_LOW_MASK1				GENMASK(15,8)
#define DPTX_DSC_BPC_SHIFT					28

#define DPTX_CONFIG_REG2					0x104
#define DSC_MAX_NUM_LINES_SHIFT				16
#define DSC_MAX_NUM_LINES_MASK				GENMASK(31, 16)

#define DPTX_VIDEO_DSCCFG					0x334
#define DPTX_DSC_LSTEER_INT_SHIFT			0
#define DPTX_DSC_LSTEER_FRAC_SHIFT			5
#define DPTX_DSC_LSTEER_XMIT_DELAY_SHIFT	16
#define DPTX_DSC_LSTEER_XMIT_DELAY_MASK     GENMASK(31,16)
#define DPTX_DSC_LSTEER_FRAC_SHIFT_MASK     GENMASK(9,5)
#define DPTX_DSC_LSTEER_INT_SHIFT_MASK      GENMASK(4,0)

#define DPTX_BITS_PER_PIXEL					8 // For Interop testing only RGB 8bpp 8bpc
#define DPTX_BITS_PER_COMPONENT				8 // For Interop testing only RGB 8bpp 8bpc


/****************************************************************
*																*
*							HDCP								*
*																*
****************************************************************/
#define DPTX_HDCP22_ELP_PKF_0           0x70004
#define DPTX_HDCP22_ELP_PKF_1           0x70008
#define DPTX_HDCP22_ELP_PKF_2           0x7000C
#define DPTX_HDCP22_ELP_PKF_3           0x70010
#define DPTX_HDCP22_ELP_DUK_0           0x70014
#define DPTX_HDCP22_ELP_DUK_1           0x70018
#define DPTX_HDCP22_ELP_DUK_2           0x7001C
#define DPTX_HDCP22_ELP_DUK_3           0x70020


/****************************************************************
*																*
*							CLK									*
*																*
****************************************************************/
#define DPTX_CLKCTRL0 			        0x0000
#define DPTX_CLKCTRL1 			        0x0004
#define DPTX_CLKCTRL2 			        0x0008
#define DPTX_CLKCTRL3 			        0x000C

#define	DPTX_CLKCTRL0_PLL_DIRECT_OUTPUT			0x01
#define	DPTX_CLKCTRL0_PLL_DIVIDER_OUTPUT		0x02

#define DPTX_CKC_CFG_CLKCTRL0	        0x0000
#define DPTX_CKC_CFG_CLKCTRL1	        0x0004
#define DPTX_CKC_CFG_CLKCTRL2	        0x0008
#define DPTX_CKC_CFG_CLKCTRL3	        0x000C
#define DPTX_CKC_CFG_PLLPMS		        0x0010
#define DPTX_CKC_CFG_PLLCON		        0x0014
#define DPTX_CKC_CFG_PLLMON		        0x0018
#define DPTX_CKC_CFG_CLKDIVC0	        0x001C
#define DPTX_CKC_CFG_CLKDIVC1	        0x0020
#define DPTX_CKC_CFG_CLKDIVC2	        0x0024
#define DPTX_CKC_CFG_CLKDIVC3	        0x0028

#define DPTX_PLLPMS_LOCK_MASK			BIT( 23 )


/****************************************************************
*																*
*							DP TCRL								*
*																*
****************************************************************/
#define DPTX_APB_SEL_REG				0x38
#define DPTX_APB_SEL_MASK				24


/****************************************************************
*																*
*							DPCD								*
*																*
****************************************************************/
#define DP_LINK_QUAL_LANE0_SET	            0x10B
#define DP_LINK_QUAL_LANE1_SET	            0x10C
#define DP_LINK_QUAL_LANE2_SET	            0x10D
#define DP_LINK_QUAL_LANE3_SET	            0x10E

#define	SINK_TDOWNSPREAD_MASK							0x01
#define	SINK_TRAINING_AUX_RD_INTERVAL_MASK				0x7F


/****************************************************************
*																*
*							Register Bank						*
*																*
****************************************************************/
#define	DP_REGISTER_BANK_REG_0					0x00000000
#define	DP_REGISTER_BANK_REG_1					0x00000004
#define	DP_REGISTER_BANK_REG_2					0x00000008
#define	DP_REGISTER_BANK_REG_3					0x0000000C

#define	DP_REGISTER_BANK_REG_4					0x00000010
#define RESET_PLL_MASK							0x80000000
#define ENABLE_BYPASS_MODE_MASK					0x00200000

#define	DP_REGISTER_BANK_REG_5					0x00000014
#define	DP_REGISTER_BANK_REG_6					0x00000018
#define	DP_REGISTER_BANK_REG_7					0x0000001C
#define	DP_REGISTER_BANK_REG_8					0x00000020
#define	DP_REGISTER_BANK_REG_9					0x00000024
#define	DP_REGISTER_BANK_REG_10					0x00000028
#define	DP_REGISTER_BANK_REG_11					0x0000002C
#define	DP_REGISTER_BANK_REG_12					0x00000030
#define	DP_REGISTER_BANK_REG_13					0x00000034
#define	DP_REGISTER_BANK_REG_14					0x00000038
#define	DP_REGISTER_BANK_REG_15					0x0000003C
#define	DP_REGISTER_BANK_REG_16					0x00000040
#define	DP_REGISTER_BANK_REG_17					0x00000044
#define	DP_REGISTER_BANK_REG_20					0x00000050
#define	DP_REGISTER_BANK_REG_21					0x00000054
#define	DP_REGISTER_BANK_REG_22					0x00000058
#define	DP_REGISTER_BANK_REG_23					0x0000005C

/****************************************************************
*																*
*							Protect								*
*																*
****************************************************************/
#define	DP_PORTECT_CFG_ACCESS					0x00
#define	DP_PORTECT_CFG_PW_OK					0x04
#define	DP_PORTECT_CFG_LOCK						0x08

#define	DP_PORTECT_CFG_PW_VAL					0x8050ACE5
#define	DP_PORTECT_CFG_LOCKED					0x01
#define	DP_PORTECT_CFG_UNLOCKED					0x00
#define	DP_PORTECT_CFG_NOT_ACCESSABLE			0x00
#define	DP_PORTECT_CFG_ACCESSABLE				0x01

/****************************************************************
*																*
*						DP Audio Sel							*
*																*
****************************************************************/
#define DPTX_AUDIO_SEL_REG				0x16051058
#define DPTX_AUDIO_SEL_MASK				0xFFFF


/****************************************************************
*																*
*						PMU										*
*																*
****************************************************************/
#define PMU_PWRSTS0						0x00
#define PMU_PWRSTS1						0x04

#define PMU_HSM_RSTN_MASK				0x08
#define PMU_MIC_PWDN_MASK				0x00001800
#define PMU_TOP_PWDN_MASK				0x00001000
#define PMU_WATCHDOG_RESET_MASK			0x00002000
#define PMU_MIC_WARM_RESET_MASK			0x00004000
#define PMU_WARM_RESET_MASK				0x00008000
#define PMU_PVT_RESET_MASK				0x00010000
#define PMU_SW_RESET_MASK				0x00020000
#define PMU_COLD_RESET_MASK				0x00040000


/****************************************************************
*																*
*						Aux										*
*																*
****************************************************************/
#define DP_DPCD_REV                         0x000

#define DP_MAX_LINK_RATE                    0x001
	
#define DP_MAX_LANE_COUNT                   0x002
#define DP_MAX_LANE_COUNT_MASK		    0x1f
#define DP_TPS3_SUPPORTED		    (1 << 6) /* 1.2 */
#define DP_ENHANCED_FRAME_CAP		    (1 << 7)
	
#define DP_MAX_DOWNSPREAD                   0x003
#define DP_NO_AUX_HANDSHAKE_LINK_TRAINING  (1 << 6)
	
#define DP_NORP                             0x004
	
#define DP_DOWNSTREAMPORT_PRESENT           0x005
#define DP_DWN_STRM_PORT_PRESENT           (1 << 0)
#define DP_DWN_STRM_PORT_TYPE_MASK         0x06
#define DP_DWN_STRM_PORT_TYPE_DP           (0 << 1)
#define DP_DWN_STRM_PORT_TYPE_ANALOG       (1 << 1)
#define DP_DWN_STRM_PORT_TYPE_TMDS         (2 << 1)
#define DP_DWN_STRM_PORT_TYPE_OTHER        (3 << 1)
#define DP_FORMAT_CONVERSION               (1 << 3)
#define DP_DETAILED_CAP_INFO_AVAILABLE	    (1 << 4) /* DPI */
	
#define DP_MAIN_LINK_CHANNEL_CODING         0x006
	
#define DP_DOWN_STREAM_PORT_COUNT	    0x007
#define DP_PORT_COUNT_MASK		    0x0f
#define DP_MSA_TIMING_PAR_IGNORED	    (1 << 6) /* eDP */
#define DP_OUI_SUPPORT			    (1 << 7)
	
#define DP_I2C_SPEED_CAP		    0x00c    /* DPI */
#define DP_I2C_SPEED_1K		    0x01
#define DP_I2C_SPEED_5K		    0x02
#define DP_I2C_SPEED_10K		    0x04
#define DP_I2C_SPEED_100K		    0x08
#define DP_I2C_SPEED_400K		    0x10
#define DP_I2C_SPEED_1M		    0x20
	
#define DP_EDP_CONFIGURATION_CAP            0x00d   /* XXX 1.2? */
#define DP_TRAINING_AUX_RD_INTERVAL         0x00e   /* XXX 1.2? */
	
	/* Multiple stream transport */
#define DP_FAUX_CAP			    0x020   /* 1.2 */
#define DP_FAUX_CAP_1			    (1 << 0)
	
#define DP_MSTM_CAP			    0x021   /* 1.2 */
#define DP_MST_CAP			    (1 << 0)
	
#define DP_GUID				    0x030   /* 1.2 */
	
#define DP_PSR_SUPPORT                      0x070   /* XXX 1.2? */
#define DP_PSR_IS_SUPPORTED                1
#define DP_PSR_CAPS                         0x071   /* XXX 1.2? */
#define DP_PSR_NO_TRAIN_ON_EXIT            1
#define DP_PSR_SETUP_TIME_330              (0 << 1)
#define DP_PSR_SETUP_TIME_275              (1 << 1)
#define DP_PSR_SETUP_TIME_220              (2 << 1)
#define DP_PSR_SETUP_TIME_165              (3 << 1)
#define DP_PSR_SETUP_TIME_110              (4 << 1)
#define DP_PSR_SETUP_TIME_55               (5 << 1)
#define DP_PSR_SETUP_TIME_0                (6 << 1)
#define DP_PSR_SETUP_TIME_MASK             (7 << 1)
#define DP_PSR_SETUP_TIME_SHIFT            1
	
	/*
	 * 0x80-0x8f describe downstream port capabilities, but there are two layouts
	 * based on whether DP_DETAILED_CAP_INFO_AVAILABLE was set.  If it was not,
	 * each port's descriptor is one byte wide.  If it was set, each port's is
	 * four bytes wide, starting with the one byte from the base info.	As of
	 * DP interop v1.1a only VGA defines additional detail.
	 */
	
	/* offset 0 */
#define DP_DOWNSTREAM_PORT_0		    0x80
#define DP_DS_PORT_TYPE_MASK		    (7 << 0)
#define DP_DS_PORT_TYPE_DP		    0
#define DP_DS_PORT_TYPE_VGA		    1
#define DP_DS_PORT_TYPE_DVI		    2
#define DP_DS_PORT_TYPE_HDMI		    3
#define DP_DS_PORT_TYPE_NON_EDID	    4
#define DP_DS_PORT_HPD			    (1 << 3)
	/* offset 1 for VGA is maximum megapixels per second / 8 */
	/* offset 2 */
#define DP_DS_VGA_MAX_BPC_MASK		    (3 << 0)
#define DP_DS_VGA_8BPC			    0
#define DP_DS_VGA_10BPC		    1
#define DP_DS_VGA_12BPC		    2
#define DP_DS_VGA_16BPC		    3
	
	/* link configuration */
#define	DP_LINK_BW_SET		            0x100
#define DP_LINK_BW_1_62		    0x06
#define DP_LINK_BW_2_7			    0x0a
#define DP_LINK_BW_5_4			    0x14    /* 1.2 */
	
#define DP_LANE_COUNT_SET	            0x101
#define DP_LANE_COUNT_MASK		    0x0f
#define DP_LANE_COUNT_ENHANCED_FRAME_EN    (1 << 7)
	
#define DP_TRAINING_PATTERN_SET	            0x102
#define DP_TRAINING_PATTERN_DISABLE	    0
#define DP_TRAINING_PATTERN_1		    1
#define DP_TRAINING_PATTERN_2		    2
#define DP_TRAINING_PATTERN_3		    3	    /* 1.2 */
#define DP_TRAINING_PATTERN_MASK	    0x3
	
#define DP_LINK_QUAL_PATTERN_DISABLE	    (0 << 2)
#define DP_LINK_QUAL_PATTERN_D10_2	    (1 << 2)
#define DP_LINK_QUAL_PATTERN_ERROR_RATE    (2 << 2)
#define DP_LINK_QUAL_PATTERN_PRBS7	    (3 << 2)
#define DP_LINK_QUAL_PATTERN_MASK	    (3 << 2)
	
#define DP_RECOVERED_CLOCK_OUT_EN	    (1 << 4)
#define DP_LINK_SCRAMBLING_DISABLE	    (1 << 5)
	
#define DP_SYMBOL_ERROR_COUNT_BOTH	    (0 << 6)
#define DP_SYMBOL_ERROR_COUNT_DISPARITY    (1 << 6)
#define DP_SYMBOL_ERROR_COUNT_SYMBOL	    (2 << 6)
#define DP_SYMBOL_ERROR_COUNT_MASK	    (3 << 6)
	
#define DP_TRAINING_LANE0_SET		    0x103
#define DP_TRAINING_LANE1_SET		    0x104
#define DP_TRAINING_LANE2_SET		    0x105
#define DP_TRAINING_LANE3_SET		    0x106
	
#define DP_TRAIN_VOLTAGE_SWING_MASK	    0x3
#define DP_TRAIN_VOLTAGE_SWING_SHIFT	    0
#define DP_TRAIN_MAX_SWING_REACHED	    (1 << 2)
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_0 (0 << 0)
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_1 (1 << 0)
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_2 (2 << 0)
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_3 (3 << 0)
	
#define DP_TRAIN_PRE_EMPHASIS_MASK	    (3 << 3)
#define DP_TRAIN_PRE_EMPH_LEVEL_0		(0 << 3)
#define DP_TRAIN_PRE_EMPH_LEVEL_1		(1 << 3)
#define DP_TRAIN_PRE_EMPH_LEVEL_2		(2 << 3)
#define DP_TRAIN_PRE_EMPH_LEVEL_3		(3 << 3)
	
#define DP_TRAIN_PRE_EMPHASIS_SHIFT	    3
#define DP_TRAIN_MAX_PRE_EMPHASIS_REACHED  (1 << 5)
	
#define DP_DOWNSPREAD_CTRL		    0x107
#define DP_SPREAD_AMP_0_5		    (1 << 4)
#define DP_MSA_TIMING_PAR_IGNORE_EN	    (1 << 7) /* eDP */
	
#define DP_MAIN_LINK_CHANNEL_CODING_SET	    0x108
#define DP_SET_ANSI_8B10B		    (1 << 0)
	
#define DP_I2C_SPEED_CONTROL_STATUS	    0x109   /* DPI */
	/* bitmask as for DP_I2C_SPEED_CAP */
	
#define DP_EDP_CONFIGURATION_SET            0x10a   /* XXX 1.2? */
	
#define DP_MSTM_CTRL			    0x111   /* 1.2 */
#define DP_MST_EN			    (1 << 0)
#define DP_UP_REQ_EN			    (1 << 1)
#define DP_UPSTREAM_IS_SRC		    (1 << 2)
	
#define DP_PSR_EN_CFG			    0x170   /* XXX 1.2? */
#define DP_PSR_ENABLE			    (1 << 0)
#define DP_PSR_MAIN_LINK_ACTIVE	    (1 << 1)
#define DP_PSR_CRC_VERIFICATION	    (1 << 2)
#define DP_PSR_FRAME_CAPTURE		    (1 << 3)
	
#define DP_ADAPTER_CTRL			    0x1a0
#define DP_ADAPTER_CTRL_FORCE_LOAD_SENSE   (1 << 0)
	
#define DP_BRANCH_DEVICE_CTRL		    0x1a1
#define DP_BRANCH_DEVICE_IRQ_HPD	    (1 << 0)
	
#define DP_PAYLOAD_ALLOCATE_SET		    0x1c0
#define DP_PAYLOAD_ALLOCATE_START_TIME_SLOT 0x1c1
#define DP_PAYLOAD_ALLOCATE_TIME_SLOT_COUNT 0x1c2
	
#define DP_SINK_COUNT			    0x200
	/* prior to 1.2 bit 7 was reserved mbz */
#define DP_GET_SINK_COUNT(x)		    ((((x) & 0x80) >> 1) | ((x) & 0x3f))
#define DP_SINK_CP_READY		    (1 << 6)
	
#define DP_DEVICE_SERVICE_IRQ_VECTOR	    0x201
#define DP_REMOTE_CONTROL_COMMAND_PENDING  (1 << 0)
#define DP_AUTOMATED_TEST_REQUEST	    (1 << 1)
#define DP_CP_IRQ			    (1 << 2)
#define DP_MCCS_IRQ			    (1 << 3)
#define DP_DOWN_REP_MSG_RDY		    (1 << 4) /* 1.2 MST */
#define DP_UP_REQ_MSG_RDY		    (1 << 5) /* 1.2 MST */
#define DP_SINK_SPECIFIC_IRQ		    (1 << 6)
	
#define DP_LANE0_1_STATUS		    0x202
#define DP_LANE2_3_STATUS		    0x203
#define DP_LANE_CR_DONE		    (1 << 0)
#define DP_LANE_CHANNEL_EQ_DONE	    (1 << 1)
#define DP_LANE_SYMBOL_LOCKED		    (1 << 2)
	
#define DP_CHANNEL_EQ_BITS (DP_LANE_CR_DONE |		\
					DP_LANE_CHANNEL_EQ_DONE |	\
					DP_LANE_SYMBOL_LOCKED)
	
#define DP_LANE_ALIGN_STATUS_UPDATED	    0x204
	
#define DP_INTERLANE_ALIGN_DONE		    (1 << 0)
#define DP_DOWNSTREAM_PORT_STATUS_CHANGED   (1 << 6)
#define DP_LINK_STATUS_UPDATED		    (1 << 7)
	
#define DP_SINK_STATUS			    0x205
#define DP_SINK_STATUS_PORT0_IN_SYNC	    (1 << 0)
	
#define DP_RECEIVE_PORT_0_STATUS	    (1 << 0)
#define DP_RECEIVE_PORT_1_STATUS	    (1 << 1)
	
#define DP_ADJUST_REQUEST_LANE0_1	    0x206
#define DP_ADJUST_REQUEST_LANE2_3	    0x207
#define DP_ADJUST_VOLTAGE_SWING_LANE0_MASK  0x03
#define DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT 0
#define DP_ADJUST_PRE_EMPHASIS_LANE0_MASK   0x0c
#define DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT  2
#define DP_ADJUST_VOLTAGE_SWING_LANE1_MASK  0x30
#define DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT 4
#define DP_ADJUST_PRE_EMPHASIS_LANE1_MASK   0xc0
#define DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT  6
	
#define DP_TEST_REQUEST			    0x218
#define DP_TEST_LINK_TRAINING		    (1 << 0)
#define DP_TEST_LINK_VIDEO_PATTERN	    (1 << 1)
#define DP_TEST_LINK_EDID_READ		    (1 << 2)
#define DP_TEST_LINK_PHY_TEST_PATTERN	    (1 << 3) /* DPCD >= 1.1 */
#define DP_TEST_LINK_FAUX_PATTERN	    (1 << 4) /* DPCD >= 1.2 */
	
#define DP_TEST_LINK_RATE		    0x219
#define DP_LINK_RATE_162		    (0x6)
#define DP_LINK_RATE_27		    (0xa)
	
#define DP_TEST_LANE_COUNT		    0x220
	
#define DP_TEST_PATTERN			    0x221
	
#define DP_TEST_CRC_R_CR		    0x240
#define DP_TEST_CRC_G_Y			    0x242
#define DP_TEST_CRC_B_CB		    0x244
	
#define DP_TEST_SINK_MISC		    0x246
#define DP_TEST_CRC_SUPPORTED		    (1 << 5)
	
#define DP_TEST_RESPONSE		    0x260
#define DP_TEST_ACK			    (1 << 0)
#define DP_TEST_NAK			    (1 << 1)
#define DP_TEST_EDID_CHECKSUM_WRITE	    (1 << 2)
	
#define DP_TEST_EDID_CHECKSUM		    0x261
	
#define DP_TEST_SINK			    0x270
#define DP_TEST_SINK_START	    (1 << 0)
	
#define DP_PAYLOAD_TABLE_UPDATE_STATUS      0x2c0   /* 1.2 MST */
#define DP_PAYLOAD_TABLE_UPDATED           (1 << 0)
#define DP_PAYLOAD_ACT_HANDLED             (1 << 1)
	
#define DP_VC_PAYLOAD_ID_SLOT_1             0x2c1   /* 1.2 MST */
	/* up to ID_SLOT_63 at 0x2ff */
	
#define DP_SOURCE_OUI			    0x300
#define DP_SINK_OUI			    0x400
#define DP_BRANCH_OUI			    0x500
	
#define DP_SET_POWER                        0x600
#define DP_SET_POWER_D0                    0x1
#define DP_SET_POWER_D3                    0x2
#define DP_SET_POWER_MASK                  0x3
	
#define DP_SIDEBAND_MSG_DOWN_REQ_BASE	    0x1000   /* 1.2 MST */
#define DP_SIDEBAND_MSG_UP_REP_BASE	    0x1200   /* 1.2 MST */
#define DP_SIDEBAND_MSG_DOWN_REP_BASE	    0x1400   /* 1.2 MST */
#define DP_SIDEBAND_MSG_UP_REQ_BASE	    0x1600   /* 1.2 MST */
	
#define DP_SINK_COUNT_ESI		    0x2002   /* 1.2 */
	/* 0-5 sink count */
#define DP_SINK_COUNT_CP_READY             (1 << 6)
	
#define DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0   0x2003   /* 1.2 */
	
#define DP_DEVICE_SERVICE_IRQ_VECTOR_ESI1   0x2004   /* 1.2 */
	
#define DP_LINK_SERVICE_IRQ_VECTOR_ESI0     0x2005   /* 1.2 */
	
#define DP_PSR_ERROR_STATUS                 0x2006  /* XXX 1.2? */
#define DP_PSR_LINK_CRC_ERROR              (1 << 0)
#define DP_PSR_RFB_STORAGE_ERROR           (1 << 1)
	
#define DP_PSR_ESI                          0x2007  /* XXX 1.2? */
#define DP_PSR_CAPS_CHANGE                 (1 << 0)
	
#define DP_PSR_STATUS                       0x2008  /* XXX 1.2? */
#define DP_PSR_SINK_INACTIVE               0
#define DP_PSR_SINK_ACTIVE_SRC_SYNCED      1
#define DP_PSR_SINK_ACTIVE_RFB             2
#define DP_PSR_SINK_ACTIVE_SINK_SYNCED     3
#define DP_PSR_SINK_ACTIVE_RESYNC          4
#define DP_PSR_SINK_INTERNAL_ERROR         7
#define DP_PSR_SINK_STATE_MASK             0x07
	
	/* DP 1.2 Sideband message defines */
	/* peer device type - DP 1.2a Table 2-92 */
#define DP_PEER_DEVICE_NONE		0x0
#define DP_PEER_DEVICE_SOURCE_OR_SST	0x1
#define DP_PEER_DEVICE_MST_BRANCHING	0x2
#define DP_PEER_DEVICE_SST_SINK		0x3
#define DP_PEER_DEVICE_DP_LEGACY_CONV	0x4
	
	/* DP 1.2 MST sideband request names DP 1.2a Table 2-80 */
#define DP_LINK_ADDRESS			0x01
#define DP_CONNECTION_STATUS_NOTIFY	0x02
#define DP_ENUM_PATH_RESOURCES		0x10
#define DP_ALLOCATE_PAYLOAD		0x11
#define DP_QUERY_PAYLOAD		0x12
#define DP_RESOURCE_STATUS_NOTIFY	0x13
#define DP_CLEAR_PAYLOAD_ID_TABLE	0x14
#define DP_REMOTE_DPCD_READ		0x20
#define DP_REMOTE_DPCD_WRITE		0x21
#define DP_REMOTE_I2C_READ		0x22
#define DP_REMOTE_I2C_WRITE		0x23
#define DP_POWER_UP_PHY			0x24
#define DP_POWER_DOWN_PHY		0x25
#define DP_SINK_EVENT_NOTIFY		0x30
#define DP_QUERY_STREAM_ENC_STATUS	0x38
	
	/* DP 1.2 MST sideband nak reasons - table 2.84 */
#define DP_NAK_WRITE_FAILURE		0x01
#define DP_NAK_INVALID_READ		0x02
#define DP_NAK_CRC_FAILURE		0x03
#define DP_NAK_BAD_PARAM		0x04
#define DP_NAK_DEFER			0x05
#define DP_NAK_LINK_FAILURE		0x06
#define DP_NAK_NO_RESOURCES		0x07
#define DP_NAK_DPCD_FAIL		0x08
#define DP_NAK_I2C_NAK			0x09
#define DP_NAK_ALLOCATE_FAIL		0x0a
	
#define MODE_I2C_START	1
#define MODE_I2C_WRITE	2
#define MODE_I2C_READ	4
#define MODE_I2C_STOP	8



#endif

