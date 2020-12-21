/*
 * linux/drivers/video/tcc/tcc_vioc_fb.c
 *
 * Based on:    Based on s3c2410fb.c, sa1100fb.c and others
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC LCD Controller Frame Buffer Driver
 *
 * Copyright (C) 2008- Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/gpio.h>
#include <linux/irq.h>

#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#include <linux/memblock.h>
#include <linux/miscdevice.h>
#include <linux/export.h>
#include <linux/rbtree.h>
#include <linux/rtmutex.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/console.h>
#include <linux/uaccess.h>

#include <asm/io.h>
#include <asm/div64.h>
#include <asm/system_info.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#include <soc/tcc/pmap.h>

#include <video/tcc/vioc_config.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tcc_scaler_ctrl.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_composite_ioctl.h>
#include <video/tcc/tcc_component_ioctl.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_lut.h>
#include <video/tcc/tcc_wmixer_ioctrl.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/tca_display_config.h>
#include <video/tcc/vioc_global.h>
#ifdef TCC_VIDEO_DISPLAY_DEINTERLACE_MODE
#include "tcc_vioc_viqe_interface.h"
#include "viqe.h"
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#endif

#if defined(CONFIG_SYNC_FB)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
#include <sw_sync.h>
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
#include <linux/fence.h>
#else
#include <linux/dma-fence.h>
#endif
#include <linux/sync_file.h>
extern struct sync_timeline *sync_timeline_create(const char *name);
extern int sw_sync_create_fence(struct sync_timeline *obj, unsigned int value, int *fd);
extern void sw_sync_timeline_inc(struct sync_timeline *obj, unsigned int value);
#endif
#include <linux/file.h>
#endif
#include "tcc_vsync.h"

#ifdef CONFIG_VIDEO_TCC_VOUT
#include <video/tcc/tcc_vout_v4l2.h>
extern int tcc_vout_get_status( unsigned int type );

#ifdef CONFIG_VOUT_USE_VSYNC_INT
extern void tcc_vout_hdmi_end( unsigned int type );
extern void tcc_vout_hdmi_start( unsigned int type );
#endif
#endif

#define STAGE_FB               0
#define STAGE_OUTPUTSTARTER    1

#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
extern void hdmi_set_activate_callback(void(*)(int, int), int, int);
#endif // CONFIG_TCC_HDMI_DRIVER_V2_0

#if defined(CONFIG_LOGO_PRESERVE_WITHOUT_FB_INIT)
static unsigned int hdmi_base_address = 0;
extern int tca_vioc_displayblock_pre_ctrl_set(struct tcc_dp_device *dp_device);
#endif

extern void tcafb_activate_var(struct tccfb_info *fbi,  struct fb_var_screeninfo *var);
/* Debugging stuff */
static int debug = 0;
#define dprintk(msg...)	if (debug) { pr_info("[DBG][FB] " msg); }

static int screen_debug = 0;
#define sprintk(msg...)	if (screen_debug) { pr_info("[DBG][FB] " msg); }

#define FB_NUM_BUFFERS 3

#if defined(CONFIG_USE_DISPLAY_FB_LOCK)
unsigned int fb_lock = false;   //TO FORBID UPDATE
#endif

#ifdef CONFIG_TCC_SCREEN_SHARE
extern void tcc_scrshare_set_sharedBuffer(unsigned int addr, unsigned int frameWidth, unsigned int frameHeight, unsigned int fmt);
#endif

#ifdef CONFIG_HDMI_DISPLAY_LASTFRAME
extern void tcc_video_info_backup(VSYNC_CH_TYPE type, struct tcc_lcdc_image_update *input_image);
extern int tcc_video_check_last_frame(struct tcc_lcdc_image_update *ImageInfo);
#endif

extern void tca_vioc_displayblock_powerOn(struct tcc_dp_device *pDisplayInfo, int specific_pclk);
extern void tca_vioc_displayblock_powerOff(struct tcc_dp_device *pDisplayInfo);
extern void tca_vioc_displayblock_disable(struct tcc_dp_device *pDisplayInfo);
#if defined(CONFIG_ARCH_TCC)
extern void tca_vioc_displayblock_extra_set(struct tcc_dp_device *pDisplayInfo, struct  tcc_fb_extra_data *tcc_fb_extra_data);
#endif
extern void tca_vioc_displayblock_timing_set(unsigned int outDevice, struct tcc_dp_device *pDisplayInfo,  struct lcdc_timimg_parms_t *mode);
extern void tca_vioc_displayblock_ctrl_set(unsigned int outDevice, struct tcc_dp_device *pDisplayInfo, stLTIMING *pstTiming, stLCDCTR *pstCtrl);
extern int tca_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);
extern void tca_fb_activate_var(unsigned int dma_addr,  struct fb_var_screeninfo *var, struct tcc_dp_device *pdp_data);
extern void tca_scale_display_update(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo);
extern void tccfb1_set_par(struct tccfb_info *fbi,  struct fb_var_screeninfo *var);

extern unsigned int tca_fb_get_fifo_underrun_count(void);

#ifdef CONFIG_DIRECT_MOUSE_CTRL
extern void tca_fb_mouse_set_icon(tcc_mouse_icon *mouse_icon);
extern int tca_fb_mouse_move(unsigned int width, unsigned int height, tcc_mouse *mouse, struct tcc_dp_device *pdp_data);
#endif
extern void tca_fb_resize_set_value(tcc_display_resize resize_value, TCC_OUTPUT_TYPE output);
extern void tca_fb_output_attach_resize_set_value(tcc_display_resize resize_value);
extern int tca_fb_divide_set_mode(struct tcc_dp_device *pdp_data, char enable, char mode);

extern void tca_fb_attach_start(struct tccfb_info *info);
extern int tca_fb_attach_stop(struct tccfb_info *info);


extern int tca_fb_suspend(struct device *dev, struct lcd_panel *disp_panel, struct lcd_panel *ext_panel);
extern int tca_fb_resume(struct device *dev, struct lcd_panel *disp_panel, struct lcd_panel *ext_panel);
extern int tca_fb_init(struct tccfb_info *dev);
extern void tca_fb_exit(void);
extern int tca_main_interrupt_reg(char SetClear, struct tccfb_info *info);
extern void tca_vsync_enable(struct tccfb_info *dev, int on);
extern void tca_vioc_displayblock_clock_select(struct tcc_dp_device *pDisplayInfo, int clk_src_hdmi_phy);
#if defined(CONFIG_FB_TCC_COMPOSITE)
extern int tcc_composite_detect(void);
#endif

#if defined(CONFIG_FB_TCC_COMPONENT)
extern int tcc_component_detect(void);
#endif

extern int tcc_2d_compression_read(void);

#if defined(__CONFIG_HIBERNATION)
extern unsigned int do_hibernation;
extern void fb_quickboot_resume(struct tccfb_info *info);
#endif//CONFIG_HIBERNATION


#define CONFIG_FB_TCC_DEVS_MAX	3	// do not change!
#if defined(CONFIG_ANDROID) || !defined(CONFIG_FB1_SUPPORT)
#define CONFIG_FB_TCC_DEVS		1
#else
#define CONFIG_FB_TCC_DEVS		2
#endif//

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
extern int tca_edr_path_configure(void);
#endif

#if (CONFIG_FB_TCC_DEVS > CONFIG_FB_TCC_DEVS_MAX)
	#undef CONFIG_FB_TCC_DEVS
	#define CONFIG_FB_TCC_DEVS	CONFIG_FB_TCC_DEVS_MAX
#endif

#define SCREEN_DEPTH_MAX	32	// 32 or 16
//								 : 32 - 32bpp(alpah+rgb888)
//								 : 16 - 16bpp(rgb565)

const unsigned int default_scn_depth[CONFIG_FB_TCC_DEVS_MAX] =
{
/* fb0, Layer0: bottom */  (32), // 32 or 16
/* fb1, Layer1: middle */  (32), //  "
/* fb2, Layer2: top    */  (16)  //  "
};

#ifdef CONFIG_OF
static struct of_device_id tccfb_of_match[] = {
	{ .compatible = "telechips,vioc-fb" },
	{}
};
MODULE_DEVICE_TABLE(of, tccfb_of_match);
#endif

#ifdef CONFIG_MALI400_UMP
/*./gpu/mali/ump/include/ump/ump_kernel_interface.h
 *  Add MALI UMP interface in fbdev !! because using X display
 */
#include "../../../gpu/arm/mali400/ump/include/ump/ump_kernel_interface.h"
#define GET_UMP_SECURE_ID_BUF(x) _IOWR('m', 311 + (x), unsigned int)
extern ump_dd_handle ump_dd_handle_create_from_phys_blocks(ump_dd_physical_block * blocks, unsigned long  num_blocks);
static ump_dd_handle ump_wrapped_buffer[CONFIG_FB_TCC_DEVS_MAX][3];
#endif

static struct lcd_panel *lcd_panel;
static struct lcd_panel *display_ext_panel = NULL;

static int lcd_video_started = 0;

TCC_OUTPUT_TYPE	Output_SelectMode =  TCC_OUTPUT_NONE;


static char  HDMI_pause = 0;
static char HDMI_video_mode = 0;

unsigned int HDMI_video_width = 0;
unsigned int HDMI_video_height = 0;
unsigned int HDMI_video_hz = 0;

unsigned int fb_chromakey_control_enabled = 0;

#if defined(CONFIG_TCC_VTA)
extern int vta_cmd_notify_change_status(const char *);
#endif

extern void tca_fb_wait_for_vsync(struct tcc_dp_device *pdata);
extern void tca_fb_vsync_activate(struct tcc_dp_device *pdata);

extern int tcc_fb_swap_vpu_frame(struct tcc_dp_device *pdp_data,
						WMIXER_INFO_TYPE *WmixerInfo,
						struct tcc_lcdc_image_update *TempImage,
						VSYNC_CH_TYPE type);


#if defined(CONFIG_SYNC_FB)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
static void tcc_fd_fence_wait(struct sync_fence *fence)
#else
static void tcc_fd_fence_wait(struct fence *fence)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
 	int err = sync_fence_wait(fence, 1000);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	int err = fence_default_wait(fence, 1, 1000);
#else
	int err = dma_fence_default_wait((struct dma_fence *) fence, 1, 1000);
#endif
 	if (err >= 0)
 		return;

 	if (err == -ETIME)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
 		err = sync_fence_wait(fence, 10 * MSEC_PER_SEC);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	       err = fence_default_wait(fence, 1, 10 * MSEC_PER_SEC);
#else
	       err = dma_fence_default_wait((struct dma_fence *) fence, 1, 10 * MSEC_PER_SEC);
#endif
}

