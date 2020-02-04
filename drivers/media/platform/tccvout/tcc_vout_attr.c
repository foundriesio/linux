/*
 * tcc_vout_attr.c
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
 * ChangeLog:
 *   
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <asm/io.h>

#include "tcc_vout.h"
#include "tcc_vout_core.h"
#include "tcc_vout_dbg.h"


/**
 * Show the vioc path of the v4l2 video output.
 */
static ssize_t vioc_path_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	char buf_sc[32];
	int ret;

	ret = sprintf(buf_sc, " - SC%d", vioc->sc.id);
	if (ret < 0)
		return ret;

	return sprintf(buf, "RDMA%d%s - WMIX%d - DISP%d\n",
					vioc->rdma.id,
					vioc->sc.id < 0 ? "" : buf_sc,
					vioc->wmix.id, vioc->disp.id);
}
DEVICE_ATTR(vioc_path, S_IRUGO, vioc_path_show, NULL);

/**
 * Show & Set the rdma of vioc path.
 */
static ssize_t vioc_rdma_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;

	dprintk("rdma%d\n", vioc->rdma.id);
	return sprintf(buf, "%d\n", vioc->rdma.id);
}

static ssize_t vioc_rdma_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);
	if ((int)val < 0 || val > 7) {
		pr_err("[ERR][VOUT] invalid rdma\n");
		return count;
	}
	dprintk("rdma%d -> rdma%d\n", vioc->rdma.id, val);

	vioc->rdma.id = val;
	vout_set_vout_path(vout);

	return count;
}
DEVICE_ATTR(vioc_rdma, S_IRUGO | S_IWUSR, vioc_rdma_show, vioc_rdma_store);

/**
 * Show & Set the scaler of vioc path.
 */
static ssize_t vioc_sc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;

	dprintk("sc%d\n", vioc->sc.id);
	return sprintf(buf, "%d\n", vioc->sc.id);
}

static ssize_t vioc_sc_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);
	if ((int)val < 0 || val > 3) {
		pr_err("[ERR][VOUT] invalid sc\n");
		return count;
	}
	dprintk("sc%d -> sc%d\n", vioc->sc.id, val);

	vioc->sc.id = val;

	return count;
}
DEVICE_ATTR(vioc_sc, S_IRUGO | S_IWUSR, vioc_sc_show, vioc_sc_store);

/**
 * Show & Set the ovp (overlay priority) of wmix.
 */
static ssize_t vioc_wmix_ovp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;

	dprintk("wmix%d ovp(%d)\n", vioc->wmix.id, vioc->wmix.ovp);
	return sprintf(buf, "%d\n", vioc->wmix.ovp);
}

static ssize_t vioc_wmix_ovp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);
	dprintk("wmix%d ovp(%d) -> ovp(%d)\n", vioc->wmix.id, vioc->wmix.ovp, val);

	vioc->wmix.ovp = val;

	return count;
}
DEVICE_ATTR(vioc_wmix_ovp, S_IRUGO | S_IWUSR, vioc_wmix_ovp_show, vioc_wmix_ovp_store);

/**
 * Show & Set the v4l2_memory.
 */
static ssize_t force_v4l2_memory_userptr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	dprintk("force(%d) v4l2_memory(%d)\n", vout->force_userptr, vout->memory);
	return sprintf(buf, "%d\n", vout->force_userptr);
}

static ssize_t force_v4l2_memory_userptr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);
	vout->force_userptr = !!val;
	dprintk("set(%d) -> force(%d)\n", val, vout->force_userptr);
	return count;
}
DEVICE_ATTR(force_v4l2_memory_userptr, S_IRUGO | S_IWUSR, force_v4l2_memory_userptr_show, force_v4l2_memory_userptr_store);

/**
 * Show & Set the pmap name.
 */
static ssize_t vout_pmap_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	dprintk("vout_pmap(%s)\n", vout->pmap.name);
	return sprintf(buf, "%s\n", vout->pmap.name);
}

static ssize_t vout_pmap_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	memset(vout->pmap.name, 0, sizeof(vout->pmap.name));
	memcpy(vout->pmap.name, buf, count - 1);
	dprintk("vout_pmap(%s)\n", vout->pmap.name);
	return count;
}
DEVICE_ATTR(vout_pmap, S_IRUGO | S_IWUSR, vout_pmap_show, vout_pmap_store);


