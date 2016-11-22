/*
 * Copyright 2015 Freescale Semiconductor, Inc.
 *
 * Freescale DCU drm device driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/console.h>

#include <drm/drmP.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_flip_work.h>

#include "fsl_dcu_drm_drv.h"

void fsl_dcu_fbdev_suspend(struct drm_device *dev)
{
	struct fsl_dcu_drm_device *fsl_dev = dev_get_drvdata(dev->dev);

	console_lock();
	drm_fb_helper_set_suspend(drm_fbdev_cma_get_helper(fsl_dev->fbdev), 1);
	console_unlock();
}

void fsl_dcu_fbdev_resume(struct drm_device *dev)
{
	struct fsl_dcu_drm_device *fsl_dev = dev_get_drvdata(dev->dev);

	console_lock();
	drm_fb_helper_set_suspend(drm_fbdev_cma_get_helper(fsl_dev->fbdev), 0);
	console_unlock();
}
