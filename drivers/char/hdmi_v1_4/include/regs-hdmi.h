/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        regs-hdmi.h
*  \brief       HDMI HDMI controller driver
*  \details   
*               Important!
*               The default tab size of this source code is setted with 8.
*  \version     1.0
*  \date        2014-2019
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not 
limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS"and nothing contained in this source 
code shall constitute any express or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a  particular 
purpose or non-infringement of any patent, copyright or other third party 
intellectual property right. No warranty is made, express or implied, regarding 
the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability 
arising from, out of or in connection with this source code or the use in the 
source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure 
Agreement between Telechips and Company. 
*/

#ifndef __ASM_ARCH_REGS_HDMI_H__
#define __ASM_ARCH_REGS_HDMI_H__

#define HDMI_LINK_CLK_FREQ                      24000000
#define HDMI_PCLK_FREQ                          50000000

#define DDICFG_REG(x)                           (x)

//@{
/**
 * @name DDI_BUS HDMI Control register
 */
#define DDICFG_HDMICTRL				DDICFG_REG(0x10)
#define HDMICTRL_RESET_HDMI			(1<<0)
#define HDMICTRL_RESET_SPDIF			(1<<1)
#define HDMICTRL_RESET_TMDS			(1<<2)
#define HDMICTRL_HDMI_ENABLE			(1<<15)		//!! CLK EN ??

//@{
/**
 * @name DDI_BUS PWDN register
 */
#define DDICFG_PWDN				DDICFG_REG(0x00)
#define PWDN_HDMI				(1<<2)

//@{
/**
 * @name DDI_BUS SWRESET register
 */
#define DDICFG_SWRESET				DDICFG_REG(0x04)
#define SWRESET_HDMI				(1<<2)

//@{
/**
 * @name DDI_BUS HDMI_AES register
 */
#define DDICFG_HDMI_AES				DDICFG_REG(0x14)

//@{
/**
 * @name DDI_BUS HDMI_AES_DATA register
 */
#define DDICFG_HDMI_AES_DATA0			DDICFG_REG(0x18)
#define DDICFG_HDMI_AES_DATA1			DDICFG_REG(0x1C)

//@{
/**
 * @name DDI_BUS HDMI_AES_HW_x register
 */
#define DDICFG_HDMI_AES_HW0			DDICFG_REG(0x20)
#define DDICFG_HDMI_AES_HW1			DDICFG_REG(0x24)
#define DDICFG_HDMI_AES_HW2			DDICFG_REG(0x28)

//@{
/**
 * @name HDMI IRQ bit values
 */
#define HDMI_IRQ_PIN_POLAR_CTL			(7)
#define HDMI_IRQ_GLOBAL				(6)
#define HDMI_IRQ_I2S				(5)
#define HDMI_IRQ_CEC				(4)
#define HDMI_IRQ_HPD_PLUG			(3)
#define HDMI_IRQ_HPD_UNPLUG			(2)
#define HDMI_IRQ_SPDIF				(1)
#define HDMI_IRQ_HDCP				(0)
//@}

#define HDMIDP_HDMI_SSREG(x)        		(x)

//@{
/**
 * @name HDMI Subsystem registers
 */
#define HDMI_SS_INTC_CON            		HDMIDP_HDMI_SSREG(0x00)
#define HDMI_SS_INTC_FLAG         	 	HDMIDP_HDMI_SSREG(0x04)
#define HDMI_SS_AESKEY_VALID      	  	HDMIDP_HDMI_SSREG(0x08)
#define HDMI_SS_HPD                 		HDMIDP_HDMI_SSREG(0x0C)

#define HDMI_SS_INTC_CON1         	    	HDMIDP_HDMI_SSREG(0x10)
#define HDMI_SS_INTC_FLAG1         	    	HDMIDP_HDMI_SSREG(0x14)
#define HDMI_SS_PHY_STATUS_0       	    	HDMIDP_HDMI_SSREG(0x20)
#define HDMI_SS_PHY_STATUS_CMU     	        HDMIDP_HDMI_SSREG(0x24)
#define HDMI_SS_PHY_STATUS_PLL      	   	HDMIDP_HDMI_SSREG(0x28)
#define HDMI_SS_PHY_CON0 	        	HDMIDP_HDMI_SSREG(0x30)
#define HDMI_SS_HPD_CTRL 	        	HDMIDP_HDMI_SSREG(0x40)
#define HDMI_SS_HPD_ST 		        	HDMIDP_HDMI_SSREG(0x44)
#define HDMI_SS_HPD_TH_X 	        	HDMIDP_HDMI_SSREG(0x50)
//@}

#define HDMIDP_HDMIREG(x)                       (0x10000+(x))

//@{
/**
 * @name HDMI control registers
 */
#define HDMI_CON_0                  		HDMIDP_HDMIREG(0x0000)
#define HDMI_CON_1                  		HDMIDP_HDMIREG(0x0004)
#define HDMI_CON_2	                  	HDMIDP_HDMIREG(0x0008)
#define HDMI_STATUS                 		HDMIDP_HDMIREG(0x0010)
#define HDMI_PHY_STATUS             		HDMIDP_HDMIREG(0x0014)
#define HDMI_STATUS_EN              		HDMIDP_HDMIREG(0x0020)
#define HDMI_SHA1_REN0              		HDMIDP_HDMIREG(0x0024)
#define HDMI_SHA1_REN1              		HDMIDP_HDMIREG(0x0028)
#define HDMI_HPD                    		HDMIDP_HDMIREG(0x0030)
#define HDMI_MODE_SEL               		HDMIDP_HDMIREG(0x0040)
#define HDCP_ENC_EN                 		HDMIDP_HDMIREG(0x0044)
#define HDMI_HPD_GEN                		HDMIDP_HDMIREG(0x05c8)
//@}

//@{
/**
 * @name HDMI video config registers
 */
#define HDMI_VACT_END_MG            		HDMIDP_HDMIREG(0x008c)

#define HDMI_H_BLANK_0  			HDMIDP_HDMIREG(0x00a0)
#define HDMI_H_BLANK_1	        		HDMIDP_HDMIREG(0x00a4)

#define HDMI_V2_BLANK_0		        	HDMIDP_HDMIREG(0x00b0)
#define HDMI_V2_BLANK_1			        HDMIDP_HDMIREG(0x00b4)
#define HDMI_V1_BLANK_0 			HDMIDP_HDMIREG(0x00b8)
#define HDMI_V1_BLANK_1	        		HDMIDP_HDMIREG(0x00bc)

#define HDMI_V_LINE_0		        	HDMIDP_HDMIREG(0x00c0)
#define HDMI_V_LINE_1			        HDMIDP_HDMIREG(0x00c4)
#define HDMI_H_LINE_0		        	HDMIDP_HDMIREG(0x00c8)
#define HDMI_H_LINE_1			        HDMIDP_HDMIREG(0x00cc)

#define HDMI_HSYNC_POL		        	HDMIDP_HDMIREG(0x00e0)
#define HDMI_VSYNC_POL			        HDMIDP_HDMIREG(0x00e4)

#define HDMI_INT_PRO_MODE		        HDMIDP_HDMIREG(0x00e8)

#define HDMI_V_BLANK_F0_0	        	HDMIDP_HDMIREG(0x0110)
#define HDMI_V_BLANK_F0_1		        HDMIDP_HDMIREG(0x0114)
#define HDMI_V_BLANK_F1_0		        HDMIDP_HDMIREG(0x0118)
#define HDMI_V_BLANK_F1_1		        HDMIDP_HDMIREG(0x011c)

#define HDMI_H_SYNC_START_0	        	HDMIDP_HDMIREG(0x0120)
#define HDMI_H_SYNC_START_1		        HDMIDP_HDMIREG(0x0124)
#define HDMI_H_SYNC_END_0		        HDMIDP_HDMIREG(0x0128)
#define HDMI_H_SYNC_END_1		        HDMIDP_HDMIREG(0x012c)

#define HDMI_V_SYNC_LINE_BEF_2_0        	HDMIDP_HDMIREG(0x0130)
#define HDMI_V_SYNC_LINE_BEF_2_1        	HDMIDP_HDMIREG(0x0134)
#define HDMI_V_SYNC_LINE_BEF_1_0        	HDMIDP_HDMIREG(0x0138)
#define HDMI_V_SYNC_LINE_BEF_1_1        	HDMIDP_HDMIREG(0x013c)

#define HDMI_V_SYNC_LINE_AFT_2_0	        HDMIDP_HDMIREG(0x0140)
#define HDMI_V_SYNC_LINE_AFT_2_1	        HDMIDP_HDMIREG(0x0144)
#define HDMI_V_SYNC_LINE_AFT_1_0	        HDMIDP_HDMIREG(0x0148)
#define HDMI_V_SYNC_LINE_AFT_1_1        	HDMIDP_HDMIREG(0x014c)

