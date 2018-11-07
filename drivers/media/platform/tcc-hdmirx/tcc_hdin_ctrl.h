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


#include <linux/videodev2.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>

#include "tcc_hdin_video.h"

#ifndef TCC_HDIN_CTRL_H
#define TCC_HDIN_CTRL_H

#if defined(CONFIG_VIDEO_HDMI_RX_MODULE_EP9553T)
#define USING_HW_I2C
#define SENSOR_EP9553T
#include "module/ep9553t.h"
#endif

extern int hdin_ctrl_get_resolution(struct file *);
extern int hdin_ctrl_get_fps(int *nFrameRate);
extern int hdin_ctrl_get_audio_samplerate(struct tcc_hdin_device *vdev);
extern int hdin_ctrl_get_audio_type(struct tcc_hdin_device *vdev);
extern int hdin_ctrl_get_audio_output_signal(struct tcc_hdin_device *vdev);
extern int hdin_ctrl_init(struct file *);
extern void hdin_ctrl_get_gpio(struct tcc_hdin_device *vdev);
extern void hdin_ctrl_port_enable(int port);
extern void hdin_ctrl_port_disable(int port);
extern void hdin_ctrl_cleanup(struct file *);
extern void hdin_ctrl_delay(int ms);

#endif