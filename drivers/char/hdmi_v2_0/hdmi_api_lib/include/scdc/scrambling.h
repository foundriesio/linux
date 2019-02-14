/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef SCRAMBLING_H_
#define SCRAMBLING_H_

void scrambling(struct hdmi_tx_dev *dev, u8 enable);

void scrambling_Enable(struct hdmi_tx_dev *dev, u8 bit);

int scrambling_get_wait_time(void);

int is_scrambling_enabled(struct hdmi_tx_dev *dev);

#endif	/* SCRAMBLING_H_ */
