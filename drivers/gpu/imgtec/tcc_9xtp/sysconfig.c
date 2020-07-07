/*******************************************************************************
*   FileName : sysconfig.c
*   Copyright (c) Telechips Inc.
*   SPDX-license-Identifier : Dual MIT/GPLv2
*   Description : 9XTP GT9524 system configuration

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

#include <linux/platform_device.h>

#include "interrupt_support.h"
#include "pvrsrv_device.h"
#include "sysconfig.h"
#include "physheap.h"
#include "tcc_9xtp_init.h"
#include <soc/tcc/pmap.h>
#include "pvrsrv.h"

static pmap_t pmap_pvr_vz;

typedef enum _PHYS_HEAP_IDX_
{
	PHYS_HEAP_IDX_SYSMEM,
	PHYS_HEAP_IDX_CARVEOUT,
	PHYS_HEAP_IDX_COUNT,
} PHYS_HEAP_IDX;

static RGX_TIMING_INFORMATION	gsRGXTimingInfo;
static RGX_DATA					gsRGXData;
static PVRSRV_DEVICE_CONFIG		gsDevCfg;
static PHYS_HEAP_FUNCTIONS		gsPhysHeapFuncs;
static PHYS_HEAP_CONFIG			gsPhysHeapConfig[PHYS_HEAP_IDX_COUNT];

/*
	CPU to Device physical address translation
*/
static
void UMAPhysHeapCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
								   IMG_UINT32 ui32NumOfAddr,
								   IMG_DEV_PHYADDR *psDevPAddr,
								   IMG_CPU_PHYADDR *psCpuPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	psDevPAddr[0].uiAddr = psCpuPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psDevPAddr[ui32Idx].uiAddr = psCpuPAddr[ui32Idx].uiAddr;
		}
	}
}

/*
	Device to CPU physical address translation
*/
static
void UMAPhysHeapDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
								   IMG_UINT32 ui32NumOfAddr,
								   IMG_CPU_PHYADDR *psCpuPAddr,
								   IMG_DEV_PHYADDR *psDevPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	psCpuPAddr[0].uiAddr = psDevPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psCpuPAddr[ui32Idx].uiAddr = psDevPAddr[ui32Idx].uiAddr;
		}
	}
}

PVRSRV_ERROR SysDevInit(void *pvOSDevice, PVRSRV_DEVICE_CONFIG **ppsDevConfig)
{
	int iIrq;
	struct resource *psDevMemRes = NULL;
	struct platform_device *psDev;

	psDev = to_platform_device((struct device *)pvOSDevice);

	if (gsDevCfg.pvOSDevice)
		return PVRSRV_ERROR_INVALID_DEVICE;

	/*
	 * Device Setup
	 */
	gsDevCfg.pvOSDevice            = pvOSDevice;
	gsDevCfg.pszName                = "TCC_PowerVR_9XTP";
	gsDevCfg.pszVersion              = NULL;

	/* Device setup information */
	psDevMemRes = platform_get_resource(psDev, IORESOURCE_MEM, 0);
	if (psDevMemRes)
	{
		gsDevCfg.sRegsCpuPBase.uiAddr = psDevMemRes->start;
		gsDevCfg.ui32RegsSize         = resource_size(psDevMemRes);
	}
	else
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: platform_get_resource failed", __func__));
		gsDevCfg.sRegsCpuPBase.uiAddr = TCC_PowerVR_9XTP_PBASE;
		gsDevCfg.ui32RegsSize         = TCC_PowerVR_9XTP_SIZE;
	}

	iIrq = platform_get_irq(psDev, 0);
	if (iIrq >= 0)
		gsDevCfg.ui32IRQ  = (IMG_UINT32) iIrq;
	else
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: platform_get_irq failed (%d)", __func__, -iIrq));
		gsDevCfg.ui32IRQ = TCC_PowerVR_9XTP_IRQ;
	}

