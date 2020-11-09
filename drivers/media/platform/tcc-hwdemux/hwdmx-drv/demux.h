// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _TCC_DMX_H_
#define _TCC_DMX_H_

#include <dvb_demux.h>
#include <dmxdev.h>

struct tcc_dmx_priv_t {
	unsigned int connected_id; // connected hwdemux id
	struct dmx_frontend fe_hw[4];
	struct dmx_frontend fe_mem;
	struct dvb_demux demux;
	struct dmxdev dmxdev;
};

struct tcc_dmx_inst_t {
	struct dvb_adapter *adapter;

	int dev_num;
	struct tcc_dmx_priv_t *dmx;
};

int tcc_dmx_init(struct tcc_dmx_inst_t *dxb_demux);
int tcc_dmx_deinit(struct tcc_dmx_inst_t *dxb_demux);

int tcc_dmx_can_write(int devid);
int tcc_dmx_ts_callback(char *p1, int p1_size, char *p2, int p2_size,
			int devid);
int tcc_dmx_sec_callback(unsigned int fid, int crc_err, char *p, int size,
			 int devid);

#endif //_TCC_DMX_H_
