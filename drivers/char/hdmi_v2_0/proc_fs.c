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
#include <include/hdmi_includes.h>
#include <include/proc_fs.h>
#include <include/hdmi_ioctls.h>
#include <include/hdmi_drm.h>
#include <phy/phy.h>

#include "hdmi_api_lib/include/bsp/i2cm.h"


#define DEBUGFS_BUF_SIZE 4096


ssize_t proc_read_hdcp_status(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	ssize_t size;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));

        char *hdcp_status_buf = devm_kzalloc(dev->parent_dev, DEBUGFS_BUF_SIZE, GFP_KERNEL);

        size = sprintf(hdcp_status_buf, "%d\n", dev->hdcp_status);

	if(dev->hotplug_status == 0 && dev->rxsense == 0 ){
		dev->hdcp_status = 0;
	}

	switch(dev->hdcp_status) {
	case 0:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP idle.");
		break;
	case 1:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 1.4 KSV list ready.");
		break;
	case 2:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 1.4 KSV list not valid.");
		break;
	case 3:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 1.4 KSV list depth exceeded.");
		break;
	case 4:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 1.4 KSV list memory access error.");
		break;
	case 5:
	case 12:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 1.4 Authentication success.");
		break;
	case 6:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 1.4 Authentication fail.");
		break;
	case 7:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 2.2 Not Capable");
		break;
	case 8:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 2.2 Capable.");
		break;
	case 9:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 2.2 Authentication lost.");
		break;
	case 10:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 2.2 Authentication success.");
		break;
	case 11:
		size += sprintf(hdcp_status_buf+size, "%s \n", "HDCP 2.2 Authentication fail.");
		break;
	}

        size = simple_read_from_buffer(usr_buf, cnt,  off_set, hdcp_status_buf, size);
        devm_kfree(dev->parent_dev, hdcp_status_buf);
	return size;
}

ssize_t proc_write_hdcp22(struct file *filp, const char __user *buffer, size_t cnt,
		loff_t *off_set){
	uint32_t hdcp22 = 0;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));
	// Check size of the input buffer
	if(cnt != sizeof(uint32_t)){
//		if(dev->verbose >= VERBOSE_IO)
			pr_err("%s:Sizes do not match [%d != %d]\n", FUNC_NAME,
				(int)cnt, (int)sizeof(uint32_t));
		return -1;
	}

	// Copy buffers
	if(copy_from_user(&hdcp22, buffer, cnt)) {
                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                return -EFAULT;
	}
        dev->hdcp22 = hdcp22;
	if(dev->verbose >= VERBOSE_IO)
		pr_info("%s:HDCP 2.2 = %d\n", FUNC_NAME, dev->hdcp22);

	return cnt;
}

/**
 * hpd functions
 */
ssize_t proc_read_hpd(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
        ssize_t size;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));

        char *hpd_buf = devm_kzalloc(dev->parent_dev, DEBUGFS_BUF_SIZE, GFP_KERNEL);
        size = sprintf(hpd_buf, "%d\n", dev->hotplug_status);
        size = simple_read_from_buffer(usr_buf, cnt,  off_set, hpd_buf, size);
        devm_kfree(dev->parent_dev, hpd_buf);
	return size;
}


