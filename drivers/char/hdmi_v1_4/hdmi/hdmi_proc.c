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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <asm/mach-types.h>
#include <linux/uaccess.h>


#include <asm/io.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <hdmi_1_4_audio.h>
#include <hdmi_1_4_video.h>
#include <hdmi_1_4_hdmi.h>
#include <hdmi_1_4_api.h>
#include <hdmi.h>

#define DEBUGFS_BUF_SIZE 4096


static ssize_t proc_write_sampling_frequency(struct file *filp, const char __user *buffer, size_t cnt,
                loff_t *off_set)
{
        int ret;
        int sampling_frequency = 0;
        struct tcc_hdmi_dev *dev = PDE_DATA(file_inode(filp));

        char *sampling_frequency_buf = devm_kzalloc(dev->pdev, cnt+1, GFP_KERNEL);

        if (sampling_frequency_buf == NULL)
                return -ENOMEM;

        ret = simple_write_to_buffer(sampling_frequency_buf, cnt, off_set, buffer, cnt);
        if (ret != cnt) {
                devm_kfree(dev->pdev, sampling_frequency_buf);
                return ret >= 0 ? -EIO : ret;
        }

        sampling_frequency_buf[cnt] = '\0';
        ret = sscanf(sampling_frequency_buf, "%d", &sampling_frequency);
        devm_kfree(dev->pdev, sampling_frequency_buf);
        if (ret < 0)
                return ret;

	switch (sampling_frequency) {
		case 32000:
		case 44100:
		case 88200:
		case 176000:
		case 48000:
		case 96000:
		case 192000:
			ret = 0;
			break;
		default:
			ret = -1;
			break;
	}
	if (ret < 0) {
		return ret;
	}

	dev->audioParam.mSamplingFrequency = sampling_frequency;
	if(!dev->suspend) {
		if(dev->power_status) {
			hdmi_audio_set_mode(dev);
			hdmi_audio_set_enable(dev, 1);
		}
	}

        return cnt;
}

static ssize_t proc_read_sampling_frequency(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
        ssize_t size;
        struct tcc_hdmi_dev *dev = PDE_DATA(file_inode(filp));
        char *sampling_frequency_buf = devm_kzalloc(dev->pdev, DEBUGFS_BUF_SIZE, GFP_KERNEL);

        size = sprintf(sampling_frequency_buf, "%d\n", dev->audioParam.mSamplingFrequency);
        size = simple_read_from_buffer(usr_buf, cnt,  off_set, sampling_frequency_buf, size);
        devm_kfree(dev->pdev, sampling_frequency_buf);
        return size;
}

static int proc_open(struct inode *inode, struct file *filp)
{
	try_module_get(THIS_MODULE);
        //struct tcc_hdmi_dev *dev = PDE_DATA(file_inode(filp));
	return 0;
}

static int proc_close(struct inode *inode, struct file *filp)
{
        //struct tcc_hdmi_dev *dev = PDE_DATA(file_inode(filp));
	module_put(THIS_MODULE);
	return 0;
}

static const struct file_operations proc_fops_sampling_frequency = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .write   = proc_write_sampling_frequency,
        .read    = proc_read_sampling_frequency,
};


void proc_interface_init(struct tcc_hdmi_dev *dev)
{
	if(dev == NULL){
		pr_err("[ERROR][HDMI_V14]%s:Device is null\n", __func__);
		return;
	}

	dev->hdmi_proc_dir = proc_mkdir("hdmi_tx", NULL);
	if(dev->hdmi_proc_dir == NULL){
		pr_err("[ERROR][HDMI_V14]%s:Could not create file system @ /proc/hdmi_tx\n",
			__func__);
	}

        dev->hdmi_proc_sampling_frequency = proc_create_data("sampling_frequency", S_IFREG | S_IRUGO | S_IWUGO,
                        dev->hdmi_proc_dir, &proc_fops_sampling_frequency, dev);
        if(dev->hdmi_proc_sampling_frequency == NULL){
                pr_err("[ERROR][HDMI_V14]%s:Could not create file system @"
                                " /proc/hdmi_tx/sampling_frequency\n", __func__);
        }
}

void proc_interface_remove(struct tcc_hdmi_dev *dev)
{

        if(dev->hdmi_proc_sampling_frequency != NULL)
                proc_remove(dev->hdmi_proc_sampling_frequency);
        if(dev->hdmi_proc_dir != NULL)
                proc_remove(dev->hdmi_proc_dir);
}



