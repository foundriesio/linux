/*
 * linux/drivers/spi/tcc_hwdemux_tsif.c
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

#include "hwdmx_cmd.h"

#define MAX_PCR_CNT 2

struct ts_demux_feed_struct
{
	int is_active; // 0:don't use external demuxer, 1:use external decoder
	int index;
	int call_decoder_index;
	int (*ts_demux_decoder)(char *p1, int p1_size, char *p2, int p2_size, int devid);
};

struct tcc_hwdmx_tsif_rx_handle
{
	struct ts_demux_feed_struct ts_demux_feed_handle;
	struct tcc_tsif_handle tsif_ex_handle;
	int pcr_pid[MAX_PCR_CNT];
	int read_buff_offset;
	int write_buff_offset;
	int end_buff_offset;
	int loop_count;
	int state;
	unsigned char *mem;
	wait_queue_head_t wait_q;
};

struct tcc_hwdmx_tsif_rx_filter_param
{
	unsigned int f_id;
	unsigned int f_type;
	unsigned int f_pid;
	unsigned int f_size;
	unsigned char *f_comp;
	unsigned char *f_mask;
	unsigned char *f_mode;
};

int tcc_hwdmx_tsif_rx_init(struct device *dev);
int tcc_hwdmx_tsif_rx_deinit(struct device *dev);
int tcc_hwdmx_tsif_rx_register(int devid, struct device *dev);
int tcc_hwdmx_tsif_rx_unregister(int devid);
struct tcc_hwdmx_tsif_rx_handle *tcc_hwdmx_tsif_rx_start(unsigned int devid);
int tcc_hwdmx_tsif_rx_stop(struct tcc_hwdmx_tsif_rx_handle *demux, unsigned int devid);
int tcc_hwdmx_tsif_rx_buffer_flush(
	struct tcc_hwdmx_tsif_rx_handle *demux, unsigned long addr, int len);
int tcc_hwdmx_tsif_rx_set_external_tsdemux(
	struct tcc_hwdmx_tsif_rx_handle *demux,
	int (*decoder)(char *p1, int p1_size, char *p2, int p2_size, int devid));
int tcc_hwdmx_tsif_rx_add_pid(
	struct tcc_hwdmx_tsif_rx_handle *demux, struct tcc_hwdmx_tsif_rx_filter_param *param);
int tcc_hwdmx_tsif_rx_remove_pid(
	struct tcc_hwdmx_tsif_rx_handle *demux, struct tcc_hwdmx_tsif_rx_filter_param *param);
int tcc_hwdmx_tsif_rx_set_pcr_pid(
	struct tcc_hwdmx_tsif_rx_handle *demux, unsigned int index, unsigned int pcr_pid);
int tcc_hwdmx_tsif_rx_get_stc(struct tcc_hwdmx_tsif_rx_handle *demux, unsigned int index, u64 *stc);
int tcc_hwdmx_tsif_rx_set_cipher_dec_pid(struct tcc_hwdmx_tsif_rx_handle *demux, unsigned int numOfPids, 
				unsigned int delete_option, unsigned short *pids);
int tcc_hwdmx_tsif_rx_set_mode(struct tcc_hwdmx_tsif_rx_handle *demux, int algo, int opmode,
	int residual, int smsg, unsigned int numOfPids, unsigned short *pids);
int tcc_hwdmx_tsif_rx_set_key(struct tcc_hwdmx_tsif_rx_handle *demux,
				int keytype, int keymode, int size, void *key);

