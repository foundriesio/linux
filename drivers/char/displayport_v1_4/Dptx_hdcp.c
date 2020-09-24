// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/delay.h>
 
#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"
#include "Dptx_drm_dp_addition.h"

#include "Dptx_keys.h"
//#include "Dptx_fpga_reg.h"

#define DPTX_HDCP_MAX_AUTH_RETRY			10

#define DPTX_HDCP_RETURN_SUCCESS			false
#define DPTX_HDCP_RETURN_FAIL				true

static void dptx_config_encrypt( struct Dptx_Params *pstDptx, u8 ucActive_Decryption )
{
	u32			uiRegMap_RmlCtl;

	uiRegMap_RmlCtl = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_REG_RMLCTL );
	uiRegMap_RmlCtl &= ~DPTX_ODPK_DECRYPT_ENABLE_MASK;
	uiRegMap_RmlCtl |= ucActive_Decryption << DPTX_ODPK_DECRYPT_ENABLE_SHIFT;
	
	Dptx_Reg_Writel( pstDptx, DPTX_HDCP_REG_RMLCTL, uiRegMap_RmlCtl );
}

static bool dptx_wait_idpk_work_data( struct Dptx_Params *pstDptx )
{
	u8			ucCount = 0;
	u32			reg;

	dptx_dbg( "Calling... " );

	while( 1 ) 
	{
		reg = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_REG_RMLSTS );
		if( reg & DPTX_IDPK_WR_OK_STS_MASK )	/* DPTX_IDPK_WR_OK_STS_MASK : When it's 1, DPK wirte is allowed */
		{
			break;
		}

		if( ucCount++ > 50 ) 
		{
			dptx_err( "Timed out" );
			return ( DPTX_HDCP_RETURN_FAIL );
		}
		udelay( 1 );
	}
	
	return ( DPTX_HDCP_RETURN_SUCCESS );
}

static bool dptx_wait_idpk_work_index( struct Dptx_Params *pstDptx, u8 ucDpkIndex )
{
	u8			ucCount = 0;
	u32			uiRegMap_RmlStatus;

	dptx_dbg( "Calling... " );

	while( 1 ) 
	{
		uiRegMap_RmlStatus = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_REG_RMLSTS );

		if( ( uiRegMap_RmlStatus & DPTX_IDPK_DATA_INDEX_MASK ) == ucDpkIndex )
		{
			break;
		}

		if( ucCount++ > 50 ) 
		{
			dptx_err( "Timed out" );
			return ( DPTX_HDCP_RETURN_FAIL );
		}
		udelay( 1 );
	}
	
	return ( DPTX_HDCP_RETURN_SUCCESS );
}

