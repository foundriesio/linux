/*
 * Copyright (C) 2008-2019 Telechips Inc.
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
#ifndef __FB_HELPER_H__
#define __FB_HELPER_H__

struct fb_panel;

struct fb_panel_funcs {
	int (*disable)(struct fb_panel *panel);
	int (*unprepare)(struct fb_panel *panel);
	int (*prepare)(struct fb_panel *panel);
	int (*enable)(struct fb_panel *panel);
	int (*get_videomode)(struct fb_panel *panel, struct videomode *vm);
};

struct fb_panel {
	struct device *dev;

	const struct fb_panel_funcs *funcs;

	struct list_head list;
};

void fb_panel_init(struct fb_panel *panel);
int fb_panel_add(struct fb_panel *panel);
void fb_panel_remove(struct fb_panel *panel);
struct fb_panel *of_fb_find_panel(const struct device_node *np);

int fb_panel_prepare(struct fb_panel *panel);
int fb_panel_enable(struct fb_panel *panel);
int fb_panel_disable(struct fb_panel *panel);
int fb_panel_unprepare(struct fb_panel *panel);
int fb_panel_get_mode(struct fb_panel *panel, struct videomode *vm);
#endif