ssize_t proc_write_hpd_lock(struct file *filp, const char __user *buffer, size_t cnt,
                loff_t *off_set){
        int ret;
        unsigned int hpd_lock = 0;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));

        char *hpd_lock_buf = devm_kzalloc(dev->parent_dev, cnt+1, GFP_KERNEL);

        if (hpd_lock_buf == NULL)
                return -ENOMEM;

        ret = simple_write_to_buffer(hpd_lock_buf, cnt, off_set, buffer, cnt);
        if (ret != cnt) {
                devm_kfree(dev->parent_dev, hpd_lock_buf);
                return ret >= 0 ? -EIO : ret;
        }

        hpd_lock_buf[cnt] = '\0';
        ret = sscanf(hpd_lock_buf, "%u", &hpd_lock);
        devm_kfree(dev->parent_dev, hpd_lock_buf);
        if (ret < 0)
                return ret;

        mutex_lock(&dev->mutex);
        if(hpd_lock) {
                set_bit(HDMI_TX_HOTPLUG_STATUS_LOCK, &dev->status);
        }
        else {
                clear_bit(HDMI_TX_HOTPLUG_STATUS_LOCK, &dev->status);
        }
        /* update hotplug_status */
        dev->hotplug_status = dev->hotplug_real_status;
        mutex_unlock(&dev->mutex);

        if(dev->verbose >= VERBOSE_IO)
                pr_info("%s:HPD LOCK = %d\n", FUNC_NAME, hpd_lock);

        return cnt;
}

/**
 * hpd functions
 */
ssize_t proc_read_hpd_lock(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
        ssize_t size;
        unsigned int hpd_lock;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));
        char *hpd_lock_buf = devm_kzalloc(dev->parent_dev, DEBUGFS_BUF_SIZE, GFP_KERNEL);

        mutex_lock(&dev->mutex);
        hpd_lock = test_bit(HDMI_TX_HOTPLUG_STATUS_LOCK, &dev->status);
        mutex_unlock(&dev->mutex);

        size = sprintf(hpd_lock_buf, "%u\n", hpd_lock);
        size = simple_read_from_buffer(usr_buf, cnt,  off_set, hpd_lock_buf, size);
        devm_kfree(dev->parent_dev, hpd_lock_buf);
        return size;
}

ssize_t proc_write_scdc_check(struct file *filp, const char __user *buffer, size_t cnt,
                loff_t *off_set){
        int ret;
        unsigned int scdc_check = 0;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));

        char *scdc_buf = devm_kzalloc(dev->parent_dev, cnt+1, GFP_KERNEL);

        if (scdc_buf == NULL)
                return -ENOMEM;

        ret = simple_write_to_buffer(scdc_buf, cnt, off_set, buffer, cnt);
        if (ret != cnt) {
                devm_kfree(dev->parent_dev, scdc_buf);
                return ret >= 0 ? -EIO : ret;
        }

        scdc_buf[cnt] = '\0';
        ret = sscanf(scdc_buf, "%u", &scdc_check);
        devm_kfree(dev->parent_dev, scdc_buf);
        if (ret < 0)
                return ret;

        mutex_lock(&dev->mutex);
        if(scdc_check) {
                set_bit(HDMI_TX_STATUS_SCDC_CHECK, &dev->status);
        }
        else {
                clear_bit(HDMI_TX_STATUS_SCDC_CHECK, &dev->status);
        }
        mutex_unlock(&dev->mutex);

        if(dev->verbose >= VERBOSE_IO)
                pr_info("%s:SCDC CHECK = %d\n", FUNC_NAME, scdc_check);

        return cnt;
}

/**
 * hpd functions
 */
ssize_t proc_read_scdc_check(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
        ssize_t size;
        unsigned int scdc_check;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));
        char *scdc_check_buf = devm_kzalloc(dev->parent_dev, DEBUGFS_BUF_SIZE, GFP_KERNEL);

        mutex_lock(&dev->mutex);
        scdc_check = test_bit(HDMI_TX_STATUS_SCDC_CHECK, &dev->status);
        mutex_unlock(&dev->mutex);

        size = sprintf(scdc_check_buf, "%u\n", scdc_check);
        size = simple_read_from_buffer(usr_buf, cnt,  off_set, scdc_check_buf, size);
        devm_kfree(dev->parent_dev, scdc_check_buf);
        return size;
}


