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
#include "../../include/identification/identification.h"
#include "../../include/identification/identification_reg.h"
#include "../../../include/hdmi_log.h"
#include "../../../include/hdmi_access.h"

u8 id_design(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read(dev, DESIGN_ID);
}

u8 id_revision(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read(dev, REVISION_ID);
}

u8 id_product_line(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read(dev, PRODUCT_ID0);
}

u8 id_product_type(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read(dev, PRODUCT_ID1);
}

int id_hdcp_support(struct hdmi_tx_dev *dev)
{
	if (hdmi_dev_read_mask(dev, PRODUCT_ID1, PRODUCT_ID1_PRODUCT_ID1_HDCP_MASK) == 3)
		return TRUE;
	else
		return FALSE;
}

int id_hdcp14_support(struct hdmi_tx_dev *dev)
{
	if (hdmi_dev_read_mask(dev, CONFIG0_ID, CONFIG0_ID_HDCP_MASK))
		return TRUE;
	else
		return FALSE;
}

int id_hdcp22_support(struct hdmi_tx_dev *dev)
{
	if (hdmi_dev_read_mask(dev, CONFIG1_ID, CONFIG1_ID_HDCP22_MASK))
		return TRUE;
	else
		return FALSE;
}

int id_phy(struct hdmi_tx_dev *dev)
{
	return hdmi_dev_read(dev, CONFIG2_ID);
}
