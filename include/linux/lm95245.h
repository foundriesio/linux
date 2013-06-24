/*
 * include/linux/lm95245.h
 *
 * LM95245, temperature monitoring device from National Semiconductors
 *
 * Copyright (c) 2013, Toradex, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _LINUX_LM95245_H
#define _LINUX_LM95245_H

struct lm95245_platform_data {
	bool enable_os_pin;
	void (*probe_callback)(struct device *dev);
};

void lm95245_get_local_temp(struct device *dev, int *temp);
void lm95245_get_remote_temp(struct device *dev, int *temp);
void lm95245_set_remote_os_limit(struct device *dev, int temp);
void lm95245_set_remote_critical_limit(struct device *dev, int temp);
void lm95245_set_local_shared_os__critical_limit(struct device *dev, int val);

#endif /* _LINUX_LM95245_H */
