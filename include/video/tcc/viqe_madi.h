/*
 * linux/include/video/tcc/viqe_madi.h
 * Author:  <linux@telechips.com>
 * Created: Jan 20, 2018
 * Description: TCC MADI h/w block 
 *
 * Copyright (C) 2018 Telechips
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
#ifndef __VIQE_MADI_H__
#define	__VIQE_MADI_H__

#define USE_UNDESCRIPTION_REGS  // no description on datasheet
//#define USE_REG_EXTRACTOR // not register setting mode but extraction mode....
//#define TEST_REG_RW

/*
 * Block Start Address
 */

typedef enum{
	VMADI_CTRL_IF = 0,	//0x12600000		// DDEI_Control_IF 		// (0x0 ~ )
	VMADI_CAP_IF,		//0x12602000		// DDE_Data_Capture_IF 	// (0x0 ~ )
	VMADI_DEINT,		//0x12604000		// cpu_clk_vdeint 			// (0x0 ~ 0x96C )
	VMADI_DEINT_LUT,	//0x12606000		// cpu_clk_vdeint 			// (0x19640000 ~ )
	VMADI_TIMMING,		//0x12608000  	// ??					// ( ~ )
	VMADI_MAX
}VMADI_TYPE;

typedef enum{
	MADI_OFF = 0,
	MADI_READY,
	MADI_ON
}MadiEn_Mode;

typedef enum{
	MADI_CAP_4 = 0, // Src-Y
	MADI_CAP_5, 	// Src-C
	MADI_CAP_6, 	// Src-Y
	MADI_CAP_7,
	MADI_CAP_8,
	MADI_CAP_9, 	// Src-C
	MADI_CAP_MAX
}MadiCAP_Type;

typedef enum{
	MADI_ADDR_0 = 0,
	MADI_ADDR_1,
	MADI_ADDR_2,
	MADI_ADDR_3,
	MADI_ADDR_MAX
}MadiADDR_Type;

typedef enum{
	MADI_VDEINT_0 = 0,
	MADI_VDEINT_4,		// Src-Y
	MADI_VDEINT_5,		// Src-C
	MADI_VDEINT_6,		// Src-Y
	MADI_VDEINT_7,		// Target-A
	MADI_VDEINT_78,		// Target-A
	MADI_VDEINT_9,		// Target-Y and C
	MADI_VDEINT_A,		// Target-A
	MADI_VDEINT_MAX
}MadiVDEINT_Type;

#define COMMON_ADDR_SHIFT		(4)
#define COMMON_ADDR_MASK		(0x0FFFFFFF)

#define COMMON_LINE_BUFF_LEVEL_SHIFT		(16)
#define COMMON_ROW_BYTES_SHIFT				(0)
#define COMMON_LINE_NUM_SHIFT				(0)
#define COMMON_LINE_TOP_SHIFT				(0)
#define COMMON_LINE_BOTTOM_SHIFT			(16)
#define COMMON_SIZE_WIDTH_SHIFT				(0)
#define COMMON_SIZE_HEIGHT_SHIFT			(16)
#define COMMON_MSB_16_SHIFT					(16)
#define COMMON_LSB_16_SHIFT					(0)

#define COMMON_LINE_BUFF_LEVEL_MASK			(0x0FFF  << COMMON_LINE_BUFF_LEVEL_SHIFT)
#define COMMON_ROWBYTES_MASK				(0xFFFF  << COMMON_ROW_BYTES_SHIFT)
#define COMMON_LINE_NUM_MASK				(0xFFFF  << COMMON_LINE_NUM_SHIFT)
#define COMMON_LINE_BOTTOM_TOP_MASK			( (0xFFFF  << COMMON_LINE_TOP_SHIFT) | (0xFFFF  << COMMON_LINE_BOTTOM_SHIFT))
#define COMMON_SIZE_MASK					( (0xFFFF  << COMMON_SIZE_WIDTH_SHIFT) | (0xFFFF  << COMMON_SIZE_HEIGHT_SHIFT))
#define COMMON_MSB_16_MASK					(0xFFFF  << COMMON_MSB_16_SHIFT)
#define COMMON_LSB_16_MASK					(0xFFFF  << COMMON_LSB_16_SHIFT)

/***************************************************************************************************************/
/*											MADI CONTROL IF		//0x12600000								   */
/***************************************************************************************************************/

/*
 * register offset
 */

#define MADICTRL_OFFSET			0x00

