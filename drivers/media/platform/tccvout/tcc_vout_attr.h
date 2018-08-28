/*
 * tcc_vout.h
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