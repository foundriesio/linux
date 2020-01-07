/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
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

#ifndef VIDEOSOURCE_COMMON_H
#define VIDEOSOURCE_COMMON_H

#include "videosource_types.h"

extern int					videosource_loglevel;

#define LOGLEVEL			LOGLEVEL_DEBUG
#define log(fmt, ...)		printk(KERN_INFO "%s - " pr_fmt(fmt), __FUNCTION__, ##__VA_ARGS__)
#define dlog(fmt, ...)		do { if(videosource_loglevel) { printk(KERN_INFO "%s - " pr_fmt(fmt), __FUNCTION__, ##__VA_ARGS__); } } while(0)
#define FUNCTION_IN			dlog("IN\n");
#define FUNCTION_OUT		dlog("OUT\n");

extern void	sensor_port_enable(int port);
extern void	sensor_port_disable(int port);

extern int videosource_create_attr_loglevel(struct device * dev);

#endif//VIDEOSOURCE_COMMON_H

