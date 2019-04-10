/*
 *  mxl532.c
 *
 *  Written by CE-Android
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.=
 */

#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/firmware.h>

#define __DVB_CORE__
#include "dvb_frontend.h"

#include "MxLWare_HYDRA_OEM_Drv.h"
#include "MxLWare_HYDRA_CommonApi.h"
#include "MxLWare_HYDRA_DeviceApi.h"

#include "tcc_gpio.h"
#include "tcc_i2c.h"
#include "frontend.h"

/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG    "[MXL532C]"

static int dev_debug = 1;

module_param(dev_debug, int, 0644);
MODULE_PARM_DESC(dev_debug, "Turn on/off device debugging (default:off).");

#define dprintk(msg...)                                \
{                                                      \
	if (dev_debug)                                     \
		printk(KERN_INFO LOG_TAG "(D)" msg);           \
}

#define eprintk(msg...)                                \
{                                                      \
	printk(KERN_INFO LOG_TAG " (E) " msg);             \
}

/*****************************************************************************
 * Defines
 ******************************************************************************/
#define DEV_ID 0
#define MXL_HYDRA_CUSTOMER_FW "MxL_5xx_FW_customer.mbin"

struct i2c_adapter *g_p_i2c_adapter=NULL;
int timeout_wait = 0;


/*****************************************************************************
* STATIC MXL532C Local Functions
******************************************************************************/
static MXL_STATUS_E mxl532c_set_serial_mpeg(tcc_data_t *priv)
{
	MXL_HYDRA_MPEGOUT_PARAM_T tsCfg;
	MXL_STATUS_E result = MXL_SUCCESS;
	
	dprintk("%s\n", __FUNCTION__);
	
	tsCfg.enable = MXL_ENABLE;
	tsCfg.mpegClkType = MXL_HYDRA_MPEG_CLK_CONTINUOUS;
	tsCfg.mpegClkPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	tsCfg.maxMpegClkRate = 104;
	tsCfg.mpegClkPhase = MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_0_DEG;
	tsCfg.lsbOrMsbFirst = MXL_HYDRA_MPEG_SERIAL_MSB_1ST;
	tsCfg.mpegSyncPulseWidth = MXL_HYDRA_MPEG_SYNC_WIDTH_BIT;
	tsCfg.mpegValidPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	tsCfg.mpegSyncPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	tsCfg.mpegMode = MXL_HYDRA_MPEG_MODE_SERIAL_3_WIRE;
	tsCfg.mpegErrorIndication = MXL_HYDRA_MPEG_ERR_INDICATION_DISABLED;

	result = MxLWare_HYDRA_API_CfgMpegOutParams(priv->devId, priv->demodId, &tsCfg);
	if(result != MXL_SUCCESS) {
		eprintk("MxLWare_HYDRA_API_CfgMpegOutParams error(%d)\n",result);
		return result;
	}
	
	dprintk("MxLWare_HYDRA_API_CfgMpegOutParams Serial Done (%d)\n",result);

	return result;
}

static MXL_STATUS_E mxl532c_set_parallel_mpeg(tcc_data_t *priv)
{
	MXL_STATUS_E result = MXL_SUCCESS;
	MXL_HYDRA_MPEGOUT_PARAM_T tsCfg;
	
	result = MxLWare_HYDRA_API_CfgTSMixMuxMode(priv->devId, MXL_HYDRA_TS_GROUP_0_3, MXL_HYDRA_TS_MUX_4_TO_1);
	if(result != MXL_SUCCESS) {
		eprintk("%s :MxLWare_HYDRA_API_CfgTSMixMuxMode error(%d)\n",__FUNCTION__,result);
		return result;
	}
	/*
	MxLWare_HYDRA_API_CfgTSMixMuxMode(priv->devId, MXL_HYDRA_TS_GROUP_4_7, MXL_HYDRA_TS_MUX_4_TO_1);
	if(result != MXL_SUCCESS) {
		eprintk("%s :MxLWare_HYDRA_API_CfgTSMixMuxMode error(%d)\n",__FUNCTION__,result);
		return result;
	}
	*/
	
	tsCfg.enable = MXL_ENABLE;
	tsCfg.mpegClkType = MXL_HYDRA_MPEG_CLK_CONTINUOUS;
	tsCfg.mpegClkPol = MXL_HYDRA_MPEG_CLK_POSITIVE;
	tsCfg.maxMpegClkRate = 104;
	tsCfg.mpegClkPhase = MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_0_DEG;
	tsCfg.lsbOrMsbFirst = MXL_HYDRA_MPEG_SERIAL_MSB_1ST;
	tsCfg.mpegSyncPulseWidth = MXL_HYDRA_MPEG_SYNC_WIDTH_BYTE;
	tsCfg.mpegValidPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	tsCfg.mpegSyncPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	tsCfg.mpegMode = MXL_HYDRA_MPEG_MODE_PARALLEL;
	tsCfg.mpegErrorIndication = MXL_HYDRA_MPEG_ERR_INDICATION_DISABLED;

	
	result = MxLWare_HYDRA_API_CfgMpegOutParams(priv->devId, priv->demodId, &tsCfg);
	if(result != MXL_SUCCESS) {
		eprintk("MxLWare_HYDRA_API_CfgMpegOutParams error(%d)\n",result);
		return result;
	}
	
	dprintk("MxLWare_HYDRA_API_CfgMpegOutParams Parallel Done (%d)\n",result);
	
	return result;
}

