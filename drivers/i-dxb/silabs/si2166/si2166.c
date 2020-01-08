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

#define LOG_TAG	"[SI2166]"

static int dev_debug = 0;
int timeout_wait = 0;

module_param(dev_debug, int, 0644);
MODULE_PARM_DESC(dev_debug, "Turn on/off device debugging (default:off).");

#define dprintk(msg...)								\
	{												\
		if(dev_debug)								\
			printk(KERN_INFO LOG_TAG "(D)" msg);	\
	}

#define eprintk(msg...)								\
	{												\
		printk(KERN_INFO LOG_TAG "(E) " msg);		\
	}

#define AVAIL_MAX_DEV_CNT	2

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

typedef struct
{
	int i_lock_retry_cnt;
} si2166_priv_t;

typedef struct
{
	int i_ref_cnt;

	bool b_res_init;

	struct mutex t_power_lock;
	
	struct {
		int i_si2166_power_gpio;
		int ia_si2166_reset_gpio[AVAIL_MAX_DEV_CNT];
		int i_lnb_power_gpio;
	} res;

	si2166_priv_t *pa_priv[AVAIL_MAX_DEV_CNT];
} si2166_t;

si2166_t *gp_instance = NULL;

static void sfn_power_ctrl(si2166_t *p_instance, int i_dev, bool b_on)
{
	int i;
	bool b_off_power = true;

	mutex_lock(&p_instance->t_power_lock);
	
	if(b_on == true)
	{
		if(tcc_gpio_get(p_instance->res.i_si2166_power_gpio) == 0)
		{
			tcc_gpio_set(p_instance->res.i_si2166_power_gpio, 1);

			msleep(20);
		}

		tcc_gpio_set(p_instance->res.ia_si2166_reset_gpio[i_dev], 0);

		msleep(100);

		tcc_gpio_set(p_instance->res.ia_si2166_reset_gpio[i_dev], 1);

		msleep(20);

		if(tcc_gpio_get(p_instance->res.i_lnb_power_gpio) == 0)
		{
			tcc_gpio_set(p_instance->res.i_lnb_power_gpio, 1);
		}
	}
	else
	{		
		tcc_gpio_set(p_instance->res.ia_si2166_reset_gpio[i_dev], 0);

		for(i = 0; i < AVAIL_MAX_DEV_CNT; i++)
		{
			if(tcc_gpio_get(p_instance->res.ia_si2166_reset_gpio[i]) == 1)
			{
				b_off_power = false;
				break;
			}
		}

		if(b_off_power == true)
		{
			tcc_gpio_set(p_instance->res.i_si2166_power_gpio, 0);
			tcc_gpio_set(p_instance->res.i_lnb_power_gpio, 0);
		}
	}

	dprintk("[%s:%d] dev(%d) - power(%d) reset(%d) lnb(%d)\n", __func__, __LINE__, 
		i_dev, 
		tcc_gpio_get(p_instance->res.i_si2166_power_gpio), 
		tcc_gpio_get(p_instance->res.ia_si2166_reset_gpio[i_dev]), 
		tcc_gpio_get(p_instance->res.i_lnb_power_gpio));

	mutex_unlock(&p_instance->t_power_lock);
}

