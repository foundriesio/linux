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

#include "AVL62X1_DVBSx.h"

AVL_int32 iCarrierFreqOffSet = 0;

AVL_ErrorCode Init_AVL62X1_ChipObject(struct AVL62X1_Chip * pAVL_ChipObject)
{
	AVL_ErrorCode r = AVL_EC_OK;

	pAVL_ChipObject->m_Diseqc_OP_Status = AVL62X1_DOS_Uninitialized;
	r = AVL_IBSP_InitSemaphore(&(pAVL_ChipObject->m_semRx));
	r |= AVL_IBSP_InitSemaphore(&(pAVL_ChipObject->m_semDiseqc));

	r |= AVL_II2C_Initialize(); // there is a internal protection to assure it will be initialized only once.

	return (r);
}

AVL_ErrorCode IBase_CheckChipReady_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 uiCoreReadyWord = 0;
	AVL_uint32 uiCoreRunning = 0;

	r = II2C_Read32_AVL62X1((AVL_uint16)(pAVL_Chip->usI2CAddr), hw_AVL62X1_c306_top_srst, &uiCoreRunning);
	r |= II2C_Read32_AVL62X1((AVL_uint16)(pAVL_Chip->usI2CAddr), rs_AVL62X1_core_ready_word, &uiCoreReadyWord);
	if( (AVL_EC_OK == r) )
	{
		if((1 == uiCoreRunning) || (uiCoreReadyWord != 0x5AA57FF7))
		{
			r = AVL_EC_GENERAL_FAIL;
		}
	}

	return (r);
}

//download and boot firmware, load default configuration, read back clock frequencies
AVL_ErrorCode IBase_Initialize_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= IBase_DownloadPatch_AVL62X1(pAVL_Chip);

	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_mpeg_ref_clk_Hz_iaddr, pAVL_Chip->m_MPEGFrequency_Hz);
	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_DMD_xtal_frequency_caddr, (AVL_uchar)(pAVL_Chip->e_Xtal));

	//load defaults command will load S2X defaults and program PLL based on XTAL and MPEG ref clk configurations
	r |= IBase_SendRxOP_AVL62X1(CMD_LD_DEFAULT, pAVL_Chip);

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_fec_clk_Hz_iaddr, &(pAVL_Chip->m_FECFrequency_Hz));
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_sys_clk_Hz_iaddr, &(pAVL_Chip->m_CoreFrequency_Hz));
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_mpeg_ref_clk_Hz_iaddr, &(pAVL_Chip->m_MPEGFrequency_Hz));

	return (r);
}

AVL_ErrorCode IBase_GetVersion_AVL62X1(struct AVL62X1_VerInfo * pVer_info, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 uiTemp = 0;
	AVL_uchar ucBuff[4] = {0};

	r = II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, 0x40000, &uiTemp);
	pVer_info->m_Chip = uiTemp;

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_DMD_patch_ver_iaddr, &uiTemp);
	Chunk32_AVL62X1(uiTemp, ucBuff);
	pVer_info->m_Patch.m_Major = ucBuff[0];
	pVer_info->m_Patch.m_Minor = ucBuff[1];
	pVer_info->m_Patch.m_Build = ucBuff[2];
	pVer_info->m_Patch.m_Build = (AVL_uint16)((pVer_info->m_Patch.m_Build)<<8) + ucBuff[3];

	pVer_info->m_API.m_Major = AVL62X1_API_VER_MAJOR;
	pVer_info->m_API.m_Minor = AVL62X1_API_VER_MINOR;
	pVer_info->m_API.m_Build = AVL62X1_API_VER_BUILD;

	return (r);
}

AVL_ErrorCode IBase_GetFunctionMode_AVL62X1(enum AVL62X1_FunctionalMode * pFuncMode, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar ucTemp = 0;

	r = II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_active_demod_mode_caddr, &ucTemp);
	*pFuncMode = (AVL62X1_FunctionalMode)ucTemp;

	return (r);
}

AVL_ErrorCode IBase_GetRxOPStatus_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK; 
	AVL_uint16 uiCmd = 0;

	r = II2C_Read16_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_DMD_command_saddr, &uiCmd);
	if( AVL_EC_OK == r )
	{
		if( CMD_IDLE != uiCmd )
		{
			r = AVL_EC_RUNNING;
		}
		else if( CMD_FAILED == uiCmd)
		{
			r = AVL_EC_COMMAND_FAILED;
		}
	}

	return (r);
}

AVL_ErrorCode IBase_SendRxOP_AVL62X1(AVL_uchar ucOpCmd, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar pucBuff[4] = {0};
	AVL_uint16 uiTemp = 0;
	const AVL_uint16 uiTimeDelay = 10;
	const AVL_uint16 uiMaxRetries = 50;//the time out window is 25*20 = 500ms
	AVL_uint32 i = 0;

	r = AVL_IBSP_WaitSemaphore(&(pAVL_Chip->m_semRx));

	while (AVL_EC_OK != IBase_GetRxOPStatus_AVL62X1(pAVL_Chip))
	{
		if (uiMaxRetries < i++)
		{
			r |= AVL_EC_RUNNING;
			break;
		}
		AVL_IBSP_Delay(uiTimeDelay);
	}
	if( AVL_EC_OK == r )
	{
		pucBuff[0] = 0;
		pucBuff[1] = ucOpCmd;
		uiTemp = DeChunk16_AVL62X1(pucBuff);
		r |= II2C_Write16_AVL62X1((AVL_uint16)(pAVL_Chip->usI2CAddr), c_AVL62X1_DMD_command_saddr, uiTemp);
	}

	i = 0;
	while (AVL_EC_OK != IBase_GetRxOPStatus_AVL62X1(pAVL_Chip))
	{
		if (uiMaxRetries < i++)
		{
			r |= AVL_EC_RUNNING;
			break;
		}
		AVL_IBSP_Delay(uiTimeDelay);
	}

	r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semRx));

	return (r);
}

AVL_ErrorCode IBase_GetSPOPStatus_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK; 
	AVL_uint16 uiCmd = 0;

	r = II2C_Read16_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_sp_command_saddr, &uiCmd);
	if( AVL_EC_OK == r )
	{
		if( CMD_IDLE != uiCmd )
		{
			r = AVL_EC_RUNNING;
		}
		else if( CMD_FAILED == uiCmd)
		{
			r = AVL_EC_COMMAND_FAILED;
		}
	}

	return (r);
}