static MXL_STATUS_E mxl532c_internal_init(tcc_data_t *priv)
{
	UINT8 devId=0;
	
	MXL_STATUS_E result = MXL_SUCCESS;
	MXL_HYDRA_INTR_CFG_T intrCfg;
	UINT32 intrMask = 0;
	MXL_HYDRA_VER_INFO_T verInfo;
	MXL_HYDRA_DEV_XTAL_T  xtalParam;
	
	const struct firmware *fw;
	UINT8 *fw_file = MXL_HYDRA_CUSTOMER_FW;
	
	dprintk("%s\n", __FUNCTION__);
	
	priv->devId = devId;
	
	result = MxLWare_HYDRA_API_CfgDrvInit(devId, (void*)priv,MXL_HYDRA_DEVICE_532C);
	
	result = MxLWare_HYDRA_OEM_DeviceReset(priv->devId);
	
	if(result != MXL_SUCCESS) {
		eprintk("%s :MxLWare_HYDRA_API_CfgDrvInit error(%d)\n",__FUNCTION__,result);
	}
	
	result = MxLWare_HYDRA_API_CfgDevOverwriteDefault(devId);
	if(result != MXL_SUCCESS) {
		eprintk("MxLWare_HYDRA_API_CfgDevOverwriteDefault error(%d)\n",result);
		return MXL_FAILURE;
	}
	
	xtalParam.xtalFreq = MXL_HYDRA_XTAL_24_MHz;
	xtalParam.xtalCap = 0x12;
	result = MxLWare_HYDRA_API_CfgDevXtal(devId,&xtalParam,MXL_DISABLE);
	if(result != MXL_SUCCESS) {
		eprintk("MxLWare_HYDRA_API_CfgDevXtal error(%d)\n",result);
		return MXL_FAILURE;
	}

	if(request_firmware(&fw, fw_file, &(g_p_i2c_adapter->dev)) == 0)
	{
		dprintk("Success firmware: fw name=%s fwSize =%d fwdata =0x%x \n", fw_file , fw->size, (unsigned int)fw->data);
	}
	else{
		eprintk("not foun the firmware file. (%s) \n", fw_file);
		return MXL_FAILURE;
	}
		
	result = MxLWare_HYDRA_API_CfgDevFWDownload(devId,fw->size, (UINT8 *)fw->data,NULL);
	if(result != MXL_SUCCESS) {
		eprintk("MxLWare_HYDRA_API_CfgDevFWDownload error(%d)\n",result);
	}
	
	result = MxLWare_HYDRA_API_ReqDevVersionInfo(devId,&verInfo);
	if(result == MXL_SUCCESS) {
		dprintk("chipId[%d] Ver[%d] mxlWareVer:%s firmwareVer:%s fwPatchVer:%s fwdownloaded(%d)",
			verInfo.chipId,verInfo.chipVer,verInfo.mxlWareVer,verInfo.firmwareVer,verInfo.fwPatchVer,verInfo.firmwareDownloaded);
	}
	else{
		eprintk("MxLWare_HYDRA_API_ReqDevVersionInfo error(%d)\n",result);
	}

	intrCfg.intrDurationInNanoSecs = 10000;
	intrCfg.intrType = HYDRA_HOST_INTR_TYPE_LEVEL_POSITIVE;
	intrMask = MXL_HYDRA_INTR_EN |
			MXL_HYDRA_INTR_FSK |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_0 |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_4 |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_1 |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_2 |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_3 |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_5;
	result = MxLWare_HYDRA_API_CfgDevInterrupt(devId,intrCfg,intrMask);
	if(result != MXL_SUCCESS) {
		eprintk("MxLWare_HYDRA_API_CfgDevInterrupt error(%d)\n",result);
		return MXL_FAILURE;
	}

	result = mxl532c_set_parallel_mpeg(priv);
	if(result != MXL_SUCCESS) {
		eprintk("mxl532c_set_parallel_ts error(%d)\n",result);
		return result;
	}

	result = MxLWare_HYDRA_API_CfgDevDiseqcFskOpMode(devId,MXL_HYDRA_AUX_CTRL_MODE_DISEQC);
	if(result != MXL_SUCCESS) {
		eprintk("MxLWare_HYDRA_API_CfgDevDiseqcFskOpMode error(%d)\n",result);
		return MXL_FAILURE;
	}

	/*
	priv->diseqcId = MXL_HYDRA_DISEQC_ID_0;
	result = MxLWare_HYDRA_API_CfgDiseqcOpMode(devId, MXL_HYDRA_DISEQC_ID_0 ,MXL_HYDRA_DISEQC_TONE_MODE,MXL_HYDRA_DISEQC_1_X,MXL_HYDRA_DISEQC_CARRIER_FREQ_22KHZ );
	if(result != MXL_SUCCESS) {
		eprintk("MxLWare_HYDRA_API_CfgDiseqcOpMode error(%d)\n",result);
		return MXL_FAILURE;
	}
	*/
	
	/*
	MXL_HYDRA_DISEQC_MSG_DELAYS_T diseqcMsgDelayPara;
	diseqcMsgDelayPara.newMsgDelayMs = 0;
	diseqcMsgDelayPara.endMsgDelayMs = 14;
	diseqcMsgDelayPara.replyDelayMs = 0;
	diseqcMsgDelayPara.startMsgDelayMs = 17;
	MxLWare_HYDRA_API_CfgDiseqcMsgDelays(MXL_HYDRA_DISEQC_ID_0,&diseqcMsgDelayPara);
	*/
	return MXL_SUCCESS;
}

