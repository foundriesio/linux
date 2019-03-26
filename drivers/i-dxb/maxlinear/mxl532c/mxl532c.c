/*
 *  mxl532C.c
 *
 *  Written by C2-G1-3T
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

//#include <linux/of_i2c.h>
#include <linux/i2c.h>

#define __DVB_CORE__
#include "dvb_frontend.h"
//#include "tcc_fe.h"
#include "frontend.h"

#include "mxl532c.h"
#include "demod_MxL532C.h"

/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG    "[MXL532C]"

static int dev_debug = 0;

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


/*****************************************************************************
 * structures
 ******************************************************************************/
typedef struct mxl532C_demod_t {
	struct i2c_adapter *i2c;

	int wait;
} mxl532C_demod_t;

typedef struct mxl532C_priv_t {
	struct mxl532C_demod_t demod;
} mxl532C_priv_t;


/*****************************************************************************
 * Variables
 ******************************************************************************/


/*****************************************************************************
 * External Functions
 ******************************************************************************/


/*****************************************************************************
 * Functions
 ******************************************************************************/
static mxl532C_priv_t *gPriv;

int mxl532C_get_config(struct mxl532C_fe_config *cfg)
{
    if (!gPriv->demod.i2c)
    {
        eprintk("%s(cannot get i2c adaptor)\n", __FUNCTION__);
        return 0;
    }

    cfg->i2c_adapter = gPriv->demod.i2c;

	return 1;
}
/*
static void mxl532C_fe_get_information(struct dvb_frontend* fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	MXL_DEMOD_TPS_INFO_T tpsCellIdInfo;

	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_CONSTELLATION_REQ, &tpsCellIdInfo);
	switch (tpsCellIdInfo.TpsInfo)
	{
		case 0:     c->modulation = QPSK;       break;
		case 1:     c->modulation = QAM_16;     break;
		case 2:     c->modulation = QAM_64;     break;
	}

	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_GUARD_INTERVAL_REQ, &tpsCellIdInfo);
	switch (tpsCellIdInfo.TpsInfo)
	{
		case 0:     c->guard_interval = GUARD_INTERVAL_1_32;      break;
		case 1:     c->guard_interval = GUARD_INTERVAL_1_16;      break;
		case 2:     c->guard_interval = GUARD_INTERVAL_1_8;       break;
		case 3:     c->guard_interval = GUARD_INTERVAL_1_4;       break;
	}

	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_CODE_RATE_REQ, &tpsCellIdInfo);
	switch (tpsCellIdInfo.TpsInfo)
	{
		case 0:     c->code_rate_HP = FEC_1_2;  break;
		case 1:     c->code_rate_HP = FEC_2_3;  break;
		case 2:     c->code_rate_HP = FEC_3_4;  break;
		case 3:     c->code_rate_HP = FEC_5_6;  break;
		case 4:     c->code_rate_HP = FEC_7_8;  break;
	}

	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_FFT_MODE_REQ, &tpsCellIdInfo);
	switch (tpsCellIdInfo.TpsInfo)
	{
		case 0:     c->transmission_mode = TRANSMISSION_MODE_2K;  break;
		case 1:     c->transmission_mode = TRANSMISSION_MODE_8K;  break;
		case 2:     c->transmission_mode = TRANSMISSION_MODE_4K;  break;
	}
}
*/

/*****************************************************************************
 * MXL532C Frontend Functions
 ******************************************************************************/
static void mxl532C_fe_release(struct dvb_frontend *fe)
{
	mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;

	if (priv)
	{
		kfree(priv);
	}

	dprintk("%s\n", __FUNCTION__);
}

static int mxl532C_fe_init(struct dvb_frontend *fe)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;

	MxL532C_Init();

	dprintk("%s\n", __FUNCTION__);

	return 0;
}

static int mxl532C_fe_sleep(struct dvb_frontend *fe)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;

	dprintk("%s\n", __FUNCTION__);

	return 0;
}

static int mxl532C_fe_tune(struct dvb_frontend* fe, bool re_tune, unsigned int mode_flags, unsigned int *delay, fe_status_t *status)
{
	mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	UINT8 bandwidth;

	if (re_tune)
	{
		switch(c->bandwidth_hz)
	    {
	    case 6000000:
			bandwidth = 6;
			break;
		case 7000000:
			bandwidth = 7;
			break;
	    case 8000000:
	    default:
			bandwidth = 8;
			break;
		}

		MxL532C_Tune(c->frequency*1000, bandwidth);

		priv->demod.wait = 200; // 2s
		*status = 0;

		dprintk("%s(frequency = %d Hz, bandwidth_hz = %d Hz)\n", __FUNCTION__, c->frequency, c->bandwidth_hz);
	}
	else
	{
		fe->ops.read_status(fe, status);
		if (*status != 0)
		{
			priv->demod.wait = 0;
		}
		if (priv->demod.wait > 0)
		{
			priv->demod.wait--;
			if (priv->demod.wait == 0)
			{
				*status = FE_TIMEDOUT;
			}
		}
	}

	if (priv->demod.wait > 0)
	{
		*delay = HZ / 100; // 10 ms
	}
	else
	{
		*delay = 3 * HZ; // 3 s
	}

	return 0;
}

static enum dvbfe_algo mxl532C_fe_get_frontend_algo(struct dvb_frontend *fe)
{
	return DVBFE_ALGO_HW;
}

static int mxl532C_fe_get_frontend(struct dvb_frontend *fe)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;

	dprintk("%s\n", __FUNCTION__);

	return 0;
}

