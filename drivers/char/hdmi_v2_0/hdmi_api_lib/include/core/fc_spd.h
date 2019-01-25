/****************************************************************************
fc_spd.h

Copyright (C) 2018 Telechips Inc.
****************************************************************************/
#ifndef HALFRAMECOMPOSERSPD_H_
#define HALFRAMECOMPOSERSPD_H_

typedef struct fc_spd_info {
	const u8 * vName;
	u8 vLength;
	const u8 * pName;
	u8 pLength;
	u8 code;
	u8 autoSend;
}fc_spd_info_t;

int fc_spd_config(struct hdmi_tx_dev *dev, fc_spd_info_t *spd_data);

#endif	/* HALFRAMECOMPOSERSPD_H_ */
