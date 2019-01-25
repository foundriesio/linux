/****************************************************************************
csc.h

Copyright (C) 2018 Telechips Inc.
****************************************************************************/
#ifndef HALCOLORSPACECONVERTER_H_
#define HALCOLORSPACECONVERTER_H_

void csc_config(struct hdmi_tx_dev *dev, videoParams_t * params,
		unsigned interpolation, unsigned decimation, unsigned color_depth);

#endif	/* HALCOLORSPACECONVERTER_H_ */