static void tcc_fb_update_regs(struct tccfb_info *tccfb, struct tcc_fenc_reg_data *regs)
{
	unsigned int BaseAddr;
	pm_runtime_get_sync(tccfb->fb->dev);

	if ((regs->fence_fd > 0) && (regs->fence))	{
		tcc_fd_fence_wait(regs->fence);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
 		sync_fence_put(regs->fence);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
		fence_put(regs->fence);
#else
		dma_fence_put((struct dma_fence *) regs->fence);
#endif
	}
#if defined(CONFIG_VIOC_AFBCDEC)
	if((system_rev!=0) && (regs->var.reserved[3]!=0))
		BaseAddr = tccfb->map_dma + regs->var.xres * regs->var.yoffset * (regs->var.bits_per_pixel/8) + regs->var.reserved[1];
	else
#endif
		BaseAddr = tccfb->map_dma + regs->var.xres * regs->var.yoffset * (regs->var.bits_per_pixel/8);

	#if !defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_STB)
	#if !defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_MID) && !defined(CONFIG_TCC_DISPLAY_MODE_USE)
	if(tccfb->pdata.Sdp_data.FbPowerState)
		tca_fb_activate_var(BaseAddr, &regs->var, &tccfb->pdata.Sdp_data);
	#endif
	#endif

	tca_fb_vsync_activate(&tccfb->pdata.Mdp_data);
	tccfb->fb->fbops->fb_pan_display(&regs->var, tccfb->fb);

	pm_runtime_put_sync(tccfb->fb->dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
	sw_sync_timeline_inc(tccfb->fb_timeline, 1);
#else
	sw_sync_timeline_inc((struct sync_timeline *)tccfb->fb_timeline, 1);
#endif
}

static void fence_handler(struct kthread_work *work)
{
	struct tcc_fenc_reg_data *data, *next;
	struct list_head saved_list;

	struct tccfb_info *tccfb =
			container_of(work, struct tccfb_info, fb_update_regs_work);

	mutex_lock(&tccfb->fb_timeline_lock);
	saved_list = tccfb->fb_update_regs_list;
	list_replace_init(&tccfb->fb_update_regs_list, &saved_list);

	list_for_each_entry_safe(data, next, &saved_list, list) {
		tcc_fb_update_regs(tccfb, data);
		list_del(&data->list);
		kfree(data);
	}
        mutex_unlock(&tccfb->fb_timeline_lock);
}

static void ext_fence_handler(struct kthread_work *work)
{
	int save_timeline_fb_max = 0, save_timeline_value = 0, i = 0;

	struct tccfb_info *tccfb =
			container_of(work, struct tccfb_info, ext_update_regs_work);

	mutex_lock(&tccfb->ext_timeline_lock);

	save_timeline_fb_max = tccfb->ext_timeline_max;
	save_timeline_value = tccfb->ext_timeline->value;

	mutex_unlock(&tccfb->ext_timeline_lock);

	pr_info("[INF][FB] ext_fence_handler\n");

	tca_fb_vsync_activate(&tccfb->pdata.Sdp_data);
//	TCC_OUTPUT_FB_UpdateSync(Output_SelectMode);
	for(i=save_timeline_value; i<save_timeline_fb_max; i++){
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
		sw_sync_timeline_inc(tccfb->fb_timeline, 1);
#else
		sw_sync_timeline_inc((struct sync_timeline *)tccfb->ext_timeline, 1);
#endif
	}
}
#endif

struct tcc_dp_device *tca_fb_get_displayType(TCC_OUTPUT_TYPE check_type)
{
	struct fb_info *info = registered_fb[0];
	struct tccfb_info *tccfb_info = NULL;
	struct tcc_dp_device *dp_device = NULL;

	if(!info)
		return NULL;

	tccfb_info = info->par;

	if(!tccfb_info)
		return NULL;

	if(tccfb_info->pdata.Mdp_data.DispDeviceType == check_type)
		dp_device = &tccfb_info->pdata.Mdp_data;
	else if(tccfb_info->pdata.Sdp_data.DispDeviceType == check_type)
		dp_device = &tccfb_info->pdata.Sdp_data;

	return dp_device;
}
EXPORT_SYMBOL(tca_fb_get_displayType);

static void send_vsync_event(struct work_struct *work)
{
	char buf[64];
	char *envp[] = { NULL, NULL };
	struct tccfb_info *fbdev = container_of(work, struct tccfb_info, vsync_work);

	snprintf(buf, sizeof(buf), "VSYNC=%llu", ktime_to_ns(fbdev->vsync_timestamp));
	envp[0] = buf;
	kobject_uevent_env(&fbdev->dev->kobj, KOBJ_CHANGE, envp);

	dprintk("%s %s\n",__func__, buf );
}

void tccfb_extoutput_activate(int fb, int stage)
{
	unsigned int BaseAddr = 0;

	struct fb_info *info = registered_fb[fb];
	struct tccfb_info *ptccfb_info = info->par;
	struct tcc_dp_device *pdp_data = NULL;

	if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
		pdp_data = &ptccfb_info->pdata.Mdp_data;
	else if(ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
		pdp_data = &ptccfb_info->pdata.Sdp_data;
	else {
		if (stage == STAGE_OUTPUTSTARTER) {
			pdp_data = &ptccfb_info->pdata.Mdp_data;
			pr_info("[INF][FB] tccfb_extoutput_activate : Output(%d)\n", ptccfb_info->pdata.Mdp_data.DispDeviceType);
		} else {
			pr_err("[INF][FB] tccfb_extoutput_activate : can't find HDMI voic display block \n");
			}
	}

	if (pdp_data == NULL) {
		pr_err("[ERR][FB] %s: pdp_data is null\n", __func__);
		return;
	}

	#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if(pdp_data->DispDeviceType == TCC_OUTPUT_HDMI) {
		tca_edr_path_configure();
	}
	#endif

    	if(pdp_data != NULL && pdp_data->FbPowerState) {
		if(stage == STAGE_OUTPUTSTARTER) {
			#if defined(CONFIG_LOGO_PRESERVE_WITHOUT_FB_INIT)
			if(hdmi_base_address != 0) {
				BaseAddr = hdmi_base_address;
				pdp_data->FbPowerState = 0;
				pr_info("[INF][FB] Load bootlogo from VIQE memory at 0x%x\r\n", hdmi_base_address);
			} else
			#endif
			BaseAddr = ptccfb_info->map_dma + ptccfb_info->fb->var.xres *  ptccfb_info->fb->var.yoffset * ( ptccfb_info->fb->var.bits_per_pixel/8);
			tca_fb_activate_var(BaseAddr, &ptccfb_info->fb->var, pdp_data);

			#if defined(TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
			tcc_vsync_reset_all();
			#endif
		}
		else {
			#if defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_MID)
			BaseAddr = ptccfb_info->map_dma + ptccfb_info->fb->var.xres *  ptccfb_info->fb->var.yoffset * ( ptccfb_info->fb->var.bits_per_pixel/8);
			tca_fb_activate_var(BaseAddr, &ptccfb_info->fb->var, pdp_data);
			#else
			BaseAddr = ptccfb_info->map_dma + ptccfb_info->fb->var.xres * ptccfb_info->fb->var.yoffset * ( ptccfb_info->fb->var.bits_per_pixel/8);
			if((pdp_data->FbBaseAddr != (unsigned int)0) && !(pdp_data->DispOrder == DD_MAIN))
				tca_fb_activate_var(pdp_data->FbBaseAddr, &ptccfb_info->fb->var, pdp_data);
			else
				tca_fb_activate_var(BaseAddr, &ptccfb_info->fb->var, pdp_data);
			#endif//

			#ifdef TCC_VIDEO_DISPLAY_BY_VSYNC_INT
			//Disable video path of main display.
			pdp_data = &ptccfb_info->pdata.Mdp_data;
			#ifdef CONFIG_ANDROID // For Video Display using TCC_LCDC_HDMI_DISPLAY
			tcc_vsync_hdmi_start(pdp_data, &lcd_video_started);
			#else
			#ifdef CONFIG_VIDEO_TCC_VOUT
			if(tcc_vout_get_status(pdp_data->ddc_info.blk_num) != TCC_VOUT_RUNNING)
			#endif
			{
				if(tcc_vsync_isVsyncRunning(VSYNC_MAIN) || tcc_vsync_isVsyncRunning(VSYNC_SUB0))
				tcc_vsync_hdmi_start(pdp_data, &lcd_video_started);
			}
			#endif // CONFIG_ANDROID
			#endif // TCC_VIDEO_DISPLAY_BY_VSYNC_INT

			#ifdef CONFIG_VOUT_USE_VSYNC_INT
			if(tcc_vout_get_status(pdp_data->ddc_info.blk_num) == TCC_VOUT_RUNNING)
				tcc_vout_hdmi_start(pdp_data->ddc_info.blk_num);
			#endif // CONFIG_VOUT_USE_VSYNC_INT
		}
	}
}
EXPORT_SYMBOL(tccfb_extoutput_activate);

void tccfb_output_starter(char output_type, char lcdc_num, stLTIMING *pstTiming, stLCDCTR *pstCtrl, int specific_pclk)
{
	struct fb_info *info;
	struct tccfb_info *ptccfb_info =NULL;
	struct tcc_dp_device *pdp_data =NULL;
        int skip_display_device = 0;

	info = registered_fb[0];
	ptccfb_info = info->par;

	if(ptccfb_info == NULL)
		goto error_null_pointer;

        if(pstTiming == NULL || pstCtrl == NULL) {
                skip_display_device = 1;
        }

	if(ptccfb_info->pdata.Mdp_data.DispNum == lcdc_num)
		pdp_data = &ptccfb_info->pdata.Mdp_data;
	else if(ptccfb_info->pdata.Sdp_data.DispNum == lcdc_num)
		pdp_data = &ptccfb_info->pdata.Sdp_data;
	else
		goto error_null_pointer;

	switch(output_type)
	{
		case TCC_OUTPUT_HDMI:
			pdp_data->DispDeviceType = TCC_OUTPUT_HDMI;
			pdp_data->FbUpdateType = FB_SC_RDMA_UPDATE;
                        tca_vioc_displayblock_powerOn(pdp_data, skip_display_device);

			if(!skip_display_device) {
				// prevent under-run
                                tca_vioc_displayblock_disable(pdp_data);
			}

			#if defined(CONFIG_LOGO_PRESERVE_WITHOUT_FB_INIT)
			if(skip_display_device) {
				if(tca_vioc_displayblock_pre_ctrl_set(pdp_data) == 0) {
					pr_info("[INF][FB] %s skip because display block status is same..\r\n", __func__);
					break;
				}
			}
			#endif
			pr_info("[INF][FB] %s set display block \r\n", __func__);
			tca_vioc_displayblock_ctrl_set(VIOC_OUTCFG_HDMI, pdp_data, pstTiming, pstCtrl);
			break;

		case TCC_OUTPUT_COMPOSITE:
			pdp_data->DispDeviceType = TCC_OUTPUT_COMPOSITE;
			pdp_data->FbUpdateType = FB_SC_RDMA_UPDATE;
			tca_vioc_displayblock_powerOn(pdp_data, specific_pclk);
			tca_vioc_displayblock_ctrl_set(VIOC_OUTCFG_SDVENC, pdp_data, pstTiming, pstCtrl);
			break;

		case TCC_OUTPUT_COMPONENT:
			pdp_data->DispDeviceType = TCC_OUTPUT_COMPONENT;
			pdp_data->FbUpdateType = FB_SC_RDMA_UPDATE;
			tca_vioc_displayblock_powerOn(pdp_data, specific_pclk);
			tca_vioc_displayblock_ctrl_set(VIOC_OUTCFG_HDVENC, pdp_data, pstTiming, pstCtrl);
			break;
	}
        tccfb_extoutput_activate(0, STAGE_OUTPUTSTARTER);
	return;

error_null_pointer:
	pr_err("[ERR][FB] %s cannot find data struct fbinfo:%p pdata:%p \n", __func__, ptccfb_info, pdp_data);
}

#if defined(CONFIG_ARCH_TCC)
void tccfb_output_starter_extra_data(char output_type, struct tcc_fb_extra_data *tcc_fb_extra_data)
{
        struct fb_info *info;
        struct tccfb_info *ptccfb_info =NULL;
        struct tcc_dp_device *pdp_data =NULL;

        info = registered_fb[0];
        ptccfb_info = info->par;

        if(ptccfb_info == NULL) {
                pr_err("[ERR][FB] %s ptccfb_info is NULL\r\n", __func__);
                goto error_null_pointer;
        }
        if(tcc_fb_extra_data == NULL){
                pr_err("[ERR][FB] %s tcc_fb_extra_data is NULL\r\n", __func__);
                goto error_null_pointer;
        }

        if(ptccfb_info->pdata.Mdp_data.DispDeviceType == output_type)
                pdp_data = &ptccfb_info->pdata.Mdp_data;
        else if(ptccfb_info->pdata.Sdp_data.DispDeviceType == output_type)
                pdp_data = &ptccfb_info->pdata.Sdp_data;
        else
                goto error_null_pointer;

        switch(output_type)
        {
                case TCC_OUTPUT_HDMI:
                        tca_vioc_displayblock_extra_set(pdp_data, tcc_fb_extra_data);
                        break;

                case TCC_OUTPUT_COMPOSITE:
                case TCC_OUTPUT_COMPONENT:
                default:
                        break;
        }

        return;

error_null_pointer:
        pr_err("[ERR][FB] %s cannot find data struct fbinfo:%p pdata:%p \n", __func__, ptccfb_info, pdp_data);
}
#endif

unsigned int tccfb_output_get_mode(void)
{
	return Output_SelectMode;
}

static int tccfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	/* validate bpp */
	if (var->bits_per_pixel > 32)
		var->bits_per_pixel = 32;
	else if (var->bits_per_pixel < 16)
		var->bits_per_pixel = 16;

	/* set r/g/b positions */
	if (var->bits_per_pixel == 16) {
		var->red.offset 	= 11;
		var->green.offset	= 5;
		var->blue.offset	= 0;
		var->red.length 	= 5;
		var->green.length	= 6;
		var->blue.length	= 5;
		var->transp.length	= 0;
	} else if (var->bits_per_pixel == 32) {
		var->red.offset 	= 16;
		var->green.offset	= 8;
		var->blue.offset	= 0;
		var->transp.offset	= 24;
		var->red.length 	= 8;
		var->green.length	= 8;
		var->blue.length	= 8;
		var->transp.length	= 8;
	} else {
		var->red.length 	= var->bits_per_pixel;
		var->red.offset 	= 0;
		var->green.length	= var->bits_per_pixel;
		var->green.offset	= 0;
		var->blue.length	= var->bits_per_pixel;
		var->blue.offset	= 0;
		var->transp.length	= 0;
	}
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	return 0;
}

/* tccfb_pan_display
 *
 * pandisplay (set) the controller from the given framebuffer
 * information
*/
static int tccfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	unsigned int base_addr = 0;
	struct tccfb_info *fbi =(struct tccfb_info *) info->par;

	if(!fbi->pdata.Mdp_data.FbPowerState) {
		pr_info("[INF][FB] %s fbi->pdata.Mdp_data.FbPowerState:%d \n", __func__, fbi->pdata.Mdp_data.FbPowerState);
		return 0;
	}

	sprintk("%s addr:0x%x Yoffset:%d \n", __func__, base_addr, var->yoffset);

	tca_fb_pan_display(var, info);

	return 0;
}



