/*
 * Copyright (c) 2017 Telechips Inc.
 *
 * Author:  Telechips Inc.
 * Created: Sep 05, 2017
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *---------------------------------------------------------------------------
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	Jinyoung Jeon <jy0.jeon@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _TCC_DRM_IPP_H_
#define _TCC_DRM_IPP_H_

#define for_each_ipp_ops(pos)	\
	for (pos = 0; pos < TCC_DRM_OPS_MAX; pos++)
#define for_each_ipp_planar(pos)	\
	for (pos = 0; pos < TCC_DRM_PLANAR_MAX; pos++)

#define IPP_GET_LCD_WIDTH	_IOR('F', 302, int)
#define IPP_GET_LCD_HEIGHT	_IOR('F', 303, int)
#define IPP_SET_WRITEBACK	_IOW('F', 304, u32)

/* definition of state */
enum drm_tcc_ipp_state {
	IPP_STATE_IDLE,
	IPP_STATE_START,
	IPP_STATE_STOP,
};

/*
 * A structure of command work information.
 * @work: work structure.
 * @ippdrv: current work ippdrv.
 * @c_node: command node information.
 * @ctrl: command control.
 */
struct drm_tcc_ipp_cmd_work {
	struct work_struct	work;
	struct tcc_drm_ippdrv	*ippdrv;
	struct drm_tcc_ipp_cmd_node *c_node;
	enum drm_tcc_ipp_ctrl	ctrl;
};

/*
 * A structure of command node.
 *
 * @list: list head to command queue information.
 * @event_list: list head of event.
 * @mem_list: list head to source,destination memory queue information.
 * @lock: lock for synchronization of access to ioctl.
 * @mem_lock: lock for synchronization of access to memory nodes.
 * @event_lock: lock for synchronization of access to scheduled event.
 * @start_complete: completion of start of command.
 * @stop_complete: completion of stop of command.
 * @property: property information.
 * @start_work: start command work structure.
 * @stop_work: stop command work structure.
 * @event_work: event work structure.
 * @state: state of command node.
 * @filp: associated file pointer.
 */
struct drm_tcc_ipp_cmd_node {
	struct list_head	list;
	struct list_head	event_list;
	struct list_head	mem_list[TCC_DRM_OPS_MAX];
	struct mutex	lock;
	struct mutex	mem_lock;
	struct mutex	event_lock;
	struct completion	start_complete;
	struct completion	stop_complete;
	struct drm_tcc_ipp_property	property;
	struct drm_tcc_ipp_cmd_work *start_work;
	struct drm_tcc_ipp_cmd_work *stop_work;
	struct drm_tcc_ipp_event_work *event_work;
	enum drm_tcc_ipp_state	state;
	struct drm_file	*filp;
};

/*
 * A structure of buffer information.
 *
 * @handles: Y, Cb, Cr each gem object handle.
 * @base: Y, Cb, Cr each planar address.
 */
struct drm_tcc_ipp_buf_info {
	unsigned long	handles[TCC_DRM_PLANAR_MAX];
	dma_addr_t	base[TCC_DRM_PLANAR_MAX];
};

/*
 * A structure of wb setting information.
 *
 * @enable: enable flag for wb.
 * @refresh: HZ of the refresh rate.
 */
struct drm_tcc_ipp_set_wb {
	__u32	enable;
	__u32	refresh;
};

/*
 * A structure of event work information.
 *
 * @work: work structure.
 * @ippdrv: current work ippdrv.
 * @buf_id: id of src, dst buffer.
 */
struct drm_tcc_ipp_event_work {
	struct work_struct	work;
	struct tcc_drm_ippdrv *ippdrv;
	u32	buf_id[TCC_DRM_OPS_MAX];
};

/*
 * A structure of source,destination operations.
 *
 * @set_fmt: set format of image.
 * @set_transf: set transform(rotations, flip).
 * @set_size: set size of region.
 * @set_addr: set address for dma.
 */
