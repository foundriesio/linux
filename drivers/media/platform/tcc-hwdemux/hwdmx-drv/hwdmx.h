// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _TCC_HWDMX_H_
#define _TCC_HWDMX_H_

struct tcc_hwdmx_inst_t {
	struct dvb_adapter adapter;
	struct tcc_fe_inst_t fe;
	struct tcc_dmx_inst_t dmx;
	struct tcc_tsif_inst_t tsif;
	struct clk *tsif_clk;
};

#endif //_TCC_HWDMX_H_