#define HDMI_V_SYNC_LINE_AFT_PXL_2_0	        HDMIDP_HDMIREG(0x0150)
#define HDMI_V_SYNC_LINE_AFT_PXL_2_1	        HDMIDP_HDMIREG(0x0154)
#define HDMI_V_SYNC_LINE_AFT_PXL_1_0	        HDMIDP_HDMIREG(0x0158)
#define HDMI_V_SYNC_LINE_AFT_PXL_1_1    	HDMIDP_HDMIREG(0x015c)

#define HDMI_V_BLANK_F2_0		        HDMIDP_HDMIREG(0x0160)
#define HDMI_V_BLANK_F2_1		        HDMIDP_HDMIREG(0x0164)
#define HDMI_V_BLANK_F3_0		        HDMIDP_HDMIREG(0x0168)
#define HDMI_V_BLANK_F3_1		        HDMIDP_HDMIREG(0x016c)
#define HDMI_V_BLANK_F4_0	        	HDMIDP_HDMIREG(0x0170)
#define HDMI_V_BLANK_F4_1	        	HDMIDP_HDMIREG(0x0174)
#define HDMI_V_BLANK_F5_0		        HDMIDP_HDMIREG(0x0178)
#define HDMI_V_BLANK_F5_1		        HDMIDP_HDMIREG(0x017c)

#define HDMI_V_SYNC_LINE_AFT_3_0	        HDMIDP_HDMIREG(0x0180)
#define HDMI_V_SYNC_LINE_AFT_3_1        	HDMIDP_HDMIREG(0x0184)
#define HDMI_V_SYNC_LINE_AFT_4_0	        HDMIDP_HDMIREG(0x0188)
#define HDMI_V_SYNC_LINE_AFT_4_1        	HDMIDP_HDMIREG(0x018c)
#define HDMI_V_SYNC_LINE_AFT_5_0	        HDMIDP_HDMIREG(0x0190)
#define HDMI_V_SYNC_LINE_AFT_5_1	        HDMIDP_HDMIREG(0x0194)
#define HDMI_V_SYNC_LINE_AFT_6_0	        HDMIDP_HDMIREG(0x0198)
#define HDMI_V_SYNC_LINE_AFT_6_1	        HDMIDP_HDMIREG(0x019c)

#define HDMI_V_SYNC_LINE_AFT_PXL_3_0	        HDMIDP_HDMIREG(0x01a0)
#define HDMI_V_SYNC_LINE_AFT_PXL_3_1	        HDMIDP_HDMIREG(0x01a4)
#define HDMI_V_SYNC_LINE_AFT_PXL_4_0	        HDMIDP_HDMIREG(0x01a8)
#define HDMI_V_SYNC_LINE_AFT_PXL_4_1	        HDMIDP_HDMIREG(0x01ac)
#define HDMI_V_SYNC_LINE_AFT_PXL_5_0    	HDMIDP_HDMIREG(0x01b0)
#define HDMI_V_SYNC_LINE_AFT_PXL_5_1	        HDMIDP_HDMIREG(0x01b4)
#define HDMI_V_SYNC_LINE_AFT_PXL_6_0    	HDMIDP_HDMIREG(0x01b8)
#define HDMI_V_SYNC_LINE_AFT_PXL_6_1	        HDMIDP_HDMIREG(0x01bc)

#define HDMI_VACT_SPACE1_0      		HDMIDP_HDMIREG(0x01c0)
#define HDMI_VACT_SPACE1_1	        	HDMIDP_HDMIREG(0x01c4)
#define HDMI_VACT_SPACE2_0		        HDMIDP_HDMIREG(0x01c8)
#define HDMI_VACT_SPACE2_1      		HDMIDP_HDMIREG(0x01cc)
#define HDMI_VACT_SPACE3_0	        	HDMIDP_HDMIREG(0x01d0)
#define HDMI_VACT_SPACE3_1		        HDMIDP_HDMIREG(0x01d4)
#define HDMI_VACT_SPACE4_0	        	HDMIDP_HDMIREG(0x01d8)
#define HDMI_VACT_SPACE4_1      	        HDMIDP_HDMIREG(0x01dc)
#define HDMI_VACT_SPACE5_0      		HDMIDP_HDMIREG(0x01e0)
#define HDMI_VACT_SPACE5_1	        	HDMIDP_HDMIREG(0x01e4)
#define HDMI_VACT_SPACE6_0		        HDMIDP_HDMIREG(0x01e8)
#define HDMI_VACT_SPACE6_1		        HDMIDP_HDMIREG(0x01ec)

#define HDMI_DC_CONTROL		        	HDMIDP_HDMIREG(0x0d00)
#define HDMI_VIDEO_PATTERN_GEN		        HDMIDP_HDMIREG(0x0d04)
//@}

//@{
/**
 * @name GCP registers
 */
#define HDMI_GCP_CON                		HDMIDP_HDMIREG(0x0200)
#define HDMI_GCP_CONEX				HDMIDP_HDMIREG(0x0204)
#define HDMI_GCP_BYTE1              		HDMIDP_HDMIREG(0x0210)
#define HDMI_GCP_BYTE2              		HDMIDP_HDMIREG(0x0214)
#define HDMI_GCP_BYTE3              		HDMIDP_HDMIREG(0x0218)
//@}


//@{
/**
 * HDMI audio sample packet register
 */
#define HDMI_ASP_CON                		HDMIDP_HDMIREG(0x0300)
#define HDMI_ASP_SP_FLAT			HDMIDP_HDMIREG(0x0304)
#define HDMI_ASP_CHCFG0 			HDMIDP_HDMIREG(0x0310)
#define HDMI_ASP_CHCFG1 			HDMIDP_HDMIREG(0x0314)
#define HDMI_ASP_CHCFG2 			HDMIDP_HDMIREG(0x0318)
#define HDMI_ASP_CHCFG3 			HDMIDP_HDMIREG(0x031C)

//@}

//@{
/**
 * @name Audio Clock Recovery packet registers
 */
#define HDMI_ACR_CON				HDMIDP_HDMIREG(0x0400)
#define HDMI_ACR_MCTS0				HDMIDP_HDMIREG(0x0410)
#define HDMI_ACR_MCTS1				HDMIDP_HDMIREG(0x0414)
#define HDMI_ACR_MCTS2				HDMIDP_HDMIREG(0x0418)
#define HDMI_ACR_N0				HDMIDP_HDMIREG(0x0430)
#define HDMI_ACR_N1				HDMIDP_HDMIREG(0x0434)
#define HDMI_ACR_N2				HDMIDP_HDMIREG(0x0438)
//@}

//@{
/**
 * HDMI ACP packet
 */
#define HDMI_ACP_CON			        HDMIDP_HDMIREG(0x0500)
#define HDMI_ACP_TYPE		        	HDMIDP_HDMIREG(0x0514)
#define HDMI_ACP_DATA00			        HDMIDP_HDMIREG(0x0520)
#define HDMI_ACP_DATA01 			HDMIDP_HDMIREG(0x0524)
#define HDMI_ACP_DATA02	        		HDMIDP_HDMIREG(0x0528)
#define HDMI_ACP_DATA03		        	HDMIDP_HDMIREG(0x052c)
#define HDMI_ACP_DATA04	        		HDMIDP_HDMIREG(0x0530)
#define HDMI_ACP_DATA05		        	HDMIDP_HDMIREG(0x0534)
#define HDMI_ACP_DATA06			        HDMIDP_HDMIREG(0x0538)
#define HDMI_ACP_DATA07		        	HDMIDP_HDMIREG(0x053c)
#define HDMI_ACP_DATA08			        HDMIDP_HDMIREG(0x0540)
#define HDMI_ACP_DATA09	        		HDMIDP_HDMIREG(0x0544)
#define HDMI_ACP_DATA10		        	HDMIDP_HDMIREG(0x0548)
#define HDMI_ACP_DATA11		        	HDMIDP_HDMIREG(0x054c)
#define HDMI_ACP_DATA12			        HDMIDP_HDMIREG(0x0550)
#define HDMI_ACP_DATA13	        		HDMIDP_HDMIREG(0x0554)
#define HDMI_ACP_DATA14		        	HDMIDP_HDMIREG(0x0558)
#define HDMI_ACP_DATA15			        HDMIDP_HDMIREG(0x055c)
#define HDMI_ACP_DATA16			        HDMIDP_HDMIREG(0x0560)
//@}


//@{
/**
 * HDMI ISRC packet
 */
