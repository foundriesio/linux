/*
 *           Copyright 2007-2017 Availink, Inc.
 *
 *  This software contains Availink proprietary information and
 *  its use and disclosure are restricted solely to the terms in
 *  the corresponding written license agreement. It shall not be 
 *  disclosed to anyone other than valid licensees without
 *  written permission of Availink, Inc.
 *
 */


///$Date: 2017-8-3 15:59 $
///
//#include <stdio.h>

#include "AVL62X1_API.h"

extern AVL_int32 iCarrierFreqOffSet;
AVL_ErrorCode AVL62X1_GetChipID(AVL_uint16 slave_addr, AVL_puint32 pChipId)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r = II2C_Read32_AVL62X1(slave_addr, AVL62X1_reset_manager__chip_id, pChipId);

	return (r);
}

AVL_ErrorCode AVL62X1_Initialize(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint16 i = 0;
	AVL_uint16 uiMaxRetries = 100;
	AVL_uint16 delay_unit_ms = 50;  //Time out window is uiMaxRetries * delay_unit_ms = 5000ms;

	r |= IBase_Initialize_AVL62X1(pAVL_Chip);  //download, boot, load defaults
	while (AVL_EC_OK != IBase_CheckChipReady_AVL62X1(pAVL_Chip))
	{
		if (i++ >= uiMaxRetries)
		{
			r |= AVL_EC_GENERAL_FAIL;
			return (r);
		}
		AVL_IBSP_Delay(delay_unit_ms);
	}

	r |= IBase_Initialize_TunerI2C_AVL62X1(pAVL_Chip); //config i2c repeater
	r |= IRx_Initialize_AVL62X1(pAVL_Chip);
	r |= IRx_SetTunerPola_AVL62X1(pAVL_Chip->e_TunerPol, pAVL_Chip);
	r |= IRx_DriveAGC_AVL62X1(AVL62X1_ON, pAVL_Chip);
	r |= IRx_SetMpegMode_AVL62X1(pAVL_Chip);
	r |= IRx_DriveMpegOutput_AVL62X1(AVL62X1_ON, pAVL_Chip);

	return (r);
}

AVL_ErrorCode AVL62X1_LockTP( struct AVL62X1_CarrierInfo * pCarrierInfo, struct AVL62X1_StreamInfo * pStreamInfo, AVL_bool blind_sym_rate, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	if(blind_sym_rate) 
	{
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_blind_sym_rate_enable_caddr, 1);

	} 
	else 
	{
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_blind_sym_rate_enable_caddr, 0);
	}

	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_blind_cfo_enable_caddr, 1);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_nom_symbol_rate_Hz_iaddr, pCarrierInfo->m_symbol_rate_Hz);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_nom_carrier_freq_Hz_iaddr, pCarrierInfo->m_carrier_freq_offset_Hz);

	//r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_ConfiguredPLScramKey_iaddr, pCarrierInfo->m_PL_scram_key);//the register 0x85c default value is "AVL62X1_PL_SCRAM_AUTO"

	if(pStreamInfo != nullptr) 
	{
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_S2X_ConfiguredStreamType_caddr, pStreamInfo->m_stream_type);
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_S2X_sp_wanted_stream_id_caddr, pStreamInfo->m_ISI);
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_S2X_PLP_ID_caddr, pStreamInfo->m_PLP_ID);
	}
	else
	{
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_S2X_ConfiguredStreamType_caddr, AVL62X1_UNDETERMINED);
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_S2X_sp_wanted_stream_id_caddr, 0);
	}

	r |= IBase_SendRxOP_AVL62X1(CMD_ACQUIRE, pAVL_Chip);

	return (r);
}

AVL_ErrorCode AVL62X1_GetLockStatus(enum AVL62X1_LockStatus * eLockStat, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar ucLockStatus = 0;

	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_sp_lock_caddr, &ucLockStatus);
	if (ucLockStatus != 0)
	{
		*eLockStat = AVL62X1_STATUS_LOCK;
	}
	else
	{
		*eLockStat = AVL62X1_STATUS_UNLOCK;
	}

	return (r);
}

AVL_ErrorCode AVL62X1_GetLostLockStatus(enum AVL62X1_LostLockStatus *eLostLockStat, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar ucLostLock = 0;

	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_lost_lock_caddr, &ucLostLock);
	if (ucLostLock != 0)
	{
		*eLostLockStat = AVL62X1_Lost_Lock_Yes;
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_lost_lock_caddr, 0);
	}
	else
	{
		*eLostLockStat = AVL62X1_Lost_Lock_No;
	}

	return (r);
}

AVL_ErrorCode AVL62X1_DiscoverStreams( struct AVL62X1_CarrierInfo * pCarrierInfo, AVL_bool blind_sym_rate, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	r |= AVL62X1_LockTP(pCarrierInfo, nullptr, blind_sym_rate, pAVL_Chip);
	return (r);
}

AVL_ErrorCode AVL62X1_GetDiscoveryStatus(enum AVL62X1_DiscoveryStatus *eDiscoveryStat, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar ucDiscStatus = 0;

	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_SP_S2X_sp_stream_discover_done_caddr, &ucDiscStatus);
	if (ucDiscStatus != 0)
	{
		*eDiscoveryStat = AVL62X1_DISCOVERY_FINISHED;
	}
	else
	{
		*eDiscoveryStat = AVL62X1_DISCOVERY_RUNNING;
	}
	return (r);
}

AVL_ErrorCode AVL62X1_GetStreamNumber(AVL_puchar pStreamNum, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_SP_S2X_sp_NumStreams_cur_TP_caddr, pStreamNum);

	return (r);
}

