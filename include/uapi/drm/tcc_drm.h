/* tcc_drm.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 * @note Tab size is 8.
 */

#ifndef _UAPI_TCC_DRM_H_
#define _UAPI_TCC_DRM_H_

#include <drm/drm.h>

/**
 * User-desired buffer creation information structure.
 *
 * @size: user-desired memory allocation size.
 *	- this size value would be page-aligned internally.
 * @flags: user request for setting memory type or cache attributes.
 * @handle: returned a handle to created gem object.
 *	- this handle will be set by gem module of kernel side.
 */
struct drm_tcc_gem_create {
	uint64_t size;
	unsigned int flags;
	unsigned int handle;
};

/**
 * A structure to gem information.
 *
 * @handle: a handle to gem object created.
 * @flags: flag value including memory type and cache attribute and
 *	this value would be set by driver.
 * @size: size to memory region allocated by gem and this size would
 *	be set by driver.
 */
struct drm_tcc_gem_info {
	unsigned int handle;
	unsigned int flags;
	uint64_t size;
};

/**
 * A structure for user connection request of virtual display.
 *
 * @connection: indicate whether doing connetion or not by user.
 * @extensions: if this value is 1 then the vidi driver would need additional
 *	128bytes edid data.
 * @edid: the edid data pointer from user side.
 */
struct drm_tcc_vidi_connection {
	unsigned int connection;
	unsigned int extensions;
	uint64_t edid;
};

#define TCC_GEM_CPU_PREP_READ        (1 << 0)
#define TCC_GEM_CPU_PREP_WRITE       (1 << 1)
#define TCC_GEM_CPU_PREP_NOWAIT      (1 << 2)

#define TCC_GEM_CPU_PREP_FLAGS       ( \
					TCC_GEM_CPU_PREP_READ | \
					TCC_GEM_CPU_PREP_WRITE | \
					TCC_GEM_CPU_PREP_NOWAIT)

struct drm_tcc_gem_cpu_prep {
	__u32 handle; /* in */
	__u32 flags; /* in, mask of TCC_GEM_CPU_PREP_x */
};

struct drm_tcc_gem_cpu_fini {
	__u32 handle; /* in */
};

struct drm_tcc_edid {
	__u32 crtc_id; /* in */
	__u8 data[512];
};

/* memory type definitions. */
enum e_drm_tcc_gem_mem_type {
	/* Physically Continuous memory and used as default. */
	TCC_BO_CONTIG = 0 << 0,
	/* Physically Non-Continuous memory. */
	TCC_BO_NONCONTIG = 1 << 0,
	/* non-cachable mapping and used as default. */
	TCC_BO_NONCACHABLE = 0 << 1,
	/* cachable mapping. */
	TCC_BO_CACHABLE = 1 << 1,
	/* write-combine mapping. */
	TCC_BO_WC = 1 << 2,
	TCC_BO_MASK = TCC_BO_NONCONTIG | TCC_BO_CACHABLE | TCC_BO_WC
};

struct drm_tcc_g2d_get_ver {
	__u32	major;
	__u32	minor;
};

struct drm_tcc_g2d_cmd {
	__u32	offset;
	__u32	data;
};

enum drm_tcc_g2d_buf_type {
	G2D_BUF_USERPTR = 1 << 31,
};

enum drm_tcc_g2d_event_type {
	G2D_EVENT_NOT,
	G2D_EVENT_NONSTOP,
	G2D_EVENT_STOP,		/* not yet */
};

struct drm_tcc_g2d_userptr {
	unsigned long userptr;
	unsigned long size;
};

struct drm_tcc_g2d_set_cmdlist {
	__u64 cmd;
	__u64 cmd_buf;
	__u32 cmd_nr;
	__u32 cmd_buf_nr;

	/* for g2d event */
	__u64 event_type;
	__u64 user_data;
};

struct drm_tcc_g2d_exec {
	__u64 async;
};

enum drm_tcc_ops_id {
	TCC_DRM_OPS_SRC,
	TCC_DRM_OPS_DST,
	TCC_DRM_OPS_MAX,
};

struct drm_tcc_sz {
	__u32 hsize;
	__u32 vsize;
};