static int sfn_init_resource(si2166_t *p_instance)
{
	int i;
	int i_ret;

	char strTag[50];

	struct device_node *p_dnode;

	p_instance->b_res_init = false;

	p_instance->res.i_si2166_power_gpio = -1;

	memset(p_instance->res.ia_si2166_reset_gpio, -1, AVAIL_MAX_DEV_CNT * sizeof(int));

	p_instance->res.i_lnb_power_gpio = -1;
	
	i_ret = tcc_i2c_init();

	if(i_ret < 0)
	{
		eprintk("[%s:%d] tcc_i2c_init() failed!\n", __func__, __LINE__);
		return -1;
	}

	p_dnode = of_find_compatible_node(NULL, NULL, "telechips,si_power");

	if(p_dnode == NULL)
	{
		eprintk("[%s:%d] of_find_compatible_node() failed!\n", __func__, __LINE__);
		return -1;
	}

	p_instance->res.i_si2166_power_gpio = of_get_named_gpio(p_dnode, "pw-gpios", 0);

	if(!gpio_is_valid(p_instance->res.i_si2166_power_gpio))
	{
		eprintk("[%s:%d] power gpio is not available!\n", __func__, __LINE__);
		return -1;
	}

	tcc_gpio_init(p_instance->res.i_si2166_power_gpio, GPIO_DIRECTION_OUTPUT);

	for(i = 0; i < AVAIL_MAX_DEV_CNT; i++)
	{
		sprintf(strTag, "si%d-gpios", i);

		p_instance->res.ia_si2166_reset_gpio[i] = of_get_named_gpio(p_dnode, strTag, 1);

		if(!gpio_is_valid(p_instance->res.ia_si2166_reset_gpio[i]))
		{
			eprintk("[%s:%d] reset gpio[%d] is not available!\n", __func__, __LINE__, i);
			i_ret = -1;
			break;
		}

		tcc_gpio_init(p_instance->res.ia_si2166_reset_gpio[i], GPIO_DIRECTION_OUTPUT);
	}

	if(i_ret < 0)
	{
		return -1;
	}

	p_instance->res.i_lnb_power_gpio = of_get_named_gpio(p_dnode, "lnb-gpios", 0);

	if(!gpio_is_valid(p_instance->res.i_lnb_power_gpio))
	{
		eprintk("[%s:%d] lnb gpio is not available!\n", __func__, __LINE__);
		return -1;
	}

	tcc_gpio_init(p_instance->res.i_lnb_power_gpio, GPIO_DIRECTION_OUTPUT);

	p_instance->b_res_init = true;

	return 0;
}

static void sfn_deinit_resource(si2166_t *p_instance)
{
	int i;

	if(gpio_is_valid(p_instance->res.i_lnb_power_gpio))
	{
		tcc_gpio_free(p_instance->res.i_lnb_power_gpio);
	}

	for(i = 0; i < AVAIL_MAX_DEV_CNT; i++)
	{
		if(gpio_is_valid(p_instance->res.ia_si2166_reset_gpio[i]))
		{
			tcc_gpio_free(p_instance->res.ia_si2166_reset_gpio[i]);
		}
	}

	if(gpio_is_valid(p_instance->res.i_si2166_power_gpio))
	{
		tcc_gpio_free(p_instance->res.i_si2166_power_gpio);
	}
	
	tcc_i2c_deinit();
}

/*****************************************************************************
 * si2166 DVB FE Functions
 ******************************************************************************/
static int si2166_fe_sleep(struct dvb_frontend *fe)
{
	si2166_t *p_instance;

	dprintk("%s()\n", __func__);

	p_instance = gp_instance;
	
	sfn_power_ctrl(p_instance, fe->dvb->num, false);

	return 0;
} 
 
static void si2166_fe_release(struct dvb_frontend *fe)
{
	si2166_t *p_instance;

	dprintk("%s()\n", __func__);

	p_instance = gp_instance;
	
	sfn_power_ctrl(p_instance, fe->dvb->num, false);
}
 
static int si2166_fe_init(struct dvb_frontend *fe)
{
	int i_ret = 0;
	
	char str_tag[20];
	char str_msg[100];

	unsigned char uc_rbuf[100];

	L0_Context t_i2c;
	
	si2166_i2c_addr_t *p_i2c_addr;

	SILABS_FE_Context *p_fe_ctx;

	si2166_t *p_instance;

	dprintk("%s()\n", __func__);

	p_instance = gp_instance;

	sfn_power_ctrl(p_instance, fe->dvb->num, true);

	p_i2c_addr = &g_i2c_addr[fe->dvb->num];

	p_fe_ctx = &FrontEnd_Table[fe->dvb->num];

	sprintf(str_tag, "fe[%d]", fe->dvb->num);

	SiLabs_API_Frontend_Chip(p_fe_ctx, 0x2183);

	SiLabs_API_SW_Init(p_fe_ctx, p_i2c_addr->uc_demod, 0xc0, p_i2c_addr->uc_tuner);

	SiLabs_API_Select_SAT_Tuner(p_fe_ctx, 0xa2018, 0);

	SiLabs_API_SAT_Select_LNB_Chip(p_fe_ctx, 0xA8304, p_i2c_addr->uc_lnb);
	SiLabs_API_SAT_tuner_I2C_connection(p_fe_ctx, 0);
	SiLabs_API_SAT_Clock(p_fe_ctx, 2, 44, 27, 2);
	SiLabs_API_SAT_Spectrum(p_fe_ctx, 0);
	SiLabs_API_SAT_AGC(p_fe_ctx, 0xa, 1, 0x0, 1);

	SiLabs_API_Set_Index_and_Tag(p_fe_ctx, fe->dvb->num, str_tag);

	SiLabs_API_HW_Connect(p_fe_ctx, 2);

	L0_Init(&t_i2c);

	/* Check demod communication */
	sprintf(str_msg, "0x%02x 0x00 0x00 1", p_i2c_addr->uc_demod);

	i_ret = L0_ReadString(&t_i2c, str_msg, uc_rbuf);

	if(i_ret != 1)
	{
		eprintk("[%s:%d] Demod communication error! - [%d]\n", __func__, __LINE__, i_ret);
		return -1;
	}

	/* Check tuner communication */
	SiLabs_API_Tuner_I2C_Enable(p_fe_ctx);

	sprintf(str_msg, "0x%02x 0x00 1", p_i2c_addr->uc_tuner);

	i_ret = L0_ReadString(&t_i2c, str_msg, uc_rbuf);

	if(i_ret != 1)
	{
		eprintk("[%s:%d] Tuner communication error! - [%d]\n", __func__, __LINE__, i_ret);
		return -1;
	}

	return 0;
}
 
