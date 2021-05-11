// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <soc/tcc/chipinfo.h>

#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_dbg.h"

#define MAX_TRY_PLL_LOCK				10

static struct Dptx_Params *pstHandle;


int32_t Dptx_Platform_Init_Params( struct Dptx_Params	*pstDptx, struct device	*pstParentDev )
{
	u8		ucStream_Index;

	pstDptx->pstParentDev				= pstParentDev;
	pstDptx->pcDevice_Name				= "Dptx V14";

	pstDptx->bSpreadSpectrum_Clock		= true;

#if defined( CONFIG_DRM_TCC )	
	pstDptx->bUsed_TCC_DRM_Interface	= true;
#endif
	
	pstDptx->eEstablished_Timing		= DMT_NOT_SUPPORTED; 

	pstDptx->uiHDCP22_RegAddr_Offset	= DP_HDCP_OFFSET;
	pstDptx->uiRegBank_RegAddr_Offset	= DP_REGISTER_BANK_OFFSET;
	pstDptx->uiCKC_RegAddr_Offset		= DP_CKC_OFFSET;
	pstDptx->uiProtect_RegAddr_Offset	= DP_PROTECT_OFFSET;

	pstDptx->ucMax_Lanes				= DPTX_MAX_LINK_LANES;

	pstDptx->pvHPD_Intr_CallBack		= NULL;

	for( ucStream_Index = 0; ucStream_Index < PHY_INPUT_STREAM_MAX; ucStream_Index++ )
	{
		pstDptx->aucVCP_Id[ucStream_Index]	= ( ucStream_Index + 1 );

		pstDptx->paucEdidBuf_Entry[ucStream_Index] = kzalloc( DPTX_EDID_BUFLEN, GFP_KERNEL );
		if( pstDptx->paucEdidBuf_Entry[ucStream_Index] == NULL ) 
		{
			dptx_err("failed to alloc EDID memory" );
			return DPTX_RETURN_ENOMEM;
		}

		memset( pstDptx->paucEdidBuf_Entry[ucStream_Index], 0, DPTX_EDID_BUFLEN );
	}
	
	pstDptx->pucEdidBuf = kzalloc( DPTX_EDID_BUFLEN, GFP_KERNEL );
	if( pstDptx->pucEdidBuf == NULL ) 
	{
		dptx_err("failed to alloc EDID memory" );
		return DPTX_RETURN_ENOMEM;
	}

	pstDptx->uiTCC805X_Revision = get_chip_rev();

	if( pstDptx->uiTCC805X_Revision == TCC805X_REVISION_CS )
	{
		Dptx_Platform_Get_PHY_StandardLane_PinConfig( pstDptx );
		Dptx_Platform_Get_SDMBypass_Ctrl( pstDptx );
		Dptx_Platform_Get_SRVCBypass_Ctrl( pstDptx );
		Dptx_Platform_Get_MuxSelect( pstDptx );
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Init( struct Dptx_Params	*pstDptx )
{	
	bool	bPLL_LockStatus;
	int32_t	iRetVal;

	iRetVal = Dptx_Platform_Set_PLL_Divisor( pstDptx );
	if( iRetVal !=  DPTX_RETURN_NO_ERROR )
	{
		dptx_err("from Dptx_Platform_Set_PLL_Divisor()" );
		return iRetVal;
	}

	iRetVal = Dptx_Platform_Get_PLLLock_Status( pstDptx, &bPLL_LockStatus );
	if( iRetVal !=  DPTX_RETURN_NO_ERROR )
	{
		dptx_err("from Dptx_Platform_Get_PLLLock_Status()" );
		return iRetVal;
	}

	iRetVal = Dptx_Platform_Set_PLL_ClockSource( pstDptx, (u8)DPTX_CLKCTRL0_PLL_DIVIDER_OUTPUT );
	if( iRetVal !=  DPTX_RETURN_NO_ERROR )
	{
		dptx_err("from Dptx_Platform_Set_PLL_ClockSource()" );
		return iRetVal;
	}

	iRetVal = Dptx_Platform_Set_RegisterBank( pstDptx, (enum PHY_LINK_RATE)pstDptx->ucMax_Rate );
	if( iRetVal !=  DPTX_RETURN_NO_ERROR )
	{
		dptx_err("from Dptx_Platform_Set_RegisterBank()" );
		return iRetVal;
	}

	pstHandle = pstDptx;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Deinit( struct Dptx_Params	*pstDptx )
{
	Dptx_Platform_Free_Handle( pstDptx );

	pstHandle = NULL;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_ProtectRegister_PW( struct Dptx_Params	*pstDptx, u32 uiProtect_Cfg_PW )
{
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_PW_OK ), (u32)uiProtect_Cfg_PW );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_ProtectRegister_CfgAccess( struct Dptx_Params	*pstDptx, bool bAccessable )
{
	if( bAccessable )
	{
		Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_ACCESS ), (u32)DP_PORTECT_CFG_ACCESSABLE );
	}
	else
	{
		Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_ACCESS ), (u32)DP_PORTECT_CFG_NOT_ACCESSABLE );
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_ProtectRegister_CfgLock( struct Dptx_Params	*pstDptx, bool bAccessable )
{
	if( bAccessable )
	{
		Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_LOCK ), (u32)DP_PORTECT_CFG_UNLOCKED );
	}
	else
	{
		Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_LOCK ), (u32)DP_PORTECT_CFG_LOCKED );
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_PLL_Divisor(	struct Dptx_Params	*pstDptx )
{
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_PLLCON ), 0x00000FC0 );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_PLLMON ), 0x00008800 );

	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKDIVC0 ), DIV_CFG_CLK_200HMZ );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKDIVC1 ), DIV_CFG_CLK_160HMZ );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKDIVC2 ), DIV_CFG_CLK_100HMZ );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKDIVC3 ), DIV_CFG_CLK_40HMZ );

	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_PLLPMS ), 0x05026403 );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_PLLPMS ), 0x85026403 );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_PLL_ClockSource( struct Dptx_Params	*pstDptx, u8 ucClockSource )
{
	u32		uiRegMap_Val = 0;

	uiRegMap_Val |= ( ucClockSource );

	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKCTRL0 ), uiRegMap_Val );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKCTRL1 ), uiRegMap_Val );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKCTRL2 ), uiRegMap_Val );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKCTRL3 ), uiRegMap_Val );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Get_PLLLock_Status( struct Dptx_Params	*pstDptx, bool *pbPll_Locked )
{
	bool	bPllLock;
	u8		ucCount = 0;
	u32		uiRegMap_PllPMS;

	do{
		uiRegMap_PllPMS = Dptx_Reg_Readl( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_PLLPMS ) );

		bPllLock = ( uiRegMap_PllPMS & DPTX_PLLPMS_LOCK_MASK ) ? true : false;
		if( !bPllLock )
		{
			mdelay( 1 );
		}
	}while( !bPllLock && ( ucCount++ < MAX_TRY_PLL_LOCK ));

	if( bPllLock )
	{
		dptx_dbg("Success to get PLL Locking after %dms", (u32)(( ucCount + 1 ) * 1 ));
	}
	else	
	{
		dptx_err("Fail to get PLL Locking ");
	}

	*pbPll_Locked = bPllLock;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_RegisterBank(	struct Dptx_Params	*pstDptx, enum PHY_LINK_RATE eLinkRate )
{
	u32		uiReg_R_data, uiReg_W_data;
	/*
	 *  Register bank settings can be changed by application
	 */

	uiReg_R_data = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ));
	if( uiReg_R_data & AXI_SLAVE_BRIDGE_RST_MASK )
	{
		uiReg_W_data = (u32)0x00000008;
	}
	else
	{
		uiReg_W_data = (u32)0x00000000;
	}
	dptx_dbg("Writing DP_REGISTER_BANK_REG_22[0x12480058] : 0x%x -> 0x%x", uiReg_R_data, uiReg_W_data);

	switch( eLinkRate )
	{
		case LINK_RATE_RBR:
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_0 ), (u32)0x004D0000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_20 ), (u32)0x00002501 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_11 ), (u32)0x00000401 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_12 ), (u32)0x00000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_13 ), (u32)0x00000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_14 ), (u32)0x00000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_16 ), (u32)0x00000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_15 ), (u32)0x08080808 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_17 ), (u32)0x10000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_20 ), (u32)0x00002501 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_21 ), (u32)0x00000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_23 ), (u32)0x00000008 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)0x00000801 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)uiReg_W_data );
			break;
		case LINK_RATE_HBR:
		case LINK_RATE_HBR2:
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_0 ),  (u32)0x002D0000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_21 ), (u32)0x00000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_20 ), (u32)0x00002501 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_1 ),  (u32)0xA00F0001 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_2 ),  (u32)0x00A80122 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_3 ),  (u32)0x001E8A00 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_4 ),  (u32)0x00000004 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_7 ),  (u32)0x00000C0C );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_8 ),  (u32)0x05460546 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_9 ),  (u32)0x00000011 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x00700000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_11 ), (u32)0x00000401 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_13 ), (u32)0xA8000122 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_14 ), (u32)0x418A001E );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_16 ), (u32)0x00004000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_15 ), (u32)0x0B000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_17 ), (u32)0x10000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_20 ), (u32)0x00002501 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x08700000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_12 ), (u32)0xA00F0003 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_23 ), (u32)0x00000008 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)0x00000801 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)uiReg_W_data );
			break;
		case LINK_RATE_HBR3:
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_0 ),  (u32)0x002D0000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_21 ), (u32)0x00000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_20 ), (u32)0x00002501 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_1 ),  (u32)0xA00F0001 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_2 ),  (u32)0x00A80122 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_3 ),  (u32)0x001E8A00 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_4 ),  (u32)0x00000004 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_7 ),  (u32)0x00000C0C );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_8 ),  (u32)0x05460546 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_9 ),  (u32)0x00000011 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x00700000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_11 ), (u32)0x00000401 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_13 ), (u32)0xA8000616 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_14 ), (u32)0x115C0045 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_16 ), (u32)0x00004000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_15 ), (u32)0x8B000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_17 ), (u32)0x10000000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_20 ), (u32)0x00002501 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x08700000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_12 ), (u32)0xA00F0003 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_23 ), (u32)0x00000008 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)0x00000801 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)uiReg_W_data );
			break;
		default:
			dptx_err("Invalid PHY rate %d\n", eLinkRate);
			return DPTX_RETURN_EINVAL;
			break;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_APAccess_Mode( struct Dptx_Params *pstDptx )
{
	u32			uiRegMap_R_ApbSel, uiRegMap_W_ApbSel;

	uiRegMap_R_ApbSel = Dptx_Reg_Direct_Read(    pstDptx->pioMIC_SubSystem_BaseAddr + DPTX_APB_SEL_REG );
	uiRegMap_W_ApbSel = ( uiRegMap_R_ApbSel | BIT( DPTX_APB_SEL_MASK ));
	Dptx_Reg_Direct_Write(   ( pstDptx->pioMIC_SubSystem_BaseAddr + DPTX_APB_SEL_REG ), uiRegMap_W_ApbSel );

	dptx_dbg("Register access...Reg[0x%x]:0x%08x -> 0x%08x",( pstDptx->pioMIC_SubSystem_BaseAddr + DPTX_APB_SEL_REG ), uiRegMap_R_ApbSel, uiRegMap_W_ApbSel );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_PMU_ColdReset_Release( struct Dptx_Params *pstDptx )
{
	u32		uiRegMap_R_HsmRst_Msk, uiRegMap_W_HsmRst_Msk;

	uiRegMap_R_HsmRst_Msk = Dptx_Reg_Direct_Read( pstDptx->pioPMU_BaseAddr + PMU_HSM_RSTN_MASK );

	if( uiRegMap_R_HsmRst_Msk & PMU_COLD_RESET_MASK )
	{
		uiRegMap_W_HsmRst_Msk = ( uiRegMap_R_HsmRst_Msk & ~PMU_COLD_RESET_MASK ); // DP Cold reset mask release
		Dptx_Reg_Direct_Write(   ( pstDptx->pioPMU_BaseAddr + PMU_HSM_RSTN_MASK ), uiRegMap_W_HsmRst_Msk );

		dptx_dbg("DP Cold reset mask release...0x%08x -> 0x%08x", uiRegMap_R_HsmRst_Msk, uiRegMap_W_HsmRst_Msk);
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_PHY_StandardLane_PinConfig( struct Dptx_Params *pstDptx )
{
	u32		uiRegMap_R_Reg24, uiRegMap_W_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ));

	uiRegMap_W_Reg24 = ( pstDptx->bPHY_Lane_Reswap ) ? ( uiRegMap_R_Reg24 | STD_EN_MASK ):( uiRegMap_R_Reg24 & ~STD_EN_MASK );

	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ), uiRegMap_W_Reg24);

	dptx_dbg("Set PHY lane swap %s...Reg[0x%x]: 0x%08x -> 0x%08x ",
					( pstDptx->bPHY_Lane_Reswap ) ? "On":"Off",
					(u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ), uiRegMap_R_Reg24, uiRegMap_W_Reg24 );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Get_PHY_StandardLane_PinConfig( struct Dptx_Params *pstDptx )
{
	u32		uiRegMap_R_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ));

	pstDptx->bPHY_Lane_Reswap = ( uiRegMap_R_Reg24 & STD_EN_MASK ) ? true:false;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_SDMBypass_Ctrl( struct Dptx_Params *pstDptx )
{
	u32		uiRegMap_R_Reg24, uiRegMap_W_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ));

	uiRegMap_W_Reg24 = ( pstDptx->bSDM_Bypass ) ? ( uiRegMap_R_Reg24 | SDM_BYPASS_MASK ):( uiRegMap_R_Reg24 & ~SDM_BYPASS_MASK );

	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ), uiRegMap_W_Reg24);

	dptx_dbg("Set SDM Bypass %s...Reg[0x%x]: 0x%08x -> 0x%08x ",
					( pstDptx->bSDM_Bypass ) ? "On":"Off",
					(u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ), uiRegMap_R_Reg24, uiRegMap_W_Reg24 );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Get_SDMBypass_Ctrl( struct Dptx_Params	*pstDptx )
{
	u32			uiRegMap_R_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ));

	pstDptx->bSDM_Bypass = ( uiRegMap_R_Reg24 & SDM_BYPASS_MASK ) ? true:false;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_SRVCBypass_Ctrl( struct Dptx_Params *pstDptx )
{
	u32		uiRegMap_R_Reg24, uiRegMap_W_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ));

	uiRegMap_W_Reg24 = ( pstDptx->bSRVC_Bypass ) ? ( uiRegMap_R_Reg24 | SRVC_BYPASS_MASK ):( uiRegMap_R_Reg24 & ~SRVC_BYPASS_MASK );

	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ), uiRegMap_W_Reg24);

	dptx_dbg("Set SRVC Bypass %s...Reg[0x%x]: 0x%08x -> 0x%08x ",
					( pstDptx->bSRVC_Bypass ) ? "On":"Off",
					(u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ), uiRegMap_R_Reg24, uiRegMap_W_Reg24 );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Get_SRVCBypass_Ctrl( struct Dptx_Params	*pstDptx )
{
	u32			uiRegMap_R_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ));

	pstDptx->bSRVC_Bypass = ( uiRegMap_R_Reg24 & SRVC_BYPASS_MASK ) ? true:false;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_MuxSelect( struct Dptx_Params	*pstDptx )
{
	u8		ucRegMap_MuxSel_Shift = 0;
	u8		ucDP_Index;
	u32		uiRegMap_R_Reg24, uiRegMap_W_Reg24;
	u32		uiRegMap_MuxSel_Mask;

	if( pstDptx->ucNumOfPorts >= PHY_INPUT_STREAM_MAX )
	{
		dptx_err("The number of ports( %d ) is larger than maximum(%d)", pstDptx->ucNumOfPorts, PHY_INPUT_STREAM_MAX );
		return DPTX_RETURN_ENODEV;
	}

	for( ucDP_Index = 0; ucDP_Index < pstDptx->ucNumOfPorts; ucDP_Index++ )
	{
		switch( ucDP_Index )
		{
			case PHY_INPUT_STREAM_0:
				uiRegMap_MuxSel_Mask = (u32)SOURCE0_MUX_SEL_MASK;
				ucRegMap_MuxSel_Shift = (u8)SOURCE0_MUX_SEL_SHIFT;
				break;
			case PHY_INPUT_STREAM_1:
				uiRegMap_MuxSel_Mask = (u32)SOURCE1_MUX_SEL_MASK;
				ucRegMap_MuxSel_Shift = (u8)SOURCE1_MUX_SEL_SHIFT;
				break;
			case PHY_INPUT_STREAM_2:
				uiRegMap_MuxSel_Mask = (u32)SOURCE2_MUX_SEL_MASK;
				ucRegMap_MuxSel_Shift = (u8)SOURCE2_MUX_SEL_SHIFT;
				break;
			case PHY_INPUT_STREAM_3:
			default:
				uiRegMap_MuxSel_Mask = (u32)SOURCE3_MUX_SEL_MASK;
				ucRegMap_MuxSel_Shift = (u8)SOURCE3_MUX_SEL_SHIFT;
				break;
		}

		uiRegMap_R_Reg24 = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ));

		uiRegMap_W_Reg24 = ( uiRegMap_R_Reg24 & ~uiRegMap_MuxSel_Mask );
		uiRegMap_W_Reg24 |= ( pstDptx->aucMuxInput_Index[ucDP_Index] << ucRegMap_MuxSel_Shift );

		Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ), uiRegMap_W_Reg24);

		dptx_dbg("Set Mux select[0x%x]( 0x%x -> 0x%x ): Mux %d -> DP %d ",
			(u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ), uiRegMap_R_Reg24, uiRegMap_W_Reg24, pstDptx->aucMuxInput_Index[ucDP_Index], ucDP_Index );
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Get_MuxSelect( struct Dptx_Params	*pstDptx )
{
	u8		ucRegMap_MuxSel_Shift = 0;
	u8		ucDP_Index;
	u32		uiRegMap_R_Reg24;
	u32		uiRegMap_MuxSel_Mask;

	for( ucDP_Index = 0; ucDP_Index < PHY_INPUT_STREAM_MAX; ucDP_Index++ )
	{
		switch( ucDP_Index )
		{
			case PHY_INPUT_STREAM_0:
				uiRegMap_MuxSel_Mask = (u32)SOURCE0_MUX_SEL_MASK;
				ucRegMap_MuxSel_Shift = (u8)SOURCE0_MUX_SEL_SHIFT;
				break;
			case PHY_INPUT_STREAM_1:
				uiRegMap_MuxSel_Mask = (u32)SOURCE1_MUX_SEL_MASK;
				ucRegMap_MuxSel_Shift = (u8)SOURCE1_MUX_SEL_SHIFT;
				break;
			case PHY_INPUT_STREAM_2:
				uiRegMap_MuxSel_Mask = (u32)SOURCE2_MUX_SEL_MASK;
				ucRegMap_MuxSel_Shift = (u8)SOURCE2_MUX_SEL_SHIFT;
				break;
			case PHY_INPUT_STREAM_3:
			default:
				uiRegMap_MuxSel_Mask = (u32)SOURCE3_MUX_SEL_MASK;
				ucRegMap_MuxSel_Shift = (u8)SOURCE3_MUX_SEL_SHIFT;
				break;
		}

		uiRegMap_R_Reg24 = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ));

		pstDptx->aucMuxInput_Index[ucDP_Index] = (( uiRegMap_R_Reg24 & uiRegMap_MuxSel_Mask ) >> ucRegMap_MuxSel_Shift );

		dptx_notice("Get Mux select[0x%x](0x%x): Mux %d <-> DP %d ", 
					(u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_24 ), uiRegMap_R_Reg24, pstDptx->aucMuxInput_Index[ucDP_Index], ucDP_Index );
	}

	return DPTX_RETURN_NO_ERROR;
}