/*
 *      tccfb_set_par - Optional function. Alters the hardware state.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
static int tccfb_set_par(struct fb_info *info)
{
	struct tccfb_info *fbi = info->par;
	struct fb_var_screeninfo *var = &info->var;

	sprintk("- tccfb_set_par pwr:%d  \n", fbi->pdata.Mdp_data.FbPowerState);

	if (var->bits_per_pixel == 16)
		fbi->fb->fix.visual = FB_VISUAL_TRUECOLOR;
	else if (var->bits_per_pixel == 32)
		fbi->fb->fix.visual = FB_VISUAL_TRUECOLOR;
	else
		fbi->fb->fix.visual = FB_VISUAL_PSEUDOCOLOR;

	fbi->fb->fix.line_length = (var->xres*var->bits_per_pixel)/8;
	if(fbi->fb->var.rotate != 0)	{
		pr_info("[INF][FB] fb rotation not support \n");
		return -1;
	}

	if(info->node == 1)
		tccfb1_set_par(fbi, var);

	return 0;
}

static int tccfb_ioctl(struct fb_info *info, unsigned int cmd,unsigned long arg)
{
	struct tccfb_info *ptccfb_info = (info != NULL)?(struct tccfb_info *)info->par:NULL;
	int screen_width = 0, screen_height = 0;

#ifdef CONFIG_MALI400_UMP
	u32 __user *psecureid = (u32 __user *) arg;
	ump_secure_id secure_id;
	ump_dd_physical_block ump_memory_description;
#endif

	screen_width = lcd_panel->xres;
	screen_height = lcd_panel->yres;

	if((0 > info->node) ||(info->node >= CONFIG_FB_TCC_DEVS))	{
		pr_err("[ERR][FB] ioctl: Error - fix.id[%d]\n", info->node);
		return 0;
	}

	switch(cmd)
	{
#ifdef CONFIG_MALI400_UMP
		case GET_UMP_SECURE_ID_BUF(0):
			{
				if (ump_wrapped_buffer[info->node][0] == 0) {
					ump_memory_description.addr = info->fix.smem_start;
					ump_memory_description.size = info->fix.smem_len;
					ump_wrapped_buffer[info->node][0] = ump_dd_handle_create_from_phys_blocks(&ump_memory_description, 1);
				}

				if(ump_wrapped_buffer[info->node][0] != 0){
					secure_id = ump_dd_secure_id_get(ump_wrapped_buffer[info->node][0]);
					return put_user( (unsigned int)secure_id, psecureid );
				}
				else
					return -ENOTSUPP;
			}

		case GET_UMP_SECURE_ID_BUF(1):
			{
				if (ump_wrapped_buffer[info->node][1] == 0) {
					ump_memory_description.addr = info->fix.smem_start;
					ump_memory_description.size = info->fix.smem_len;
					ump_wrapped_buffer[info->node][1] = ump_dd_handle_create_from_phys_blocks(&ump_memory_description, 1);
				}

				if(ump_wrapped_buffer[info->node][1] != 0){
					secure_id = ump_dd_secure_id_get(ump_wrapped_buffer[info->node][1]);
					return put_user( (unsigned int)secure_id, psecureid );
				}
				else
					return -ENOTSUPP;
			}

		case GET_UMP_SECURE_ID_BUF(2):
			{
				if (ump_wrapped_buffer[info->node][2] == 0) {
					ump_memory_description.addr = info->fix.smem_start + (info->var.xres * info->var.yres * (info->var.bits_per_pixel/4));
					ump_memory_description.size = info->fix.smem_len;
					ump_wrapped_buffer[info->node][2] = ump_dd_handle_create_from_phys_blocks( &ump_memory_description, 1);
				}
				if(ump_wrapped_buffer[info->node][2] != 0){
					secure_id = ump_dd_secure_id_get(ump_wrapped_buffer[info->node][2]);
					return put_user( (unsigned int)secure_id, psecureid );
				}
				else
					return -ENOTSUPP;
			}
#endif
		case TCC_LCD_FBIOPUT_VSCREENINFO:
		{
#if defined(CONFIG_SYNC_FB)
			struct fb_var_screeninfo var_info;
			struct tcc_fenc_reg_data *regs;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
 			struct sync_pt *pt;
			struct sync_fence *fence;
#endif
			int ret = 0, fd = 0;

			struct tcc_dp_device *pdp_data = &ptccfb_info->pdata.Mdp_data;

			if (copy_from_user(&var_info,
					   (struct fb_var_screeninfo __user *)arg,
					   sizeof(struct fb_var_screeninfo))) {
				ret = -EFAULT;
				break;
			}

			info->var = var_info;

			if(pdp_data == NULL)
				return 0;

			if(!pdp_data->FbPowerState)
				return 0;


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
			fd = get_unused_fd();
#else
			fd = get_unused_fd_flags(0);
#endif

			if(fd< 0){
				pr_err("[ERR][FB] fb fence sync get fd error : %d \n", fd);
				break;
			}
#endif
			mutex_lock(&ptccfb_info->output_lock);

			if (!ptccfb_info->output_on) {
				ptccfb_info->fb_timeline_max++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
				pt = sw_sync_pt_create(ptccfb_info->fb_timeline, ptccfb_info->fb_timeline_max);
				fence = sync_fence_create("display", pt);
				sync_fence_install(fence, fd);
#else
				ret = sw_sync_create_fence((struct sync_timeline *)ptccfb_info->fb_timeline, ptccfb_info->fb_timeline_max, &fd);
				if(ret){
					pr_err("[ERR][FB] sw_sync_create_fence fail!!! line : %d \n", __LINE__);
					ret = -EFAULT;
					mutex_unlock(&ptccfb_info->output_lock);
					break;
				}
#endif
				var_info.reserved[2] = fd;
				var_info.reserved[3] = 0xf;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
				sw_sync_timeline_inc(ptccfb_info->fb_timeline, 1);
#else
				sw_sync_timeline_inc((struct sync_timeline *)ptccfb_info->fb_timeline, 1);
#endif
				pr_info("[INF][FB] lcd display update on power off state \n");

				if (copy_to_user((struct fb_var_screeninfo __user *)arg,
						 &var_info,
						 sizeof(struct fb_var_screeninfo))) {
					ret = -EFAULT;
				}
				mutex_unlock(&ptccfb_info->output_lock);
				break;
			}

			regs = kzalloc(sizeof(struct tcc_fenc_reg_data), GFP_KERNEL);

			if (!regs) {
				pr_err("[ERR][FB] fb fence sync could not allocate \n");
				mutex_unlock(&ptccfb_info->output_lock);
				return -ENOMEM;
			}

			mutex_lock(&ptccfb_info->fb_timeline_lock);
			regs->screen_cpu = ptccfb_info->screen_cpu;
			regs->screen_dma= ptccfb_info->screen_dma;
			regs->var = var_info;

			regs->fence_fd = var_info.reserved[2];
			if(regs->fence_fd > 0)
			{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
 				regs->fence = sync_fence_fdget(regs->fence_fd);
#else
				regs->fence = (struct fence *) sync_file_get_fence(regs->fence_fd);
#endif
				if (!regs->fence ) {
					pr_warn("[WAR][FB] failed to import fence fd\n");
				}
			}
			list_add_tail(&regs->list, &ptccfb_info->fb_update_regs_list);

			ptccfb_info->fb_timeline_max++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
			pt = sw_sync_pt_create(ptccfb_info->fb_timeline, ptccfb_info->fb_timeline_max);
			fence = sync_fence_create("display", pt);
			sync_fence_install(fence, fd);
#else
			ret = sw_sync_create_fence((struct sync_timeline *)ptccfb_info->fb_timeline, ptccfb_info->fb_timeline_max, &fd);
			if(ret){
				pr_err("[ERR][FB] sw_sync_create_fence fail!!! line : %d \n", __LINE__);
				ret = -EFAULT;
				mutex_unlock(&ptccfb_info->fb_timeline_lock);
				mutex_unlock(&ptccfb_info->output_lock);
				break;
			}
#endif
			var_info.reserved[2] = fd;
			var_info.reserved[3] = 0xf;
			mutex_unlock(&ptccfb_info->fb_timeline_lock);

			#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
			queue_kthread_work(&ptccfb_info->fb_update_regs_worker,
									&ptccfb_info->fb_update_regs_work);
			#else
			kthread_queue_work(&ptccfb_info->fb_update_regs_worker,
									&ptccfb_info->fb_update_regs_work);
			#endif


			mutex_unlock(&ptccfb_info->output_lock);

			if(copy_to_user((struct fb_var_screeninfo __user *)arg, &var_info,	sizeof(struct fb_var_screeninfo))) {
				ret = -EFAULT;
				break;
			}
#endif
		}
		break;

		case TCC_LCDC_SET_OUTPUT_RESIZE_MODE:
			{
				tcc_display_resize resize_value;
				struct tcc_dp_device *pdp_data = NULL;

				if(copy_from_user((void *)&resize_value, (const void *)arg, sizeof(tcc_display_resize)))
					return -EFAULT;

				//pr_debug("[DBG][FB] %s : TCC_LCDC_SET_OUTPUT_RESIZE_MODE, mode=%d\n", __func__, mode);

				tca_fb_resize_set_value(resize_value, TCC_OUTPUT_MAX);

				if((ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					|| (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)  || (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT))
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else if((ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					|| (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)  || (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT))
					pdp_data = &ptccfb_info->pdata.Sdp_data;
				//else
				//	pr_err("[ERR][FB] TCC_LCDC_SET_OUTPUT_RESIZE_MODE Can't find display device , Main:%d, Sub:%d\n", ptccfb_info->pdata.Mdp_data.DispDeviceType, ptccfb_info->pdata.Sdp_data.DispDeviceType);

				if(pdp_data != NULL) {
					if(pdp_data->FbPowerState)	{
						tca_fb_activate_var(pdp_data->FbBaseAddr, &ptccfb_info->fb->var, pdp_data);
					}
				}
			}
			break;

		case TCC_SECONDARY_OUTPUT_RESIZE_MODE_STB:
			{
				tcc_display_resize resize_value;
				#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
				struct tcc_dp_device *pdp_data = NULL;
				#endif

				if(copy_from_user((void *)&resize_value, (const void *)arg, sizeof(tcc_display_resize)))
					return -EFAULT;

				#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB

				pr_info("[INF][FB] TCC_SECONDARY_OUTPUT_RESIZE_MODE_STB, mode=left:%d, right:%d, up:%d, down:%d\n",resize_value.resize_left, resize_value.resize_right, resize_value.resize_up, resize_value.resize_down);

				tca_fb_resize_set_value(resize_value, TCC_OUTPUT_COMPONENT);

				if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT)
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else if(ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT)
					pdp_data = &ptccfb_info->pdata.Sdp_data;
				//else
				//	pr_err("[ERR][FB] TCC_LCDC_SET_OUTPUT_RESIZE_MODE Can't find display device , Main:%d, Sub:%d\n", ptccfb_info->pdata.Mdp_data.DispDeviceType, ptccfb_info->pdata.Sdp_data.DispDeviceType);

				if(pdp_data != NULL) {
					if(pdp_data->FbPowerState)	{
						tca_fb_activate_var(pdp_data->FbBaseAddr, &ptccfb_info->fb->var, pdp_data);
					}
				}
				#else
					pr_info("[INF][FB] TCC_SECONDARY_OUTPUT_RESIZE_MODE_STB function Not Use \n");
				#endif//
			}
			break;
		case TCC_LCDC_SET_OUTPUT_ATTACH_RESIZE_MODE:
			{
				int ret = 0;
				tcc_display_resize resize_value;
				struct tcc_dp_device *pdp_data = NULL;

				pdp_data = &ptccfb_info->pdata.Mdp_data;

				if(copy_from_user((void *)&resize_value, (const void *)arg, sizeof(tcc_display_resize)))
					return -EFAULT;

				//dprintk("%s : TCC_LCDC_SET_OUTPUT_ATTACH_RESIZE_MODE, mode=%d\n", __func__, mode);

				tca_fb_output_attach_resize_set_value(resize_value);
				if(ptccfb_info){
					ret = tca_fb_attach_stop(ptccfb_info);
					if(ret >= 0)
						tca_fb_attach_start(ptccfb_info);
				}
			}
			break;


		#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
		case TCC_LCDC_SET_HDMI_R2YMD:
			{
				unsigned int r2ymd;

					struct tcc_dp_device *pdp_data = NULL;

					if(copy_from_user((void *)&r2ymd, (const void *)arg, sizeof(r2ymd)))
						return -EFAULT;

					if((ptccfb_info->pdata.Mdp_data.FbPowerState != true) || (ptccfb_info->pdata.Mdp_data.DispDeviceType ==TCC_OUTPUT_HDMI))
						pdp_data = &ptccfb_info->pdata.Mdp_data;
					else if((ptccfb_info->pdata.Sdp_data.FbPowerState != true) || (ptccfb_info->pdata.Sdp_data.DispDeviceType ==TCC_OUTPUT_HDMI))
						pdp_data = &ptccfb_info->pdata.Sdp_data;
					else
						pr_err("[ERR][FB] hdmi power on  : can't find HDMI voic display block \n");

					if(pdp_data && ((r2ymd >> 16) == TCC_LCDC_SET_HDMI_R2YMD_MAGIC)) {
						VIOC_DISP_SetR2YMD(pdp_data->ddc_info.virt_addr, (unsigned char)(r2ymd & 0xFF));
					}
			}
			break;
		#endif

		case TCC_LCDC_HDMI_START:
			{
				struct tcc_dp_device *pdp_data = NULL;

                                if(ptccfb_info != NULL) {
        				if((ptccfb_info->pdata.Mdp_data.FbPowerState != true) || (ptccfb_info->pdata.Mdp_data.DispDeviceType ==TCC_OUTPUT_HDMI))
        					pdp_data = &ptccfb_info->pdata.Mdp_data;
        				else if((ptccfb_info->pdata.Sdp_data.FbPowerState != true) || (ptccfb_info->pdata.Sdp_data.DispDeviceType ==TCC_OUTPUT_HDMI))
        					pdp_data = &ptccfb_info->pdata.Sdp_data;

        				if(pdp_data != NULL)
        				{
        					if(pdp_data->FbPowerState == true)
        					{
        						pr_info("[INF][FB] HDMI voic display block power off  \n");
        						tca_vioc_displayblock_disable(pdp_data);
        						tca_vioc_displayblock_powerOff(pdp_data);
        					}

        					pdp_data->DispDeviceType = TCC_OUTPUT_HDMI;
        					tca_vioc_displayblock_powerOn(pdp_data, 0);
        				} else {
        				        pr_err("[ERR][FB] hdmi power on  : can't find HDMI voic display block \n");
        				}
                                }
			}
			break;

		case TCC_LCDC_HDMI_TIMING:
			{
                                int skip_activate = 0;
				struct tcc_dp_device *pdp_data = NULL;
				struct lcdc_timimg_parms_t lcdc_timing;

				dprintk(" TCC_LCDC_HDMI_TIMING: \n");

				if (copy_from_user((void*)&lcdc_timing, (const void*)arg, sizeof(struct lcdc_timimg_parms_t)))
					return -EFAULT;

                                if(ptccfb_info != NULL) {
        				if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
        					pdp_data = &ptccfb_info->pdata.Mdp_data;
        				else if(ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
        					pdp_data = &ptccfb_info->pdata.Sdp_data;

        				if(pdp_data != NULL) {
        					tca_vioc_displayblock_timing_set(VIOC_OUTCFG_HDMI, pdp_data, &lcdc_timing);
        					#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
						#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
                                                if ( DV_PATH_DIRECT & vioc_get_path_type() ) {
                                                        skip_activate = 1;
                                                        pr_info("[INF][FB] %s TCC_LCDC_HDMI_TIMING DV mode\r\n", __func__);
        					        hdmi_set_activate_callback(tccfb_extoutput_activate, info->node, STAGE_FB);
                                                } else {
                                                        /* Remove Callaback */
                                                        hdmi_set_activate_callback(NULL, 0, 0);
                                                }
        					#endif /* CONFIG_VIOC_DOLBY_VISION_EDR */
                                                #if defined(CONFIG_DRM)
                                                skip_activate = 1;
						#if defined(CONFIG_LCD_HDMI640X480TU)
						if(display_ext_panel != NULL && pdp_data->DispOrder == DD_SUB) {
							if(!strncmp("HDMI_480TU", display_ext_panel->name, 10)) {
								skip_activate = 0;
							}
						}
						#endif
                                                if(skip_activate && pdp_data->FbPowerState){
                                        		VIOC_DISP_TurnOn(pdp_data->ddc_info.virt_addr);
                                        	}
                                                #endif /*CONFIG_DRM */
                                                #endif /* CONFIG_TCC_HDMI_DRIVER_V2_0 */

                                                if(!skip_activate) {
        					        tccfb_extoutput_activate(info->node, STAGE_FB);
                                                }
                                                if(display_ext_panel != NULL){
                                                        if(display_ext_panel->set_power != NULL)
                                                                display_ext_panel->set_power(display_ext_panel, 3/* turn on by external app */, NULL);
                                                }
        				} else {
        				        pr_err("[ERR][FB] hdmi timing setting : can't find HDMI voic display block \n");
                                        }
                                }

			}
			break;


		case TCC_LCDC_HDMI_END:
			{
				struct tcc_dp_device *pdp_data = NULL;

                                if(ptccfb_info != NULL) {
        				if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
        					pdp_data = &ptccfb_info->pdata.Mdp_data;
        				else if(ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
        					pdp_data = &ptccfb_info->pdata.Sdp_data;

        				if(pdp_data != NULL){
                                                if(display_ext_panel != NULL){
                                                        if(display_ext_panel->set_power != NULL) {
                                                                display_ext_panel->set_power(display_ext_panel, 2 /* turn off by external app */, NULL);
                                                        }
                                                }
                                                #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
                                                tca_vioc_displayblock_clock_select(pdp_data, 0);
                                                #endif

        					#if defined(CONFIG_TCC_VTA)
        					vta_cmd_notify_change_status(__func__);
        					#endif

        					#ifdef CONFIG_VIDEO_TCC_VOUT
        					if(tcc_vout_get_status(pdp_data->ddc_info.blk_num) != TCC_VOUT_RUNNING)
        					#endif
        					{
        						#ifdef TCC_VIDEO_DISPLAY_BY_VSYNC_INT
        						#ifdef CONFIG_ANDROID // For Video Display using TCC_LCDC_HDMI_DISPLAY
        						tcc_vsync_hdmi_end(pdp_data);
        						#else
        						if(tcc_vsync_isVsyncRunning(VSYNC_MAIN) || tcc_vsync_isVsyncRunning(VSYNC_SUB0))
        							tcc_vsync_hdmi_end(pdp_data);
        						#endif
        						#endif
        					}

        					#ifdef CONFIG_VOUT_USE_VSYNC_INT
        					if(tcc_vout_get_status(pdp_data->ddc_info.blk_num) == TCC_VOUT_RUNNING)
        						tcc_vout_hdmi_end(pdp_data->ddc_info.blk_num);
        					#endif

        					tca_vioc_displayblock_disable(pdp_data);
        					tca_vioc_displayblock_powerOff(pdp_data);
        					pdp_data->DispDeviceType = TCC_OUTPUT_NONE;
        				} else {
        				        pr_err("[ERR][FB] TCC_LCDC_HDMI_END : can't find HDMI voic display block \n");
        				}
                                }
			}
			break;



		case TCC_LCDC_HDMI_DISPLAY:
			{
				struct tcc_lcdc_image_update ImageInfo;
				struct tcc_dp_device *pdp_data = NULL;
				if (copy_from_user((void *)&ImageInfo, (const void *)arg, sizeof(struct tcc_lcdc_image_update))){
					return -EFAULT;
				}

			#if defined(CONFIG_VIDEO_DISPLAY_SWAP_VPU_FRAME) && defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
				/* ImageInfo.enable
				 *  0: stop playing
				 *  1: Video is playing
				 */
				if (ImageInfo.enable != 0) {
					/* ptccfb_info->image_enable
					 *  0: ImageInfo is 1st frame (start playing)
					 *  1: Video is playing
					 */
					if (ptccfb_info->image_enable != 0) {
						if (ptccfb_info->swap_buf_status == SWAP_BUF_START) {
							if (ptccfb_info->swap_buf_id >= ImageInfo.buffer_unique_id) {
								dprintk("0x%X (%d >= %d)\n", ptccfb_info->swap_buf_status, ptccfb_info->swap_buf_id, ImageInfo.buffer_unique_id);
								return 0;
							}
						}
					}
				} else {
					ptccfb_info->swap_buf_id = 0;
				}

				ptccfb_info->swap_buf_status = SWAP_BUF_END;
				ptccfb_info->image_enable = ImageInfo.enable;
			#endif

				if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else	 if(ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					pdp_data = &ptccfb_info->pdata.Sdp_data;
				//else
				//	pr_err("TCC_LCDC_HDMI_DISPLAY Can't find HDMI output , Main:%d, Sub :%d \n", ptccfb_info->pdata.Mdp_data.DispDeviceType, ptccfb_info->pdata.Sdp_data.DispDeviceType);

				if(pdp_data && Output_SelectMode == TCC_OUTPUT_HDMI)
				{
		#ifdef TCC_VIDEO_DISPLAY_BY_VSYNC_INT
					// this code prevent a error of transparent on layer0 after stopping v
					if(ImageInfo.Lcdc_layer == 0 && ImageInfo.enable == 0) {
						tcc_vsync_set_output_mode_all(-1);
					}
				#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
					if(ImageInfo.enable != 0){
						tcc_vsync_set_output_mode_all(TCC_OUTPUT_HDMI);
						if(ImageInfo.output_toMemory)
							tcc_video_clear_last_frame(ImageInfo.Lcdc_layer, 1);
						if( 0 >= tcc_video_check_last_frame(&ImageInfo) ){
							pr_info("[INF][FB] skip 2 this frame for last-frame \n");
							return 0;
						}
						tcc_video_info_backup(VSYNC_MAIN, &ImageInfo);
					}
				#endif
		#endif
					if(pdp_data->FbPowerState)
						tca_scale_display_update(pdp_data, (struct tcc_lcdc_image_update *)&ImageInfo);
				}

				return 0;

			}
			break;


		case TCC_LCDC_HDMI_CHECK:
			{
				unsigned int ret_mode = 0;

				if(HDMI_pause || ((screen_width < screen_height)&& (!HDMI_video_mode)))
				{
					ret_mode = 1;
					dprintk("\n %d : %d %d  \n ", HDMI_pause, screen_width, screen_height);
				}

				put_user(ret_mode, (unsigned int __user*)arg);
			}
			break;

		case TCC_LCDC_HDMI_MODE_SET:
 			{
				TCC_HDMI_M uiHdmi;

				if(get_user(uiHdmi, (int __user *) arg))
					return -EFAULT;

				dprintk("%s: TCC_LCDC_HDMI_MODE_SET [%d] video_M:%d Output_SelectMode:%d   \n", __func__ , uiHdmi , HDMI_video_mode, Output_SelectMode);

				switch(uiHdmi)
				{
					case TCC_HDMI_SUSEPNED:
						HDMI_pause = 1;
						break;
					case TCC_HDMI_RESUME:
						HDMI_pause = 0;
						break;
					case TCC_HDMI_VIDEO_START:
						HDMI_video_mode = 1;
						break;
					case TCC_HDMI_VIDEO_END:
						#if defined(CONFIG_TCC_VTA)
						vta_cmd_notify_change_status(__func__);
						#endif

						HDMI_video_mode = 0;
						//TCC_OUTPUT_FB_CaptureVideoImg(0);
						#if defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT) && defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
						tcc_video_clear_last_frame(RDMA_VIDEO,true);
						tcc_video_clear_last_frame(RDMA_VIDEO_SUB,true);
						#endif
						break;
					default:
						break;
				}
 			}
			break;


		case TCC_LCDC_HDMI_GET_SIZE:
			{
				tcc_display_size HdmiSize;
				HdmiSize.width = HDMI_video_width;
				HdmiSize.height = HDMI_video_height;
  				HdmiSize.frame_hz = HDMI_video_hz;

				dprintk("%s: TCC_LCDC_HDMI_GET_SIZE -  HDMI_video_width:%d HDMI_video_height:%d   \n", __func__ , HDMI_video_width, HDMI_video_height);
				if (copy_to_user((tcc_display_size *)arg, &HdmiSize, sizeof(HdmiSize)))		{
					return -EFAULT;
				}
			}
			break;
		case TCC_LCDC_HDMI_SET_SIZE:
			{
				tcc_display_size HdmiSize;
				if (copy_from_user((void *)&HdmiSize, (const void *)arg, sizeof(tcc_display_size)))
					return -EFAULT;

				HDMI_video_width = HdmiSize.width;
				HDMI_video_height = HdmiSize.height;
				HDMI_video_hz = HdmiSize.frame_hz;

				if(HDMI_video_hz == 23)
					HDMI_video_hz = 24;
				else if(HDMI_video_hz == 29)
					HDMI_video_hz = 30;

				dprintk("%s: TCC_LCDC_HDMI_SET_SIZE -  HDMI_video_width:%d HDMI_video_height:%d   \n", __func__ , HDMI_video_width, HDMI_video_height);
			}
			break;

                #if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
                case TCC_LCDC_HDMI_EXTRA_SET:
                        {
                                struct tcc_dp_device *pdp_data = NULL;
                                struct tcc_fb_extra_data tcc_fb_extra_data;

                                dprintk(" TCC_LCDC_HDMI_EXTRA_SET: \n");

                                if (copy_from_user((void*)&tcc_fb_extra_data, (const void*)arg, sizeof(struct tcc_fb_extra_data)))
                                        return -EFAULT;

                                if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
                                        pdp_data = &ptccfb_info->pdata.Mdp_data;
                                else if(ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
                                        pdp_data = &ptccfb_info->pdata.Sdp_data;
                                else
                                        pr_err("[ERR][FB] hdmi timing setting : can't find HDMI voic display block \n");

                                if(pdp_data != NULL) {
                                        tca_vioc_displayblock_extra_set(pdp_data, &tcc_fb_extra_data);
                                }
                        }
                        break;
                #endif

                #if defined(TCC_LCDC_HDMI_DISPDEV_ID)
                case TCC_LCDC_HDMI_DISPDEV_ID:
                        {
                                int dispdev_id = -1;
                                struct tcc_dp_device *pdp_data = NULL;

                                if(ptccfb_info != NULL) {
                                        if((ptccfb_info->pdata.Mdp_data.FbPowerState != true) || (ptccfb_info->pdata.Mdp_data.DispDeviceType ==TCC_OUTPUT_HDMI))
                                                pdp_data = &ptccfb_info->pdata.Mdp_data;
                                        else if((ptccfb_info->pdata.Sdp_data.FbPowerState != true) || (ptccfb_info->pdata.Sdp_data.DispDeviceType ==TCC_OUTPUT_HDMI))
                                                pdp_data = &ptccfb_info->pdata.Sdp_data;

                                        if(pdp_data != NULL)
                                        {
                                                pr_info("[INF][FB] %s TCC_LCDC_HDMI_DISPDEV_ID = %d\r\n", __func__, pdp_data->DispNum);
                                                dispdev_id = pdp_data->DispNum;
                                        } else {
                                                pr_err("[ERR][FB] TCC_LCDC_HDMI_DISPDEV_ID  : can't find HDMI voic display block \n");
                                        }
                                        if (copy_to_user((int *)arg, &dispdev_id, sizeof(int))) {
                                                return -EFAULT;
                                        }
                                }
                        }
                        break;
                #else
                #warning("Please check TCC_LCDC_HDMI_DISPDEV_ID")
                #endif
		
        case TCC_EXT_FBIOPUT_VSCREENINFO:
        case TCC_HDMI_FBIOPUT_VSCREENINFO:
        case TCC_CVBS_FBIOPUT_VSCREENINFO:
        case TCC_COMPONENT_FBIOPUT_VSCREENINFO:
                {
			unsigned int BaseAddr = 0;
                        struct fb_var_screeninfo var;
			struct tcc_dp_device *pdp_data;
			external_fbioput_vscreeninfo sc_info;

                        #if defined(CONFIG_SYNC_FB)
                        int fd;
                        #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
                        struct sync_pt *pt;
			struct sync_fence *fence;
                        #endif

                        #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
                        #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
			fd = get_unused_fd();
                        #else
			fd = get_unused_fd_flags(0);
                        #endif // LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
                        #endif // // LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
                        #endif // CONFIG_SYNC_FB

                        if(ptccfb_info == NULL) {
                                pr_err("[ERR][FB] TCC_XXX_FBIOPUT_VSCREENINFO ptccfb_info is NULL\r\n");
				return 0;
                        }

                        pdp_data = &ptccfb_info->pdata.Sdp_data;
			if(pdp_data == NULL) {
                                pr_err("[ERR][FB] TCC_XXX_FBIOPUT_VSCREENINFO pdp_data is NULL\r\n");
				return 0;
			}
                        pdp_data->DispOrder = DD_SUB;
			if (copy_from_user((void*)&sc_info, (const void*)arg, sizeof(external_fbioput_vscreeninfo))) {
				return -EFAULT;
			}

			#if !defined(CONFIG_BUILTIN_2ND_DISPLAY_ON_1ST_DISPLAY)
			if(!pdp_data->FbPowerState) {
				return 0;
			}
			#endif

			memset(&var, 0, sizeof(struct fb_var_screeninfo));
			var.xres = sc_info.width;
			var.yres = sc_info.height;
			var.bits_per_pixel = sc_info.bits_per_pixel;
			var.yoffset = sc_info.offset;
			var.xres_virtual= sc_info.xres_virtual;

		 	BaseAddr = ptccfb_info->map_dma + sc_info.offset;
			tca_fb_activate_var(BaseAddr,  &var, pdp_data);

			if(pdp_data->FbPowerState)
				tca_fb_vsync_activate(pdp_data);

			#if defined(CONFIG_SYNC_FB)

			mutex_lock(&ptccfb_info->ext_timeline_lock);
                        #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
			if(fd< 0){
                                pr_err("[ERR][FB] TCC_XXX_FBIOPUT_VSCREENINFO fb fence sync get fd error : %d \n", fd);
				break;
			}
                        #endif // LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)

			ptccfb_info->ext_timeline_max++;
                        #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
			pt = sw_sync_pt_create(ptccfb_info->ext_timeline, ptccfb_info->ext_timeline_max);
			fence = sync_fence_create("display_ext", pt);
			sync_fence_install(fence, fd);
                        #else
			{
				int ret = sw_sync_create_fence((struct sync_timeline *)ptccfb_info->ext_timeline, ptccfb_info->ext_timeline_max, &fd);
				if(ret){
					pr_err("[ERR][FB] sw_sync_create_fence fail!!! line : %d \n", __LINE__);
					mutex_unlock(&ptccfb_info->ext_timeline_lock);
					return  -EFAULT;
				}
			}
                        #endif // LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
			sc_info.fence_fd = fd;
                        #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
			sw_sync_timeline_inc(ptccfb_info->fb_timeline, 1);
                        #else
			sw_sync_timeline_inc((struct sync_timeline *)ptccfb_info->ext_timeline, 1);
                        #endif
			mutex_unlock(&ptccfb_info->ext_timeline_lock);

			if (copy_to_user((external_fbioput_vscreeninfo *)arg, (void*)&sc_info, sizeof(external_fbioput_vscreeninfo)))
				return -EFAULT;

			#endif
		}
		break;

	case TCC_SH_DISPLAY_FBIOPUT_VSCREENINFO:
		{
			unsigned int BaseAddr = 0;
			external_fbioput_vscreeninfo sc_info;
			struct tcc_dp_device *pdp_data = NULL;
			pdp_data = &ptccfb_info->pdata.Sdp_data;

			if (copy_from_user((void*)&sc_info, (const void*)arg, sizeof(external_fbioput_vscreeninfo)))
				return -EFAULT;

		 	BaseAddr = ptccfb_info->map_dma + sc_info.offset;
			pr_info("[INF][FB] Base address : 0x%08x \n", BaseAddr);	
#ifdef CONFIG_TCC_SCREEN_SHARE				
			tcc_scrshare_set_sharedBuffer(BaseAddr, sc_info.width, sc_info.height, TCC_LCDC_IMG_FMT_RGB888);
#endif			
		}
		break;

                case TCC_LCDC_COMPOSITE_CHECK:
			{
				unsigned int composite_detect = 1;

				#if defined(CONFIG_FB_TCC_COMPOSITE)
				composite_detect = tcc_composite_detect();
				#endif

				if (copy_to_user((void *)arg, &composite_detect, sizeof(unsigned int)))
					return -EFAULT;
			}
			break;

		case TCC_LCDC_COMPOSITE_MODE_SET:
			{
				struct tcc_dp_device *pdp_data = NULL;
				LCDC_COMPOSITE_MODE composite_mode;

				if(copy_from_user((void *)&composite_mode, (const void *)arg, sizeof(LCDC_COMPOSITE_MODE))){
					return -EFAULT;
				}

				if(composite_mode == LCDC_COMPOSITE_UI_MODE)
				{
					#if defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_STB)
						//Don't change output mode.
					#else
					Output_SelectMode = TCC_OUTPUT_COMPOSITE;
					#endif

 					pr_info("[INF][FB] TCC_LCDC_COMPOSITE_MODE_SET : Output_SelectMode = %d , composite_mode = %d\n", Output_SelectMode, composite_mode);

					//TCC_OUTPUT_FB_ClearVideoImg();

					if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)
						pdp_data = &ptccfb_info->pdata.Mdp_data;
					else if(ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)
						pdp_data = &ptccfb_info->pdata.Sdp_data;

					if(pdp_data != NULL) {
						if(pdp_data->FbPowerState) {
							if(pdp_data->DispOrder == DD_MAIN)
								 pdp_data->FbBaseAddr = ptccfb_info->map_dma + ptccfb_info->fb->var.xres * ptccfb_info->fb->var.yoffset * ( ptccfb_info->fb->var.bits_per_pixel/8);

							tca_fb_activate_var(pdp_data->FbBaseAddr, &ptccfb_info->fb->var, pdp_data);
						}
					}

					//TCC_OUTPUT_FB_MouseShow(1, TCC_OUTPUT_COMPOSITE);

					#ifdef TCC_VIDEO_DISPLAY_BY_VSYNC_INT
						tca_vsync_video_display_enable();

						#if defined(CONFIG_TCC_DISPLAY_MODE_USE)|| defined(CONFIG_TCC_DISPLAY_LCD_CVBS)
							tcc_vsync_set_firstFrameFlag_all(1);
						#endif

						tcc_vsync_set_output_mode_all(-1);
					#endif
				}
				else if(composite_mode == LCDC_COMPOSITE_NONE_MODE)
				{
					Output_SelectMode = TCC_OUTPUT_NONE;

					if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)
						pdp_data = &ptccfb_info->pdata.Mdp_data;
					else if(ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)
						pdp_data = &ptccfb_info->pdata.Sdp_data;

					#ifdef TCC_VIDEO_DISPLAY_BY_VSYNC_INT
						//---> @inbest: for v4l2 vout driver
						#ifdef CONFIG_ANDROID
						tca_vsync_video_display_disable();	// @inbest: orignal code
						#else
						#ifdef CONFIG_VIDEO_TCC_VOUT
						if(tcc_vout_get_status(1) != TCC_VOUT_RUNNING)
						#endif
						{
							if(tcc_vsync_isVsyncRunning(VSYNC_MAIN) || tcc_vsync_isVsyncRunning(VSYNC_SUB0)) {
								tca_vsync_video_display_disable();

								#if defined(CONFIG_FB_TCC_COMPOSITE)
								if(pdp_data) {
									struct tcc_lcdc_image_update ImageInfo;
									memset(&ImageInfo, 0x00, sizeof(struct tcc_lcdc_image_update));
									ImageInfo.enable = 0;
									ImageInfo.Lcdc_layer = RDMA_VIDEO;
									tca_scale_display_update(pdp_data, &ImageInfo);
								}
								#endif
							}
						}
						#endif
						//<---

						#if !defined(CONFIG_TCC_DISPLAY_MODE_USE)|| defined(CONFIG_TCC_DISPLAY_LCD_CVBS)
							tcc_vsync_set_firstFrameFlag_all(1);
						#endif
					#endif

					#ifdef TCC_VIDEO_DISPLAY_DEINTERLACE_MODE
						if(is_deinterlace_enabled(VSYNC_MAIN))
							TCC_VIQE_DI_DeInit60Hz();
					#endif
				}
			}
			break;

		case TCC_LCDC_COMPONENT_CHECK:
			{
				unsigned int component_detect;

				#if defined(CONFIG_FB_TCC_COMPONENT)
					component_detect = tcc_component_detect();
				#endif

				if (copy_to_user((void *)arg, &component_detect, sizeof(unsigned int)))
					return -EFAULT;
			}
			break;

		case TCC_LCDC_COMPONENT_MODE_SET:
			{
				struct tcc_dp_device *pdp_data = NULL;
				LCDC_COMPONENT_MODE component_mode;

				if(copy_from_user((void *)&component_mode, (const void *)arg, sizeof(LCDC_COMPONENT_MODE))){
					return -EFAULT;
				}

				if(component_mode == LCDC_COMPONENT_UI_MODE)
				{

				#if defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_STB)
					//Don't change output mode.
				#else
					Output_SelectMode = TCC_OUTPUT_COMPONENT;
				#endif

 					pr_info("[INF][FB] TCC_LCDC_COMPONENT_MODE_SET : Output_SelectMode = %d , component_mode = %d\n", Output_SelectMode, component_mode);

					//TCC_OUTPUT_FB_ClearVideoImg();

					if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT)
						pdp_data = &ptccfb_info->pdata.Mdp_data;
					else if(ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT)
						pdp_data = &ptccfb_info->pdata.Sdp_data;

					if(pdp_data != NULL) {
						if(pdp_data->FbPowerState) {
							tca_fb_activate_var(pdp_data->FbBaseAddr, &ptccfb_info->fb->var, pdp_data);
						}
					}

					//TCC_OUTPUT_FB_MouseShow(1, TCC_OUTPUT_COMPONENT);

					#ifdef TCC_VIDEO_DISPLAY_BY_VSYNC_INT
						//---> @alank: for v4l2 vout driver
						#ifdef CONFIG_ANDROID
						tca_vsync_video_display_enable();	// @alank: orignal code
						#else
						#ifdef CONFIG_VIDEO_TCC_VOUT
						if(tcc_vout_get_status(1) != TCC_VOUT_RUNNING)
						#endif
						{
							if(tcc_vsync_isVsyncRunning(VSYNC_MAIN) || tcc_vsync_isVsyncRunning(VSYNC_SUB0))
								tca_vsync_video_display_enable();
						}
						#endif
						//<---

						#if defined(CONFIG_TCC_DISPLAY_MODE_USE)
							tcc_vsync_set_firstFrameFlag_all(1);
						#endif

						tcc_vsync_set_output_mode_all(-1);
					#endif
				}
				else if(component_mode == LCDC_COMPONENT_NONE_MODE)
				{
					Output_SelectMode = TCC_OUTPUT_NONE;

					if(ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT)
						pdp_data = &ptccfb_info->pdata.Mdp_data;
					else if(ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT)
						pdp_data = &ptccfb_info->pdata.Sdp_data;

					#ifdef TCC_VIDEO_DISPLAY_BY_VSYNC_INT
						//---> @alank: for v4l2 vout driver
						#ifdef CONFIG_ANDROID
						tca_vsync_video_display_disable();	// @alank: orignal code
						#else
						#ifdef CONFIG_VIDEO_TCC_VOUT
						if(tcc_vout_get_status(1) != TCC_VOUT_RUNNING)
						#endif
						{
							if(tcc_vsync_isVsyncRunning(VSYNC_MAIN) || tcc_vsync_isVsyncRunning(VSYNC_SUB0)) {
								tca_vsync_video_display_disable();

								#if defined(CONFIG_FB_TCC_COMPONENT)
								if(pdp_data) {
									struct tcc_lcdc_image_update ImageInfo;
									memset(&ImageInfo, 0x00, sizeof(struct tcc_lcdc_image_update));
									ImageInfo.enable = 0;
									ImageInfo.Lcdc_layer = RDMA_VIDEO;
									tca_scale_display_update(pdp_data, &ImageInfo);
								}
								#endif
							}
						}
						#endif
						//<---

						#if !defined(CONFIG_TCC_DISPLAY_MODE_USE)
							tcc_vsync_set_firstFrameFlag_all(1);
						#endif
					#endif

					#ifdef TCC_VIDEO_DISPLAY_DEINTERLACE_MODE
						if(is_deinterlace_enabled(VSYNC_MAIN))
							TCC_VIQE_DI_DeInit60Hz();
					#endif
				}
			}
			break;

		case TCC_LCDC_FBCHANNEL_ONOFF:
			{
				unsigned int onOff;
				struct tcc_dp_device *pdp_data = NULL;

				if(copy_from_user((void *)&onOff, (const void *)arg, sizeof(unsigned int)))
					return -EFAULT;

				if((ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					|| (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)  || (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT) )
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else	 if((ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					|| (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)  || (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT) )
					pdp_data = &ptccfb_info->pdata.Sdp_data;

				#if defined(CONFIG_MACH_TCC8930ST)
					#if !defined(CONFIG_TCC_DISPLAY_MODE_USE)
						if(pdp_data != NULL)
						{
							pr_info("[INF][FB] TCC_LCDC_FBCHANNEL_ONOFF: onOff=%d, addr=0x%08x\n", onOff, pdp_data->rdma_info[RDMA_FB].virt_addr);

							if(onOff)
                				VIOC_RDMA_SetImageEnable(pdp_data->rdma_info[RDMA_FB].virt_addr);
							else
                				VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[RDMA_FB].virt_addr);
						}
					#endif
				#endif
			}
			break;

		case TCC_LCDC_MOUSE_SHOW:
			{
				unsigned int enable;
				struct tcc_dp_device *pdp_data = NULL;

				if((ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					|| (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)  || (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT) )
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else	 if((ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					|| (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)  || (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT) )
					pdp_data = &ptccfb_info->pdata.Sdp_data;
				else
					pr_err("[ERR][FB] TCC_LCDC_MOUSE_SHOW Can't find output , Main:%d, Sub :%d \n", ptccfb_info->pdata.Mdp_data.DispDeviceType, ptccfb_info->pdata.Sdp_data.DispDeviceType);

				if(copy_from_user((void *)&enable, (const void *)arg, sizeof(unsigned int)))
					return -EFAULT;

				if(pdp_data != NULL)
				{
					if(enable == 0)
						VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[RDMA_MOUSE].virt_addr);
				}
			}
			break;
#ifdef CONFIG_DIRECT_MOUSE_CTRL
		case TCC_LCDC_MOUSE_MOVE:
			{
				tcc_mouse mouse;
				struct tcc_dp_device *pdp_data = NULL;

				if (copy_from_user((void *)&mouse, (const void *)arg, sizeof(tcc_mouse)))
					return -EFAULT;

				if((ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					|| (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)  || (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT) )
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else	 if((ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
					|| (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)  || (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT) )
					pdp_data = &ptccfb_info->pdata.Sdp_data;
				else
					pr_err("[ERR][FB] TCC_LCDC_MOUSE_MOVE Can't find  output , Main:%d, Sub :%d \n", ptccfb_info->pdata.Mdp_data.DispDeviceType, ptccfb_info->pdata.Sdp_data.DispDeviceType);

				if(pdp_data != NULL)
					tca_fb_mouse_move(ptccfb_info->fb->var.xres, ptccfb_info->fb->var.yres, &mouse, pdp_data);
			}
			break;
		case TCC_LCDC_MOUSE_ICON:
			{
				tcc_mouse_icon mouse_icon;
				if (copy_from_user((void *)&mouse_icon, (const void *)arg, sizeof(tcc_mouse_icon)))
					return -EFAULT;
				tca_fb_mouse_set_icon(&mouse_icon);
			}
			break;
#else
		case TCC_LCDC_MOUSE_MOVE:
		case TCC_LCDC_MOUSE_ICON:
				pr_warn("[WAR][FB] Need to enable related FEATURE(Config).\n");
			break;
#endif
		case TCC_LCDC_3D_UI_ENABLE:
			{
				unsigned int mode;
				struct tcc_dp_device *pdp_data = NULL;

				if(copy_from_user((void *)&mode, (const void *)arg, sizeof(unsigned int)))
					return -EFAULT;

                                if(ptccfb_info != NULL) {
                                        switch(ptccfb_info->pdata.Mdp_data.DispDeviceType) {
                                                case TCC_OUTPUT_HDMI:
                                                case TCC_OUTPUT_COMPOSITE:
                                                case TCC_OUTPUT_COMPONENT:
                                                        pdp_data = &ptccfb_info->pdata.Mdp_data;
                                                        break;
                                                default:
                                                        break;
                                        }

                                        if(pdp_data == NULL) {
                                                switch(ptccfb_info->pdata.Sdp_data.DispDeviceType) {
                                                        case TCC_OUTPUT_HDMI:
                                                        case TCC_OUTPUT_COMPOSITE:
                                                        case TCC_OUTPUT_COMPONENT:
                                                                pdp_data = &ptccfb_info->pdata.Sdp_data;
                                                                break;
                                                        default:
                                                                break;
                                                }
                                        }

                                        if(pdp_data != NULL) {
                                                tca_fb_divide_set_mode(pdp_data, true, mode);

                                                if(pdp_data->FbPowerState) {
                                                        tca_fb_activate_var(pdp_data->FbBaseAddr, &ptccfb_info->fb->var, pdp_data);
                                                }
                                        } else {
                                                pr_err("[ERR][FB] TCC_LCDC_3D_UI_ENABLE Can't find  output , Main:%d, Sub :%d \n", ptccfb_info->pdata.Mdp_data.DispDeviceType, ptccfb_info->pdata.Sdp_data.DispDeviceType);
                                        }
                                }
			}
			break;

		case TCC_LCDC_3D_UI_DISABLE:
			{
				struct tcc_dp_device *pdp_data = NULL;

                                if(ptccfb_info != NULL) {
                                        switch(ptccfb_info->pdata.Mdp_data.DispDeviceType) {
                                                case TCC_OUTPUT_HDMI:
                                                case TCC_OUTPUT_COMPOSITE:
                                                case TCC_OUTPUT_COMPONENT:
                                                        pdp_data = &ptccfb_info->pdata.Mdp_data;
                                                        break;
                                                default:
                                                        break;
                                        }

                                        if(pdp_data == NULL) {
                                              switch(ptccfb_info->pdata.Sdp_data.DispDeviceType) {
                                                case TCC_OUTPUT_HDMI:
                                                case TCC_OUTPUT_COMPOSITE:
                                                case TCC_OUTPUT_COMPONENT:
                                                        pdp_data = &ptccfb_info->pdata.Sdp_data;
                                                        break;
                                                default:
                                                        break;
                                              }
                                        }

        				if(pdp_data != NULL) {
        					tca_fb_divide_set_mode(pdp_data, false, 0);

                                                if(pdp_data->FbPowerState) {
                                                        tca_fb_activate_var(pdp_data->FbBaseAddr, &ptccfb_info->fb->var, pdp_data);
                                                }
        				} else {
                				pr_err("[ERR][FB] TCC_LCDC_3D_UI_DISABLE Can't find  output , Main:%d, Sub :%d \n", ptccfb_info->pdata.Mdp_data.DispDeviceType, ptccfb_info->pdata.Sdp_data.DispDeviceType);
        				}
                                }
			}
			break;


		case TCC_LCDC_GET_DISPLAY_TYPE:
			{
				unsigned int display_type = 0; /* Support HDMI/CVBS/COMPONENT output */

                            #if defined(CONFIG_FB_TCC_COMPOSITE) && defined(CONFIG_FB_TCC_COMPONENT)
                                    display_type = 0; /* Support HDMI/CVBS/COMPONENT output */
                            #endif

                            #if defined(CONFIG_FB_TCC_COMPOSITE) && !defined(CONFIG_FB_TCC_COMPONENT)
                                    display_type = 1; /* Support HDMI/CVBS output */
                            #endif

                            #if !defined(CONFIG_FB_TCC_COMPOSITE) && !defined(CONFIG_FB_TCC_COMPONENT)
                                    display_type = 2; /* Support HDMI output */
                            #endif


				if (copy_to_user((void *)arg, &display_type, sizeof(unsigned int)))
					return -EFAULT;

				//TCC_OUTPUT_FB_DetachOutput(1);
			}
			break;
		case TCC_LCDC_DISPLAY_END:
			{
				struct tcc_lcdc_image_update ImageInfo;
				pr_info("[INF][FB] TCC_LCDC_DISPLAY_END lcd_video_started %d\n", lcd_video_started);

				memset(&ImageInfo, 0x00, sizeof(struct tcc_lcdc_image_update));

				if(info->node == 0) {
					if(!lcd_video_started)
						return 0;

					ImageInfo.Lcdc_layer = RDMA_VIDEO;
					lcd_video_started = 0;
				}
				else if(info->node == 1) {
					ImageInfo.Lcdc_layer = RDMA_FB1;
				}
				else {
					pr_err("[ERR][FB] fb ioctl : TCC_LCDC_DISPLAY_END can not find fb %d node \n", info->node);
					return -1;
				 }

				ImageInfo.enable = 0;
				tca_scale_display_update(&ptccfb_info->pdata.Mdp_data, (struct tcc_lcdc_image_update *)&ImageInfo);

				lcd_video_started = 0;
			}
			break;

		case TCC_LCDC_DISPLAY_UPDATE:
			{
				struct tcc_lcdc_image_update ImageInfo;

				if (copy_from_user((void *)&ImageInfo, (const void *)arg, sizeof(struct tcc_lcdc_image_update))){
					return -EFAULT;
				}

				tca_scale_display_update(&ptccfb_info->pdata.Mdp_data, (struct tcc_lcdc_image_update *)&ImageInfo);
				lcd_video_started = 1;
			}
			break;

#if defined(CONFIG_USE_DISPLAY_FB_LOCK)
			case TCC_FB_UPDATE_LOCK:
			{
				int fblock = 0;
				if (copy_from_user(&fblock, (const void*)arg, sizeof(int))) {
					return -EFAULT;
				}
				fb_lock = fblock;
				pr_info("[INF][FB] ioctl: fb_lock - [%d]\n", fb_lock);
			}
			break;
#endif
		case FBIO_DISABLE:
			{
				unsigned int type;
				struct tcc_dp_device *pdp_data = NULL;
				if (copy_from_user((void *)&type, (const void *)arg, sizeof(unsigned int ))) {
					return -EFAULT;
				}

				if(type == DD_MAIN)
					pdp_data = &ptccfb_info->pdata.Mdp_data;
				else
					pdp_data = &ptccfb_info->pdata.Sdp_data;
				#if defined(CONFIG_SYNC_FB)
				if(pdp_data)
				{

					if(pdp_data->rdma_info[RDMA_FB].virt_addr)
					{
						int sc_num = 0;
						volatile void __iomem *pscale_addr = NULL;
						sc_num = VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[RDMA_FB].blk_num);
						if(sc_num > 0)
							pscale_addr = VIOC_SC_GetAddress(sc_num);

						spin_lock_irq(&ptccfb_info->spin_lockDisp);
						#if 1
						VIOC_RDMA_SetImageAlphaEnable(pdp_data->rdma_info[RDMA_FB].virt_addr, 1);
						VIOC_RDMA_SetImageAlphaSelect(pdp_data->rdma_info[RDMA_FB].virt_addr, 0);
						#if defined(CONFIG_ARCH_TCC897X) /*|| defined(CONFIG_ARCH_TCC803X)*/
						VIOC_RDMA_SetImageAlpha(pdp_data->rdma_info[RDMA_FB].virt_addr, 0xFFF, 0xFFF);
						#else
						VIOC_RDMA_SetImageAlpha(pdp_data->rdma_info[RDMA_FB].virt_addr, 0x0, 0x0);
						#endif//
						#else //
						VIOC_RDMA_SetImageSize(pdp_data->rdma_info[RDMA_FB].virt_addr, 0, 0);

						if(pscale_addr){
							VIOC_SC_SetBypass(pscale_addr, 1);
							VIOC_SC_SetUpdate(pscale_addr);
						}
						#endif//
						VIOC_RDMA_SetImageUpdate(pdp_data->rdma_info[RDMA_FB].virt_addr);
						spin_unlock_irq(&ptccfb_info->spin_lockDisp);
					}
				}
				#endif//
			}
			break;

		case TCC_LCDC_FB_CHROMAKEY_CONTROL:
		case TCC_LCDC_FB_CHROMAKEY_CONTROL_KERNEL:
			{
				struct tcc_dp_device *pdp_data = NULL;

				lcdc_chromakey_params chromakey_ctrl;
				unsigned int chroma_en;
				unsigned int key_r, key_g, key_b;
				unsigned int mask_r, mask_g, mask_b;

				if (cmd == TCC_LCDC_FB_CHROMAKEY_CONTROL_KERNEL) {
					if (NULL == memcpy((void *)&chromakey_ctrl, (const void *)arg, sizeof(lcdc_chromakey_params)))
						return -EFAULT;
				} else {
					if (copy_from_user((void *)&chromakey_ctrl, (const void *)arg, sizeof(lcdc_chromakey_params)))
						return -EFAULT;
				}

				pdp_data = &ptccfb_info->pdata.Mdp_data;

				if(pdp_data)
				{
					if(pdp_data->wmixer_info.virt_addr)
					{
						chromakey_ctrl.lcdc_num = pdp_data->DispNum;
						chromakey_ctrl.layer_num = RDMA_FB;

						chroma_en = chromakey_ctrl.enable;
						key_r  = chromakey_ctrl.chromaR & 0xffff;
						key_g  = chromakey_ctrl.chromaG & 0xffff;
						key_b  = chromakey_ctrl.chromaB & 0xffff;
						mask_r = (chromakey_ctrl.chromaR >> 16) & 0xffff;
						mask_g = (chromakey_ctrl.chromaG >> 16) & 0xffff;
						mask_b = (chromakey_ctrl.chromaB >> 16) & 0xffff;

						VIOC_WMIX_SetChromaKey(pdp_data->wmixer_info.virt_addr, chromakey_ctrl.layer_num, 
											   chroma_en, key_r, key_g, key_b, mask_r, mask_g, mask_b);

						VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);

						if(chroma_en == 1)
							fb_chromakey_control_enabled = 1;
						else
							fb_chromakey_control_enabled = 0;

						pr_info("[INF][FB] FB_CHROMAKEY_CONTROL(%d), R[0x%04x] G[0x%04x] B[0x%04x], MR[0x%04x] MG[0x%04x] MB[0x%04x]\n",
											chroma_en, key_r, key_g, key_b, mask_r, mask_g, mask_b);
					}
				}
			}
			break;

	// VSYNC PART
		case TCC_LCDC_REFER_VSYNC_ENABLE:
			tca_vsync_enable(ptccfb_info, 1);
			break;

		case TCC_LCDC_REFER_VSYNC_DISABLE:
			tca_vsync_enable(ptccfb_info, 0);
			break;

		case FBIO_WAITFORVSYNC:
			tca_fb_wait_for_vsync(&ptccfb_info->pdata.Mdp_data);
			break;



// 2D Compression PART
	case GET_2D_COMPRESSION_FB_INFO:
		{
			struct pmap pmap_fb_video;

			if(info->node == 0)
				pmap_get_info("fb_video", &pmap_fb_video);
			else
				pmap_get_info("fb_video1", &pmap_fb_video);

			if (copy_to_user((void*)arg, &(pmap_fb_video.base), sizeof(unsigned int)*2))
				return -EFAULT;
		}
		break;

	case CHECK_2D_COMPRESSION_EN:
		{
			unsigned int ts_address;
			unsigned int bMark_ffff = 0;
			if (copy_from_user((void *)&ts_address, (const void *)arg, sizeof(unsigned int))){
				return -EFAULT;
			}
#if defined(CONFIG_SUPPORT_2D_COMPRESSION)
	#if !defined(CONFIG_ANDROID)
			if( 0 == tcc_2d_compression_read() )
				bMark_ffff = 1;
	#endif

			if( bMark_ffff && CONFIG_ADDITIONAL_TS_BUFFER_LINES != 0 )
			{
				unsigned int *remap_addr;

				remap_addr = (unsigned int *)ioremap_nocache(ts_address, 4096);
				if(remap_addr != NULL){
					remap_addr[0] = 0xffffffff;
					iounmap((void*)remap_addr);
				}
				else{
					pr_err("[ERR][FB] CHECK_2D_COMPRESSION_EN :: ioremap error for 0x%x \n", ts_address);
					return -EFAULT;
				}
			}
#else
			bMark_ffff = 1;
#endif
			if( bMark_ffff )
				ts_address = 0xffffffff;

			if (copy_to_user((void*)arg, &(ts_address), sizeof(unsigned int))){
				return -EFAULT;
			}
		}
		break;

	case TCC_LCDC_GET_DISP_FU_STATUS:
	    {
		    unsigned int fu_count=0;

		    fu_count = tca_fb_get_fifo_underrun_count();

		    if (copy_to_user((void *)arg, &fu_count, sizeof(unsigned int)))
			    return -EFAULT;

	    }
	    break;

	case TCC_LCDC_SET_COLOR_ENHANCE:
		{
			struct tcc_dp_device *pdp_data = NULL;
			struct lcdc_colorenhance_params params;

			if (copy_from_user((void *)&params, (const void *)arg, sizeof(struct lcdc_colorenhance_params))){
				return -EFAULT;
			}
			if(params.lcdc_type== DD_MAIN)
				pdp_data = &ptccfb_info->pdata.Mdp_data;
			else if(params.lcdc_type== DD_SUB)
				pdp_data = &ptccfb_info->pdata.Sdp_data;
			else
				return -EFAULT;

			if(pdp_data)
			{
				if(pdp_data->ddc_info.virt_addr)
				{
					#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
					pr_info("[INF][FB] TCC_LCDC_SET_COLOR_ENHANCE lcdc:0x%x contrast:0x%x saturation:0x%x brightness:0x%x hue:0x%x\n", params.lcdc_type, params.contrast, params.saturation, params.brightness, params.hue);
					if(params.hue < 0x100){
						VIOC_DISP_SetCENH_hue(pdp_data->ddc_info.virt_addr, params.hue);
						VIOC_DISP_DCENH_hue_onoff(pdp_data->ddc_info.virt_addr, 1);
					}
					else
						VIOC_DISP_DCENH_hue_onoff(pdp_data->ddc_info.virt_addr, 0);

					if((params.brightness >= 0x400) && (params.saturation >= 0x400) && (params.contrast >= 0x400))
						VIOC_DISP_DCENH_onoff(pdp_data->ddc_info.virt_addr, 0);
					else{
						if(params.brightness < 0x400)
							VIOC_DISP_SetCENH_brightness(pdp_data->ddc_info.virt_addr, params.brightness);
						if(params.saturation < 0x400)
							VIOC_DISP_SetCENH_saturation(pdp_data->ddc_info.virt_addr, params.saturation);
						if(params.contrast < 0x400)
							VIOC_DISP_SetCENH_contrast(pdp_data->ddc_info.virt_addr, params.contrast);

						VIOC_DISP_DCENH_onoff(pdp_data->ddc_info.virt_addr, 1);
					}	
						
					#else //CONFIG_ARCH_TCC803X, CONFIG_ARCH_TCC897X
					pr_info("[INF][FB] TCC_LCDC_SET_COLOR_ENHANCE lcdc:0x%x contrast:0x%x , brightness:0x%x hue:0x%x \n", params.lcdc_type, params.contrast, params.brightness, params.hue);
					VIOC_DISP_SetColorEnhancement(pdp_data->ddc_info.virt_addr,
						(signed char)params.contrast, (signed char)params.brightness, (signed char)params.hue);
					#endif			
				}
			}
		}
		break;
	case TCC_LCDC_GET_COLOR_ENHANCE:
		{
			struct tcc_dp_device *pdp_data = NULL;
			struct lcdc_colorenhance_params params;

			if (copy_from_user((void *)&params, (const void *)arg, sizeof(struct lcdc_colorenhance_params))){
				return -EFAULT;
			}
			if(params.lcdc_type== DD_MAIN)
				pdp_data = &ptccfb_info->pdata.Mdp_data;
			else if(params.lcdc_type== DD_SUB)
				pdp_data = &ptccfb_info->pdata.Sdp_data;
			else
				return -EFAULT;

			if(pdp_data)
			{
				if(pdp_data->ddc_info.virt_addr)
				{
					#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
					VIOC_DISP_GetCENH_hue(pdp_data->ddc_info.virt_addr,(unsigned int *)&params.hue);
					VIOC_DISP_GetCENH_brightness(pdp_data->ddc_info.virt_addr, (unsigned int *)&params.brightness);
					VIOC_DISP_GetCENH_saturation(pdp_data->ddc_info.virt_addr, (unsigned int *)&params.saturation);
					VIOC_DISP_GetCENH_contrast(pdp_data->ddc_info.virt_addr, (unsigned int *)&params.contrast);
					VIOC_DISP_GetCENH_hue_onoff(pdp_data->ddc_info.virt_addr, (unsigned int *)&params.check_hue_onoff);					
					VIOC_DISP_GetCENH_onoff(pdp_data->ddc_info.virt_addr, (unsigned int *)&params.check_colE_onoff);					
					pr_info("[INF][FB] TCC_LCDC_GET_COLOR_ENHANCE lcdc:0x%x hue:0x%x onoff:%d\n", params.lcdc_type, params.hue, params.check_hue_onoff);
					pr_info("[INF][FB] TCC_LCDC_GET_COLOR_ENHANCE lcdc:0x%x contrast:0x%x saturation:0x%x brightness:0x%x onoff:%d\n", params.lcdc_type, params.contrast, params.saturation, params.brightness, params.check_colE_onoff);
					#else //CONFIG_ARCH_TCC803X, CONFIG_ARCH_TCC897X

					VIOC_DISP_GetColorEnhancement(pdp_data->ddc_info.virt_addr,
						(signed char *)&params.contrast, (signed char *)&params.brightness, (signed char *)&params.hue);
					pr_info("[INF][FB] TCC_LCDC_SET_COLOR_ENHANCE lcdc:%d contrast:%d brightness:%d hue:%d \n", params.lcdc_type, params.contrast, params.brightness, params.hue);
					#endif			
				}
			}
			if (copy_to_user((void *)arg, &params, sizeof(struct lcdc_colorenhance_params))){
				return -EFAULT;
			}
		}
		break;

	case TCC_LCDC_FB_SWAP_VPU_FRAME:
		{
		#if defined(CONFIG_VIDEO_DISPLAY_SWAP_VPU_FRAME) && defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
			int ret;
			WMIXER_INFO_TYPE WmixerInfo;
			struct tcc_lcdc_image_update *TempImage;
			struct tcc_dp_device *pdp_data = NULL;

			pr_info("[INF][FB] TCC_LCDC_FB_SWAP_VPU_FRAME idx(%ld)\n", arg);

			if (ptccfb_info->image_enable == 0) {
				return 0;
			}
			if (ptccfb_info->swap_buf_status != SWAP_BUF_END) {
				return 0;
			}

			if (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI
				&& ptccfb_info->pdata.Mdp_data.FbPowerState)
			{
				pdp_data = &ptccfb_info->pdata.Mdp_data;
			}
			else if (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI
				&& ptccfb_info->pdata.Sdp_data.FbPowerState)
			{
				pdp_data = &ptccfb_info->pdata.Sdp_data;
			}
			else
			{
				pr_warn("[WAN][FB] TCC_LCDC_FB_SWAP_VPU_FRAME (M:%d, S:%d)\n",
					ptccfb_info->pdata.Mdp_data.DispDeviceType,
					ptccfb_info->pdata.Sdp_data.DispDeviceType);
				return -ENODEV;
			}

			TempImage = (struct tcc_lcdc_image_update *)kmalloc(sizeof(struct tcc_lcdc_image_update), GFP_KERNEL);
			if (!TempImage) {
				pr_err("[ERR][FB] TCC_LCDC_FB_SWAP_VPU_FRAME kmalloc\n");
				return -EFAULT;
			}

			ret = tcc_fb_swap_vpu_frame(pdp_data, &WmixerInfo, TempImage, VSYNC_MAIN);

			kfree((const void*)TempImage);

			if (ret < 0) {
				pr_err("[ERR][FB] TCC_LCDC_FB_SWAP_VPU_FRAME swap\n");
				return -EFAULT;
			}

			//ptccfb_info->swap_buf_id = ret;
			ptccfb_info->swap_buf_id = arg;
			ptccfb_info->swap_buf_status = SWAP_BUF_START;

		#endif
			break;
		}

	default:
		dprintk("ioctl: Unknown [%d/0x%X]", cmd, cmd);
		break;
	}


	return 0;
}

