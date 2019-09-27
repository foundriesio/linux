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
#include "include/hdmi_includes.h"
#include <linux/gpio.h>
#include <linux/clk.h>  // clk (example clk_set_rate)
#include <asm/bitops.h> // bit macros
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_ddicfg.h>

#if defined(CONFIG_PM)
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#include <include/irq_handlers.h>
#include <include/hdmi_ioctls.h>
#include <include/hdmi_access.h>
#include <include/video_params.h>
#include <include/hdmi_drm.h>
#include <include/hdmi_log.h>

#include <bsp/i2cm.h>
#include <core/main_controller.h>
#include <phy/phy.h>
#include <core/video.h>
#include <core/fc_video.h>
#include <core/audio.h>
#include <bsp/i2cm.h>
#include <phy/phy.h>
#include <core/irq.h>
#include <api/api.h>
#include <scdc/scrambling.h>
#include <scdc/scdc.h>
#include <soc/soc.h>
#include <core/fc_avi.h>
#include <core/packets.h>

#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <hdmi_api_lib/src/core/video/video_packetizer_reg.h>
#include <hdcp/hdcp.h>

int dwc_hdmi_is_suspended(struct hdmi_tx_dev *dev)
{
        int suspend_status = 0;

        if(dev != NULL) {
                suspend_status = (dev->status &
                        ((1 << HDMI_TX_STATUS_SUSPEND_L1) | (1 << HDMI_TX_STATUS_SUSPEND_L0)))?1:0;
        }

        return suspend_status;
}

#if defined(CONFIG_TCC_HDMI_TIME_PROFILE)
#include <linux/time.h>

int dwc_hdmi_get_time(struct timeval *prev_val, struct timeval *curr_val)
{
    int  time_diff_ms = 0;
    if(prev_val != NULL && curr_val != NULL) {
            time_diff_ms = (curr_val->tv_sec - prev_val->tv_sec)*1000;
            time_diff_ms += (curr_val->tv_usec - prev_val->tv_usec)/1000;
    }

    return time_diff_ms;
}
#endif

/* Reset HDMI Link and PHY */
void dwc_hdmi_hw_reset(struct hdmi_tx_dev *dev, int reset_on)
{
        #if defined(CONFIG_TCC_HDMI_TIME_PROFILE)
        struct timeval prev_val, curr_val;
        do_gettimeofday(&prev_val);
        #endif

        do {
                if(dev == NULL) {
                        pr_err("%s dev is NULL\r\n", __func__);
                        break;
                }

                if(test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
                        pr_info("%s skip, because dev was suspended\r\n", __func__);
                        break;
                }

                if(reset_on) {
                        // 0. Disable Interrupt of HDMI
                        dwc_hdmi_disable_interrupt(dev);

                        if(!test_bit(HDMI_TX_STATUS_PHY_ALIVE, &dev->status)) {
                                // 1. PHY STANDBY
                                dwc_hdmi_phy_standby(dev);

                                // 2. RESET4-HDMI LINK RESET
                                VIOC_DDICONFIG_reset_hdmi_link(dev->ddibus_io, 1);
                        } else {
                                pr_info("%s skip, because hdmi phy only alived\r\n", __func__);
                        }
                }
                else {
                         if(!test_bit(HDMI_TX_STATUS_PHY_ALIVE, &dev->status)) {
                                // 0. RESET3-HDMICTRL SWRESET LINK
                                 VIOC_DDICONFIG_reset_hdmi_link(dev->ddibus_io, 0);

                                // 1. PHY STANDBY
                                dwc_hdmi_phy_standby(dev);
                        } else {
                                pr_info("%s skip, because hdmi phy only alived\r\n", __func__);
                        }

                        /**
        		 * HPD = gpio | enhpdrxsense
        		 * set enhpdrxsense to 0
        		 */
                        hdmi_phy_enable_hpd_sense(dev);

                        // 2. Set Tmds bit order..!!
        		#if defined(HDMI_CTRL_TB_VAL)
        		VIOC_DDICONFIG_Set_tmds_bit_order(dev->ddibus_io, HDMI_CTRL_TB_VAL);
        		#endif


                        // 3. Initialize I2C Timing
                        hdmi_i2cddc_reset(dev);
                        hdmi_i2cddc_fast_mode(dev, 0);
                        // SFRFREQ is HDCP1.4 FREQ(50MHz) /2  -> 25MHz
                        hdmi_i2cddc_clk_config(dev, HDMI_DDC_SFRCLK,
                                I2C_MIN_SS_SCL_LOW_TIME,
                                I2C_MIN_SS_SCL_HIGH_TIME,
                                I2C_MIN_FS_SCL_LOW_TIME,
                                I2C_MIN_FS_SCL_HIGH_TIME);
                        hdmi_i2cddc_sda_hold(dev, HDMI_DDC_SDA_HOLD);
                        // 4. Enable Interrupt of HDMI
                        dwc_hdmi_enable_interrupt(dev);
                }
        } while(0);
        #if defined(CONFIG_TCC_HDMI_TIME_PROFILE)
        do_gettimeofday(&curr_val);
        pr_info("%s (%d) - %dms\r\n", __func__, reset_on, dwc_hdmi_get_time(&prev_val, &curr_val));
        #endif
}

/*!
 * @brief               This function turn off hdmi display power
 * @details             This function controls blocks related with display device such as hdmi phy, dibus
 * @date                2018.05.14 */