#define HDMI_ISRC_CON   			HDMIDP_HDMIREG(0x0600)
#define HDMI_ISRC1_HEADER1      		HDMIDP_HDMIREG(0x0614)
#define HDMI_ISRC1_DATA00	        	HDMIDP_HDMIREG(0x0620)
#define HDMI_ISRC1_DATA01		        HDMIDP_HDMIREG(0x0624)
#define HDMI_ISRC1_DATA02		        HDMIDP_HDMIREG(0x0628)
#define HDMI_ISRC1_DATA03       		HDMIDP_HDMIREG(0x062c)
#define HDMI_ISRC1_DATA04	        	HDMIDP_HDMIREG(0x0630)
#define HDMI_ISRC1_DATA05		        HDMIDP_HDMIREG(0x0634)
#define HDMI_ISRC1_DATA06	        	HDMIDP_HDMIREG(0x0638)
#define HDMI_ISRC1_DATA07		        HDMIDP_HDMIREG(0x063c)
#define HDMI_ISRC1_DATA08	        	HDMIDP_HDMIREG(0x0640)
#define HDMI_ISRC1_DATA09		        HDMIDP_HDMIREG(0x0644)
#define HDMI_ISRC1_DATA10	        	HDMIDP_HDMIREG(0x0648)
#define HDMI_ISRC1_DATA11		        HDMIDP_HDMIREG(0x064c)
#define HDMI_ISRC1_DATA12	        	HDMIDP_HDMIREG(0x0650)
#define HDMI_ISRC1_DATA13		        HDMIDP_HDMIREG(0x0654)
#define HDMI_ISRC1_DATA14	        	HDMIDP_HDMIREG(0x0658)
#define HDMI_ISRC1_DATA15		        HDMIDP_HDMIREG(0x065c)
#define HDMI_ISRC2_DATA00       		HDMIDP_HDMIREG(0x06a0)
#define HDMI_ISRC2_DATA01	        	HDMIDP_HDMIREG(0x06a4)
#define HDMI_ISRC2_DATA02		        HDMIDP_HDMIREG(0x06a8)
#define HDMI_ISRC2_DATA03	        	HDMIDP_HDMIREG(0x06ac)
#define HDMI_ISRC2_DATA04		        HDMIDP_HDMIREG(0x06b0)
#define HDMI_ISRC2_DATA05	        	HDMIDP_HDMIREG(0x06b4)
#define HDMI_ISRC2_DATA06		        HDMIDP_HDMIREG(0x06b8)
#define HDMI_ISRC2_DATA07       		HDMIDP_HDMIREG(0x06bc)
#define HDMI_ISRC2_DATA08	        	HDMIDP_HDMIREG(0x06c0)
#define HDMI_ISRC2_DATA09		        HDMIDP_HDMIREG(0x06c4)
#define HDMI_ISRC2_DATA10       		HDMIDP_HDMIREG(0x06c8)
#define HDMI_ISRC2_DATA11	        	HDMIDP_HDMIREG(0x06cc)
#define HDMI_ISRC2_DATA12		        HDMIDP_HDMIREG(0x06d0)
#define HDMI_ISRC2_DATA13	        	HDMIDP_HDMIREG(0x06d4)
#define HDMI_ISRC2_DATA14		        HDMIDP_HDMIREG(0x06d8)
#define HDMI_ISRC2_DATA15       		HDMIDP_HDMIREG(0x06dc)
//@}



//@{
/**
 * @name AVI packet registers
 */
#define HDMI_AVI_CON                		HDMIDP_HDMIREG(0x0700)
#define HDMI_AVI_HEADER0            		HDMIDP_HDMIREG(0x0710)
#define HDMI_AVI_HEADER1            		HDMIDP_HDMIREG(0x0714)
#define HDMI_AVI_HEADER2            		HDMIDP_HDMIREG(0x0718)
#define HDMI_AVI_CHECK_SUM          		HDMIDP_HDMIREG(0x071c)
#define HDMI_AVI_BYTE1              		HDMIDP_HDMIREG(0x0720)
#define HDMI_AVI_BYTE2              		HDMIDP_HDMIREG(0x0724)
#define HDMI_AVI_BYTE3              		HDMIDP_HDMIREG(0x0728)
#define HDMI_AVI_BYTE4              		HDMIDP_HDMIREG(0x072c)
#define HDMI_AVI_BYTE5              		HDMIDP_HDMIREG(0x0730)
#define HDMI_AVI_BYTE6              		HDMIDP_HDMIREG(0x0734)
#define HDMI_AVI_BYTE7              		HDMIDP_HDMIREG(0x0738)
#define HDMI_AVI_BYTE8              		HDMIDP_HDMIREG(0x073c)
#define HDMI_AVI_BYTE9              		HDMIDP_HDMIREG(0x0740)
#define HDMI_AVI_BYTE10             		HDMIDP_HDMIREG(0x0744)
#define HDMI_AVI_BYTE11             		HDMIDP_HDMIREG(0x0748)
#define HDMI_AVI_BYTE12             		HDMIDP_HDMIREG(0x074c)
#define HDMI_AVI_BYTE13             		HDMIDP_HDMIREG(0x0750)
//@}

//@{
/**
 * @name AUI packet registers
 */
#define HDMI_AUI_CON                		HDMIDP_HDMIREG(0x0800)
#define HDMI_AUI_HEADER0            		HDMIDP_HDMIREG(0x0810)
#define HDMI_AUI_HEADER1            		HDMIDP_HDMIREG(0x0814)
#define HDMI_AUI_HEADER2            		HDMIDP_HDMIREG(0x0818)
#define HDMI_AUI_CHECK_SUM          		HDMIDP_HDMIREG(0x081c)
#define HDMI_AUI_BYTE1              		HDMIDP_HDMIREG(0x0820)
#define HDMI_AUI_BYTE2              		HDMIDP_HDMIREG(0x0824)
#define HDMI_AUI_BYTE3              		HDMIDP_HDMIREG(0x0828)
#define HDMI_AUI_BYTE4              		HDMIDP_HDMIREG(0x082c)
#define HDMI_AUI_BYTE5              		HDMIDP_HDMIREG(0x0830)
#define HDMI_AUI_BYTE6		    		HDMIDP_HDMIREG(0x0834)
#define HDMI_AUI_BYTE7		    		HDMIDP_HDMIREG(0x0838)
#define HDMI_AUI_BYTE8		    		HDMIDP_HDMIREG(0x083c)
#define HDMI_AUI_BYTE9		    		HDMIDP_HDMIREG(0x0840)
#define HDMI_AUI_BYTE10		    		HDMIDP_HDMIREG(0x0844)
#define HDMI_AUI_BYTE11		    		HDMIDP_HDMIREG(0x0848)
#define HDMI_AUI_BYTE12		    		HDMIDP_HDMIREG(0x084c)
//@}

//@{
/**
 * HDMI MPEG packet
 */
#define HDMI_MPG_CON    			HDMIDP_HDMIREG(0x0900)
#define HDMI_MPG_CHECK_SUM      		HDMIDP_HDMIREG(0x091c)
#define HDMI_MPG_BYTE1		        	HDMIDP_HDMIREG(0x0920)
#define HDMI_MPG_BYTE2	        		HDMIDP_HDMIREG(0x0924)
#define HDMI_MPG_BYTE3		        	HDMIDP_HDMIREG(0x0928)
#define HDMI_MPG_BYTE4			        HDMIDP_HDMIREG(0x092c)
#define HDMI_MPG_BYTE5		        	HDMIDP_HDMIREG(0x0930)
//@}

//@{
/**
 * @name SPD packet registers
 */
#define HDMI_SPD_CON                		HDMIDP_HDMIREG(0x0a00)
#define HDMI_SPD_HEADER0            		HDMIDP_HDMIREG(0x0a10)
#define HDMI_SPD_HEADER1            		HDMIDP_HDMIREG(0x0a14)
#define HDMI_SPD_HEADER2            		HDMIDP_HDMIREG(0x0a18)
#define HDMI_SPD_CHECK_SUM          		HDMIDP_HDMIREG(0x0a20) //HDMI_SPD_BYTE0
#define HDMI_SPD_DATA1				HDMIDP_HDMIREG(0x0a24)
#define HDMI_SPD_DATA2              		HDMIDP_HDMIREG(0x0a28)
#define HDMI_SPD_DATA3              		HDMIDP_HDMIREG(0x0a2c)
#define HDMI_SPD_DATA4             		HDMIDP_HDMIREG(0x0a30)
#define HDMI_SPD_DATA5              		HDMIDP_HDMIREG(0x0a34)
#define HDMI_SPD_DATA6              		HDMIDP_HDMIREG(0x0a38)
#define HDMI_SPD_DATA7              		HDMIDP_HDMIREG(0x0a3c)
#define HDMI_SPD_DATA08	        		HDMIDP_HDMIREG(0x0a40)
#define HDMI_SPD_DATA09		        	HDMIDP_HDMIREG(0x0a44)
#define HDMI_SPD_DATA10		        	HDMIDP_HDMIREG(0x0a48)
#define HDMI_SPD_DATA11	        		HDMIDP_HDMIREG(0x0a4c)
#define HDMI_SPD_DATA12		        	HDMIDP_HDMIREG(0x0a50)
#define HDMI_SPD_DATA13			        HDMIDP_HDMIREG(0x0a54)
#define HDMI_SPD_DATA14 			HDMIDP_HDMIREG(0x0a58)
#define HDMI_SPD_DATA15	        		HDMIDP_HDMIREG(0x0a5c)
#define HDMI_SPD_DATA16		        	HDMIDP_HDMIREG(0x0a60)
#define HDMI_SPD_DATA17			        HDMIDP_HDMIREG(0x0a64)
#define HDMI_SPD_DATA18	        		HDMIDP_HDMIREG(0x0a68)
#define HDMI_SPD_DATA19		        	HDMIDP_HDMIREG(0x0a6c)
#define HDMI_SPD_DATA20		        	HDMIDP_HDMIREG(0x0a70)
#define HDMI_SPD_DATA21		        	HDMIDP_HDMIREG(0x0a74)
#define HDMI_SPD_DATA22		        	HDMIDP_HDMIREG(0x0a78)
#define HDMI_SPD_DATA23	        		HDMIDP_HDMIREG(0x0a7c)
#define HDMI_SPD_DATA24		        	HDMIDP_HDMIREG(0x0a80)
#define HDMI_SPD_DATA25	        		HDMIDP_HDMIREG(0x0a84)
#define HDMI_SPD_DATA26	        		HDMIDP_HDMIREG(0x0a88)
#define HDMI_SPD_DATA27	        		HDMIDP_HDMIREG(0x0a8c)
//@}

