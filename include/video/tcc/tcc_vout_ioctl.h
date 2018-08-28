/****************************************************************************
linux/include/video/tcc/tcc_vout_ioctl.h
Description: TCC Vsync Driver 

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
#ifndef __VOUT_IOCTL_H__
#define __VOUT_IOCTL_H__

#include <linux/videodev2.h>

#define VIDIOC_USER_CLEAR_FRAME _IOW('V', BASE_VIDIOC_PRIVATE+1, int)
#define VIDIOC_USER_DISPLAY_LASTFRAME _IOW('V', BASE_VIDIOC_PRIVATE+2, int)
#define VIDIOC_USER_CAPTURE_LASTFRAME _IOW('V', BASE_VIDIOC_PRIVATE+3, struct v4l2_buffer)
#define VIDIOC_USER_SET_OUTPUT_MODE _IOW('V', BASE_VIDIOC_PRIVATE+4, int)
#endif