AVL_ErrorCode IBase_SendSPOP_AVL62X1(AVL_uchar ucOpCmd, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar pucBuff[4] = {0};
	AVL_uint16 uiTemp = 0;
	const AVL_uint16 uiTimeDelay = 10;
	const AVL_uint16 uiMaxRetries = 50;//the time out window is 10*50 = 500ms
	AVL_uint32 i = 0;

	r = AVL_IBSP_WaitSemaphore(&(pAVL_Chip->m_semRx));

	while (AVL_EC_OK != IBase_GetSPOPStatus_AVL62X1(pAVL_Chip))
	{
		if (uiMaxRetries < i++)
		{
			r |= AVL_EC_RUNNING;
			break;
		}
		AVL_IBSP_Delay(uiTimeDelay);
	}
	if( AVL_EC_OK == r )
	{
		pucBuff[0] = 0;
		pucBuff[1] = ucOpCmd;
		uiTemp = DeChunk16_AVL62X1(pucBuff);
		r |= II2C_Write16_AVL62X1((AVL_uint16)(pAVL_Chip->usI2CAddr), c_AVL62X1_SP_sp_command_saddr, uiTemp);
	}

	i = 0;
	while (AVL_EC_OK != IBase_GetSPOPStatus_AVL62X1(pAVL_Chip))
	{
		if (uiMaxRetries < i++)
		{
			r |= AVL_EC_RUNNING;
			break;
		}
		AVL_IBSP_Delay(uiTimeDelay);
	}

	r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semRx));

	return (r);
}

AVL_ErrorCode IBase_Halt_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= IBase_SendRxOP_AVL62X1(CMD_HALT, pAVL_Chip);

	return (r);
}

AVL_ErrorCode IBase_Sleep_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= IBase_SendRxOP_AVL62X1(CMD_SLEEP, pAVL_Chip);

	return (r);
}

AVL_ErrorCode IBase_Wakeup_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= IBase_SendRxOP_AVL62X1(CMD_WAKE, pAVL_Chip);

	return (r);
}

AVL_ErrorCode IBase_Initialize_TunerI2C_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	//AVL_uint32 uiSetMode = 2;  //bit mode
	AVL_uint32 uiTemp = 0;
	AVL_uint32 bit_rpt_divider = 0;

	//reset tuner i2c block
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_i2c_srst, 1);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_i2c_srst, 0);

	//tuner_i2c_cntrl: {rpt_addr[23:16],...,i2c_mode[8],...,src_sel[0]}
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_i2c_cntrl, &uiTemp);
	uiTemp = (uiTemp&0xFFFFFFFE);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_i2c_cntrl, uiTemp);

	//hw_i2c_bit_rpt_cntrl: {doubleFFen, stop_check, rpt_sel, rpt_enable}
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_hw_i2c_bit_rpt_cntrl, 0x6);

	bit_rpt_divider = (0x2A)*(pAVL_Chip->m_CoreFrequency_Hz/1000)/(240*1000);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, psc_tuner_i2c__tuner_hw_i2c_bit_rpt_clk_div, bit_rpt_divider);

	//configure GPIO
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__i2c_clk2_sel, 7); //M3_SCL
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__i2c_data2_sel, 8); //M3_SDA
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__m3_scl_sel, 6); //I2C_CLK2
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, gpio_debug__m3_sda_sel, 5); //I2C_DATA2

	return (r);
}

AVL_ErrorCode IRx_Initialize_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_input_format_caddr, 0x0);  // 0: 2's complement, 1: offset binary
	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_input_select_caddr, 0x1);  //0: Digital, 1: ADC in

	return (r);
}

AVL_ErrorCode IRx_GetTunerPola_AVL62X1(enum AVL62X1_SpectrumPolarity *pTuner_Pol, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar uiTemp = 0;

	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_carrier_spectrum_invert_status_caddr, &uiTemp);
	*pTuner_Pol = (enum AVL62X1_SpectrumPolarity)uiTemp;

	return (r);
}

AVL_ErrorCode IRx_SetTunerPola_AVL62X1(enum AVL62X1_SpectrumPolarity enumTuner_Pol, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_tuner_spectrum_invert_caddr, (AVL_uchar)enumTuner_Pol);

	return (r);
}


//the FW (versions >= 24739) supports setting the PLS seed manually.
AVL_ErrorCode IRx_ConfigPLS_AVL62X1(AVL_uint32 uiShiftValue, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 uiTemp = 0;
	r = II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_ConfiguredPLScramKey_iaddr, &uiTemp);
    uiTemp  &=  (!(1<<23)); //bit[23] eq 0 -> Manually configure scramble code according to bits [18:0]
    uiTemp  &=  (!(1<<18));//bit[18] eq 0 -> bits[17:0] represent the sequence shift of the x(i) sequence in the Gold code as defined as the "code number n" in the DVB-S2 standard
    uiTemp  |=  0x3FFFF;
	uiTemp  &=  uiShiftValue;
	r = II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_ConfiguredPLScramKey_iaddr, uiTemp);
	return (r);
}