static MXL_STATUS_E mxl532c_internal_tune(struct dvb_frontend* fe)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	
	MXL_HYDRA_TUNER_ID_E tunerId;
	MXL_HYDRA_TUNE_PARAMS_T tuneParams;
	MXL_BOOL_E tunerEnable;
	MXL_STATUS_E result = MXL_SUCCESS;
	
	dprintk("%s fe->id(%d) \n", __FUNCTION__,fe->id);
	
	tunerId = fe->id;

	if(fe->id >= MXL_HYDRA_TUNER_MAX){
		eprintk("MXL_HYDRA_TUNER_MAX \n");
		tunerId = MXL_HYDRA_TUNER_ID_3;
	}
	
	if(fe->id > 0){
		tunerId = MXL_HYDRA_TUNER_ID_1;
		dprintk("## Set MXL_HYDRA_TUNER_ID_1 \n");
	}
	else{
		tunerId = MXL_HYDRA_TUNER_ID_0;
		dprintk("## Set MXL_HYDRA_TUNER_ID_0 \n");
	}


	priv->tunerId = tunerId;
	
	priv->demodId = priv->tunerId; //map 1 to 1 .. need to do??

	MxLWare_HYDRA_API_CfgTunerEnable(priv->devId, tunerId, MXL_ENABLE);
	/*
	result = MxLWare_HYDRA_API_CfgTSPidFilterCtrl(priv->devId,priv->demodId,MXL_HYDRA_TS_PIDS_DROP_ALL);
	if(result !=MXL_SUCCESS) {
		eprintk("%s : error(%d)\n",__FUNCTION__,result);
	}
	*/
	result = MxLWare_HYDRA_API_CfgDemodSearchFreqOffset(priv->devId,priv->demodId,MXL_HYDRA_SEARCH_MAX_OFFSET);
	if(result !=MXL_SUCCESS) {
		eprintk("%s : error(%d)\n",__FUNCTION__,result);
	}
	MxLWare_HYDRA_OEM_SleepInMs(600);

	MxLWare_HYDRA_API_ReqTunerEnableStatus(priv->devId, tunerId,&tunerEnable);
	dprintk("MxLWare_HYDRA_API_ReqTunerEnableStatus (%d) frequency(%d) symbol_rate(%d) \n", tunerEnable,c->frequency,c->symbol_rate);
	
	tuneParams.standardMask = MXL_HYDRA_DVBS2;			//c->delivery_system
	tuneParams.frequencyInHz = c->frequency * 1000;
	tuneParams.freqSearchRangeKHz = 0;
	tuneParams.symbolRateKSps = c->symbol_rate/1000;
	tuneParams.spectrumInfo = MXL_HYDRA_SPECTRUM_NON_INVERTED; //c->inversion
	
	tuneParams.params.paramsS2.fec = MXL_HYDRA_FEC_AUTO; 		//c->fec_inner
	tuneParams.params.paramsS2.modulation = MXL_HYDRA_MOD_QPSK; //c ->modulation
	tuneParams.params.paramsS2.pilots = MXL_HYDRA_PILOTS_ON;	// c ->pilot
	tuneParams.params.paramsS2.rollOff = MXL_HYDRA_ROLLOFF_AUTO;	// c ->rolloff

	result = MxLWare_HYDRA_API_CfgDemodChanTune(priv->devId, tunerId, priv->demodId, &tuneParams);
	if(result!=MXL_SUCCESS) {
		eprintk("%s : error(%d)\n",__FUNCTION__,result);
	}
	MxLWare_HYDRA_OEM_SleepInMs(300);

	return result;

}
 /*****************************************************************************
  * MXL532C DVB FE Functions
  ******************************************************************************/