static int si2166_fe_tune(struct dvb_frontend *fe, bool re_tune, unsigned int mode_flags, unsigned int *delay, fe_status_t *status)
{
	int i_ret;
	
	CUSTOM_Status_Struct *ptStatus = &FE_Status;

	SILABS_FE_Context *p_fe_ctx = &FrontEnd_Table[fe->dvb->num];
	
	si2166_t *p_instance = gp_instance;

	si2166_priv_t *p_priv = p_instance->pa_priv[fe->dvb->num];

	dprintk("%s() %d 0x%x\n", __func__, re_tune, mode_flags);
	
	if(re_tune == true)
	{
		struct dtv_frontend_properties *c = &fe->dtv_property_cache;
		
		if(mode_flags & FE_TUNE_MODE_BSCAN_INIT)
		{
			dprintk("[%s:%d] Scan Init!\n", __func__, __LINE__);
			
			SiLabs_API_Channel_Seek_Init(p_fe_ctx, 950000, 2150000, 8000000, 0, 1000000, 45000000, 0, 0, 0, 0);

			SiLabs_API_Handshake_Setup(p_fe_ctx, 1, 0);

			*status = FE_REINIT;
		}
		else if(mode_flags & FE_TUNE_MODE_BSCAN_NEXT)
		{
			int i_standard = 0;
			int i_frequency = 0;
			int i_bandwidth_hz = 0;
			int i_stream = 0;
			unsigned int ui_symbol_rate_bps = 0;
			int i_constellation = 0;
			int i_polarization = 0;
			int i_band = 0;
			int i_num_data_slice = 0;
			int i_num_plp = 0;
			int i_t2_base_lite = 0;

			dprintk("[%s:%d] Scan Next!\n", __func__, __LINE__);

			i_ret = SiLabs_API_Channel_Seek_Next(
				p_fe_ctx, 
				&i_standard, 
				&i_frequency, 
				&i_bandwidth_hz, 
				&i_stream, 
				&ui_symbol_rate_bps, 
				&i_constellation, 
				&i_polarization, 
				&i_band, 
				&i_num_data_slice, 
				&i_num_plp, 
				&i_t2_base_lite);

			if(i_ret >= 1)
			{
				c->frequency 	= i_frequency;
				c->symbol_rate 	= ui_symbol_rate_bps;

				if(i_ret > 1)
				{
					*status = FE_REINIT;
				}
				else
				{
					*status = FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC;
				}
			}
			else
			{
				*status = FE_TIMEDOUT;
			}
		}
		else if(mode_flags & FE_TUNE_MODE_BSCAN_END)
		{
			dprintk("[%s:%d] Scan End!\n", __func__, __LINE__);
			
			SiLabs_API_Channel_Seek_End(p_fe_ctx);

			SiLabs_API_Handshake_Setup(p_fe_ctx, 0, 0);

			*status = FE_REINIT;
		}
		else
		{
			int i_lock;
			int i_standard;

			unsigned int ui_constellation;
			unsigned int ui_polarization;
			unsigned int ui_band;

			switch(c->delivery_system)
			{
			case SYS_DVBS:
				{
					dprintk("[%s:%d] standard : DVB-S\n", __func__, __LINE__);
					
					i_standard = SILABS_STANDARD_DVB_S;
				}
				break;
			case SYS_DVBS2:
			default:
				{
					dprintk("[%s:%d] standard : DVB-S2\n", __func__, __LINE__);
					
					i_standard = SILABS_STANDARD_DVB_S2;
				}
				break;
			}

			switch(c->modulation)
			{
			case QPSK:
				{
					dprintk("[%s:%d] constellation : QPSK\n", __func__, __LINE__);

					ui_constellation = SILABS_CONSTEL_QPSK;
				}
				break;
			case QAM_16:
				{
					dprintk("[%s:%d] constellation : QAM16\n", __func__, __LINE__);

					ui_constellation = SILABS_CONSTEL_QAM16;
				}
				break;
			case QAM_32:
				{
					dprintk("[%s:%d] constellation : QAM32\n", __func__, __LINE__);

					ui_constellation = SILABS_CONSTEL_QAM32;
				}
				break;	
			case QAM_64:
				{
					dprintk("[%s:%d] constellation : QAM64\n", __func__, __LINE__);

					ui_constellation = SILABS_CONSTEL_QAM64;
				}
				break;
			case QAM_128:
				{
					dprintk("[%s:%d] constellation : QAM128\n", __func__, __LINE__);

					ui_constellation = SILABS_CONSTEL_QAM128;
				}
				break;
			case QAM_256:
				{
					dprintk("[%s:%d] constellation : QAM256\n", __func__, __LINE__);

					ui_constellation = SILABS_CONSTEL_QAM256;
				}
				break;
			case QAM_AUTO:
				{
					dprintk("[%s:%d] constellation : QAMAUTO\n", __func__, __LINE__);

					ui_constellation = SILABS_CONSTEL_QAMAUTO;
				}
				break;
			case PSK_8:
				{
					dprintk("[%s:%d] constellation : 8PSK\n", __func__, __LINE__);

					ui_constellation = SILABS_CONSTEL_8PSK;
				}
				break;
			case APSK_16:
				{
					dprintk("[%s:%d] constellation : 16PSK\n", __func__, __LINE__);

					ui_constellation = SILABS_CONSTEL_16APSK;
				}
				break;
			case DQPSK:
				{
					dprintk("[%s:%d] constellation : DQPSK\n", __func__, __LINE__);

					ui_constellation = SILABS_CONSTEL_DQPSK;
				}
				break;
			case VSB_8:
			case VSB_16:
			case APSK_32:
			case QAM_4_NR:
			default:
				{
					eprintk("[%s:%d] constellation : invalid or unknonw (%d)\n", __func__, __LINE__, c->modulation);
					return -1;
				}
				break;			
			}

			switch(c->voltage)
			{
			case SEC_VOLTAGE_13:
				{
					dprintk("[%s:%d] volatage : 13\n", __func__, __LINE__);
					
					ui_polarization = SILABS_POLAR_VERTICAL;
				}
				break;
			case SEC_VOLTAGE_18:
				{
					dprintk("[%s:%d] volatage : 18\n", __func__, __LINE__);
					
					ui_polarization = SILABS_POLAR_HORIZONTAL;
				}
				break;
			default:
				{
					eprintk("[%s:%d] volatage : invalid(%d)\n", __func__, __LINE__, c->voltage);
					return -1;
				}
				break;
			}

			switch(c->sectone)
			{
			case SEC_TONE_ON:
				{
					dprintk("[%s:%d] tone : on\n", __func__, __LINE__);
					
					ui_band = SILABS_BAND_HIGH;
				}
				break;
			case SEC_TONE_OFF:
				{
					dprintk("[%s:%d] tone : off\n", __func__, __LINE__);
					
					ui_band = SILABS_BAND_LOW;
				}
				break;
			default:
				{
					eprintk("[%s:%d] tone : invalid(%d)\n", __func__, __LINE__, c->sectone);
					return -1;
				}
				break;
			}

			i_lock = SiLabs_API_lock_to_carrier(
				p_fe_ctx, 
				i_standard, 
				(int)c->frequency, 
				(int)c->bandwidth_hz, 
				(unsigned int)SILABS_STREAM_HP, 
				c->symbol_rate, 
				ui_constellation, 
				ui_polarization, 
				ui_band, 
				0, -1, 0);

			if(i_lock == 1)
			{
				dprintk("[%s:%d] lock ok!\n", __func__, __LINE__);
				
				SiLabs_API_FE_status_selection(p_fe_ctx, ptStatus, 0x00);

				SiLabs_API_TS_Mode(p_fe_ctx, SILABS_TS_SERIAL);

				*status = FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC;

				p_priv->i_lock_retry_cnt = 0;
			}
			else
			{
				dprintk("[%s:%d] lock fail!\n", __func__, __LINE__);
				
				*status = 0;

				p_priv->i_lock_retry_cnt = 10;
			}
		}
	}
	else
	{
		SiLabs_API_FE_status_selection(p_fe_ctx, ptStatus, FE_LOCK_STATE);
		
		if(ptStatus->demod_lock == 1)
		{
			*status = FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC;

			p_priv->i_lock_retry_cnt = 0;
		}
		else
		{
			*status = 0;

			if(p_priv->i_lock_retry_cnt > 0)
			{
				p_priv->i_lock_retry_cnt--;

				if(p_priv->i_lock_retry_cnt == 0)
				{
					dprintk("[%s:%d] lock timeout!\n", __func__, __LINE__);
					
					*status = FE_TIMEDOUT;
				}
			}
		}
	}

	if(p_priv->i_lock_retry_cnt > 0)
	{
		*delay = HZ / 100; // 10 ms
	}
	else
	{
		*delay = 3 * HZ; // 10 sess
	}

	return 0;
}