AVL_ErrorCode IRx_DriveAGC_AVL62X1(enum AVL62X1_Switch enumOn_Off, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 uiTemp = 0;
	AVL_int32 rfagc_slope = 0;
	AVL_uint32 min_gain = 0;
	AVL_uint32 max_gain = 0;
	AVL_uint32 min_gain_mV = 0;
	AVL_uint32 max_gain_mV = 0;

	if (AVL62X1_ON == enumOn_Off)
	{
		//set RF AGC polarity according to AGC slope sign
		if(pAVL_Chip->pTuner->fpGetAGCSlope == nullptr)
		{
			rfagc_slope = -1;
		}
		else
		{
			pAVL_Chip->pTuner->fpGetAGCSlope(pAVL_Chip->pTuner, &rfagc_slope);
		}
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_S2X_rf_agc_pol_caddr, (AVL_uchar)(rfagc_slope < 0));

		//set min and max gain values according to AGC saturation points
		if(pAVL_Chip->pTuner->fpGetMinGainVoltage == nullptr || pAVL_Chip->pTuner->fpGetMaxGainVoltage == nullptr)
		{
			//set some reasonable defaults
			if(rfagc_slope > 0)
			{
				min_gain_mV = 100;
				max_gain_mV = 3200;
			}
			else
			{
				min_gain_mV = 3200;
				max_gain_mV = 100;
			}
		}
		else
		{
			pAVL_Chip->pTuner->fpGetMinGainVoltage(pAVL_Chip->pTuner, &min_gain_mV);
			pAVL_Chip->pTuner->fpGetMaxGainVoltage(pAVL_Chip->pTuner, &max_gain_mV);
			min_gain_mV = AVL_min(min_gain_mV, 3300);
			max_gain_mV = AVL_min(max_gain_mV, 3300);
		}

		if(rfagc_slope > 0)
		{
			min_gain = (min_gain_mV*(1<<16))/3300;
			max_gain = (max_gain_mV*(1<<16))/3300;
		}
		else
		{
			min_gain = ((3300 - min_gain_mV)*(1<<16))/3300;
			max_gain = ((3300 - max_gain_mV)*(1<<16))/3300;
		}

		min_gain = AVL_min(min_gain, 0xFFFF);
		max_gain = AVL_min(max_gain, 0xFFFF);

		r |= II2C_Write16_AVL62X1((pAVL_Chip->usI2CAddr), c_AVL62X1_S2X_rf_agc_min_gain_saddr, min_gain);
		r |= II2C_Write16_AVL62X1((pAVL_Chip->usI2CAddr), c_AVL62X1_S2X_rf_agc_max_gain_saddr, max_gain);

		//enable sigma delta output
		r |= II2C_Read32_AVL62X1((pAVL_Chip->usI2CAddr), aagc__analog_agc_sd_control_reg, &uiTemp);
		uiTemp |= (0x1<<0x1); //agc_sd_on bit
		r |= II2C_Write32_AVL62X1((pAVL_Chip->usI2CAddr), aagc__analog_agc_sd_control_reg, uiTemp);

		//configure GPIO
		r |= II2C_Write32_AVL62X1((pAVL_Chip->usI2CAddr), gpio_debug__agc1_sel, 6);
		r |= II2C_Write32_AVL62X1((pAVL_Chip->usI2CAddr), gpio_debug__agc2_sel, 6);
	}
	else if (AVL62X1_OFF == enumOn_Off)
	{
		r |= II2C_Read32_AVL62X1((pAVL_Chip->usI2CAddr), aagc__analog_agc_sd_control_reg, &uiTemp);
		uiTemp &= ~(0x1<<0x1); //agc_sd_on bit
		r |= II2C_Write32_AVL62X1((pAVL_Chip->usI2CAddr), aagc__analog_agc_sd_control_reg, uiTemp);

		//configure GPIO
		r |= II2C_Write32_AVL62X1((pAVL_Chip->usI2CAddr), gpio_debug__agc1_sel, 2); //high-Z
		r |= II2C_Write32_AVL62X1((pAVL_Chip->usI2CAddr), gpio_debug__agc2_sel, 2); //high-Z
	}
	else
	{
		r |= AVL_EC_GENERAL_FAIL;
	}
	return (r);
}

AVL_ErrorCode IRx_GetCarrierFreqOffset_AVL62X1(AVL_pint32 piFreqOffsetHz, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 uiTemp = 0;

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_carrier_freq_Hz_iaddr, &uiTemp);
	*piFreqOffsetHz = (AVL_int32)uiTemp;

	
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_carrier_freq_err_Hz_iaddr, &uiTemp);
	*piFreqOffsetHz += (AVL_int32)uiTemp;
	*piFreqOffsetHz -= iCarrierFreqOffSet;

	return (r);
}

AVL_ErrorCode IRx_GetSROffset_AVL62X1(AVL_pint32 piSROffsetPPM, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_int32 sr_error = 0;
	AVL_uint32 sr = 0;

	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_symbol_rate_Hz_iaddr, &sr);
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_symbol_rate_error_Hz_iaddr, (AVL_puint32)&sr_error);

	//*piSROffsetPPM = (AVL_int32)(((double)sr_error * 1e6) / (double)sr);
	*piSROffsetPPM = (AVL_int32)(((unsigned long)sr_error * (unsigned long)1e6) / (unsigned long)sr);
	return (r);
}

AVL_ErrorCode IRx_ErrorStatMode_AVL62X1(struct AVL62X1_ErrorStatConfig stErrorStatConfig, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL62X1_uint64 time_tick_num = {0,0};

	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__auto1_manual0_mode__offset, (AVL_uint32)stErrorStatConfig.eErrorStatMode);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr,ber_esm__timetick1_bytetick0__offset, (AVL_uint32)stErrorStatConfig.eAutoErrorStatType);

	Multiply32_AVL62X1(&time_tick_num, pAVL_Chip->m_MPEGFrequency_Hz/1000, stErrorStatConfig.uiTimeThresholdMs);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__time_tick_low__offset, time_tick_num.uiLowWord);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__time_tick_high__offset, time_tick_num.uiHighWord);

	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__byte_tick_low__offset, (AVL_uint32)stErrorStatConfig.uiNumberThresholdByte);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__byte_tick_high__offset, 0); //high 32-bit is not used

	if (stErrorStatConfig.eErrorStatMode == AVL62X1_ERROR_STAT_AUTO)
	{
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__tick_clear_req__offset, 0);
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__tick_clear_req__offset, 1);
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__tick_clear_req__offset, 0);
	}

	return (r);
}

struct AVL62X1_ErrorStatus AVL62X1_esm;

