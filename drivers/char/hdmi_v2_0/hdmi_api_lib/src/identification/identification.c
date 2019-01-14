/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
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