static enum dvbfe_algo si2166_fe_get_frontend_algo(struct dvb_frontend *fe)
{
	dprintk("%s()\n", __func__);

	return DVBFE_ALGO_HW;
}

static int si2166_fe_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	CUSTOM_Status_Struct *ptStatus = &FE_Status;
	
	SILABS_FE_Context *p_fe_ctx = &FrontEnd_Table[fe->dvb->num];

	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	dprintk("%s()\n", __func__);

	SiLabs_API_FE_status_selection(p_fe_ctx, ptStatus, 0x00);

	if(ptStatus->demod_lock == 1)
	{
		*status = FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC;
	}
	else
	{
		*status = 0;
	}

	c->ssi.len = 1;
	c->ssi.stat[0].scale = FE_SCALE_COUNTER;
	c->ssi.stat[0].svalue = ptStatus->SSI;

	c->sqi.len = 1;
	c->sqi.stat[0].scale = FE_SCALE_COUNTER;
	c->sqi.stat[0].svalue = ptStatus->SQI;

	c->ber.len = 1;
	c->ber.stat[0].scale = FE_SCALE_COUNTER;
	c->ber.stat[0].uvalue = (((__u64)ptStatus->ber_mant << 32) | ptStatus->ber_exp);

	c->rssi.len = 1;
	c->rssi.stat[0].scale = FE_SCALE_COUNTER;
	c->rssi.stat[0].svalue = ptStatus->rssi;

	c->cnr.len = 1;
	c->cnr.stat[0].scale = FE_SCALE_COUNTER;
	c->cnr.stat[0].svalue = ptStatus->c_n_100;

	c->per.len = 1;
	c->per.stat[0].scale = FE_SCALE_COUNTER;
	c->per.stat[0].uvalue = (((__u64)ptStatus->per_mant << 32) | ptStatus->per_exp);

	return  0;
}
 