static int mxl532C_fe_read_status(struct dvb_frontend* fe, fe_status_t* status)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;

	if(MxL532C_GetLock() == 1)
	{
		*status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	}
	else
	{
		*status = FE_TIMEDOUT;
	}

	dprintk("%s(status = %d)\n", __FUNCTION__, *status);

	return  0;
}

static int mxl532C_fe_read_ber(struct dvb_frontend* fe, u32* ber)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;

	*ber = MxL532C_GetBER();

	dprintk("%s(ber = %d)\n", __FUNCTION__, *ber);

	return 0;
}

static int mxl532C_fe_read_signal_strength(struct dvb_frontend* fe, u16* strength)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;

	*strength = MxL532C_GetSigStrength();

	dprintk("%s(strength = %d)\n", __FUNCTION__, *strength);

	return 0;
}

static int mxl532C_fe_read_snr(struct dvb_frontend* fe, u16* snr)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;

	*snr = MxL532C_GetSNR();

	dprintk("%s(snr = %d)\n", __FUNCTION__, *snr);

	return 0;
}

static int mxl532C_fe_read_ucblocks(struct dvb_frontend* fe, u32* ucblocks)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;
	int ret;

	ret = mxl532C_fe_read_ber(fe, ucblocks);

	dprintk("%s(ucblocks = %d, ret = %d\n", __FUNCTION__, *ucblocks, ret);

	return ret;
}

static int mxl532C_fe_i2c_gate_ctrl(struct dvb_frontend* fe, int enable)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;

	dprintk("%s\n", __FUNCTION__);

	return 0;
}

static int mxl532C_fe_set_property(struct dvb_frontend* fe, struct dtv_property* tvp)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;
	//const struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	dprintk("%s(CMD = %d)\n", __FUNCTION__, tvp->cmd);

	return 0;
}

static int mxl532C_fe_get_property(struct dvb_frontend* fe, struct dtv_property* tvp)
{
	//mxl532C_priv_t *priv = (mxl532C_priv_t *)fe->demodulator_priv;
	//const struct dtv_frontend_properties *c = &fe->dtv_property_cache;
/*
	switch(tvp->cmd)
	{
	case DTV_FREQUENCY:
		mxl532C_fe_get_information(fe);
		break;
	}
*/
	dprintk("%s(CMD = %d)\n", __FUNCTION__, tvp->cmd);

	return 0;
}


/*****************************************************************************
 * MXL532C OPS
 ******************************************************************************/
static struct dvb_frontend_ops mxl532C_fe_ops = {

	.info = {
		.name = "TCC DVB-S2 (MxL532C)",
		.type = FE_OFDM,
		.frequency_min = 51000,
		.frequency_max = 858000,
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

	.delsys = { SYS_DVBT },

	.release = mxl532C_fe_release,

	.init = mxl532C_fe_init,
	.sleep = mxl532C_fe_sleep,

	.tune = mxl532C_fe_tune,
	.get_frontend_algo = mxl532C_fe_get_frontend_algo,

	.get_frontend = mxl532C_fe_get_frontend,

	.read_status = mxl532C_fe_read_status,
	.read_ber = mxl532C_fe_read_ber,
	.read_signal_strength = mxl532C_fe_read_signal_strength,
	.read_snr = mxl532C_fe_read_snr,
	.read_ucblocks = mxl532C_fe_read_ucblocks,

	.i2c_gate_ctrl = mxl532C_fe_i2c_gate_ctrl,

	.set_property = mxl532C_fe_set_property,
	.get_property = mxl532C_fe_get_property,
};


/*****************************************************************************
 * MXL532C Init
 ******************************************************************************/
static int mxl532C_init(struct device_node *node, mxl532C_priv_t *priv)
{
	struct device_node *adapter_np;

	adapter_np = of_parse_phandle(node, "i2c-parent", 0);
	if (adapter_np)
	{
		priv->demod.i2c = of_find_i2c_adapter_by_node(adapter_np);
	}

	dprintk("%s\n", __FUNCTION__);

	return 0;
}


/*****************************************************************************
 * MXL532C Probe
 ******************************************************************************/
static int mxl532C_probe(tcc_fe_priv_t *inst)
{
	mxl532C_priv_t *priv = kzalloc(sizeof(mxl532C_priv_t), GFP_KERNEL);
	int retval;

	if (priv == NULL)
	{
		eprintk("%s(kzalloc fail)\n", __FUNCTION__);
		return -1;
	}

	retval = mxl532C_init(inst->of_node, priv);
	if (retval != 0)
	{
		eprintk("%s(Init Error %d)\n", __FUNCTION__, retval);
		kfree(priv);
		return -1;
	}

	inst->fe.demodulator_priv = priv;

	gPriv = priv;

	dprintk("%s\n", __FUNCTION__);

	return 0;
}


/*****************************************************************************
 * MXL532C Module Init/Exit
 ******************************************************************************/
static struct tcc_dxb_fe_driver mxl532C_driver = {
	.probe = mxl532C_probe,
	.compatible = "maxlinear,mxl532C",
	.fe_ops = &mxl532C_fe_ops,
};

static int __init mxl532C_module_init(void)
{
	if (tcc_fe_register(&mxl532C_driver) != 0)
		return -EPERM;

	dprintk("%s\n", __FUNCTION__);

	return 0;
}

static void __exit mxl532C_module_exit(void)
{
	tcc_fe_unregister(&mxl532C_driver);

	dprintk("%s\n", __FUNCTION__);
}

module_init(mxl532C_module_init);
module_exit(mxl532C_module_exit);

MODULE_DESCRIPTION("TCC DVBS2 (MxL532C)");
MODULE_AUTHOR("Telechips");
MODULE_LICENSE("GPL");
