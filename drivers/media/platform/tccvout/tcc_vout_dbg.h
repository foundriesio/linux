/*
 * tcc_vout_dbg.h
 *
 * Copyright (C) 2013 Telechips, Inc. 
 *
 * Video-for-Linux (Version 2) video output driver for Telechips SoC.
 * 
 * This package is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. 
 * 
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED 
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 *   
 */
#ifndef __TCC_VOUT_DBG_H__
#define __TCC_VOUT_DBG_H__

#include <linux/mm.h>
#include <linux/slab.h>

#ifdef CONFIG_TCC_VOUT_DEBUG
#define dprintk(fmt, args...) printk("\e[33m[DBG][VOUT]%s(%d) \e[0m" fmt, __func__, __LINE__, ## args);
#else
#define dprintk(fmt, args...)
#endif
#ifdef CONFIG_TCC_VOUT_DBG_BUF
#define dbprintk(fmt, args...) printk("\e[36m[DBG][VOUT]%s(%d) \e[0m" fmt, __func__, __LINE__, ## args);
#else
#define dbprintk(fmt, args...)
#endif
#ifdef CONFIG_TCC_VOUT_DBG_INT
#define dtprintk(fmt, args...) printk("\e[35m[DBG][VOUT]%s(%d) \e[0m" fmt, __func__, __LINE__, ## args);
#else
#define dtprintk(fmt, args...)
#endif
#ifdef CONFIG_TCC_VOUT_DBG_INFO
#define diprintk(fmt, args...) printk("\e[33m[DBG][VOUT]%s(%d) \e[0m" fmt, __func__, __LINE__, ## args);
#else
#define diprintk(fmt, args...)
#endif

#define fourcc2char(fourcc) \
        ((char) ((fourcc)     &0xff)), \
        ((char) (((fourcc)>>8 )&0xff)), \
        ((char) (((fourcc)>>16)&0xff)), \
        ((char) (((fourcc)>>24)&0xff))

#ifdef CONFIG_TCC_VOUT_DEBUG
extern char *fourcc2str(unsigned int fourcc, char buf[4]);
extern void print_v4l2_capability(struct v4l2_capability *cap, const char *str);
extern void print_v4l2_buf_type(enum v4l2_buf_type type, const char *str);
extern void print_v4l2_memory(enum v4l2_memory mem, const char *str);
extern void print_v4l2_pix_format(struct v4l2_pix_format *pix, const char *str);
extern void print_v4l2_pix_format_mplane(struct v4l2_pix_format_mplane *pix_mp, const char *str);
extern void print_v4l2_rect(const struct v4l2_rect *rect, const char *str);
extern void print_v4l2_fmtdesc(struct v4l2_fmtdesc *fmt, const char *str);
extern void print_v4l2_buffer(struct v4l2_buffer *buf, const char *str);
extern void print_v4l2_reqbufs_format(enum tcc_pix_fmt pfmt, unsigned int pixelformat, const char *str);
extern void print_vioc_vout_path(struct tcc_vout_device *vout, const char *str);
extern void print_vioc_deintl_path(struct tcc_vout_device *vout, const char *str);
extern void print_vioc_subplane_info(struct tcc_vout_device *vout, const char *str);
#else
#define fourcc2str(fmt, args...)
#define print_v4l2_capability(fmt, args...)
#define print_v4l2_buf_type(fmt, args...)
#define print_v4l2_memory(fmt, args...)
#define print_v4l2_pix_format(fmt, args...)
#define print_v4l2_pix_format_mplane(fmt, args...)
#define print_v4l2_rect(fmt, args...)
#define print_v4l2_fmtdesc(fmt, args...)
#define print_v4l2_buffer(fmt, args...)
#define print_v4l2_reqbufs_format(fmt, args...)
#define print_vioc_vout_path(fmt, args...)
#define print_vioc_deintl_path(fmt, args...)
#define print_vioc_subplane_info(fmt, args...)
#endif

#endif //__TCC_VOUT_DBG_H__
