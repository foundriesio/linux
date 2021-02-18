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

#ifndef _TCA_HWDEMUX_CIPHER_H_
#define _TCA_HWDEMUX_CIPHER_H_

#include "tcc_cipher_ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int HWDEMUXCIPHERCMD_Set_Algorithm(struct stHWDEMUXCIPHER_ALGORITHM *
					  pARG, unsigned int uiTimeOut);
extern int HWDEMUXCIPHERCMD_Set_Key(struct stHWDEMUXCIPHER_KEY *pARG,
				    unsigned int uiTotalIndex,
				    unsigned int uiCurrentIndex,
				    unsigned int uiTimeOut);
extern int HWDEMUXCIPHERCMD_Set_IV(struct stHWDEMUXCIPHER_VECTOR *pARG,
				   unsigned int uiTimeOut);
extern int HWDEMUXCIPHERCMD_Cipher_Run(struct stHWDEMUXCIPHER_EXECUTE *pARG,
				       unsigned int uiTimeOut);
#ifdef __cplusplus
}
#endif
#endif
