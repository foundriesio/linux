/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALFRAMECOMPOSERACP_H_
#define HALFRAMECOMPOSERACP_H_

void fc_acp_type(struct hdmi_tx_dev *dev, u8 type);

void fc_acp_type_dependent_fields(struct hdmi_tx_dev *dev, u8 * fields, u8 fieldsLength);

#endif	/* HALFRAMECOMPOSERACP_H_ */
