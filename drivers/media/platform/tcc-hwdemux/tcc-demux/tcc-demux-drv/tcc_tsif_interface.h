/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef	_TCC_TSIF_INTERFACE_H_
#define	_TCC_TSIF_INTERFACE_H_

#include <linux/interrupt.h>

#define MAX_PCR_CNT                 2
#define MAX_TS_CNT                  32
#define MAX_SEC_CNT                 256
#define PID_NULL                    0xffff

#define TS_PACKET_LIST              1000

enum {
	TSIF_FILTER_TYPE_SECTION = 0,
	TSIF_FILTER_TYPE_TS,
	TSIF_FILTER_TYPE_MAX,
};

struct packet_info
{
	int valid; //1:valid, 0:invalid
	char *p1;
	int p1_size;

	char *p2;
	int p2_size;
	struct list_head list;
};

struct packet_list
{
	int current_index;
	spinlock_t lock;
	struct packet_info ts_packet[TS_PACKET_LIST];
};

typedef struct pcr_table_t
{
	unsigned short iPID[MAX_PCR_CNT];
} pcr_table_t;

typedef struct ts_table_t
{
	unsigned short iPID[MAX_TS_CNT];
	unsigned short iUsers[MAX_TS_CNT];
} ts_table_t;

typedef struct sec_table_t
{
	unsigned short iPID[MAX_SEC_CNT];
	unsigned short iUseSW[MAX_SEC_CNT];
} sec_table_t;

typedef struct tcc_tsif_tasklet_t {
	struct tasklet_struct tsif_tasklet;
	struct packet_list ts_packet_list;
	struct list_head ts_packet_bank;
} tcc_tsif_tasklet_t;

typedef struct tcc_tsif_priv_t {
	struct file filp;
	struct mutex mutex;
	int users;

	int is_start_dma;

	struct pcr_table_t pcr_table;
	struct ts_table_t  ts_table;
	struct sec_table_t sec_table;

	struct tcc_tsif_tasklet_t task[TSIF_FILTER_TYPE_MAX];
} tcc_tsif_priv_t;

typedef struct tcc_tsif_inst_t {
	struct dvb_adapter *adapter;

	int dev_num;
	tcc_tsif_priv_t *tsif;
} tcc_tsif_inst_t;


int tcc_tsif_init(tcc_tsif_inst_t *tsif);
int tcc_tsif_deinit(tcc_tsif_inst_t *tsif);

int tcc_tsif_interface_open(unsigned int devid);
int tcc_tsif_interface_close(unsigned int devid);
int tcc_tsif_interface_set_pid(struct dvb_demux_feed *feed, unsigned int devid);
int tcc_tsif_interface_remove_pid(struct dvb_demux_feed *feed, unsigned int devid);
int tcc_tsif_interface_set_pcrpid(int on, struct dvb_demux_feed *feed, unsigned int devid);
unsigned int tcc_tsif_interface_get_stc(unsigned int index, unsigned int devid);

#endif//_TCC_TSIF_INTERFACE_H_
