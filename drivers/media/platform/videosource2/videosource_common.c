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

#include <linux/delay.h>
#include <linux/device.h>

#include "videosource_common.h"

int			videosource_loglevel;
atomic_t	videosource_attr_loglevel;

ssize_t videosource_attr_loglevel_show(struct device * dev, struct device_attribute * attr, char * buf) {
	long val = atomic_read(&videosource_attr_loglevel);

	sprintf(buf, "%ld\n", val);

	return val;
}

ssize_t videosource_attr_loglevel_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count) {
	long val;
	int error = kstrtoul(buf, 10, &val);
	if(error)
		return error;

	atomic_set(&videosource_attr_loglevel, val);
	videosource_loglevel = val;

	return count;
}

static DEVICE_ATTR(videosource_attr_loglevel, S_IRUGO|S_IWUSR|S_IWGRP, videosource_attr_loglevel_show, videosource_attr_loglevel_store);

int videosource_create_attr_loglevel(struct device * dev) {
	int		ret = 0;

	ret = device_create_file(dev, &dev_attr_videosource_attr_loglevel);
	if(ret < 0)
		loge("failed create sysfs: videosource_attr_loglevel\n");

	// set log level as default
	atomic_set(&videosource_attr_loglevel, LOGLEVEL);

	return ret;
}

void sensor_port_enable(int port) {
	if(0 < port) gpio_set_value_cansleep(port, ENABLE);
}

void sensor_port_disable(int port) {
	if(0 < port) gpio_set_value_cansleep(port, DISABLE);
}

