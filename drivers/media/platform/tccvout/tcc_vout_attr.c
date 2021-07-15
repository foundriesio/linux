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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "tcc_vout.h"
#include "tcc_vout_core.h"
#include "tcc_vout_dbg.h"


/**
 * Show the vioc path of the v4l2 video output.
 */
static ssize_t vioc_path_show(struct device *dev,
	struct device_attribute *attr, char *buf)
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
DEVICE_ATTR(vioc_path, 0444, vioc_path_show, NULL);

/**
 * Show & Set the rdma of vioc path.
 */
static ssize_t vioc_rdma_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;

	dprintk("rdma%d\n", vioc->rdma.id);
	return sprintf(buf, "%d\n", vioc->rdma.id);
}

static ssize_t vioc_rdma_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int val, ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	if (val < 0 || val > 7) {
		pr_err("[ERR][VOUT] invalid rdma\n");
		return count;
	}
	dprintk("rdma%d -> rdma%d\n", vioc->rdma.id, val);

	vioc->rdma.id = val;
	vout_set_vout_path(vout);

	return count;
}
DEVICE_ATTR(vioc_rdma, 0644, vioc_rdma_show, vioc_rdma_store);

/**
 * Show & Set the scaler of vioc path.
 */
static ssize_t vioc_sc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;

	dprintk("sc%d\n", vioc->sc.id);
	return sprintf(buf, "%d\n", vioc->sc.id);
}

static ssize_t vioc_sc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int val, ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	if (val < 0 || val > 3) {
		pr_err("[ERR][VOUT] invalid sc\n");
		return count;
	}
	dprintk("sc%d -> sc%d\n", vioc->sc.id, val);

	vioc->sc.id = val;

	return count;
}
DEVICE_ATTR(vioc_sc, 0644, vioc_sc_show, vioc_sc_store);

/**
 * Show & Set the ovp (overlay priority) of wmix.
 */
static ssize_t vioc_wmix_ovp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;

	dprintk("wmix%d ovp(%d)\n", vioc->wmix.id, vioc->wmix.ovp);
	return sprintf(buf, "%d\n", vioc->wmix.ovp);
}

static ssize_t vioc_wmix_ovp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int val;
	int ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	dprintk("wmix%d ovp(%d) -> ovp(%d)\n",
		vioc->wmix.id, vioc->wmix.ovp, val);

	vioc->wmix.ovp = val;

	return count;
}
DEVICE_ATTR(vioc_wmix_ovp, 0644,
	vioc_wmix_ovp_show, vioc_wmix_ovp_store);

/**
 * Show & Set the v4l2_memory.
 */
static ssize_t force_v4l2_memory_userptr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	dprintk("force(%d) v4l2_memory(%d)\n",
		vout->force_userptr, vout->memory);
	return sprintf(buf, "%d\n", vout->force_userptr);
}

static ssize_t force_v4l2_memory_userptr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	unsigned int val;
	int ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	vout->force_userptr = !!val;
	dprintk("set(%d) -> force(%d)\n", val, vout->force_userptr);
	return count;
}
DEVICE_ATTR(force_v4l2_memory_userptr, 0644,
	force_v4l2_memory_userptr_show, force_v4l2_memory_userptr_store);

/**
 * Show & Set the pmap name.
 */
static ssize_t vout_pmap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	dprintk("vout_pmap(%s)\n", vout->pmap.name);
	return sprintf(buf, "%s\n", vout->pmap.name);
}

static ssize_t vout_pmap_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
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
DEVICE_ATTR(vout_pmap, 0644, vout_pmap_show, vout_pmap_store);


/*==============================================================================
 * DE-INTERLACE
 * ------------
 * deinterlace_path_show
 * deinterlace_type_show/store
 * deinterlace_rdma_show/store
 * deinterlace_pmap_show/store
 * deinterlace_bufs_show/store
 *==============================================================================
 */
/**
 * Show interlace information.
 */
