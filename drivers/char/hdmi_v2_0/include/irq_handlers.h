// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef __IRQ_HANDLERS_H__
#define __IRQ_HANDLERS_H__

int
dwc_init_interrupts(struct hdmi_tx_dev *dev);
int
dwc_deinit_interrupts(struct hdmi_tx_dev *dev);
void
dwc_hdmi_enable_interrupt(struct hdmi_tx_dev *dev);
void
dwc_hdmi_disable_interrupt(struct hdmi_tx_dev *dev);
void
dwc_hdmi_tx_set_hotplug_interrupt(struct hdmi_tx_dev *dev, int enable);
#endif