static void schedule_palette_update(struct tccfb_info *fbi,
				    unsigned int regno, unsigned int val)
{
	unsigned long flags;

	local_irq_save(flags);

	local_irq_restore(flags);
}

/* from pxafb.c */
static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int tccfb_setcolreg(unsigned regno,
			       unsigned red, unsigned green, unsigned blue,
			       unsigned transp, struct fb_info *info)
{
	struct tccfb_info *fbi = info->par;
	unsigned int val;

	/* dprintk("setcol: regno=%d, rgb=%d,%d,%d\n", regno, red, green, blue); */

	switch (fbi->fb->fix.visual) {
		case FB_VISUAL_TRUECOLOR:
			/* true-colour, use pseuo-palette */

			if (regno < 16) {
				u32 *pal = fbi->fb->pseudo_palette;

				val  = chan_to_field(red,   &fbi->fb->var.red);
				val |= chan_to_field(green, &fbi->fb->var.green);
				val |= chan_to_field(blue,  &fbi->fb->var.blue);

				pal[regno] = val;
			}
			break;

		case FB_VISUAL_PSEUDOCOLOR:
			if (regno < 256) {
				/* currently assume RGB 5-6-5 mode */

				val  = ((red   >>  0) & 0xf800);
				val |= ((green >>  5) & 0x07e0);
				val |= ((blue  >> 11) & 0x001f);

				//writel(val, S3C2410_TFTPAL(regno));
				schedule_palette_update(fbi, regno, val);
			}
			break;

		default:
			return 1;   /* unknown type */
	}

	return 0;
}