struct drm_tcc_pos {
	__u32 x;
	__u32 y;
	__u32 w;
	__u32 h;
};

enum drm_tcc_flip {
	TCC_DRM_FLIP_NONE = (0 << 0),
	TCC_DRM_FLIP_VERTICAL = (1 << 0),
	TCC_DRM_FLIP_HORIZONTAL = (1 << 1),
	TCC_DRM_FLIP_BOTH = TCC_DRM_FLIP_VERTICAL | TCC_DRM_FLIP_HORIZONTAL,
};

enum drm_tcc_degree {
	TCC_DRM_DEGREE_0,
	TCC_DRM_DEGREE_90,
	TCC_DRM_DEGREE_180,
	TCC_DRM_DEGREE_270,
};

enum drm_tcc_planer {
	TCC_DRM_PLANAR_Y,
	TCC_DRM_PLANAR_CB,
	TCC_DRM_PLANAR_CR,
	TCC_DRM_PLANAR_MAX,
};

/**
 * A structure for getting a fake-offset that can be used with mmap.
 *
 * @handle: handle of gem object.
 * @reserved: just padding to be 64-bit aligned.
 * @offset: a fake-offset of gem object.
 */
struct drm_tcc_gem_map {
	__u32 handle;
	__u32 reserved;
	__u64 offset;
};

/**
 * A structure for ipp supported property list.
 *
 * @version: version of this structure.
 * @ipp_id: id of ipp driver.
 * @count: count of ipp driver.
 * @writeback: flag of writeback supporting.
 * @flip: flag of flip supporting.
 * @degree: flag of degree information.
 * @csc: flag of csc supporting.
 * @crop: flag of crop supporting.
 * @scale: flag of scale supporting.
 * @refresh_min: min hz of refresh.
 * @refresh_max: max hz of refresh.
 * @crop_min: crop min resolution.
 * @crop_max: crop max resolution.
 * @scale_min: scale min resolution.
 * @scale_max: scale max resolution.
 */
struct drm_tcc_ipp_prop_list {
	__u32	version;
	__u32	ipp_id;
	__u32	count;
	__u32	writeback;
	__u32	flip;
	__u32	degree;
	__u32	csc;
	__u32	crop;
	__u32	scale;
	__u32	refresh_min;
	__u32	refresh_max;
	__u32	reserved;
	struct drm_tcc_sz	crop_min;
	struct drm_tcc_sz	crop_max;
	struct drm_tcc_sz	scale_min;
	struct drm_tcc_sz	scale_max;
};

/**
 * A structure for ipp config.
 *
 * @ops_id: property of operation directions.
 * @flip: property of mirror, flip.
 * @degree: property of rotation degree.
 * @fmt: property of image format.
 * @sz: property of image size.
 * @pos: property of image position(src-cropped,dst-scaler).
 */
struct drm_tcc_ipp_config {
	enum drm_tcc_ops_id ops_id;
	enum drm_tcc_flip	flip;
	enum drm_tcc_degree	degree;
	__u32	fmt;
	struct drm_tcc_sz	sz;
	struct drm_tcc_pos	pos;
};

enum drm_tcc_ipp_cmd {
	IPP_CMD_NONE,
	IPP_CMD_M2M,
	IPP_CMD_WB,
	IPP_CMD_OUTPUT,
	IPP_CMD_MAX,
};

/**
 * A structure for ipp property.
 *
 * @config: source, destination config.
 * @cmd: definition of command.
 * @ipp_id: id of ipp driver.
 * @prop_id: id of property.
 * @refresh_rate: refresh rate.
 */
struct drm_tcc_ipp_property {
	struct drm_tcc_ipp_config config[TCC_DRM_OPS_MAX];
	enum drm_tcc_ipp_cmd	cmd;
	__u32	ipp_id;
	__u32	prop_id;
	__u32	refresh_rate;
};

enum drm_tcc_ipp_buf_type {
	IPP_BUF_ENQUEUE,
	IPP_BUF_DEQUEUE,
};