/*
 * Control-I/F Registers offset
 */

#define MADICTRL_SW_RESET_DEINT_SHIFT		(31)	// sw reset for DEINT
#define MADICTRL_SW_RESET_DCAP_SHIFT		(30)	// sw reset for DCAP
#define MADICTRL_SW_RESET_NODE_19_SHIFT		(23)	// sw reset for NODE.19
#define MADICTRL_PWDN_NODE_19_SHIFT			(22)	// power down for NODE.19
#define MADICTRL_THRESHOLD_NODE_19_SHIFT	(16)	// threshold for NODE.19
#define MADICTRL_SW_RESET_NODE_18_SHIFT		(15)	// sw reset for NODE.18
#define MADICTRL_PWDN_NODE_18_SHIFT			(14)	// power down for NODE.18
#define MADICTRL_THRESHOLD_NODE_18_SHIFT	(8)		// threshold for NODE.18
#define MADICTRL_SW_RESET_NODE_16_SHIFT		(7)		// sw reset for NODE.16
#define MADICTRL_PWDN_NODE_16_SHIFT			(6)		// power down for NODE.16
#define MADICTRL_THRESHOLD_NODE_16_SHIFT	(0)		// threshold for NODE.16

#define MADICTRL_SW_RESET_DEINT_MASK		(0x1  << MADICTRL_SW_RESET_DEINT_SHIFT)
#define MADICTRL_SW_RESET_DCAP_MASK			(0x1  << MADICTRL_SW_RESET_DCAP_SHIFT)
#define MADICTRL_SW_RESET_NODE_19_MASK		(0x1  << MADICTRL_SW_RESET_NODE_19_SHIFT)
#define MADICTRL_PWDN_NODE_19_MASK			(0x1  << MADICTRL_PWDN_NODE_19_SHIFT)
#define MADICTRL_THRESHOLD_NODE_19_MASK		(0x1F << MADICTRL_THRESHOLD_NODE_19_SHIFT)
#define MADICTRL_SW_RESET_NODE_18_MASK		(0x1  << MADICTRL_SW_RESET_NODE_18_SHIFT)
#define MADICTRL_PWDN_NODE_18_MASK			(0x1  << MADICTRL_PWDN_NODE_18_SHIFT)
#define MADICTRL_THRESHOLD_NODE_18_MASK		(0x1F << MADICTRL_THRESHOLD_NODE_18_SHIFT)
#define MADICTRL_SW_RESET_NODE_16_MASK		(0x1  << MADICTRL_SW_RESET_NODE_16_SHIFT)
#define MADICTRL_PWDN_NODE_16_MASK			(0x1  << MADICTRL_PWDN_NODE_16_SHIFT)
#define MADICTRL_THRESHOLD_NODE_16_MASK		(0x1F << MADICTRL_THRESHOLD_NODE_16_SHIFT)


/***************************************************************************************************************/
/*									   MADI CAPTURE I/F		//0x12602000								       */
/***************************************************************************************************************/

/*
 * register offset
 */

// 0x0 ~ 0x8c :: To do...

#define M4_CAP_OFFSET (0x400) // Src-Y
#define M5_CAP_OFFSET (0x500) // Src-C
#define M6_CAP_OFFSET (0x600) // Src-Y
#define M7_CAP_OFFSET (0x700) // 
#define M8_CAP_OFFSET (0x800) // PCLK-Timming
#define M9_CAP_OFFSET (0x900) // Src-C

#define MADICAP_M0_YUV420_MODE_SHIFT		(1)
#define MADICAP_M0_YUV420_MODE_MASK			(0x1 << MADICAP_M0_YUV420_MODE_SHIFT)

#define MADICAP_M0_YUV420_MODE_OFFSET		0x44


// M4: 0x400
// M5: 0x500
// M6: 0x600
// M9: 0x900
#define MADICAP_LEFT_Y0_ADDR_OFFSET			0x00
#define MADICAP_LEFT_Y1_ADDR_OFFSET			0x04
#define MADICAP_LEFT_Y2_ADDR_OFFSET			0x08
#define MADICAP_LEFT_Y3_ADDR_OFFSET			0x0C
#define MADICAP_LEFT_Y4_ADDR_OFFSET			0x10
#define MADICAP_LEFT_Y5_ADDR_OFFSET			0x14
#define MADICAP_RIGHT_Y0_ADDR_OFFSET		0x18
#define MADICAP_RIGHT_Y1_ADDR_OFFSET		0x1C
#define MADICAP_RIGHT_Y2_ADDR_OFFSET		0x20
#define MADICAP_RIGHT_Y3_ADDR_OFFSET		0x24
#define MADICAP_RIGHT_Y4_ADDR_OFFSET		0x28
#define MADICAP_RIGHT_Y5_ADDR_OFFSET		0x2C