AVL_ErrorCode IRx_ResetBER_AVL62X1(struct AVL62X1_BERConfig *pBERConfig, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 uiTemp = 0;
	AVL_uint16 uiLFSR_Sync = 0;
	AVL_uint32 uiCnt = 0;
	AVL_uint32 uiByteCnt = 0;
	AVL_uint32 uiBER_FailCnt = 0;
	AVL_uint32 uiBitErrors = 0;

	AVL62X1_esm.m_LFSR_Sync = 0;
	AVL62X1_esm.m_LostLock = 0;
	AVL62X1_esm.m_SwCntNumBits.uiHighWord = 0;
	AVL62X1_esm.m_SwCntNumBits.uiLowWord = 0;
	AVL62X1_esm.m_SwCntBitErrors.uiHighWord = 0;
	AVL62X1_esm.m_SwCntBitErrors.uiLowWord = 0;
	AVL62X1_esm.m_NumBits.uiHighWord = 0;
	AVL62X1_esm.m_NumBits.uiLowWord = 0;
	AVL62X1_esm.m_BitErrors.uiHighWord = 0;
	AVL62X1_esm.m_BitErrors.uiLowWord = 0;
	AVL62X1_esm.m_BER = 0;

	//ber software reset
	r = II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, &uiTemp);
	uiTemp |= 0x00000002;
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);

	//alway inverted
	pBERConfig->eBERFBInversion = AVL62X1_LFSR_FB_INVERTED;

	//set Test Pattern and Inversion
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, &uiTemp);
	uiTemp &= 0xFFFFFFCF;
	//BER_Test_Pattern:bit 5 --- 0:LFSR_15; 1:LFSR_23
	//BER_FB_Inversion:bit 4 --- 0:NOT_INVERTED; 1:INVERTED
	uiTemp |= ((AVL_uint32)(pBERConfig->eBERTestPattern) << 5) | ((AVL_uint32)(pBERConfig->eBERFBInversion) << 4);
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);

	uiTemp &= 0xFFFFFE3F;
	uiTemp |= ((pBERConfig->uiLFSRStartPos)<<6);//For SFU or other standard, the start position of LFSR is 1, just follow the 0x47 sync word
	r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);

	while(!uiLFSR_Sync)
	{
		uiTemp |= 0x00000006;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);
		uiTemp &= 0xFFFFFFFD;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);

		uiCnt = 0;
		uiByteCnt = 0;
		while((uiByteCnt < 1000) && (uiCnt < 200))
		{
			r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__byte_num, &uiByteCnt);
			uiCnt++;
		}

		uiTemp |= 0x00000006;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);
		uiTemp &= 0xFFFFFFF9;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);

		uiCnt = 0;
		uiByteCnt = 0;
		while((uiByteCnt < 10000) && (uiCnt < 200))
		{
			uiCnt++;
			r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__byte_num, &uiByteCnt);
		}

		uiTemp &= 0xFFFFFFF9;
		uiTemp |= 0x00000002;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);

		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__byte_num, &uiByteCnt);
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__ber_err_cnt, &uiBitErrors);
		if(uiCnt == 200)
		{
			break;
		}
		else if((uiByteCnt << 3) < (10 * uiBitErrors))
		{
			uiBER_FailCnt++;
			if(uiBER_FailCnt > 10)
			{
				break;
			}
		}
		else
		{
			uiLFSR_Sync = 1;
		}
	}

	if(uiLFSR_Sync == 1)
	{
		uiTemp &= 0xFFFFFFF9;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);
	}

	pBERConfig->uiLFSRSynced = uiLFSR_Sync;
	AVL62X1_esm.m_LFSR_Sync = uiLFSR_Sync;

	return (r);
}

AVL_ErrorCode IRx_GetBER_AVL62X1(AVL_puint32 puiBER_x1e9, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 uiHwCntBitErrors = 0;
	AVL_uint32 uiHwCntNumBits = 0;
	AVL_uint32 uiTemp = 0;
	AVL62X1_uint64 uiTemp64;

	r = II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__ber_err_cnt, &uiHwCntBitErrors);
	r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__byte_num, &uiHwCntNumBits);
	uiHwCntNumBits <<= 3;

	//Keep the hw counts into sw struct to avoid hw registers overflow
	if(uiHwCntNumBits > (AVL_uint32)(1 << 31))
	{
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, &uiTemp);
		uiTemp |= 0x00000002;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__ber_err_cnt, &uiHwCntBitErrors);
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__byte_num, &uiHwCntNumBits);
		uiTemp &= 0xFFFFFFFD;
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ber_esm__esm_cntrl, uiTemp);
		uiHwCntNumBits <<= 3;
		Add32To64_AVL62X1(&AVL62X1_esm.m_SwCntNumBits, uiHwCntNumBits);
		Add32To64_AVL62X1(&AVL62X1_esm.m_SwCntBitErrors, uiHwCntBitErrors);
		uiHwCntNumBits = 0;
		uiHwCntBitErrors = 0;
	}

	AVL62X1_esm.m_NumBits.uiHighWord = AVL62X1_esm.m_SwCntNumBits.uiHighWord;
	AVL62X1_esm.m_NumBits.uiLowWord = AVL62X1_esm.m_SwCntNumBits.uiLowWord;
	Add32To64_AVL62X1(&AVL62X1_esm.m_NumBits, uiHwCntNumBits);
	AVL62X1_esm.m_BitErrors.uiHighWord = AVL62X1_esm.m_SwCntBitErrors.uiHighWord;
	AVL62X1_esm.m_BitErrors.uiLowWord = AVL62X1_esm.m_SwCntBitErrors.uiLowWord;
	Add32To64_AVL62X1(&AVL62X1_esm.m_BitErrors, uiHwCntBitErrors);

	// Compute the BER, this BER is multiplied with 1e9 because the float operation is not supported
	Multiply32_AVL62X1(&uiTemp64, AVL62X1_esm.m_BitErrors.uiLowWord, AVL_CONSTANT_10_TO_THE_9TH);
	AVL62X1_esm.m_BER = Divide64_AVL62X1(AVL62X1_esm.m_NumBits, uiTemp64);

	//keep the BER user wanted
	*puiBER_x1e9 = AVL62X1_esm.m_BER;

	return (r);
}

AVL_ErrorCode IRx_GetAcqRetries_AVL62X1(AVL_puchar pucRetryNum, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr, s_AVL62X1_S2X_acq_retry_count_caddr, pucRetryNum);
	return (r);
}

