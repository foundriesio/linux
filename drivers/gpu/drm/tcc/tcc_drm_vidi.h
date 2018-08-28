/* tcc_drm_vidi.h
 *
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
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 * Author: Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _TCC_DRM_VIDI_H_
#define _TCC_DRM_VIDI_H_

#ifdef CONFIG_DRM_TCC_VIDI
int vidi_connection_ioctl(struct drm_device *drm_dev, void *data,
				struct drm_file *file_priv);
#else
#define vidi_connection_ioctl	NULL
#endif

#endif
