/*************************************************************************/ /*!
@File           vz_physheap_generic.c
@Title          System virtualization physheap configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System virtualization physical heap configuration
@License        Dual MIT/GPLv2

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
#include "allocmem.h"
#include "physheap.h"
#include "rgxdevice.h"
#include "pvrsrv_device.h"
#include "rgxfwutils.h"

#include "dma_support.h"
#include "vz_support.h"
#include "vz_vmm_pvz.h"
#include "vz_physheap.h"


static PVRSRV_ERROR
SysVzCreatePhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig,
					PVRSRV_DEVICE_PHYS_HEAP ePhysHeap)
{
	IMG_UINT64 ui64HeapSize;
	IMG_DEV_PHYADDR sHeapBase;
	PHYS_HEAP_TYPE eHeapMgmtType;
	PHYS_HEAP_CONFIG *psPhysHeapConfig;
	PVRSRV_ERROR eError = PVRSRV_OK;

	/* Lookup GPU/FW physical heap config, allocate primary region */
	psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, ePhysHeap);
	PVR_LOG_GOTO_IF_FALSE((NULL != psPhysHeapConfig), "Invalid physheap config", e0);

	/* Lookup physheap addr/size from VM manager type */
	eError = SysVzGetPhysHeapAddrSize(psDevConfig,
									  ePhysHeap,
									  &sHeapBase,
									  &ui64HeapSize);
	PVR_LOG_GOTO_IF_ERROR(eError, "SysVzGetPhysHeapAddrSize", e0);

	/* ... UMA / LMA / DMA ... */
	if (sHeapBase.uiAddr == 0)
	{
		eHeapMgmtType = (ui64HeapSize == 0) ? PHYS_HEAP_TYPE_UMA : PHYS_HEAP_TYPE_DMA;
	}
	else
	{
		eHeapMgmtType = (ui64HeapSize == 0) ? PHYS_HEAP_TYPE_UNKNOWN : PHYS_HEAP_TYPE_LMA;
	}

	if (ePhysHeap == PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL)
	{
		/* Firmware physheaps require additional init */
		psPhysHeapConfig->pszPDumpMemspaceName = "SYSMEM";
		psPhysHeapConfig->psMemFuncs = psDevConfig->pasPhysHeaps[0].psMemFuncs;
	}

	switch (eHeapMgmtType)
	{
		case PHYS_HEAP_TYPE_UMA:
		{
			/* UMA heaps are managed using the kernel's facilities, no further setup required */
#if defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
			if (ePhysHeap == PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: %s Invalid firmware physical heap configuration:"
										"static carveout systems must have a fixed base address.",
										__func__, PVRSRV_VZ_MODE_IS(HOST) ? "Host" : "Guest"));
				eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
				goto e0;
			}
