/*
 * linux/drivers/media/platform/tcc-hwdemux/tcc_hwdemux.h
 *
 * Copyright (C) 2010 Telechips, Inc.
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

#ifndef INCLUDED_TCA_HWDMX_PARAM
#define INCLUDED_TCA_HWDMX_PARAM

#define FILTER_TYPE_SECTION 0
#define FILTER_TYPE_TS 1
#define FILTER_TYPE_PES 2
#define FILTER_TYPE_PCR 3

struct tcc_tsif_filter {
	unsigned int f_id;
	unsigned int f_type;
	unsigned int f_pid;
	unsigned int f_size;
	unsigned char *f_comp;
	unsigned char *f_mask;
	unsigned char *f_mode;
};

#endif