/**
 *      tccfb_blank
 *	@blank_mode: the blank mode we want.
 *	@info: frame buffer structure that represents a single frame buffer
 *
 *	Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *	blanking succeeded, != 0 if un-/blanking failed due to e.g. a
 *	video mode which doesn't support it. Implements VESA suspend
 *	and powerdown modes on hardware that supports disabling hsync/vsync:
 *	blank_mode == 2: suspend vsync
 *	blank_mode == 3: suspend hsync
 *	blank_mode == 4: powerdown
 *
 *	Returns negative errno on error, or zero on success.
 *
 */
#ifdef CONFIG_PM
static int tcc_fb_enable(struct fb_info *info)
{
	int ret = 0;
	struct tccfb_info *fbi =(struct tccfb_info *) info->par;

	mutex_lock(&fbi->output_lock);
	if(fbi->output_on) {
		ret = -EBUSY;
		goto err;
	}

	fbi->output_on = true;
	pm_runtime_get_sync(fbi->dev);

err:
	mutex_unlock(&fbi->output_lock);
	return ret;
}

static int tcc_fb_disable(struct fb_info *info)
{
	int ret = 0;
	struct tccfb_info *fbi =(struct tccfb_info *) info->par;

	mutex_lock(&fbi->output_lock);

#if defined(CONFIG_SYNC_FB)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	flush_kthread_worker(&fbi->fb_update_regs_worker);
	flush_kthread_worker(&fbi->ext_update_regs_worker);
#else
	kthread_flush_worker(&fbi->fb_update_regs_worker);
	kthread_flush_worker(&fbi->ext_update_regs_worker);
#endif
#endif//
	if(!fbi->output_on) {
		ret = -EBUSY;
		goto err;
	}
	fbi->output_on = false;
	pm_runtime_put_sync(fbi->dev);

err:
	mutex_unlock(&fbi->output_lock);
	return ret;
}
#endif