#endif
			break;
		}

		case PHYS_HEAP_TYPE_DMA:
		{
			IMG_UINT64 ui64DmaAllocSize;
			DMA_ALLOC *psDmaAlloc;
			PHYS_HEAP_REGION *psRegion;

#if defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
			/* on static setups using carveout memory, the Host driver must preallocate
			 * continguous memory for all Guests. Host uses only the initial span of ui64HeapSize */
			ui64DmaAllocSize = PVRSRV_VZ_MODE_IS(HOST) ? (ui64HeapSize * RGX_NUM_OS_SUPPORTED) : ui64HeapSize;
#else
			ui64DmaAllocSize = ui64HeapSize;
#endif

			if (psPhysHeapConfig->pasRegions == NULL)
			{
				PVR_LOG_GOTO_IF_FALSE((psPhysHeapConfig->ui32NumOfRegions == 0),
								"Invalid heap config: no regions defined, but region counter > 0", e0);

				psRegion = OSAllocZMem(sizeof(PHYS_HEAP_REGION));
				PVR_LOG_GOTO_IF_NOMEM(psRegion, eError, e0);

				psPhysHeapConfig->ui32NumOfRegions++;
			}
			else
			{
				psRegion = psPhysHeapConfig->pasRegions;
			}

			if (psRegion->hPrivData == NULL)
			{
				psDmaAlloc = OSAllocZMem(sizeof(DMA_ALLOC));
				psRegion->hPrivData = psDmaAlloc;
				PVR_LOG_GOTO_IF_NOMEM(psDmaAlloc, eError, e1);

				psDmaAlloc->pvOSDevice = psDevConfig->pvOSDevice;
			}
			else
			{
				psDmaAlloc = (DMA_ALLOC*)psRegion->hPrivData;
			}

			psDmaAlloc->ui64Size = ui64DmaAllocSize;

			eError = SysDmaAllocMem(psDmaAlloc);
			PVR_LOG_GOTO_IF_ERROR(eError, "SysDmaAllocMem", e1);

			/* Verify the validity of DMA physheap region */
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			PVR_LOG_GOTO_IF_FALSE((psDmaAlloc->sBusAddr.uiAddr != 0), "Invalid DMA physheap base address", e1);
			PVR_LOG_GOTO_IF_FALSE((psDmaAlloc->ui64Size == ui64DmaAllocSize), "Invalid DMA physheap size", e1);
			eError = PVRSRV_OK;

			eError = SysDmaRegisterForIoRemapping(psDmaAlloc);
			PVR_LOG_GOTO_IF_ERROR(eError, "SysDmaRegisterForIoRemapping", e1);

			/* initialise the region with the DMA allocation */
			psRegion->sStartAddr.uiAddr = psDmaAlloc->sBusAddr.uiAddr;
			psRegion->sCardBase.uiAddr = psDmaAlloc->sBusAddr.uiAddr;

			/* the driver uses only its designated heap size, in case of over-allocation */
			psRegion->uiSize = ui64HeapSize;

			/* copy back the region pointer into the phys config in case it was NULL at init */
			psPhysHeapConfig->pasRegions = psRegion;
			break;
		}

		case PHYS_HEAP_TYPE_LMA:
		{
			PHYS_HEAP_REGION *psRegion;

			if (psPhysHeapConfig->pasRegions == NULL)
			{
				PVR_LOG_GOTO_IF_FALSE((psPhysHeapConfig->ui32NumOfRegions == 0),
								"Invalid heap config: no regions defined, but region counter > 0", e0);

				psRegion = OSAllocZMem(sizeof(PHYS_HEAP_REGION));
				PVR_LOG_GOTO_IF_NOMEM(psRegion, eError, e0);

				psPhysHeapConfig->ui32NumOfRegions++;

				/* initialise the region */
				if (psPhysHeapConfig->psMemFuncs->pfnDevPAddrToCpuPAddr)
				{
					psPhysHeapConfig->psMemFuncs->pfnDevPAddrToCpuPAddr(NULL, 1, &psRegion->sStartAddr, &sHeapBase);
				}
				else
				{
					psRegion->sStartAddr.uiAddr = sHeapBase.uiAddr;
				}

				psRegion->sCardBase = sHeapBase;
				psRegion->uiSize = ui64HeapSize;
			}
			else
			{
				psRegion = psPhysHeapConfig->pasRegions;
			}

			/* Verify the validity of the UMA carve-out physheap region */
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			PVR_LOG_GOTO_IF_FALSE((psRegion->sStartAddr.uiAddr != 0),"Invalid LMA/carveout physheap CPU base address", e0);
			PVR_LOG_GOTO_IF_FALSE((psRegion->sCardBase.uiAddr != 0), "Invalid LMA/carveout physheap DEV base address", e0);
			PVR_LOG_GOTO_IF_FALSE((psRegion->uiSize != 0),           "Invalid LMA/carveout physheap size", e0);
			eError = PVRSRV_OK;

			/* copy back the region pointer into the phys config in case it was NULL at init */
			psPhysHeapConfig->pasRegions = psRegion;
			break;
		}

		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Physical heaps of fixed base address but unbounded size are not supported.", __func__));
			PVR_DPF((PVR_DBG_ERROR, "%s: Invalid heap type = %u; base address = 0x%"IMG_UINT64_FMTSPECx, __func__, eHeapMgmtType, sHeapBase.uiAddr));
			eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		}
	}

	psPhysHeapConfig->eType = eHeapMgmtType;

e0:
	return eError;

e1:
	if (psPhysHeapConfig->pasRegions != NULL)
	{
		SysDmaFreeMem(psPhysHeapConfig->pasRegions[0].hPrivData);
		OSFreeMem(psPhysHeapConfig->pasRegions[0].hPrivData);
	}

	OSFreeMem(psPhysHeapConfig->pasRegions);
	psPhysHeapConfig->pasRegions = NULL;

	return eError;
}

static void
SysVzDestroyPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig,
					 PVRSRV_DEVICE_PHYS_HEAP ePhysHeap)
{
	PHYS_HEAP_CONFIG *psPhysHeapConfig;

	psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, ePhysHeap);
	if (psPhysHeapConfig == NULL ||
		psPhysHeapConfig->pasRegions == NULL)
	{
		return;
	}

	switch (psPhysHeapConfig->eType)
	{
		case PHYS_HEAP_TYPE_UMA:
		{
			/* nothing to do for an OS managed heap */
			break;
		}
		case PHYS_HEAP_TYPE_DMA:
		{
			DMA_ALLOC *psDmaAlloc = psPhysHeapConfig->pasRegions[0].hPrivData;

			SysDmaDeregisterForIoRemapping(psDmaAlloc);
			SysDmaFreeMem(psDmaAlloc);
			OSFreeMem(psDmaAlloc);
			psPhysHeapConfig->pasRegions[0].hPrivData = NULL;
			OSFreeMem(psPhysHeapConfig->pasRegions);
			psPhysHeapConfig->pasRegions = NULL;
			psPhysHeapConfig->ui32NumOfRegions--;
			break;
		}
		case PHYS_HEAP_TYPE_LMA:
		{
			/* nothing to do for a driver managed heap */
			break;
		}
		default:
		{
			/* uninitialised heap */
			break;
		}
	}
}

PVRSRV_ERROR SysVzInitDevPhysHeaps(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;

	eError = SysVzCreatePhysHeap(psDevConfig, PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL);
	PVR_LOG_RETURN_IF_ERROR(eError, "SysVzCreatePhysHeap(PHYS_HEAP_FW_LOCAL)");

	eError = SysVzCreatePhysHeap(psDevConfig, PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL);
	PVR_LOG_RETURN_IF_ERROR(eError, "SysVzCreatePhysHeap(PHYS_HEAP_GPU_LOCAL)");

	return eError;
}

void SysVzDeInitDevPhysHeaps(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	SysVzDestroyPhysHeap(psDevConfig, PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL);
	SysVzDestroyPhysHeap(psDevConfig, PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL);
}

/******************************************************************************
 End of file (vz_physheap_generic.c)
******************************************************************************/
