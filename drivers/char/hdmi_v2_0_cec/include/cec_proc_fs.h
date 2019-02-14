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
****************************************************************************/
#ifndef __CEC_PROC_FS_H__
#define __CEC_PROC_FS_H__

void hdmi_cec_proc_interface_init(struct cec_device *dev);

void hdmi_cec_proc_interface_remove(struct cec_device *dev);
#if 0
ssize_t
proc_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off_set);

ssize_t
proc_write(struct file *filp, const char __user *buffer, size_t cnt,
		loff_t *off_set);
#endif
int cec_proc_open(struct inode *inode, struct file *filp);

int cec_proc_close(struct inode *inode, struct file *filp);
#endif /* __CEC_PROC_FS_H__ */
