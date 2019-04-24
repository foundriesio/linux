/*
 *  si2166.c
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
	 
#define __DVB_CORE__
#include "dvb_frontend.h"

#include <linux/i-dxb/silabs/si2166.h>
#include "SiLabs_API_L3_Wrapper.h"

#include "tcc_gpio.h"
#include "tcc_i2c.h"
#include "frontend.h"

#define LOG_TAG    "[SI2166]"

static int dev_debug = 1;
int timeout_wait = 0;

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

#define FE_DEV_CNT 2;

typedef struct {
	struct i2c_adapter *i2c;
	int is_initialized;
	int fe_id;
	int fe_cnt;
	int ref_cnt;
	int gpio_power;
	int gpio_reset[2];
} si2166_priv_t;

typedef struct tag_si2166_i2c_addr_t
{
	unsigned char uc_lnb;
	unsigned char uc_tuner;
	unsigned char uc_demod;
} si2166_i2c_addr_t;

si2166_i2c_addr_t g_i2c_addr[SILABS_FE_TYPE_MAX] = {
	 { 0x10, 0xc4, 0xc8 }, 
	 { 0x16, 0xc4, 0xce },	 
	 { 0x00, 0x00, 0x00 }, 
	 { 0x00, 0x00, 0x00 }
};

static int si2166_internal_init(si2166_priv_t *priv)
{
	int i;
	int ret = 0;
	char str_tag[20];
	
	dprintk("%s fe_id(%d)\n", __FUNCTION__,priv->fe_id);
	
	i = priv->fe_id;
	//for(i = 0; i < priv->fe_cnt; i++)
	{
		si2166_i2c_addr_t *p_i2c_addr = &g_i2c_addr[i];

		SILABS_FE_Context *p_ctx = &FrontEnd_Table[i];

		sprintf(str_tag, "fe[%d]", i);

		SiLabs_API_Frontend_Chip(p_ctx, 0x2183);

		SiLabs_API_SW_Init(p_ctx, p_i2c_addr->uc_demod, 0xc0, p_i2c_addr->uc_tuner);

		SiLabs_API_Select_SAT_Tuner(p_ctx, 0xa2018, 0);

		SiLabs_API_SAT_Select_LNB_Chip(p_ctx, 0xA8304, p_i2c_addr->uc_lnb);
		SiLabs_API_SAT_tuner_I2C_connection(p_ctx, 0);
		SiLabs_API_SAT_Clock(p_ctx, 2, 44, 27, 2);
		SiLabs_API_SAT_Spectrum(p_ctx, 0);
		SiLabs_API_SAT_AGC(p_ctx, 0xa, 1, 0x0, 1);

		SiLabs_API_Set_Index_and_Tag(p_ctx, i, str_tag);

		SiLabs_API_HW_Connect(p_ctx, 2);
	}

	//for(i = 0; i< priv->fe_cnt; i++)
	{
		char str_msg[100];

		unsigned char uc_rbuf[100];

		L0_Context t_i2c;

		si2166_i2c_addr_t *p_i2c_addr = &g_i2c_addr[i];

		SILABS_FE_Context *p_ctx = &FrontEnd_Table[i];

		L0_Init(&t_i2c);

		/* Check demod communication */
		sprintf(str_msg, "0x%02x 0x00 0x00 1", p_i2c_addr->uc_demod);

		if(L0_ReadString(&t_i2c, str_msg, uc_rbuf) != 1)
		{
			printk("[%s:%d] Demod communication error!\n", __func__, __LINE__);
			ret = -1;
			return ret;
		}

		/* Check tuner communication */
		SiLabs_API_Tuner_I2C_Enable(p_ctx);

		sprintf(str_msg, "0x%02x 0x00 1", p_i2c_addr->uc_tuner);

		if(L0_ReadString(&t_i2c, str_msg, uc_rbuf) != 1)
		{
			printk("[%s:%d] Tuner communication error!\n", __func__, __LINE__);
			ret = -1;
			return ret;
		}
	}

	return ret;
}

