/****************************************************************************
proc_fs.h

Copyright (C) 2018 Telechips Inc.
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