static int tccfb_blank(int blank_mode, struct fb_info *info)
{
#ifdef CONFIG_PM
	int ret = 0;

	struct tccfb_info *fbi =(struct tccfb_info *) info->par;
	#if defined(CONFIG_MALI400_UMP)
	return 0; // For mali400 X display drivers
	#endif

	pm_runtime_get_sync(fbi->dev);

	switch(blank_mode)
	{
		case FB_BLANK_POWERDOWN:
		case FB_BLANK_NORMAL:
			tcc_fb_disable(info);
			break;
		case FB_BLANK_UNBLANK:
			tcc_fb_enable(info);
			break;
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		default:
			ret = -EINVAL;
	}
	pm_runtime_put_sync(fbi->dev);

#endif

	return 0;
}

#ifdef CONFIG_DMA_SHARED_BUFFER
#ifdef CONFIG_ANDROID
static struct sg_table *fb_ion_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	int err;
	struct sg_table *table;
	struct fb_info *info = (struct fb_info *)attachment->dmabuf->priv;

	if (info == NULL) {
		pr_err("[ERR][FB] %s: info is null\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (table == NULL) {
		pr_err("[ERR][FB] %s: kzalloc failed\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	err = sg_alloc_table(table, 1, GFP_KERNEL);
	if (err) {
		kfree(table);
		return ERR_PTR(err);
	}

	sg_set_page(table->sgl, NULL, info->fix.smem_len, 0);
	sg_dma_address(table->sgl) = info->fix.smem_start;
	debug_dma_map_sg(NULL, table->sgl, table->nents, table->nents, DMA_BIDIRECTIONAL);

	return table;
}

static void fb_ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	debug_dma_unmap_sg(NULL, table->sgl, table->nents, DMA_BIDIRECTIONAL);
	sg_free_table(table);

	kfree(table);
}

static int fb_ion_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	return -ENOSYS;
}

static void fb_ion_dma_buf_release(struct dma_buf *dmabuf)
{
	return;
}

static void *fb_ion_dma_buf_map(struct dma_buf *dmabuf, unsigned long offset)
{
	return 0;
}


struct dma_buf_ops fb_dma_buf_ops = {
	.map_dma_buf = fb_ion_map_dma_buf,
	.unmap_dma_buf = fb_ion_unmap_dma_buf,
	.mmap = fb_ion_mmap,
	.release = fb_ion_dma_buf_release,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,11,12)
	.kmap_atomic = fb_ion_dma_buf_map,
	.kmap = fb_ion_dma_buf_map,
#else
	.map_atomic = fb_ion_dma_buf_map,
	.map = fb_ion_dma_buf_map,
#endif
};

struct dma_buf* tccfb_dmabuf_export(struct fb_info *info)
{
	struct dma_buf *dmabuf;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,0,9)
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	exp_info.ops = &fb_dma_buf_ops;
	exp_info.size = info->fix.smem_len;
	exp_info.flags = O_RDWR;
	exp_info.priv = info;
	dmabuf = dma_buf_export(&exp_info);