//	gsDevCfg.eCacheSnoopingMode     = PVRSRV_DEVICE_SNOOP_EMULATED;

	/* No power management on TCC system */
	gsDevCfg.pfnPrePowerState       = TccPrePowerState;
	gsDevCfg.pfnPostPowerState      = TccPostPowerState;
		
	/* Setup RGX specific timing data */
	gsRGXTimingInfo.ui32CoreClockSpeed	  = RGX_TCC8059_CORE_CLOCK_SPEED;
	gsRGXTimingInfo.bEnableActivePM		  = IMG_TRUE;
	gsRGXTimingInfo.bEnableRDPowIsland      = IMG_FALSE;
	gsRGXTimingInfo.ui32ActivePMLatencyms  = SYS_RGX_ACTIVE_POWER_LATENCY_MS;

	/* Setup RGX specific data */
	gsRGXData.psRGXTimingInfo = &gsRGXTimingInfo;

	/* No clock frequency either */
	gsDevCfg.pfnClockFreqGet        = NULL;
	gsDevCfg.pfnCheckMemAllocSize   = NULL;
	gsDevCfg.hDevData               = &gsRGXData;
	gsDevCfg.bHasFBCDCVersion31 = IMG_FALSE;

	/* TCC 9XTP Init */
	gsDevCfg.hSysData = (IMG_HANDLE)RgxTccInit(&gsDevCfg);
	if (!gsDevCfg.hSysData)
	{
		gsDevCfg.pvOSDevice = NULL;
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}


	/*
	 * Setup information about physical memory heap(s) we have
	 */
	gsPhysHeapFuncs.pfnCpuPAddrToDevPAddr = UMAPhysHeapCpuPAddrToDevPAddr;
	gsPhysHeapFuncs.pfnDevPAddrToCpuPAddr = UMAPhysHeapDevPAddrToCpuPAddr;

	/* GENERAL heap represents memory managed by the kernel */
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].ui32PhysHeapID = PHYS_HEAP_IDX_SYSMEM;
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].pszPDumpMemspaceName = "SYSMEM";
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].psMemFuncs = &gsPhysHeapFuncs;
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].hPrivData = NULL;
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].eType = PHYS_HEAP_TYPE_UMA;
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].ui32NumOfRegions = 0;

	/* FW heap uses carveout memory */	
	gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].ui32PhysHeapID = PHYS_HEAP_IDX_CARVEOUT;	
	gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].pszPDumpMemspaceName = "SYSMEM_FW";	
	gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].psMemFuncs = &gsPhysHeapFuncs;	
	gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].hPrivData = NULL;	
	
	if( PVRSRV_VZ_MODE_IS(NATIVE))//PVRSRV_APPHINT_DRIVERMODE == IMG_UINT32_C(DRIVER_MODE_NATIVE))
		gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].eType = PHYS_HEAP_TYPE_UMA;	
	else
	{
		if(0 > pmap_get_info("pvr_vz", &pmap_pvr_vz)){
			printk("%s-%d : pvr_vz allocation is failed.\n", __func__, __LINE__);
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}
		printk("@@@@@@@@@@@@@@@@@@@@ %s - 0x%x - 0x%x - %d - %d - %d\n",
					pmap_pvr_vz.name, pmap_pvr_vz.base, pmap_pvr_vz.size,
					pmap_pvr_vz.groups, pmap_pvr_vz.rc, pmap_pvr_vz.flags);
		gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].eType = PHYS_HEAP_TYPE_LMA;	

		gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].ui32NumOfRegions = 1;	
		gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].pasRegions = OSAllocMem(sizeof(PHYS_HEAP_REGION));	
		gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].pasRegions->uiSize = RGX_NUM_OS_SUPPORTED * RGX_FIRMWARE_RAW_HEAP_SIZE + PVRSRV_1M_PAGE_SIZE;	
		gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].pasRegions->sStartAddr.uiAddr = pmap_pvr_vz.base;	
		gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].pasRegions->sCardBase.uiAddr = pmap_pvr_vz.base;
	}
	/* Device's physical heaps */
	gsDevCfg.pasPhysHeaps             = gsPhysHeapConfig;
	gsDevCfg.ui32PhysHeapCount     = ARRAY_SIZE(gsPhysHeapConfig);

	/* Device's physical heap IDs */
	gsDevCfg.aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL] = PHYS_HEAP_IDX_SYSMEM;
	gsDevCfg.aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL] = PHYS_HEAP_IDX_SYSMEM;
	gsDevCfg.aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL]  = PHYS_HEAP_IDX_CARVEOUT;
	gsDevCfg.aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_EXTERNAL] = PHYS_HEAP_IDX_SYSMEM;

	*ppsDevConfig = &gsDevCfg;

	return PVRSRV_OK;
}

void SysDevDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVR_UNREFERENCED_PARAMETER(psDevConfig);

	OSFreeMem(gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].pasRegions);
	gsPhysHeapConfig[PHYS_HEAP_IDX_CARVEOUT].pasRegions = NULL;

	/* Tcc UnInit */
	RgxTccUnInit(psDevConfig->hSysData);
	psDevConfig->hSysData = NULL;

	psDevConfig->pvOSDevice = NULL;
}

static IMG_BOOL SystemISRHandler(void *pvData)
{
	tcc_context *psSysData = pvData;
	IMG_BOOL bHandled;

	/* Any special system interrupt handling goes here */
	bHandled = psSysData->pfnDeviceLISR(psSysData->pvDeviceLISRData);

	return bHandled;
}

PVRSRV_ERROR SysInstallDeviceLISR(IMG_HANDLE hSysData,
								  IMG_UINT32 ui32IRQ,
								  const IMG_CHAR *pszName,
								  PFN_LISR pfnLISR,
								  void *pvData,
								  IMG_HANDLE *phLISRData)
{
	tcc_context *psSysData = (tcc_context *)hSysData;
	PVRSRV_ERROR eError;
	if (psSysData->hSysLISRData)	
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: ISR for %s already installed!", __func__, pszName));
		return PVRSRV_ERROR_CANT_REGISTER_CALLBACK;	
	}	

	/* Wrap the device LISR */	
	psSysData->pfnDeviceLISR = pfnLISR;
	psSysData->pvDeviceLISRData = pvData;
	
	eError = OSInstallSystemLISR(&psSysData->hSysLISRData, ui32IRQ, pszName,	                             
							SystemISRHandler, psSysData,
							SYS_IRQ_FLAG_TRIGGER_DEFAULT);
	if (eError != PVRSRV_OK)	
	{
		return eError;
	}

	*phLISRData = psSysData;
	PVR_LOG(("Installed device LISR %s on IRQ %d", pszName, ui32IRQ));

	return PVRSRV_OK;	
}

PVRSRV_ERROR SysUninstallDeviceLISR(IMG_HANDLE hLISRData)
{
	tcc_context *psSysData = (tcc_context *)hLISRData;
	PVRSRV_ERROR eError;

	PVR_ASSERT(psSysData);

	eError = OSUninstallSystemLISR(psSysData->hSysLISRData);
	if (eError != PVRSRV_OK)	
	{
		return eError;
	}

	/* clear interrupt data */
	psSysData->pfnDeviceLISR    = NULL;	
	psSysData->pvDeviceLISRData = NULL;
	psSysData->hSysLISRData     = NULL;

	return PVRSRV_OK;
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_DEVICE_CONFIG *psDevConfig,
						  DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
						  void *pvDumpDebugFile)
{
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(pfnDumpDebugPrintf);
	PVR_UNREFERENCED_PARAMETER(pvDumpDebugFile);

	return PVRSRV_OK;
}

/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
