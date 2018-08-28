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
#include <core/fc_acp.h>

void fc_acp_type(struct hdmi_tx_dev *dev, u8 type)
{
	LOG_TRACE1(type);
	hdmi_dev_write(dev, FC_ACP0, type);
}

void fc_acp_type_dependent_fields(struct hdmi_tx_dev *dev, u8 * fields, u8 fieldsLength)
{
	u8 c ;
	LOG_TRACE1(fields[0]);
	if (fieldsLength > (FC_ACP1 - FC_ACP16 + 1)) {
		fieldsLength = (FC_ACP1 - FC_ACP16 + 1);
		LOGGER(SNPS_WARN,"ACP Fields Truncated");
	}

	for (c = 0; c < fieldsLength; c++)
		hdmi_dev_write(dev, FC_ACP1 - c, fields[c]);
}