ssize_t proc_write_ddc_check(struct file *filp, const char __user *buffer, size_t cnt,
                loff_t *off_set){
        int ret;
        char *ddc_buf = NULL;
        unsigned int ddc_addr, ddc_len;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));

        if(dev == NULL || dev->parent_dev == NULL)
                return -ENOMEM;

        ddc_buf = devm_kzalloc(dev->parent_dev, cnt+1, GFP_KERNEL);

        if (ddc_buf == NULL)
                return -ENOMEM;

        ret = simple_write_to_buffer(ddc_buf, cnt, off_set, buffer, cnt);
        if (ret != cnt) {
                devm_kfree(dev->parent_dev, ddc_buf);
                return ret >= 0 ? -EIO : ret;
        }

        ddc_buf[cnt] = '\0';
        ret = sscanf(ddc_buf, "%u %u", &ddc_addr, &ddc_len);
        devm_kfree(dev->parent_dev, ddc_buf);
        if (ret < 0) {
                return ret;
        }

        mutex_lock(&dev->mutex);
        if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
                if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                        hdmi_ddc_check(dev, ddc_addr, ddc_len);
                }else {
                        pr_err("%s HDMI is not powred <%d>\r\n", __func__, __LINE__);
                }
        } else {
                pr_err("##%s Failed to hdmi_ddc_check because hdmi linke was suspended \r\n", __func__);
        }
        mutex_unlock(&dev->mutex);

        return cnt;
}

#if defined(CONFIG_TCC_RUNTIME_GET_EDID_ID)
ssize_t proc_write_edid_machine_id(struct file *filp, const char __user *buffer, size_t cnt,
                loff_t *off_set){
        int ret;
        unsigned int edid_machine_id = 0;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));

        char *edid_machine_id_buf = devm_kzalloc(dev->parent_dev, cnt+1, GFP_KERNEL);

        if (edid_machine_id_buf == NULL)
                return -ENOMEM;

        ret = simple_write_to_buffer(edid_machine_id_buf, cnt, off_set, buffer, cnt);
        if (ret != cnt) {
                devm_kfree(dev->parent_dev, edid_machine_id_buf);
                return ret >= 0 ? -EIO : ret;
        }

        edid_machine_id_buf[cnt] = '\0';
        ret = sscanf(edid_machine_id_buf, "%u", &edid_machine_id);
        devm_kfree(dev->parent_dev, edid_machine_id_buf);
        if (ret < 0)
                return ret;

        mutex_lock(&dev->mutex);
        dev->edid_machine_id = edid_machine_id;
        mutex_unlock(&dev->mutex);

        if(dev->verbose >= VERBOSE_IO)
                pr_info("[%s] edid_machine_id = %d\n", FUNC_NAME, edid_machine_id);

        return cnt;
}

ssize_t proc_read_edid_machine_id(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
        ssize_t size;
        unsigned int edid_machine_id;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));
        char *edid_machine_id_buf = devm_kzalloc(dev->parent_dev, DEBUGFS_BUF_SIZE, GFP_KERNEL);

        mutex_lock(&dev->mutex);
        edid_machine_id = dev->edid_machine_id;
        mutex_unlock(&dev->mutex);

        size = sprintf(edid_machine_id_buf, "%u\n", edid_machine_id);
        size = simple_read_from_buffer(usr_buf, cnt,  off_set, edid_machine_id_buf, size);
        devm_kfree(dev->parent_dev, edid_machine_id_buf);
        return size;
}
#endif