//@{
/**
 * HDMI GAMUT packet
 */
#define HDMI_GAMUT_CON		        	HDMIDP_HDMIREG(0x0b00)
#define HDMI_GAMUT_HEADER0		        HDMIDP_HDMIREG(0x0b14)
#define HDMI_GAMUT_HEADER1      		HDMIDP_HDMIREG(0x0b18)
#define HDMI_GAMUT_HEADER2	        	HDMIDP_HDMIREG(0x0b1c)
#define HDMI_GAMUT_DATA00	        	HDMIDP_HDMIREG(0x0b20)
#define HDMI_GAMUT_DATA01		        HDMIDP_HDMIREG(0x0b24)
#define HDMI_GAMUT_DATA02	        	HDMIDP_HDMIREG(0x0b28)
#define HDMI_GAMUT_DATA03		        HDMIDP_HDMIREG(0x0b2c)
#define HDMI_GAMUT_DATA04       		HDMIDP_HDMIREG(0x0b30)
#define HDMI_GAMUT_DATA05	        	HDMIDP_HDMIREG(0x0b34)
#define HDMI_GAMUT_DATA06	        	HDMIDP_HDMIREG(0x0b38)
#define HDMI_GAMUT_DATA07	        	HDMIDP_HDMIREG(0x0b3c)
#define HDMI_GAMUT_DATA08	        	HDMIDP_HDMIREG(0x0b40)
#define HDMI_GAMUT_DATA09	        	HDMIDP_HDMIREG(0x0b44)
#define HDMI_GAMUT_DATA10	        	HDMIDP_HDMIREG(0x0b48)
#define HDMI_GAMUT_DATA11	        	HDMIDP_HDMIREG(0x0b4c)
#define HDMI_GAMUT_DATA12       		HDMIDP_HDMIREG(0x0b50)
#define HDMI_GAMUT_DATA13	        	HDMIDP_HDMIREG(0x0b54)
#define HDMI_GAMUT_DATA14	        	HDMIDP_HDMIREG(0x0b58)
#define HDMI_GAMUT_DATA15	        	HDMIDP_HDMIREG(0x0b5c)
#define HDMI_GAMUT_DATA16	        	HDMIDP_HDMIREG(0x0b60)
#define HDMI_GAMUT_DATA17	        	HDMIDP_HDMIREG(0x0b64)
#define HDMI_GAMUT_DATA18	        	HDMIDP_HDMIREG(0x0b68)
#define HDMI_GAMUT_DATA19	        	HDMIDP_HDMIREG(0x0b6c)
#define HDMI_GAMUT_DATA20	        	HDMIDP_HDMIREG(0x0b70)
#define HDMI_GAMUT_DATA21	        	HDMIDP_HDMIREG(0x0b74)
#define HDMI_GAMUT_DATA22	        	HDMIDP_HDMIREG(0x0b78)
#define HDMI_GAMUT_DATA23	        	HDMIDP_HDMIREG(0x0b7c)
#define HDMI_GAMUT_DATA24	        	HDMIDP_HDMIREG(0x0b80)
#define HDMI_GAMUT_DATA25	        	HDMIDP_HDMIREG(0x0b84)
#define HDMI_GAMUT_DATA26	        	HDMIDP_HDMIREG(0x0b88)
#define HDMI_GAMUT_DATA27	        	HDMIDP_HDMIREG(0x0b8c)
//@}

//@{
/**
 * HDMI VSI packet
 */
#define HDMI_VSI_CON	        		HDMIDP_HDMIREG(0x0c00)
#define HDMI_VSI_HEADER0	        	HDMIDP_HDMIREG(0x0c10)
#define HDMI_VSI_HEADER1	        	HDMIDP_HDMIREG(0x0c14)
#define HDMI_VSI_HEADER2	        	HDMIDP_HDMIREG(0x0c18)
#define HDMI_VSI_DATA00		        	HDMIDP_HDMIREG(0x0c20)
#define HDMI_VSI_DATA01		        	HDMIDP_HDMIREG(0x0c24)
#define HDMI_VSI_DATA02		        	HDMIDP_HDMIREG(0x0c28)
#define HDMI_VSI_DATA03		        	HDMIDP_HDMIREG(0x0c2c)
#define HDMI_VSI_DATA04		        	HDMIDP_HDMIREG(0x0c30)
#define HDMI_VSI_DATA05		        	HDMIDP_HDMIREG(0x0c34)
#define HDMI_VSI_DATA06		        	HDMIDP_HDMIREG(0x0c38)
#define HDMI_VSI_DATA07		        	HDMIDP_HDMIREG(0x0c3c)
#define HDMI_VSI_DATA08		        	HDMIDP_HDMIREG(0x0c40)
#define HDMI_VSI_DATA09		        	HDMIDP_HDMIREG(0x0c44)
#define HDMI_VSI_DATA10		        	HDMIDP_HDMIREG(0x0c48)
#define HDMI_VSI_DATA11		        	HDMIDP_HDMIREG(0x0c4c)
#define HDMI_VSI_DATA12		        	HDMIDP_HDMIREG(0x0c50)
#define HDMI_VSI_DATA13		        	HDMIDP_HDMIREG(0x0c54)
#define HDMI_VSI_DATA14		        	HDMIDP_HDMIREG(0x0c58)
#define HDMI_VSI_DATA15		        	HDMIDP_HDMIREG(0x0c5c)
#define HDMI_VSI_DATA16		        	HDMIDP_HDMIREG(0x0c60)
#define HDMI_VSI_DATA17		        	HDMIDP_HDMIREG(0x0c64)
#define HDMI_VSI_DATA18		        	HDMIDP_HDMIREG(0x0c68)
#define HDMI_VSI_DATA19		        	HDMIDP_HDMIREG(0x0c6c)
#define HDMI_VSI_DATA20		        	HDMIDP_HDMIREG(0x0c70)
#define HDMI_VSI_DATA21		        	HDMIDP_HDMIREG(0x0c74)
#define HDMI_VSI_DATA22		        	HDMIDP_HDMIREG(0x0c78)
#define HDMI_VSI_DATA23		        	HDMIDP_HDMIREG(0x0c7c)
#define HDMI_VSI_DATA24		        	HDMIDP_HDMIREG(0x0c80)
#define HDMI_VSI_DATA25		        	HDMIDP_HDMIREG(0x0c84)
#define HDMI_VSI_DATA26		        	HDMIDP_HDMIREG(0x0c88)
#define HDMI_VSI_DATA27		        	HDMIDP_HDMIREG(0x0c8c)
//@}

//@{
/**
 * @name HDCP config registers
 */
#define HDCP_AN_SEED_SEL			HDMIDP_HDMIREG(0x0E48)
#define HDCP_AN_SEED_0  			HDMIDP_HDMIREG(0x0E58)
#define HDCP_AN_SEED_1	        		HDMIDP_HDMIREG(0x0E5C)
#define HDCP_AN_SEED_2		        	HDMIDP_HDMIREG(0x0E60)
#define HDCP_AN_SEED_3		        	HDMIDP_HDMIREG(0x0E64)

#define HDCP_SHA1_00    			HDMIDP_HDMIREG(0x7000)
#define HDCP_SHA1_01	        		HDMIDP_HDMIREG(0x7004)
#define HDCP_SHA1_02		        	HDMIDP_HDMIREG(0x7008)
#define HDCP_SHA1_03    			HDMIDP_HDMIREG(0x700c)
#define HDCP_SHA1_04	        		HDMIDP_HDMIREG(0x7010)
#define HDCP_SHA1_05		        	HDMIDP_HDMIREG(0x7014)
#define HDCP_SHA1_06	        		HDMIDP_HDMIREG(0x7018)
#define HDCP_SHA1_07		        	HDMIDP_HDMIREG(0x701c)
#define HDCP_SHA1_08	        		HDMIDP_HDMIREG(0x7020)
#define HDCP_SHA1_09		        	HDMIDP_HDMIREG(0x7024)
#define HDCP_SHA1_10		        	HDMIDP_HDMIREG(0x7028)
#define HDCP_SHA1_11		        	HDMIDP_HDMIREG(0x702c)
#define HDCP_SHA1_12		        	HDMIDP_HDMIREG(0x7030)
#define HDCP_SHA1_13	        		HDMIDP_HDMIREG(0x7034)
#define HDCP_SHA1_14	        		HDMIDP_HDMIREG(0x7038)
#define HDCP_SHA1_15	        		HDMIDP_HDMIREG(0x703c)
#define HDCP_SHA1_16	        		HDMIDP_HDMIREG(0x7040)
#define HDCP_SHA1_17	        		HDMIDP_HDMIREG(0x7044)
#define HDCP_SHA1_18	        		HDMIDP_HDMIREG(0x7048)
#define HDCP_SHA1_19	        		HDMIDP_HDMIREG(0x704c)

