/**************************************************************************
 * Copyright (C) 2020 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/
#ifndef __TCC_DRM_EDID_H__
#define __TCC_DRM_EDID_H__

int tcc_make_edid_from_display_mode(
	struct edid *edid, struct drm_display_mode *mode);
#endif
