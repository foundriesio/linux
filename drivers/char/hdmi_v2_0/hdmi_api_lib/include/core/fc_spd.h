/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
 * Synopsys DesignWare HDMI driver
 */
#ifndef HALFRAMECOMPOSERSPD_H_
#define HALFRAMECOMPOSERSPD_H_

struct fc_spd_info {
	const u8 *vName;
	u8 vLength;
	const u8 *pName;
	u8 pLength;
	u8 code;
	u8 autoSend;
};

int fc_spd_config(struct hdmi_tx_dev *dev, struct fc_spd_info *spd_data);

#endif	/* HALFRAMECOMPOSERSPD_H_ */