/**
 * A structure for ipp buffer operations.
 *
 * @ops_id: operation directions.
 * @buf_type: definition of buffer.
 * @prop_id: id of property.
 * @buf_id: id of buffer.
 * @handle: Y, Cb, Cr each planar handle.
 * @user_data: user data.
 */
struct drm_tcc_ipp_queue_buf {
	enum drm_tcc_ops_id	ops_id;
	enum drm_tcc_ipp_buf_type	buf_type;
	__u32	prop_id;
	__u32	buf_id;
	__u32	handle[TCC_DRM_PLANAR_MAX];
	__u32	reserved;
	__u64	user_data;
};

enum drm_tcc_ipp_ctrl {
	IPP_CTRL_PLAY,
	IPP_CTRL_STOP,
	IPP_CTRL_PAUSE,
	IPP_CTRL_RESUME,
	IPP_CTRL_MAX,
};

/**
 * A structure for ipp start/stop operations.
 *
 * @prop_id: id of property.
 * @ctrl: definition of control.
 */
struct drm_tcc_ipp_cmd_ctrl {
	__u32	prop_id;
	enum drm_tcc_ipp_ctrl	ctrl;
};

#define DRM_TCC_GEM_CREATE		0x00
/* Reserved 0x03 ~ 0x05 for tcc specific gem ioctl */
#define DRM_TCC_GEM_MAP			0x01
#define DRM_TCC_GEM_CPU_PREP            0x02
#define DRM_TCC_GEM_CPU_FINI            0x03

#define DRM_TCC_GEM_GET			0x04
#define DRM_TCC_VIDI_CONNECTION		0x07

#define DRM_TCC_GET_EDID		0x10 // Additional crtc ioctl for edid

/* G2D */
#define DRM_TCC_G2D_GET_VER		0x20
#define DRM_TCC_G2D_SET_CMDLIST	0x21
#define DRM_TCC_G2D_EXEC		0x22

/* IPP - Image Post Processing */
#define DRM_TCC_IPP_GET_PROPERTY	0x30
#define DRM_TCC_IPP_SET_PROPERTY	0x31
#define DRM_TCC_IPP_QUEUE_BUF	0x32
#define DRM_TCC_IPP_CMD_CTRL	0x33

#define DRM_IOCTL_TCC_GEM_CREATE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GEM_CREATE, struct drm_tcc_gem_create)

#define DRM_IOCTL_TCC_GEM_MAP		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GEM_MAP, struct drm_tcc_gem_map)

#define DRM_IOCTL_TCC_GEM_GET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GEM_GET,	struct drm_tcc_gem_info)

#define DRM_IOCTL_TCC_VIDI_CONNECTION	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_VIDI_CONNECTION, struct drm_tcc_vidi_connection)

#define DRM_IOCTL_TCC_GEM_CPU_PREP	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GEM_CPU_PREP, struct drm_tcc_gem_cpu_prep)

#define DRM_IOCTL_TCC_GEM_CPU_FINI	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GEM_CPU_FINI, struct drm_tcc_gem_cpu_fini)

#define DRM_IOCTL_TCC_GET_EDID	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GET_EDID, struct drm_tcc_edid)

#define DRM_IOCTL_TCC_IPP_GET_PROPERTY	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_IPP_GET_PROPERTY, struct drm_tcc_ipp_prop_list)
#define DRM_IOCTL_TCC_IPP_SET_PROPERTY	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_IPP_SET_PROPERTY, struct drm_tcc_ipp_property)
#define DRM_IOCTL_TCC_IPP_QUEUE_BUF	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_IPP_QUEUE_BUF, struct drm_tcc_ipp_queue_buf)
#define DRM_IOCTL_TCC_IPP_CMD_CTRL		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_IPP_CMD_CTRL, struct drm_tcc_ipp_cmd_ctrl)

/* TCC specific events */
#define DRM_TCC_IPP_EVENT		0x80000001

struct drm_tcc_ipp_event {
	struct drm_event base;
	__u64 user_data;
	__u32 tv_sec;
	__u32 tv_usec;
	__u32 prop_id;
	__u32 reserved;
	__u32 buf_id[TCC_DRM_OPS_MAX];
};

#endif /* _UAPI_TCC_DRM_H_ */