#else
	dmabuf = dma_buf_export(info, &fb_dma_buf_ops, info->fix.smem_len, O_RDWR, NULL);
#endif

	return dmabuf;
}
#else

static int dmabuf_attach(struct dma_buf *buf, struct device *dev,
			 struct dma_buf_attachment *attach)
{
	return 0;
}

static void dmabuf_detach(struct dma_buf *buf,
			  struct dma_buf_attachment *attach)
{
}

static struct sg_table *dmabuf_map_dma_buf(struct dma_buf_attachment *attach,
					   enum dma_data_direction dir)
{
	struct fb_info *priv = attach->dmabuf->priv;
	struct sg_table *sgt;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return NULL;

	if (sg_alloc_table(sgt, 1, GFP_KERNEL)) {
		kfree(sgt);
		return NULL;
	}

	sg_dma_address(sgt->sgl) = priv->fix.smem_start;
	sg_dma_len(sgt->sgl) = priv->fix.smem_len;

	return sgt;
}

static void dmabuf_unmap_dma_buf(struct dma_buf_attachment *attach,
				 struct sg_table *sgt,
				 enum dma_data_direction dir)
{
	sg_free_table(sgt);
	kfree(sgt);
}

static void dmabuf_release(struct dma_buf *buf)
{
}

static void *dmabuf_kmap(struct dma_buf *buf, unsigned long page)
{
	struct fb_info *priv = buf->priv;
	return priv->screen_base + page;
}

static void dmabuf_kunmap(struct dma_buf *buf, unsigned long page, void *vaddr)
{
}

static void *dmabuf_kmap_atomic(struct dma_buf *buf, unsigned long page)
{
	return NULL;
}

static void dmabuf_kunmap_atomic(struct dma_buf *buf, unsigned long page,
				 void *vaddr)
{
}

static void dmabuf_vm_open(struct vm_area_struct *vma)
{
}

static void dmabuf_vm_close(struct vm_area_struct *vma)
{
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,10,17)
static int dmabuf_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
#else
static int dmabuf_vm_fault(struct vm_fault *vmf)
#endif
{
	return 0;
}

static const struct vm_operations_struct dmabuf_vm_ops = {
	.open = dmabuf_vm_open,
	.close = dmabuf_vm_close,
	.fault = dmabuf_vm_fault,
};

static int dmabuf_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
	pgprot_t prot = vm_get_page_prot(vma->vm_flags);
	struct fb_info *priv = buf->priv;

	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_ops = &dmabuf_vm_ops;
	vma->vm_private_data = priv;
	vma->vm_page_prot = pgprot_writecombine(prot);

	return remap_pfn_range(vma, vma->vm_start, priv->fix.smem_start >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static void *dmabuf_vmap(struct dma_buf *buf)
{
	return NULL;
}

static void dmabuf_vunmap(struct dma_buf *buf, void *vaddr)
{
}

static const struct dma_buf_ops dmabuf_ops = {
	.attach = dmabuf_attach,
	.detach = dmabuf_detach,
	.map_dma_buf = dmabuf_map_dma_buf,
	.unmap_dma_buf = dmabuf_unmap_dma_buf,
	.release = dmabuf_release,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,11,12)
	.kmap_atomic = dmabuf_kmap_atomic,
	.kunmap_atomic = dmabuf_kunmap_atomic,
	.kmap = dmabuf_kmap,
	.kunmap = dmabuf_kunmap,
#else
	.map_atomic = dmabuf_kmap_atomic,
	.unmap_atomic = dmabuf_kunmap_atomic,
	.map = dmabuf_kmap,
	.unmap = dmabuf_kunmap,
#endif
	.mmap = dmabuf_mmap,
	.vmap = dmabuf_vmap,
	.vunmap = dmabuf_vunmap,
};

struct dma_buf* tccfb_dmabuf_export(struct fb_info *info)
{
	struct dma_buf *dmabuf;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,0,9)
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	exp_info.ops = &dmabuf_ops;
	exp_info.size = info->fix.smem_len;
	exp_info.flags = O_RDWR;
	exp_info.priv = info;
	dmabuf = dma_buf_export(&exp_info);
#else
	dmabuf = dma_buf_export(info, &dmabuf_ops, info->fix.smem_len, O_RDWR, NULL);
#endif

	return dmabuf;
}

#endif
#endif	//CONFIG_DMA_SHARED_BUFFER


static struct fb_ops tccfb_ops = {
	.owner			= THIS_MODULE,
	.fb_check_var	= tccfb_check_var,
	.fb_set_par		= tccfb_set_par,
	.fb_blank		= tccfb_blank,
	.fb_setcolreg	= tccfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_ioctl		= tccfb_ioctl,
	.fb_pan_display = tccfb_pan_display,
#ifdef CONFIG_DMA_SHARED_BUFFER
	.fb_dmabuf_export = tccfb_dmabuf_export,
#endif
};

/*
 * tccfb_map_video_memory():
 *	Allocates the DRAM memory for the frame buffer.  This buffer is
 *	remapped into a non-cached, non-buffered, memory region to
 *	allow palette and pixel writes to occur without flushing the
 *	cache.  Once this area is remapped, all virtual memory
 *	access to the video memory should occur at the new region.
 */