#if defined(CONFIG_TCC_RUNTIME_DRM_TEST)
ssize_t proc_write_drm(struct file *filp, const char __user *buffer, size_t cnt,
                loff_t *off_set){

        static int stage = 0;
        int ret;
        unsigned int drm_test_run = 0;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));

        char *drm_test_run_buf = devm_kzalloc(dev->parent_dev, cnt+1, GFP_KERNEL);

        if (drm_test_run_buf == NULL)
                return -ENOMEM;

        ret = simple_write_to_buffer(drm_test_run_buf, cnt, off_set, buffer, cnt);
        if (ret != cnt) {
                devm_kfree(dev->parent_dev, drm_test_run_buf);
                return ret >= 0 ? -EIO : ret;
        }

        drm_test_run_buf[cnt] = '\0';
        ret = sscanf(drm_test_run_buf, "%u", &drm_test_run);
        devm_kfree(dev->parent_dev, drm_test_run_buf);
        if (ret < 0)
                return ret;

        if(drm_test_run) {
                int i;
                DRM_Packet_t drm_param;
                memset(&drm_param, 0, sizeof(drm_param));
                drm_param.mInfoFrame.version = 1;
                drm_param.mInfoFrame.length = 26;
                drm_param.mDescriptor_type1.Descriptor_ID = 0;
                switch(++stage) {
                        case 1:
                                drm_param.mDescriptor_type1.EOTF = 0;
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                break;
                        case 2:
                                drm_param.mDescriptor_type1.EOTF = 1;
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                break;
                        case 3:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                break;
                        case 4:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                        drm_param.mDescriptor_type1.disp_primaries_x[0] = (1 << i);
                                        hdmi_update_drm_configure(dev, &drm_param);
                                        hdmi_apply_drm(dev);
                                        pr_info("%s primaries_x[0] = %d\r\n", __func__,  i);
                                        msleep(1000);
                                }
                                break;
                        case 5:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.disp_primaries_x[1] = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s primaries_x[1] = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;
                        case 6:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.disp_primaries_x[2] = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s primaries_x[2] = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;
                        case 7:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.disp_primaries_y[0] = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s disp_primaries_y[0] = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;
                        case 8:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.disp_primaries_y[1] = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s disp_primaries_y[1] = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;
                        case 9:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.disp_primaries_y[2] = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s disp_primaries_y[2] = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;

                        case 10:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.white_point_x = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s white_point_x = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;
                        case 11:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.white_point_y = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s white_point_y = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;
                        case 12:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.max_disp_mastering_luminance = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s max_disp_mastering_luminance = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;
                        case 13:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.min_disp_mastering_luminance = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s min_disp_mastering_luminance = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;
                        case 14:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.max_content_light_level = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s max_content_light_level = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;
                        case 15:
                                drm_param.mDescriptor_type1.EOTF = 2;
                                for(i=0; i< 16; i++) {
                                drm_param.mDescriptor_type1.max_frame_avr_light_level = (1 << i);
                                hdmi_update_drm_configure(dev, &drm_param);
                                hdmi_apply_drm(dev);
                                pr_info("%s max_frame_avr_light_level = %d\r\n", __func__,  i);
                                msleep(1000);
                                }
                                break;
                }

        } else {
                stage = 0;
        }

        return cnt;
}
#endif


#if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
ssize_t proc_write_phy_regs(struct file *filp, const char __user *buffer, size_t cnt, loff_t *off_set){
        ssize_t size;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));

        char *phy_regs_buf = devm_kzalloc(dev->parent_dev, cnt+1, GFP_KERNEL);

        do {
                if (phy_regs_buf == NULL) {
                        size =  -ENOMEM;
                        break;
                }

                size = simple_write_to_buffer(phy_regs_buf, cnt, off_set, buffer, cnt);
                if (size != cnt) {

                        if(size >= 0) {
                                size = -EIO;
                                break;
                        }
                }
                phy_regs_buf[cnt] = '\0';

                if(0 == dwc_hdmi_proc_write_phy_regs(dev, phy_regs_buf)) {
                        size = cnt;
                        if(test_bit(HDMI_TX_HOTPLUG_STATUS_LOCK, &dev->status)){
                                pr_info("%s hpd is locked\r\n", __func__);
                        } else {

                                dev->hotplug_status = 0;
                                wake_up_interruptible(&dev->poll_wq);
                                pr_info("%s set hotplug_status swing\r\n", __func__);
                                schedule_delayed_work(&dev->hdmi_restore_hotpug_work, usecs_to_jiffies(1000000));
                        }
                } else {
                        size = 0;
                }
        }
        while(0);

        if(phy_regs_buf != NULL);
                devm_kfree(dev->parent_dev, phy_regs_buf);
        return size;
}

