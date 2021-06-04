/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms
 * of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef _TCC_ASRC_M2M_H_
#define _TCC_ASRC_M2M_H_

#include "tcc_asrc_m2m_ioctl.h"

struct tcc_asrc_t;

extern int tcc_asrc_m2m_drvinit(struct platform_device *pdev);
extern int tcc_asrc_m2m_txisr_ch(struct tcc_asrc_t *asrc,	int asrc_pair);
extern int tcc_pl080_asrc_m2m_txisr_ch(struct tcc_asrc_t *asrc,	int asrc_pair);
extern int tcc_asrc_m2m_start(
	struct tcc_asrc_t *asrc,
	int asrc_pair,
	enum tcc_asrc_drv_bitwidth_t src_bitwidth,
	enum tcc_asrc_drv_bitwidth_t dst_bitwidth,
	enum tcc_asrc_drv_ch_t channels,
	uint32_t ratio_shift22);
extern int tcc_asrc_m2m_stop(struct tcc_asrc_t *asrc, int asrc_pair);
extern int tcc_asrc_m2m_push_data(
	struct tcc_asrc_t *asrc,
	int asrc_pair,
	void *data,
	uint32_t size);
extern int tcc_asrc_m2m_pop_data(
	struct tcc_asrc_t *asrc,
	int asrc_pair,
	void *data,
	uint32_t size);

extern int tcc_asrc_m2m_volume_gain(
	struct tcc_asrc_t *asrc,
	int asrc_pair,
	uint32_t volume_gain);
extern int tcc_asrc_m2m_volume_ramp(
	struct tcc_asrc_t *asrc,
	int asrc_pair,
	uint32_t ramp_gain,
	uint32_t dn_time,
	uint32_t dn_wait,
	uint32_t up_time,
	uint32_t up_wait);

#endif /*_TCC_ASRC_M2M_H_*/