AVL_ErrorCode AVL62X1_GetStreamList(struct AVL62X1_StreamInfo *pStreams, const AVL_uchar max_num_streams, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 stream_list_ptr = 0;
	AVL_uchar num_streams = 0;
	AVL_uchar stream = 0;
	AVL_uchar tmp8 = 0;

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_SP_S2X_sp_DVB_STREAM_addr_iaddr, &stream_list_ptr);
	r |= AVL62X1_GetStreamNumber(&num_streams, pAVL_Chip);
	if(num_streams > max_num_streams)
	{
		num_streams = max_num_streams;
	}
	for(stream=0; stream<num_streams; stream++) 
	{
		//TODO: this could probably be optimized to a single I2C burst read
		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, stream_list_ptr + stream*AVL62X1_DVB_STREAM_struct_size + AVL62X1_DVB_STREAM_CarrierIndex_caddr, &tmp8);
		pStreams[stream].m_carrier_index = tmp8;

		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, stream_list_ptr + stream*AVL62X1_DVB_STREAM_struct_size + AVL62X1_DVB_STREAM_StreamType_caddr, &tmp8);
		pStreams[stream].m_stream_type = (AVL62X1_DVBStreamType)tmp8;

		if(pStreams[stream].m_stream_type == AVL62X1_T2MI) 
		{
			r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, stream_list_ptr + stream*AVL62X1_DVB_STREAM_struct_size + AVL62X1_DVB_STREAM_PLP_ID_caddr, &tmp8);
			pStreams[stream].m_PLP_ID = tmp8;
		}

		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, stream_list_ptr + stream*AVL62X1_DVB_STREAM_struct_size + AVL62X1_DVB_STREAM_ISI_caddr, &tmp8);
		pStreams[stream].m_ISI = tmp8;
	}

	return (r);
}

AVL_ErrorCode AVL62X1_SwitchStream(struct AVL62X1_StreamInfo *pStreamInfo, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar uilockstatus = 0;

	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_fec_lock_caddr, &uilockstatus);
	if(uilockstatus != 0)
	{
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_S2X_ConfiguredStreamType_caddr, pStreamInfo->m_stream_type);
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_S2X_sp_wanted_stream_id_caddr, pStreamInfo->m_ISI);
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_S2X_PLP_ID_caddr, pStreamInfo->m_PLP_ID);

		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_sp_lock_caddr, 0);
		r |= IBase_SendSPOP_AVL62X1(SP_CMD_ACQUIRE, pAVL_Chip);
	}
	else
	{
		r |= AVL_EC_GENERAL_FAIL; // demod isn't locked.
	}

	return (r);
}

AVL_ErrorCode AVL62X1_GetSNR( AVL_pint16 piSNR_x100db, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Read16_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_snr_dB_x100_saddr, (AVL_puint16)piSNR_x100db);

	return (r);
}

AVL_ErrorCode AVL62X1_GetSignalInfo(struct AVL62X1_CarrierInfo * pCarrierInfo, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar ucTemp = 0;
	AVL_uint32 uiTemp = 0;

	//If blind carrier freq search was performed, this is the carrier freq as determined
	//  by the blind search algorithm. Otherwise, it is just the nominal carrier freq from the config.
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_carrier_freq_Hz_iaddr, &uiTemp);
	pCarrierInfo->m_carrier_freq_offset_Hz = (AVL_int32)uiTemp;

	//Difference, in Hertz, between nominal carrier freq and current freq as indicated by the frequency loop.
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_carrier_freq_err_Hz_iaddr, &uiTemp);
	pCarrierInfo->m_carrier_freq_offset_Hz += (AVL_int32)uiTemp;

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_symbol_rate_Hz_iaddr, &uiTemp);
	pCarrierInfo->m_symbol_rate_Hz = uiTemp;

	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_signal_type_caddr, &ucTemp);
	pCarrierInfo->m_signal_type = (AVL62X1_Standard)(ucTemp);

	if(pCarrierInfo->m_signal_type == AVL62X1_DVBS) 
	{
		pCarrierInfo->m_modulation = AVL62X1_QPSK;

		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_dvbs_code_rate_caddr, &ucTemp);
		pCarrierInfo->m_coderate.m_dvbs_code_rate = (AVL62X1_DVBS_CodeRate)(ucTemp);

		pCarrierInfo->m_roll_off = AVL62X1_RollOff_35;
	} 
	else 
	{
		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_s2_pilot_on_caddr, &ucTemp);
		pCarrierInfo->m_pilot = (AVL62X1_Pilot)(ucTemp);

		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_s2_fec_len_caddr, &ucTemp);
		pCarrierInfo->m_dvbs2_fec_length = (AVL62X1_DVBS2_FECLength)(ucTemp);

		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_s2_modulation_caddr, &ucTemp);
		pCarrierInfo->m_modulation = (AVL62X1_ModulationMode)(ucTemp);

		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_s2_code_rate_caddr, &ucTemp);
		pCarrierInfo->m_coderate.m_dvbs2_code_rate = (AVL62X1_DVBS2_CodeRate)(ucTemp);

		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_ccm1_acm0_caddr, &ucTemp);
		if(ucTemp == 0) 
		{
			pCarrierInfo->m_dvbs2_ccm_acm = AVL62X1_DVBS2_ACM;
			pCarrierInfo->m_PLS_ACM = 0;
		} 
		else 
		{
			pCarrierInfo->m_dvbs2_ccm_acm = AVL62X1_DVBS2_CCM;
			r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_ccm_pls_mode_caddr, &ucTemp);
			pCarrierInfo->m_PLS_ACM = ucTemp;
		}

		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_alpha_caddr, &ucTemp);
		pCarrierInfo->m_roll_off = (AVL62X1_RollOff)(ucTemp);

		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_PLScramKey_iaddr, &uiTemp);
		pCarrierInfo->m_PL_scram_key = uiTemp;

		//r |= AVL_AVL62X1_GetStreamNumber(&pCarrierInfo->m_num_streams, pAVL_Chip);
		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_SP_S2X_sp_SIS_MIS_caddr, &ucTemp);
		pCarrierInfo->m_SIS_MIS = (AVL62X1_SIS_MIS)(ucTemp);
	}
	return (r);
}