AVL_ErrorCode IRx_SetMpegMode_AVL62X1( struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_sp_ts_serial_caddr, (AVL_uchar)(pAVL_Chip->e_Mode));
	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_sp_ts0_tsp1_caddr, (AVL_uchar)(pAVL_Chip->e_Format));
	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_sp_ts_clock_edge_caddr, (AVL_uchar)(pAVL_Chip->e_ClkPol));
	if (pAVL_Chip->e_Mode != AVL62X1_MPM_Parallel)
	{
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_sp_ts_serial_outpin_caddr, (AVL_uchar)(pAVL_Chip->e_SerPin));
		r |= IRx_EnableMpegContiClock_AVL62X1(pAVL_Chip);
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, (c_AVL62X1_SP_sp_mpeg_bus_misc_2_iaddr+2), 0x3); //enhance the MPEG driver ability.
	}

	return (r);
}

AVL_ErrorCode IRx_SetMpegBitOrder_AVL62X1(enum AVL62X1_MpegBitOrder e_BitOrder, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_sp_ts_serial_msb_caddr, (AVL_uchar)e_BitOrder);

	return (r);
}

AVL_ErrorCode IRx_SetMpegErrorPolarity_AVL62X1(enum AVL62X1_MpegPolarity e_ErrorPol, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_sp_ts_error_polarity_caddr, (AVL_uchar)e_ErrorPol);

	return (r);
}

AVL_ErrorCode IRx_SetMpegValidPolarity_AVL62X1(enum AVL62X1_MpegPolarity e_ValidPol, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_SP_sp_ts_valid_polarity_caddr, (AVL_uchar)e_ValidPol);

	return (r);
}

AVL_ErrorCode IRx_EnableMpegContiClock_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r = II2C_Write32_AVL62X1((AVL_uint16)(pAVL_Chip->usI2CAddr), c_AVL62X1_SP_sp_ts_cntns_clk_frac_n_iaddr, 1);
	r |= II2C_Write32_AVL62X1((AVL_uint16)(pAVL_Chip->usI2CAddr), c_AVL62X1_SP_sp_ts_cntns_clk_frac_d_iaddr, 1);
	r |= II2C_Write8_AVL62X1((AVL_uint16)(pAVL_Chip->usI2CAddr), c_AVL62X1_SP_sp_enable_ts_continuous_caddr, 1);

	return (r);
}

AVL_ErrorCode IRx_DisableMpegContiClock_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	r |= II2C_Write8_AVL62X1((AVL_uint16)(pAVL_Chip->usI2CAddr), c_AVL62X1_SP_sp_enable_ts_continuous_caddr, 0);

	return (r);
}

AVL_ErrorCode IRx_DriveMpegOutput_AVL62X1(enum AVL62X1_Switch enumOn_Off, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;

	if (AVL62X1_ON == enumOn_Off)
	{
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, ts_output_intf__mpeg_bus_off, 0x00);
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ts_output_intf__mpeg_bus_e_b, 0x00);
	}
	else if (AVL62X1_OFF == enumOn_Off)
	{
		r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr, ts_output_intf__mpeg_bus_off, 0xFF);
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, ts_output_intf__mpeg_bus_e_b, 0xFFF);
	}
	else
	{
		r |= AVL_EC_GENERAL_FAIL;
	}
	return (r);
}

AVL_ErrorCode IDiseqc_Initialize_AVL62X1(struct AVL62X1_Diseqc_Para *pDiseqcPara, struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 i1 = 0;

	r = AVL_IBSP_WaitSemaphore(&(pAVL_Chip->m_semDiseqc));
	if( AVL_EC_OK == r )
	{
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_srst, 1);
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_samp_frac_n, 2000000);
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_samp_frac_d, pAVL_Chip->m_CoreFrequency_Hz);

		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tone_frac_n, (pDiseqcPara->uiToneFrequencyKHz)<<1);
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tone_frac_d, (pAVL_Chip->m_CoreFrequency_Hz/1000));

		// Initialize the tx_control
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, &i1);
		i1 &= 0x00000300;
		i1 |= 0x20;     //reset tx_fifo
		i1 |= ((AVL_uint32)(pDiseqcPara->eTXGap) << 6);
		i1 |= ((AVL_uint32)(pDiseqcPara->eTxWaveForm) << 4);
		i1 |= (1<<3);           //enable tx gap.
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);
		i1 &= ~(0x20);  //release tx_fifo reset
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_cntrl, i1);

		// Initialize the rx_control
		i1 = ((AVL_uint32)(pDiseqcPara->eRxWaveForm) << 2);
		i1 |= (1<<1);   //active the receiver
		i1 |= (1<<3);   //envelop high when tone present
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_rx_cntrl, i1);
		i1 = (AVL_uint32)(pDiseqcPara->eRxTimeout);
		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__rx_msg_tim, i1);

		r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_srst, 0);

		if( AVL_EC_OK == r )
		{
			pAVL_Chip->m_Diseqc_OP_Status = AVL62X1_DOS_Initialized;
		}
	}
	r |= AVL_IBSP_ReleaseSemaphore(&(pAVL_Chip->m_semDiseqc));

	return (r);
}

AVL_ErrorCode IDiseqc_IsSafeToSwitchMode_AVL62X1(struct AVL62X1_Chip * pAVL_Chip)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint32 i1 = 0;

	switch( pAVL_Chip->m_Diseqc_OP_Status )
	{
	case AVL62X1_DOS_InModulation:
	case AVL62X1_DOS_InTone:
		r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, diseqc__diseqc_tx_st, &i1);
		if( 1 != ((i1 & 0x00000040) >> 6) ) //check if the last transmit is done
		{
			r |= AVL_EC_RUNNING;
		}
		break;
	case AVL62X1_DOS_InContinuous:
	case AVL62X1_DOS_Initialized:
		break;
	default:
		r |= AVL_EC_GENERAL_FAIL;
		break;
	}
	return (r);
}

