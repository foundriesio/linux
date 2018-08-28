/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        cec_proc_fs.h
*  \brief       HDMI CEC controller driver
*  \details   
*  \version     1.0
*  \date        2014-2015
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written  permission  of Telechips including not 
limited to re-distribution in source  or binary  form  is strictly prohibited.
This source  code is  provided "AS IS"and nothing contained in this source 
code  shall  constitute any express  or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a   particular 
purpose or non-infringement  of  any  patent,  copyright  or  other third party 
intellectual property right. No warranty is made, express or implied, regarding 
the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability 
arising from, out of or in connection with this source  code or the  use in the 
source code. 
This source code is provided subject  to the  terms of a Mutual  Non-Disclosure 
Agreement between Telechips and Company.
*******************************************************************************/


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