/**
 * hpd functions
 */
ssize_t proc_read_phy_regs(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
        ssize_t size = -EINVAL;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));
        char *phy_regs_buf = devm_kzalloc(dev->parent_dev, DEBUGFS_BUF_SIZE, GFP_KERNEL);
        if(phy_regs_buf != NULL) {
                size = dwc_hdmi_proc_read_phy_regs(dev, phy_regs_buf, DEBUGFS_BUF_SIZE);
                if(size > 0) {
                        size = simple_read_from_buffer(usr_buf, cnt,  off_set, phy_regs_buf, size);
                }
                devm_kfree(dev->parent_dev, phy_regs_buf);
        }
        return size;
}
#endif


#if defined(CONFIG_TCC_RUNTIME_DV_VSIF)
extern int hdmi_api_vsif_update_by_index(int index);
ssize_t proc_write_dv_vsif(struct file *filp, const char __user *buffer, size_t cnt, loff_t *off_set){
        ssize_t size;
        int dv_vsif_index;
        struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));
        char *dv_vsfi_buf = devm_kzalloc(dev->parent_dev, cnt+1, GFP_KERNEL);

        do {
                if (dv_vsfi_buf == NULL) {
                        size =  -ENOMEM;
                        break;
                }

                size = simple_write_to_buffer(dv_vsfi_buf, cnt, off_set, buffer, cnt);
                if (size != cnt) {
                        if(size >= 0) {
                                size = -EIO;
                                break;
                        }
                }
                dv_vsfi_buf[cnt] = '\0';

                sscanf(dv_vsfi_buf, "%u", &dv_vsif_index);
                devm_kfree(dev->parent_dev, dv_vsfi_buf);

                if(dv_vsif_index < 0 || dv_vsif_index > 6) {
                        pr_err("invalid index %d\r\n", dv_vsif_index);
                        break;
                }
                hdmi_api_vsif_update_by_index(dv_vsif_index);
        }
        while(0);

        return size;
}
#endif


static const struct file_operations proc_fops_hdcp_status = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .read  = proc_read_hdcp_status,
};

static const struct file_operations proc_fops_hpd = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .read    = proc_read_hpd,
};

static const struct file_operations proc_fops_hpd_lock = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .write   = proc_write_hpd_lock,
        .read    = proc_read_hpd_lock,
};

static const struct file_operations proc_fops_hdcp22 = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .write   = proc_write_hdcp22,

};
static const struct file_operations proc_fops_scdc_check = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .write   = proc_write_scdc_check,
        .read    = proc_read_scdc_check,
};

static const struct file_operations proc_fops_ddc_check = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .write   = proc_write_ddc_check,
};

#if defined(CONFIG_TCC_RUNTIME_GET_EDID_ID)
static const struct file_operations proc_fops_edid_machine_id = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .write   = proc_write_edid_machine_id,
        .read    = proc_read_edid_machine_id,
};
#endif

#if defined(CONFIG_TCC_RUNTIME_DRM_TEST)
static const struct file_operations proc_fops_drm = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .write   = proc_write_drm,
};
#endif

#if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
static const struct file_operations proc_fops_phy_regs = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .write   = proc_write_phy_regs,
        .read    = proc_read_phy_regs,
};
#endif

#if defined(CONFIG_TCC_RUNTIME_DV_VSIF)
static const struct file_operations proc_fops_dv_vsif = {
        .owner   = THIS_MODULE,
        .open    = proc_open,
        .release = proc_close,
        .write   = proc_write_dv_vsif,
};
#endif


int
proc_open(struct inode *inode, struct file *filp){
	try_module_get(THIS_MODULE);
        //struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));
	return 0;
}

