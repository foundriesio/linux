// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*
* NOTE: Tab size is 8
*/
#ifndef __TCC_HDMI_V_2_0_CEC_ACCESS_H__
#define __TCC_HDMI_V_2_0_CEC_ACCESS_H__
int cec_dev_write(struct cec_device *cec_dev, unsigned int offset,
        unsigned int data);
int cec_dev_read(struct cec_device *cec_dev, unsigned int offset);
void cec_dev_write_mask(struct cec_device *cec_dev, unsigned int addr,
        unsigned char mask, unsigned char data);
unsigned int cec_dev_read_mask(struct cec_device *cec_dev,
        unsigned int addr, unsigned char mask);
int cec_dev_sel_write(struct cec_device *cec_dev, unsigned int data);
int cec_dev_sel_read(struct cec_device *cec_dev);

#endif				/* __HDMI_ACCESS_H__ */

