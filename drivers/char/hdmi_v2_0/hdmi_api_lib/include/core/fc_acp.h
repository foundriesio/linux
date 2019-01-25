/****************************************************************************
fc_acp.h

Copyright (C) 2018 Telechips Inc.
****************************************************************************/
#ifndef HALFRAMECOMPOSERACP_H_
#define HALFRAMECOMPOSERACP_H_

void fc_acp_type(struct hdmi_tx_dev *dev, u8 type);

void fc_acp_type_dependent_fields(struct hdmi_tx_dev *dev, u8 * fields, u8 fieldsLength);

#endif	/* HALFRAMECOMPOSERACP_H_ */
