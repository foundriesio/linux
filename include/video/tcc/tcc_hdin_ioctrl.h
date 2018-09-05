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


//#define ATAG_HDIN     0x5441002c

#define VIDIOC_USER_SET_CAMINFO_TOBEOPEN 		_IOWR ('V', BASE_VIDIOC_PRIVATE, int)
#define VIDIOC_USER_GET_MAX_RESOLUTION 			_IOWR ('V', BASE_VIDIOC_PRIVATE+1, int)
#define VIDIOC_USER_GET_SENSOR_FRAMERATE		_IOWR ('V', BASE_VIDIOC_PRIVATE+2, int)
#define VIDIOC_USER_SET_CAMERA_ADDR				_IOWR ('V', BASE_VIDIOC_PRIVATE+3, struct v4l2_requestbuffers)
#define VIDIOC_USER_INT_CHECK						_IOWR ('V', BASE_VIDIOC_PRIVATE+4, int) // 20131018 swhwang, for check video frame interrupt
#define VIDIOC_USER_AUDIO_SR_CHECK					_IOWR ('V', BASE_VIDIOC_PRIVATE+5, int) //for hdmi in sound sample rate
#define VIDIOC_USER_AUDIO_TYPE_CHECK					_IOWR ('V', BASE_VIDIOC_PRIVATE+6, int) //for hdmi in audio type
#define VIDIOC_USER_AUDIO_OUTPUT_CHECK				_IOWR ('V', BASE_VIDIOC_PRIVATE+7, int) 