struct tcc_drm_ipp_ops {
	int (*set_fmt)(struct device *dev, u32 fmt);
	int (*set_transf)(struct device *dev,
		enum drm_tcc_degree degree,
		enum drm_tcc_flip flip, bool *swap);
	int (*set_size)(struct device *dev, int swap,
		struct drm_tcc_pos *pos, struct drm_tcc_sz *sz);
	int (*set_addr)(struct device *dev,
		 struct drm_tcc_ipp_buf_info *buf_info, u32 buf_id,
		enum drm_tcc_ipp_buf_type buf_type);
};

/*
 * A structure of ipp driver.
 *
 * @drv_list: list head for registed sub driver information.
 * @parent_dev: parent device information.
 * @dev: platform device.
 * @drm_dev: drm device.
 * @dedicated: dedicated ipp device.
 * @ops: source, destination operations.
 * @event_workq: event work queue.
 * @c_node: current command information.
 * @cmd_list: list head for command information.
 * @cmd_lock: lock for synchronization of access to cmd_list.
 * @prop_list: property informations of current ipp driver.
 * @check_property: check property about format, size, buffer.
 * @reset: reset ipp block.
 * @start: ipp each device start.
 * @stop: ipp each device stop.
 * @sched_event: work schedule handler.
 */
struct tcc_drm_ippdrv {
	struct list_head	drv_list;
	struct device	*parent_dev;
	struct device	*dev;
	struct drm_device	*drm_dev;
	bool	dedicated;
	struct tcc_drm_ipp_ops	*ops[TCC_DRM_OPS_MAX];
	struct workqueue_struct	*event_workq;
	struct drm_tcc_ipp_cmd_node *c_node;
	struct list_head	cmd_list;
	struct mutex	cmd_lock;
	struct drm_tcc_ipp_prop_list prop_list;

	int (*check_property)(struct device *dev,
		struct drm_tcc_ipp_property *property);
	int (*reset)(struct device *dev);
	int (*start)(struct device *dev, enum drm_tcc_ipp_cmd cmd);
	void (*stop)(struct device *dev, enum drm_tcc_ipp_cmd cmd);
	void (*sched_event)(struct work_struct *work);
};

#ifdef CONFIG_DRM_TCC_IPP
extern int tcc_drm_ippdrv_register(struct tcc_drm_ippdrv *ippdrv);
extern int tcc_drm_ippdrv_unregister(struct tcc_drm_ippdrv *ippdrv);
extern int tcc_drm_ipp_get_property(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int tcc_drm_ipp_set_property(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int tcc_drm_ipp_queue_buf(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int tcc_drm_ipp_cmd_ctrl(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int tcc_drm_ippnb_register(struct notifier_block *nb);
extern int tcc_drm_ippnb_unregister(struct notifier_block *nb);
extern int tcc_drm_ippnb_send_event(unsigned long val, void *v);
extern void ipp_sched_cmd(struct work_struct *work);
extern void ipp_sched_event(struct work_struct *work);

#else
static inline int tcc_drm_ippdrv_register(struct tcc_drm_ippdrv *ippdrv)
{
	return -ENODEV;
}

static inline int tcc_drm_ippdrv_unregister(struct tcc_drm_ippdrv *ippdrv)
{
	return -ENODEV;
}

static inline int tcc_drm_ipp_get_property(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file_priv)
{
	return -ENOTTY;
}

static inline int tcc_drm_ipp_set_property(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file_priv)
{
	return -ENOTTY;
}

static inline int tcc_drm_ipp_queue_buf(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file)
{
	return -ENOTTY;
}

static inline int tcc_drm_ipp_cmd_ctrl(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file)
{
	return -ENOTTY;
}

static inline int tcc_drm_ippnb_register(struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int tcc_drm_ippnb_unregister(struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int tcc_drm_ippnb_send_event(unsigned long val, void *v)
{
	return -ENOTTY;
}
#endif

#endif /* _TCC_DRM_IPP_H_ */