static void si2166_set_tune_param(struct dvb_frontend* fe, si2166_tune_params_t *p_param)
{
	si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	
	p_param->i_fe_id = priv->fe_id;
	p_param->i_frequency = c->frequency;
	p_param->i_bandwidth_hz = 0;//c->bandwidth_hz;
	p_param->i_data_slice_id = 0;
	p_param->i_isi_id = -1;
	p_param->i_plp_id = -1;
	p_param->i_t2_lock_mode = 0;
	p_param->ui_symbol_rate_bps = c->symbol_rate;
	p_param->e_stream = SILABS_STREAM_HP;
	p_param->e_polar = SILABS_POLAR_HORIZONTAL;
	p_param->e_band_type = SILABS_BAND_TYPE_LOW;

	switch(c->delivery_system)
	{
		case SYS_DVBS2:
			p_param->e_standard = SILABS_STANDARD_DVB_S2;
			break;
		case SYS_DVBS:
			p_param->e_constel = SILABS_STANDARD_DVB_S;
			break;
		default:
			p_param->e_standard = SILABS_STANDARD_DVB_S2;
			break;
	}
	
	switch(c->modulation)
	{
		case QPSK:
			p_param->e_constel = SILABS_CONSTEL_QPSK;
			break;
		case QAM_16:
			p_param->e_constel = SILABS_CONSTEL_QAM16;
			break;
		case QAM_32:
			p_param->e_constel = SILABS_CONSTEL_QAM32;
			break;
		
		case QAM_64:
			p_param->e_constel = SILABS_CONSTEL_QAM64;
			break;
		case QAM_128:
			p_param->e_constel = SILABS_CONSTEL_QAM128;
			break;
		case QAM_256:
			p_param->e_constel = SILABS_CONSTEL_QAM256;
			break;
		case QAM_AUTO:
			p_param->e_constel = SILABS_CONSTEL_QAMAUTO;
			break;
		case PSK_8:
			p_param->e_constel = SILABS_CONSTEL_8PSK;
			break;
		default:
			p_param->e_constel = SILABS_CONSTEL_QPSK;
			break;
			
	}
	
}

static int si2166_internal_tune(struct dvb_frontend* fe)
{
	int i_lock = 2;
	int ret=0;
	si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;

	si2166_tune_params_t t_params;
	SILABS_FE_Context *p_ctx;

	if(priv->fe_id < 0 ||priv->fe_id >= priv->fe_cnt)
	{
		 printk("[%s:%d] FE id error! - %d!\n", __func__, __LINE__,priv->fe_id);				 
		 ret = -1;
		 return ret;
	}
	
	si2166_set_tune_param(fe,&t_params);
	
	p_ctx = &FrontEnd_Table[t_params.i_fe_id];
	p_ctx->polarization = (int)t_params.e_polar;
	p_ctx->band		 = (int)t_params.e_band_type;

	switch(p_ctx->polarization)
	{
		case SILABS_POLAR_HORIZONTAL:
		{
			SiLabs_API_SAT_voltage_and_tone(p_ctx, 18, 0);
		}
		break;
		case SILABS_POLAR_VERTICAL:
		{
			SiLabs_API_SAT_voltage_and_tone(p_ctx, 13, 0);
		}
		break;
		default:
		{
			ret = -1;
			return ret;
		}
		break;
	}
	
	if(!SiLabs_API_switch_to_standard(p_ctx, (int)t_params.e_standard, 0))
	{
		ret = -1;
		return ret;
	}

	while(i_lock > 1)
	{
		 i_lock = SiLabs_API_lock_to_carrier(
			 p_ctx, 
			 (int)t_params.e_standard, 
			 t_params.i_frequency, 
			 t_params.i_bandwidth_hz, 
			 (unsigned int)t_params.e_stream, 
			 t_params.ui_symbol_rate_bps, 
			 (unsigned int)t_params.e_constel, 
			 (unsigned int)t_params.e_polar, 
			 (unsigned int)t_params.e_band_type, 
			 t_params.i_data_slice_id, 
			 t_params.i_plp_id, 
			 t_params.i_t2_lock_mode);

		 if(i_lock > 1)
		 {
			 dprintk("Console Lock: Handshaking after %6d ms\n", i_lock);
		 }
	}

	if(i_lock == 1)
	{
		char str_msg[100];
		SiLabs_API_FE_status(p_ctx, &FE_Status);
		SiLabs_API_Text_status(p_ctx, &FE_Status, str_msg);

		dprintk("%s", str_msg);
	}

	SiLabs_API_TS_Mode(p_ctx, SILABS_TS_SERIAL);

	return ret;
}


/*****************************************************************************
  * si2166 DVB FE Functions
  ******************************************************************************/
static int si2166_fe_sleep(struct dvb_frontend* fe)
{
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	 
	 dprintk("%s ref_cnt(%d) \n", __FUNCTION__,priv->ref_cnt);
	 
	 //tcc_gpio_set(priv->gpio_power, 0);
 
	 return 0;
}
 
 
static void si2166_fe_release(struct dvb_frontend* fe)
{
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
 
	 dprintk("%s fe_id(%d) \n", __FUNCTION__,priv->fe_id);
	 
	 tcc_gpio_set(priv->gpio_power, 0);
	 tcc_gpio_set(priv->gpio_reset[priv->fe_id],0);
}
 