/*==========================================================================================
 * DE-INTERLACE
 * ------------
 * deinterlace_path_show
 * deinterlace_type_show/store
 * deinterlace_rdma_show/store
 * deinterlace_pmap_show/store
 * deinterlace_bufs_show/store
 *==========================================================================================
 */
/**
 * Show interlace information.
 */
static ssize_t deinterlace_path_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret;

	switch (vout->deinterlace) {
	case VOUT_DEINTL_S:
		ret = sprintf(buf, "RDMA%d - DEINTL_S - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id, vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_VIQE_2D:
		ret = sprintf(buf, "RDMA%d - VIQE_2D - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id, vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_VIQE_3D:
		ret = sprintf(buf, "RDMA%d - VIQE_3D - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id, vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_VIQE_BYPASS:
		ret = sprintf(buf, "RDMA%d - VIQE_BYPASS - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id,  vioc->m2m_wmix.id, vioc->sc.id,vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_NONE:
	default:
		ret = sprintf(buf, "RDMA%d - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id, vioc->sc.id, vioc->m2m_wdma.id);
		break;
	}

	return ret;
}
DEVICE_ATTR(deinterlace_path, S_IRUGO, deinterlace_path_show, NULL);

/**
 * Select De-interlacer.
 */
static ssize_t deinterlace_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	return sprintf(buf, "%d\n", vout->deinterlace);
}
static ssize_t deinterlace_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);
	vout->deinterlace = val;
	dprintk("set(%d) -> deintl(%d)\n", val, vout->deinterlace);

	if (vout->deinterlace)
		vout_set_m2m_path(0, vout);

	return count;
}
DEVICE_ATTR(deinterlace, S_IRUGO | S_IWUSR, deinterlace_show, deinterlace_store);

/**
 * Select deinterlace's rdma.
 */
static ssize_t deinterlace_rdma_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	return sprintf(buf, "%d\n", vioc->m2m_rdma.id);
}
static ssize_t deinterlace_rdma_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);

	if (!(val == 16 || val == 17)) {
		pr_err("[ERR][VOUT] invalid rdma (use 16 or 17)\n");
		return count;
	}
	dprintk("set(%d) -> m2m_rdma(%d)\n", vioc->m2m_rdma.id, val);

	vioc->m2m_rdma.id = val;
	vout_set_m2m_path(0, vout);

	return count;
}
DEVICE_ATTR(deinterlace_rdma, S_IRUGO | S_IWUSR, deinterlace_rdma_show, deinterlace_rdma_store);

/**
 * Show & Set the scaler of deintl_path.
 */
static ssize_t deinterlace_sc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;

	dprintk("sc%d\n", vioc->sc.id);
	return sprintf(buf, "%d\n", vioc->sc.id);
}

static ssize_t deinterlace_sc_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);
	if ((int)val < 0 || val > 3) {
		pr_err("[ERR][VOUT] invalid sc\n");
		return count;
	}
	dprintk("sc%d -> sc%d\n", vioc->sc.id, val);

	vioc->sc.id = val;

	return count;
}
DEVICE_ATTR(deinterlace_sc, S_IRUGO | S_IWUSR, deinterlace_sc_show, deinterlace_sc_store);

/**
 * Show & Set the deintl_pmap name.
 */
static ssize_t deinterlace_pmap_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	dprintk("deintl_pmap(%s)\n", vout->deintl_pmap.name);
	return sprintf(buf, "%s\n", vout->deintl_pmap.name);
}

static ssize_t deintlerlace_pmap_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	memset(vout->deintl_pmap.name, 0, sizeof(vout->deintl_pmap.name));
	memcpy(vout->deintl_pmap.name, buf, count - 1);
	dprintk("deintl_pmap(%s)\n", vout->deintl_pmap.name);
	return count;
}
DEVICE_ATTR(deinterlace_pmap, S_IRUGO | S_IWUSR, deinterlace_pmap_show, deintlerlace_pmap_store);

/**
 * Select De-interlacer buffers.
 */
