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
#ifndef __PROC_FS_H__
#define __PROC_FS_H__

void
proc_interface_init(struct hdmi_tx_dev *dev);

void
proc_interface_remove(struct hdmi_tx_dev *dev);

ssize_t
proc_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off_set);

ssize_t
proc_write(struct file *filp, const char __user *buffer, size_t cnt,
		loff_t *off_set);

int
proc_open(struct inode *inode, struct file *filp);

int
proc_close(struct inode *inode, struct file *filp);

ssize_t
proc_read_edid(struct file *filp, char __user *buf, size_t cnt, loff_t *off_set);

ssize_t
proc_write_bind(struct file *filp, const char __user *buffer, size_t cnt,
		loff_t *off_set);

ssize_t
proc_write_hdcp_keys(struct file *filp, const char __user *buffer, size_t cnt,
		loff_t *off_set);

ssize_t
proc_write_phy_config(struct file *filp, const char __user *buffer, size_t cnt,
		loff_t *off_set);

#endif /* __PROC_FS_H__ */