#define MADICAP_FRM_START_ADDR_OFFSET		0x30
#define MADICAP_LINE_BUFFER_LEVEL_OFFSET	0x34
#define MADICAP_LINE_CTRL_OFFSET			0x38

#define MADICAP_BUFFER_CTRL_OFFSET			0x40
#define MADICAP_DATA_SIZE_OFFSET			0x44
#define MADICAP_FRM_OPTION_OFFSET			0x48

#define MADICAP_3D_OPTION_OFFSET			0x68
#define MADICAP_CORE_CTRL_OFFSET			0x6C

#ifdef USE_UNDESCRIPTION_REGS
#define MADICAP_80_WIDTH_SIZE_OFFSET		0x80
#define MADICAP_80_HEIGHT_SIZE_OFFSET		0x84
#endif

// M7
#define MADICAP_M7_WIDTH_0C_OFFSET			0x0C
#define MADICAP_M7_HEIGHT_10_OFFSET			0x10

// M8
#define MADICAP_PCLK_00_OFFSET				0x00
#define MADICAP_PCLK_04_OFFSET				0x04
#define MADICAP_PCLK_10_OFFSET				0x10
#define MADICAP_PCLK_14_OFFSET				0x14
#define MADICAP_PCLK_18_OFFSET				0x18
#define MADICAP_PCLK_20_OFFSET				0x20

#define MADICAP_PCLK_VDE_OFFSET				0x28
#define MADICAP_PCLK_82C_OFFSET				0x2C
#define MADICAP_PCLK_VSTART_OFFSET			0x40 // need to set?
#define MADICAP_PCLK_HEIGHT_OFFSET			0x44
#define MADICAP_PCLK_HSTART_OFFSET			0x48 // need to set?
#define MADICAP_PCLK_WIDTH_OFFSET			0x4C
#define MADICAP_PCLK_LINE_LIMIT_OFFSET		0x50
#define MADICAP_PCLK_VS_DELAY_OFFSET		0x54 // need to set?
#define MADICAP_PCLK_HEIGHT_OUT_OFFSET		0x58
#define MADICAP_PCLK_WIDTH_OUT_OFFSET		0x5C
#define MADICAP_PCLK_868_OFFSET				0x68
	
#define MADICAP_M8_WIDTH_8C_OFFSET			0x8C
#define MADICAP_M8_HEIGHT_90_OFFSET			0x90
#define MADICAP_M8_WIDTH_98_OFFSET			0x98
#define MADICAP_M8_HEIGHT_9C_OFFSET			0x9C


	// ?? 0x88C/898	- out_width[31:16]
	// ?? 0x890/86C	- out_height[31:16]
////////////////////////////////////////////////////////////////////////////


// 0x828 ~ 0x860 :: To do...

// 0xA00 ~ 0xA3C :: To do...



/*
 * Control-I/F Registers offset
 */




/***************************************************************************************************************/
/*											MADI DEINT 	//0x12604000								           */
/***************************************************************************************************************/

/*
 * register offset
 */

#define M0_VDEINT_OFFSET 	(0x000)
#define M4_VDEINT_OFFSET 	(0x400) // Src-Y
#define M5_VDEINT_OFFSET 	(0x480) // Src-C
#define M6_VDEINT_OFFSET 	(0x500) // Src-Y
#define M7_VDEINT_OFFSET 	(0x580) // Target-A
#define M78_VDEINT_OFFSET 	(0x600) // Target-A
#define M9_VDEINT_OFFSET 	(0x680) // Target-Y and C
#define MA_VDEINT_OFFSET 	(0x740) // Target-A

// 0x000 ~ 0x088 :: To do... ???
#define MADIVDEINT_PFU_VDE_START_OFFSET			0x38
#define MADIVDEINT_PSU_VDE_START_OFFSET			0x3C
#define MADIVDEINT_VSIZE_OFFSET					0x40
#define MADIVDEINT_HSIZE_OFFSET					0x4C

