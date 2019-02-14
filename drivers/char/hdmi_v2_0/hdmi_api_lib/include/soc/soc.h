/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef __HDMI_SOC_H__
#define __HDMI_SOC_H__

int hdmi_get_soc_features(struct hdmi_tx_dev *dev, hdmi_soc_features *soc_features);
int hdmi_get_board_features(struct hdmi_tx_dev *dev, hdmi_board_features *board_features);

#endif	/* __HDMI_SOC_H__ */

