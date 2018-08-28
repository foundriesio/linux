/*
 * identification.c
 *
 *  Created on: Jan 28, 2015
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