AVL_ErrorCode AVL62X1_GetSignalStrength(AVL_puint16 puiSignalStrength, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint16 uiTemp = 0;

	r = II2C_Read16_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_DMD_rfagc_gain_saddr, &uiTemp);
	if (uiTemp <= 25000)
	{
		*puiSignalStrength = 100;
	}
	else if (uiTemp >= 55000)
	{
		*puiSignalStrength = 10;
	}
	else
	{
		*puiSignalStrength = (55000-uiTemp)*90/30000+10;
	}

	return (r);
}

AVL_ErrorCode AVL62X1_GetSignalQuality(AVL_puint16 puiSignalQuality, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_int16 iTemp = 0;

	r = AVL62X1_GetSNR(&iTemp, pAVL_Chip);
	if (iTemp >= 2500)
	{
		*puiSignalQuality = 100;
	}
	else if (iTemp <= 0)
	{
		*puiSignalQuality = 10;
	}
	else
	{
		*puiSignalQuality = iTemp*90/2500+10;
	}

	return (r);
}

extern struct AVL62X1_ErrorStatus AVL62X1_esm;

AVL_ErrorCode AVL62X1_ResetPER(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 uiTemp = 0;

	AVL62X1_esm.m_SwCntNumPkts.uiHighWord = 0;
	AVL62X1_esm.m_SwCntNumPkts.uiLowWord = 0;
	AVL62X1_esm.m_SwCntPktErrors.uiHighWord = 0;
	AVL62X1_esm.m_SwCntPktErrors.uiLowWord = 0;
	AVL62X1_esm.m_NumPkts.uiHighWord = 0;
	AVL62X1_esm.m_NumPkts.uiLowWord = 0;
	AVL62X1_esm.m_PktErrors.uiHighWord = 0;
	AVL62X1_esm.m_PktErrors.uiLowWord = 0;
	AVL62X1_esm.m_PER = 0;

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, &uiTemp);
	uiTemp |= 0x00000001;
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, &uiTemp);
	uiTemp |= 0x00000008;
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);
	uiTemp |= 0x00000001;
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);
	uiTemp &= 0xFFFFFFFE;
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);

	return (r);
}

AVL_ErrorCode AVL62X1_GetPER(AVL_puint32 puiPER_x1e9, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	enum AVL62X1_LockStatus lock_status = AVL62X1_STATUS_UNLOCK;
	AVL_uint32 uiHwCntPktErrors = 0;
	AVL_uint32 uiHwCntNumPkts = 0;
	AVL_uint32 uiTemp = 0;
	AVL62X1_uint64 uiTemp64 = {0,0};

	r |= AVL62X1_GetLockStatus(&lock_status, pAVL_Chip);

	//record the lock status before return the PER
	if(AVL62X1_STATUS_LOCK == lock_status)
	{
		AVL62X1_esm.m_LostLock = 0;
	}
	else
	{
		AVL62X1_esm.m_LostLock = 1;
		return *puiPER_x1e9 = AVL_CONSTANT_10_TO_THE_9TH;
	}

	r = II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__packet_err_cnt, &uiHwCntPktErrors);
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__packet_num, &uiHwCntNumPkts);

	if(uiHwCntNumPkts > (1<<30))
	{
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, &uiTemp);
		uiTemp |= 0x00000001;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__packet_err_cnt, &uiHwCntPktErrors);
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__packet_num, &uiHwCntNumPkts);
		uiTemp &= 0xFFFFFFFE;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);
		Add32To64_AVL62X1(&AVL62X1_esm.m_SwCntNumPkts, uiHwCntNumPkts);
		Add32To64_AVL62X1(&AVL62X1_esm.m_SwCntPktErrors, uiHwCntPktErrors);
		uiHwCntNumPkts = 0;
		uiHwCntPktErrors = 0;
	}       

	AVL62X1_esm.m_NumPkts.uiHighWord = AVL62X1_esm.m_SwCntNumPkts.uiHighWord;
	AVL62X1_esm.m_NumPkts.uiLowWord = AVL62X1_esm.m_SwCntNumPkts.uiLowWord;
	Add32To64_AVL62X1(&AVL62X1_esm.m_NumPkts, uiHwCntNumPkts);
	AVL62X1_esm.m_PktErrors.uiHighWord = AVL62X1_esm.m_SwCntPktErrors.uiHighWord;
	AVL62X1_esm.m_PktErrors.uiLowWord = AVL62X1_esm.m_SwCntPktErrors.uiLowWord;
	Add32To64_AVL62X1(&AVL62X1_esm.m_PktErrors, uiHwCntPktErrors);

	// Compute the PER
	Multiply32_AVL62X1(&uiTemp64, AVL62X1_esm.m_PktErrors.uiLowWord, AVL_CONSTANT_10_TO_THE_9TH);
	AVL62X1_esm.m_PER = Divide64_AVL62X1(AVL62X1_esm.m_NumPkts, uiTemp64);
	//keep the PER user wanted
	*puiPER_x1e9 = AVL62X1_esm.m_PER;

	return (r);
}

/************************************************************************/
/* Diseqc                                                               */
/************************************************************************/
#define Diseqc_delay 20

AVL_ErrorCode AVL62X1_IDiseqc_ReadModulationData( AVL_puchar pucBuff, AVL_puchar pucSize, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 i1 = 0;
	AVL_uint32 i2 = 0;
	AVL_uchar pucBuffTemp[4] = {0};

	r = AVL_IBSP_WaitSemaphore(&(pAVL_Chip->m_semDiseqc));
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_rx_st, &i1);
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, &i2);
	if((i2>>8) & 0x01)
	{
		pAVL_Chip->m_Diseqc_OP_Status = AVL62X1_DOS_InModulation; 
	}
	if( AVL62X1_DOS_InModulation == pAVL_Chip->m_Diseqc_OP_Status )
	{
		// In modulation mode
		if( (!((i2>>8) & 0x01 ) && (0x00000004 == (i1 & 0x00000004))) || (((i2>>8) & 0x01 ) &&(0x00000004 != (i1 & 0x00000004))))
		{
			*pucSize = (AVL_uchar)((i1 & 0x00000078)>>3);
			//Receive data
			for( i1=0; i1<*pucSize; i1++ )
			{
				r |= II2C_Read_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__rx_fifo, pucBuffTemp, 4);
				pucBuff[i1] = pucBuffTemp[3];
			}
		}
		else
		{
			r = AVL_EC_GENERAL_FAIL;
		}
	}
	else
	{
		r = AVL_EC_GENERAL_FAIL;
	}

	r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semDiseqc));

	return (r);
}

