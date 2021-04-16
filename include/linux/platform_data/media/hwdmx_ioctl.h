/*
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2008
 * Description: Telechips Linux H/W Demux Driver
 *
 * Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef INCLUDED_HWDMX_IOCTL
#define INCLUDED_HWDMX_IOCTL

struct tsmp_info {
	int dmxch;
	int enabled;
};

#define HWDMX_IOCTL_MAGIC 'D'
#define IOCTL_HWDMX_SET_INTERFACE 0
#define IOCTL_HWDMX_SET_DEBUG_MODE 1
#define IOCTL_HWDMX_SET_SMP _IOW(HWDMX_IOCTL_MAGIC, 2, struct tsmp_info)

#endif /* INCLUDED_HWDMX_IOCTL */
