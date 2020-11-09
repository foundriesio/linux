// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _TCC_TSIF_H_
#define _TCC_TSIF_H_

#include <linux/interrupt.h>
#include "../hwdmx-core/hwdmx_core.h"

#define MAX_PCR_CNT 2
#define MAX_TS_CNT 128
#define MAX_SEC_CNT 256
#define PID_NULL 0xffff

#define TS_PACKET_LIST 1000

enum {
	TSIF_FILTER_TYPE_SECTION = 0,
	TSIF_FILTER_TYPE_TS,
	TSIF_FILTER_TYPE_MAX,
};

struct packet_info {
	int valid;		// 1:valid, 0:invalid
	char *p1;
	int p1_size;

	char *p2;
	int p2_size;
	struct list_head list;
};

struct packet_list {
	int current_index;
	spinlock_t lock;
	struct packet_info ts_packet[TS_PACKET_LIST];
};

struct pcr_table_t {
	unsigned short iPID[MAX_PCR_CNT];
};

struct ts_table_t {
	unsigned short iPID[MAX_TS_CNT];
	unsigned short iUsers[MAX_TS_CNT];
};

struct sec_table_t {
	unsigned short iPID[MAX_SEC_CNT];
	unsigned short iUseSW[MAX_SEC_CNT];
	unsigned short iNum;
};

struct tcc_tsif_tasklet_t {
	struct tasklet_struct tsif_tasklet;
	struct packet_list ts_packet_list;
	struct list_head ts_packet_bank;
};

struct tcc_tsif_priv_t {
	struct tcc_demux_handle demux;
	struct mutex mutex;
	int users;

	int is_start_dma;

	struct pcr_table_t pcr_table;
	struct ts_table_t ts_table;
	struct sec_table_t sec_table;

	struct tcc_tsif_tasklet_t task[TSIF_FILTER_TYPE_MAX];
};

struct tcc_tsif_inst_t {
	struct dvb_adapter *adapter;
	struct device *dev;

	int dev_num;
	struct tcc_tsif_priv_t *tsif;
};

int tcc_tsif_init(struct tcc_tsif_inst_t *tsif);
int tcc_tsif_deinit(struct tcc_tsif_inst_t *tsif);

int tcc_tsif_start(unsigned int devid);
int tcc_tsif_stop(unsigned int devid);
int tcc_tsif_set_pid(struct dvb_demux_feed *feed, unsigned int devid);
int tcc_tsif_remove_pid(struct dvb_demux_feed *feed, unsigned int devid);
int tcc_tsif_set_pcrpid(int on, struct dvb_demux_feed *feed,
			unsigned int devid);
int tcc_tsif_get_stc(unsigned int index, unsigned int devid, u64 *stc);
int tcc_tsif_can_write(unsigned int devid);

#endif //_TCC_TSIF_H_
