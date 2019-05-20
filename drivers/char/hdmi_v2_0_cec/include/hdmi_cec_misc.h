/****************************************************************************
Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA

NOTE: Tab size is 8
****************************************************************************/

#ifndef __HDMI_CEC_MISC_H__
#define __HDMI_CEC_MISC_H__
extern int hdmi_cec_request_irq(struct cec_device *dev);
extern void cec_power_on(struct cec_device *dev);
extern void cec_power_off(struct cec_device *dev);
extern int hdmi_cec_misc_register(struct cec_device *dev);
extern int hdmi_cec_misc_deregister(struct cec_device *dev);
#endif

