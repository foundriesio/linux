/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef _TCC_ADMA_PCM_DT_H_
#define _TCC_ADMA_PCM_DT_H_

#include "tcc_adma.h"

typedef enum {
	TCC_ADMA_I2S_STEREO = 0,
	TCC_ADMA_I2S_7_1CH = 1,
	TCC_ADMA_I2S_9_1CH = 2,
	TCC_ADMA_SPDIF = 3,
	TCC_ADMA_CDIF = 4,
	TCC_ADMA_MAX,
} TCC_ADMA_DEV_TYPE;

struct tcc_adma_info {
	TCC_ADMA_DEV_TYPE dev_type;
	bool tdm_mode;
};

#endif //_TCC_ADMA_PCM_DT_H_
