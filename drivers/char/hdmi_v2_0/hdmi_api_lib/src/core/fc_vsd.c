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
#include <core/fc_vsd.h>

void fc_vsd_vendor_OUI(struct hdmi_tx_dev *dev, u32 id)
{
	LOG_TRACE1(id);
	hdmi_dev_write(dev, (FC_VSDIEEEID0), id);
	hdmi_dev_write(dev, (FC_VSDIEEEID1), id >> 8);
	hdmi_dev_write(dev, (FC_VSDIEEEID2), id >> 16);
}

u8 fc_vsd_vendor_payload(struct hdmi_tx_dev *dev, const u8 * data, unsigned short length)
{
	const unsigned short size = 24;
	unsigned i;
	LOG_TRACE();
	if (data == NULL) {
		LOGGER(SNPS_WARN,"invalid parameter");
		return 1;
	}
	if (length > size) {
		length = size;
		LOGGER(SNPS_WARN,"vendor payload truncated");
	}
	for (i = 0; i < length; i++) {
		hdmi_dev_write(dev, (FC_VSDPAYLOAD0 + (i*4)), data[i]);
	}
	// VSD sise IEEE(3-bit) + length
	hdmi_dev_write(dev, FC_VSDSIZE, length + 3);
	return 0;
}

