/****************************************************************************
 *
 * Copyright (C) 2013 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef TCCVIN_COMMON_H
#define TCCVIN_COMMON_H

extern int				tccvin_loglevel;

#define LOGLEVEL			LOGLEVEL_DEBUG
#define LOG_TAG			"VIN"

#define loge(fmt, ...)			pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logw(fmt, ...)			pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logd(fmt, ...)			pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logi(fmt, ...)			pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define log				logi
#define dlog(fmt, ...)			do { if(tccvin_loglevel) { logd(fmt, ##__VA_ARGS__); } } while(0)

#define FUNCTION_IN				dlog("IN\n");
#define FUNCTION_OUT			dlog("OUT\n");

extern int tccvin_create_attr_loglevel(struct device * dev);

#endif//TCCVIN_COMMON_H

