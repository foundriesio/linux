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
#ifndef __HDMI_API_H__
#define __HDMI_API_H__

void hdmi_api_AvMute(
	int enable);

int hdmi_api_update_colorimetry(
	colorimetry_t colorimetry,
	ext_colorimetry_t ext_colorimetry);

int hdmi_api_update_quantization(
	int quantization_range);

void hdmi_set_drm(
	DRM_Packet_t *drmparm);

#endif /*__HDMI_API_H__*/