AVL_ErrorCode AVL62X1_IDiseqc_SendModulationData( const AVL_puchar pucBuff, AVL_uchar ucSize, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 i1 = 0;
	AVL_uint32 i2 = 0;
	AVL_uchar pucBuffTemp[8] = {0};
	AVL_uchar Continuousflag = 0;
	AVL_uint16 uiTempOutTh = 0;

	if( ucSize>8 )
	{
		r = AVL_EC_MemoryRunout;
	}
	else
	{
		r = AVL_IBSP_WaitSemaphore(&(pAVL_Chip->m_semDiseqc));   
		r |= IDiseqc_IsSafeToSwitchMode_AVL62X1(pAVL_Chip);
		if( AVL_EC_OK ==  r)
		{
			if (pAVL_Chip->m_Diseqc_OP_Status == AVL62X1_DOS_InContinuous)
			{
				r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, &i1);
				if ((i1>>10) & 0x01)
				{
					Continuousflag = 1;
					i1 &= 0xfffff3ff;
					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);
					r |= AVL_IBSP_Delay(Diseqc_delay);		//delay 20ms
				}
			}
			//reset rx_fifo
			r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_rx_cntrl, &i2);
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_rx_cntrl, (i2|0x01));
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_rx_cntrl, (i2&0xfffffffe));

			r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, &i1);
			i1 &= 0xfffffff8;	//set to modulation mode and put it to FIFO load mode
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);

			//trunk address
			ChunkAddr_AVL62X1(diseqc__tx_fifo, pucBuffTemp);
			pucBuffTemp[3] = 0;
			pucBuffTemp[4] = 0;
			pucBuffTemp[5] = 0;
			for( i2=0; i2<ucSize; i2++ )
			{
				pucBuffTemp[6] = pucBuff[i2];

				r |= II2C_Write_AVL62X1(pAVL_Chip->usI2CAddr, pucBuffTemp, 7);
			}                           
			i1 |= (1<<2);  //start fifo transmit.
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);

			if( AVL_EC_OK == r )
			{
				pAVL_Chip->m_Diseqc_OP_Status = AVL62X1_DOS_InModulation;
			}
			do 
			{
				r |= AVL_IBSP_Delay(1);
				if (++uiTempOutTh > 500)
				{
					r |= AVL_EC_TimeOut;
					r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semDiseqc));
					return(r);
				}
				r = II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_st, &i1);
			} while ( 1 != ((i1 & 0x00000040) >> 6) );

			r = AVL_IBSP_Delay(Diseqc_delay);		//delay 20ms
			if (Continuousflag == 1)			//resume to send out wave
			{
				//No data in FIFO
				r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, &i1);
				i1 &= 0xfffffff8; 
				i1 |= 0x03;		//switch to continuous mode
				r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);

				//start to send out wave
				i1 |= (1<<10);  
				r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);
				if( AVL_EC_OK == r )
				{
					pAVL_Chip->m_Diseqc_OP_Status = AVL62X1_DOS_InContinuous;
				}
			}
		}
		r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semDiseqc));
	}

	return (r);
}

AVL_ErrorCode AVL62X1_IDiseqc_GetTxStatus( struct AVL62X1_Diseqc_TxStatus * pTxStatus, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 i1 = 0;

	r = AVL_IBSP_WaitSemaphore(&(pAVL_Chip->m_semDiseqc));
	if( (AVL62X1_DOS_InModulation == pAVL_Chip->m_Diseqc_OP_Status) || (AVL62X1_DOS_InTone == pAVL_Chip->m_Diseqc_OP_Status) )
	{
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_st, &i1);
		pTxStatus->m_TxDone = (AVL_uchar)((i1 & 0x00000040)>>6);
		pTxStatus->m_TxFifoCount = (AVL_uchar)((i1 & 0x0000003c)>>2);
	}
	else
	{
		r |= AVL_EC_GENERAL_FAIL;
	}
	r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semDiseqc));

	return (r);
}

AVL_ErrorCode AVL62X1_IDiseqc_GetRxStatus(struct AVL62X1_Diseqc_RxStatus * pRxStatus, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 i1 = 0;

	r = AVL_IBSP_WaitSemaphore(&(pAVL_Chip->m_semDiseqc));
	if( AVL62X1_DOS_InModulation == pAVL_Chip->m_Diseqc_OP_Status )
	{
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_rx_st, &i1);
		pRxStatus->m_RxDone = (AVL_uchar)((i1 & 0x00000004)>>2);
		pRxStatus->m_RxFifoCount = (AVL_uchar)((i1 & 0x000000078)>>3);
	}
	else
	{
		r |= AVL_EC_GENERAL_FAIL;
	}
	r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semDiseqc));

	return (r);
}

