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

#ifndef	_TCC_FE_H_
#define	_TCC_FE_H_

typedef struct tcc_fe_priv_t {
	struct device_node *of_node;           // child node
	struct dvb_frontend fe;
	int isUsing;
} tcc_fe_priv_t;

typedef struct tcc_fe_inst_t {
	struct dvb_adapter *adapter;
	struct device_node *of_node;           // parent node

	int dev_num;
	tcc_fe_priv_t *fe;
} tcc_fe_inst_t;

int tcc_fe_init(tcc_fe_inst_t *inst);
int tcc_fe_deinit(tcc_fe_inst_t *inst);


struct tcc_dxb_fe_driver {
	int (*probe)(struct tcc_fe_priv_t *);
	const char *compatible;
	tcc_fe_priv_t **fe;
	struct dvb_frontend_ops *fe_ops;
};

extern int tcc_fe_register(struct tcc_dxb_fe_driver *pdrv);
extern int tcc_fe_unregister(struct tcc_dxb_fe_driver *pdrv);

#endif//_TCC_FE_H_