static int si2166_fe_read_ber(struct dvb_frontend *fe, unsigned int *ber)
{
	dprintk("%s()\n", __func__);
	
	*ber = 0;

	return 0;
}
 
static int si2166_fe_read_signal_strength(struct dvb_frontend *fe, unsigned short *strength)
{
	dprintk("%s()\n", __func__);

	*strength = 0;

	return 0;
}

static int si2166_fe_read_snr(struct dvb_frontend *fe, unsigned short *snr)
{
	dprintk("%s()\n", __func__);

	*snr = 0;

	return 0;
}

static int si2166_fe_read_ucblocks(struct dvb_frontend *fe, unsigned int *ucblocks)
{
	dprintk("%s()\n", __func__);
	
	return 0;
}
 
static int si2166_fe_diseqc_reset_overload(struct dvb_frontend* fe)
{
	dprintk("%s()\n", __func__);
	
	return 0;
}
 
static int si2166_fe_diseqc_send_master_cmd(struct dvb_frontend *fe, struct dvb_diseqc_master_cmd *cmd)
{
	unsigned char cont_tone = 0;

	SILABS_FE_Context *p_fe_ctx = &FrontEnd_Table[fe->dvb->num];

	dprintk("%s()\n", __func__);
	
	if(p_fe_ctx->band == SILABS_BAND_HIGH)
	{
		cont_tone = 1;
	}
	else
	{
		cont_tone = 0;
	}

	SiLabs_API_SAT_send_diseqc_sequence(p_fe_ctx, (signed int)cmd->msg_len, cmd->msg, cont_tone, 0, 0, 1);

	return 0;
}
 
