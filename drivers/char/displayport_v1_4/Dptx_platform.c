// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_dbg.h"


//#define ENABLE_CTS_TEST
//#define CTS_HBR2_TEST		/* HBR2 -> vboost 3 */
//#define CTS_HBR3_TEST		/* HBR3 -> vboost 3 */

#define MAX_TRY_PLL_LOCK				10

static struct Dptx_Params *pstHandle;


bool Dptx_Platform_Init_Params( struct Dptx_Params	*pstDptx, struct device	*pstParentDev )
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

	pstDptx->pvHPD_Intr_CallBack		= NULL;

	for( ucStream_Index = 0; ucStream_Index < PHY_INPUT_STREAM_MAX; ucStream_Index++ )
	{
		pstDptx->aucVCP_Id[ucStream_Index]	= ( ucStream_Index + 1 );

#if defined( CONFIG_DP_INPUT_PORT )
		pstDptx->aucDP_InputPort[ucStream_Index] = ucStream_Index;
#endif
	
		pstDptx->paucEdidBuf_Entry[ucStream_Index] = kzalloc( DPTX_EDID_BUFLEN, GFP_KERNEL );
		if( pstDptx->paucEdidBuf_Entry[ucStream_Index] == NULL ) 
        	{
		      dptx_err("failed to alloc EDID memory" );
		      return ( DPTX_RETURN_FAIL );
         	}

		memset( pstDptx->paucEdidBuf_Entry[ucStream_Index], 0, DPTX_EDID_BUFLEN );
	}
	
	pstDptx->pucEdidBuf = kzalloc( DPTX_EDID_BUFLEN, GFP_KERNEL );
	if( pstDptx->pucEdidBuf == NULL ) 
	{
		dptx_err("failed to alloc EDID memory" );
		return ( DPTX_RETURN_FAIL );
	}

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_Init( struct Dptx_Params	*pstDptx )
{	
	bool				bRetVal, bPLL_LockStatus;

	bRetVal = Dptx_Platform_Set_PLL_Divisor( pstDptx, DIV_CFG_CLK_200HMZ, DIV_CFG_CLK_160HMZ, DIV_CFG_CLK_100HMZ, DIV_CFG_CLK_40HMZ );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		dptx_err("from Dptx_Platform_Set_PLL_Divisor()" );
		return (DPTX_RETURN_FAIL);
	}

	bRetVal = Dptx_Platform_Get_PLLLock_Status( pstDptx, &bPLL_LockStatus );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		dptx_err("from Dptx_Platform_Get_PLLLock_Status()" );
		return (DPTX_RETURN_FAIL);
	}

	bRetVal = Dptx_Platform_Set_PLL_ClockSource( pstDptx, (u8)DPTX_CLKCTRL0_PLL_DIVIDER_OUTPUT );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		dptx_err("from Dptx_Platform_Set_PLL_ClockSource()" );
		return (DPTX_RETURN_FAIL);
	}

	bRetVal = Dptx_Platform_Set_RegisterBank( pstDptx, (enum PHY_LINK_RATE)pstDptx->ucMax_Rate );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		dptx_err("from Dptx_Platform_Set_RegisterBank()" );
		return (DPTX_RETURN_FAIL);
	}

	pstHandle = pstDptx;

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_Deinit( struct Dptx_Params	*pstDptx )
{
	Dptx_Platform_Free_Handle( pstDptx );

	pstHandle = NULL;
	
	return (DPTX_RETURN_SUCCESS);
}