static void dwc_hdmi_phy_power_on(struct hdmi_tx_dev *dev)
{
        #if defined(CONFIG_TCC_HDMI_TIME_PROFILE)
        struct timeval prev_val, curr_val;
        do_gettimeofday(&prev_val);
        #endif

        if(++dev->display_clock_enable_count == 1 &&
                !test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
                if(dev->verbose >= VERBOSE_IO)
                        printk("%s enable display clock\r\n", __func__);

                if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_DDIBUS])) {
                        clk_prepare_enable(dev->clk[HDMI_CLK_INDEX_DDIBUS]);
                }

                if(dwc_hdmi_phy_use_peri_clock(dev)) {
                        // HDMI_CLK_INDEX_PERI - 24MHz
                        if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_PERI])) {
                                clk_set_rate(dev->clk[HDMI_CLK_INDEX_PERI], HDMI_PHY_REF_CLK_RATE);
                                clk_prepare_enable(dev->clk[HDMI_CLK_INDEX_PERI]);
                                if(dev->verbose >= VERBOSE_IO)
                                        pr_info("%s: HDMI PHY24M is %dHz\r\n", FUNC_NAME, (int)clk_get_rate(dev->clk[HDMI_CLK_INDEX_PERI]));
                        }
                }

                // PCLK is set to 50MHz
                if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_APB])) {
                        clk_set_rate(dev->clk[HDMI_CLK_INDEX_APB], dev->clk_freq_apb);
                        clk_prepare_enable(dev->clk[HDMI_CLK_INDEX_APB]);
                        if(dev->verbose >= VERBOSE_IO)
                                        pr_info("%s: HDMI PCLK is %dHz\r\n", FUNC_NAME, (int)clk_get_rate(dev->clk[HDMI_CLK_INDEX_APB]));
                }

                if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_ISOIP])) {
                        clk_prepare_enable(dev->clk[HDMI_CLK_INDEX_ISOIP]);
                }
        }
        if(dev->verbose >= VERBOSE_IO)
                printk("%s dev->display_clock_enable_count=%d\r\n", __func__, dev->display_clock_enable_count);
        #if defined(CONFIG_TCC_HDMI_TIME_PROFILE)
        do_gettimeofday(&curr_val);
        pr_info("%s - %dms\r\n", __func__, dwc_hdmi_get_time(&prev_val, &curr_val));
        #endif
}


/*!
 * @brief               This function turn off hdmi display power
 * @details             This function controls blocks related with display device such as hdmi phy, dibus
 * @date                2018.05.14 */
static void dwc_hdmi_phy_power_off(struct hdmi_tx_dev *dev)
{
        if(dev->display_clock_enable_count == 1) {
		/* Clear pixel clock information */
		dev->hdmi_tx_ctrl.pixel_clock = 0;
                if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
                        if(dev->verbose >= VERBOSE_IO)
                                printk("%s disable display clock\r\n", __func__);
                        if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_ISOIP])) {
                                clk_disable_unprepare(dev->clk[HDMI_CLK_INDEX_ISOIP]);
                        }
                        if(dwc_hdmi_phy_use_peri_clock(dev)) {
                                if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_PERI])) {
                                        clk_disable_unprepare(dev->clk[HDMI_CLK_INDEX_PERI]);
                                }
                        }
                        if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_APB])) {
                                clk_disable_unprepare(dev->clk[HDMI_CLK_INDEX_APB]);
                        }
                        if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_DDIBUS])) {
                                clk_disable_unprepare(dev->clk[HDMI_CLK_INDEX_DDIBUS]);
                        }
                }
                dev->display_clock_enable_count = 0;
        } else {
                if(dev->display_clock_enable_count > 1) {
                        dev->display_clock_enable_count--;
                }
        }
        if(dev->verbose >= VERBOSE_IO)
                printk("%s dev->display_clock_enable_count=%d\r\n", __func__, dev->display_clock_enable_count);
}


static void dwc_hdmi_link_power_on_core(struct hdmi_tx_dev *dev, int need_link_reset)
{
        #if defined(CONFIG_TCC_HDMI_TIME_PROFILE)
        struct timeval prev_val, curr_val;
        do_gettimeofday(&prev_val);
        #endif

        if(++dev->hdmi_clock_enable_count == 1 &&
                !test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
                if(dev->verbose >= VERBOSE_IO)
                        printk("%s enable hdmi clock\r\n", __func__);

                if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_SPDIF])) {
                        clk_set_rate(dev->clk[HDMI_CLK_INDEX_SPDIF], HDMI_SPDIF_REF_CLK_RATE);
                        clk_prepare_enable(dev->clk[HDMI_CLK_INDEX_SPDIF]);
                        if(dev->verbose >= VERBOSE_IO)
                                        pr_info("%s: HDMI SPDIF is %dHz\r\n", FUNC_NAME, (int)clk_get_rate(dev->clk[HDMI_CLK_INDEX_SPDIF]));
                }
                // HDCP14 is set to 50MHz
                if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_HDCP14])) {
                        clk_set_rate(dev->clk[HDMI_CLK_INDEX_HDCP14], HDMI_HDCP14_CLK_RATE);
                        clk_prepare_enable(dev->clk[HDMI_CLK_INDEX_HDCP14]);
                        if(dev->verbose >= VERBOSE_IO)
                                pr_info("%s: HDMI HDCP14 is %dHz\r\n", FUNC_NAME, (int)clk_get_rate(dev->clk[HDMI_CLK_INDEX_HDCP14]));
                }

                // HDCP22 is set to 50MHz
                if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_HDCP22])) {
                        clk_set_rate(dev->clk[HDMI_CLK_INDEX_HDCP22], HDMI_HDCP22_CLK_RATE);
                        clk_prepare_enable(dev->clk[HDMI_CLK_INDEX_HDCP22]);
                        if(dev->verbose >= VERBOSE_IO)
                                pr_info("%s: HDMI HDCP22 is %dHz\r\n", FUNC_NAME, (int)clk_get_rate(dev->clk[HDMI_CLK_INDEX_HDCP22]));
                }

                // Select REF - PHY 24MHz
                VIOC_DDICONFIG_Set_refclock(dev->ddibus_io, dev->hdmi_ref_src_clk << HDMI_CTRL_REF_SHIFT);

                /* hdcp prng */
                VIOC_DDICONFIG_Set_prng(dev->ddibus_io, 1);


                // ENABLE HDMI LINK
                VIOC_DDICONFIG_Set_hdmi_enable(dev->ddibus_io, 1);

                if(need_link_reset)
                        dwc_hdmi_hw_reset(dev, 0);

                set_bit(HDMI_TX_STATUS_POWER_ON, &dev->status);
        }
        if(dev->verbose >= VERBOSE_IO)
                printk("dwc_hdmi_link_power_on_core : dev->hdmi_clock_enable_count=%d\r\n", dev->hdmi_clock_enable_count);

        #if defined(CONFIG_TCC_HDMI_TIME_PROFILE)
        do_gettimeofday(&curr_val);
        pr_info("%s - %dms\r\n", __func__, dwc_hdmi_get_time(&prev_val, &curr_val));
        #endif

}

static void dwc_hdmi_link_power_on(struct hdmi_tx_dev *dev)
{
        dwc_hdmi_link_power_on_core(dev, 1);
}