static int mxl532c_fe_sleep(struct dvb_frontend* fe)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	
	MxLWare_HYDRA_API_CfgDevPowerMode(priv->devId,MXL_HYDRA_PWR_MODE_STANDBY);
	
	priv->ref_cnt--;
	dprintk("%s ref_cnt(%d) \n", __FUNCTION__,priv->ref_cnt);
	
	//tcc_gpio_set(priv->gpio_lnb_slp, 0);
	//tcc_gpio_set(priv->gpio_fe_pwr, 0);

	return 0;
}


static void mxl532c_fe_release(struct dvb_frontend* fe)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;

	dprintk("%s ref_cnt(%d) \n", __FUNCTION__,priv->ref_cnt);
	
	if(priv->ref_cnt==0){
		MxLWare_HYDRA_API_CfgDrvUnInit(priv->devId);
		tcc_gpio_set(priv->gpio_lnb_slp, 0);
		tcc_gpio_set(priv->gpio_fe_pwr, 0);
	}
 }

static int mxl532c_fe_init(struct dvb_frontend* fe)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	MXL_STATUS_E result = MXL_SUCCESS;

	dprintk("\n %s: dbv->num(%d) name(%s) id(%d) ref_cnt(%d) \n", __FUNCTION__,fe->dvb->num,fe->dvb->name,fe->id,priv->ref_cnt);

	priv->tunerId = fe->id;

	if(priv->ref_cnt==0){
		tcc_gpio_set(priv->gpio_lnb_slp, 1);
		tcc_gpio_set(priv->gpio_fe_pwr, 1);
	}
	
	priv->ref_cnt++;

	if(priv->is_initialized==0){
		result = mxl532c_internal_init(priv);
		if(result == MXL_SUCCESS)
			priv->is_initialized = 1;
	}

	return 0;
	 
}

static int mxl532c_fe_tune(struct dvb_frontend* fe, bool re_tune, unsigned int mode_flags, unsigned int *delay, fe_status_t *status)
{
	MXL_STATUS_E result = MXL_SUCCESS;
	printk("re_tune(%d) timeout_wait(%d) \n ",re_tune, timeout_wait);
	if (re_tune) {
		result = mxl532c_internal_tune(fe);
		if(result == MXL_SUCCESS){
			timeout_wait = 10;
		}
		else{
			timeout_wait = 0;
		}
		*status = 0;
	}
	else
	{
		fe->ops.read_status(fe, status);
		if (*status != 0)
		{
			timeout_wait = 0;
		}
		if (timeout_wait > 0){
			timeout_wait--;
			if(timeout_wait == 0)
				*status = FE_TIMEDOUT;
		}
	}

	if (timeout_wait > 0)
	{
		*delay = HZ / 100; // 10 ms
	}
	else
	{
		*delay = 10 * HZ; // 10 sess
	}

	return 0;
}

