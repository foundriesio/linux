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

#include <drm/drmP.h>
#include <drm/drm_edid.h>

#include <video/of_videomode.h>
#include <video/videomode.h>

#define LOG_TAG "DRMEDID"

static const u8 base_edid[EDID_LENGTH] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
	0x50, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x1E, 0x01, 0x03, 0x80, 0x50, 0x2D, 0x78,
	0x1A, 0x0D, 0xC9, 0xA0, 0x57, 0x47, 0x98, 0x27,
	0x12, 0x48, 0x4C, 0x20, 0x00, 0x00, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x70, 0x17,
	0x80, 0x00, 0x70, 0xD0, 0x00, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
	0x00, 0x00, 0x00, 0xFC, 0x00, 0x42, 0x4F, 0x45,
	0x20, 0x57, 0x4C, 0x43, 0x44, 0x20, 0x31, 0x32,
	0x2E, 0x33, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

int tcc_make_edid_from_display_mode(
	struct edid *edid, struct drm_display_mode *mode)
{
	struct detailed_pixel_timing *pixel_data;
	u8 csum, *raw_edid;
	int i, ret = -1;

	if(!edid)
		goto out;

	raw_edid = (u8 *)edid;
	pixel_data = &edid->detailed_timings[0].data.pixel_data;

	/* Duplicate edid from base_edid */
	memcpy(edid, base_edid, EDID_LENGTH);

	/* Set detailed timing information from display mode */
	edid->detailed_timings[0].pixel_clock =
		cpu_to_le16(DIV_ROUND_UP(mode->clock, 10));
	pixel_data->hactive_lo = mode->hdisplay & 0xff;
	pixel_data->hblank_lo = (mode->htotal - mode->hdisplay) & 0xff;
	pixel_data->hactive_hblank_hi = (mode->hdisplay >> 4) & 0xf0;
	pixel_data->vactive_lo = mode->vdisplay & 0xff;
	pixel_data->vblank_lo = (mode->vtotal - mode->vdisplay) & 0xff;
	pixel_data->vactive_vblank_hi = (mode->vdisplay >> 4) & 0xf0;
	pixel_data->hsync_offset_lo =
		(mode->hsync_start - mode->hdisplay) & 0xff;
	pixel_data->hsync_pulse_width_lo =
		(mode->hsync_end - mode->hsync_start) & 0xff;

	pixel_data->vsync_offset_pulse_width_lo =
		(((mode->vsync_start - mode->vdisplay) & 0xf) << 4) |
		((mode->vsync_end - mode->vsync_start) & 0xf);
	pixel_data->hsync_vsync_offset_pulse_width_hi =
		(((mode->hsync_start - mode->hdisplay) >> 8) << 6) ||
		(((mode->hsync_end - mode->hsync_start) >> 8) << 4) ||
		(((mode->vsync_start - mode->vdisplay) >> 4) << 2) ||
		((mode->vsync_end - mode->vsync_start) >> 4);

	pixel_data->width_mm_lo = mode->width_mm & 0xff;
	pixel_data->height_mm_lo = mode->height_mm & 0xff;
	pixel_data->width_height_mm_hi =
		(mode->width_mm >> 8) << 4 || mode->height_mm >> 8;
	pixel_data->hborder = 0x0;
	pixel_data->vborder = 0x0;
	pixel_data->misc = 0x18;

	/* Checksum */
	for (i = 0, csum = 0; i < EDID_LENGTH; i++)
		csum += raw_edid[i];

	edid->checksum = 0xFF - csum + 1;
	return 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(tcc_make_edid_from_display_mode);
