// SPDX-License-Identifier: GPL-2.0-or-later

/* tcc_drm_core.c
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Author:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drm/drmP.h>

#include "tcc_drm_drv.h"
#include "tcc_drm_crtc.h"

static LIST_HEAD(tcc_drm_subdrv_list);

int tcc_drm_subdrv_register(struct tcc_drm_subdrv *subdrv)
{
	if (!subdrv)
		return -EINVAL;

	list_add_tail(&subdrv->list, &tcc_drm_subdrv_list);

	return 0;
}

int tcc_drm_subdrv_unregister(struct tcc_drm_subdrv *subdrv)
{
	if (!subdrv)
		return -EINVAL;

	list_del(&subdrv->list);

	return 0;
}

int tcc_drm_device_subdrv_probe(struct drm_device *dev)
{
	struct tcc_drm_subdrv *subdrv, *n;
	int err;

	if (!dev)
		return -EINVAL;

	list_for_each_entry_safe(subdrv, n, &tcc_drm_subdrv_list, list) {
		if (subdrv->probe) {
			subdrv->drm_dev = dev;

			/*
			 * this probe callback would be called by sub driver
			 * after setting of all resources to this sub driver,
			 * such as clock, irq and register map are done.
			 */
			err = subdrv->probe(dev, subdrv->dev);
			if (err) {
				DRM_DEBUG("tcc drm subdrv probe failed.\n");
				list_del(&subdrv->list);
				continue;
			}
		}
	}

	return 0;
}

int tcc_drm_device_subdrv_remove(struct drm_device *dev)
{
	struct tcc_drm_subdrv *subdrv;

	if (!dev) {
		WARN(1, "Unexpected drm device unregister!\n");
		return -EINVAL;
	}

	list_for_each_entry(subdrv, &tcc_drm_subdrv_list, list) {
		if (subdrv->remove)
			subdrv->remove(dev, subdrv->dev);
	}

	return 0;
}

int tcc_drm_subdrv_open(struct drm_device *dev, struct drm_file *file)
{
	struct tcc_drm_subdrv *subdrv;
	int ret;

	list_for_each_entry(subdrv, &tcc_drm_subdrv_list, list) {
		if (subdrv->open) {
			ret = subdrv->open(dev, subdrv->dev, file);
			if (ret)
				goto err;
		}
	}

	return 0;

err:
	list_for_each_entry_continue_reverse(
		subdrv, &tcc_drm_subdrv_list, list) {
		if (subdrv->close)
			subdrv->close(dev, subdrv->dev, file);
	}
	return ret;
}

void tcc_drm_subdrv_close(struct drm_device *dev, struct drm_file *file)
{
	struct tcc_drm_subdrv *subdrv;

	list_for_each_entry(subdrv, &tcc_drm_subdrv_list, list) {
		if (subdrv->close)
			subdrv->close(dev, subdrv->dev, file);
	}
}
