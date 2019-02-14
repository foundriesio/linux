/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef __HDMI_ACCESS_H__
#define __HDMI_ACCESS_H__

#ifndef BIT
#define BIT(n)          (1 << n)
#endif

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern int hdmi_dev_write(struct hdmi_tx_dev *hdmi_dev, uint32_t offset, uint32_t data);
extern int hdmi_dev_read(struct hdmi_tx_dev *hdmi_dev, uint32_t offset);
extern void hdmi_dev_write_mask(struct hdmi_tx_dev *hdmi_dev, u32 addr, u8 mask, u8 data);
extern u32 hdmi_dev_read_mask(struct hdmi_tx_dev *hdmi_dev, u32 addr, u8 mask);
#endif				/* __HDMI_ACCESS_H__ */