AVL_ErrorCode AVL62X1_IDiseqc_SendTone( AVL_uchar ucTone, AVL_uchar ucCount, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 i1 = 0;
	AVL_uint32 i2 = 0;
	AVL_uchar pucBuffTemp[8];
	AVL_uchar Continuousflag = 0;
	AVL_uint16 uiTempOutTh = 0;

	if( ucCount>8 )
	{
		r = AVL_EC_MemoryRunout;
	}
	else
	{
		r = AVL_IBSP_WaitSemaphore(&(pAVL_Chip->m_semDiseqc));
		r |= IDiseqc_IsSafeToSwitchMode_AVL62X1(pAVL_Chip);

		if( AVL_EC_OK == r )
		{
			if (pAVL_Chip->m_Diseqc_OP_Status == AVL62X1_DOS_InContinuous)
			{
				r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, &i1);
				if ((i1>>10) & 0x01)
				{
					Continuousflag = 1;
					i1 &= 0xfffff3ff;
					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);
					r |= AVL_IBSP_Delay(Diseqc_delay);		//delay 20ms
				}
			}
			//No data in the FIFO.
			r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, &i1);
			i1 &= 0xfffffff8;  //put it into the FIFO load mode.
			if( 0 == ucTone )
			{
				i1 |= 0x01;
			}
			else
			{
				i1 |= 0x02;
			}
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);

			//trunk address
			ChunkAddr_AVL62X1(diseqc__tx_fifo, pucBuffTemp);
			pucBuffTemp[3] = 0;
			pucBuffTemp[4] = 0;
			pucBuffTemp[5] = 0;
			pucBuffTemp[6] = 1;

			for( i2=0; i2<ucCount; i2++ )
			{
				r |= II2C_Write_AVL62X1(pAVL_Chip->usI2CAddr, pucBuffTemp, 7);
			}

			i1 |= (1<<2);  //start fifo transmit.
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);
			if( AVL_EC_OK == r )
			{
				pAVL_Chip->m_Diseqc_OP_Status = AVL62X1_DOS_InTone;
			}
			do 
			{
				r |= AVL_IBSP_Delay(1);
				if (++uiTempOutTh > 500)
				{
					r |= AVL_EC_TimeOut;
					r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semDiseqc));
					return(r);
				}
				r = II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_st, &i1);
			} while ( 1 != ((i1 & 0x00000040) >> 6) );

			r = AVL_IBSP_Delay(Diseqc_delay);		//delay 20ms
			if (Continuousflag == 1)			//resume to send out wave
			{
				//No data in FIFO
				r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, &i1);
				i1 &= 0xfffffff8; 
				i1 |= 0x03;		//switch to continuous mode
				r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);

				//start to send out wave
				i1 |= (1<<10);  
				r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);
				if( AVL_EC_OK == r )
				{
					pAVL_Chip->m_Diseqc_OP_Status = AVL62X1_DOS_InContinuous;
				}
			}
		}
		r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semDiseqc));
	}

	return (r);
}

AVL_ErrorCode AVL62X1_IDiseqc_Start22K(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 i1 = 0;

	r = AVL_IBSP_WaitSemaphore(&(pAVL_Chip->m_semDiseqc));
	r |= IDiseqc_IsSafeToSwitchMode_AVL62X1(pAVL_Chip);

	if( AVL_EC_OK == r )
	{
		//No data in FIFO
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, &i1);
		i1 &= 0xfffffff8; 
		i1 |= 0x03;		//switch to continuous mode
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);

		//start to send out wave
		i1 |= (1<<10);  
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);
		if( AVL_EC_OK == r )
		{
			pAVL_Chip->m_Diseqc_OP_Status = AVL62X1_DOS_InContinuous;
		}
	}
	r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semDiseqc));

	return (r);
}

AVL_ErrorCode AVL62X1_IDiseqc_Stop22K(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 i1 = 0;

	r = AVL_IBSP_WaitSemaphore(&(pAVL_Chip->m_semDiseqc));
	if( AVL62X1_DOS_InContinuous == pAVL_Chip->m_Diseqc_OP_Status )
	{
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, &i1);
		i1 &= 0xfffff3ff;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);
	}

	r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semDiseqc));

	return (r);
}

/************************************************************************/
/* Tuner I2C                                                            */
/************************************************************************/
AVL_ErrorCode AVL62X1_OpenTunerI2C(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_hw_i2c_bit_rpt_cntrl, 0x07);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_hw_i2c_bit_rpt_cntrl, 0x07);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_hw_i2c_bit_rpt_cntrl, 0x07);

	return (r);
}

AVL_ErrorCode AVL62X1_CloseTunerI2C(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_hw_i2c_bit_rpt_cntrl, 0x06);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_hw_i2c_bit_rpt_cntrl, 0x06);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_hw_i2c_bit_rpt_cntrl, 0x06);

	return (r);
}

/************************************************************************/
/* GPIO                                                                 */
/************************************************************************/
AVL_ErrorCode AVL62X1_SetGPIODir( enum AVL62X1_GPIO_Pin ePin, enum AVL62X1_GPIO_Pin_Direction eDir, struct AVL62X1_Chip * pAVL_Chip )
{
	AVL_ErrorCode r = AVL_EC_OK;
	switch(ePin)
	{
	case AVL62X1_GPIO_Pin_TUNER_SDA:
		if(eDir == AVL62X1_GPIO_DIR_OUTPUT)
		{
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__i2c_data2_sel, (AVL_uint32)(AVL62X1_GPIO_VALUE_LOGIC_0));
		}
		else if(eDir == AVL62X1_GPIO_DIR_INPUT)
		{
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__i2c_data2_sel, (AVL_uint32)(AVL62X1_GPIO_VALUE_HIGH_Z));
		}
		else
		{
			return AVL_EC_GENERAL_FAIL;
		}
		break;
	case AVL62X1_GPIO_Pin_TUNER_SCL:
		if(eDir == AVL62X1_GPIO_DIR_OUTPUT)
		{
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__i2c_clk2_sel, (AVL_uint32)(AVL62X1_GPIO_VALUE_LOGIC_0));
		}
		else if(eDir == AVL62X1_GPIO_DIR_INPUT)
		{
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__i2c_clk2_sel, (AVL_uint32)(AVL62X1_GPIO_VALUE_HIGH_Z));
		}
		else
		{
			return AVL_EC_GENERAL_FAIL;
		}
		break;
	case AVL62X1_GPIO_Pin_S_AGC2:
		if(eDir == AVL62X1_GPIO_DIR_OUTPUT)
		{
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__agc2_sel, (AVL_uint32)(AVL62X1_GPIO_VALUE_LOGIC_0));
		}
		else if(eDir == AVL62X1_GPIO_DIR_INPUT)
		{
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__agc2_sel, (AVL_uint32)(AVL62X1_GPIO_VALUE_HIGH_Z));
		}
		else
		{
			return AVL_EC_GENERAL_FAIL;
		}
		break;
	case AVL62X1_GPIO_Pin_LNB_PWR_EN:
		if(eDir == AVL62X1_GPIO_DIR_OUTPUT)
		{
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__lnb_cntrl_0_sel, (AVL_uint32)(AVL62X1_GPIO_VALUE_LOGIC_0));
		}
		else if(eDir == AVL62X1_GPIO_DIR_INPUT)
		{
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__lnb_cntrl_0_sel, (AVL_uint32)(AVL62X1_GPIO_VALUE_HIGH_Z));
		}
		else
		{
			return AVL_EC_GENERAL_FAIL;
		}
		break;
	case AVL62X1_GPIO_Pin_LNB_PWR_SEL:
		if(eDir == AVL62X1_GPIO_DIR_OUTPUT)
		{
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__lnb_cntrl_1_sel, (AVL_uint32)(AVL62X1_GPIO_VALUE_LOGIC_0));
		}
		else if(eDir == AVL62X1_GPIO_DIR_INPUT)
		{
			r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__lnb_cntrl_1_sel, (AVL_uint32)(AVL62X1_GPIO_VALUE_HIGH_Z));
		}
		else
		{
			return AVL_EC_GENERAL_FAIL;
		}
		break;
	default:
		return AVL_EC_GENERAL_FAIL;
	}
	return (r);
}