AVL_ErrorCode IBase_DownloadPatch_AVL62X1(struct AVL62X1_Chip * pAVL_Chip) 
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_puchar pPatchData = 0;
	AVL_uint32 patch_idx = 0; 
	AVL_uint32 total_patch_len = 0;
	AVL_uint32 standard = 0;
	AVL_uint32 args_addr = 0;
	AVL_uint32 data_section_offset = 0;
	AVL_uint32 reserved_len = 0;
	AVL_uint32 script_len = 0;
	AVL_uchar unary_op = 0;
	AVL_uchar binary_op = 0;
	AVL_uchar addr_mode_op = 0;
	AVL_uint32 script_start_idx = 0;
	AVL_uint32 num_cmd_words = 0;
	AVL_uint32 next_cmd_idx = 0;
	AVL_uint32 num_cond_words = 0;
	AVL_uint32 condition = 0;
	AVL_uint32 operation =0;
	AVL_uint32 value = 0;
	//AVL_uint32 tmp_top_valid = 0;
	//AVL_uint32 core_rdy_word = 0;
	AVL_uint32 cmd = 0;
	AVL_uint32 num_rvs = 0;
	AVL_uint32 rv0_idx = 0;
	AVL_uint32 exp_crc_val = 0;
	AVL_uint32 start_addr = 0;
	AVL_uint32 crc_result = 0;
	AVL_uint32 length = 0; 
	AVL_uint32 dest_addr = 0;
	AVL_uint32 src_data_offset = 0;
	AVL_uint32 data = 0;
	AVL_uint16 data1 = 0;
	AVL_uchar data2 = 0;
	AVL_uint32 src_addr = 0;
	AVL_uint32 descr_addr = 0;
	AVL_uint32 num_descr = 0;
	AVL_uint32 type = 0;
	AVL_uint32 ref_addr = 0;
	AVL_uint32 ref_size = 0;
	AVL_uint32 ready = 0;
	AVL_uint32 dma_max_tries = 0;
	AVL_uint32 dma_tries = 0;
	AVL_uint32 rv = 0;
	AVL_char temp[3];
	AVL_puchar pPatchDatatemp = 0 ;
	AVL_puchar pPatchDatatemp1 = 0;
	AVL_uint32 cond = 0;
	//AVL_uint32 d=0;
	//	AVL_uint32 e = 0;
	AVL_uchar got_cmd_exit = 0;
	AVL_uint16 num_records = 0;
	AVL_uint16 record_length = 0;
	AVL_uint16 addr_offset = 0;
	AVL_uint16 record_cnt = 0;
	AVL_uint32 match_value = 0;
	AVL_uint32 max_polls = 0;
	AVL_uint32 polls = 0;

	pPatchData = pAVL_Chip->pPatchData;

	patch_idx = 4; //INDEX IS A BYTE OFFSET
	total_patch_len = patch_read32_AVL62X1(pPatchData, &patch_idx);
	standard = patch_read32_AVL62X1(pPatchData, &patch_idx);
	args_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);
	data_section_offset = patch_read32_AVL62X1(pPatchData, &patch_idx);
	reserved_len = patch_read32_AVL62X1(pPatchData, &patch_idx);
	patch_idx += 4*reserved_len; //skip over reserved area for now
	script_len = patch_read32_AVL62X1(pPatchData, &patch_idx);

	if((patch_idx/4 + script_len) != data_section_offset) 
	{
		r = AVL_EC_GENERAL_FAIL;
		return (r);
	}

	script_start_idx = patch_idx/4;

	while((patch_idx/4 <(script_start_idx+script_len)) && !got_cmd_exit)
	{
		num_cmd_words = patch_read32_AVL62X1(pPatchData, &patch_idx);
		next_cmd_idx = patch_idx + (num_cmd_words-1)*4; //BYTE OFFSET
		num_cond_words = patch_read32_AVL62X1(pPatchData, &patch_idx);

		if(num_cond_words == 0) 
		{
			condition = 1;
		}
		else
		{
			for( cond=0; cond<num_cond_words; cond++) 
			{
				operation = patch_read32_AVL62X1(pPatchData, &patch_idx);
				value = patch_read32_AVL62X1(pPatchData, &patch_idx);
				unary_op = (operation>>8) & 0xFF;
				binary_op = operation & 0xFF;
				addr_mode_op = ((operation>>16)&0x3);

				if( (addr_mode_op == PATCH_OP_ADDR_MODE_VAR_IDX) && (binary_op != PATCH_OP_BINARY_STORE)) 
				{ 
					value = pAVL_Chip->m_variable_array[value]; //grab variable value
				}

				switch(unary_op) 
				{
				case PATCH_OP_UNARY_LOGICAL_NEGATE:
					value = !value;
					break;
				case PATCH_OP_UNARY_BITWISE_NEGATE:
					value = ~value;
					break;
				case PATCH_OP_UNARY_BITWISE_AND:
					//value = FIXME
					break;
				case PATCH_OP_UNARY_BITWISE_OR:
					//value = FIXME
					break;
				}
				switch(binary_op) 
				{
				case PATCH_OP_BINARY_LOAD:
					condition = value;
					break;
				case PATCH_OP_BINARY_STORE:
					pAVL_Chip->m_variable_array[value] = condition;
					break;
				case PATCH_OP_BINARY_AND:
					condition = condition && value;
					break;
				case PATCH_OP_BINARY_OR:
					condition = condition || value;
					break;
				case PATCH_OP_BINARY_BITWISE_AND:
					condition = condition & value;
					break;
				case PATCH_OP_BINARY_BITWISE_OR:
					condition = condition | value;
					break;
				case PATCH_OP_BINARY_EQUALS:
					condition = condition == value;
					break;
				case PATCH_OP_BINARY_NOT_EQUALS:
					condition = condition != value;
					break;
				default:
					r = AVL_EC_GENERAL_FAIL;
					return (r);
				} 
			} //for conditions
		}

		if(condition) 
		{
			cmd = patch_read32_AVL62X1(pPatchData, &patch_idx);
			switch(cmd) 
			{
			case PATCH_CMD_PING: //1
				{
					r |= IBase_SendRxOP_AVL62X1(CMD_PING, pAVL_Chip);

					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					rv0_idx = patch_read32_AVL62X1(pPatchData, &patch_idx);
					pAVL_Chip->m_variable_array[rv0_idx] = (r == AVL_EC_OK);
					patch_idx += 4*(num_rvs - 1); //skip over any extraneous RV's
					break;
				}
			case PATCH_CMD_VALIDATE_CRC://0
				{
					exp_crc_val = patch_read32_AVL62X1(pPatchData, &patch_idx);
					start_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);
					length = patch_read32_AVL62X1(pPatchData, &patch_idx);

					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_DMD_fw_command_args_addr_iaddr, args_addr);
					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, args_addr+0, start_addr);
					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, args_addr+4, length);
					r |= IBase_SendRxOP_AVL62X1(CMD_CALC_CRC, pAVL_Chip);

					r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, args_addr+8,&crc_result);

					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					rv0_idx = patch_read32_AVL62X1(pPatchData, &patch_idx);
					pAVL_Chip->m_variable_array[rv0_idx] = (crc_result == exp_crc_val);

					patch_idx += 4*(num_rvs - 1); //skip over any extraneous RV's

					break;
				}
			case PATCH_CMD_LD_TO_DEVICE://2
				{
					length = patch_read32_AVL62X1(pPatchData, &patch_idx); //in words
					dest_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);
					src_data_offset = patch_read32_AVL62X1(pPatchData, &patch_idx);
					src_data_offset += data_section_offset; //add in base offset
					src_data_offset *= 4; //convert to byte offset