static int si2166_fe_init(struct dvb_frontend* fe)
{
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
 
	 dprintk("\n %s: dbv->num(%d) name(%s) id(%d) is_initialized(%d) \n", __FUNCTION__,fe->dvb->num,fe->dvb->name,fe->id,priv->is_initialized);
 
	 priv->fe_id = fe->dvb->num;
 
	 tcc_gpio_set(priv->gpio_power, 1);
	 msleep(20);
	 tcc_gpio_set(priv->gpio_reset[priv->fe_id], 0);
	 msleep(100);
	 tcc_gpio_set(priv->gpio_reset[priv->fe_id], 1);
	 
	 if(priv->is_initialized==0){
		 if(si2166_internal_init(priv) >=0)
			 priv->is_initialized = 1;
	 }
 
	 return 0;
}
 
static int si2166_fe_tune(struct dvb_frontend* fe, bool re_tune, unsigned int mode_flags, unsigned int *delay, fe_status_t *status)
{
	 int result = 0;
	 printk("re_tune(%d) timeout_wait(%d) \n ",re_tune, timeout_wait);
	 if (re_tune) {
		 result = si2166_internal_tune(fe);
		 if(result == 0){
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
 
static int si2166_fe_read_status(struct dvb_frontend* fe, fe_status_t* status)
{
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	 int result = 1;
	 SILABS_FE_Context *p_ctx = &FrontEnd_Table[priv->fe_id];
	 
	 result = SiLabs_API_FE_status(p_ctx, &FE_Status);
 
	 //dprintk("demodLock.agcLock(%d) , demodLock.fecLock(%d)\n", demodLock.agcLock, demodLock.fecLock);
 
	 if(result != 1) {
		 eprintk("%s : error(%d)\n",__FUNCTION__,result);
	 }
 
	 if(FE_Status.demod_lock == 1)
	 {
	  *status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	 }
	 else
	 {
	  *status = 0;
	 }
 
	 return  0;
}
 
static enum dvbfe_algo si2166_fe_get_frontend_algo(struct dvb_frontend *fe)
{
	 return DVBFE_ALGO_HW;
}
  
static int si2166_fe_read_snr(struct dvb_frontend* fe, unsigned short* snr)
{
	 
	 int result = 1;
 
	 return result;
}
 
 /* valid in DSS or DVB-S */
 static int si2166_fe_read_ber(struct dvb_frontend* fe, unsigned int* ber)
 {
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	 int result = 1;
 
	 return result;
 }
 
 static int si2166_fe_read_signal_strength(struct dvb_frontend* fe, unsigned short* snr)
 {
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	 int result = 1;
  
	 return result;
 }
 
 static int si2166_fe_read_ucblocks(struct dvb_frontend* fe, unsigned int* ucblocks)
 {
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	 int result = 1;
 
 
	 return result;
 }
 
 static int si2166_fe_diseqc_reset_overload(struct dvb_frontend* fe)
 {
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	 int result = 1;
 
	 dprintk("%s : (%d) \n",__FUNCTION__,priv->fe_id);
 
	 return result;
 }
 
 static int si2166_fe_diseqc_send_master_cmd(struct dvb_frontend* fe, struct dvb_diseqc_master_cmd* cmd)
 {
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	 int result = 1;
 
	 dprintk("%s : (%d) \n",__FUNCTION__,priv->fe_id);
 
	 return result;
 }
 
 static int si2166_fe_diseqc_recv_slave_reply(struct dvb_frontend* fe, struct dvb_diseqc_slave_reply* reply)
 {
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	 int result = 1;
	 
	 dprintk("%s : (%d) \n",__FUNCTION__,priv->fe_id);
 
	 return result;
 
 }
 
 static int si2166_fe_diseqc_send_burst(struct dvb_frontend* fe, fe_sec_mini_cmd_t minicmd)
 {
	 si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	 int result = 1;
	 dprintk("%s : (%d) \n",__FUNCTION__,priv->fe_id);
 
	 return result;
 }
 
  static int si2166_fe_set_tone(struct dvb_frontend* fe, fe_sec_tone_mode_t tone)
  {
	  si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	  int result = 1;
	  dprintk("%s : (%d) \n",__FUNCTION__,priv->fe_id);
	  
	  return result;
  }
 
  static int si2166_fe_set_voltage(struct dvb_frontend* fe, fe_sec_voltage_t voltage)
  {
	  si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	  int result = 1;
	  dprintk("%s : (%d) \n",__FUNCTION__,priv->fe_id);
	  
	  return result;
  }
 
  static int si2166_fe_enable_high_lnb_voltage(struct dvb_frontend* fe, long arg)
  {
	  si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	  int result = 1;
	  dprintk("%s : (%d) \n",__FUNCTION__,priv->fe_id);
	  
	  return result;
  }
 
  static int si2166_fe_i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
  {
	  si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	  int result = 1;
	  dprintk("%s : (%d) \n",__FUNCTION__,priv->fe_id);
	  
	  return result;
  }
  
  static int si2166_fe_get_property(struct dvb_frontend* fe, struct dtv_property* tvp)
  {
	  si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	  int result = 1;
	  dprintk("%s : (%d) \n",__FUNCTION__,priv->fe_id);
	  
	  return result;
  }
  
  static int si2166_fe_set_property(struct dvb_frontend* fe, struct dtv_property* tvp)
  {
	  si2166_priv_t *priv = (si2166_priv_t *)fe->demodulator_priv;
	  int result = 1;
	  //dprintk("%s : (%d) cmd(%d)\n",__FUNCTION__,priv->fe_id,tvp->cmd);
	  
	  
	  return result;
  }
 
  static int si2166_fe_get_frontend(struct dvb_frontend *fe, struct dtv_frontend_properties *props)
  {
  
	  return 0;
  }
  


 /*****************************************************************************
  * si2166 DVB FE OPS
  ******************************************************************************/
 static struct dvb_frontend_ops si2166_fe_ops = {
 
	 .info = {
		 .name = "TCC DVB-S2 (si2166)",
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
 
	 .release = si2166_fe_release,
 
	 .init = si2166_fe_init,
	 .sleep = si2166_fe_sleep,
 
	 .tune = si2166_fe_tune,
	 
	 .get_frontend_algo = si2166_fe_get_frontend_algo,
	 .get_frontend = si2166_fe_get_frontend,
 
	 .read_status = si2166_fe_read_status,
	 .read_ber = si2166_fe_read_ber,
	 
	 .read_signal_strength = si2166_fe_read_signal_strength,
	 .read_snr = si2166_fe_read_snr,
	 .read_ucblocks = si2166_fe_read_ucblocks,
 
	 .diseqc_reset_overload = si2166_fe_diseqc_reset_overload,
	 .diseqc_send_master_cmd = si2166_fe_diseqc_send_master_cmd,
	 .diseqc_recv_slave_reply = si2166_fe_diseqc_recv_slave_reply,
	 .diseqc_send_burst = si2166_fe_diseqc_send_burst,
	 .set_tone = si2166_fe_set_tone,
	 .set_voltage = si2166_fe_set_voltage,
	 .enable_high_lnb_voltage = si2166_fe_enable_high_lnb_voltage,
	 .i2c_gate_ctrl = si2166_fe_i2c_gate_ctrl,
 
	 .set_property = si2166_fe_set_property,
	 .get_property = si2166_fe_get_property,
 };

 /*****************************************************************************
  * si2166_init
  ******************************************************************************/
static int si2166_init(struct device_node *node, si2166_priv_t *priv)
{
 
	if(tcc_i2c_init() >=0) {
		priv->i2c= tcc_i2c_get_adapter();
	}
	
	priv->fe_cnt = FE_DEV_CNT;
	
	priv->gpio_power  =  of_get_named_gpio(node, "pw-gpios", 0);
	tcc_gpio_init(priv->gpio_power, GPIO_DIRECTION_OUTPUT);
	
	priv->gpio_reset[0]  = of_get_named_gpio(node, "si0-gpios", 1); 
	priv->gpio_reset[1]  = of_get_named_gpio(node, "si1-gpios ", 1); 
	
	tcc_gpio_init(priv->gpio_reset[0], GPIO_DIRECTION_OUTPUT);
	tcc_gpio_init(priv->gpio_reset[1], GPIO_DIRECTION_OUTPUT);
	
	return 0;
 }


 /*****************************************************************************
  * si2166 Module Init/Exit
  ******************************************************************************/
 static int si2166_probe(tcc_fe_priv_t *inst)
 {
	 si2166_priv_t *priv = kzalloc(sizeof(si2166_priv_t), GFP_KERNEL);
	 int retval;
	 
	 if (priv == NULL)
	 {
		 eprintk("%s(kzalloc fail)\n", __FUNCTION__);
		 return -1;
	 }

 	 memset(priv, 0x00, sizeof(si2166_priv_t));
	 retval = si2166_init(inst->of_node, priv);
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
 * SI2166 Module Init/Exit
 ******************************************************************************/
static struct tcc_dxb_fe_driver si2166_driver = {
	.probe = si2166_probe,
	.compatible = "telechips,si_power",
	.fe_ops = &si2166_fe_ops,
};

static int __init si2166_module_init(void)
{
	dprintk("%s \n", __FUNCTION__);
	if (tcc_fe_register(&si2166_driver) != 0)
		return -EPERM;

	return 0;
}

static void __exit si2166_module_exit(void)
{
	dprintk("%s\n", __FUNCTION__);
	tcc_fe_unregister(&si2166_driver);
}

module_init(si2166_module_init);
module_exit(si2166_module_exit);

MODULE_DESCRIPTION("TCC DVBS2 (si2166");
MODULE_AUTHOR("CE Android");
MODULE_LICENSE("GPL");

