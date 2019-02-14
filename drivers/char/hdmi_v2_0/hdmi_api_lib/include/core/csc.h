/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALCOLORSPACECONVERTER_H_
#define HALCOLORSPACECONVERTER_H_

void csc_config(struct hdmi_tx_dev *dev, videoParams_t * params,
		unsigned interpolation, unsigned decimation, unsigned color_depth);

#endif	/* HALCOLORSPACECONVERTER_H_ */
