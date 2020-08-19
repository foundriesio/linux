/*************************************************************************/ /*!
@File
@Title          Functions for recording ION memory stats.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef PVR_ION_STATS_H
#define PVR_ION_STATS_H

#include "pvrsrv_error.h"
#include "img_defs.h"

struct dma_buf;

#if defined(PVRSRV_ENABLE_PVR_ION_STATS)
PVRSRV_ERROR PVRSRVIonStatsInitialise(void);

void PVRSRVIonStatsDestroy(void);

void PVRSRVIonAddMemAllocRecord(struct dma_buf *psDmaBuf);

void PVRSRVIonRemoveMemAllocRecord(struct dma_buf *psDmaBuf);
#else
static INLINE PVRSRV_ERROR PVRSRVIonStatsInitialise(void)
{
	return PVRSRV_OK;
}

static INLINE void PVRSRVIonStatsDestroy(void)
{
}

static INLINE void PVRSRVIonAddMemAllocRecord(struct dma_buf *psDmaBuf)
{
	PVR_UNREFERENCED_PARAMETER(psDmaBuf);
}

static INLINE void PVRSRVIonRemoveMemAllocRecord(struct dma_buf *psDmaBuf)
{
	PVR_UNREFERENCED_PARAMETER(psDmaBuf);
}
#endif /* defined(PVRSRV_ENABLE_PVR_ION_STATS) */

#endif /* PVR_ION_STATS_H */