int
proc_close(struct inode *inode, struct file *filp){
        //struct hdmi_tx_dev *dev = PDE_DATA(file_inode(filp));
	module_put(THIS_MODULE);
	return 0;
}

void proc_interface_init(struct hdmi_tx_dev *dev){
	if(dev == NULL){
		pr_err("%s:Device is null\n", FUNC_NAME);
		return;
	}

	dev->hdmi_proc_dir = proc_mkdir("hdmi_tx", NULL);
	if(dev->hdmi_proc_dir == NULL){
		pr_err("%s:Could not create file system @ /proc/hdmi_tx\n",
			FUNC_NAME);
	}

	// HPD
	//pr_info("%s:Installing /proc/hdmi_tx/hpd file\n", FUNC_NAME);

	dev->hdmi_proc_hpd = proc_create_data("hpd", S_IFREG | S_IRUGO,
			dev->hdmi_proc_dir, &proc_fops_hpd, dev);
	if(dev->hdmi_proc_hpd == NULL){
		pr_err("%s:Could not create file system @"
				" /proc/hdmi_tx/hpd\n", FUNC_NAME);
	}

        // HPD_LOCK
        dev->hdmi_proc_hpd_lock = proc_create_data("hpd_lock", S_IFREG | S_IRUGO | S_IWUGO,
                        dev->hdmi_proc_dir, &proc_fops_hpd_lock, dev);
        if(dev->hdmi_proc_hpd_lock == NULL){
                pr_err("%s:Could not create file system @"
                                " /proc/hdmi_tx/hpd_lock\n", FUNC_NAME);
        }

	// HDCP 2.2 status
	//pr_info("%s:Installing /proc/hdmi_tx/hdcp2_status file\n", FUNC_NAME);

	dev->hdmi_proc_hdcp_status = proc_create_data("hdcp_status", S_IFREG | S_IRUGO,
			dev->hdmi_proc_dir, &proc_fops_hdcp_status, dev);
	if(dev->hdmi_proc_hdcp_status == NULL){
		pr_err("%s:Could not create file system @"
				" /proc/hdmi_tx/hdcp_status\n", FUNC_NAME);
	}

	// HDCP 2.2
	//pr_info("%s:Installing /proc/hdmi_tx/hdcp22 file\n", FUNC_NAME);

	dev->hdmi_proc_hdcp22 = proc_create_data("hdcp22", S_IFREG | S_IWUGO,
			dev->hdmi_proc_dir, &proc_fops_hdcp22, dev);
	if(dev->hdmi_proc_hdcp22 == NULL){
		pr_err("%s:Could not create file system @"
				" /proc/hdmi_tx/hdcp22\n", FUNC_NAME);
	}

        //pr_info("%s:Installing /proc/hdmi_tx/scdc_check file\n", FUNC_NAME);
        dev->hdmi_proc_scdc_check = proc_create_data("scdc_check", S_IFREG | S_IRUGO | S_IWUGO,
                        dev->hdmi_proc_dir, &proc_fops_scdc_check, dev);
        if(dev->hdmi_proc_scdc_check == NULL){
                pr_err("%s:Could not create file system @"
                                " /proc/hdmi_tx/scdc_check\n", FUNC_NAME);
        }

        //pr_info("%s:Installing /proc/hdmi_tx/ddc_check file\n", FUNC_NAME);
        dev->hdmi_proc_ddc_check = proc_create_data("ddc_check", S_IFREG | S_IWUGO,
                        dev->hdmi_proc_dir, &proc_fops_ddc_check, dev);
        if(dev->hdmi_proc_ddc_check == NULL){
                pr_err("%s:Could not create file system @"
                                " /proc/hdmi_tx/ddc_check\n", FUNC_NAME);
        }

        #if defined(CONFIG_TCC_RUNTIME_GET_EDID_ID)
        //pr_info("%s:Installing /proc/hdmi_tx/edid_machine_id file\n", FUNC_NAME);
        dev->hdmi_proc_edid_machine_id = proc_create_data("edid_machine_id", S_IFREG | S_IRUGO | S_IWUGO,
                  dev->hdmi_proc_dir, &proc_fops_edid_machine_id, dev);
        if(dev->hdmi_proc_edid_machine_id == NULL){
          pr_err("%s:Could not create file system @"
                          " /proc/hdmi_tx/edid_machine_id\n", FUNC_NAME);
        }
        #endif

        #if defined(CONFIG_TCC_RUNTIME_DRM_TEST)
        //pr_info("%s:Installing /proc/hdmi_tx/drm file\n", FUNC_NAME);
        dev->hdmi_proc_drm = proc_create_data("drm", S_IFREG | S_IRUGO | S_IWUGO,
                        dev->hdmi_proc_dir, &proc_fops_drm, dev);
        if(dev->hdmi_proc_drm == NULL){
                pr_err("%s:Could not create file system @"
                                " /proc/hdmi_tx/drm\n", FUNC_NAME);
        }
        #endif

        #if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
        //pr_info("%s:Installing /proc/hdmi_tx/phy_regs file\n", FUNC_NAME);
        dev->hdmi_proc_phy_regs = proc_create_data("phy_regs", S_IFREG | S_IRUGO | S_IWUGO,
                        dev->hdmi_proc_dir, &proc_fops_phy_regs, dev);
        if(dev->hdmi_proc_phy_regs == NULL){
                pr_err("%s:Could not create file system @"
                                " /proc/hdmi_tx/phy_regs\n", FUNC_NAME);
        }
        #endif

        #if defined(CONFIG_TCC_RUNTIME_DV_VSIF)
        dev->hdmi_proc_dv_vsif = proc_create_data("dv_vsif", S_IFREG | S_IWUGO,
                        dev->hdmi_proc_dir, &proc_fops_dv_vsif, dev);
        if(dev->hdmi_proc_dv_vsif == NULL){
                pr_err("%s:Could not create file system @"
                                " /proc/hdmi_tx/dv_vsif\n", FUNC_NAME);
        }
        #endif
}