#define MADIVDEINT_M10_YUV_MODE_SHIFT			(0)
#define MADIVDEINT_M10_YUV_MODE_MASK			(0x1 << MADIVDEINT_M10_YUV_MODE_SHIFT)
#define MADIVDEINT_M10_YUV_MODE_OFFSET			0x54

#define MADIVDEINT_VSIZE_OUT_OFFSET				0x70
#define MADIVDEINT_HSIZE_OUT_OFFSET				0x74

#define MADIVDEINT_M11_YUV_MODE_SHIFT			(5)
#define MADIVDEINT_M11_YUV_MODE_MASK			(0x1 << MADIVDEINT_M11_YUV_MODE_SHIFT)
#define MADIVDEINT_M11_YUV_MODE_OFFSET			0x240

#define MADIVDEINT_MEAS_H_OFFSET				0x284
#define MADIVDEINT_MEAS_V_OFFSET				0x288

#define MADIVDEINT_MEAS_SHIFT					(0)
#define MADIVDEINT_MEAS_MASK					(0xFFF << MADIVDEINT_MEAS_SHIFT)


#define MADIVDEINT_MV_2EC_OFFSET				0x2EC
#define MADIVDEINT_MV_2F4_OFFSET				0x2F4
#define MADIVDEINT_WIN3_H_OFFSET				0x318
#define MADIVDEINT_WIN3_V_OFFSET				0x31C

#define MADIVDEINT_MV_H_SHIFT					(12)
#define MADIVDEINT_MV_H_MASK					(0xFFF << MADIVDEINT_MV_H_SHIFT)

#define MADIVDEINT_MV_W_SHIFT					(0)
#define MADIVDEINT_MV_W_MASK					(0xFFF << MADIVDEINT_MV_W_SHIFT)

#define MADIVDEINT_WIN3_SIZE_SHIFT				(0)
#define MADIVDEINT_WIN3_SIZE_MASK				(0xFFF << MADIVDEINT_WIN3_SIZE_SHIFT)


// 0x200 ~ 0x33c :: To do... dump???

// 0x400 ~ 0x47c :: data_yc - dump???
// 0x480 ~ 0x4fc :: data_uvc - dump???

// 0x500 ~ 0x57c :: data_ypp - dump???
// 0x580 ~ 0x5fc :: data_ap - dump???

// 0x600 ~ 0x67c :: data_app - dump???
// 0x680 ~ 0x73c :: y and c out - dump???

// 0x740 ~ 0x7bc :: a out - dump???

// 0x800 ~ 0x8bc :: - dump???
// 0x940 ~ 0x96c :: - dump???


//M4 ~ MA
#define MADIVDEINT_DATA_SIZE_OFFSET				0x24

#define MADIVDEINT_Y0_ST_ADDR_OFFSET			0x40
#define MADIVDEINT_Y1_ST_ADDR_OFFSET			0x44
#define MADIVDEINT_Y2_ST_ADDR_OFFSET			0x48
#define MADIVDEINT_Y3_ST_ADDR_OFFSET			0x4C
#define MADIVDEINT_Y4_ST_ADDR_OFFSET			0x50
#define MADIVDEINT_Y5_ST_ADDR_OFFSET			0x54

#define MADIVDEINT_LINE_BUFFER_LEVEL_OFFSET		0x74
#define MADIVDEINT_LINE_CTRL_OFFSET				0x78
#define MADIVDEINT_TOP_BOTTOM_OFFSET			0x7C

#define MADIVDEINT_OUT_C0_ST_ADDR_OFFSET		0x80
#define MADIVDEINT_OUT_C1_ST_ADDR_OFFSET		0x84
#define MADIVDEINT_OUT_C2_ST_ADDR_OFFSET		0x88
#define MADIVDEINT_OUT_C3_ST_ADDR_OFFSET		0x8C
#define MADIVDEINT_OUT_C4_ST_ADDR_OFFSET		0x90
#define MADIVDEINT_OUT_C5_ST_ADDR_OFFSET		0x94

#define MADIVDEINT_OUT_C0_LINE_BUFFER_LEVEL_OFFSET		0xB4
#define MADIVDEINT_OUT_C0_LINE_CTRL_OFFSET				0xB8
#define MADIVDEINT_OUT_C0_BC_OFFSET						0xBC


/*
 * DEINT Registers offset
 */





/***************************************************************************************************************/
/*										MADI DEINT_LUT 	//0x12606000								           */
/***************************************************************************************************************/

/*
 * register offset
 */


