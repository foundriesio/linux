// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _TCC_FE_H_
#define _TCC_FE_H_

typedef struct tcc_fe_priv_t
{
	struct device_node *of_node; // child node
	struct dvb_frontend fe;
	int isUsing;
} tcc_fe_priv_t;

typedef struct tcc_fe_inst_t
{
	struct dvb_adapter *adapter;
	struct device *dev;

	int dev_num;
	tcc_fe_priv_t *fe;
} tcc_fe_inst_t;

int tcc_fe_init(tcc_fe_inst_t *inst);
int tcc_fe_deinit(tcc_fe_inst_t *inst);

struct tcc_dxb_fe_driver
{
	int (*probe)(struct tcc_fe_priv_t *);
	const char *compatible;
	tcc_fe_priv_t **fe;
	struct dvb_frontend_ops *fe_ops;
};

extern int tcc_fe_register(struct tcc_dxb_fe_driver *pdrv);
extern int tcc_fe_unregister(struct tcc_dxb_fe_driver *pdrv);

#endif //_TCC_FE_H_
