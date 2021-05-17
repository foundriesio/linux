/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (c) 2016 Telechips Inc.
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

int tcc_plane_init(struct drm_device *dev,
		struct drm_plane *drm_plane,
		enum drm_plane_type plane_type,
		const uint32_t *pixel_formats,
		unsigned int num_pixel_formats);