bool Dptx_Platform_Set_ProtectRegister_PW( struct Dptx_Params	*pstDptx, u32 uiProtect_Cfg_PW )
{
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_PW_OK ), (u32)uiProtect_Cfg_PW );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_Set_ProtectRegister_CfgAccess( struct Dptx_Params	*pstDptx, bool bAccessable )
{
	if( bAccessable )
	{
		Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_ACCESS ), (u32)DP_PORTECT_CFG_ACCESSABLE );
	}
	else
	{
		Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_ACCESS ), (u32)DP_PORTECT_CFG_NOT_ACCESSABLE );
	}

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_Set_ProtectRegister_CfgLock( struct Dptx_Params	*pstDptx, bool bAccessable )
{
	if( bAccessable )
	{
		Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_LOCK ), (u32)DP_PORTECT_CFG_UNLOCKED );
	}
	else
	{
		Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_LOCK ), (u32)DP_PORTECT_CFG_LOCKED );
	}

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_Set_PLL_Divisor(	struct Dptx_Params	*pstDptx, u32 uiBLK0_Divisor, u32 uiBLK1_Divisor, u32 uiBLK2_Divisor, u32 uiBLK3_Divisor )
{
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_PLLCON ), 0x00000FC0 );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_PLLMON ), 0x00008800 );
	
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKDIVC0 ), uiBLK0_Divisor );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKDIVC1 ), uiBLK1_Divisor );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKDIVC2 ), uiBLK2_Divisor );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_CLKDIVC3 ), uiBLK3_Divisor );
	
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_PLLPMS ), 0x05026403 );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CKC_CFG_PLLPMS ), 0x85026403 );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_Set_PLL_ClockSource( struct Dptx_Params	*pstDptx, u8 ucClockSource )
{
	u32		uiRegMap_Val = 0;

	uiRegMap_Val |= ( ucClockSource );
	
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CLKCTRL0 ), uiRegMap_Val );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CLKCTRL1 ), uiRegMap_Val );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CLKCTRL2 ), uiRegMap_Val );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiCKC_RegAddr_Offset + DPTX_CLKCTRL3 ), uiRegMap_Val );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_Get_PLLLock_Status( struct Dptx_Params	*pstDptx, bool *pbPll_Locked )
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
	
	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_Set_RegisterBank(	struct Dptx_Params	*pstDptx, enum PHY_LINK_RATE eLinkRate )
{
	/*
	 *  Register bank settings can be changed by application
	 */

	switch( eLinkRate )
	{
		case LINK_RATE_RBR:
	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_0 ), (u32)0x004D0000 );	
	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_20 ), (u32)0x00002501 );	
			// SoC guild for HDCP
			//Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_11 ), (u32)0x00000001 );	
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
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)0x00000000 );
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
			
			//Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x00300000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x00700000 );
	
	                Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_11 ), (u32)0x00000401 );	
	                Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_13 ), (u32)0xA8000122 );	
	                Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_14 ), (u32)0x418A001E );	

			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_16 ), (u32)0x00004000 );	
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_15 ), (u32)0x0B000000 );
			
	                Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_17 ), (u32)0x10000000 );	
	                Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_20 ), (u32)0x00002501 );

			//Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x08300000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x08700000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_12 ), (u32)0xA00F0003 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_23 ), (u32)0x00000008 );	
	                Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)0x00000801 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)0x00000000 );
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
			
			//Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x00300000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x00700000 );
                  	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_11 ), (u32)0x00000401 );	
                     	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_13 ), (u32)0xA8000616 );	
                	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_14 ), (u32)0x115C0045 );	

			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_16 ), (u32)0x00004000 );	
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_15 ), (u32)0x8B000000 );
                   	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_17 ), (u32)0x10000000 );	
                      	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_20 ), (u32)0x00002501 );
			
			//Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x08300000 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_10 ), (u32)0x08700000 );
                      	Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_12 ), (u32)0xA00F0003 );
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_23 ), (u32)0x00000008 );	
			Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)0x00000801 );
                        Dptx_Reg_Writel( pstDptx, (u32)( pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_22 ), (u32)0x00000000 );	

		        //    mdelay( 30 );
			break;
		default:	
			dptx_err("Invalid PHY rate %d\n", eLinkRate);
			break;
	}

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_Set_APAccess_Mode( struct Dptx_Params *pstDptx )
{
	u32			uiRegMap_R_ApbSel, uiRegMap_W_ApbSel;

	uiRegMap_R_ApbSel = Dptx_Reg_Direct_Read(    pstDptx->pioMIC_SubSystem_BaseAddr + DPTX_APB_SEL_REG );
	uiRegMap_W_ApbSel = ( uiRegMap_R_ApbSel | BIT( DPTX_APB_SEL_MASK ));
	Dptx_Reg_Direct_Write(   ( pstDptx->pioMIC_SubSystem_BaseAddr + DPTX_APB_SEL_REG ), uiRegMap_W_ApbSel );

	dptx_dbg("Register access...Reg[0x%x]:0x%08x -> 0x%08x",( pstDptx->pioMIC_SubSystem_BaseAddr + DPTX_APB_SEL_REG ), uiRegMap_R_ApbSel, uiRegMap_W_ApbSel );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_Set_PMU_ColdReset_Release(	struct Dptx_Params	*pstDptx )
{
	u32		uiRegMap_R_HsmRst_Msk, uiRegMap_W_HsmRst_Msk;

	uiRegMap_R_HsmRst_Msk = Dptx_Reg_Direct_Read( pstDptx->pioPMU_BaseAddr + PMU_HSM_RSTN_MASK );

	if( uiRegMap_R_HsmRst_Msk & PMU_COLD_RESET_MASK )
	{
		uiRegMap_W_HsmRst_Msk = ( uiRegMap_R_HsmRst_Msk & ~PMU_COLD_RESET_MASK ); // DP Cold reset mask release
		Dptx_Reg_Direct_Write(   ( pstDptx->pioPMU_BaseAddr + PMU_HSM_RSTN_MASK ), uiRegMap_W_HsmRst_Msk );

		dptx_dbg("DP Cold reset mask release...0x%08x -> 0x%08x", uiRegMap_R_HsmRst_Msk, uiRegMap_W_HsmRst_Msk);
	}
	
	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Platform_ClkPath_To_XIN(	struct Dptx_Params	*pstDptx )
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
	
	return ( DPTX_RETURN_SUCCESS );
}


bool Dptx_Platform_Free_Handle( struct Dptx_Params	*pstDptx_Handle )
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

	return ( DPTX_RETURN_SUCCESS );
}