AVL_ErrorCode AVL62X1_SetGPIOVal( enum AVL62X1_GPIO_Pin ePin, enum AVL62X1_GPIO_Pin_Value eVal, struct AVL62X1_Chip * pAVL_Chip )
{
	AVL_ErrorCode r = AVL_EC_OK;

	if((eVal != AVL62X1_GPIO_VALUE_LOGIC_0) && (eVal != AVL62X1_GPIO_VALUE_LOGIC_1) && (eVal != AVL62X1_GPIO_VALUE_HIGH_Z))
		return AVL_EC_GENERAL_FAIL;

	switch(ePin)
	{
	case AVL62X1_GPIO_Pin_TUNER_SDA:
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__i2c_data2_sel, (AVL_uint32)(eVal));
		break;
	case AVL62X1_GPIO_Pin_TUNER_SCL:
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__i2c_clk2_sel, (AVL_uint32)(eVal));
		break;
	case AVL62X1_GPIO_Pin_S_AGC2:
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__agc2_sel, (AVL_uint32)(eVal));
		break;
	case AVL62X1_GPIO_Pin_LNB_PWR_EN:
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__lnb_cntrl_0_sel, (AVL_uint32)(eVal));
		break;
	case AVL62X1_GPIO_Pin_LNB_PWR_SEL:
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__lnb_cntrl_1_sel, (AVL_uint32)(eVal));
		break;
	default:
		return AVL_EC_GENERAL_FAIL;
	}
	return (r);
}

AVL_ErrorCode AVL62X1_GetGPIOVal(enum AVL62X1_GPIO_Pin ePin, enum AVL62X1_GPIO_Pin_Value * peVal, struct AVL62X1_Chip * pAVL_Chip )
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 tmp = 0;

	switch(ePin)
	{
	case AVL62X1_GPIO_Pin_TUNER_SDA:
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__i2c_data2_i, &tmp);
		break;
	case AVL62X1_GPIO_Pin_TUNER_SCL:
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__i2c_clk2_i, &tmp);
		break;
	case AVL62X1_GPIO_Pin_S_AGC2:
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__agc2_i, &tmp);
		break;
	case AVL62X1_GPIO_Pin_LNB_PWR_EN:
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__lnb_cntrl_0_i, &tmp);
		break;
	case AVL62X1_GPIO_Pin_LNB_PWR_SEL:
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__lnb_cntrl_1_i, &tmp);
		break;
	default:
		return AVL_EC_GENERAL_FAIL;
	}

	*peVal = (AVL62X1_GPIO_Pin_Value)tmp;
	return (r);
}

/************************************************************************/
/* BlindScan                                                            */
/************************************************************************/
//Start the blind scan operation on a single tuner step
AVL_ErrorCode AVL62X1_BlindScan_Start(struct AVL62X1_BlindScanParams * pBSParams, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 samp_rate_Hz = 0;
	AVL_uint16 samp_rate_ratio = 0;

	//r |= IBase_SendRxOP_AVL62X1(CMD_ACQUIRE, pAVL_Chip);

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_sample_rate_Hz_iaddr, &samp_rate_Hz);
	if(samp_rate_Hz == 0)
	{
		samp_rate_ratio = (1<<15);
	}
	else
	{
		//TunerLPF is single-sided, ratio based on double-sided
		samp_rate_ratio = (AVL_uint16)(((AVL_uint32)(2*pBSParams->m_TunerLPF_100kHz)*(1<<15)) / (samp_rate_Hz/100000));
	}

	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_nom_carrier_freq_Hz_iaddr, 0);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_ConfiguredPLScramKey_iaddr, AVL62X1_PL_SCRAM_AUTO);
	r |= II2C_Write16_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_BWfilt_to_Rsamp_ratio_saddr, samp_rate_ratio);
	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_search_range_percent_caddr, 90); //TODO
	r |= II2C_Write16_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_blind_scan_min_sym_rate_kHz_saddr, pBSParams->m_MinSymRate_kHz);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_bs_cent_freq_tuner_Hz_iaddr, pBSParams->m_TunerCenterFreq_100kHz*100*1000);

	r |= IBase_SendRxOP_AVL62X1(CMD_BLIND_SCAN, pAVL_Chip);

	return (r);
}