#define HDCP_KSV_LIST_0 			HDMIDP_HDMIREG(0x7050)
#define HDCP_KSV_LIST_1	        		HDMIDP_HDMIREG(0x7054)
#define HDCP_KSV_LIST_2		        	HDMIDP_HDMIREG(0x7058)
#define HDCP_KSV_LIST_3			        HDMIDP_HDMIREG(0x705c)
#define HDCP_KSV_LIST_4		        	HDMIDP_HDMIREG(0x7060)
#define HDCP_KSV_LIST_CON		        HDMIDP_HDMIREG(0x7064)

#define HDCP_SHA_RESULT	        		HDMIDP_HDMIREG(0x7070)

#define HDCP_CTRL1		        	HDMIDP_HDMIREG(0x7080)
#define HDCP_CTRL2			        HDMIDP_HDMIREG(0x7084)

#define HDCP_CHECK_RESULT		        HDMIDP_HDMIREG(0x7090)

#define HDCP_BKSV_0		        	HDMIDP_HDMIREG(0x70a0)
#define HDCP_BKSV_1			        HDMIDP_HDMIREG(0x70a4)
#define HDCP_BKSV_2	        		HDMIDP_HDMIREG(0x70a8)
#define HDCP_BKSV_3		        	HDMIDP_HDMIREG(0x70ac)
#define HDCP_BKSV_4			        HDMIDP_HDMIREG(0x70b0)

#define HDCP_AKSV_0	        		HDMIDP_HDMIREG(0x70c0)
#define HDCP_AKSV_1		        	HDMIDP_HDMIREG(0x70c4)
#define HDCP_AKSV_2			        HDMIDP_HDMIREG(0x70c8)
#define HDCP_AKSV_3     			HDMIDP_HDMIREG(0x70cc)
#define HDCP_AKSV_4	        		HDMIDP_HDMIREG(0x70d0)

#define HDCP_AN_0       			HDMIDP_HDMIREG(0x70e0)
#define HDCP_AN_1	        		HDMIDP_HDMIREG(0x70e4)
#define HDCP_AN_2		        	HDMIDP_HDMIREG(0x70e8)
#define HDCP_AN_3	        		HDMIDP_HDMIREG(0x70ec)
#define HDCP_AN_4		        	HDMIDP_HDMIREG(0x70f0)
#define HDCP_AN_5	        		HDMIDP_HDMIREG(0x70f4)
#define HDCP_AN_6	        		HDMIDP_HDMIREG(0x70f8)
#define HDCP_AN_7	        		HDMIDP_HDMIREG(0x70fc)

#define HDCP_BCAPS		        	HDMIDP_HDMIREG(0x7100)

#define HDCP_BSTATUS_0	        		HDMIDP_HDMIREG(0x7110)
#define HDCP_BSTATUS_1		        	HDMIDP_HDMIREG(0x7114)

#define HDCP_RI_0	        		HDMIDP_HDMIREG(0x7140)
#define HDCP_RI_1		        	HDMIDP_HDMIREG(0x7144)

#define HDCP_OFFSET_TX_0			HDMIDP_HDMIREG(0x7160)
#define HDCP_OFFSET_TX_1			HDMIDP_HDMIREG(0x7164)
#define HDCP_OFFSET_TX_2			HDMIDP_HDMIREG(0x7168)
#define HDCP_OFFSET_TX_3			HDMIDP_HDMIREG(0x716C)

#define HDCP_CYCLE_AA				HDMIDP_HDMIREG(0x7170)

#define HDCP_I2C_INT    			HDMIDP_HDMIREG(0x7180)
#define HDCP_AN_INT	        		HDMIDP_HDMIREG(0x7190)
#define HDCP_WDT_INT		        	HDMIDP_HDMIREG(0x71a0)
#define HDCP_RI_INT			        HDMIDP_HDMIREG(0x71b0)

#define HDCP_RI_COMPARE_0       		HDMIDP_HDMIREG(0x71d0)
#define HDCP_RI_COMPARE_1	        	HDMIDP_HDMIREG(0x71d4)
#define HDCP_FRAME_COUNT		        HDMIDP_HDMIREG(0x71e0)
//@}

#define HDMI_IP_VER                             HDMIDP_HDMIREG(0xD000)

#define HDMI_RGB_ROUND_EN               	HDMIDP_HDMIREG(0xD500)
#define HDMI_VACT_SPACE_R_0	                HDMIDP_HDMIREG(0xD504)
#define HDMI_VACT_SPACE_R_1             	HDMIDP_HDMIREG(0xD508)
#define HDMI_VACT_SPACE_G_0     	        HDMIDP_HDMIREG(0xD50C)
#define HDMI_VACT_SPACE_G_1             	HDMIDP_HDMIREG(0xD510)
#define HDMI_VACT_SPACE_B_0     	        HDMIDP_HDMIREG(0xD514)
#define HDMI_VACT_SPACE_B_1              	HDMIDP_HDMIREG(0xD518)
#define HDMI_BLUE_SCREEN_R_0	                HDMIDP_HDMIREG(0xD520)
#define HDMI_BLUE_SCREEN_R_1                 	HDMIDP_HDMIREG(0xD524)
#define HDMI_BLUE_SCREEN_G_0	                HDMIDP_HDMIREG(0xD528)
#define HDMI_BLUE_SCREEN_G_1            	HDMIDP_HDMIREG(0xD52C)
#define HDMI_BLUE_SCREEN_B_0    	        HDMIDP_HDMIREG(0xD530)
#define HDMI_BLUE_SCREEN_B_1	                HDMIDP_HDMIREG(0xD534)


//@{
/**
 * @name CSC control registers
 */
//@{
#define HDMI_HSYNC_TEST_START			HDMIDP_HDMIREG(0xD100)
#define HDMI_HSYNC_TH				HDMIDP_HDMIREG(0xD104)
#define HDMI_HSYNC_LT_LIMIT			HDMIDP_HDMIREG(0xD110)
#define HDMI_HSYNC_WITDH_CUR			HDMIDP_HDMIREG(0xD120)
#define HDMI_HSYNC_WITDH_MAX			HDMIDP_HDMIREG(0xD124)
#define HDMI_HSYNC_WIDTH_MIN			HDMIDP_HDMIREG(0xD128)
#define HDMI_HSYNC_DI_END_MATCH			HDMIDP_HDMIREG(0xD130)
#define HDMI_HSYNC_DGUA_END_MATCH		HDMIDP_HDMIREG(0xD140)
//@}

#define HDMIDP_AESREG(x)        		(0x20000 + (x))

//@{
/**
 * @name AES config registers
 */
#define HDMI_SS_AES_START  	        	HDMIDP_AESREG(0x00)
#define HDMI_SS_AES_DATA       	        	HDMIDP_AESREG(0x40)
//@}

//@{
/**
 * @name AES bit values
 */
#define AES_START                           	1<<0
//@}

//@{
/**
 * @name HDCP IRQ bit values
 */
#define HDCP_I2C_INT_NUM            		(0)
#define HDCP_WATCHDOG_INT_NUM       		(1)
#define HDCP_AN_WRITE_INT_NUM       		(2)
#define HDCP_UPDATE_PJ_INT_NUM      		(3)
#define HDCP_UPDATE_RI_INT_NUM      		(4)
#define HDCP_AUD_FIFO_OVF_EN_NUM    		(5)
#define HDCP_AUTHEN_ACK_NUM         		(7)
//@}

//@{
/**
 * @name HDCP bit values
 */
#define HDCP_ENABLE                 		(1<<1)
#define HDCP_TIMEOUT                		(1<<2)
#define HDCP_REVOCATION_SET         		(1<<0)

#define HDCP_RI_NOT_MATCH           		(1<<1)
#define HDCP_RI_MATCH               		(1<<0|1<<1)

#define HDCP_SHA1_VALID_READY       		(1<<1)
#define HDCP_SHA1_VALID             		(1<<0)

#define HDCP_KSV_LIST_READ_DONE     		(1<<0)
#define HDCP_KSV_LIST_END           		(1<<1)
#define HDCP_KSV_LIST_EMPTY         		(1<<2)
#define HDCP_KSV_WRITE_DONE         		(1<<3)

#define HDCP_COMPARE_FRAME_COUNT0_ENABLE    	(1<<7 | 0)
#define HDCP_COMPARE_FRAME_COUNT1_ENABLE    	(1<<7 | 0x7F)
//@}

//@{
/**
 * @name HDMI bit values
 */
#define HPD_SW_ENABLE               		(1<<0)
#define HPD_SW_DISABLE              		(0)
#define HPD_ON                      		(1<<1)
#define HPD_OFF                     		(0)

