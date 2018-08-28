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
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <drm/drmP.h>

#include <drm/tcc_drm.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_atomic_helper.h>
#include "tcc_drm_drv.h"
#include "tcc_drm_crtc.h"
#include "tcc_drm_fb.h"
#include "tcc_drm_gem.h"
#include "tcc_drm_plane.h"

/*
 * This function is to get X or Y size shown via screen. This needs length and
 * start position of CRTC.
 *
 *      <--- length --->
 * CRTC ----------------
 *      ^ start        ^ end
 *
 * There are six cases from a to f.
 *
 *             <----- SCREEN ----->
 *             0                 last
 *   ----------|------------------|----------
 * CRTCs
 * a -------
 *        b -------
 *        c --------------------------
 *                 d --------
 *                           e -------
 *                                  f -------
 */
static int tcc_plane_get_size(int start, unsigned length, unsigned last)
{
	int end = start + length;
	int size = 0;

	if (start <= 0) {
		if (end > 0)
			size = min_t(unsigned, end, last);
	} else if (start <= last) {
		size = min_t(unsigned, last - start, length);
	}

	return size;
}

static void tcc_plane_mode_set(struct drm_plane *plane,
				  struct drm_crtc *crtc,
				  struct drm_framebuffer *fb,
				  int crtc_x, int crtc_y,
				  unsigned int crtc_w, unsigned int crtc_h,
				  uint32_t src_x, uint32_t src_y,
				  uint32_t src_w, uint32_t src_h)
{
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	unsigned int actual_w;
	unsigned int actual_h;

	actual_w = tcc_plane_get_size(crtc_x, crtc_w, mode->hdisplay);
	actual_h = tcc_plane_get_size(crtc_y, crtc_h, mode->vdisplay);

	if (crtc_x < 0) {
		if (actual_w)
			src_x -= crtc_x;
		crtc_x = 0;
	}

	if (crtc_y < 0) {
		if (actual_h)
			src_y -= crtc_y;
		crtc_y = 0;
	}

	/* set ratio */
	tcc_plane->h_ratio = (src_w << 16) / crtc_w;
	tcc_plane->v_ratio = (src_h << 16) / crtc_h;

	/* set drm framebuffer data. */
	tcc_plane->src_x = src_x;
	tcc_plane->src_y = src_y;
	tcc_plane->src_w = (actual_w * tcc_plane->h_ratio) >> 16;
	tcc_plane->src_h = (actual_h * tcc_plane->v_ratio) >> 16;

	/* set plane range to be displayed. */
	tcc_plane->crtc_x = crtc_x;
	tcc_plane->crtc_y = crtc_y;
	tcc_plane->crtc_w = actual_w;
	tcc_plane->crtc_h = actual_h;

	DRM_DEBUG_KMS("plane : offset_x/y(%d,%d), width/height(%d,%d)",
			tcc_plane->crtc_x, tcc_plane->crtc_y,
			tcc_plane->crtc_w, tcc_plane->crtc_h);

	plane->crtc = crtc;
}

static struct drm_plane_funcs tcc_plane_funcs = {
	.update_plane	= drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy	= drm_plane_cleanup,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static int tcc_plane_atomic_check(struct drm_plane *plane,
				     struct drm_plane_state *state)
{
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);
	int nr;
	int i;

	if (!state->fb)
		return 0;

	nr = drm_format_num_planes(state->fb->pixel_format);
	for (i = 0; i < nr; i++) {
		struct tcc_drm_gem *tcc_gem =
					tcc_drm_fb_gem(state->fb, i);
		if (!tcc_gem) {
			DRM_DEBUG_KMS("gem object is null\n");
			return -EFAULT;
		}

		tcc_plane->dma_addr[i] = tcc_gem->dma_addr +
					    state->fb->offsets[i];

		DRM_DEBUG_KMS("buffer: %d, dma_addr = 0x%lx\n",
				i, (unsigned long)tcc_plane->dma_addr[i]);
	}

	return 0;
}

static void tcc_plane_atomic_update(struct drm_plane *plane,
				       struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(state->crtc);
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);

	if (!state->crtc)
		return;

	tcc_plane_mode_set(plane, state->crtc, state->fb,
			      state->crtc_x, state->crtc_y,
			      state->crtc_w, state->crtc_h,
			      state->src_x >> 16, state->src_y >> 16,
			      state->src_w >> 16, state->src_h >> 16);

	tcc_plane->pending_fb = state->fb;

	if (tcc_crtc->ops->update_plane)
		tcc_crtc->ops->update_plane(tcc_crtc, tcc_plane);
}

static void tcc_plane_atomic_disable(struct drm_plane *plane,
					struct drm_plane_state *old_state)
{
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(old_state->crtc);

	if (!old_state->crtc)
		return;

	if (tcc_crtc->ops->disable_plane)
		tcc_crtc->ops->disable_plane(tcc_crtc,
						tcc_plane);
}

static const struct drm_plane_helper_funcs plane_helper_funcs = {
	.atomic_check = tcc_plane_atomic_check,
	.atomic_update = tcc_plane_atomic_update,
	.atomic_disable = tcc_plane_atomic_disable,
};

static void tcc_plane_attach_zpos_property(struct drm_plane *plane,
					      unsigned int zpos)
{
	struct drm_device *dev = plane->dev;
	struct tcc_drm_private *dev_priv = dev->dev_private;
	struct drm_property *prop;

	prop = dev_priv->plane_zpos_property;
	if (!prop) {
		prop = drm_property_create_range(dev, DRM_MODE_PROP_IMMUTABLE,
						 "zpos", 0, MAX_PLANE - 1);
		if (!prop)
			return;

		dev_priv->plane_zpos_property = prop;
	}

	drm_object_attach_property(&plane->base, prop, zpos);
}

enum drm_plane_type tcc_plane_get_type(unsigned int zpos,
					  unsigned int cursor_win)
{
		if (zpos == DEFAULT_WIN)
			return DRM_PLANE_TYPE_PRIMARY;
		else if (zpos == cursor_win)
			return DRM_PLANE_TYPE_CURSOR;
		else
			return DRM_PLANE_TYPE_OVERLAY;
}

int tcc_plane_init(struct drm_device *dev,
		      struct tcc_drm_plane *tcc_plane,
		      unsigned long possible_crtcs, enum drm_plane_type type,
		      const uint32_t *formats, unsigned int fcount,
		      unsigned int zpos)
{
	int err;

	err = drm_universal_plane_init(dev, &tcc_plane->base, possible_crtcs,
				       &tcc_plane_funcs, formats, fcount,
				       type);
	if (err) {
		DRM_ERROR("failed to initialize plane\n");
		return err;
	}

	drm_plane_helper_add(&tcc_plane->base, &plane_helper_funcs);

	tcc_plane->zpos = zpos;

	if (type == DRM_PLANE_TYPE_OVERLAY)
		tcc_plane_attach_zpos_property(&tcc_plane->base, zpos);

	return 0;
}