//Cancel blind scan process
AVL_ErrorCode AVL62X1_BlindScan_Cancel(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= IBase_SendRxOP_AVL62X1(CMD_HALT, pAVL_Chip);

	return (r);
}

//Get the status of a currently-running blind scan operation (single tuner step)
AVL_ErrorCode AVL62X1_BlindScan_GetStatus(struct AVL62X1_BlindScanInfo * pBSInfo, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar tmp8 = 0;
	AVL_uchar tmp8_1 = 0;
	AVL_uint32 tmp32 = 0;
	enum AVL62X1_FunctionalMode func_mode;

	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_current_bs_pair_index_caddr, &tmp8);
	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_bs_num_carrier_candidates_caddr, &tmp8_1);
	if(tmp8_1 == 0)
	{
		pBSInfo->m_ScanProgress = 0;
	}
	else
	{
		pBSInfo->m_ScanProgress = (AVL_uchar)((100*(AVL_uint32)tmp8)/(AVL_uint32)tmp8_1);
	}

	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_bs_num_confirmed_carriers_caddr, &tmp8);
	if(tmp8 == 255) 
	{
		pBSInfo->m_NumCarriers = 0;
	} 
	else 
	{
		pBSInfo->m_NumCarriers = tmp8;
	}

	r |= IBase_GetFunctionMode_AVL62X1(&func_mode, pAVL_Chip);
	pBSInfo->m_BSFinished = (AVL_uchar)((func_mode == AVL62X1_FuncMode_Idle) && (tmp8 != 255)); //idle and num confirmed carriers not init value

	pBSInfo->m_NumStreams = 0; //DEPRECATED

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_bs_next_start_freq_Hz_iaddr, &tmp32);
	pBSInfo->m_NextFreqStep_Hz = tmp32;

	return (r);
}

AVL_ErrorCode AVL62X1_BlindScan_GetCarrierList(const struct AVL62X1_BlindScanParams *pBSParams, struct AVL62X1_BlindScanInfo * pBSInfo, struct AVL62X1_CarrierInfo * pCarriers, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar carrier = 0;
	AVL_uchar tmp8 = 0;
	AVL_uint16 tmp16 = 0;
	AVL_uint32 carrier_list_ptr = 0;
	AVL_uint32 tmp32 = 0;

	AVL62X1_BlindScan_GetStatus(pBSInfo, pAVL_Chip);
	if(pBSInfo->m_BSFinished == 0) 
	{
		r = AVL_EC_RUNNING;
		return (r);
	}

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_bs_carrier_list_address_iaddr, &carrier_list_ptr);

	for(carrier=0; carrier<pBSInfo->m_NumCarriers; carrier++) 
	{
		//TODO: this could probably be optimized to a single I2C burst read
		pCarriers[carrier].m_carrier_index = carrier;
		pCarriers[carrier].m_roll_off = AVL62X1_RollOff_UNKNOWN;

		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_CarrierFreqHz_iaddr, &tmp32);
		pCarriers[carrier].m_carrier_freq_offset_Hz = 0;
		//pCarriers[carrier].m_rf_freq_kHz = (AVL_uint32)(pBSParams->m_TunerCenterFreq_100kHz*100 + (AVL_int32)tmp32/1000.0f + 0.5);
		pCarriers[carrier].m_rf_freq_kHz = (AVL_uint32)(pBSParams->m_TunerCenterFreq_100kHz*100 + (AVL_int32)(tmp32+ 500)/1000);

		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_SymbolRateHz_iaddr, &tmp32);
		pCarriers[carrier].m_symbol_rate_Hz = tmp32;

		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_SignalType_caddr, &tmp8);
		pCarriers[carrier].m_signal_type = (AVL62X1_Standard)tmp8;

		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_PLScramKeyMSBs_caddr, &tmp8);
		pCarriers[carrier].m_PL_scram_key = tmp8;
		pCarriers[carrier].m_PL_scram_key <<= 16;

		r |= II2C_Read16_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_PLScramKeyLSBs_saddr, &tmp16);
		pCarriers[carrier].m_PL_scram_key |= tmp16;

		pCarriers[carrier].m_num_streams = 0;
		pCarriers[carrier].m_SIS_MIS = AVL62X1_SIS_MIS_UNKNOWN;

		r |= II2C_Read16_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_SNR_dB_x100_saddr, &tmp16);
		pCarriers[carrier].m_SNR_dB_x100 = tmp16;

		if(pCarriers[carrier].m_signal_type == AVL62X1_DVBS2) 
		{
			r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_PLS_ACM_caddr, &tmp8);
			pCarriers[carrier].m_PLS_ACM = tmp8;
			pCarriers[carrier].m_dvbs2_ccm_acm = (AVL62X1_DVBS2_CCMACM)(pCarriers[carrier].m_PLS_ACM != 0);

			r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_Mod_caddr, &tmp8);
			pCarriers[carrier].m_modulation = (AVL62X1_ModulationMode)tmp8;

			r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_Pilot_caddr, &tmp8);
			pCarriers[carrier].m_pilot = (AVL62X1_Pilot)tmp8;

			r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_FECLen_caddr, &tmp8);
			pCarriers[carrier].m_dvbs2_fec_length = (AVL62X1_DVBS2_FECLength)tmp8;

			r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_CodeRate_caddr, &tmp8);
			pCarriers[carrier].m_coderate.m_dvbs2_code_rate = (AVL62X1_DVBS2_CodeRate)tmp8;
		} 
		else if(pCarriers[carrier].m_signal_type == AVL62X1_DVBS)
		{
			pCarriers[carrier].m_PLS_ACM = 16; //QPSK 1/2
			pCarriers[carrier].m_dvbs2_ccm_acm = AVL62X1_DVBS2_CCM;
			pCarriers[carrier].m_modulation = AVL62X1_QPSK;
			pCarriers[carrier].m_pilot = AVL62X1_Pilot_OFF;

			r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, carrier_list_ptr + carrier*AVL62X1_SAT_CARRIER_struct_size + AVL62X1_SAT_CARRIER_CodeRate_caddr, &tmp8);
			pCarriers[carrier].m_coderate.m_dvbs_code_rate = (AVL62X1_DVBS_CodeRate)tmp8;
		}        
	} //for carriers

	return (r);
}

