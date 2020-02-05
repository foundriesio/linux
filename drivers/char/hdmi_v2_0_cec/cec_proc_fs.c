/****************************************************************************
hdmi_cec

Copyright (C) 2018 Telechips Inc.

NOTE: Tab size is 8
****************************************************************************/
#include "include/hdmi_cec.h"
#include "include/hdmi_cec_misc.h"

#include "hdmi_cec_lib/cec.h"
#include"include/cec_proc_fs.h"

#define DEBUGFS_BUF_SIZE 4096


ssize_t proc_write_wake_up(struct file *filp, const char __user *buffer, size_t cnt,
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
                                if(dev->cec_wakeup_enable) {
                                        dev->clk_enable_count -= (dev->cec_wakeup_enable-1);
                                        dev->cec_wakeup_enable = 0;
                                }
				break;

			case 1:
                                if(!dev->cec_wakeup_enable) {
                                        dev->cec_wakeup_enable = 1;
                                }
				break;
			case 9:
				cec_check_wake_up_interrupt(dev);
				break;
        }

        return cnt;
}


static const struct file_operations proc_fops_wakeup = {
        .owner   = THIS_MODULE,
        .open    = cec_proc_open,
        .release = cec_proc_close,
        .write   = proc_write_wake_up,
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
		printk(KERN_ERR "[ERROR][HDMI_CEC] %s:Device is null\n", __func__);
		return;
	}

	dev->cec_proc_dir = proc_mkdir("hdmi_cec", NULL);
	if(dev->cec_proc_dir == NULL){
		printk(KERN_ERR "[ERROR][HDMI_CEC] %s:Could not create file system @ /proc/hdmi_cec\n",
			__func__);
	}

    dev->cec_proc_wakeup = proc_create_data("wakeup", S_IFREG | S_IRUGO | S_IWUGO,
                    dev->cec_proc_dir, &proc_fops_wakeup, dev);
    if(dev->cec_proc_wakeup == NULL){
            printk(KERN_ERR "[ERROR][HDMI_CEC] %s:Could not create file system @"
                            " /proc/hdmi_tx/wake_up\n", __func__);
    }

}

void hdmi_cec_proc_interface_remove(struct cec_device *dev)
{

	if(dev->cec_proc_wakeup != NULL)
		proc_remove(dev->cec_proc_wakeup);

	if(dev->cec_proc_dir != NULL)
		proc_remove(dev->cec_proc_dir);
}