static int mxl532c_fe_read_status(struct dvb_frontend* fe, fe_status_t* status)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	MXL_HYDRA_DEMOD_LOCK_T demodLock;
	MXL_STATUS_E result = MXL_SUCCESS;

	result = MxLWare_HYDRA_API_ReqDemodLockStatus(priv->devId,priv->demodId, &demodLock);	 

	//dprintk("demodLock.agcLock(%d) , demodLock.fecLock(%d)\n", demodLock.agcLock, demodLock.fecLock);

	if(result != MXL_SUCCESS) {
		eprintk("%s : error(%d)\n",__FUNCTION__,result);
	}

	if(demodLock.fecLock == MXL_TRUE)
	{
	 *status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	}
	else
	{
	 *status = 0;
	}

	return  0;
}

static enum dvbfe_algo mxl532c_fe_get_frontend_algo(struct dvb_frontend *fe)
{
	return DVBFE_ALGO_HW;
}
 
static int mxl532c_fe_read_snr(struct dvb_frontend* fe, unsigned short* snr)
{
 	
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	SINT16 snr100xdB = 0;
	MXL_STATUS_E result = MXL_SUCCESS;

	result = MxLWare_HYDRA_API_ReqDemodSNR(priv->devId, priv->demodId ,&snr100xdB);
	
 	if(result != MXL_SUCCESS) {
		eprintk("%s : error(%d)\n",__FUNCTION__,result);
	}
	*snr = snr100xdB/100;
	
	return result;
}

/* valid in DSS or DVB-S */
static int mxl532c_fe_read_ber(struct dvb_frontend* fe, unsigned int* ber)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	MXL_HYDRA_DEMOD_SCALED_BER_T scaledBer;
	MXL_STATUS_E result = MXL_SUCCESS;

	result = MxLWare_HYDRA_API_ReqDemodScaledBER(priv->devId, priv->demodId ,&scaledBer);

	if(result == MXL_SUCCESS) {
	 dprintk("%s : scaledBerIter1(%d) scaledBerIter2(%d)\n",__FUNCTION__,scaledBer.scaledBerIter1,scaledBer.scaledBerIter2);
	}

	return result;
}

static int mxl532c_fe_read_signal_strength(struct dvb_frontend* fe, unsigned short* snr)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	SINT32 inputPwrLevel;
	MXL_STATUS_E result = MXL_SUCCESS;
 
	result = MxLWare_HYDRA_API_ReqDemodRxPowerLevel(priv->devId, priv->demodId ,&inputPwrLevel);
 
	if(result == MXL_SUCCESS) {
	  dprintk("%s : inputPwrLevel(%d) \n",__FUNCTION__,inputPwrLevel);
	}
 
	return result;
}

static int mxl532c_fe_read_ucblocks(struct dvb_frontend* fe, unsigned int* ucblocks)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	MXL_HYDRA_DEMOD_SCALED_BER_T scaledBer;
	MXL_STATUS_E result = MXL_SUCCESS;

	result = MxLWare_HYDRA_API_ReqDemodScaledBER(priv->devId, priv->demodId ,&scaledBer);

	if(result == MXL_SUCCESS) {
	 dprintk("%s : scaledBerIter1(%d) scaledBerIter2(%d)\n",__FUNCTION__,scaledBer.scaledBerIter1,scaledBer.scaledBerIter2);
	}

	return result;
}

static int mxl532c_fe_diseqc_reset_overload(struct dvb_frontend* fe)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	MXL_STATUS_E result = MXL_SUCCESS;

	dprintk("%s : (%d) \n",__FUNCTION__,priv->devId);

	return result;
}

static int mxl532c_fe_diseqc_send_master_cmd(struct dvb_frontend* fe, struct dvb_diseqc_master_cmd* cmd)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	MXL_STATUS_E result = MXL_SUCCESS;

	dprintk("%s : (%d) \n",__FUNCTION__,priv->devId);

	return result;
}