static int __init tccfb_map_video_memory(struct tccfb_info *fbi, int plane)
{
	struct device_node *of_node = fbi->dev->of_node;
	struct device_node *mem_region;
	struct resource res;
	int ret;
#if 0
	struct pmap pmap_fb_video;

	switch (plane) {
	case 0:
		pmap_get_info("fb_video", &pmap_fb_video);
		break;

	case 1:
		pmap_get_info("fb1_video", &pmap_fb_video);
		break;
	}

	if (pmap_fb_video.size == 0) {
		fbi->map_size  = fbi->fb->var.xres_virtual * fbi->fb->var.yres_virtual * (fbi->fb->var.bits_per_pixel/ 8);
		fbi->map_cpu = dma_alloc_writecombine(fbi->dev, fbi->map_size, &fbi->map_dma, GFP_KERNEL);
		pr_info("map_video_memory (fbi=%p) kernel memory, dma:0x%x map_size:%08x\n", fbi,fbi->map_dma, fbi->map_size);
	} else {
		fbi->map_dma = pmap_fb_video.base;
		fbi->map_size = pmap_fb_video.size;
		fbi->map_cpu = ioremap_nocache(fbi->map_dma, fbi->map_size);
		pr_info("[INF][FB] plane: %d  map_video_memory (fbi=%p) used map memory,map dma:0x%x cpu:%p   size:%08x\n", plane, fbi, fbi->map_dma, fbi->map_cpu, fbi->map_size);
	}
#else
	if (plane >= CONFIG_FB_TCC_DEVS)
		return -ENODEV;

	mem_region = of_parse_phandle(of_node, "memory-region", plane);
	if (!mem_region) {
		dev_err(fbi->dev, "[ERR][FB] no memory regions\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(mem_region, 0, &res);
	of_node_put(mem_region);
	if (ret)
		dev_warn(fbi->dev, "[WAR][FB] failed to translate memory regions (%d)\n", ret);

	if (ret || resource_size(&res) == 0) {
		fbi->map_size  = fbi->fb->var.xres_virtual * fbi->fb->var.yres_virtual * (fbi->fb->var.bits_per_pixel/ 8);
		fbi->map_cpu = dma_alloc_writecombine(fbi->dev, fbi->map_size, &fbi->map_dma, GFP_KERNEL);
		if (fbi->map_cpu == NULL) {
			pr_err("[ERR][FB] %s: error dma_alloc map_cpu\n", __func__);
			goto exit;
		}
		dprintk("%s by dma_alloc_writecombine()\n", __func__);
	} else {
		fbi->map_dma = res.start;
		fbi->map_size = resource_size(&res);
		fbi->map_cpu = ioremap_nocache(fbi->map_dma, fbi->map_size);
		if (fbi->map_cpu == NULL) {
			pr_err("[ERR][FB] %s: error ioremap map_cpu\n", __func__);
			goto exit;
		}
		dprintk("%s by ioremap_nocache()\n", __func__);
	}
#endif

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	dprintk("%s: plane(%d) fbi(%p) phy(0x%llx)=>virt(%p) size(0x%x)\n", __func__, plane, fbi, fbi->map_dma, fbi->map_cpu, fbi->map_size);
#else
	dprintk("%s: plane(%d) fbi(%p) phy(0x%x)=>virt(%p) size(0x%x)\n", __func__, plane, fbi, fbi->map_dma, fbi->map_cpu, fbi->map_size);
#endif

	if (fbi->map_cpu) {
		unsigned int tca_get_scaler_num(TCC_OUTPUT_TYPE Output, unsigned int Layer);

		/* prevent initial garbage on screen */
		#if defined(CONFIG_LOGO_PRESERVE_WITHOUT_FB_INIT)
		struct pmap pmap;
		int scnum;
		volatile void __iomem *pSC = NULL;
		volatile void __iomem *pWDMA = NULL;
		
		pmap_get_info("viqe", &pmap);
		hdmi_base_address = (unsigned int)pmap.base;
		scnum = tca_get_scaler_num(TCC_OUTPUT_HDMI, RDMA_VIDEO);

		pSC = VIOC_SC_GetAddress(scnum);
		pWDMA = VIOC_WDMA_GetAddress(fbi->pdata.Mdp_data.DispNum);
		if(hdmi_base_address != 0 && pWDMA != NULL && pSC != NULL) {
			VIOC_CONFIG_PlugOut(scnum);
			VIOC_CONFIG_SWReset(scnum, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(scnum, VIOC_CONFIG_CLEAR);

			VIOC_SC_SetBypass (pSC, OFF);
			VIOC_SC_SetDstSize (pSC, fbi->fb->var.xres, fbi->fb->var.yres);
			VIOC_SC_SetOutSize (pSC, fbi->fb->var.xres, fbi->fb->var.yres);
			VIOC_CONFIG_PlugIn(scnum, VIOC_WDMA | get_vioc_index(fbi->pdata.Mdp_data.DispNum));
			
			VIOC_WDMA_SetImageFormat(pWDMA, (fbi->fb->var.bits_per_pixel==32)?TCC_LCDC_IMG_FMT_RGB888:TCC_LCDC_IMG_FMT_RGB565);
			VIOC_WDMA_SetImageSize(pWDMA, fbi->fb->var.xres, fbi->fb->var.yres);
			VIOC_WDMA_SetImageOffset(pWDMA, (fbi->fb->var.bits_per_pixel==32)?TCC_LCDC_IMG_FMT_RGB888:TCC_LCDC_IMG_FMT_RGB565, fbi->fb->var.xres);
			VIOC_WDMA_SetImageBase(pWDMA, hdmi_base_address, 0, 0);
			VIOC_SC_SetUpdate (pSC);
			VIOC_WDMA_SetImageEnable(pWDMA, 0);
			mdelay(50);
			VIOC_CONFIG_PlugOut(scnum);

			pr_info("[INF][FB] Capture bootlogo to VIQE memory at 0x%x\r\n", hdmi_base_address);
		} else {
			pr_info("[INF][FB] SKIP Capture bootlogo to VIQE memory\r\n");
		}
		/* CONFIG_LOGO_PRESERVE_WITHOUT_FB_INIT */
		#else
		#if !defined(CONFIG_LOGO) && defined(CONFIG_PLATFORM_AVN) && !defined(CONFIG_ANDROID)
		volatile void __iomem * pWDMA;
		pWDMA = VIOC_WDMA_GetAddress(fbi->pdata.Mdp_data.DispNum);
		VIOC_WDMA_SetImageFormat(pWDMA, (fbi->fb->var.bits_per_pixel==32)?TCC_LCDC_IMG_FMT_RGB888:TCC_LCDC_IMG_FMT_RGB565);
		VIOC_WDMA_SetImageSize(pWDMA, fbi->fb->var.xres, fbi->fb->var.yres);
		VIOC_WDMA_SetImageOffset(pWDMA, (fbi->fb->var.bits_per_pixel==32)?TCC_LCDC_IMG_FMT_RGB888:TCC_LCDC_IMG_FMT_RGB565, fbi->fb->var.xres);
		VIOC_WDMA_SetImageBase(pWDMA, (unsigned int)fbi->map_dma, 0, 0);
		VIOC_WDMA_SetImageEnable(pWDMA, 0);
		dprintk("%s: keep bootlogo (doen't clear mem)\n", __func__);
		/* !CONFIG_LOGO && CONFIG_PLATFORM_AVN && !CONFIG_ANDROID */
		#else
		memset_io(fbi->map_cpu, 0x00, fbi->map_size);
		dprintk("%s: clear fb mem\n", __func__);
		/* !(!CONFIG_LOGO && CONFIG_PLATFORM_AVN && !CONFIG_ANDROID) */
		#endif
		/* !CONFIG_LOGO_PRESERVE_WITHOUT_FB_INIT */
		#endif
		fbi->screen_dma		= fbi->map_dma;
		fbi->fb->screen_base	= fbi->map_cpu;
		fbi->fb->fix.smem_start  = fbi->screen_dma;
		fbi->fb->fix.smem_len = fbi->map_size;
	}

exit:
	return fbi->map_cpu ? 0 : -ENOMEM;
}

static inline void tccfb_unmap_video_memory(struct tccfb_info *fbi)
{
	if (fbi->map_cpu)
		dma_free_writecombine(fbi->dev,fbi->map_size,fbi->map_cpu, fbi->map_dma);
}


static int tcc_vioc_set_rdma_arbitor(struct device_node *np)
{
	void __iomem *virt_addr=NULL;
	int num_of_arbitor;
	int ret=0;
	int i;

	if (of_property_read_u32(np, "arbitor_num", &num_of_arbitor)){
	   pr_err( "[ERR][FB] could not find num_of_arbitor nubmer\n");
	   ret = -ENODEV;
	   return ret;
	}

	for(i = 0; i < num_of_arbitor; i++) {
		virt_addr = of_iomap(np, is_VIOC_REMAP ? (i + num_of_arbitor) : i);
		*(volatile uint *)virt_addr = ((1<<31) | (0<<28) | (2<<16) | (0<<12) | (10<<0));
	}

	return ret;
}

static int tcc_dp_dt_parse_data(struct tccfb_info *info)
{
	int ret = 0;
	struct device_node *main_np, *extend_np;
	struct device_node *rdma_arbitor_np=NULL;

	if (info->dev->of_node) {
		// Get default display number.
		if (of_property_read_u32(info->dev->of_node, "telechips,fbdisplay_num", &info->pdata.lcdc_number)){
			pr_err("[ERR][FB] could not find  telechips,fbdisplay_nubmer\n");
			ret = -ENODEV;
		}

		if(info->pdata.lcdc_number) {
			main_np = of_find_node_by_name(info->dev->of_node, "fbdisplay1");
			extend_np = of_find_node_by_name(info->dev->of_node, "fbdisplay0");
		}
		else {
			main_np = of_find_node_by_name(info->dev->of_node, "fbdisplay0");
			extend_np = of_find_node_by_name(info->dev->of_node, "fbdisplay1");
		}

		if (!main_np) {
			pr_err("[ERR][FB] could not find fb node number %d\n", info->pdata.lcdc_number);
			return -ENODEV;
		}

		// Default display block
		tcc_vioc_display_dt_parse(main_np, &info->pdata.Mdp_data);
		info->pdata.lcdc_number = info->pdata.Mdp_data.DispNum;


		// Sub displayblock
		tcc_vioc_display_dt_parse(extend_np, &info->pdata.Sdp_data);

		#if defined(CONFIG_BUILTIN_2ND_DISPLAY_ON_1ST_DISPLAY)
		memcpy(&info->pdata.Sdp_data.ddc_info,
			&info->pdata.Mdp_data.ddc_info, sizeof(info->pdata.Mdp_data.ddc_info));
		info->pdata.Sdp_data.FbPowerState = 0;
		memcpy(&info->pdata.Sdp_data.wmixer_info,
			&info->pdata.Mdp_data.wmixer_info, sizeof(info->pdata.Mdp_data.wmixer_info));
		memcpy(&info->pdata.Sdp_data.rdma_info[RDMA_FB],
			&info->pdata.Mdp_data.rdma_info[1], sizeof(info->pdata.Mdp_data.rdma_info[0]));
		#endif

		// Set rdma arbitor resgister and disable disp emergency flag according to SOC guide for TCC896x
		rdma_arbitor_np = of_parse_phandle(info->dev->of_node, "telechips,rdma_arbitor",0);
		if(rdma_arbitor_np) {
			VIOC_DISP_EmergencyFlagDisable(info->pdata.Mdp_data.ddc_info.virt_addr);
			VIOC_DISP_EmergencyFlagDisable(info->pdata.Sdp_data.ddc_info.virt_addr);
			tcc_vioc_set_rdma_arbitor(rdma_arbitor_np);
		 }

	}
	return ret;

}

static char tccfb_driver_name[]="tccfb";
static int tccfb_probe(struct platform_device *pdev)
{
	struct tccfb_info *info;
	struct fb_info *fbinfo;

	int ret = 0 , plane = 0;
	unsigned int screen_width, screen_height;

	if (lcd_panel == NULL) {
		pr_err("[ERR][FB] no LCD panel data\n");
		return -EINVAL;
	}

// 	const struct of_device_id *of_id = of_match_device(tccfb_of_match, &pdev->dev);

	pr_info("\x1b[1;38m [INF][FB] LCD panel is %s %s %d x %d \x1b[0m \n", lcd_panel->manufacturer, lcd_panel->name, lcd_panel->xres, lcd_panel->yres);

        if(display_ext_panel != NULL) {
                pr_info("\x1b[1;38m [INF][FB] Extended panel is %s %s %d x %d \x1b[0m \n",
                        display_ext_panel->manufacturer, display_ext_panel->name, display_ext_panel->xres, display_ext_panel->yres);
        }

    screen_width      = lcd_panel->xres;
    screen_height     = lcd_panel->yres;

	pr_info("[INF][FB] %s, screen_width=%d, screen_height=%d \n", __func__, screen_width, screen_height);


	for(plane = 0; plane < CONFIG_FB_TCC_DEVS; plane++)
	{
		struct tccfb_info *info_reg;
		struct fb_info *fbinfo_reg;
		int err;

		fbinfo_reg = framebuffer_alloc(sizeof(struct tccfb_info), &pdev->dev);
		info_reg = fbinfo_reg->par;
		info_reg->fb = fbinfo_reg;
		info_reg->dev = &pdev->dev;

		if(plane ==0){
			info = info_reg;
			fbinfo = fbinfo_reg;

		}
		platform_set_drvdata(pdev, info_reg);

		tcc_dp_dt_parse_data(info_reg);

		/* kobject_rename - change the name of an object */
		err = kobject_rename(&pdev->dev.kobj, "tccfb");
		if (err) {
			framebuffer_release(fbinfo_reg);
			return -ENOMEM;
		}

		strcpy(fbinfo_reg->fix.id, tccfb_driver_name);

		fbinfo_reg->fix.type			= FB_TYPE_PACKED_PIXELS;
		fbinfo_reg->fix.type_aux		= 0;
		fbinfo_reg->fix.xpanstep		= 0;
		fbinfo_reg->fix.ypanstep		= 1;
		fbinfo_reg->fix.ywrapstep		= 0;
		fbinfo_reg->fix.accel			= FB_ACCEL_NONE;
		fbinfo_reg->fix.visual = FB_VISUAL_TRUECOLOR;
		fbinfo_reg->fix.type = FB_TYPE_PACKED_PIXELS;

		fbinfo_reg->var.nonstd			= 0;
		fbinfo_reg->var.activate		= FB_ACTIVATE_VBL;		// 160614 inbest
		fbinfo_reg->var.accel_flags		= 0;
		fbinfo_reg->var.vmode			= FB_VMODE_NONINTERLACED;

		fbinfo_reg->fbops				= &tccfb_ops;
		fbinfo_reg->flags				= FBINFO_FLAG_DEFAULT;

		fbinfo_reg->var.xres			= screen_width;
		fbinfo_reg->var.xres_virtual	= fbinfo_reg->var.xres;
		fbinfo_reg->var.yres			= screen_height;

		fbinfo_reg->var.yres_virtual	= fbinfo_reg->var.yres * FB_NUM_BUFFERS;
		fbinfo_reg->var.bits_per_pixel	= default_scn_depth[plane];
		fbinfo_reg->fix.line_length 	= fbinfo_reg->var.xres * fbinfo_reg->var.bits_per_pixel/8;


#if defined(CONFIG_ADDITIONAL_TS_BUFFER_LINES)
		fbinfo_reg->var.reserved[0] = CONFIG_ADDITIONAL_TS_BUFFER_LINES;
#endif

		fbinfo_reg->pseudo_palette          = devm_kzalloc(&pdev->dev, sizeof(u32) * 16, GFP_KERNEL);
		if (!fbinfo_reg->pseudo_palette) {
				pr_err("Failed to allocate pseudo_palette\r\n");
				ret = -ENOMEM;
				goto free_framebuffer;
		}

		info_reg->output_on = true;
		// have to modify pjj
		// panel init function E??aC?????? AE??a
		info_reg->pdata.Mdp_data.DispOrder = DD_MAIN;
		info_reg->pdata.Sdp_data.DispOrder = DD_SUB;

		info_reg->pdata.Mdp_data.FbPowerState = true;
		info_reg->pdata.Mdp_data.FbUpdateType = FB_RDMA_UPDATE;
		info_reg->pdata.Mdp_data.DispDeviceType = TCC_OUTPUT_LCD;

		info_reg->image_enable = 0;
		info_reg->swap_buf_id = 0;
		info_reg->swap_buf_status = SWAP_BUF_END;

		tccfb_check_var(&fbinfo_reg->var, fbinfo_reg);

		/* Initialize video memory */
		ret = tccfb_map_video_memory(info_reg, plane);
		if (ret  < 0) {
			pr_err("[ERR][FB] Failed to allocate video RAM: %d\n", ret);
			ret = -ENOMEM;
			#if defined(CONFIG_PLATFORM_AVN) && !defined(CONFIG_ANDROID)
			goto free_palette;
			#else
			        framebuffer_release(fbinfo_reg);
				continue;
			#endif
		}

		ret = register_framebuffer(fbinfo_reg);
		if (ret < 0) {
			pr_err("[ERR][FB] Failed to register framebuffer device: %d\n", ret);
			goto free_video_memory;
		}

		if (plane == 0)	{// top layer
			if (fb_prepare_logo(fbinfo_reg, FB_ROTATE_UR)) {
				/* Start display and show logo on boot */
				pr_info("[INF][FB] fb_show_logo\n");
				#if 0
				// 'fbinfo_reg->cmap' dosen't have any data!!!
				fb_set_cmap(&fbinfo_reg->cmap, fbinfo);
				#else
				// So, we use fb_alloc_cmap_gfp function(fb_default_camp(default_16_colors))
				fb_alloc_cmap_gfp(&fbinfo->cmap, 16, 0, GFP_KERNEL);
				fb_alloc_cmap_gfp(&fbinfo_reg->cmap, 16, 0, GFP_KERNEL);
				#endif
				fb_show_logo(fbinfo_reg, FB_ROTATE_UR);
        		}
       	}
		spin_lock_init(&info_reg->spin_lockDisp);
		pr_info("[INF][FB] fb%d: %s frame buffer device info->dev:0x%p  \n", fbinfo->node, fbinfo->fix.id, info->dev);
	}

	#ifdef CONFIG_PM
	pm_runtime_set_active(info->dev);
	pm_runtime_enable(info->dev);
	pm_runtime_get_noresume(info->dev);  //increase usage_count
	#endif

	mutex_init(&info->output_lock);

	info->pdata.Mdp_data.pandisp_sync.state = 1;
	init_waitqueue_head(&info->pdata.Mdp_data.pandisp_sync.waitq);

	info->pdata.Mdp_data.disp_dd_sync.state = 1;
	init_waitqueue_head(&info->pdata.Mdp_data.disp_dd_sync.waitq);

	info->pdata.Sdp_data.pandisp_sync.state = 1;
	init_waitqueue_head(&info->pdata.Sdp_data.pandisp_sync.waitq);

	info->pdata.Sdp_data.disp_dd_sync.state = 1;
	init_waitqueue_head(&info->pdata.Sdp_data.disp_dd_sync.waitq);

	INIT_WORK(&info->vsync_work, send_vsync_event);

#if defined(CONFIG_SYNC_FB)
	INIT_LIST_HEAD(&info->fb_update_regs_list);
	mutex_init(&info->fb_timeline_lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	init_kthread_worker(&info->fb_update_regs_worker);
#else
	kthread_init_worker(&info->fb_update_regs_worker);
#endif
	info->fb_update_regs_thread = kthread_run(kthread_worker_fn,
			&info->fb_update_regs_worker, "tccfb");

	if (IS_ERR(info->fb_update_regs_thread)) {
		int err = PTR_ERR(info->fb_update_regs_thread);
		info->fb_update_regs_thread = NULL;

		pr_err("[ERR][FB] failed to run update_regs thread\n");
		return err;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	init_kthread_work(&info->fb_update_regs_work, fence_handler);
#else
	kthread_init_work(&info->fb_update_regs_work, fence_handler);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
	info->fb_timeline = sw_sync_timeline_create("tccfb");
#else
	info->fb_timeline = (struct sw_sync_timeline *)sync_timeline_create("tccfb");
#endif
	info->fb_timeline_max	=  0;

	INIT_LIST_HEAD(&info->ext_update_regs_list);
	mutex_init(&info->ext_timeline_lock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	init_kthread_worker(&info->ext_update_regs_worker);
#else
	kthread_init_worker(&info->ext_update_regs_worker);
#endif

	info->ext_update_regs_thread = kthread_run(kthread_worker_fn,
			&info->ext_update_regs_worker, "tccfb-ext");

	if (IS_ERR(info->ext_update_regs_thread)) {
		int err = PTR_ERR(info->ext_update_regs_thread);
		info->ext_update_regs_thread = NULL;

		pr_err("[ERR][FB] failed to run update_regs thread\n");
		return err;
	}

	#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	init_kthread_work(&info->ext_update_regs_work, ext_fence_handler);
	#else
	kthread_init_work(&info->ext_update_regs_work, ext_fence_handler);
	#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
	info->ext_timeline = sw_sync_timeline_create("tccfb-ext");
	info->ext_timeline_max	=  0;
#else
	info->ext_timeline = (struct sw_sync_timeline *)sync_timeline_create("tccfb-ext");
#endif
#endif

	if(lcd_panel->init)
		lcd_panel->init(lcd_panel, &info->pdata.Mdp_data);

        if(display_ext_panel != NULL && display_ext_panel->init != NULL) {
		display_ext_panel->init(display_ext_panel, &info->pdata.Sdp_data);
        }

	tca_fb_init(info);
	tca_main_interrupt_reg(true, info);

#ifdef CONFIG_ANDROID
	#ifdef CONFIG_PLATFORM_STB
	#if !defined(CONFIG_LOGO_PRESERVE_WITHOUT_FB_INIT)
	tcafb_activate_var(info, &fbinfo->var);
	#endif
	tccfb_set_par(fbinfo);
	#endif
#else	/* Linux */
	#ifdef CONFIG_PLATFORM_AVN
	tcafb_activate_var(info, &fbinfo->var);
	tccfb_set_par(fbinfo);
	#endif
#endif
	return 0;

#if defined(CONFIG_PLATFORM_AVN) && !defined(CONFIG_ANDROID)
free_palette:
#endif
        devm_kfree(&pdev->dev, fbinfo->pseudo_palette);
free_framebuffer:
        framebuffer_release(fbinfo);

free_video_memory:
	tccfb_unmap_video_memory(info);
	pr_err("[ERR][FB] init failed.\n");
	return ret;
}

/*
 *  Cleanup
 */
static int tccfb_remove(struct platform_device *pdev)
{
	struct tccfb_info	 *info = platform_get_drvdata(pdev);
	struct fb_info	   *fbinfo = info->fb;

	pr_info("[INF][FB] %s  \n", __func__);

	tca_main_interrupt_reg(false, info);

	tccfb_unmap_video_memory(info);

#ifdef CONFIG_PM
	pm_runtime_disable(info->dev);
#endif

	clk_put(info->pdata.Mdp_data.vioc_clock);
	clk_put(info->pdata.Mdp_data.ddc_clock);

	unregister_framebuffer(fbinfo);

	return 0;
}

int tccfb_register_panel(struct lcd_panel *panel)
{
	dprintk(" %s  name:%s\n", __func__, panel->name);

	lcd_panel = panel;
	return 1;
}
EXPORT_SYMBOL(tccfb_register_panel);

struct lcd_panel *tccfb_get_panel(void)
{
	return lcd_panel;
}
EXPORT_SYMBOL(tccfb_get_panel);


int tccfb_register_ext_panel(struct lcd_panel *panel)
{
        pr_info("[INF][FB] %s\n", __func__);
        display_ext_panel = panel;
        return 1;
}
EXPORT_SYMBOL(tccfb_register_ext_panel);


struct lcd_panel *tccfb_get_hdmi_ext_panel(void)
{
        return display_ext_panel;
}
EXPORT_SYMBOL(tccfb_get_hdmi_ext_panel);

#ifdef CONFIG_PM
int tcc_fb_runtime_suspend(struct device *dev)
{
	pr_info("[INF][FB] %s\n", __func__);

	tca_fb_suspend(dev, lcd_panel, display_ext_panel);

	return 0;
}

int tcc_fb_runtime_resume(struct device *dev)
{
	pr_info("[INF][FB] %s\n",__func__);

	tca_fb_resume(dev, lcd_panel, display_ext_panel);

	return 0;
}


/* suspend and resume support for the lcd controller */
static int tccfb_suspend(struct device *dev)
{
	pr_info("[INF][FB] %s\n", __func__);
#ifndef CONFIG_PM
	tca_fb_suspend(dev, lcd_panel, display_ext_panel);
#endif//
	return 0;
}

static int tccfb_resume(struct device *dev)
{
#ifndef CONFIG_PM
	tca_fb_resume(dev, lcd_panel, display_ext_panel);
#endif//
	return 0;
}
static int tccfb_thaw(struct device *dev)
{
	//After you create a QuickBoot Image, before restarting
	#if defined(__CONFIG_HIBERNATION)
	struct platform_device *fb_device = container_of(dev, struct platform_device, dev);
	struct tccfb_info	   *fbi = platform_get_drvdata(fb_device);
	pr_info("[INF][FB] %s\n", __func__);
	if(do_hibernation)
	{
		fb_quickboot_resume(fbi);
	}
	#endif//
	return 0;
}

static int tccfb_freeze(struct device *dev)
{
	//It used to suspend when creating Quickboot Image
	//tca_fb_suspend(dev, lcd_panel);
	//pr_info("[INF][FB] %s\n", __func__);

	return 0;
}

static int tccfb_restore(struct device *dev)
{
	// Used for Loading after resume to Quickboot Image
	#if defined(__CONFIG_HIBERNATION)
	struct platform_device *fb_device = container_of(dev, struct platform_device, dev);
	struct tccfb_info	   *fbi = platform_get_drvdata(fb_device);
	pr_info("[INF][FB] %s\n", __func__);
	if(do_hibernation)
	{
		fb_quickboot_resume(fbi);
	}
	#endif//

	return 0;
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops tccfb_pm_ops = {
	.runtime_suspend      = tcc_fb_runtime_suspend,
	.runtime_resume       = tcc_fb_runtime_resume,
	.suspend	= tccfb_suspend,
	.resume = tccfb_resume,
	.freeze = tccfb_freeze,
	.thaw = tccfb_thaw,
	.restore = tccfb_restore,
};
#endif


static struct platform_driver tccfb_driver = {
	.probe		= tccfb_probe,
	.remove		= tccfb_remove,
	.driver		= {
		.name	= "tccfb",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm		= &tccfb_pm_ops,
#endif
		.of_match_table = of_match_ptr(tccfb_of_match),
	},
};

//int tccfb_init(void)
static int __init tccfb_init(void)
{
	pr_info("[INF][FB] %s\n", __func__);
	return platform_driver_register(&tccfb_driver);
}

static void __exit tccfb_exit(void)
{
	pr_info("[INF][FB] %s\n", __func__);
	tca_fb_exit();

	platform_driver_unregister(&tccfb_driver);
}


module_init(tccfb_init);
module_exit(tccfb_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC Framebuffer driver");
MODULE_LICENSE("GPL");
