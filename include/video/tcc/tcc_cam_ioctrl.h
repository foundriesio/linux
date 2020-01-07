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

#ifndef __TCC_CAMERA_IOCTL_H__
#define __TCC_CAMERA_IOCTL_H__

#define V4L2_CID_ISO 							V4L2_CID_PRIVATE_BASE+0
#define V4L2_CID_EFFECT 						V4L2_CID_PRIVATE_BASE+1
#define V4L2_CID_ZOOM 							V4L2_CID_PRIVATE_BASE+2
#define V4L2_CID_FLIP 							V4L2_CID_PRIVATE_BASE+3
#define V4L2_CID_SCENE 							V4L2_CID_PRIVATE_BASE+4
#define V4L2_CID_METERING_EXPOSURE 				V4L2_CID_PRIVATE_BASE+5
#define V4L2_CID_FLASH 							V4L2_CID_PRIVATE_BASE+6
#define V4L2_CID_FOCUS_MODE						V4L2_CID_PRIVATE_BASE+7
#define V4L2_CID_LAST_PRIV 						V4L2_CID_FLASH
#define V4L2_CID_MAX 							V4L2_CID_LAST_PRIV+1

#define VIDIOC_USER_JPEG_CAPTURE 				_IOWR ('V', BASE_VIDIOC_PRIVATE+1, int)
#define VIDIOC_USER_GET_CAPTURE_INFO 			_IOWR ('V', BASE_VIDIOC_PRIVATE+2, TCCXXX_JPEG_ENC_DATA)
#define VIDIOC_USER_PROC_AUTOFOCUS 				_IOWR ('V', BASE_VIDIOC_PRIVATE+3, int)
#define VIDIOC_USER_SET_CAMINFO_TOBEOPEN 		_IOWR ('V', BASE_VIDIOC_PRIVATE+4, int)
#define VIDIOC_USER_GET_MAX_RESOLUTION 			_IOWR ('V', BASE_VIDIOC_PRIVATE+5, int)
#define VIDIOC_USER_GET_SENSOR_FRAMERATE		_IOWR ('V', BASE_VIDIOC_PRIVATE+6, int)
#define VIDIOC_USER_GET_ZOOM_SUPPORT			_IOWR ('V', BASE_VIDIOC_PRIVATE+7, int)
#define VIDIOC_USER_SET_CAMERA_ADDR				_IOWR ('V', BASE_VIDIOC_PRIVATE+8, struct v4l2_requestbuffers)
#define VIDIOC_ASSIGN_ALLOCATED_BUF				_IOWR ('V', BASE_VIDIOC_PRIVATE+8, struct v4l2_buffer)
#define VIDIOC_USER_INT_CHECK					_IOWR ('V', BASE_VIDIOC_PRIVATE+9, int)
#define VIDIOC_CHECK_PATH_STATUS				_IOWR ('V', BASE_VIDIOC_PRIVATE+9, int)
#define VIDIOC_USER_GET_CAM_STATUS				_IOWR ('V', BASE_VIDIOC_PRIVATE+10, int)
#define VIDIOC_SET_WMIXER_OVP					_IOWR ('V', BASE_VIDIOC_PRIVATE+11, int)
#define VIDIOC_SET_ALPHA_BLENDING               _IO ('V', BASE_VIDIOC_PRIVATE+12)

#define VIDIOC_USER_AUDIO_SR_CHECK				_IOWR ('V', BASE_VIDIOC_PRIVATE+13, int) //for hdmi in sound sample rate
#define VIDIOC_USER_AUDIO_TYPE_CHECK			_IOWR ('V', BASE_VIDIOC_PRIVATE+14, int) //for hdmi in audio type
#define VIDIOC_USER_AUDIO_OUTPUT_CHECK			_IOWR ('V', BASE_VIDIOC_PRIVATE+15, int)

#define VIDIOC_SET_VIDEOSOURCE_FORMAT			_IOWR ('V', BASE_VIDIOC_PRIVATE+16, videosource_format_t)

#define DIRECT_DISPLAY_IF_INITIALIZE			_IOWR ('V', BASE_VIDIOC_PRIVATE+50, int)
#define DIRECT_DISPLAY_IF_START					_IOWR ('V', BASE_VIDIOC_PRIVATE+51, DIRECT_DISPLAY_IF_PARAMETERS)
#define DIRECT_DISPLAY_IF_STOP					_IOWR ('V', BASE_VIDIOC_PRIVATE+52, int)
#define DIRECT_DISPLAY_IF_TERMINATE				_IOWR ('V', BASE_VIDIOC_PRIVATE+53, int)

#endif//__TCC_CAMERA_IOCTL_H__

