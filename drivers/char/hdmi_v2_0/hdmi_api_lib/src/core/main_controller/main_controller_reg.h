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
#ifndef SRC_CORE_MAIN_CONTROLLER_MAIN_CONTROLLER_REG_H_
#define SRC_CORE_MAIN_CONTROLLER_MAIN_CONTROLLER_REG_H_

/*****************************************************************************
 *                                                                           *
 *                         Main Controller Registers                         *
 *                                                                           *
 *****************************************************************************/

//Main Controller Synchronous Clock Domain Disable Register
#define MC_CLKDIS  0x00010004
#define MC_CLKDIS_PIXELCLK_DISABLE_MASK  0x00000001 //Pixel clock synchronous disable signal
#define MC_CLKDIS_TMDSCLK_DISABLE_MASK  0x00000002 //TMDS clock synchronous disable signal
#define MC_CLKDIS_PREPCLK_DISABLE_MASK  0x00000004 //Pixel Repetition clock synchronous disable signal
#define MC_CLKDIS_AUDCLK_DISABLE_MASK  0x00000008 //Audio Sampler clock synchronous disable signal
#define MC_CLKDIS_CSCCLK_DISABLE_MASK  0x00000010 //Color Space Converter clock synchronous disable signal
#define MC_CLKDIS_CECCLK_DISABLE_MASK  0x00000020 //CEC Engine clock synchronous disable signal
#define MC_CLKDIS_HDCPCLK_DISABLE_MASK  0x00000040 //HDCP clock synchronous disable signal

//Main Controller Software Reset Register Main controller software reset request per clock domain
#define MC_SWRSTZREQ  0x00010008
#define MC_SWRSTZREQ_PIXELSWRST_REQ_MASK  0x00000001 //Pixel software reset request
#define MC_SWRSTZREQ_TMDSSWRST_REQ_MASK  0x00000002 //TMDS software reset request
#define MC_SWRSTZREQ_PREPSWRST_REQ_MASK  0x00000004 //Pixel Repetition software reset request
#define MC_SWRSTZREQ_II2SSWRST_REQ_MASK  0x00000008 //I2S audio software reset request
#define MC_SWRSTZREQ_ISPDIFSWRST_REQ_MASK  0x00000010 //SPDIF audio software reset request
#define MC_SWRSTZREQ_CECSWRST_REQ_MASK  0x00000040 //CEC software reset request
#define MC_SWRSTZREQ_IGPASWRST_REQ_MASK  0x00000080 //GPAUD interface soft reset request

//Main Controller HDCP Bypass Control Register
#define MC_OPCTRL  0x0001000C
#define MC_OPCTRL_HDCP_BLOCK_BYP_MASK  0x00000001 //Block HDCP bypass mechanism - 1'b0: This is the default value

//Main Controller Feed Through Control Register
#define MC_FLOWCTRL  0x00010010
#define MC_FLOWCTRL_FEED_THROUGH_OFF_MASK  0x00000001 //Video path Feed Through enable bit: - 1b: Color Space Converter is in the video data path

//Main Controller PHY Reset Register
#define MC_PHYRSTZ  0x00010014
#define MC_PHYRSTZ_PHYRSTZ_MASK  0x00000001 //HDMI Source PHY active low reset control for PHY GEN1, active high reset control for PHY GEN2

//Main Controller Clock Present Register
#define MC_LOCKONCLOCK  0x00010018
#define MC_LOCKONCLOCK_CECCLK_MASK  0x00000001 //CEC clock status
#define MC_LOCKONCLOCK_AUDIOSPDIFCLK_MASK  0x00000004 //SPDIF clock status
#define MC_LOCKONCLOCK_I2SCLK_MASK  0x00000008 //I2S clock status
#define MC_LOCKONCLOCK_PREPCLK_MASK  0x00000010 //Pixel Repetition clock status
#define MC_LOCKONCLOCK_TCLK_MASK  0x00000020 //TMDS clock status
#define MC_LOCKONCLOCK_PCLK_MASK  0x00000040 //Pixel clock status
#define MC_LOCKONCLOCK_IGPACLK_MASK  0x00000080 //GPAUD interface clock status

#endif /* SRC_CORE_MAIN_CONTROLLER_MAIN_CONTROLLER_REG_H_ */