void Dptx_Platform_Set_Device_Handle( struct Dptx_Params	*pstDptx )
{
	pstHandle = pstDptx;
}

struct Dptx_Params *Dptx_Platform_Get_Device_Handle( void )
{
	return ( pstHandle );
}

bool Dptx_Platform_DirectWrite_Reg( struct Dptx_Params	*pstDptx )
{
//	u32 	uiRead_RegVal;
//	enum  HPD_Detection_Status	eHPD_Status;

 	dptx_dbg("Starting Dptx_Platform_DirectWrite_Reg()... ");

	Dptx_Reg_Writel( pstDptx, 0x400, 0x12001302 );
	Dptx_Reg_Writel( pstDptx, 0x300, 0x00010000 );
	Dptx_Reg_Writel( pstDptx, 0x324, 0x001400C0 );
	Dptx_Reg_Writel( pstDptx, 0x500, 0xC0000000 );
	Dptx_Reg_Writel( pstDptx, 0x504, 0x00000000 );
	Dptx_Reg_Writel( pstDptx, 0x30C, 0x00000003 );
	Dptx_Reg_Writel( pstDptx, 0x328, 0x20000000 );
	Dptx_Reg_Writel( pstDptx, 0x310, 0x07800460 );
	Dptx_Reg_Writel( pstDptx, 0x32C, 0x00008000 );
	Dptx_Reg_Writel( pstDptx, 0x314, 0x002D0438 );
	Dptx_Reg_Writel( pstDptx, 0x318, 0x002C0058 );
	Dptx_Reg_Writel( pstDptx, 0x31C, 0x00050004 );
	Dptx_Reg_Writel( pstDptx, 0x330, 0x0000017E );
	Dptx_Reg_Writel( pstDptx, 0x320, 0x000207a3 );
	mdelay( 1000 );	

	writel( 0x004C0141,  (void *) 0x12000000 );
	Dptx_Reg_Writel( pstDptx, 0x300, 0x00010020 );

	dptx_dbg("Done Dptx_Platform_DirectWrite_Reg()... ");

	return (DPTX_RETURN_SUCCESS);
}



