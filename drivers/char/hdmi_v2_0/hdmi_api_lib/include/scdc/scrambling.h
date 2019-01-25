/****************************************************************************
scrambling.h

Copyright (C) 2018 Telechips Inc.
****************************************************************************/
#ifndef SCRAMBLING_H_
#define SCRAMBLING_H_

void scrambling(struct hdmi_tx_dev *dev, u8 enable);

void scrambling_Enable(struct hdmi_tx_dev *dev, u8 bit);

int scrambling_get_wait_time(void);

int is_scrambling_enabled(struct hdmi_tx_dev *dev);

#endif	/* SCRAMBLING_H_ */