static int si2166_fe_diseqc_recv_slave_reply(struct dvb_frontend *fe, struct dvb_diseqc_slave_reply *reply)
{
	int i_ret = 0;

	int i_reply_len;

	SILABS_FE_Context *p_fe_ctx = &FrontEnd_Table[fe->dvb->num];

	dprintk("%s()\n", __func__);

	i_ret = SiLabs_API_SAT_read_diseqc_reply(p_fe_ctx, &i_reply_len, reply->msg);

	if(i_ret < 0)
	{
		reply->timeout = 1;
		reply->msg_len = 0;
	}
	else
	{
		reply->timeout = 0;
		reply->msg_len = (unsigned char)i_reply_len;
	}

	return 0;
}
 
static int si2166_fe_diseqc_send_burst(struct dvb_frontend *fe, fe_sec_mini_cmd_t minicmd)
{
	dprintk("%s()\n", __func__);
	
	return 0;
}
 
static int si2166_fe_set_tone(struct dvb_frontend *fe, fe_sec_tone_mode_t tone)
{
	SILABS_FE_Context *p_fe_ctx = &FrontEnd_Table[fe->dvb->num];

	dprintk("%s()\n", __func__);

	if(tone == SEC_TONE_ON)
	{
		p_fe_ctx->band = SILABS_BAND_HIGH;
		
		SiLabs_API_SAT_tone(p_fe_ctx, 1);

		dprintk("[%s:%d] tone : on\n", __func__, __LINE__);
	}
	else
	{
		p_fe_ctx->band = SILABS_BAND_LOW;
		
		SiLabs_API_SAT_tone(p_fe_ctx, 0);

		dprintk("[%s:%d] tone : off\n", __func__, __LINE__);
	}

	return 0;
}
 
static int si2166_fe_set_voltage(struct dvb_frontend *fe, fe_sec_voltage_t voltage)
{
	SILABS_FE_Context *p_fe_ctx = &FrontEnd_Table[fe->dvb->num];

	dprintk("%s()\n", __func__);

	switch(voltage)
	{
	case SEC_VOLTAGE_13:
		{
			p_fe_ctx->polarization = SILABS_POLARIZATION_VERTICAL;

			SiLabs_API_SAT_voltage(p_fe_ctx, 13);

			dprintk("[%s:%d] voltage : 13\n", __func__, __LINE__);
		}
		break;
	case SEC_VOLTAGE_18:
		{
			p_fe_ctx->polarization = SILABS_POLARIZATION_HORIZONTAL;

			SiLabs_API_SAT_voltage(p_fe_ctx, 18);

			dprintk("[%s:%d] voltage : 18\n", __func__, __LINE__);
		}
		break;
	default:
		{
			p_fe_ctx->polarization = SILABS_POLARIZATION_DO_NOT_CHANGE;

			SiLabs_API_SAT_voltage(p_fe_ctx, 0);

			dprintk("[%s:%d] voltage : 0\n", __func__, __LINE__);
		}
		break;
	}

	return 0;
}
 
static int si2166_fe_enable_high_lnb_voltage(struct dvb_frontend *fe, long arg)
{
	dprintk("%s()\n", __func__);
	
	return 0;
}

static int si2166_fe_i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	dprintk("%s()\n", __func__);
	
	return 0;
}
  
static int si2166_fe_get_frontend(struct dvb_frontend *fe, struct dtv_frontend_properties *props)
{
	dprintk("%s()\n", __func__);
	
	return 0;
}