static bool dptx_write_dpk_keys( struct Dptx_Params *pstDptx )
{
	bool						bRetVal;
	u8							ucDdkIndex = 0;
	u32							uiRegMap_DpkCRC;
	struct Dptx_Hdcp_Params		*pstHdcpParams		= &pstDptx->stHdcpParams;
	struct Dptx_Hdcp_Aksv		*pstHdcpAksv		= &pstHdcpParams->stHdcpAksv;

	dptx_dbg( "Calling... " );

	dptx_config_encrypt( pstDptx, 0 );
	
	bRetVal = dptx_wait_idpk_work_data( pstDptx );
	if( bRetVal == DPTX_HDCP_RETURN_FAIL ) 
	{
		return ( DPTX_HDCP_RETURN_FAIL );
	}

	bRetVal = dptx_wait_idpk_work_index( pstDptx, 0 );
	if( bRetVal == DPTX_HDCP_RETURN_FAIL ) 
	{
		return ( DPTX_HDCP_RETURN_FAIL );
	}

	dptx_dbg( "pstHdcpAksv.msb = 0x%08x, lsb = 0x%08x", pstHdcpAksv->msb, pstHdcpAksv->lsb );

	Dptx_Reg_Writel( pstDptx, DPTX_HDCP_REG_DPK1, pstHdcpAksv->msb );
	Dptx_Reg_Writel( pstDptx, DPTX_HDCP_REG_DPK0, pstHdcpAksv->lsb );

	bRetVal = dptx_wait_idpk_work_data( pstDptx );
	if( bRetVal == DPTX_HDCP_RETURN_FAIL ) 
	{
		return ( DPTX_HDCP_RETURN_FAIL );
	}

	dptx_config_encrypt( pstDptx, 1 );
	
	dptx_dbg( "enc_key = 0x%08x", pstHdcpParams->uiHdcp_EncKey );
	
	Dptx_Reg_Writel( pstDptx, DPTX_HDCP_REG_SEED, pstHdcpParams->uiHdcp_EncKey );

	while( ucDdkIndex < 40 ) 
	{
		bRetVal = dptx_wait_idpk_work_index( pstDptx, ucDdkIndex + 1 );
		if( bRetVal == DPTX_HDCP_RETURN_FAIL ) 
		{
			return ( DPTX_HDCP_RETURN_FAIL );
		}

		dptx_dbg( "DPK index = %d", ucDdkIndex );
		dptx_dbg( "dpk.msb = 0x%08x", pstHdcpParams->astHdcpDpk[ucDdkIndex].msb );
		dptx_dbg( "dpk.lsb = 0x%08x", pstHdcpParams->astHdcpDpk[ucDdkIndex].lsb );
		dptx_dbg( "\n");

		Dptx_Reg_Writel( pstDptx, DPTX_HDCP_REG_DPK1,    pstHdcpParams->astHdcpDpk[ucDdkIndex].msb );
		Dptx_Reg_Writel( pstDptx, DPTX_HDCP_REG_DPK0,    pstHdcpParams->astHdcpDpk[ucDdkIndex].lsb );

		bRetVal = dptx_wait_idpk_work_data( pstDptx );
		if( bRetVal == DPTX_HDCP_RETURN_FAIL ) 
		{
			return ( DPTX_HDCP_RETURN_FAIL );
		}
		
		ucDdkIndex++;
	}

	uiRegMap_DpkCRC = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_REG_DPK_CRC );

	dptx_dbg( "CRC == 0x%08x", uiRegMap_DpkCRC );

	return ( DPTX_HDCP_RETURN_SUCCESS );
}

static bool dptx_init_hdcp_keys( struct Dptx_Params *pstDptx )
{
	u32							uiRegMap_DPK, uiKey_Number, uiDdk_count = 0;
	struct Dptx_Hdcp_Dpk		stHdcp_Dpk;
	struct Dptx_Hdcp_Params		*pstHdcpParams = &pstDptx->stHdcpParams;
	
	for( uiKey_Number = 0; uiKey_Number < 280; uiKey_Number = ( uiKey_Number + 7 ) )
	{
		uiRegMap_DPK = 0;
		uiRegMap_DPK |= dpk_keys[uiKey_Number] << 16;
		uiRegMap_DPK |= dpk_keys[uiKey_Number + 1] << 8;
		uiRegMap_DPK |= dpk_keys[uiKey_Number + 2] << 0;
		
		stHdcp_Dpk.msb = uiRegMap_DPK;
		uiRegMap_DPK = 0;
		uiRegMap_DPK |= dpk_keys[uiKey_Number + 3] << 24;
		uiRegMap_DPK |= dpk_keys[uiKey_Number + 4] << 16;
		uiRegMap_DPK |= dpk_keys[uiKey_Number + 5] << 8;
		uiRegMap_DPK |= dpk_keys[uiKey_Number + 6] << 0;
		stHdcp_Dpk.lsb = uiRegMap_DPK;

		pstHdcpParams->astHdcpDpk[uiDdk_count] = stHdcp_Dpk;
		
		uiDdk_count++;
	}

	uiRegMap_DPK = 0;
	uiRegMap_DPK |= dpk_aksv[0] << 16;
	uiRegMap_DPK |= dpk_aksv[1] << 8;
	uiRegMap_DPK |= dpk_aksv[2] << 0;
	
	pstHdcpParams->stHdcpAksv.msb = uiRegMap_DPK;

	uiRegMap_DPK = 0;
	uiRegMap_DPK |= dpk_aksv[3] << 24;
	uiRegMap_DPK |= dpk_aksv[4] << 16;
	uiRegMap_DPK |= dpk_aksv[5] << 8;
	uiRegMap_DPK |= dpk_aksv[6] << 0;
	
	pstHdcpParams->stHdcpAksv.lsb = uiRegMap_DPK;

	uiRegMap_DPK = 0;
	uiRegMap_DPK |= sw_enc_key[1] << 0;
	uiRegMap_DPK |= sw_enc_key[0] << 8;
	
	pstHdcpParams->uiHdcp_EncKey = uiRegMap_DPK;

	return ( DPTX_HDCP_RETURN_SUCCESS );
}

