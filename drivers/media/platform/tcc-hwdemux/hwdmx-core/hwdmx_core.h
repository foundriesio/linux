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

#ifndef INCLUDED_HWDMX_CORE
#define INCLUDED_HWDMX_CORE

#include <linux/types.h>
#include "tca_hwdemux_param.h"

struct tcc_demux_handle
{
	void *handle;
};

int hwdmx_register(int devid, struct device *dev);
int hwdmx_unregister(int devid);
int hwdmx_start(struct tcc_demux_handle *dmx, unsigned int devid);
int hwdmx_stop(struct tcc_demux_handle *dmx, unsigned int devid);
int hwdmx_buffer_flush(struct tcc_demux_handle *dmx, unsigned long addr, int len);
int hwdmx_set_external_tsdemux(
	struct tcc_demux_handle *dmx,
	int (*decoder)(char *p1, int p1_size, char *p2, int p2_size, int devid));
int hwdmx_add_pid(struct tcc_demux_handle *dmx, struct tcc_tsif_filter *param);
int hwdmx_remove_pid(struct tcc_demux_handle *dmx, struct tcc_tsif_filter *param);
int hwdmx_set_pcr_pid(struct tcc_demux_handle *dmx, unsigned int index, unsigned int pcr_pid);
int hwdmx_get_stc(struct tcc_demux_handle *dmx, unsigned int index, u64 *stc);
int hwdmx_set_cipher_dec_pid(struct tcc_demux_handle *dmx,	unsigned int numOfPids, 
	unsigned int delete_option, unsigned short *pids);
int hwdmx_set_cipher_mode(struct tcc_demux_handle *dmx, int algo, int opmode,
	int residual, int smsg, unsigned int numOfPids, unsigned short *pids);
int hwdmx_set_key(struct tcc_demux_handle *dmx, int keytype, int keymode, int size, void *key);

int hwdmx_input_ts(int devid, uintptr_t mmap_buf, int size);

/* deprecated */
int hwdmx_input_stream(uintptr_t mmap_buf, int size);

#endif /* INCLUDED_HWDMX_CORE */