// 0x000 ~ 0x0fc :: - dump


#define MADIDEINT_LUT_OFFSET			0x00

/*
 * DEINT_LUT Registers offset
 */





/***************************************************************************************************************/
/*									MADI TIMMING GENERATOR		//0x12608000								   */
/***************************************************************************************************************/

/*
 * register offset
 */

#define MADITIMMING_GEN_OFFSET			0x00

/*
 * Timming Generator Registers offset
 */

#define MADITIMMING_GEN_GO_REQ_OFFSET			0x00
#define MADITIMMING_GEN_GO_ACK_OFFSET			0x04
#define MADITIMMING_GEN_CFG_H_TOTAL_OFFSET		0x08
#define MADITIMMING_GEN_CFG_H_START_OFFSET		0x0C
#define MADITIMMING_GEN_CFG_H_END_OFFSET		0x10
#define MADITIMMING_GEN_CFG_V_TOTAL_OFFSET		0x14
#define MADITIMMING_GEN_CFG_V_START_OFFSET		0x18
#define MADITIMMING_GEN_CFG_V_END_OFFSET		0x1C
#define MADITIMMING_GEN_CFG_CODE_OFFSET			0x20
#define MADITIMMING_GEN_CFG_FIELD_OFFSET		0x24
#define MADITIMMING_GEN_CFG_FREEZE_OFFSET		0x28
#define MADITIMMING_GEN_CFG_CGLP_CTRL_OFFSET	0x2C
#define MADITIMMING_GEN_CFG_IREQ_EN_OFFSET		0x30
#define MADITIMMING_GEN_CFG_IREQ_CLR_OFFSET		0x34
#define MADITIMMING_GEN_CFG_STS_IREQ_RAW_OFFSET	0x38
#define MADITIMMING_GEN_CFG_STS_ACTIVE_OFFSET	0x40
#define MADITIMMING_GEN_CFG_STS_TG_OFFSET		0x80

#define MADI_INT_START 			(1<<0)
#define MADI_INT_ACTIVATED 		(1<<1)
#define MADI_INT_DEACTIVATED 	(1<<2)
#define MADI_INT_VDEINT 		(1<<8)
#define MADI_INT_VNR		 	(1<<16) // no connection in internal IP.
#define MADI_INT_ALL			(MADI_INT_START | MADI_INT_ACTIVATED | MADI_INT_DEACTIVATED | MADI_INT_VDEINT | MADI_INT_VNR)

/***************************************************************************************************************/
/*												Extern function												   */
/***************************************************************************************************************/

extern void VIQE_MADI_Ctrl_Enable(MadiEn_Mode mode);
extern void VIQE_MADI_Ctrl_Disable(void);
extern void VIQE_MADI_SetBasicConfiguration(unsigned int odd_first);
extern void VIQE_MADI_Set_SrcImgBase(MadiADDR_Type type, unsigned int YAddr, unsigned int CAddr);
extern void VIQE_MADI_Set_TargetImgBase(MadiADDR_Type type, unsigned int Alpha, unsigned int YAddr, unsigned int CAddr);
extern void VIQE_MADI_Get_TargetImgBase(MadiADDR_Type type, unsigned int *YAddr, unsigned int *CAddr);

extern void VIQE_MADI_Set_SrcImgSize(unsigned int width, unsigned int height, unsigned int src_yuv420,
			      unsigned int bpp, unsigned int crop_sx,
			      unsigned int crop_sy, unsigned int crop_w,
			      unsigned int crop_h);
extern void VIQE_MADI_Set_TargetImgSize(unsigned int src_width, unsigned int src_height, unsigned int out_width /*crop_w*/, unsigned int out_height/*crop_h*/, unsigned int bpp);
extern void VIQE_MADI_Gen_Timming(unsigned int out_width, unsigned int out_height);

extern void VIQE_MADI_Set_Clk_Gating(void);
extern void VIQE_MADI_Go_Request(void);
extern void VIQE_MADI_Set_odd_field_first(void);
extern void VIQE_MADI_Change_Cfg(void);

#ifdef TEST_REG_RW
extern void VIQE_MADI_Reg_RW_Test(void);
#endif

extern volatile void __iomem* VIQE_MADI_GetAddress(VMADI_TYPE type);
extern void VIQE_MADI_DUMP(VMADI_TYPE type, unsigned int size);

#ifdef USE_REG_EXTRACTOR
extern void _reg_print_ext(void);
#endif

#endif
