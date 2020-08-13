// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

//*******************************************
//	Remote controller define
//*******************************************

#ifndef	__TCC_CEC_INTERFACE_H__
#define	__TCC_CEC_INTERFACE_H__


/*****************************************************************************
*
* Function define
*
******************************************************************************/
extern int TccCECInterface_Init(struct tcc_hdmi_cec_dev * dev);
extern int TccCECInterface_ParseMessage(struct tcc_hdmi_cec_dev * dev, int size);

#endif /* end of __TCC_CEC_INTERFACE_H__ */

