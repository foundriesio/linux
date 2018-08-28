/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        cec_proc_fs.c
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
#include "include/hdmi_cec.h"
#include "hdmi_cec_lib/cec.h"
#include"include/cec_proc_fs.h"

#define DEBUGFS_BUF_SIZE 4096


ssize_t proc_write_wake_up_test(struct file *filp, const char __user *buffer, size_t cnt,
                loff_t *off_set){
        int ret;
        unsigned int wake_up_test = 0;
        struct cec_device *dev = PDE_DATA(file_inode(filp));

        char *wakeup_test_buf = devm_kzalloc(dev->parent_dev, cnt+1, GFP_KERNEL);
        
        if (wakeup_test_buf == NULL)
                return -ENOMEM;

        ret = simple_write_to_buffer(wakeup_test_buf, cnt, off_set, buffer, cnt);
        if (ret != cnt) {
                devm_kfree(dev->parent_dev, wakeup_test_buf);
                return ret >= 0 ? -EIO : ret;
        }

        wakeup_test_buf[cnt] = '\0';
        ret = sscanf(wakeup_test_buf, "%u", &wake_up_test);
        devm_kfree(dev->parent_dev, wakeup_test_buf);
        if (ret < 0)
                return ret;
        
        switch(wake_up_test) {
			case 0: default:
				break;
				
			case 1:
                cec_CfgWakeupFlag(dev,1);
				break;
			case 9:
				cec_check_wake_up_interrupt(dev);
				break;
        }
        
        return cnt;
}


static const struct file_operations proc_fops_wakeup_test = {
        .owner   = THIS_MODULE,
        .open    = cec_proc_open,
        .release = cec_proc_close,
        .write   = proc_write_wake_up_test,
};


int cec_proc_open(struct inode *inode, struct file *filp){
	try_module_get(THIS_MODULE);
	return 0;
}

int cec_proc_close(struct inode *inode, struct file *filp){
	module_put(THIS_MODULE);
	return 0;
}

void hdmi_cec_proc_interface_init(struct cec_device *dev){
	if(dev == NULL){
		pr_err("%s:Device is null\n", __func__);
		return;
	}

	dev->cec_proc_dir = proc_mkdir("hdmi_cec", NULL);
	if(dev->cec_proc_dir == NULL){
		pr_err("%s:Could not create file system @ /proc/hdmi_cec\n",
			__func__);
	}

    dev->cec_proc_wakeup_test = proc_create_data("wakeup_test", S_IFREG | S_IRUGO | S_IWUGO,
                    dev->cec_proc_dir, &proc_fops_wakeup_test, dev);
    if(dev->cec_proc_wakeup_test == NULL){
            pr_err("%s:Could not create file system @"
                            " /proc/hdmi_tx/wake_up_test\n", __func__);
    }

}

void hdmi_cec_proc_interface_remove(struct cec_device *dev){

	if(dev->cec_proc_wakeup_test != NULL)
		proc_remove(dev->cec_proc_wakeup_test);

	if(dev->cec_proc_dir != NULL)
		proc_remove(dev->cec_proc_dir);
}