static void dwc_hdmi_link_power_off(struct hdmi_tx_dev *dev)
{
        if(dev->hdmi_clock_enable_count == 1) {
                clear_bit(HDMI_TX_STATUS_POWER_ON, &dev->status);
                if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
                        if(dev->verbose >= VERBOSE_IO)
                                printk("dwc_hdmi_link_power_off\r\n");
                        dwc_hdmi_hw_reset(dev, 1);

                        // DISABLE HDMI LINK
                        VIOC_DDICONFIG_Set_hdmi_enable(dev->ddibus_io, 0);

                        if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_HDCP14])) {
                                clk_disable_unprepare(dev->clk[HDMI_CLK_INDEX_HDCP14]);
                        }
                        if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_HDCP22])) {
                                clk_disable_unprepare(dev->clk[HDMI_CLK_INDEX_HDCP22]);
                        }

                        if(!IS_ERR(dev->clk[HDMI_CLK_INDEX_SPDIF])) {
                                clk_disable_unprepare(dev->clk[HDMI_CLK_INDEX_SPDIF]);
                        }
                }
                dev->hdmi_clock_enable_count = 0;
        }else {
                if(dev->hdmi_clock_enable_count > 1) {
                        dev->hdmi_clock_enable_count--;
                }
        }
        if(dev->verbose >= VERBOSE_IO)
                printk("%s : dev->hdmi_clock_enable_count=%d\r\n", __func__, dev->hdmi_clock_enable_count);
}


void dwc_hdmi_power_on_core(struct hdmi_tx_dev *dev, int need_link_reset)
{
        dwc_hdmi_phy_power_on(dev);
        dwc_hdmi_link_power_on_core(dev, need_link_reset);
}

void dwc_hdmi_power_on(struct hdmi_tx_dev *dev)
{
	dwc_hdmi_power_on_core(dev, 1);
}

void dwc_hdmi_power_off(struct hdmi_tx_dev *dev)
{
        dwc_hdmi_link_power_off(dev);
        dwc_hdmi_phy_power_off(dev);
}

void dwc_hdmi_set_hdcp_keepout(struct hdmi_tx_dev *dev)
{
        if(dev != NULL) {
                videoParams_t * video = dev->videoParam;

                if(video != NULL) {
                        if(dev->hdmi_tx_ctrl.hdcp_on ||
                                (video->mScdcPresent && video->mScrambling)) {
				/*
				 * When the HDMI 2.0 support is enabled (DWC_HDMI_TX_20 parameter is set), all
	                         * output video modes with a TMDS frequency higher than 340 MHz require that the
	                         * Scrambler feature is activated by setting the field fc_invidconf.HDCP_keepout
	                         * register to 1`b1 and field fc_scramber_ctrl.scrambler_en register to 1`b1.
	                         */
                                fc_video_hdcp_keepout(dev, 1);
                        } else {
                                fc_video_hdcp_keepout(dev, 0);
                        }
                }
        }
}

encoding_t hdmi_get_current_output_encoding(struct hdmi_tx_dev *dev)
{
        int type;
        encoding_t encoding = RGB;
        if(dev != NULL) {
                type = hdmi_dev_read_mask(dev, FC_AVICONF0, FC_AVICONF0_RGC_YCC_INDICATION_MASK);
                switch(type) {
                        case 1:
                                encoding = YCC422;
                                break;
                        case 2:
                                encoding = YCC444;
                                break;
                        case 3:
                                encoding = YCC420;
                                break;
                        default:
                                break;
                }
        }
        return encoding;
}

color_depth_t hdmi_get_current_output_depth(struct hdmi_tx_dev *dev, encoding_t encoding)
{
        color_depth_t color_depth = COLOR_DEPTH_8;

        unsigned int reg_remap_size, reg_color_depth;

        if(dev != NULL) {
                reg_color_depth = hdmi_dev_read_mask(dev, VP_PR_CD, VP_PR_CD_COLOR_DEPTH_MASK);
                reg_remap_size = hdmi_dev_read_mask(dev, VP_REMAP, VP_REMAP_YCC422_SIZE_MASK);
                if(encoding == YCC422) {
                        switch(reg_remap_size) {
                                case 0:
                                        break;
                                case 1:
                                        color_depth = COLOR_DEPTH_10;
                                        break;
                                case 2:
                                        color_depth = COLOR_DEPTH_12;
                                        break;
                        }
                } else {
                        switch(reg_color_depth) {
                                case 0:
                                        break;
                                case 5:
                                        color_depth = COLOR_DEPTH_10;
                                        break;
                                case 6:
                                        color_depth = COLOR_DEPTH_12;
                                        break;
                        }
                }
        }
        return color_depth;
}

video_mode_t hdmi_get_current_output_mode(struct hdmi_tx_dev *dev)
{

        video_mode_t video_mode = DVI;

        if(dev != NULL) {
                video_mode = hdmi_dev_read_mask(dev, FC_INVIDCONF, FC_INVIDCONF_DVI_MODEZ_MASK);
        }
        return video_mode;
}

unsigned int hdmi_get_current_output_vic(struct hdmi_tx_dev *dev)
{
        unsigned int vic = 0;
        int hdmi_video_format, hdmi_vic;


        if(dev != NULL) {
                if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                vic = hdmi_dev_read_mask(dev, FC_AVIVID, FC_AVIVID_FC_AVIVID_MASK);
                                if(vic == 0) {
                                        hdmi_video_format = hdmi_dev_read(dev, FC_VSDPAYLOAD0);
                                        hdmi_vic = hdmi_dev_read(dev, FC_VSDPAYLOAD0+4);
                                        hdmi_video_format >>= 5;
                                        if(hdmi_video_format == 1) {
                                                vic = videoParams_GetCeaVicCode(hdmi_vic);
                                        }
                                }
                        } else {
				pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
			}
                } else {
			pr_err("## Failed to get vic because hdmi linke was suspended\r\n");
		}
        }
        return vic;
}


#ifdef CONFIG_PM
static int hdmi_tx_blank(struct hdmi_tx_dev *dev, int blank_mode)
{
        int ret = 0;
        struct device *pdev = (dev!=NULL)?dev->parent_dev:NULL;
        pr_info("%s : blank(mode=%d)\n",__func__, blank_mode);

        if(pdev != NULL) {
                switch(blank_mode)
                {
                        case FB_BLANK_POWERDOWN:
                        case FB_BLANK_NORMAL:
                                pm_runtime_put_sync(pdev);
                                break;

                        case FB_BLANK_UNBLANK:
                                if(pdev->power.usage_count.counter == 1) {
                                /*
                                 * usage_count = 1 ( resume ), blank_mode = 0 ( FB_BLANK_UNBLANK ) means that
                                 * this driver is stable state when booting. don't call runtime_suspend or resume state  */
                                } else {
                	                pm_runtime_get_sync(pdev);
                                }
                                break;
                        case FB_BLANK_HSYNC_SUSPEND:
                        case FB_BLANK_VSYNC_SUSPEND:
                        default:
                                ret = -EINVAL;
                }
        }
        return ret;
}
#endif

