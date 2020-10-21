/*
 * TCC DRM Parallel output support.
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * Contacts: Andrzej Hajda <a.hajda@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __TCC_DRM_DPI_H__
#define __TCC_DRM_DPI_H__

#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
#include <include/dptx_drm.h>
struct tcc_drm_dp_callback_funcs {
	int (*attach)(struct drm_encoder *encoder, int dp_id, int flags );
	int (*detach)(struct drm_encoder *encoder, int dp_id, int flags );
	int (*register_helper_funcs)(struct drm_encoder *encoder, struct dptx_drm_helper_funcs *dptx_ops);
};
#endif
#endif