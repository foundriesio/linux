// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <asm/io.h>

#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"


//#define ENABLE_REG_RW_DBG

static u32 __dptx_readl( struct Dptx_Params *pstDptx, u32 uiOffset )
{
	u32 uiReadData = __raw_readl( pstDptx->pioDPLink_BaseAddr + uiOffset );

	return uiReadData;
}

static void __dptx_writel( struct Dptx_Params *pstDptx, u32 uiOffset, u32 uiData )
{
	__raw_writel( uiData, ( pstDptx->pioDPLink_BaseAddr + uiOffset ));
}

u32 Dptx_Reg_Readl( struct Dptx_Params *pstDptx, u32 uiOffset )
{
	u32			uiReadData;
	
	uiReadData = __dptx_readl( pstDptx, uiOffset );

	return (uiReadData);
}

void Dptx_Reg_Writel( struct Dptx_Params *pstDptx, u32 uiOffset, u32 uiData )
{
	__dptx_writel( pstDptx, uiOffset, uiData );
}

u32 Dptx_Reg_Direct_Read( volatile void __iomem *Reg )
{
	u32 uiReadData = __raw_readl( Reg );
	
	return uiReadData;
}

void Dptx_Reg_Direct_Write( volatile void __iomem *Reg, u32 uiData )
{
	__raw_writel( uiData, Reg );
}

void Dptx_Reg_Set_AudioSel( u32 uiData )
{
	u32			uiWriteData;
	
	uiWriteData = uiData & DPTX_AUDIO_SEL_MASK;

	dptx_dbg("WRITE: addr=0x%05x data=0x%08x", (u32)(DPTX_AUDIO_SEL_REG), uiWriteData);

	writel( uiWriteData , DPTX_AUDIO_SEL_REG );
}

u32 Dptx_Reg_Get_AudioSel(void)
{
	u32			uiReadData;
	
	uiReadData = (readl( DPTX_AUDIO_SEL_REG )) & DPTX_AUDIO_SEL_MASK;

	dptx_dbg("Read: addr=0x%05x data=0x%08x", (u32)(DPTX_AUDIO_SEL_REG), uiReadData);

	return ( uiReadData );
}