#define HDMI_EXTERNAL_VIDEO         		(1<<1)
#define HDMI_INTERNAL_VIDEO			(1<<0)

#define DO_NOT_TRANSMIT             		(0)
#define TRANSMIT_ONCE               		(1)
#define TRANSMIT_EVERY_VSYNC        		(1<<1)
#define GCP_TRANSMIT_EVERY_VSYNC    		(1<<3|1<<2|1<<1)

#define HDMI_SYS_ENABLE             		(1<<0)
#define HDMI_PWDN_ENABLE            		(1<<1)
#define HDMI_ASP_ENABLE             		(1<<2)
#define HDMI_YCBCR422_ENABLE        		(1<<3)
#define HDMI_ENCODING_OPTION_ENABLE 		(1<<4)
#define HDMI_BLUE_SCR_ENABLE        		(1<<5)

#define HDMI_MODE_SEL_DVI           		(1<<0)
#define HDMI_MODE_SEL_HDMI          		(1<<1)

#define HDMICON1_DOUBLE_PIXEL_REPETITION    	(1<<0)

#define HDMICON1_LIMIT_MASK         		(1<<5|1<<6)
#define HDMICON1_RGB_LIMIT          		(1<<5)
#define HDMICON1_YCBCR_LIMIT        		(1<<6)

#define HDMICON2_DVI                		(1<<5|1<<1)
#define HDMICON2_HDMI               		(0)

#define ACR_MEASURED_CTS_MODE       		(1<<2)

#define AVI_HEADER                  		(0x82 + 0x02 + 0x0D)

#define AVI_HEADER_BYTE0			0x82
#define AVI_HEADER_BYTE1			0x2
#define AVI_HEADER_BYTE2			0xD

#define AVI_PACKET_BYTE_LENGTH      		(13)
#define AVI_CS_RGB                  		(0)
#define AVI_CS_Y422                 		(1<<5)
#define AVI_CS_Y444                 		(1<<6)
#define AVI_QUANTIZATION_MASK       		(1<<2|1<<3)
#define AVI_QUANTIZATION_DEFAULT    		(0)
#define AVI_QUANTIZATION_LIMITED    		(1<<2)
#define AVI_QUANTIZATION_FULL       		(1<<3)
#define AVI_PIXEL_REPETITION_DOUBLE 		(1<<0)
#define AVI_FORMAT_MASK             		(0x0F)
#define AVI_FORMAT_ASPECT_AS_PICTURE    	(1<<3)
#define AVI_PICTURE_ASPECT_RATIO_4_3    	(1<<4)
#define AVI_PICTURE_ASPECT_RATIO_16_9   	(1<<5)
#define AVI_COLORIMETRY_MASK			0xC0 // 0b11000000
#define AVI_COLORIMETRY_NO_DATA			0
#define AVI_COLORIMETRY_ITU601			0x40 // SMPTE170M, ITU601
#define AVI_COLORIMETRY_ITU709			0x80 // ITU709
#define AVI_COLORIMETRY_EXTENDED		0xC0 // Extended Colorimetry
#define AVI_COLORIMETRY_EXT_MASK		0x70 // 0b01110000
#define AVI_COLORIMETRY_EXT_xvYCC601		(0<<4)
#define AVI_COLORIMETRY_EXT_xvYCC709		(1<<4)

#define AUI_HEADER                  		(0x84 + 0x01 + 0x0A)

#define AUI_HEADER_BYTE0			0x84
#define AUI_HEADER_BYTE1			0x1
#define AUI_HEADER_BYTE2			0xA

#define AUI_PACKET_BYTE_LENGTH      		(10)
#define AUI_CC_2CH                  		(1)
#define AUI_CC_3CH                  		(2)
#define AUI_CC_4CH                  		(3)
#define AUI_CC_5CH                  		(4)
#define AUI_CC_6CH                  		(5)
#define AUI_CC_7CH                  		(6)
#define AUI_CC_8CH                  		(7)
#define HDMI_AUI_SF_MASK            		(1<<4|1<<3|1<<2)
#define HDMI_AUI_SF_SF_32KHZ        		(1<<2)
#define HDMI_AUI_SF_SF_44KHZ        		(1<<3)
#define HDMI_AUI_SF_SF_88KHZ        		(1<<4)
#define HDMI_AUI_SF_SF_176KHZ       		(1<<4|1<<3)
#define HDMI_AUI_SF_SF_48KHZ        		(1<<3|1<<2)
#define HDMI_AUI_SF_SF_96KHZ        		(1<<4|1<<2)
#define HDMI_AUI_SF_SF_192KHZ       		(1<<4|1<<3|1<<2)

#define SPD_PACKET_TYPE				(0x81)
#define SPD_PACKET_VERSION			(0x01)
#define SPD_PACKET_BYTE_LENGTH			(0x07)
#define SPD_HEADER                  		(SPD_PACKET_TYPE + SPD_PACKET_VERSION + SPD_PACKET_BYTE_LENGTH)
#define SPD_PACKET_ID0				(0x03)
#define SPD_PACKET_ID1				(0x0c)
#define SPD_PACKET_ID2				(0x00)
#define SPD_PACKET_ID_LENGTH			(0x03)
#define SPD_HDMI_VIDEO_FORMAT_NONE		(0x00)
#define SPD_HDMI_VIDEO_FORMAT_VIC		(0x01)
#define SPD_HDMI_VIDEO_FORMAT_3D		(0x02)

#define SPD_3D_STRUCT_FRAME_PACKING		(0x00)
#define SPD_3D_STRUCT_TOP_AND_BOTTOM		(0x06)
#define SPD_3D_STRUCT_SIDE_BY_SIDE		(0x08)

#define GCP_AVMUTE_ON               		(1<<0)
#define GCP_AVMUTE_OFF              		(1<<4)
#define GCP_CD_NOT_INDICATED        		(0)
#define GCP_CD_24BPP                		(1<<2)
#define GCP_CD_30BPP                		(1<<0|1<<2)
#define GCP_CD_36BPP                		(1<<1|1<<2)

#define HDMI_DC_CTL_8               		(0)
#define HDMI_DC_CTL_10              		(1<<0)
#define HDMI_DC_CTL_12              		(1<<1)

#define ASP_TYPE_MASK               		(1<<5|1<<6)
#define ASP_LPCM_TYPE               		(0)
#define ASP_DSD_TYPE               		(1<<5)
#define ASP_HBR_TYPE                		(1<<6)
#define ASP_DST_TYPE                		(1<<5|1<<6)
#define ASP_MODE_MASK               		(1<<4)
#define ASP_LAYOUT_0                		(0)
#define ASP_LAYOUT_1                		(1<<4)
#define ASP_SP_MASK                 		(0x0F)
#define ASP_SP_0                    		(1<<0)
#define ASP_SP_1                    		(1<<1)
#define ASP_SP_2                    		(1<<2)
#define ASP_SP_3                    		(1<<3)
#define ASP_FLAT_2CH				(1<<4)

#define HDMI_MODE_SEL_HDMI          		(1<<1)

#define DI_EVEN_PKT_ADJUST			(1<<0)

#define HDCP_EESS_START				(1<<0)
#define HDCP_EESS_STOP				(0)

#define HDMI_OLD_IP_VER		                (1<<7)
#define HDMI_NEW_IP_VER                 	(0)

//@}

#define HDMIDP_SPDIFREG(x)        		(0x30000 + (x))
//@{
/**
 * @name SPDIF registers
 */
#define HDMI_SS_SPDIF_CLK_CTRL          	HDMIDP_SPDIFREG(0x00)
#define HDMI_SS_SPDIF_OP_CTRL           	HDMIDP_SPDIFREG(0x04)
#define HDMI_SS_SPDIF_IRQ_MASK          	HDMIDP_SPDIFREG(0x08)
#define HDMI_SS_SPDIF_IRQ_STATUS        	HDMIDP_SPDIFREG(0x0C)
#define HDMI_SS_SPDIF_CONFIG_1          	HDMIDP_SPDIFREG(0x10)
#define HDMI_SS_SPDIF_CONFIG_2          	HDMIDP_SPDIFREG(0x14)

#define HDMI_SS_SPDIF_USER_VALUE_1      	HDMIDP_SPDIFREG(0x20)
#define HDMI_SS_SPDIF_USER_VALUE_2      	HDMIDP_SPDIFREG(0x24)
#define HDMI_SS_SPDIF_USER_VALUE_3      	HDMIDP_SPDIFREG(0x28)
#define HDMI_SS_SPDIF_USER_VALUE_4      	HDMIDP_SPDIFREG(0x2C)
#define HDMI_SS_SPDIF_CH_STATUS_0_1     	HDMIDP_SPDIFREG(0x30)
#define HDMI_SS_SPDIF_CH_STATUS_0_2     	HDMIDP_SPDIFREG(0x34)
#define HDMI_SS_SPDIF_CH_STATUS_0_3     	HDMIDP_SPDIFREG(0x38)
#define HDMI_SS_SPDIF_CH_STATUS_0_4     	HDMIDP_SPDIFREG(0x3C)
#define HDMI_SS_SPDIF_CH_STATUS_1       	HDMIDP_SPDIFREG(0x40)