static ssize_t deinterlace_path_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret;

	switch (vout->deinterlace) {
	case VOUT_DEINTL_S:
		ret = sprintf(buf,
			"RDMA%d - DEINTL_S - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id,
			vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_VIQE_2D:
		ret = sprintf(buf,
			"RDMA%d - VIQE_2D - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id,
			vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_VIQE_3D:
		ret = sprintf(buf,
			"RDMA%d - VIQE_3D - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id,
			vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_VIQE_BYPASS:
		ret = sprintf(buf,
			"RDMA%d - VIQE_BYPASS - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id,  vioc->m2m_wmix.id,
			vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_NONE:
	default:
		ret = sprintf(buf, "RDMA%d - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id,
			vioc->sc.id, vioc->m2m_wdma.id);
		break;
	}

	return ret;
}
DEVICE_ATTR(deinterlace_path, 0444, deinterlace_path_show, NULL);

/**
 * Select De-interlacer.
 */
static ssize_t deinterlace_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	return sprintf(buf, "%d\n", vout->deinterlace);
}
static ssize_t deinterlace_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	unsigned int val;
	int ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	vout->deinterlace = val;
	dprintk("set(%d) -> deintl(%d)\n", val, vout->deinterlace);

	if (vout->deinterlace)
		vout_set_m2m_path(0, vout);

	return count;
}
DEVICE_ATTR(deinterlace, 0644,
	deinterlace_show, deinterlace_store);

/**
 * Select deinterlace's rdma.
 */
static ssize_t deinterlace_rdma_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;

	return sprintf(buf, "%d\n", vioc->m2m_rdma.id);
}
static ssize_t deinterlace_rdma_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int val, ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	if (!(val == 16 || val == 17)) {
		pr_err("[ERR][VOUT] invalid rdma (use 16 or 17)\n");
		return count;
	}
	dprintk("set(%d) -> m2m_rdma(%d)\n", vioc->m2m_rdma.id, val);

	vioc->m2m_rdma.id = val;
	vout_set_m2m_path(0, vout);

	return count;
}
DEVICE_ATTR(deinterlace_rdma, 0644,
	deinterlace_rdma_show, deinterlace_rdma_store);

/**
 * Show & Set the scaler of deintl_path.
 */
static ssize_t deinterlace_sc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;

	dprintk("sc%d\n", vioc->sc.id);
	return sprintf(buf, "%d\n", vioc->sc.id);
}

static ssize_t deinterlace_sc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int val, ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	if (val < 0 || val > 3) {
		pr_err("[ERR][VOUT] invalid sc\n");
		return count;
	}
	dprintk("sc%d -> sc%d\n", vioc->sc.id, val);

	vioc->sc.id = val;

	return count;
}
DEVICE_ATTR(deinterlace_sc, 0644,
	deinterlace_sc_show, deinterlace_sc_store);

/**
 * Show & Set the deintl_pmap name.
 */
static ssize_t deinterlace_pmap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	dprintk("deintl_pmap(%s)\n", vout->deintl_pmap.name);
	return sprintf(buf, "%s\n", vout->deintl_pmap.name);
}

static ssize_t deintlerlace_pmap_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
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
DEVICE_ATTR(deinterlace_pmap, 0644,
	deinterlace_pmap_show, deintlerlace_pmap_store);

/**
 * Select De-interlacer buffers.
 */
static ssize_t deinterlace_bufs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	return sprintf(buf, "%d\n", vout->deintl_nr_bufs);
}
static ssize_t deinterlace_bufs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	int val, ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	vout->deintl_nr_bufs = val;
	dprintk("set(%d) -> deintl_nr_bufs(%d)\n", val, vout->deintl_nr_bufs);
	return count;
}
DEVICE_ATTR(deinterlace_bufs, 0644,
	deinterlace_bufs_show, deinterlace_bufs_store);

/**
 * Select rdma first field.
 */
static ssize_t deinterlace_bfield_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;

	return sprintf(buf, "%d\n", vioc->m2m_rdma.bf);
}
static ssize_t deinterlace_bfield_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int val;
	int ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	if (val != 0 || val != 1) {
		pr_err("[ERR][VOUT] invalid rdma's bfield. select 0(top-field first) or 1(bottom-field first)\n");
		return count;
	}
	dprintk("set(%d) -> m2m_rdma.bf(%d)\n", vioc->m2m_rdma.bf, val);
	vioc->m2m_rdma.bf = val;
	return count;
}
DEVICE_ATTR(deinterlace_bfield, 0644,
	deinterlace_bfield_show, deinterlace_bfield_store);

/**
 * Select De-interlacer.
 */
static ssize_t deinterlace_force_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	return sprintf(buf, "%d\n", vout->deintl_force);
}
static ssize_t deinterlace_force_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	int val, ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	vout->deintl_force = val;
	dprintk("set(%d) -> deintl_force(%d)\n", val, vout->deintl_force);
	return count;
}
DEVICE_ATTR(deinterlace_force, 0644,
	deinterlace_force_show, deinterlace_force_store);

/**
 * Select type of vout path.
 * 0 = m2m path
 * 1 = on-the-fly path
 */
