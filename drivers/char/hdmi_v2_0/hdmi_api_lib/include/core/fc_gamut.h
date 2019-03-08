/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALFRAMECOMPOSERGAMUT_H_
#define HALFRAMECOMPOSERGAMUT_H_

void fc_gamut_enable_tx(struct hdmi_tx_dev *dev, u8 enable);

void fc_gamut_config(struct hdmi_tx_dev *dev);

void fc_gamut_packet_config(struct hdmi_tx_dev *dev, const u8 * gbdContent, u8 length);

#endif	/* HALFRAMECOMPOSERGAMUT_H_ */