#define HDMI_SS_SPDIF_FRAME_PERIOD_1    	HDMIDP_SPDIFREG(0x48)
#define HDMI_SS_SPDIF_FRAME_PERIOD_2    	HDMIDP_SPDIFREG(0x4C)
#define HDMI_SS_SPDIF_PC_INFO_1         	HDMIDP_SPDIFREG(0x50)
#define HDMI_SS_SPDIF_PC_INFO_2         	HDMIDP_SPDIFREG(0x54)
#define HDMI_SS_SPDIF_PD_INFO_1         	HDMIDP_SPDIFREG(0x58)
#define HDMI_SS_SPDIF_PD_INFO_2         	HDMIDP_SPDIFREG(0x5C)
#define HDMI_SS_SPDIF_DATA_BUF_0_1      	HDMIDP_SPDIFREG(0x60)
#define HDMI_SS_SPDIF_DATA_BUF_0_2      	HDMIDP_SPDIFREG(0x64)
#define HDMI_SS_SPDIF_DATA_BUF_0_3      	HDMIDP_SPDIFREG(0x68)
#define HDMI_SS_SPDIF_USER_BUF_0        	HDMIDP_SPDIFREG(0x6C)
#define HDMI_SS_SPDIF_DATA_BUF_1_1      	HDMIDP_SPDIFREG(0x70)
#define HDMI_SS_SPDIF_DATA_BUF_1_2      	HDMIDP_SPDIFREG(0x74)
#define HDMI_SS_SPDIF_DATA_BUF_1_3      	HDMIDP_SPDIFREG(0x78)
#define HDMI_SS_SPDIF_USER_BUF_1        	HDMIDP_SPDIFREG(0x7C)
//@}

//@{
/**
 * @name SPDIF bit values
 */
#define SPDIF_CLK_CTRL_ENABLE			(1)
#define SPDIF_CLK_CTRL_DISABLE			(0)

#define SPDIF_IRQ_ENABLE_ALL			(0xFF)
#define SPDIF_IRQ_DISABLE_ALL			(0)

#define SPDIF_CH_STATUS_0_1_LPCM		(0)
#define SPDIF_CH_STATUS_0_1_NPCM		(1<<1)

#define SPDIF_USER_VALUE_1_24BIT        	(0xB)

#define SPDIF_CONFIG_1_NOISE_FILTER_2_SAMPLES   (1<<6)
#define SPDIF_CONFIG_1_NOISE_FILTER_3_SAMPLES   (0)
#define SPDIF_CONFIG_1_LPCM                     (0)
#define SPDIF_CONFIG_1_NPCM                     (1<<5)
#define SPDIF_CONFIG_1_PCPD_MANUAL              (1<<4)
#define SPDIF_CONFIG_1_WORD_LENGTH_MANUAL       (1<<3)
#define SPDIF_CONFIG_1_UVCP_ENABLE              (1<<2)
#define SPDIF_CONFIG_1_HDMI_1_BURST             (0)
#define SPDIF_CONFIG_1_HDMI_2_BURST             (1<<1)
#define SPDIF_CONFIG_1_ALIGN_16BIT              (0)
#define SPDIF_CONFIG_1_ALIGN_32BIT              (1<<0)

#define SPDIF_CLK_RECOVERY_FAIL_MASK            (1<<0)
#define SPDIF_STATUS_RECOVERED_MASK             (1<<1)
#define SPDIF_PREAMBLE_DETECTED_MASK            (1<<2)
#define SPDIF_HEADER_NOT_DETECTED_MASK          (1<<3)
#define SPDIF_HEADER_DETECTED_MASK              (1<<4)
#define SPDIF_PAPD_NOT_DETECTED_MASK            (1<<5)
#define SPDIF_ABNORMAL_PD_MASK                  (1<<6)
#define SPDIF_BUFFER_OVERFLOW_MASK              (1<<7)

#define SPDIF_SIGNAL_RESET                      (0)
#define SPDIF_SIGNAL_DETECT                     (1<<0)
#define SPDIF_RUNNING                           (1<<1 | 1<<0)
//@}


//@}
#define HDMIDP_I2SREG(x)        		(0x40000 + (x))
//@{
/**
 * @name I2S registers
 */
#define HDMI_SS_I2S_CLK_CON         		HDMIDP_I2SREG(0x000)
#define HDMI_SS_I2S_CON_1           		HDMIDP_I2SREG(0x004)
#define HDMI_SS_I2S_CON_2           		HDMIDP_I2SREG(0x008)
#define HDMI_SS_I2S_PIN_SEL_0       		HDMIDP_I2SREG(0x00C)
#define HDMI_SS_I2S_PIN_SEL_1       		HDMIDP_I2SREG(0x010)
#define HDMI_SS_I2S_PIN_SEL_2       		HDMIDP_I2SREG(0x014)
#define HDMI_SS_I2S_PIN_SEL_3       		HDMIDP_I2SREG(0x018)
#define HDMI_SS_I2S_DSD_CON         		HDMIDP_I2SREG(0x01C)

#define HDMI_SS_I2S_IN_MUX_CON      		HDMIDP_I2SREG(0x020)
#define HDMI_SS_I2S_CH_ST_CON       		HDMIDP_I2SREG(0x024)
#define HDMI_SS_I2S_CH_ST_0         		HDMIDP_I2SREG(0x028)
#define HDMI_SS_I2S_CH_ST_1         		HDMIDP_I2SREG(0x02C)
#define HDMI_SS_I2S_CH_ST_2         		HDMIDP_I2SREG(0x030)
#define HDMI_SS_I2S_CH_ST_3         		HDMIDP_I2SREG(0x034)
#define HDMI_SS_I2S_CH_ST_4         		HDMIDP_I2SREG(0x038)
#define HDMI_SS_I2S_CH_ST_SH_0      		HDMIDP_I2SREG(0x03C)
#define HDMI_SS_I2S_CH_ST_SH_1      		HDMIDP_I2SREG(0x040)
#define HDMI_SS_I2S_CH_ST_SH_2      		HDMIDP_I2SREG(0x044)
#define HDMI_SS_I2S_CH_ST_SH_3      		HDMIDP_I2SREG(0x048)
#define HDMI_SS_I2S_CH_ST_SH_4      		HDMIDP_I2SREG(0x04C)
#define HDMI_SS_I2S_MUX_CH          		HDMIDP_I2SREG(0x054)
#define HDMI_SS_I2S_MUX_CUV         		HDMIDP_I2SREG(0x058)
#define HDMI_SS_I2S_IRQ_MASK			HDMIDP_I2SREG(0x05C)
#define HDMI_SS_I2S_IRQ_STATUS			HDMIDP_I2SREG(0x060)


#define HDMI_SS_I2S_CH0_LEFT_0			HDMIDP_I2SREG(0x064)
#define HDMI_SS_I2S_CH0_LEFT_1			HDMIDP_I2SREG(0x068)
#define HDMI_SS_I2S_CH0_LEFT_2			HDMIDP_I2SREG(0x06C)
#define HDMI_SS_I2S_CH0_LEFT_3			HDMIDP_I2SREG(0x070)
#define HDMI_SS_I2S_CH0_RIGHT_0			HDMIDP_I2SREG(0x074)
#define HDMI_SS_I2S_CH0_RIGHT_1			HDMIDP_I2SREG(0x078)
#define HDMI_SS_I2S_CH0_RIGHT_2			HDMIDP_I2SREG(0x07C)
#define HDMI_SS_I2S_CH0_RIGHT_3			HDMIDP_I2SREG(0x080)

#define HDMI_SS_I2S_CH0_LEFT_0			HDMIDP_I2SREG(0x064)
#define HDMI_SS_I2S_CH0_LEFT_1			HDMIDP_I2SREG(0x068)
#define HDMI_SS_I2S_CH0_LEFT_2			HDMIDP_I2SREG(0x06C)
#define HDMI_SS_I2S_CH0_LEFT_3			HDMIDP_I2SREG(0x070)
#define HDMI_SS_I2S_CH0_RIGHT_0			HDMIDP_I2SREG(0x074)
#define HDMI_SS_I2S_CH0_RIGHT_1			HDMIDP_I2SREG(0x078)
#define HDMI_SS_I2S_CH0_RIGHT_2			HDMIDP_I2SREG(0x07C)
#define HDMI_SS_I2S_CH0_RIGHT_3			HDMIDP_I2SREG(0x080)

#define HDMI_SS_I2S_CH1_LEFT_0			HDMIDP_I2SREG(0x084)
#define HDMI_SS_I2S_CH1_LEFT_1			HDMIDP_I2SREG(0x088)
#define HDMI_SS_I2S_CH1_LEFT_2			HDMIDP_I2SREG(0x08C)
#define HDMI_SS_I2S_CH1_LEFT_3			HDMIDP_I2SREG(0x090)
#define HDMI_SS_I2S_CH1_RIGHT_0			HDMIDP_I2SREG(0x094)
#define HDMI_SS_I2S_CH1_RIGHT_1			HDMIDP_I2SREG(0x098)
#define HDMI_SS_I2S_CH1_RIGHT_2			HDMIDP_I2SREG(0x09C)
#define HDMI_SS_I2S_CH1_RIGHT_3			HDMIDP_I2SREG(0x0A0)