static int mxl532c_fe_diseqc_recv_slave_reply(struct dvb_frontend* fe, struct dvb_diseqc_slave_reply* reply)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	MXL_STATUS_E result = MXL_SUCCESS;
	
	dprintk("%s : (%d) \n",__FUNCTION__,priv->devId);

	return result;

}

static int mxl532c_fe_diseqc_send_burst(struct dvb_frontend* fe, fe_sec_mini_cmd_t minicmd)
{
	tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	MXL_STATUS_E result = MXL_SUCCESS;
	dprintk("%s : (%d) \n",__FUNCTION__,priv->devId);

	return result;
}

 static int mxl532c_fe_set_tone(struct dvb_frontend* fe, fe_sec_tone_mode_t tone)
 {
	 tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	 MXL_STATUS_E result = MXL_SUCCESS;
	 dprintk("%s : (%d) \n",__FUNCTION__,priv->devId);
	 
	 return result;
 }

 static int mxl532c_fe_set_voltage(struct dvb_frontend* fe, fe_sec_voltage_t voltage)
 {
	 tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	 MXL_STATUS_E result = MXL_SUCCESS;
	 dprintk("%s : (%d) \n",__FUNCTION__,priv->devId);
	 
	 return result;
 }

 static int mxl532c_fe_enable_high_lnb_voltage(struct dvb_frontend* fe, long arg)
 {
	 tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	 MXL_STATUS_E result = MXL_SUCCESS;
	 dprintk("%s : (%d) \n",__FUNCTION__,priv->devId);
	 
	 return result;
 }

 static int mxl532c_fe_i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
 {
	 tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	 MXL_STATUS_E result = MXL_SUCCESS;
	 dprintk("%s : (%d) \n",__FUNCTION__,priv->devId);
	 
	 return result;
 }
 
 static int mxl532c_fe_get_property(struct dvb_frontend* fe, struct dtv_property* tvp)
 {
	 tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	 MXL_STATUS_E result = MXL_SUCCESS;
	 dprintk("%s : (%d) \n",__FUNCTION__,priv->devId);
	 
	 return result;
 }
 
 static int mxl532c_fe_set_property(struct dvb_frontend* fe, struct dtv_property* tvp)
 {
	 tcc_data_t *priv = (tcc_data_t *)fe->demodulator_priv;
	 MXL_STATUS_E result = MXL_SUCCESS;
	 //dprintk("%s : (%d) cmd(%d)\n",__FUNCTION__,priv->devId,tvp->cmd);
	 
	 
	 return result;
 }

 static int mxl532c_fe_get_frontend(struct dvb_frontend *fe, struct dtv_frontend_properties *props)
 {
 
	 return 0;
 }
 


 /*****************************************************************************
  * MXL532C DVB FE OPS
  ******************************************************************************/
 static struct dvb_frontend_ops mxl532c_fe_ops = {
 
	 .info = {
		 .name = "TCC DVB-S2 (MXL532C)",
		 .type = FE_QPSK,
		 .frequency_min = 850000,
		 .frequency_max = 2300000,
		 .caps =
			 FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			 FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			 FE_CAN_QPSK | FE_CAN_QAM_AUTO |
			 FE_CAN_TRANSMISSION_MODE_AUTO |
			 FE_CAN_GUARD_INTERVAL_AUTO |
			 FE_CAN_HIERARCHY_AUTO |
			 FE_CAN_RECOVER |
			 FE_CAN_MUTE_TS
	 },
 
	 .delsys = { SYS_DVBS, SYS_DVBS2 },
 
	 .release = mxl532c_fe_release,
 
	 .init = mxl532c_fe_init,
	 .sleep = mxl532c_fe_sleep,
 
	 .tune = mxl532c_fe_tune,
	 
	 .get_frontend_algo = mxl532c_fe_get_frontend_algo,
	 .get_frontend = mxl532c_fe_get_frontend,
 
	 .read_status = mxl532c_fe_read_status,
	 .read_ber = mxl532c_fe_read_ber,
	 
	 .read_signal_strength = mxl532c_fe_read_signal_strength,
	 .read_snr = mxl532c_fe_read_snr,
	 .read_ucblocks = mxl532c_fe_read_ucblocks,
 
	 .diseqc_reset_overload = mxl532c_fe_diseqc_reset_overload,
	 .diseqc_send_master_cmd = mxl532c_fe_diseqc_send_master_cmd,
	 .diseqc_recv_slave_reply = mxl532c_fe_diseqc_recv_slave_reply,
	 .diseqc_send_burst = mxl532c_fe_diseqc_send_burst,
	 .set_tone = mxl532c_fe_set_tone,
	 .set_voltage = mxl532c_fe_set_voltage,
	 .enable_high_lnb_voltage = mxl532c_fe_enable_high_lnb_voltage,
	 .i2c_gate_ctrl = mxl532c_fe_i2c_gate_ctrl,
 
	 .set_property = mxl532c_fe_set_property,
	 .get_property = mxl532c_fe_get_property,
 };

 /*****************************************************************************
  * mxl532c_init
  ******************************************************************************/