static ssize_t deinterlace_bufs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	return sprintf(buf, "%d\n", vout->deintl_nr_bufs);
}
static ssize_t deinterlace_bufs_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);
	vout->deintl_nr_bufs = val;
	dprintk("set(%d) -> deintl_nr_bufs(%d)\n", val, vout->deintl_nr_bufs);
	return count;
}
DEVICE_ATTR(deinterlace_bufs, S_IRUGO | S_IWUSR, deinterlace_bufs_show, deinterlace_bufs_store);

/**
 * Select rdma first field.
 */
static ssize_t deinterlace_bfield_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	return sprintf(buf, "%d\n", vioc->m2m_rdma.bf);
}
static ssize_t deinterlace_bfield_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);
	if (val != 0 || val != 1) {
		pr_err("[ERR][VOUT] invalid rdma's bfield. select 0(top-field first) or 1(bottom-field first)\n");
		return count;
	}
	dprintk("set(%d) -> m2m_rdma.bf(%d)\n", vioc->m2m_rdma.bf, val);
	vioc->m2m_rdma.bf = val;
	return count;
}
DEVICE_ATTR(deinterlace_bfield, S_IRUGO | S_IWUSR, deinterlace_bfield_show, deinterlace_bfield_store);

/**
 * Select De-interlacer.
 */
static ssize_t deinterlace_force_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	return sprintf(buf, "%d\n", vout->deintl_force);
}
static ssize_t deinterlace_force_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);
	vout->deintl_force = val;
	dprintk("set(%d) -> deintl_force(%d)\n", val, vout->deintl_force);
	return count;
}
DEVICE_ATTR(deinterlace_force, S_IRUGO | S_IWUSR, deinterlace_force_show, deinterlace_force_store);

/**
 * Select type of vout path.
 * 0 = m2m path
 * 1 = on-the-fly path
 */
static ssize_t otf_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	return sprintf(buf, "%d\n", vout->onthefly_mode);
}
static ssize_t otf_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	struct tcc_vout_device *vout = dev->platform_data;
	unsigned int val;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	val = simple_strtoul(buf, NULL, 10);
	vout->onthefly_mode = !!val;
	dprintk("set(%d) -> onthefly_mode(%d)\n", val, vout->onthefly_mode);
	#endif
	return count;
}
DEVICE_ATTR(otf_mode, S_IRUGO | S_IWUSR, otf_mode_show, otf_mode_store);
/**
 * Create the device files
 */
void tcc_vout_attr_create(struct platform_device *pdev)
{
	device_create_file(&pdev->dev, &dev_attr_vioc_path);
	device_create_file(&pdev->dev, &dev_attr_vioc_rdma);
	device_create_file(&pdev->dev, &dev_attr_vioc_wmix_ovp);
	device_create_file(&pdev->dev, &dev_attr_force_v4l2_memory_userptr);
	device_create_file(&pdev->dev, &dev_attr_vout_pmap);
	/* deinterlace */
	device_create_file(&pdev->dev, &dev_attr_deinterlace);
	device_create_file(&pdev->dev, &dev_attr_deinterlace_path);
	device_create_file(&pdev->dev, &dev_attr_deinterlace_rdma);
	device_create_file(&pdev->dev, &dev_attr_deinterlace_pmap);
	device_create_file(&pdev->dev, &dev_attr_deinterlace_bufs);
	device_create_file(&pdev->dev, &dev_attr_deinterlace_bfield);
	device_create_file(&pdev->dev, &dev_attr_deinterlace_force);
	device_create_file(&pdev->dev, &dev_attr_deinterlace_sc);
	/* on-the-fly path */
	device_create_file(&pdev->dev, &dev_attr_otf_mode);
}

/**
 * Remove the device files
 */
void tcc_vout_attr_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_vioc_path);
	device_remove_file(&pdev->dev, &dev_attr_vioc_rdma);
	device_remove_file(&pdev->dev, &dev_attr_vioc_wmix_ovp);
	device_remove_file(&pdev->dev, &dev_attr_force_v4l2_memory_userptr);
	device_remove_file(&pdev->dev, &dev_attr_vout_pmap);
	/* deinterlace */
	device_remove_file(&pdev->dev, &dev_attr_deinterlace);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_path);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_rdma);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_pmap);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_bufs);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_bfield);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_force);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_sc);
	/* on-the-fly path */
	device_remove_file(&pdev->dev, &dev_attr_otf_mode);
}