static ssize_t otf_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	return sprintf(buf, "%d\n", vout->onthefly_mode);
}
static ssize_t otf_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	struct tcc_vout_device *vout = dev->platform_data;
	int val, ret;

	if (vout->status != TCC_VOUT_IDLE) {
		pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	vout->onthefly_mode = !!val;
	dprintk("set(%d) -> onthefly_mode(%d)\n", val, vout->onthefly_mode);
	#endif
	return count;
}
DEVICE_ATTR(otf_mode, 0644, otf_mode_show, otf_mode_store);

static ssize_t vout_mute_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcc_vout_device *vout = dev->platform_data;

	return sprintf(buf, "%d\n", vout->vout_mute);
}

static ssize_t vout_mute_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_vout_device *vout = dev->platform_data;
	int val, ret;

	ret = kstrtoul(buf, 10, (unsigned long *)&val);
	void __iomem *rdma_addr = vout->vioc->rdma.addr;

#if defined(CONFIG_TCC_DUAL_DISPLAY)
	unsigned int m2m_dual_wdma_base0 =
		vout->vioc->m2m_dual_wdma[M2M_DUAL_0].img.base0;
	unsigned int m2m_dual_wdma_base1 =
		vout->vioc->m2m_dual_wdma[M2M_DUAL_0].img.base1;
	unsigned int m2m_dual_wdma_base2 =
		vout->vioc->m2m_dual_wdma[M2M_DUAL_0].img.base2;
	unsigned int m2m_dual_width =
		vout->vioc->m2m_dual_wmix[M2M_DUAL_0].width;
	unsigned int m2m_dual_height =
		vout->vioc->m2m_dual_wmix[M2M_DUAL_0].height;
#endif
	dprintk("set vout_mute (%d) -> (%d)\n", vout->vout_mute, val);

	#if defined(CONFIG_TCC_DUAL_DISPLAY)
	if ((vout->id == VOUT_MAIN) &&
		(vout->disp_mode == 2 || vout->disp_mode == 3))
		return count;
	#endif

	mutex_lock(&vout->lock);
	vout->vout_mute = val;

	if ((vout->vioc != NULL) &&
		(vout->status == TCC_VOUT_RUNNING) &&
		(VIOC_DISP_Get_TurnOnOff(vout->vioc->disp.addr))) {
		if (vout->vout_mute == VOUT_MUTE_ON) {
			VIOC_RDMA_SetImageDisable(rdma_addr);
		} else if ((vout->vout_mute == VOUT_MUTE_OFF)
			&& (vout->is_streaming == 1)) {
			VIOC_WMIX_SetPosition(vout->vioc->wmix.addr,
				vout->vioc->wmix.pos,
				vout->vioc->wmix.left, vout->vioc->wmix.top);
			VIOC_WMIX_SetUpdate(vout->vioc->wmix.addr);
			if (vout->id == VOUT_MAIN) {
				#if defined(CONFIG_TCC_DUAL_DISPLAY)
				VIOC_RDMA_SetImageBase(rdma_addr,
					m2m_dual_wdma_base0,
					m2m_dual_wdma_base1,
					m2m_dual_wdma_base2);
				VIOC_RDMA_SetImageSize(rdma_addr,
					m2m_dual_width,
					m2m_dual_height);
				VIOC_RDMA_SetImageOffset(rdma_addr,
					vout->vioc->m2m_wdma.fmt,
					m2m_dual_width);
				#else
				VIOC_RDMA_SetImageBase(rdma_addr,
					vout->vioc->m2m_wdma.img.base0,
					vout->vioc->m2m_wdma.img.base1,
					vout->vioc->m2m_wdma.img.base2);
				VIOC_RDMA_SetImageSize(rdma_addr,
					vout->vioc->rdma.width,
					vout->vioc->rdma.height);
				VIOC_RDMA_SetImageOffset(rdma_addr,
					vout->vioc->rdma.fmt,
					vout->vioc->rdma.width);
				#endif
			} else {
				VIOC_RDMA_SetImageBase(rdma_addr,
					vout->vioc->m2m_wdma.img.base0,
					vout->vioc->m2m_wdma.img.base1,
					vout->vioc->m2m_wdma.img.base2);
				VIOC_RDMA_SetImageSize(rdma_addr,
					vout->vioc->rdma.width,
					vout->vioc->rdma.height);
				VIOC_RDMA_SetImageOffset(rdma_addr,
					vout->vioc->rdma.fmt,
					vout->vioc->rdma.width);
			}
			VIOC_RDMA_SetImageEnable(rdma_addr);
		}
	}

	mutex_unlock(&vout->lock);

	return count;
}
DEVICE_ATTR(vout_mute, 0644, vout_mute_show, vout_mute_store);

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
	device_create_file(&pdev->dev, &dev_attr_vout_mute);
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
	device_remove_file(&pdev->dev, &dev_attr_vout_mute);
}