AVL_ErrorCode AVL62X1_BlindScan_GetStreamList(struct AVL62X1_CarrierInfo *pCarrier, struct AVL62X1_StreamInfo *pStreams, const AVL_uchar max_num_streams, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar tmp8;

	r |= AVL62X1_GetStreamNumber(&tmp8, pAVL_Chip);
	if(pCarrier != nullptr)
	{
		pCarrier->m_num_streams = tmp8;
	}

	r |= AVL62X1_GetStreamList(pStreams, max_num_streams, pAVL_Chip);

	if(pCarrier != nullptr)
	{
		r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_SP_S2X_sp_SIS_MIS_caddr, &tmp8);
		pCarrier->m_SIS_MIS = (AVL62X1_SIS_MIS)(tmp8);
	}

	return (r);
}

AVL_ErrorCode AVL62X1_BlindScan_ConfirmCarrier(const struct AVL62X1_BlindScanParams *pBSParams, struct AVL62X1_CarrierInfo * pCarrierInfo, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_int32 cfo_Hz;

	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_blind_sym_rate_enable_caddr, 0);
	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_blind_cfo_enable_caddr, 0);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_nom_symbol_rate_Hz_iaddr, pCarrierInfo->m_symbol_rate_Hz);

	//back-calculate CFO from BS RF freq result
	cfo_Hz = ((AVL_int32)(pCarrierInfo->m_rf_freq_kHz) - (AVL_int32)(pBSParams->m_TunerCenterFreq_100kHz)*100)*1000;
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_nom_carrier_freq_Hz_iaddr, (AVL_uint32)(cfo_Hz));

	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_ConfiguredPLScramKey_iaddr, pCarrierInfo->m_PL_scram_key);
	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_S2X_ConfiguredStreamType_caddr, AVL62X1_UNDETERMINED);
	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_S2X_sp_wanted_stream_id_caddr, 0);

	r |= IBase_SendRxOP_AVL62X1(CMD_ACQUIRE, pAVL_Chip);
	return (r);
}

//When calling this function in blindscan mode, either set
//  pTuner->ucBlindScanMode = 1, pCarrierInfo = nullptr, or pCarrierInfo->m_symbol_rate_Hz = 0xFFFFFFFF
AVL_ErrorCode AVL62X1_Optimize_Carrier(struct AVL_Tuner *pTuner, struct AVL62X1_CarrierInfo * pCarrierInfo)
{
	AVL_ErrorCode r = AVL_EC_OK;
	const AVL_uint32 sym_rate_error_Hz = 5*1000*1000;
	AVL_uint32 carrier_BW_Hz = 0;
	AVL_uint32 maxLPF_Hz = 0;
	AVL_uint32 minLPF_Hz = 0;
	AVL_int32 IF_Hz = 0;
	AVL_uint32 tuner_step_size_Hz = 0;
	AVL_uint32 LPF_step_size_Hz = 0;

	if(pTuner->fpGetRFFreqStepSize == nullptr)
	{
		tuner_step_size_Hz = 250000;
	}
	else
	{
		pTuner->fpGetRFFreqStepSize(pTuner, &tuner_step_size_Hz);
	}
	if(pTuner->fpGetMaxLPF == nullptr)
	{
		maxLPF_Hz = 34000000;
	}
	else
	{
		pTuner->fpGetMaxLPF(pTuner, &maxLPF_Hz);
	}
	if(pTuner->fpGetMinLPF == nullptr)
	{
		minLPF_Hz = 10000000;
	}
	else
	{
		pTuner->fpGetMinLPF(pTuner, &minLPF_Hz);
	}
	if(pTuner->fpGetLPFStepSize == nullptr)
	{
		LPF_step_size_Hz = 1000000;
	}
	else
	{
		pTuner->fpGetLPFStepSize(pTuner, &LPF_step_size_Hz);
	}

	if(pCarrierInfo == nullptr)
	{
		pTuner->ucBlindScanMode = 1;
	}
	else if(pCarrierInfo->m_symbol_rate_Hz == 0xFFFFFFFF)
	{
		pTuner->ucBlindScanMode = 1;
	}
	if(pTuner->ucBlindScanMode == 1)
	{
		//Set tuner LPF wide open
		pTuner->uiLPFHz = maxLPF_Hz;
	}
	else
	{
		//double-sided carrier BW
		carrier_BW_Hz = (pCarrierInfo->m_symbol_rate_Hz/100)*135; //35% rolloff
		carrier_BW_Hz += sym_rate_error_Hz + tuner_step_size_Hz/2;

		if(pCarrierInfo->m_symbol_rate_Hz < AVL62X1_IF_SHIFT_MAX_SR_HZ)
		{
			//remove apriori CFO
			pCarrierInfo->m_rf_freq_kHz += pCarrierInfo->m_carrier_freq_offset_Hz/1000;
			//adjust by IF
			IF_Hz = (carrier_BW_Hz/2);
			pCarrierInfo->m_rf_freq_kHz -= IF_Hz/1000;
			pCarrierInfo->m_carrier_freq_offset_Hz = IF_Hz;
			pTuner->uiRFFrequencyHz = pCarrierInfo->m_rf_freq_kHz*1000;
			carrier_BW_Hz *= 2;
		}

		iCarrierFreqOffSet = pCarrierInfo->m_carrier_freq_offset_Hz;
		//Set tuner LPF so that carrier edge is an octave from the LPF 3dB point
		pTuner->uiLPFHz = Min32_AVL62X1(carrier_BW_Hz + LPF_step_size_Hz/2, maxLPF_Hz);
		pTuner->uiLPFHz = Max32_AVL62X1(pTuner->uiLPFHz, minLPF_Hz);
	}

	return (r);
}
