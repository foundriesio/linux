/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.

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

#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <video/tcc/tcc_fb.h>
#include <media/v4l2-common.h>

#include "tccvin_switchmanager.h"
#include "../videosource/videosource_if.h"

static int					debug = 0;
#define TAG					"tccvin_switchmanager"
#define log(msg, arg...)	do { printk(KERN_INFO TAG ": %s - " msg, __func__, ## arg); } while(0)
#define dlog(msg, arg...)	do { if(debug) { printk(KERN_INFO TAG ": %s - " msg, __func__, ## arg); } } while(0)
#define FUNCTION_IN			dlog("IN\n");
#define FUNCTION_OUT		dlog("OUT\n");

extern struct lcd_panel *tccfb_get_panel(void);

int tccvin_switchmanager_start_preview(tccvin_dev_t * vdev) {
#ifdef VIDEO_TCCVIN_PRESET_VIDEOSOURCE
	static int	videosource_preset	= 1;
#endif//VIDEO_TCCVIN_PRESET_VIDEOSOURCE
	int			ret					= 0;

//	FUNCTION_IN

	dlog("cam_streaming = %d, preview_method = %d\n", vdev->cam_streaming, vdev->v4l2.preview_method);
	if((vdev->v4l2.preview_method == PREVIEW_DD) && (vdev->cam_streaming == 0)) {
#ifdef VIDEO_TCCVIN_PRESET_VIDEOSOURCE
		if(videosource_preset) {
			videosource_preset = 0;
		} else {
#endif//VIDEO_TCCVIN_PRESET_VIDEOSOURCE
			videosource_open();
			videosource_if_change_mode(0);
#ifdef VIDEO_TCCVIN_PRESET_VIDEOSOURCE
		}
#endif//VIDEO_TCCVIN_PRESET_VIDEOSOURCE

		// start stream
		ret = tccvin_v4l2_streamon(vdev, &vdev->v4l2.preview_method);

		vdev->cam_streaming = 1;
	}
	dlog("cam_streaming = %d, preview_method = %d\n", vdev->cam_streaming, vdev->v4l2.preview_method);

//	FUNCTION_OUT
	return 0;
}

int tccvin_switchmanager_stop_preview(tccvin_dev_t * vdev) {
	int		ret = 0;

//	FUNCTION_IN

	dlog("cam_streaming = %d, preview_method = %d\n", vdev->cam_streaming, vdev->v4l2.preview_method);
	if((vdev->v4l2.preview_method == PREVIEW_DD) && (vdev->cam_streaming != 0)) {
		// stop preview
		ret = tccvin_v4l2_streamoff(vdev, &vdev->v4l2.preview_method);

		// stop video source
		videosource_close();

		vdev->cam_streaming = 0;
	}
	dlog("cam_streaming = %d, preview_method = %d\n", vdev->cam_streaming, vdev->v4l2.preview_method);

//	FUNCTION_OUT
	return ret;
}

extern int switch_reverse_check_state(void);
//extern volatile int camera_core_resume_done;
int tccvin_switchmanager_monitor_thread(void * data) {
	tccvin_dev_t	* vdev = (tccvin_dev_t *)data;
	int				prev_switch_status = 0, curr_switch_status = 0;

	FUNCTION_IN

	// IMPORTANT
	curr_switch_status = 0;

	// switching
	while(1) {
		msleep(100);

		if(kthread_should_stop())
			break;

		mutex_lock(&vdev->tccvin_switchmanager_lock);

		prev_switch_status = curr_switch_status;
		curr_switch_status = switch_reverse_check_state();
		dlog("prev_switch_status: %d, curr_switch_status: %d\n", prev_switch_status, curr_switch_status);
		if(prev_switch_status != curr_switch_status) {
			log("prev_switch_status: %d, curr_switch_status: %d\n", prev_switch_status, curr_switch_status);
			if(curr_switch_status) {
				tccvin_switchmanager_start_preview(vdev);
			} else {
				tccvin_switchmanager_stop_preview(vdev);
			}
		}

		mutex_unlock(&vdev->tccvin_switchmanager_lock);
	}

	FUNCTION_OUT
	return 0;
}

int tcc_cam_swtichmanager_start_monitor(tccvin_dev_t * vdev) {
	FUNCTION_IN

	if(vdev->threadSwitching != NULL) {
		printk(KERN_ERR "%s - FAILED: thread(0x%p) is not null\n", __func__, vdev->threadSwitching);
		return -1;
	} else {
		vdev->threadSwitching = kthread_run(tccvin_switchmanager_monitor_thread, (void *)vdev, "threadSwitching");
		if(IS_ERR_OR_NULL(vdev->threadSwitching)) {
			printk("%s - FAILED: kthread_run\n", __func__);
			vdev->threadSwitching = NULL;
			return -1;
		}
	}

	FUNCTION_OUT
	return 0;
}

int tccvin_switchmanager_stop_monitor(tccvin_dev_t * vdev) {
	FUNCTION_IN

	if(vdev->threadSwitching == NULL) {
		printk(KERN_ERR "%s - FAILED: thread(0x%p) is null\n", __func__, vdev->threadSwitching);
		return -1;
	} else {
		if(kthread_stop(vdev->threadSwitching) != 0) {
			printk("%s - FAILED: kthread_stop\n", __func__);
			return -1;
		}
		vdev->threadSwitching = NULL;
	}

	FUNCTION_OUT
	return 0;
}

int tccvin_switchmanager_handover_handler(tccvin_dev_t * vdev) {
	int reverse_switch_status = 0;

	FUNCTION_IN

	// All data in the context before making snapshot image is not restored yet during snapshot resume.
	// So, it will fail if you access the gpio by calling tccvin_switchmanager_get_gear_status();
	mutex_lock(&vdev->tccvin_switchmanager_lock);

	reverse_switch_status = switch_reverse_check_state();
	if(reverse_switch_status) {
		// handover
		vdev->cif.is_handover_needed = 1;
		log("handover is %s needed\n", (vdev->cif.is_handover_needed) ? "" : "NOT");

		tccvin_switchmanager_start_preview(vdev);
	}

	mutex_unlock(&vdev->tccvin_switchmanager_lock);

	FUNCTION_OUT
	return 0;
}

int tccvin_switchmanager_probe(tccvin_dev_t * vdev) {
	FUNCTION_IN

	mutex_init(&vdev->tccvin_switchmanager_lock);

	// open a videoinput path
	dlog("[%d] is_dev_opened: %d\n", vdev->plt_dev->id, vdev->is_dev_opened);
	if(vdev->is_dev_opened == DISABLE) {
		struct lcd_panel * lcd_panel_info;
		lcd_panel_info = tccfb_get_panel();

		// init the v4l2 data
		tccvin_v4l2_init(vdev);
		log("pannel size is %d x %d \n", lcd_panel_info->xres, lcd_panel_info->yres);
		tccvin_cif_set_resolution(vdev, lcd_panel_info->xres, lcd_panel_info->yres, V4L2_PIX_FMT_RGB32);

		// set preview method as direct display
		vdev->v4l2.preview_method = PREVIEW_DD;

		vdev->is_dev_opened  = ENABLE;
	}
	dlog("[%d] is_dev_opened: %d\n", vdev->plt_dev->id, vdev->is_dev_opened);

	// handover
//	tccvin_switchmanager_handover_handler(vdev);

	// start to monitor
	tcc_cam_swtichmanager_start_monitor(vdev);

	FUNCTION_OUT
	return 0;
}