static int si2166_fe_set_property(struct dvb_frontend *fe, struct dtv_property *tvp)
{
	int i_standard;
	
	SILABS_FE_Context *p_fe_ctx = &FrontEnd_Table[fe->dvb->num];
	
	dprintk("%s()\n", __func__);

	switch(tvp->cmd)
	{
	case DTV_DELIVERY_SYSTEM:
		{
			switch(tvp->u.data)
			{
			case SYS_DVBS:
				{
					dprintk("[%s:%d] standard : DVB-S\n", __func__, __LINE__);
				
					i_standard = SILABS_STANDARD_DVB_S;
				}
				break;
			case SYS_DVBS2:
			default:
				{
					dprintk("[%s:%d] standard : DVB-S2\n", __func__, __LINE__);
				
					i_standard = SILABS_STANDARD_DVB_S2;
				}
				break;
			}

			SiLabs_API_switch_to_standard(p_fe_ctx, i_standard, 0);
		}
		break;
	case DTV_DVBS_BSCAN_ABORT:
		{
			dprintk("[%s:%d] Abort Blind Scan!\n", __func__, __LINE__);
			
			SiLabs_API_Channel_Seek_Abort(p_fe_ctx);

			SiLabs_API_Handshake_Setup(p_fe_ctx, 0, 0);
		}
		break;
	default:
		break;
	}	
	
	return 0;
}

static int si2166_fe_get_property(struct dvb_frontend *fe, struct dtv_property *tvp)
{
	dprintk("%s()\n", __func__);
	
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
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 | FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 
			| FE_CAN_FEC_AUTO | FE_CAN_QPSK | FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO 
			| FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO | FE_CAN_RECOVER | FE_CAN_MUTE_TS 
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

static int si2166_probe(tcc_fe_priv_t *p_fe_priv)
{
	struct dvb_frontend *p_fe = &p_fe_priv->fe;
	
	si2166_priv_t *p_priv;
	
	si2166_t *p_instance = gp_instance;

	dprintk("%s()\n", __func__);

	p_priv = kzalloc(sizeof(si2166_priv_t), GFP_KERNEL);

	if(p_priv == NULL)
	{
		eprintk("[%s:%d] kzalloc() failed!\n", __func__, __LINE__);
		return -1;
	}

	p_instance->pa_priv[p_fe->dvb->num] = p_priv;
	
	return 0;
}

static struct tcc_dxb_fe_driver si2166_driver = {
	.probe = si2166_probe,
	.compatible = "telechips,si_power",
	.fe_ops = &si2166_fe_ops,
};

static int __init si2166_module_init(void)
{
	int i;
	int i_ret = 0;
	
	si2166_t *p_instance;
	
	dprintk("%s()\n", __func__);

	p_instance = (si2166_t *)kzalloc(sizeof(si2166_t), GFP_KERNEL);

	if(p_instance == NULL)
	{
		eprintk("[%s:%d] kzalloc() failed!\n", __func__, __LINE__);
		i_ret = -1;
		goto err;
	}

	mutex_init(&p_instance->t_power_lock);

	i_ret = sfn_init_resource(p_instance);

	if(i_ret < 0)
	{
		eprintk("[%s:%d] sfn_init_resource() failed!\n", __func__, __LINE__);
		goto err;
	}

	gp_instance = p_instance;

	i_ret = tcc_fe_register(&si2166_driver);

	if(i_ret != 0)
	{
		eprintk("[%s:%d] tcc_fe_register() failed!\n", __func__, __LINE__);
		goto err;
	}

	return 0;

err:

	if(p_instance != NULL)
	{
		sfn_deinit_resource(p_instance);
		
		for(i = 0; i < AVAIL_MAX_DEV_CNT; i++)
		{
			if(p_instance->pa_priv[i] != NULL)
			{
				kfree(p_instance->pa_priv[i]);
			}
		}
		
		kfree(p_instance);
	}

	gp_instance = NULL;

	return i_ret;
}

static void __exit si2166_module_exit(void)
{
	int i;
	
	si2166_t *p_instance;
	
	dprintk("%s()\n", __func__);

	tcc_fe_unregister(&si2166_driver);

	p_instance = gp_instance;

	if(p_instance != NULL)
	{
		sfn_deinit_resource(p_instance);

		for(i = 0; i < AVAIL_MAX_DEV_CNT; i++)
		{
			if(p_instance->pa_priv[i] != NULL)
			{
				kfree(p_instance->pa_priv[i]);
			}
		}
		
		kfree(p_instance);
	}

	gp_instance = NULL;
}

module_init(si2166_module_init);
module_exit(si2166_module_exit);

MODULE_DESCRIPTION("TCC DVBS2 (si2166");
MODULE_AUTHOR("CE Android");
MODULE_LICENSE("GPL");
