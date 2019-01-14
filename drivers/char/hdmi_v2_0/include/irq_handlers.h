/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
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
