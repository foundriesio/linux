/*
 *  tcc_dmx.h
 *
 *  Written by C2-G1-3T
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.=
 */

#ifndef _TCC_DMX_H_
#define _TCC_DMX_H_

#include <dvb_demux.h>
#include <dmxdev.h>

typedef struct tcc_dmx_priv_t
{
	unsigned int connected_id; // connected hwdemux id
	struct dmx_frontend fe_hw[4];
	struct dmx_frontend fe_mem;
	struct dvb_demux demux;
	struct dmxdev dmxdev;
} tcc_dmx_priv_t;

typedef struct tcc_dmx_inst_t
{
	struct dvb_adapter *adapter;

	int dev_num;
	tcc_dmx_priv_t *dmx;
} tcc_dmx_inst_t;

typedef void (*tcc_dmx_smpcb)(int, uintptr_t, int, uintptr_t, int);

int tcc_dmx_init(tcc_dmx_inst_t *dxb_demux);
int tcc_dmx_deinit(tcc_dmx_inst_t *dxb_demux);

int tcc_dmx_can_write(int devid);
void tcc_dmx_set_smpcb(int devid, tcc_dmx_smpcb cb);
void tcc_dmx_unset_smpcb(int devid);
int tcc_dmx_ts_callback(char *p1, int p1_size, char *p2, int p2_size, int devid);
int tcc_dmx_sec_callback(unsigned int fid, int crc_err, char *p, int size, int devid);

#endif //_TCC_DMX_H_
