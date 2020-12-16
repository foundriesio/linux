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
#include "structures_cm4.h"

#define Hw37	(1LL << 37)
#define Hw36	(1LL << 36)
#define Hw35	(1LL << 35)
#define Hw34	(1LL << 34)
#define Hw33	(1LL << 33)
#define Hw32	(1LL << 32)
#define Hw31	0x80000000
#define Hw30	0x40000000
#define Hw29	0x20000000
#define Hw28	0x10000000
#define Hw27	0x08000000
#define Hw26	0x04000000
#define Hw25	0x02000000
#define Hw24	0x01000000
#define Hw23	0x00800000
#define Hw22	0x00400000
#define Hw21	0x00200000
#define Hw20	0x00100000
#define Hw19	0x00080000
#define Hw18	0x00040000
#define Hw17	0x00020000
#define Hw16	0x00010000
#define Hw15	0x00008000
#define Hw14	0x00004000
#define Hw13	0x00002000
#define Hw12	0x00001000
#define Hw11	0x00000800
#define Hw10	0x00000400
#define Hw9		0x00000200
#define Hw8		0x00000100
#define Hw7		0x00000080
#define Hw6		0x00000040
#define Hw5		0x00000020
#define Hw4		0x00000010
#define Hw3		0x00000008
#define Hw2		0x00000004
#define Hw1		0x00000002
#define Hw0		0x00000001
#define HwZERO	0x00000000

#ifndef uint32_t
typedef unsigned int uint32_t;
#endif

#define BITSET(X, M) ((X) |= ((uint32_t)(M)))
#define BITCSETXC(X, C) (((uint32_t)(X)) & ~((uint32_t)(C)))
#define BITCSET(X, C, S) ((X) = (BITCSETXC(X, C) | ((uint32_t)(S))))
#define BITCLR(X, M) ((X) &= ~((uint32_t)(M)))

struct HWDMX_HANDLE {
	void __iomem *mbox0_base;
	void __iomem *mbox1_base;
	void __iomem *code_base;
	void __iomem *data_base;
	void __iomem *cfg_base;
};

struct tcc_demux_handle {
	void *handle;
};

int hwdmx_register(int devid, struct device *dev);
int hwdmx_unregister(int devid);
int hwdmx_start(struct tcc_demux_handle *dmx, unsigned int devid);
int hwdmx_stop(struct tcc_demux_handle *dmx, unsigned int devid);
int hwdmx_buffer_flush(struct tcc_demux_handle *dmx, unsigned long addr,
		       int len);
int hwdmx_set_external_tsdemux(struct tcc_demux_handle *dmx,
			       int (*decoder)(char *p1, int p1_size, char *p2,
					       int p2_size, int devid));
int hwdmx_add_pid(struct tcc_demux_handle *dmx, struct tcc_tsif_filter *param);
int hwdmx_remove_pid(struct tcc_demux_handle *dmx,
		     struct tcc_tsif_filter *param);
int hwdmx_set_pcr_pid(struct tcc_demux_handle *dmx, unsigned int index,
		      unsigned int pcr_pid);
int hwdmx_get_stc(struct tcc_demux_handle *dmx, unsigned int index, u64 *stc);
int hwdmx_set_cipher_dec_pid(struct tcc_demux_handle *dmx,
			     unsigned int numOfPids, unsigned int delete_option,
			     unsigned short *pids);
int hwdmx_set_cipher_mode(struct tcc_demux_handle *dmx, int algo, int opmode,
			  int residual, int smsg, unsigned int numOfPids,
			  unsigned short *pids);
int hwdmx_set_key(struct tcc_demux_handle *dmx, int keytype, int keymode,
		  int size, void *key);

int hwdmx_input_ts(int devid, uintptr_t mmap_buf, int size);

/* deprecated */
int hwdmx_input_stream(uintptr_t mmap_buf, int size);

#endif /* INCLUDED_HWDMX_CORE */