int32_t Dptx_Platform_Set_ClkPath_To_XIN( struct Dptx_Params *pstDptx )
{
	u32			uiRegMap_PLLPMS;

	Dptx_Platform_Set_ProtectRegister_PW( pstDptx, (u32)DP_PORTECT_CFG_PW_VAL );
	Dptx_Platform_Set_ProtectRegister_CfgLock( pstDptx, true );
	Dptx_Platform_Set_ProtectRegister_CfgAccess( pstDptx, true );

	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiCKC_RegAddr_Offset + DP_REGISTER_BANK_REG_0 ), (u32)0x00 );
	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiCKC_RegAddr_Offset + DP_REGISTER_BANK_REG_1 ), (u32)0x00 );
	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiCKC_RegAddr_Offset + DP_REGISTER_BANK_REG_2 ), (u32)0x00 );
	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiCKC_RegAddr_Offset + DP_REGISTER_BANK_REG_3 ), (u32)0x00 );

	uiRegMap_PLLPMS = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiCKC_RegAddr_Offset + DP_REGISTER_BANK_REG_4 ) );
	uiRegMap_PLLPMS = ( uiRegMap_PLLPMS | ENABLE_BYPASS_MODE_MASK);
	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiCKC_RegAddr_Offset + DP_REGISTER_BANK_REG_4 ), (u32)uiRegMap_PLLPMS );

	uiRegMap_PLLPMS = Dptx_Reg_Readl( pstDptx, (u32)( pstDptx->uiCKC_RegAddr_Offset + DP_REGISTER_BANK_REG_4 ) );
	uiRegMap_PLLPMS = ( uiRegMap_PLLPMS & ~RESET_PLL_MASK);
	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiCKC_RegAddr_Offset + DP_REGISTER_BANK_REG_4 ), (u32)uiRegMap_PLLPMS );

	dptx_dbg("Clk path to XIN...Reg[0x%x] -> 0x%08x", (u32)( pstDptx->uiCKC_RegAddr_Offset + DP_REGISTER_BANK_REG_4 ), uiRegMap_PLLPMS);

	return DPTX_RETURN_NO_ERROR;
}