#define HDMI_SS_I2S_CH2_LEFT_0			HDMIDP_I2SREG(0x0A4)
#define HDMI_SS_I2S_CH2_LEFT_1			HDMIDP_I2SREG(0x0A8)
#define HDMI_SS_I2S_CH2_LEFT_2			HDMIDP_I2SREG(0x0AC)
#define HDMI_SS_I2S_CH2_LEFT_3			HDMIDP_I2SREG(0x0B0)
#define HDMI_SS_I2S_CH2_RIGHT_0			HDMIDP_I2SREG(0x0B4)
#define HDMI_SS_I2S_CH2_RIGHT_1			HDMIDP_I2SREG(0x0B8)
#define HDMI_SS_I2S_CH2_RIGHT_2			HDMIDP_I2SREG(0x0BC)
#define HDMI_SS_I2S_CH2_RIGHT_3			HDMIDP_I2SREG(0x0C0)

#define HDMI_SS_I2S_CH3_LEFT_0			HDMIDP_I2SREG(0x0C4)
#define HDMI_SS_I2S_CH3_LEFT_1			HDMIDP_I2SREG(0x0C8)
#define HDMI_SS_I2S_CH3_LEFT_2			HDMIDP_I2SREG(0x0CC)
#define HDMI_SS_I2S_CH3_RIGHT_0			HDMIDP_I2SREG(0x0D0)
#define HDMI_SS_I2S_CH3_RIGHT_1			HDMIDP_I2SREG(0x0D4)
#define HDMI_SS_I2S_CH3_RIGHT_2			HDMIDP_I2SREG(0x0D8)

//@{
/**
 * @name I2S bit values
 */
#define I2S_PIN_SEL_AUDIO_0             	(0)
#define I2S_PIN_SEL_AUDIO_1             	(1)
#define I2S_PIN_SEL_AUDIO_2             	(2)
#define I2S_PIN_SEL_AUDIO_3             	(3)
#define I2S_PIN_SEL_AUDIO_4             	(4)
#define I2S_PIN_SEL_AUDIO_5             	(5)
#define I2S_PIN_SEL_AUDIO_6             	(6)

#define I2S_CLK_CON_ENABLE              	(1)
#define I2S_CLK_CON_DISABLE             	(0)

#define I2S_CON_SC_POL_FALLING          	(0<<1)
#define I2S_CON_SC_POL_RISING           	(1<<1)

#define I2S_CON_CH_POL_LOW              	(0)
#define I2S_CON_CH_POL_HIGH             	(1)

#define I2S_CON_MSB                     	(0<<6)
#define I2S_CON_LSB                     	(1<<6)

#define I2S_CON_BIT_CH_32               	(0<<4)
#define I2S_CON_BIT_CH_48               	(1<<4)
#define I2S_CON_BIT_CH_64               	(2<<4)

#define I2S_CON_DATA_NUM_MASK           	(3<<2)
#define I2S_CON_DATA_NUM_16             	(1<<2)
#define I2S_CON_DATA_NUM_20             	(2<<2)
#define I2S_CON_DATA_NUM_24             	(3<<2)

#define I2S_CON_I2S_MODE_BASIC              	(0)
#define I2S_CON_I2S_MODE_LEFT_JUSTIFIED     	(2)
#define I2S_CON_I2S_MODE_RIGHT_JUSTIFIED    	(3)

#define I2S_DSD_CON_POL_RISING          	(1<<1)
#define I2S_DSD_CON_POL_FALLING         	(0<<1)

#define I2S_DSD_CON_ENABLE              	(1)
#define I2S_DSD_CON_DISABLE             	(0)

#define I2S_IN_MUX_IN_ENABLE            	(1<<4)
#define I2S_IN_MUX_SELECT_DSD           	(2<<2)
#define I2S_IN_MUX_SELECT_I2S           	(1<<2)
#define I2S_IN_MUX_SELECT_SPDIF         	(0)
#define I2S_IN_MUX_CUV_ENABLE           	(1<<1)
#define I2S_IN_MUX_ENABLE               	(1)

#define I2S_MUX_CH_0_LEFT_ENABLE        	(1<<0)
#define I2S_MUX_CH_0_RIGHT_ENABLE       	(1<<1)
#define I2S_MUX_CH_1_LEFT_ENABLE        	(1<<2)
#define I2S_MUX_CH_1_RIGHT_ENABLE       	(1<<3)
#define I2S_MUX_CH_2_LEFT_ENABLE        	(1<<4)
#define I2S_MUX_CH_2_RIGHT_ENABLE       	(1<<5)
#define I2S_MUX_CH_3_LEFT_ENABLE        	(1<<6)
#define I2S_MUX_CH_3_RIGHT_ENABLE       	(1<<7)
#define I2S_MUX_CH_ALL_ENABLE           	(0xFF)

#define I2S_MUX_CUV_LEFT_ENABLE         	(1<<0)
#define I2S_MUX_CUV_RIGHT_ENABLE        	(1<<1)

#define I2S_CH_ST_0_TYPE_MASK           	(1<<1)
#define I2S_CH_ST_0_TYPE_LPCM           	(0)
#define I2S_CH_ST_0_TYPE_NLPCM          	(1<<1)

#define I2S_CH_ST_0_COPY_MASK           	(1<<2)
#define I2S_CH_ST_0_NOT_COPYRIGHTED     	(1<<2)

#define I2S_CH_ST_2_CHANNEL_MASK        	(0xF0)
#define I2S_CH_ST_2_CH_UNDEFINED        	(0)
#define I2S_CH_ST_2_CH_01               	(0x1<<4)
#define I2S_CH_ST_2_CH_02               	(0x2<<4)
#define I2S_CH_ST_2_CH_03               	(0x3<<4)
#define I2S_CH_ST_2_CH_04               	(0x4<<4)
#define I2S_CH_ST_2_CH_05               	(0x5<<4)
#define I2S_CH_ST_2_CH_06               	(0x6<<4)
#define I2S_CH_ST_2_CH_07               	(0x7<<4)
#define I2S_CH_ST_2_CH_08               	(0x8<<4)
#define I2S_CH_ST_2_CH_09               	(0x9<<4)
#define I2S_CH_ST_2_CH_10               	(0xA<<4)
#define I2S_CH_ST_2_CH_11               	(0xB<<4)
#define I2S_CH_ST_2_CH_12               	(0xC<<4)
#define I2S_CH_ST_2_CH_13               	(0xD<<4)
#define I2S_CH_ST_2_CH_14               	(0xE<<4)
#define I2S_CH_ST_2_CH_15               	(0xF<<4)

#define I2S_CH_ST_3_SF_MASK             	(0x0F)
#define I2S_CH_ST_3_SF_44KHZ            	(0)
#define I2S_CH_ST_3_SF_88KHZ            	(1<<3)
#define I2S_CH_ST_3_SF_176KHZ           	(1<<3|1<<2)
#define I2S_CH_ST_3_SF_48KHZ            	(1<<1)
#define I2S_CH_ST_3_SF_96KHZ            	(1<<3|1<<1)
#define I2S_CH_ST_3_SF_192KHZ           	(1<<3|1<<2|1<<1)
#define I2S_CH_ST_3_SF_768KHZ			(1<<3|1<<0)
#define I2S_CH_ST_3_SF_32KHZ            	(1<<1|1<<0)


#define I2S_CH_ST_4_WL_MASK			(0x0F)
#define I2S_CH_ST_4_WL_20_NOT_DEFINED		(0)
#define I2S_CH_ST_4_WL_20_16			(1<<1)
#define I2S_CH_ST_4_WL_20_18			(1<<2)
#define I2S_CH_ST_4_WL_20_19			(1<<3)
#define I2S_CH_ST_4_WL_20_20			(1<<3|1<<1)
#define I2S_CH_ST_4_WL_20_17			(1<<3|1<<2)
#define I2S_CH_ST_4_WL_24_NOT_DEFINED		(1<<0)
#define I2S_CH_ST_4_WL_24_20			(1<<1|1<<0)
#define I2S_CH_ST_4_WL_24_22			(1<<2|1<<0)
#define I2S_CH_ST_4_WL_24_23			(1<<3|1<<0)
#define I2S_CH_ST_4_WL_24_24			(1<<3|1<<1|1<<0)
#define I2S_CH_ST_4_WL_24_21			(1<<3|1<<2|1<<0)

#define I2S_CH_ST_UPDATE                	(1<<0)
//@}


//@}

#define HDMIDP_PHYREG(x)        		(0x60000 + (x))

//@{
/**
 * @name PHY registers
 */
//To Do Later


//@}


//@{
/**
 * @name PHY bit values
 */
//To Do Later


//@}

#endif /* __ASM_ARCH_REGS_HDMI_H__ */
