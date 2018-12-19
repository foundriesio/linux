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
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <include/hdmi_ioctls.h>
#include <core/fc_spd.h>
#include <core/fc_packets.h>

void fc_spd_VendorName(struct hdmi_tx_dev *dev, const u8 * data, unsigned short length)
{
	unsigned short i;
	LOG_TRACE();
	for (i = 0; i < length; i++) {
		hdmi_dev_write(dev, FC_SPDVENDORNAME0 + (i*4), data[i]);
	}
}

void fc_spd_ProductName(struct hdmi_tx_dev *dev, const u8 * data, unsigned short length)
{
	unsigned short i;
	LOG_TRACE();
	for (i = 0; i < length; i++) {
		hdmi_dev_write(dev, FC_SPDPRODUCTNAME0 + (i*4), data[i]);
	}
}

void fc_spd_SourceDeviceInfo(struct hdmi_tx_dev *dev, u8 code)
{
	LOG_TRACE1(code);
	hdmi_dev_write(dev, FC_SPDDEVICEINF, code);
}

int fc_spd_config(struct hdmi_tx_dev *dev, fc_spd_info_t *spd_data)
{

        const unsigned short vSize = 8;
	const unsigned short pSize = 16;

	LOG_TRACE();

	if(spd_data == NULL){
		LOGGER(SNPS_ERROR, "Improper argument: spd_data");
		return FALSE;
	}

	fc_packets_AutoSend(dev, 0, SPD_TX);	/* prevent sending half the info. */

	if (spd_data->vName == NULL) {
		LOGGER(SNPS_ERROR,"invalid parameter");
		return FALSE;
	}
	if (spd_data->vLength > vSize) {
		spd_data->vLength = vSize;
		LOGGER(SNPS_WARN,"vendor name truncated");
	}
	if (spd_data->pName == NULL) {
		LOGGER(SNPS_ERROR,"invalid parameter");
		return FALSE;
	}
	if (spd_data->pLength > pSize) {
		spd_data->pLength = pSize;
		LOGGER(SNPS_WARN,"product name truncated");
	}

	fc_spd_VendorName(dev, spd_data->vName, spd_data->vLength);
	fc_spd_ProductName(dev, spd_data->pName, spd_data->pLength);

	fc_spd_SourceDeviceInfo(dev, spd_data->code);

	if (spd_data->autoSend) {
		fc_packets_AutoSend(dev, spd_data->autoSend, SPD_TX);
	} else {
		fc_packets_ManualSend(dev, SPD_TX);
	}

	return TRUE;
}
