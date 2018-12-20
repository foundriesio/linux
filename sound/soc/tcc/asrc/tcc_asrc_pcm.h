/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
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

#ifndef _TCC_ASRC_PCM_H_
#define _TCC_ASRC_PCM_H_

#include <linux/platform_device.h>
#include "tcc_asrc_drv.h"

int tcc_asrc_pcm_drvinit(struct platform_device *pdev);
extern int tcc_pl080_asrc_pcm_isr_ch(struct tcc_asrc_t *asrc, int asrc_pair);

#endif //_TCC_ASRC_PCM_H_
