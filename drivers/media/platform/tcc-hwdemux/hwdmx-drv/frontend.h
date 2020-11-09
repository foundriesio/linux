// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _TCC_FE_H_
#define _TCC_FE_H_

struct tcc_fe_priv_t {
	struct device_node *of_node;	// child node
	struct dvb_frontend fe;
	int isUsing;
};

struct tcc_fe_inst_t {
	struct dvb_adapter *adapter;
	struct device *dev;

	int dev_num;
	struct tcc_fe_priv_t *fe;
};

int tcc_fe_init(struct tcc_fe_inst_t *inst);
int tcc_fe_deinit(struct tcc_fe_inst_t *inst);

struct tcc_dxb_fe_driver {
	int (*probe)(struct tcc_fe_priv_t *);
	const char *compatible;
	struct tcc_fe_priv_t **fe;
	struct dvb_frontend_ops *fe_ops;
};

extern int tcc_fe_register(struct tcc_dxb_fe_driver *pdrv);
extern int tcc_fe_unregister(struct tcc_dxb_fe_driver *pdrv);

#endif //_TCC_FE_H_
