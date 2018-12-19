/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        extenddisplay.cpp
*  \brief       HDMI TX controller driver
*  \details
*  \version     1.0
*  \date        2014-2018
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not
limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS"and nothing contained in this source
code shall constitute any express or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a particular
purpose or non-infringement of any patent, copyright or other third party
intellectual property right. No warranty is made, express or implied, regarding
the information's accuracy, completeness, or performance.
In no event shall Telechips be liable for any claim, damages or other liability
arising from, out of or in connection with this source code or the use in the
source code.
This source code is provided subject to the terms of a Mutual Non-Disclosure
Agreement between Telechips and Company.
*/

#ifndef SRC_IDENTIFICATION_IDENTIFICATION_REG_H_
#define SRC_IDENTIFICATION_IDENTIFICATION_REG_H_

/*****************************************************************************
 *                                                                           *
 *                          Identification Registers                         *
 *                                                                           *
 *****************************************************************************/

//Design Identification Register
#define DESIGN_ID  0x00000000
#define DESIGN_ID_DESIGN_ID_MASK  0x000000FF //Design ID code fixed by Synopsys that Identifies the instantiated DWC_hdmi_tx controller

//Revision Identification Register
#define REVISION_ID  0x00000004
#define REVISION_ID_REVISION_ID_MASK  0x000000FF //Revision ID code fixed by Synopsys that Identifies the instantiated DWC_hdmi_tx controller

//Product Identification Register 0
#define PRODUCT_ID0  0x00000008
#define PRODUCT_ID0_PRODUCT_ID0_MASK  0x000000FF //This one byte fixed code Identifies Synopsys's product line ("A0h" for DWC_hdmi_tx products)

//Product Identification Register 1
#define PRODUCT_ID1  0x0000000C
#define PRODUCT_ID1_PRODUCT_ID1_TX_MASK  0x00000001 //This bit Identifies Synopsys's DWC_hdmi_tx Controller according to Synopsys product line
#define PRODUCT_ID1_PRODUCT_ID1_RX_MASK  0x00000002 //This bit Identifies Synopsys's DWC_hdmi_rx Controller according to Synopsys product line
#define PRODUCT_ID1_PRODUCT_ID1_HDCP_MASK  0x000000C0 //These bits identify a Synopsys's HDMI Controller with HDCP encryption according to Synopsys product line

//Configuration Identification Register 0
#define CONFIG0_ID  0x00000010
#define CONFIG0_ID_HDCP_MASK  0x00000001 //Indicates if HDCP is present
#define CONFIG0_ID_CEC_MASK  0x00000002 //Indicates if CEC is present
#define CONFIG0_ID_CSC_MASK  0x00000004 //Indicates if Color Space Conversion block is present
#define CONFIG0_ID_HDMI14_MASK  0x00000008 //Indicates if HDMI 1
#define CONFIG0_ID_AUDI2S_MASK  0x00000010 //Indicates if I2S interface is present
#define CONFIG0_ID_AUDSPDIF_MASK  0x00000020 //Indicates if the SPDIF audio interface is present
#define CONFIG0_ID_PREPEN_MASK  0x00000080 //Indicates if it is possible to use internal pixel repetition

//Configuration Identification Register 1
#define CONFIG1_ID  0x00000014
#define CONFIG1_ID_CONFAPB_MASK  0x00000002 //Indicates that configuration interface is APB interface
#define CONFIG1_ID_HDMI20_MASK  0x00000020 //Indicates if HDMI 2
#define CONFIG1_ID_HDCP22_MASK  0x00000040 //Indicates if HDCP 2

//Configuration Identification Register 2
#define CONFIG2_ID  0x00000018
#define CONFIG2_ID_PHYTYPE_MASK  0x000000FF //Indicates the type of PHY interface selected: 0x00: Legacy PHY (HDMI TX PHY) 0xF2: PHY GEN2 (HDMI 3D TX PHY) 0xE2: PHY GEN2 (HDMI 3D TX PHY) + HEAC PHY 0xC2: PHY MHL COMBO (MHL+HDMI 2

//Configuration Identification Register 3
#define CONFIG3_ID  0x0000001C
#define CONFIG3_ID_CONFGPAUD_MASK  0x00000001 //Indicates that the audio interface is Generic Parallel Audio (GPAUD)
#define CONFIG3_ID_CONFAHBAUDDMA_MASK  0x00000002 //Indicates that the audio interface is AHB AUD DMA

#endif /* SRC_IDENTIFICATION_IDENTIFICATION_REG_H_ */
