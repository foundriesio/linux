/*
 * Copyright (C) Telechips, Inc.
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
#ifndef __TCC_VOUT_ATTR_H__
#define __TCC_VOUT_ATTR_H__

extern void tcc_vout_attr_create(struct platform_device *dev);
extern void tcc_vout_attr_remove(struct platform_device *dev);

/**
 * This file contains the interface to the Linux device attributes.
 */
extern struct device_attribute dev_attr_vioc_path;
extern struct device_attribute dev_attr_vioc_rdma;
extern struct device_attribute dev_attr_vioc_sc;
extern struct device_attribute dev_attr_vioc_wmix_ovp;
extern struct device_attribute dev_attr_vioc_wmix_ovp;
extern struct device_attribute dev_attr_force_v4l2_memory_userptr;
extern struct device_attribute dev_attr_vout_pmap;
/* deinterlace */
extern struct device_attribute dev_attr_deinterlace;
extern struct device_attribute dev_attr_deinterlace_path;
extern struct device_attribute dev_attr_deinterlace_rdma;
extern struct device_attribute dev_attr_deinterlace_pmap;
extern struct device_attribute dev_attr_deinterlace_bufs;
extern struct device_attribute dev_attr_deinterlace_bfield;
extern struct device_attribute dev_attr_deinterlace_sc;
extern struct device_attribute dev_attr_deinterlace_force;
/* on-the-fly */
extern struct device_attribute dev_attr_otf_mode;

#endif //__TCC_VOUT_ATTR_H__
