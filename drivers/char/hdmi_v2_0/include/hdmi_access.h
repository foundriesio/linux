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
