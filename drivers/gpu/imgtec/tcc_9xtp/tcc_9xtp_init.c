// SPDX-License-Identifier: Dual MIT/GPL
/*
 *   FileName : tcc_9XTP_init.c
 *   Copyright (c) Telechips Inc.
 *   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 *   Description : 9XTP GT9524 Initialization

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

#if defined(SUPPORT_PDVFS)
#include "rgxpdvfs.h"
#endif


#include <linux/clkdev.h>
#include <linux/hardirq.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/version.h>
#include "power.h"
#include "tcc_9xtp_init.h"
#include "pvrsrv_device.h"
#include "pvrsrv.h"
#include "syscommon.h"
#include <linux/clk-provider.h>
#include <linux/pm_runtime.h>
#include <linux/pm_opp.h>
#include <linux/devfreq_cooling.h>
#include <linux/thermal.h>
#include "rgxdevice.h"

#include <linux/dma-mapping.h>

static struct tcc_context *g_platform;
//#define CLK_CONTROL_IN_TF_A_ROM

static void RgxEnableClock(struct tcc_context *platform)
{
	if (!platform->gpu_clk)	{
		PVR_DPF((PVR_DBG_ERROR, "gpu_clk is null\n"));
		return;
	}

	if (!platform->gpu_active) {
		#if defined(CLK_CONTROL_IN_TF_A_ROM)
		clk_prepare_enable(platform->gpu_clk);
		#else
		//CLKMASK unmask
		OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_CLKMASK,
			(IMG_UINT32)GPU_3DENGINE_CLKMASK_FULL_MASK);
		#endif
		platform->gpu_active = IMG_TRUE;
	}
}

static void RgxDisableClock(struct tcc_context *platform)
{
	if (!platform->gpu_clk) {
		PVR_DPF((PVR_DBG_ERROR, "gpu_clk is null\n"));
		return;
	}

	if (platform->gpu_active) {
		#if defined(CLK_CONTROL_IN_TF_A_ROM)
		clk_disable_unprepare(platform->gpu_clk);
		#else
		//CLKMASK mask
		IMG_UINT32 value = OSReadHWReg32(platform->pv3DBusConfReg,
					GPU_3DENGINE_CLKMASK);
		OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_CLKMASK,
			(value & ~((IMG_UINT32)GPU_3DENGINE_CLKMASK_3D_MASK)));
		#endif
		platform->gpu_active = IMG_FALSE;
	}
}

static void RgxEnablePower(struct tcc_context *platform)
{
	struct device *dev = (struct device *)platform->dev_config->pvOSDevice;

	if (!platform->bEnablePd) {
		pm_runtime_get_sync(dev);
		platform->bEnablePd = IMG_TRUE;
	}
}

static void RgxDisablePower(struct tcc_context *platform)
{
	struct device *dev = (struct device *)platform->dev_config->pvOSDevice;

	if (platform->bEnablePd) {
		pm_runtime_put_sync(dev);
		platform->bEnablePd = IMG_FALSE;
	}
}

void RgxResume(struct tcc_context *platform)
{
//	if( gRev == 0 )
	{
		RgxEnablePower(platform);
		RgxEnableClock(platform);
	}
}

void RgxSuspend(struct tcc_context *platform)
{
//        if( gRev == 0 )
	{
		RgxDisableClock(platform);
		RgxDisablePower(platform);
	}
}

PVRSRV_ERROR TccPrePowerState(IMG_HANDLE hSysData,
				 PVRSRV_DEV_POWER_STATE eNewPowerState,
				 PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				 IMG_BOOL bForced)
{
	struct tcc_context *platform = (struct tcc_context *)hSysData;

	if (PVRSRV_VZ_MODE_IS(GUEST)
		 || (platform->dev_config->psDevNode->bAutoVzFwIsUp))
		return PVRSRV_OK;
	else if ((eNewPowerState != eCurrentPowerState) &&
			 (eNewPowerState != PVRSRV_DEV_POWER_STATE_ON))
		RgxSuspend(platform);

	return PVRSRV_OK;
}

PVRSRV_ERROR TccPostPowerState(IMG_HANDLE hSysData,
				  PVRSRV_DEV_POWER_STATE eNewPowerState,
				  PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				  IMG_BOOL bForced)
{
	struct tcc_context *platform = (struct tcc_context *)hSysData;

	if (PVRSRV_VZ_MODE_IS(GUEST)
		 || (platform->dev_config->psDevNode->bAutoVzFwIsUp))
		return PVRSRV_OK;
	else if ((eNewPowerState != eCurrentPowerState) &&
			 (eCurrentPowerState != PVRSRV_DEV_POWER_STATE_ON))
		RgxResume(platform);

	return PVRSRV_OK;
}

void RgxTccUnInit(struct tcc_context *platform)
{
	struct device *dev = (struct device *)platform->dev_config->pvOSDevice;

	if (platform->gpu_clk) {
		devm_clk_put(dev, platform->gpu_clk);
		platform->gpu_clk = NULL;
	}

#ifdef SUPPORT_AUTOVZ
	/* ensure the firmware detects that this driver is shutting down */
	OSSleepms(PVR_AUTOVZ_WDG_PERIOD_MS);
