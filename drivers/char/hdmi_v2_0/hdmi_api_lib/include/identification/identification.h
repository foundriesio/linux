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

#ifndef INCLUDE_IDENTIFICATION_IDENTIFICATION_H_
#define INCLUDE_IDENTIFICATION_IDENTIFICATION_H_

#include "../../../include/hdmi_includes.h"

#define PRODUCT_HDMI_TX  0xA0

/**
 * Read product design information
 * @param baseAddr base address of controller
 * @return the design number stored in the hardware
 */
u8 id_design(struct hdmi_tx_dev *dev);

/**
 * Read product revision information
 * @param baseAddr base address of controller
 * @return the revision number stored in the hardware
 */
u8 id_revision(struct hdmi_tx_dev *dev);

/**
 * Read product line information
 * @param baseAddr base address of controller
 * @return the product line stored in the hardware
 */
u8 id_product_line(struct hdmi_tx_dev *dev);

/**
 * Read product type information
 * @param baseAddr base address of controller
 * @return the product type stored in the hardware
 */
u8 id_product_type(struct hdmi_tx_dev *dev);

/**
 * Check if HDCP is instantiated in hardware
 * @param baseAddr base address of controller
 * @return TRUE if hardware supports HDCP encryption
 */
int id_hdcp_support(struct hdmi_tx_dev *dev);

int id_hdcp14_support(struct hdmi_tx_dev *dev);

int id_hdcp22_support(struct hdmi_tx_dev *dev);

int id_phy(struct hdmi_tx_dev *dev);

#endif /* INCLUDE_IDENTIFICATION_IDENTIFICATION_H_ */