void proc_interface_remove(struct hdmi_tx_dev *dev){

	if(dev->hdmi_proc_hdcp_status != NULL)
		proc_remove(dev->hdmi_proc_hdcp_status);

	if(dev->hdmi_proc_hpd != NULL)
		proc_remove(dev->hdmi_proc_hpd);

        if(dev->hdmi_proc_hpd_lock != NULL)
                        proc_remove(dev->hdmi_proc_hpd_lock);

	if(dev->hdmi_proc_hdcp22 != NULL)
		proc_remove(dev->hdmi_proc_hdcp22);

        if(dev->hdmi_proc_scdc_check != NULL)
                proc_remove(dev->hdmi_proc_scdc_check);

        if(dev->hdmi_proc_ddc_check != NULL)
                proc_remove(dev->hdmi_proc_ddc_check);

        #if defined(CONFIG_TCC_RUNTIME_GET_EDID_ID)
        if(dev->hdmi_proc_edid_machine_id != NULL)
                proc_remove(dev->hdmi_proc_edid_machine_id);
        #endif

        #if defined(CONFIG_TCC_RUNTIME_DRM_TEST)
        if(dev->hdmi_proc_drm != NULL)
                proc_remove(dev->hdmi_proc_drm);
        #endif

        #if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
        if(dev->hdmi_proc_phy_regs != NULL)
                proc_remove(dev->hdmi_proc_phy_regs);
        #endif
        #if defined(CONFIG_TCC_RUNTIME_DV_VSIF)
        if(dev->hdmi_proc_dv_vsif != NULL)
                proc_remove(dev->hdmi_proc_dv_vsif);
        #endif
        if(dev->hdmi_proc_dir != NULL)
                proc_remove(dev->hdmi_proc_dir);
}