static int mxl532c_init(struct device_node *node, tcc_data_t *priv)
{
 
	if(tcc_i2c_init() >=0) {
		g_p_i2c_adapter = tcc_i2c_get_adapter();
	}
	
	priv->gpio_lnb_slp  = of_get_named_gpio(node, "gpios",   0); // LNB_SLP
	priv->gpio_fe_reset  = of_get_named_gpio(node, "gpios",   2); // DVB RESET
	
	tcc_gpio_init(priv->gpio_lnb_slp, GPIO_DIRECTION_OUTPUT);
	tcc_gpio_init(priv->gpio_fe_reset, GPIO_DIRECTION_OUTPUT);
	
	priv->gpio_lnb1_irq  = of_get_named_gpio(node, "irq-gpios ",   0); // LNB_IRQ
	priv->gpio_lnb2_irq  = of_get_named_gpio(node, "irq-gpios ",   1); // LNB_IRQ
	
	tcc_gpio_init(priv->gpio_lnb1_irq, GPIO_DIRECTION_INPUT);
	tcc_gpio_init(priv->gpio_lnb1_irq, GPIO_DIRECTION_INPUT);
	
	priv->gpio_fe_pwr  = of_get_named_gpio(node, "pw-gpios",   0); // SLP_CTL
	tcc_gpio_init(priv->gpio_fe_pwr, GPIO_DIRECTION_OUTPUT);
	
	return 0;
 }


 /*****************************************************************************
  * MXL532C Module Init/Exit
  ******************************************************************************/
 static int mxl532c_probe(tcc_fe_priv_t *inst)
 {
	 tcc_data_t *priv = kzalloc(sizeof(tcc_data_t), GFP_KERNEL);
	 int retval;
	 
	 if (priv == NULL)
	 {
		 eprintk("%s(kzalloc fail)\n", __FUNCTION__);
		 return -1;
	 }

 	 memset(priv, 0x00, sizeof(tcc_data_t));
	 retval = mxl532c_init(inst->of_node, priv);
	 if (retval != 0)
	 {
		 eprintk("%s(Init Error %d)\n", __FUNCTION__, retval);
		 kfree(priv);
		 return -1;
	 }
 
	 inst->fe.demodulator_priv = priv;
	 
	 dprintk("%s\n", __FUNCTION__);
	 return 0;
 }

 /*****************************************************************************
  * MXL532C Module Init/Exit
  ******************************************************************************/
 static struct tcc_dxb_fe_driver mxl532c_driver = {
	 .probe = mxl532c_probe,
	 .compatible = "maxlinear,mxl532c",
	 .fe_ops = &mxl532c_fe_ops,
 };
 
 static int __init mxl532c_module_init(void)
 {
	 dprintk("%s \n", __FUNCTION__);
	 if (tcc_fe_register(&mxl532c_driver) != 0)
		 return -EPERM;
 
	 return 0;
 }
 
 static void __exit mxl532c_module_exit(void)
 {
	 dprintk("%s\n", __FUNCTION__);
	 tcc_fe_unregister(&mxl532c_driver);
 }
 
 module_init(mxl532c_module_init);
 module_exit(mxl532c_module_exit);
 
 MODULE_DESCRIPTION("TCC DVBS2 (MXL532C");
 MODULE_AUTHOR("CE Android");
 MODULE_LICENSE("GPL");