static bool dptx_write_hdcp22_test_keys( struct Dptx_Params *pstDptx )
{
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiHDCP22_RegAddr_Offset | ( DPTX_HDCP22_ELP_PKF_0 & 0xFFFF ) ), hdcp22_test_pkf[0] );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiHDCP22_RegAddr_Offset | ( DPTX_HDCP22_ELP_PKF_1 & 0xFFFF ) ), hdcp22_test_pkf[1] );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiHDCP22_RegAddr_Offset | ( DPTX_HDCP22_ELP_PKF_2 & 0xFFFF ) ), hdcp22_test_pkf[2] );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiHDCP22_RegAddr_Offset | ( DPTX_HDCP22_ELP_PKF_3 & 0xFFFF ) ), hdcp22_test_pkf[3] );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiHDCP22_RegAddr_Offset | ( DPTX_HDCP22_ELP_DUK_0 & 0xFFFF ) ), hdcp22_test_duk[0] );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiHDCP22_RegAddr_Offset | ( DPTX_HDCP22_ELP_DUK_1 & 0xFFFF ) ), hdcp22_test_duk[1] );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiHDCP22_RegAddr_Offset | ( DPTX_HDCP22_ELP_DUK_2 & 0xFFFF ) ), hdcp22_test_duk[2] );
	Dptx_Reg_Writel( pstDptx, ( pstDptx->uiHDCP22_RegAddr_Offset | ( DPTX_HDCP22_ELP_DUK_3 & 0xFFFF ) ), hdcp22_test_duk[3] );

	return ( DPTX_HDCP_RETURN_SUCCESS );
}



bool Dptx_Hdcp_Init( struct Dptx_Params *pstDptx )
{
	bool						bRetVal;
	struct Dptx_Hdcp_Params 	*pstHdcpParams = &pstDptx->stHdcpParams;

	//dptx_dbg( "Calling... " );

	pstHdcpParams->ucAuthen_fail_count	= 0;
	pstHdcpParams->ucHDCP13_Enable		= 0;
	pstHdcpParams->ucHDCP22_Enable		= 0;
	
	bRetVal = dptx_init_hdcp_keys( pstDptx );
	if( bRetVal == DPTX_HDCP_RETURN_FAIL )
	{
		return ( DPTX_HDCP_RETURN_FAIL );	
	}

	bRetVal = dptx_write_hdcp22_test_keys( pstDptx );
	if( bRetVal == DPTX_HDCP_RETURN_FAIL )
	{
		return ( DPTX_HDCP_RETURN_FAIL );	
	}

	return ( DPTX_HDCP_RETURN_SUCCESS );	
}