#endif

	iounmap((void __iomem *) platform->pv3DBusConfReg);
	pm_runtime_disable(dev);
	devm_kfree(dev, platform);
}

struct tcc_context *RgxTccInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	struct resource *psDevMemRes = NULL;
	struct device *dev = (struct device *)psDevConfig->pvOSDevice;
	struct tcc_context *platform;
	RGX_DATA *psRGXData = (RGX_DATA *)psDevConfig->hDevData;
#ifndef SUPPORT_AUTOVZ
	IMG_UINT32 value;
#endif

	platform = devm_kzalloc(dev, sizeof(struct tcc_context), GFP_KERNEL);
	if (platform == NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to kzalloc tcc_context", __func__));
		return NULL;
	}

	g_platform = platform;
	dma_set_coherent_mask(dev, DMA_BIT_MASK(64));

	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;

	PVR_DPF((PVR_DBG_MESSAGE, "%s: dma_mask = %llx", __func__,
			 dev->coherent_dma_mask));

	//To support core-reset in native-mode

	psDevMemRes = platform_get_resource(to_platform_device(dev),
		 IORESOURCE_MEM, 1);
	platform->pv3DBusConfReg = (void __iomem *)ioremap(psDevMemRes->start,
		 resource_size(psDevMemRes));

	if (platform->pv3DBusConfReg == 0) {
		PVR_DPF((PVR_DBG_ERROR, "%s: mapping register bank failed !",
			 __func__));
		return NULL;
	}

#ifndef SUPPORT_AUTOVZ
	OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_PWRDOWN,
			 (IMG_UINT32)GPU_3DENGINE_PWRDOWN_FULL_MASK);
	OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_SWRESET,
			 (IMG_UINT32)GPU_3DENGINE_SWRESET_FULL_MASK);
	OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_CLKMASK,
			 (IMG_UINT32)GPU_3DENGINE_CLKMASK_FULL_MASK);

	//PWRDOWN, SWRESET, CLKMASK
	value = OSReadHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_PWRDOWN);
	OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_PWRDOWN,
		 (value & ~((IMG_UINT32)GPU_3DENGINE_PWRDOWN_PWRDNREQN_MASK)));
	value = OSReadHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_SWRESET);
	OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_SWRESET,
		 (value & ~((IMG_UINT32)GPU_3DENGINE_SWRESET_3D_MASK)));
	value = OSReadHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_CLKMASK);
	OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_CLKMASK,
		 (value & ~((IMG_UINT32)GPU_3DENGINE_CLKMASK_3D_MASK)));

	//pwrdown not request, not reset, unmask
	OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_PWRDOWN,
			 (IMG_UINT32)GPU_3DENGINE_PWRDOWN_FULL_MASK);
	OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_SWRESET,
			 (IMG_UINT32)GPU_3DENGINE_SWRESET_FULL_MASK);
	OSWriteHWReg32(platform->pv3DBusConfReg, GPU_3DENGINE_CLKMASK,
			 (IMG_UINT32)GPU_3DENGINE_CLKMASK_FULL_MASK);
#endif

	platform->dev_config = psDevConfig;
	platform->gpu_active = IMG_FALSE;

	platform->bEnablePd = IMG_FALSE;
	pm_runtime_enable(dev);

	platform->gpu_clk = devm_clk_get(dev, "9XTP_clk");
	if (IS_ERR_OR_NULL(platform->gpu_clk)) {
		PVR_DPF((PVR_DBG_ERROR,
		 "%s: Failed to find gpu_clk clock source", __func__));
		goto fail0;
	}

	if (psRGXData && psRGXData->psRGXTimingInfo) {
		psRGXData->psRGXTimingInfo->ui32CoreClockSpeed
			 = clk_get_rate(platform->gpu_clk);
	}

	return platform;

fail0:
	devm_kfree(dev, platform);
	return NULL;

}
