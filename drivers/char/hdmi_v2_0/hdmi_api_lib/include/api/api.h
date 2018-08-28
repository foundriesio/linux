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
#ifndef __API_H__
#define __API_H__

/** @addtogroup init_api_grp Initialization API Routines
 *
 * These routines handle initialization of the CIL and PCD driver components
 * and the DWC_usb3 controller.
 */
/** @{ */

/**
 * Configure API.
 * Configure the modules of the API according to the parameters given by
 * the user. If EDID at sink is read, it does parameter checking using the 
 * Check methods against the sink's E-EDID. Violations are outputted to the 
 * buffer.
 * Shall only be called after an Init call or configure.
 * @return TRUE when successful
 * @note during this function, all controller's interrupts are disabled
 * @note this function needs to have the HW initialized before the first call
 */
int hdmi_api_Configure(struct hdmi_tx_dev *dev);


/**
 * AV Mute in the General Control Packet
 * @param enable TRUE set the AVMute in the general control packet
 */
void hdmi_api_avmute(struct hdmi_tx_dev *dev, int enable);

/** @} */

#endif	/* __API_H__*/