int32_t Dptx_Platform_Free_Handle( struct Dptx_Params	*pstDptx_Handle )
{
	u8		ucStream_Index;

	if( pstDptx_Handle->pucEdidBuf != NULL ) 
	{
		kfree( pstDptx_Handle->pucEdidBuf );
		pstDptx_Handle->pucEdidBuf = NULL;
	}

	for( ucStream_Index = 0; ucStream_Index < PHY_INPUT_STREAM_MAX; ucStream_Index++ )
	{
		pstDptx_Handle->paucEdidBuf_Entry[ucStream_Index] = kzalloc( DPTX_EDID_BUFLEN, GFP_KERNEL );
		if( pstDptx_Handle->paucEdidBuf_Entry[ucStream_Index] != NULL ) 
		{
			kfree( pstDptx_Handle->paucEdidBuf_Entry[ucStream_Index] );
		}
	}

	if( pstDptx_Handle != NULL ) 
	{
		kfree( pstDptx_Handle );
	}

	return DPTX_RETURN_NO_ERROR;
}

void Dptx_Platform_Set_Device_Handle( struct Dptx_Params	*pstDptx )
{
	pstHandle = pstDptx;
}

struct Dptx_Params *Dptx_Platform_Get_Device_Handle( void )
{
	return ( pstHandle );
}

