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
#include <linux/err.h>
#include <linux/module.h>


#ifdef CONFIG_PM
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <video/display_timing.h>
#include <video/videomode.h>
#include <panel_helper.h>

static DEFINE_MUTEX(panel_lock);
static LIST_HEAD(panel_list);

void fb_panel_init(struct fb_panel *panel)
{
	INIT_LIST_HEAD(&panel->list);
}
EXPORT_SYMBOL(fb_panel_init);

int fb_panel_add(struct fb_panel *panel)
{
	mutex_lock(&panel_lock);
	list_add_tail(&panel->list, &panel_list);
	mutex_unlock(&panel_lock);

	return 0;
}
EXPORT_SYMBOL(fb_panel_add);

void fb_panel_remove(struct fb_panel *panel)
{
	mutex_lock(&panel_lock);
	list_del_init(&panel->list);
	mutex_unlock(&panel_lock);
}
EXPORT_SYMBOL(fb_panel_remove);

struct fb_panel *of_fb_find_panel(const struct device_node *np)
{
	struct fb_panel *panel;

	mutex_lock(&panel_lock);

	list_for_each_entry(panel, &panel_list, list) {
		if (panel->dev->of_node == np) {
			mutex_unlock(&panel_lock);
			return panel;
		}
	}

	mutex_unlock(&panel_lock);
	return NULL;
}
EXPORT_SYMBOL(of_fb_find_panel);

int fb_panel_prepare(struct fb_panel *panel)
{
	int ret = -ENODEV;
	if(panel && panel->funcs && panel->funcs->prepare)
		ret = panel->funcs->prepare(panel);
	return ret;
}
EXPORT_SYMBOL(fb_panel_prepare);

int fb_panel_enable(struct fb_panel *panel)
{
	int ret = -ENODEV;
	if(panel && panel->funcs && panel->funcs->enable)
		ret = panel->funcs->enable(panel);
	return ret;
}
EXPORT_SYMBOL(fb_panel_enable);

int fb_panel_disable(struct fb_panel *panel)
{
	int ret = -ENODEV;
	if(panel && panel->funcs && panel->funcs->disable)
		ret = panel->funcs->disable(panel);
	return ret;
}
EXPORT_SYMBOL(fb_panel_disable);

int fb_panel_unprepare(struct fb_panel *panel)
{
	int ret = -ENODEV;
	if(panel && panel->funcs && panel->funcs->unprepare)
		ret = panel->funcs->unprepare(panel);
	return ret;
}
EXPORT_SYMBOL(fb_panel_unprepare);

int fb_panel_get_mode(struct fb_panel *panel, struct videomode *vm)
{
	int ret = -ENODEV;
	if(panel && panel->funcs && panel->funcs->get_videomode)
		ret = panel->funcs->get_videomode(panel, vm);
	return ret;
}
EXPORT_SYMBOL(fb_panel_get_mode);

MODULE_AUTHOR("Jayden Kim <kimdy@telechips.com>");
MODULE_DESCRIPTION("FB panel infrastructure");
MODULE_LICENSE("GPL and additional rights");