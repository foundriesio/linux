/* SPDX-License-Identifier: Dual MIT/GPL */
/*
 *   FileName : sysconfig.h
 *   Copyright (c) Telechips Inc.
 *   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 *   Description : System Description Header

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "pvrsrv_device.h"
#include "rgxdevice.h"

#if !defined(__SYSCCONFIG_H__)
#define __SYSCCONFIG_H__


#define RGX_TCC8050_CORE_CLOCK_SPEED (700*1000*1000)	/* Dolphin3M */
#define RGX_TCC8053_CORE_CLOCK_SPEED (420*1000*1000)	/* Dolphin3E */
#define RGX_TCC8059_CORE_CLOCK_SPEED (420*1000*1000)	/* Dolphin+QD */
#define RGX_TCC8060_CORE_CLOCK_SPEED (700*1000*1000)	/* Dolphin3H */
#define TCC_PowerVR_9XTP_PBASE        0x10100000
#define TCC_PowerVR_9XTP_SIZE         0x10000
#define TCC_PowerVR_9XTP_IRQ          123

#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (100)
#define TCC_PMAP_PVR_VZ			IMG_UINT64_C(0x80000000)
/*****************************************************************************
 * system specific data structures
 *****************************************************************************/

#endif	/* __SYSCCONFIG_H__ */