#define BURST
#ifdef BURST
					length *= 4; //Convert to byte length

					pPatchDatatemp = pPatchData + src_data_offset;
					pPatchDatatemp1 = pPatchDatatemp - 3;
					temp[0] = *(pPatchDatatemp -1);
					temp[1] = *(pPatchDatatemp -2);
					temp[2] = *(pPatchDatatemp -3);
					ChunkAddr_AVL62X1(dest_addr, pPatchDatatemp1);

					r |= II2C_Write_AVL62X1(pAVL_Chip->usI2CAddr, pPatchDatatemp1, (AVL_uint32)(length+3));

					* pPatchDatatemp1 = temp[2];
					*(pPatchDatatemp1+1) = temp[1];
					*(pPatchDatatemp1+2) = temp[0];

#else
					for(i=0; i<length; i++) 
					{
						//FIXME: make this a burst write
						tdata = patch_read32_AVL62X1(pPatchData, &src_data_offset);
						r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, dest_addr+i*4,tdata);
					}
#endif

					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					patch_idx += 4*(num_rvs); //no RV's defined yet

					break;
				}

			case PATCH_CMD_LD_TO_DEVICE_IMM://7
				{
					length = patch_read32_AVL62X1(pPatchData, &patch_idx); //in bytes
					dest_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);
					data = patch_read32_AVL62X1(pPatchData, &patch_idx);

					if(length == 4) 
					{
						r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr,dest_addr,data);
					} 
					else if(length == 2) 
					{
						r |= II2C_Write16_AVL62X1(pAVL_Chip->usI2CAddr,dest_addr,data);
					} 
					else if(length == 1) 
					{
						r |= II2C_Write8_AVL62X1(pAVL_Chip->usI2CAddr,dest_addr,data);
					}

					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					patch_idx += 4*(num_rvs); //no RV's defined yet
					break;
				}
			case PATCH_CMD_LD_TO_DEVICE_PACKED:
				{
					length = patch_read32_AVL62X1(pPatchData, &patch_idx); //in words
					src_data_offset = patch_read32_AVL62X1(pPatchData, &patch_idx);

					src_data_offset += data_section_offset; //add in base offset to make it absolute
					src_data_offset *= 4; //convert to byte offset
					length *= 4; //Convert to byte length

					src_data_offset += 2; //skip over address offset length. assume it's 2 for now!
					num_records = (pPatchData[src_data_offset]<<8) + pPatchData[src_data_offset+1]; //number of records B.E.
					src_data_offset += 2;
					dest_addr = (pPatchData[src_data_offset]<<24) + (pPatchData[src_data_offset+1]<<16) +
						(pPatchData[src_data_offset+2]<<8) + (pPatchData[src_data_offset+3]<<0); //destination address B.E.
					src_data_offset += 4;

					//AVL_puchar pRecordData = new AVL_uchar[(1<<16)+3];
					for(record_cnt = 0; record_cnt<num_records; record_cnt++) 
					{
						addr_offset = (pPatchData[src_data_offset]<<8) + pPatchData[src_data_offset+1]; //address offset B.E.
						src_data_offset += 2;
						record_length = (pPatchData[src_data_offset]<<8) + pPatchData[src_data_offset+1]; //data len B.E.
						src_data_offset += 2;

						//temporarily save patch data that will be overwritten
						temp[0] = pPatchData[src_data_offset-3];
						temp[1] = pPatchData[src_data_offset-2];
						temp[2] = pPatchData[src_data_offset-1];

						//break address into 3 bytes and put in patch array right in front of data
						ChunkAddr_AVL62X1(dest_addr + addr_offset, &(pPatchData[src_data_offset-3]));
						r |= II2C_Write_AVL62X1(pAVL_Chip->usI2CAddr, &(pPatchData[src_data_offset-3]), record_length+3);

						//restore patch data
						pPatchData[src_data_offset-3] = temp[0];
						pPatchData[src_data_offset-2] = temp[1];
						pPatchData[src_data_offset-1] = temp[2];

						src_data_offset += record_length;
					}
					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					patch_idx += 4*(num_rvs); //no RV's defined yet
					break;
				}
			case PATCH_CMD_RD_FROM_DEVICE://8 8
				{
					length = patch_read32_AVL62X1(pPatchData, &patch_idx); //in bytes
					src_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);
					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					rv0_idx = patch_read32_AVL62X1(pPatchData, &patch_idx);

					if(length == 4)
					{
						r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr,src_addr,&data);
						pAVL_Chip->m_variable_array[rv0_idx] = data;                      
					} 
					else if(length == 2) 
					{         
						r |= II2C_Read16_AVL62X1(pAVL_Chip->usI2CAddr,src_addr,&data1);
						pAVL_Chip->m_variable_array[rv0_idx] = data1;
					} 
					else if(length == 1) 
					{
						r |= II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr,src_addr,&data2);
						pAVL_Chip->m_variable_array[rv0_idx] = data2;
					}
					patch_idx += 4*(num_rvs - 1); //skip over any extraneous RV's
					break;
				}
			case PATCH_CMD_DMA://3
				{
					descr_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);
					num_descr = patch_read32_AVL62X1(pPatchData, &patch_idx);

					pPatchDatatemp = pPatchData + patch_idx;
					pPatchDatatemp1 = pPatchDatatemp - 3;
					temp[0] = *(pPatchDatatemp -1);
					temp[1] = *(pPatchDatatemp -2);
					temp[2] = *(pPatchDatatemp -3);
					ChunkAddr_AVL62X1(descr_addr, pPatchDatatemp1);

					r |= II2C_Write_AVL62X1(pAVL_Chip->usI2CAddr, pPatchDatatemp1, (AVL_uint32)(num_descr*3*4));
					* pPatchDatatemp1 = temp[2];
					*(pPatchDatatemp1+1) = temp[1];
					*(pPatchDatatemp1+2) = temp[0];
					patch_idx += 12*num_descr;

					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr,c_AVL62X1_DMD_fw_command_args_addr_iaddr,descr_addr);
					r |= IBase_SendRxOP_AVL62X1(CMD_DMA, pAVL_Chip);

					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					patch_idx += 4*(num_rvs); //no RV's defined yet
					break;
				}
			case PATCH_CMD_DECOMPRESS://4
				{
					type = patch_read32_AVL62X1(pPatchData, &patch_idx);
					src_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);
					dest_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);

					if(type == PATCH_CMP_TYPE_ZLIB) 
					{
						ref_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);
						ref_size = patch_read32_AVL62X1(pPatchData, &patch_idx);
					}

					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, c_AVL62X1_DMD_fw_command_args_addr_iaddr, args_addr);
					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, args_addr+0, type);
					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, args_addr+4, src_addr);
					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, args_addr+8, dest_addr);
					if(type == PATCH_CMP_TYPE_ZLIB) 
					{
						r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, args_addr+12, ref_addr);
						r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, args_addr+16, ref_size);
					}

					r |= IBase_SendRxOP_AVL62X1(CMD_DECOMPRESS, pAVL_Chip);

					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					patch_idx += 4*(num_rvs); //no RV's defined yet
					break;
				}
			case PATCH_CMD_ASSERT_CPU_RESET://5
				{
					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, hw_AVL62X1_c306_top_srst, 1);

					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					patch_idx += 4*(num_rvs); //no RV's defined yet
					break;
				}
			case PATCH_CMD_RELEASE_CPU_RESET://6
				{
					r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, hw_AVL62X1_c306_top_srst, 0);

					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					patch_idx += 4*(num_rvs); //no RV's defined yet
					break;
				}
			case PATCH_CMD_DMA_HW://9
				{
					descr_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);
					num_descr = patch_read32_AVL62X1(pPatchData, &patch_idx);

					temp[0] = *(pPatchData + patch_idx -1);
					temp[1] = *(pPatchData + patch_idx -2);
					temp[2] = *(pPatchData + patch_idx -3);
					ChunkAddr_AVL62X1(descr_addr, pPatchData + patch_idx -3);

					if(num_descr >0)
					{
						r |= II2C_Write_AVL62X1(pAVL_Chip->usI2CAddr, pPatchData + patch_idx -3, num_descr*12+3);
					}

					*(pPatchData + patch_idx -1) = temp[0];
					*(pPatchData + patch_idx -2) = temp[1];
					*(pPatchData + patch_idx -3) = temp[2];

					patch_idx += num_descr * 3 * 4;
					dma_tries = 0;
					dma_max_tries = 20;
					do
					{
						if(dma_tries > dma_max_tries)
						{
							return AVL_EC_GENERAL_FAIL; //FIXME return a value to check instead, and load the bootstrap
						}
						AVL_IBSP_Delay(10);//ms
						r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, hw_AVL62X1_dma_sys_status, &ready);
						//System::Console::WriteLine("num_dma_tries pre: {0}",dma_tries);
						dma_tries++;
					} while(!(0x01 & ready));

					if(ready)
					{
						r |= II2C_Write32_AVL62X1(pAVL_Chip->usI2CAddr, hw_AVL62X1_dma_sys_cmd, descr_addr); //Trigger DMA
					}

					dma_tries = 0;
					dma_max_tries = 20;
					do
					{
						if(dma_tries > dma_max_tries)
						{
							return AVL_EC_GENERAL_FAIL; //FIXME return a value to check instead, and load the bootstrap
						}
						AVL_IBSP_Delay(10);//ms
						r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr, hw_AVL62X1_dma_sys_status, &ready);
						//System::Console::WriteLine("num_dma_tries pre: {0}",dma_tries);
						dma_tries++;
					} while(!(0x01 & ready));

					//Add return value later
					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					patch_idx += 4*(num_rvs); //no RV's defined yet
					break;
				}

			case PATCH_CMD_SET_COND_IMM://10
				{
					rv = patch_read32_AVL62X1(pPatchData, &patch_idx);
					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					rv0_idx = patch_read32_AVL62X1(pPatchData, &patch_idx);
					pAVL_Chip->m_variable_array[rv0_idx] = rv;
					patch_idx += 4*(num_rvs - 1); //skip over any extraneous RV's
					break;
				}
			case PATCH_CMD_EXIT:
				{
					got_cmd_exit = 1;
					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					patch_idx = 4*(script_start_idx+script_len); //skip over any extraneous RV's
					break;
				}
			case PATCH_CMD_POLL_WAIT:
				{
					length = patch_read32_AVL62X1(pPatchData, &patch_idx); //in bytes
					src_addr = patch_read32_AVL62X1(pPatchData, &patch_idx);
					match_value = patch_read32_AVL62X1(pPatchData, &patch_idx);
					max_polls = patch_read32_AVL62X1(pPatchData, &patch_idx);
					polls = 0;
					do 
					{
						if(length == 4) 
						{
							r |= II2C_Read32_AVL62X1(pAVL_Chip->usI2CAddr,src_addr,&data);
							if(data == match_value)
								break;
						} 
						else if(length == 2)
						{
							r = II2C_Read16_AVL62X1(pAVL_Chip->usI2CAddr,src_addr,&data1);
							if(data1 == match_value)
								break;
						} 
						else if(length == 1) 
						{
							r = II2C_Read8_AVL62X1(pAVL_Chip->usI2CAddr,src_addr,&data2);
							if(data2 == match_value)
								break;
						}
						AVL_IBSP_Delay(10);//ms
						polls += 1;
					} while(polls < max_polls);
					num_rvs = patch_read32_AVL62X1(pPatchData, &patch_idx);
					patch_idx += 4*(num_rvs); //no RV's defined yet
					break;
				}
			} //switch cmd
		} 
		else 
		{
			patch_idx = next_cmd_idx; //jump to next command
			continue;
		}
	}

	return (r);
}