bool Dptx_Hdcp_Init_Authenticate( struct Dptx_Params *pstDptx )
{
	bool						bHDCP22_Sink = true;
	u32							uiRegMap_HdcpIntr_Status;
	u32							uiRegmap_Hdcp_Observation;
	struct Dptx_Hdcp_Params		*pstHdcpParams = &pstDptx->stHdcpParams;


	uiRegMap_HdcpIntr_Status = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_INT_STS );

	dptx_dbg( " >>>> HDCP_INT_STS = 0x%08x ", uiRegMap_HdcpIntr_Status );

	if( uiRegMap_HdcpIntr_Status & DPTX_HDCP_KSV_ACCESS_INT )
	{
		dptx_dbg( "KSV memory access guaranteed for read, write access " );
	}
	
	if( uiRegMap_HdcpIntr_Status & DPTX_HDCP22_GPIOINT ) 
	{
		uiRegmap_Hdcp_Observation = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_OBS );
		if( uiRegmap_Hdcp_Observation & DPTX_HDCP22_SINK_CAPABLE_SINK )	/* 1 : HDCP 22 Sink */
		{
			bHDCP22_Sink = true;
			dptx_dbg( "HDCP2.2 sink " );
		}
		else
		{
			bHDCP22_Sink = false;
			dptx_dbg( "Sink not HDCP2.2 Sink.. Got to HDCP13 " );
		}
	}
	else
	{
		dptx_dbg( "DPTX_HDCP22_GPIOINT isn't triggered " );
		bHDCP22_Sink = true;
	}
	
	if( bHDCP22_Sink )
	{
		if( uiRegmap_Hdcp_Observation & DPTX_HDCP22_AUTHENTICATION_SUCCESS ) 
		{
			dptx_dbg( "HDCP22 authentication process was successful " );
		}
		if( uiRegmap_Hdcp_Observation & DPTX_HDCP22_AUTHENTICATION_FAILED ) 
		{
			dptx_dbg( "HDCP22 authentication process was failed " );
		}
	}
	else
	{
		if( uiRegMap_HdcpIntr_Status & DPTX_HDCP_KSV_SHA1 )
		{
			dptx_dbg( "HDCP13 SHA1 verification has been done" );
		}

		if( uiRegMap_HdcpIntr_Status & DPTX_HDCP_AUX_RESP_TIMEOUT ) /* DPTX_HDCP_AUX_RESP_TIMEOUT[Bit 3] : Aux transfer initiated by HDCP13CTRL did not receive a response an timedout */
		{
			dptx_dbg( "HDCP13CTRL did not receive.. time out " );

			pstHdcpParams->ucAuthen_fail_count = DPTX_HDCP_MAX_AUTH_RETRY;
			Dptx_Hdcp_Set_HDCP13_State( pstDptx, false );
		}

		if( uiRegMap_HdcpIntr_Status & DPTX_HDCP_FAILED ) 	/* HDCP authentication process was failed */
		{
			if( pstHdcpParams->ucAuthen_fail_count++ > DPTX_HDCP_MAX_AUTH_RETRY )
			{
				Dptx_Hdcp_Set_HDCP13_State( pstDptx, false );
			}

			dptx_dbg( "HDCP13 authentication process was failed( %d -> %d ) ", pstHdcpParams->ucAuthen_fail_count, DPTX_HDCP_MAX_AUTH_RETRY );
		}

		if( uiRegMap_HdcpIntr_Status & DPTX_HDCP_ENGAGED ) 
		{
			pstHdcpParams->ucAuthen_fail_count = 0;

			dptx_dbg( "HDCP13 authentication process was successful" );
		}
	}

	Dptx_Reg_Writel( pstDptx, DPTX_HDCP_INT_CLR, uiRegMap_HdcpIntr_Status );

	return ( DPTX_HDCP_RETURN_SUCCESS );
}