static long
dwc_hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
        long ret = -EFAULT;
        struct hdmi_tx_dev *dev = (struct hdmi_tx_dev *)file->private_data;

	if(dev == NULL){
		pr_err("%s:hdmi_tx_dev device is NULL\n", FUNC_NAME);
                ret = -EINVAL;
                goto end_process;
	}

	switch(cmd){
	#if defined(DEBUG_HDMI_LINK)
	case FB_HDMI_CORE_READ:
                {
                        dwc_hdmi_ioctl_data hdmi_data;
        		if(copy_from_user(&hdmi_data, (void __user *)arg, sizeof(dwc_hdmi_ioctl_data))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
        		hdmi_data.value = hdmi_dev_read(dev, hdmi_data.address);

        		if(copy_to_user((void __user *)arg, &hdmi_data, sizeof(dwc_hdmi_ioctl_data))) {
                                pr_err("%s:READ:  reg 0x%08x [-EIO]\n", FUNC_NAME, hdmi_data.address);
                                break;
                        }
                        ret = 0;
	        }
		break;

	case FB_HDMI_CORE_WRITE:
                {
                        dwc_hdmi_ioctl_data hdmi_data;
        		if(copy_from_user(&hdmi_data, (void __user *)arg, sizeof(dwc_hdmi_ioctl_data)) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
        		}
        		if(hdmi_dev_write(dev, hdmi_data.address, hdmi_data.value)) {
                                ret = -EIO;
                                pr_err("%s:WRITE: reg 0x%08x [-EIO]\n", __func__, hdmi_data.address);
        			break;
        		}
                        if(copy_to_user((void __user *)arg, &hdmi_data, sizeof(dwc_hdmi_ioctl_data))) {
                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        ret = 0;
	        }
		break;
	#endif


        case FB_HDMI_GET_HDCP22:
                if(copy_to_user((void __user *)arg, &dev->hdcp22, sizeof(dev->hdcp22))) {
                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                        break;
                }
                ret = 0;
                break;

        case HDMI_TIME_PROFILE:
                {
                        #if defined(CONFIG_TCC_HDMI_TIME_PROFILE)
                        int time_diff;
                        struct timeval curr_val;
                        char *time_log = kmalloc(100, GFP_KERNEL);

                        if(time_log != NULL) {
                                if(copy_from_user(&time_log, (void __user *)arg, 100)) {
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                do_gettimeofday(&curr_val);
                                if(time_log[0] == 0) {
                                        /* 0 means start */
                                        memcpy(&dev->time_backup, &curr_val, sizeof(curr_val));
                                        pr_info(" >> %s\r\n", time_log+1);
                                } else {
                                        /* 1 means stop */
                                        time_diff = dwc_hdmi_get_time(&dev->time_backup, &curr_val);
                                        pr_info(" >> [%d]ms %s\r\n", time_diff, time_log+1);
                                }

                                kfree(time_log);
                        }
                        #endif
                        ret = 0;
                }
                break;

        case HDMI_DRV_LOG:
                {
                        struct {
                                char* data;
                                int   len;
                        }log_data;

                        char* tmp_pointer;

                        if(copy_from_user(&log_data, (void __user *)arg, sizeof(log_data))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        tmp_pointer = (char __user *)log_data.data;
                        log_data.data = memdup_user(tmp_pointer, log_data.len);
                        if (IS_ERR_OR_NULL(log_data.data)) {
                                ret = PTR_ERR(log_data.data);
                                pr_err("HDMI_DRV_LOG memdup_user failed (ret=%d)\r\n", (int)ret);
                                break;
                        }
                        pr_info("%s\r\n", log_data.data);
                        if(log_data.data != NULL)
                                kfree(log_data.data);
                        ret = 0;
                }
                break;

        case HDMI_GET_PWR_STATUS:
                {
                        unsigned int data;
                        mutex_lock(&dev->mutex);
                        data = test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status);
                        if(copy_to_user((void __user *)arg, &data, sizeof(unsigned int))) {
                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                mutex_unlock(&dev->mutex);
                                break;
                        }
                        ret = 0;
                        mutex_unlock(&dev->mutex);
                }
                break;

        case HDMI_SET_PWR_CONTROL:
                {
                        unsigned int data ;
                        if(copy_from_user(&data, (void __user *)arg, sizeof(unsigned int))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        mutex_lock(&dev->mutex);
                        if(dev->verbose >= VERBOSE_IO)
                                pr_info("HDMI_SET_PWR_CONTROL <%d>\r\n", data);

                        switch(data) {
                                case 0:
                                        /* HDMI_AND_LCDC_OFF */
                                        clear_bit(HDMI_TX_STATUS_PHY_ALIVE, &dev->status);
                                        dwc_hdmi_power_off(dev);
                                        break;
                                case 1:
                                        /* HDMI_AND_LCDC_ON */
                                        clear_bit(HDMI_TX_STATUS_PHY_ALIVE, &dev->status);
                                        dwc_hdmi_power_on(dev);
                                        break;
                                case 2:
                                        /* HDMI_OFF */
                                        set_bit(HDMI_TX_STATUS_PHY_ALIVE, &dev->status);
                                        dwc_hdmi_link_power_off(dev);
                                        break;
                                case 3:
                                        /* HDMI_ON */
                                        set_bit(HDMI_TX_STATUS_PHY_ALIVE, &dev->status);
                                        dwc_hdmi_link_power_on(dev);
                                        break;
                        }
                        if(dev->verbose >= VERBOSE_IO)
                                pr_info("HDMI_SET_PWR_CONTROL done\r\n");
                        ret = 0;
                        mutex_unlock(&dev->mutex);
                }
                break;

        case HDMI_GET_SUSPEND_STATUS:
                {
                        unsigned int data;
                        mutex_lock(&dev->mutex);
                        data = dwc_hdmi_is_suspended(dev);
                        if(copy_to_user((void __user *)arg, &data, sizeof(unsigned int))) {
                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                mutex_unlock(&dev->mutex);
                                break;
                        }
                        ret = 0;
                        mutex_unlock(&dev->mutex);
                }
                break;

        case HDMI_GET_OUTPUT_ENABLE_STATUS:
                {
                        unsigned int data;
                        mutex_lock(&dev->mutex);
                        data = test_bit(HDMI_TX_STATUS_OUTPUT_ON, &dev->status);
                        if(copy_to_user((void __user *)arg, &data, sizeof(unsigned int))) {
                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                mutex_unlock(&dev->mutex);
                                break;
                        }
                        ret = 0;
                        mutex_unlock(&dev->mutex);
                }
                break;

        case HDMI_IOC_BLANK:
                {
                        #ifdef CONFIG_PM
                        int data ;
                        if(copy_from_user(&data, (void __user *)arg, sizeof(int))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        ret = hdmi_tx_blank(dev, data);
                        #endif
                }
                break;

        case HDMI_GET_SOC_FEATURES:
                {
                        hdmi_soc_features soc_features;
                        hdmi_get_soc_features(dev, &soc_features);
                        if(copy_to_user((void __user *)arg, &soc_features, sizeof(soc_features))) {
                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
				break;
                        }
			ret = 0;
                }
                break;

        case HDMI_GET_BOARD_FEATURES:
                        {
                                hdmi_board_features board_features;
                                hdmi_get_board_features(dev, &board_features);
                                if(copy_to_user((void __user *)arg, &board_features, sizeof(board_features))) {
                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                ret = 0;
                        }
                        break;

        case HDMI_STORE_DOLBYVISION_VSIF_LIST:
                {
                        hdmi_dolbyvision_vsif_transfer_data transfer_data;

                        if(copy_from_user(&transfer_data, (void __user *)arg, sizeof(transfer_data))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }

                        if(transfer_data.size != 300) {
                                break;
                        }

                        #if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
                        if(dev->dolbyvision_visf_list != NULL) {
                                devm_kfree(dev->parent_dev, dev->dolbyvision_visf_list);
                        }
                        dev->dolbyvision_visf_list = devm_kmalloc(dev->parent_dev, transfer_data.size, GFP_KERNEL);
                        if (IS_ERR_OR_NULL(dev->dolbyvision_visf_list)) {
                                ret = PTR_ERR(dev->dolbyvision_visf_list);
                                dev->dolbyvision_visf_list = NULL;
                                pr_err("HDMI_STORE_DOLBYVISION_VSIF_LIST memdup_user failed (ret=%d)\r\n", (int)ret);
                                break;
                        }
                        memcpy(dev->dolbyvision_visf_list, transfer_data.vsif_list, transfer_data.size);
                        #endif // CONFIG_VIOC_DOLBY_VISION_EDR
                        ret = 0;
                }
                break;
	case HDMI_GET_DTD_INFO:
                {
                        dwc_hdmi_dtd_data dtd_param;
                        if(copy_from_user(&dtd_param, (void __user *)arg, sizeof(dtd_param))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        if(hdmi_dtd_fill(&dtd_param.dtd, dtd_param.code, dtd_param.refreshRate)) {
                                pr_err("%s invalid code (%d) and (%d)hz\r\n", __func__, dtd_param.code, dtd_param.refreshRate);
                                break;
                        }
                        if(copy_to_user((void __user *)arg, &dtd_param, sizeof(dtd_param))) {
                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        ret = 0;
	        }
                break;


        // DDC
        case HDMI_DDC_WRITE_DATA:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                unsigned char *data_ptrs;
                                dwc_hdmi_ddc_transfer_data transfer_data;
                                if(copy_from_user(&transfer_data, (void __user *)arg, sizeof(transfer_data))) {
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        mutex_unlock(&dev->mutex);
                                        break;

                                }
                                data_ptrs = memdup_user((u8 __user *)transfer_data.data, transfer_data.len);
                                if (IS_ERR_OR_NULL(data_ptrs)) {
                                        ret = PTR_ERR(data_ptrs);
                                        pr_err("HDMI_DDC_WRITE_DATA memdup_user failed (ret=%d)\r\n", (int)ret);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                ret = hdmi_ddc_write(dev, transfer_data.i2cAddr, transfer_data.addr, transfer_data.len, transfer_data.data);
                                kfree(data_ptrs);
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                } else {
                        pr_err("## Failed to hdmi_ddc_write because hdmi linke was suspended \r\n");
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_DDC_READ_DATA:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                unsigned char* data_ptrs;
                                dwc_hdmi_ddc_transfer_data transfer_data;
                                if(copy_from_user(&transfer_data, (void __user *)arg, sizeof(transfer_data))) {
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                data_ptrs = devm_kmalloc(dev->parent_dev, transfer_data.len, GFP_KERNEL);
                                if (IS_ERR_OR_NULL(data_ptrs)) {
                                        ret = PTR_ERR(data_ptrs);
                                        pr_err("HDMI_DDC_READ_DATA memdup_user failed (ret=%d)\r\n", (int)ret);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                ret = hdmi_ddc_read(dev, transfer_data.i2cAddr, transfer_data.segment, transfer_data.pointer, transfer_data.addr, transfer_data.len, data_ptrs);
                                if (ret < 0) {
                                        switch(transfer_data.i2cAddr) {
                                                case 0x50:
							/* HDMI EDID */
                                                        pr_err("## Failed to edid read\r\n");
                                                        break;
                                                case 0x74:
							/* HDMI HDCP */
                                                        pr_err("## Failed to hdcp read\r\n");
                                                        break;
                                                default:
                                                        pr_err("## Failed to i2c unknown(0x%x) read\r\n", transfer_data.i2cAddr);
                                                        break;
                                        }
                                        devm_kfree(dev->parent_dev, data_ptrs);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                if(copy_to_user(transfer_data.data, data_ptrs, transfer_data.len)) {
                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        devm_kfree(dev->parent_dev, data_ptrs);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                devm_kfree(dev->parent_dev, data_ptrs);
                                ret = 0;
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                } else {
                        pr_err("## Failed to hdmi_ddc_read because hdmi linke was suspended \r\n");
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_SCDC_GET_SINK_VERSION:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                unsigned int version;
                                ret = scdc_read_sink_version(dev, &version);
                                if (ret < 0) {
                                        pr_err("## Failed to scdc_read_sink_version\r\n");
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                if(copy_to_user((void __user *)arg, &version, sizeof(version))) {
                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                ret = 0;
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                } else {
                        pr_err("## Failed to scdc_read_sink_version because hdmi linke was suspended \r\n");
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_SCDC_GET_SOURCE_VERSION:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                unsigned int version;
                                ret = scdc_read_source_version(dev, &version);
                                if (ret < 0) {
                                        pr_err("## Failed to scdc_read_source_version\r\n");
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                if(copy_to_user((void __user *)arg, &version, sizeof(version))) {
                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                ret = 0;
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                } else {
                        pr_err("## Failed to scdc_read_source_version because hdmi linke was suspended \r\n");
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_SCDC_SET_SOURCE_VERSION:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
				unsigned int version;
				if(copy_from_user(&version, (void __user *)arg, sizeof(unsigned int))) {
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        mutex_unlock(&dev->mutex);
                                        break;
				}
                                ret = scdc_write_source_version(dev, version);
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                } else {
                        pr_err("## Failed to scdc_write_source_version because hdmi linke was suspended \r\n");
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_DDC_BUS_CLEAR:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                hdmi_i2cddc_bus_clear(dev);
                                ret = 0;
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                } else {
                        pr_err("## Failed to scdc_write_source_version because hdmi linke was suspended\r\n");
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_VIDEO_SET_TMDS_CONFIG_INIT:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                ret = scdc_set_tmds_bit_clock_ratio_and_scrambling(dev, 0, 0);
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_VIDEO_SET_SCRAMBLING:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                int enable;
                                if(copy_from_user(&enable, (void __user *)arg, sizeof(int))) {
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                scrambling(dev, (uint8_t)enable);
                                ret = 0;
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_VIDEO_GET_SCRAMBLE_STATUS:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                int enable = scdc_scrambling_status(dev);
                                if(copy_to_user((void __user *)arg, &enable, sizeof(enable))) {
                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                ret = 0;
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_VIDEO_GET_SCRAMBLE_WAIT_TIME:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                int wait_time_ms = scrambling_get_wait_time();
                                if(copy_to_user((void __user *)arg, &wait_time_ms, sizeof(wait_time_ms))) {
                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                ret = 0;
                        }
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_VIDEO_GET_TMDS_CONFIG_STATUS:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                int tmds_config_status = scdc_tmds_config_status(dev);
                                if(copy_to_user((void __user *)arg, &tmds_config_status, sizeof(tmds_config_status))) {
                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        mutex_unlock(&dev->mutex);
                                        break;
                                }
                                ret = 0;
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_VIDEO_GET_ERROR_DETECTION:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                if(dev->hotplug_status) {
                                        struct hdmi_scdc_error_data hdmi_scdc_error_data;
                                        memset(&hdmi_scdc_error_data, 0, sizeof(hdmi_scdc_error_data));
                                        ret = scdc_error_detection(dev, &hdmi_scdc_error_data);
                                        if(!ret && (void __user *)arg != NULL) {
                                                if(copy_to_user((void __user *)arg, &hdmi_scdc_error_data, sizeof(struct hdmi_scdc_error_data))) {
                                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                        mutex_unlock(&dev->mutex);
                                                        break;
                                                }
                                                ret = 0;
                                        }
                                }
                        }else {
                                //pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                }
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_AUDIO_INIT:
                mutex_lock(&dev->mutex);
                if(!dwc_hdmi_is_suspended(dev)) {
                        if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                		ret = audio_Initialize(dev);
                		pr_info("HDMI Audio Init!!!\n");
                        }else {
                                pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                        }
                } else {
                        pr_err("## Failed to audio_Initialize because hdmi linke was suspended\r\n");
                }
                mutex_unlock(&dev->mutex);
		break;

	case HDMI_AUDIO_CONFIG:
                {
                        audioParams_t   audioParam;
                        if(copy_from_user(&audioParam, (void __user *)arg, sizeof(audioParams_t))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }

                        mutex_lock(&dev->mutex);
                        if(!dwc_hdmi_is_suspended(dev)) {
                                if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                        memcpy(dev->audioParam, &audioParam, sizeof(audioParam));
                                        ret = audio_Configure(dev, &audioParam);
                		}else {
                                        pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                                }
                        } else {
                                pr_err("## Failed to audio_Configure because hdmi linke was suspended\r\n");
                        }
                        mutex_unlock(&dev->mutex);
	        }
		break;

        case HDMI_HPD_SET_ENABLE:
                {
                        int hotplug_irq_enable;
                        if(copy_from_user(&hotplug_irq_enable, (void __user *)arg, sizeof(hotplug_irq_enable))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }

                        mutex_lock(&dev->mutex);
                        if(!dwc_hdmi_is_suspended(dev)) {
                                if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                        if(dev->hotplug_irq_enable != hotplug_irq_enable) {
                                                dev->hotplug_irq_enable = hotplug_irq_enable;
                                                hdmi_hpd_enable(dev);
                                        }
                                        ret = 0;
                                }else {
                                        pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                                }
                        }else {
                                pr_err("## Failed to hdmi_hpd_enable because hdmi linke was suspended\r\n");
                        }
			dev->hotplug_irq_enable = hotplug_irq_enable;
                        mutex_unlock(&dev->mutex);
                }
                break;

        case HDMI_HPD_GET_STATUS:
                mutex_lock(&dev->mutex);
                if(copy_to_user((void __user *)arg, &dev->hotplug_status, sizeof(int))) {
                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                        mutex_unlock(&dev->mutex);
                        break;
                }
                ret = 0;
                mutex_unlock(&dev->mutex);
                break;

        case HDMI_SET_VENDOR_INFOFRAME:
                {
                        productParams_t productParams;
                        if(copy_from_user(&productParams, (void __user *)arg, sizeof(productParams_t))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }

                        mutex_lock(&dev->mutex);
                        if(!dwc_hdmi_is_suspended(dev)) {

                                if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                        ret = vendor_Configure(dev, &productParams);
                                } else {
                                        pr_err(" HDMI is not powred <%d>\r\n", __LINE__);

                                }
                        } else {
                                pr_err("## Failed to vendor_Configure because hdmi linke was suspended\r\n");
                        }
                        mutex_unlock(&dev->mutex);
                }
                break;

        case HDMI_SET_AV_MUTE:
                {
                        void fc_force_output(struct hdmi_tx_dev *dev, int enable);
                        unsigned int magic;
                        if(copy_from_user(&magic, (void __user *)arg, sizeof(unsigned int))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }

                        BUG_ON((magic & 0xFFFFFFF0) != HDMI_AV_MUTE_MAGIC);

                        mutex_lock(&dev->mutex);
                        hdmi_api_avmute(dev, (magic & 1));
                        ret = 0;
                        mutex_unlock(&dev->mutex);
                }
                break;

        case HDMI_API_GET_NEED_PRE_CONFIG:
                {
                        int pre_config = 0;
                        if(copy_to_user((void __user *)arg, &pre_config, sizeof(int))) {
                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                mutex_unlock(&dev->mutex);
                                break;
                        }
                        ret = 0;
                }
                break;

        case HDMI_API_CONFIG:
        case HDMI_API_CONFIG_EX:
                {
                        size_t api_size = sizeof(dwc_hdmi_api_data);
                        dwc_hdmi_api_ex_data api_data;
                        if(cmd == HDMI_API_CONFIG_EX) {
                                api_size = sizeof(dwc_hdmi_api_ex_data);
                        }

                        if(copy_from_user(&api_data, (void __user *)arg, api_size)) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        // Need Validate Check.!!
                        memcpy(dev->videoParam, &api_data.videoParam, sizeof(videoParams_t));
                        memcpy(dev->audioParam, &api_data.audioParam, sizeof(audioParams_t));
                        memcpy(dev->productParam, &api_data.productParam, sizeof(productParams_t));

                        /* In general, HDCP Keepout is set to 1 when TMDS character rate is higher
			 * than 340 MHz or when HDCP is enabled.
			 * If HDCP Keepout is set to 1 then the control period configuration is changed
			 * in order to supports scramble and HDCP encryption. But some SINK needs this
			 * packet configuration always even if HDMI ouput is not scrambled or HDCP is
			 * not enabled. */
                        dev->hdmi_tx_ctrl.sink_need_hdcp_keepout = 0;
                        if(cmd == HDMI_API_CONFIG_EX) {
				if(api_data.api_extension.sink_manufacture == 1) {
					/* VIZIO */
					dev->hdmi_tx_ctrl.sink_need_hdcp_keepout = 1;
				}
				if(api_data.api_extension.param0 & (1 << 0)) {
					dev->hdmi_tx_ctrl.sink_need_hdcp_keepout = 1;
                                }
                        }

                        mutex_lock(&dev->mutex);
                        dev->hdmi_tx_ctrl.pixel_clock = hdmi_phy_get_actual_tmds_bit_ratio_by_videoparam(dev, (videoParams_t*) dev->videoParam);
			ret = hdmi_api_Configure(dev);
                        #if defined(CONFIG_PLATFORM_AVN)
                    	schedule_work(&dev->hdmi_output_event_work);
                        #endif
                        mutex_unlock(&dev->mutex);
                }
                break;

        case HDMI_API_DISABLE:
                {
                        unsigned int magic;
                        if(copy_from_user(&magic, (void __user *)arg, sizeof(unsigned int))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        BUG_ON(magic != HDMI_API_DISABLE_MAGIC);
                        mutex_lock(&dev->mutex);
                        pr_info(" >> HDMI_API_DISABLE\r\n");
			ret = hdmi_api_Disable(dev);
			#if defined(CONFIG_PLATFORM_AVN)
			schedule_work(&dev->hdmi_output_event_work);
			#endif
                        mutex_unlock(&dev->mutex);
                }
                break;

	case HDMI_API_PHY_MASK:
		{
			unsigned int magic, val;
			if(copy_from_user(&magic, (void __user *)arg, sizeof(unsigned int))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
			}
			BUG_ON((magic & HDMI_API_PHY_MASK_MAGIC) != HDMI_API_PHY_MASK_MAGIC);

			mutex_lock(&dev->mutex);
                        if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &dev->status)) {
                                if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
                                        val = (magic & ~HDMI_API_PHY_MASK_MAGIC);
                                        dwc_hdmi_phy_mask(dev, val);
                                        ret = 0;
                                } else {
                                        pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                                }
                        } else {
                                pr_err("## Failed to dwc_hdmi_phy_mask because hdmi linke was suspended\r\n");
                        }
			mutex_unlock(&dev->mutex);
		}
		break;

	case HDMI_API_DRM_CONFIG:
		{
			DRM_Packet_t DRMParm;
                        if(copy_from_user(&DRMParm, (void __user *)arg, sizeof(DRM_Packet_t))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        mutex_lock(&dev->mutex);
                        hdmi_update_drm_configure(dev, &DRMParm);
                        ret = 0;
                        mutex_unlock(&dev->mutex);
		}
		break;

	case HDMI_API_DRM_SET:
		{
                        mutex_lock(&dev->mutex);
                        hdmi_apply_drm(dev);
                        ret = 0;
                        mutex_unlock(&dev->mutex);
		}
		break;

        case HDMI_API_DRM_CLEAR:
                {
                        mutex_lock(&dev->mutex);
                        hdmi_clear_drm(dev);
                        ret = 0;
                        mutex_unlock(&dev->mutex);
                }
                break;

        case HDMI_API_DRM_GET_VALID:
                {
                        int valid = 0;
                        mutex_lock(&dev->mutex);
                        if(test_bit(HDMI_TX_HDR_VALID, &dev->status)) {
                                // HDR
                                valid = 2;
                        }
                        else if(test_bit(HDMI_TX_HLG_VALID, &dev->status)) {
                                // HLG
                                valid = 3;
                        }
                        //printk("drm valid=%d\r\n", valid);
                        if(copy_to_user((void __user *)arg, &valid, sizeof(valid))) {
                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                mutex_unlock(&dev->mutex);
                                break;
                        }
                        ret = 0;
                        mutex_unlock(&dev->mutex);
                }
                break;

	case HDMI_API_SET_VSIF_UPDATE_HDR_10P:

		{
			int update;
                        if(copy_from_user(&update, (void __user *)arg, sizeof(int))) {
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        mutex_lock(&dev->mutex);
			if(update == 0) {
				clear_bit(HDMI_TX_VSIF_UPDATE_FOR_HDR_10P, &dev->status);
			} else {
				set_bit(HDMI_TX_VSIF_UPDATE_FOR_HDR_10P, &dev->status);
			}
                        ret = 0;
                        mutex_unlock(&dev->mutex);
		}
		break;

	case HDMI_API_GET_DRM_CONFIG:
		{
			if(dev->drmParm == NULL) {
				pr_err("%s drmParm is NULL at line(%d)\r\n", __func__, __LINE__);
				break;
			}
			mutex_lock(&dev->mutex);
			if(copy_to_user((void __user *)arg, dev->drmParm, sizeof(DRM_Packet_t))) {
                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                mutex_unlock(&dev->mutex);
                                break;
                        }
                        ret = 0;
			mutex_unlock(&dev->mutex);
		}
		break;

	case HDCP22_CTRL_REG_RESET:
		{
			mutex_lock(&dev->mutex);
			if(!dwc_hdmi_is_suspended(dev)) {
				if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
					_HDCP22CtrlRegReset(dev);
				} else {
                                        pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                                }
			} else {
                                pr_err("## Failed to hdcp22ctrl reg reset because hdmi linke was suspended\r\n");
                        }
			ret = 0;
			mutex_unlock(&dev->mutex);
		}
		break;

	case HDMI_GET_PHY_RX_SENSE_STATUS:
		{
			mutex_lock(&dev->mutex);
			if(!dwc_hdmi_is_suspended(dev)) {
				if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
					int rx_sense = hdmi_phy_get_rx_sense_status(dev);
					ret = copy_to_user((void __user *)arg, &rx_sense, sizeof(rx_sense));
				} else {
                                        pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                                }
			}else {
                                pr_err("## Failed to hdmi_phy_get_rxsense because hdmi linke was suspended\r\n");
                        }
			mutex_unlock(&dev->mutex);
		}
		break;

	case HDMI_GET_HDCP22_STATUS:
		{
			mutex_lock(&dev->mutex);
			if(!dwc_hdmi_is_suspended(dev)) {
				if(test_bit(HDMI_TX_STATUS_POWER_ON, &dev->status)) {
					int hdcp22_status = _HDCP22RegStatusRead(dev);
					ret = copy_to_user((void __user *)arg, &hdcp22_status, sizeof(hdcp22_status));
				} else {
                                        pr_err(" HDMI is not powred <%d>\r\n", __LINE__);
                                }
			}else {
                                pr_err("## Failed to hdcp22_status because hdmi linke was suspended\r\n");
                        }
			mutex_unlock(&dev->mutex);
		}
		break;

	default:
		if (dev->verbose >= VERBOSE_IO)
                        pr_err("%s:IOCTL (0x%x) is unknown!\n", FUNC_NAME, cmd);
		break;
	}

end_process:
	return ret;
}

#if defined(CONFIG_COMPAT)
static long dwc_hdmi_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
        return dwc_hdmi_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int
dwc_hdmi_open(struct inode *inode, struct file *file) {

        struct miscdevice *misc = (struct miscdevice *)file->private_data;
        struct hdmi_tx_dev *dev = dev_get_drvdata(misc->parent);


        file->private_data = dev;


        dev->open_cs++;

        //dwc_hdmi_power_on(dev);

	return 0;
}

static int
dwc_hdmi_release(struct inode *inode, struct file *file){
        struct hdmi_tx_dev *dev = (struct hdmi_tx_dev *)file->private_data;
        if(dev->verbose)
                pr_info("%s: read function\n", FUNC_NAME);

        dev->open_cs--;

        //dwc_hdmi_power_off(dev);

	return 0;
}

static ssize_t
dwc_hdmi_read( struct file *file, char *buf, size_t count, loff_t *f_pos ){
        struct hdmi_tx_dev *dev = (struct hdmi_tx_dev *)file->private_data;
	if(dev->verbose)
		pr_info("%s: read function\n", FUNC_NAME);
	return 0;
}

static ssize_t
dwc_hdmi_write( struct file *file, const char *buf, size_t count, loff_t *f_pos ){
        struct hdmi_tx_dev *dev = (struct hdmi_tx_dev *)file->private_data;
	if(dev->verbose)
		pr_info("%s: write function\n", FUNC_NAME);
	return count;
}

static unsigned int
dwc_hdmi_poll(struct file *file, poll_table *wait){
        unsigned int mask = 0;
        struct hdmi_tx_dev *dev = (struct hdmi_tx_dev *)file->private_data;

        static int prev_hpd = -1;
        static int prev_hdr = -1;
        static int prev_hlg = -1;

        int current_drm;

        if(dev->verbose)
                pr_info("%s: poll function\n", FUNC_NAME);

        //printk("+dwc_hdmi_poll\r\n");
        poll_wait(file, &dev->poll_wq, wait);

        if(prev_hpd != dev->hotplug_status) {
                prev_hpd = dev->hotplug_status;
                mask = POLLIN;
        }
        current_drm = test_bit(HDMI_TX_HDR_VALID, &dev->status);
        if(prev_hdr != current_drm) {
                prev_hdr = current_drm;
                mask = POLLIN;
        }
        current_drm = test_bit(HDMI_TX_HLG_VALID, &dev->status);
        if(prev_hlg != current_drm) {
                prev_hlg = current_drm;
                mask = POLLIN;
        }

        //printk("-dwc_hdmi_poll\r\n");
        return mask;
}


static const struct file_operations dwc_hdmi_fops =
{
        .owner          = THIS_MODULE,
        .open           = dwc_hdmi_open,
        .release        = dwc_hdmi_release,
        .read           = dwc_hdmi_read,
        .write          = dwc_hdmi_write,
        .unlocked_ioctl = dwc_hdmi_ioctl,
        #if defined(CONFIG_COMPAT)
        .compat_ioctl   = dwc_hdmi_compat_ioctl,
        #endif
        .poll           = dwc_hdmi_poll,
};


/**
 * @short misc register routine
 * @param[in] dev pointer to the hdmi_tx_devstructure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
int
dwc_hdmi_misc_register(struct hdmi_tx_dev *dev) {
        int ret = 0;

        dev->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
        if(dev->misc == NULL) {
                ret = -1;
        }else {
                dev->misc->minor = MISC_DYNAMIC_MINOR;
                dev->misc->name = "dw-hdmi-tx";
                dev->misc->fops = &dwc_hdmi_fops;
                dev->misc->parent = dev->parent_dev;
                ret = misc_register(dev->misc);
        }
        if(ret < 0) {
                goto end_process;
        }
        dev_set_drvdata(dev->parent_dev, dev);

end_process:
        return ret;
}

/**
 * @short misc deregister routine
 * @param[in] dev pointer to the hdmi_tx_devstructure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
int
dwc_hdmi_misc_deregister(struct hdmi_tx_dev *dev) {
        if(dev->misc) {
                misc_deregister(dev->misc);
                kfree(dev->misc);
                dev->misc = 0;
        }

        return 0;
}

