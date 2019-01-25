/****************************************************************************
hdmi_cec

Copyright (C) 2018 Telechips Inc.
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
