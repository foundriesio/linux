/****************************************************************************
 *	 TCC Version 0.6
 *	 Copyright (c) telechips, Inc.
 *	 ALL RIGHTS RESERVED
 *
****************************************************************************/

#ifndef _OVERLAY_H
#define _OVERLAY_H

#ifndef ADDRESS_ALIGNED
#define ADDRESS_ALIGNED
#define ALIGN_BIT (0x8-1)
#define BIT_0 3
#define GET_ADDR_YUV42X_spY(Base_addr) 		(((((unsigned int)Base_addr) + ALIGN_BIT)>> BIT_0)<<BIT_0)
#define GET_ADDR_YUV42X_spU(Yaddr, x, y) 		(((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT)>> BIT_0)<<BIT_0)
#define GET_ADDR_YUV422_spV(Uaddr, x, y) 		(((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#define GET_ADDR_YUV420_spV(Uaddr, x, y) 		(((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#endif



#define OVERLAY_PUSH_VIDEO_BUFFER	90
#define OVERLAY_SET_CONFIGURE		50
#define OVERLAY_SET_LAYER		51
#define OVERLAY_GET_LAYER		52
#define OVERLAY_DISALBE_LAYER	53
#define OVERLAY_SET_OVP			54
#define OVERLAY_GET_OVP			55

typedef struct
{
	unsigned int sx;
	unsigned int sy;
	unsigned int width;
	unsigned int height;
	unsigned int format;
	unsigned int transform;
} overlay_config_t;

typedef struct
{
	overlay_config_t cfg;
	unsigned int addr;
	unsigned int addr1;
	unsigned int addr2;
	unsigned int afbc_dec_num;
	unsigned int afbc_dec_need;
} overlay_video_buffer_t;

#endif
