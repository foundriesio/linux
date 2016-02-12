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

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_flip_work.h>
#include <drm/drm_fb_cma_helper.h>

#include "fsl_dcu_drm_crtc.h"
#include "fsl_dcu_drm_drv.h"

void fsl_dcu_cleanup_atomic_state(struct drm_device *dev,
				  struct drm_atomic_state *state)
{
	drm_atomic_helper_cleanup_planes(dev, state);
	drm_atomic_state_free(state);
}

static int fsl_dcu_drm_atomic_commit(struct drm_device *dev,
				     struct drm_atomic_state *state,
				     bool async)
{
	struct fsl_dcu_drm_device *fsl_dev = dev->dev_private;
	int ret;

	ret = drm_atomic_helper_prepare_planes(dev, state);
	if (ret < 0)
		return ret;

	/*
	 * This is the point of no return - everything below never fails except
	 * when the hw goes bonghits. Which means we can commit the new state on
	 * the software side now.
	 */
	drm_atomic_helper_swap_state(dev, state);

	drm_atomic_helper_commit_modeset_disables(dev, state);
	drm_atomic_helper_commit_planes(dev, state, false);
	drm_atomic_helper_commit_modeset_enables(dev, state);

	if (async) {
		fsl_dev->cleanup_state = state;
	} else {
		drm_atomic_helper_wait_for_vblanks(dev, state);
		fsl_dcu_cleanup_atomic_state(dev, state);
	}

	return 0;
}

static const struct drm_mode_config_funcs fsl_dcu_drm_mode_config_funcs = {
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = fsl_dcu_drm_atomic_commit,
	.fb_create = drm_fb_cma_create,
};

int fsl_dcu_drm_modeset_init(struct fsl_dcu_drm_device *fsl_dev)
{
	int ret;

	drm_mode_config_init(fsl_dev->drm);

	fsl_dev->drm->mode_config.min_width = 0;
	fsl_dev->drm->mode_config.min_height = 0;
	fsl_dev->drm->mode_config.max_width = 2031;
	fsl_dev->drm->mode_config.max_height = 2047;
	fsl_dev->drm->mode_config.funcs = &fsl_dcu_drm_mode_config_funcs;

	ret = fsl_dcu_drm_crtc_create(fsl_dev);
	if (ret)
		return ret;

	ret = fsl_dcu_drm_encoder_create(fsl_dev, &fsl_dev->crtc.base);
	if (ret)
		goto fail_encoder;

	ret = fsl_dcu_drm_connector_create(fsl_dev, &fsl_dev->encoder);
	if (ret)
		goto fail_connector;

	drm_mode_config_reset(fsl_dev->drm);
	drm_kms_helper_poll_init(fsl_dev->drm);

	return 0;
fail_encoder:
	fsl_dev->crtc.base.funcs->destroy(&fsl_dev->crtc.base);
fail_connector:
	fsl_dev->encoder.funcs->destroy(&fsl_dev->encoder);
	return ret;
}