void Dptx_Hdcp_Set_HDCP13_State( struct Dptx_Params *pstDptx, bool bEnable )
{
	u32							uiRegMap_HdcpCfg;
	struct Dptx_Hdcp_Params		*pstHdcpParams   = &pstDptx->stHdcpParams;

	dptx_dbg( "Calling... " );

	uiRegMap_HdcpCfg = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_CFG );

	if( bEnable ) 
	{
		uiRegMap_HdcpCfg |= DPTX_CFG_EN_HDCP13;
		uiRegMap_HdcpCfg |= DPTX_CFG_EN_HDCP;

		pstHdcpParams->ucHDCP13_Enable = 1;
	} 
	else 
	{
		uiRegMap_HdcpCfg &= ~DPTX_CFG_EN_HDCP13;
		uiRegMap_HdcpCfg &= ~DPTX_CFG_EN_HDCP;

		pstHdcpParams->ucHDCP13_Enable = 0;
	}
	
	Dptx_Reg_Writel( pstDptx, DPTX_HDCP_CFG, uiRegMap_HdcpCfg );
}

void Dptx_Hdcp_Set_HDCP22_State( struct Dptx_Params *pstDptx, bool bEnable )
{
	u32							uiRegMap_HdcpCfg;
	struct Dptx_Hdcp_Params		*pstHdcpParams;

	dptx_dbg( "Calling... " );
	
	pstHdcpParams = &pstDptx->stHdcpParams;

	uiRegMap_HdcpCfg = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_CFG );
	if( bEnable ) 
	{
		uiRegMap_HdcpCfg &= ~DPTX_CFG_EN_HDCP13;
		uiRegMap_HdcpCfg |= DPTX_CFG_EN_HDCP;
		
		pstHdcpParams->ucHDCP22_Enable = 1;
	} 
	else 
	{
		uiRegMap_HdcpCfg |= DPTX_CFG_EN_HDCP13;
		uiRegMap_HdcpCfg &= ~DPTX_CFG_EN_HDCP;
		
		pstHdcpParams->ucHDCP22_Enable = 0;
	}
	
	Dptx_Reg_Writel( pstDptx, DPTX_HDCP_CFG, uiRegMap_HdcpCfg );
}

bool Dptx_Hdcp_Configure_HDCP13( struct Dptx_Params *pstDptx )
{
	bool						bRetVal;
	u8							ucDPCD_Revision, ucMajorNum, ucMinorNum;
	u32							uiRegMap_HdcpCfg;
	struct Dptx_Hdcp_Params		*pstHdcpParams = &pstDptx->stHdcpParams;

	dptx_dbg( "Calling... " );

	bRetVal	= dptx_write_dpk_keys( pstDptx );
	if( bRetVal == DPTX_HDCP_RETURN_FAIL ) 
	{
		return ( DPTX_HDCP_RETURN_FAIL );
	}

	bRetVal = Dptx_Aux_Read_DPCD( pstDptx, DP_DPCD_REV, &ucDPCD_Revision );
	if( bRetVal == DPTX_HDCP_RETURN_FAIL ) 
	{
		return ( DPTX_HDCP_RETURN_FAIL );
	}

	ucMajorNum = ( (ucDPCD_Revision & 0xf0) >> 4 );
	ucMinorNum = ( ucDPCD_Revision & 0xf );

	uiRegMap_HdcpCfg = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_CFG );
	uiRegMap_HdcpCfg &= ~DPTX_HDCP_CFG_DPCD_PLUS_MASK;
	
	if( ucMajorNum== 1 && ucMinorNum >= 2 )
	{
		uiRegMap_HdcpCfg |= 1 << DPTX_HDCP_CFG_DPCD_PLUS_SHIFT;	/* DPTX_HDCP_CFG_DPCD_PLUS : DPCD revision of Sink is 1.2 or higher. updating by SW after reading DPCD revision */
	}

	Dptx_Reg_Writel( pstDptx, DPTX_HDCP_CFG, uiRegMap_HdcpCfg );
	
	Dptx_Hdcp_Set_HDCP13_State( pstDptx, true );
	
	pstHdcpParams->ucHDCP13_Enable = 1;

	return false;
}

