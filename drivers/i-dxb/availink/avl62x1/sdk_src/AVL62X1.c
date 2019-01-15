/*
 *  avl62x1.c
 *
 *  Written by CE
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

#include "AVL62X1_API.h"
#include "AVL_Tuner.h"
#include "AV201X.h"
#include "AV201X_MoreAPI.h"

#include "tcc_gpio.h"
#include "frontend.h"


/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG    "[AVL62x1]"
//#define AUTO_TEST

static int dev_debug = 1;
static unsigned int g_freq = 2000;
static unsigned int g_symbol = 27500;
static unsigned int g_test = 0;

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
// tune_mode
#define TUNE_MODE_NORMAL              0
#define TUNE_MODE_BLINDSCAN           1

// blindscan_state
#define BLINDSCAN_STATE_STOP          0
#define BLINDSCAN_STATE_START         1
#define BLINDSCAN_STATE_CHECK_STATE   2
#define BLINDSCAN_STATE_GET_PROGRESS  3
#define BLINDSCAN_STATE_GET_INFO      4
#define BLINDSCAN_STATE_CANCEL        5

//a8304
#define LNB_I2C_SLAVE_ADDR	0x08

/* Private Macros ------------------------------------------------------ */
#define A8304_I2CID_BASE 0X00

#define A8304_DATA_POWER_13V     0x2  //13.667
#define A8304_DATA_POWER_14V     0x5  //14.333

#define A8304_DATA_POWER_18V     0xB  //18.667 
#define A8304_DATA_POWER_19V     0xD  //19.333


#define A8304_BIT_LNB_OFF           (0x0 << 4)
#define A8304_BIT_LNB_ON            (0x1<< 4)
#define A8304_BIT_LNB_22K            (0x1<<5) // to use extern 22k
//#define A8304_BIT_LNB_22K            (0x0)  //to use intern 22k

#define A8304_DATA_13V_OUT_INTERNAL                  ((A8304_DATA_POWER_13V | A8304_BIT_LNB_ON)&(~A8304_BIT_LNB_22K))
#define A8304_DATA_18V_OUT_INTERNAL                  ((A8304_DATA_POWER_18V | A8304_BIT_LNB_ON)&(~A8304_BIT_LNB_22K))
#define A8304_DATA_14V_OUT_INTERNAL                ((A8304_DATA_POWER_14V | A8304_BIT_LNB_ON)&(~A8304_BIT_LNB_22K))
#define A8304_DATA_19V_OUT_INTERNAL                ((A8304_DATA_POWER_19V | A8304_BIT_LNB_ON)&(~A8304_BIT_LNB_22K))

#define A8304_DATA_13V_OUT_EXTERNAL                ((A8304_DATA_POWER_13V | A8304_BIT_LNB_ON)|A8304_BIT_LNB_22K)
#define A8304_DATA_18V_OUT_EXTERNAL                ((A8304_DATA_POWER_18V | A8304_BIT_LNB_ON)|A8304_BIT_LNB_22K)
#define A8304_DATA_14V_OUT_EXTERNAL                ((A8304_DATA_POWER_14V | A8304_BIT_LNB_ON)|A8304_BIT_LNB_22K)
#define A8304_DATA_19V_OUT_EXTERNAL                ((A8304_DATA_POWER_19V | A8304_BIT_LNB_ON)|A8304_BIT_LNB_22K)

#define A8304_DATA_LNBPOWER_OFF         A8304_BIT_LNB_OFF

#define A8304_LNB_STATS_REG_ADD 0x00
#define A8304_LNB_CTL_REG_ADD   0x00

#define A8304_LNB_STATUS            0x01
#define A8304_LNB_GET_CPOK_STATUS  0x02
#define A8304_LNB_GET_OC_STATUS    0x04
#define A8304_LNB_GET_PNG_STATUS   0x10
#define A8304_LNB_GET_TDS_STATUS   0x40
#define A8304_LNB_GET_UVLO_STATUS  0x80


typedef enum
{
    USM_FE_22K_FROM_DEMOD,
    USM_FE_22K_FROM_LNB
} USM_FE_22K_Source_t;

/*****************************************************************************
 * structures
 ******************************************************************************/
typedef struct avl62x1_demod_t {
	struct i2c_adapter *i2c;
	struct i2c_adapter *lnb_i2c;

	int wait;
	int blindscan_state;

	struct AVL62X1_Chip AVLChip;
	struct AVL_Tuner Tuner;
	struct AVL62X1_BlindScanParams BSPara;
	struct AVL62X1_BlindScanInfo BSInfo;
	AVL_uint32 cur_freq_100kHz;
	int index;
	int config_idx;

	int gpio_fe_power;
	int gpio_fe_reset;
	int gpio_fe_lock;
	int gpio_fe_fault;
	int gpio_lnb_power;
	int gpio_lnb_sp1v;
	int gpio_lnb_s18v;
	int gpio_ant_ctrl;
	int lnb_int_port;
	int lnb_high_voltage;
	int lnb_source22k;
	unsigned char lnb_control;
	//int lnb_int_irq;
	//struct workqueue_struct *lnb_wq;
	#ifdef AUTO_TEST
	struct timer_list timer;
	#endif
} avl62x1_demod_t;

typedef struct avl62x1_priv_t {
	struct avl62x1_demod_t demod;
	int tune_mode;
	struct work_struct  lnb_work;
} avl62x1_priv_t;

#define AVL62X1_Chip_Count      1
typedef struct AVL_Frontend
{
	struct AVL62X1_Chip * pAVL62X1_Chip;
	struct AVL_Tuner * pTuner;
}AVL_Frontend;

extern AVL_uchar ucPatchData[];
/*Customer define */
static struct AVL_Tuner gTuner1 = 
{
	0x62,                        ///< I2C slave address.
	0,                           ///< Tuner lock status.  1 Lock; 0 unlock

	1000*1000*1000,              ///< Tuner center frequency. Unit:Hz
	34*1000*1000,                ///< Tuner low pass filter bandwidth. Unit:Hz

	0,                           ///< Not in blindscan mode

	NULL,                        ///< Other parameters.

	&AV201X_Initialize,       ///< Pointer to the tuner initialization function.
	&AV201X_Lock,             ///< Pointer to the tuner Lock function.
	&AV201X_GetLockStatus,    ///< Pointer to the tuner GetLockStatus function.
	NULL,                     ///< Pointer to the tuner GetRFStrength function.
	&AV201X_GetMaxLPF,        ///< Pointer to the tuner GetMaxLPF function
	&AV201X_GetMinLPF,        ///< Pointer to the tuner GetMinLPF function
	&AV201X_GetLPFStepSize,   ///< Pointer to the tuner GetLPFStepSize function
	&AV201X_GetAGCSlope,      ///< Pointer to the tuner GetAGCSlope function
	&AV201X_GetMinGainVoltage,///< Pointer to the tuner GetMinGainVoltage function
	&AV201X_GetMaxGainVoltage,///< Pointer to the tuner GetMaxGainVoltage function
	&AV201X_GetRFFreqStepSize ///< Pointer to the tuner GetRFFreqStepSize function
};

static struct AVL62X1_Chip g_AVL62X1_Chip1 = 
{
	AVL62X1_SA_0,                   ///< Device I2C slave address.
	AVL62X1_RefClk_27M,             ///< Configures the Availink device's reference clock.Refer to enum AVL62X1_Xtal.
	ucPatchData,                 ///< Point to Availink device firmware.

	&gTuner1,                    ///< Pointer to tuner struct
	AVL62X1_Spectrum_Normal,        ///< Defines the spectrum polarity. Refer to enum AVL62X1_SpectrumPolarity.

	AVL62X1_MPM_Serial,             ///< The MPEG output mode. Refer to enum AVL62X1_MpegMode.		//TCC changed default value from AVL62X1_MPM_Parallel,
	AVL62X1_MPCP_Rising,            ///< The MPEG output clock polarity. Refer to enum AVL62X1_MpegClockPolarity. The clock polarity should be configured to meet the back end device's requirement.
	AVL62X1_MPF_TS,                 ///< The MPEG output format. Refer to enum AVL62X1_MpegFormat.
	AVL62X1_MPSP_DATA0,             ///< The MPEG output pin. Refer to enum AVL62X1_MpegSerialPin. Only valid when the MPEG interface has been configured to operate in serial mode.
	135000000,                    ///< The MPEG output clock frequency.
};

#if (AVL62X1_Chip_Count == 2)
static struct AVL_Tuner gTuner2 = 
{
	0x62,
	0,

	1000*1000*1000,
	34*1000*1000,

	0,

	NULL,

	&AV201X_Initialize,
	&AV201X_Lock,
	&AV201X_GetLockStatus,
	NULL,
	&AV201X_GetMaxLPF,
	&AV201X_GetMinLPF,
	&AV201X_GetLPFStepSize,
	&AV201X_GetAGCSlope,
	&AV201X_GetMinGainVoltage,
	&AV201X_GetMaxGainVoltage,
	&AV201X_GetRFFreqStepSize
};

static struct AVL62X1_Chip g_AVL62X1_Chip2 = 
{
	AVL62X1_SA_1,
	AVL62X1_RefClk_27M,
	ucPatchData,

	&gTuner2,
	AVL62X1_Spectrum_Normal,

	AVL62X1_MPM_Parallel,
	AVL62X1_MPCP_Rising,
	AVL62X1_MPF_TS,
	AVL62X1_MPSP_DATA0,
	135000000,
};
#endif

/*
static struct AVL_Frontend AVL_FE[AVL62X1_Chip_Count] = 
{
	{&g_AVL62X1_Chip1, &gTuner1},
#if (AVL62X1_Chip_Count == 2)
	{&g_AVL62X1_Chip2, &gTuner2}
#endif
};
*/
//BlindScan Parameters
#define BS_Start_Freq_KHz      950*1000
#define BS_Stop_Freq_KHz       2150*1000
#define BS_Min_SR_Ksps         2000

/*
static struct AVL62X1_CarrierInfo g_CarrierInfo[256];
static struct AVL62X1_StreamInfo g_StreamInfo[1024];
static AVL_uint16 g_CarrierNum = 0;
static AVL_uint16 g_StreamNum = 0;
*/
/*End customer define */

/*****************************************************************************
 * Variables
 ******************************************************************************/


/*****************************************************************************
 * External Functions
 ******************************************************************************/


 /*****************************************************************************
 * Functions
 ******************************************************************************/
  static ssize_t avl_freq_store(struct device *_dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	char *s;
	g_freq = simple_strtol(buf, &s, 10);

	printk("g_freq=%d\n", g_freq);

	return count;
}

DEVICE_ATTR(freq, S_IRUGO | S_IWUSR, NULL, avl_freq_store);

 static ssize_t avl_symbol_store(struct device *_dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	char *s;
	g_symbol = simple_strtol(buf, &s, 10);

	printk("g_symbol=%d\n", g_symbol);

	return count;
}

DEVICE_ATTR(symbol, S_IRUGO | S_IWUSR, NULL, avl_symbol_store);

 static ssize_t avl_test_store(struct device *_dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	if (!strncmp(buf, "run", 3)){
		printk("Set test mode\n");
		g_test = 1;
	}
	else{
		printk("Clear test mode\n");
		g_test = 0;
	}

	return count;
}

DEVICE_ATTR(test, S_IRUGO | S_IWUSR, NULL, avl_test_store);

#ifdef AUTO_TEST
/* Group all the eHCI SQ attributes */
static struct attribute *dvb_sq_attrs[] = {
		&dev_attr_test.attr,
		&dev_attr_freq.attr,
		&dev_attr_symbol.attr,
		NULL,
};

static struct attribute_group dvb_attr_group = {
	.name = NULL,	/* we want them in the same directory */
	.attrs = dvb_sq_attrs,
};
#endif

static int AVL62x1_Reset(avl62x1_demod_t *demod)
{
	tcc_gpio_set(demod->gpio_fe_reset, 0);
	AVL_IBSP_Delay(300);
	tcc_gpio_set(demod->gpio_fe_reset, 1);
	return 0;
}

static int AVL62x1_PowerCtrl(avl62x1_demod_t *demod, int on)
{
	tcc_gpio_set(demod->gpio_fe_power, on);
	return 0;
}
/*
static int AVL62x1_LnbPowerCtrl(avl62x1_demod_t *demod, int on)
{
	tcc_gpio_set(demod->gpio_lnb_power, on);
	return 0;
}
*/
static int AVL62x1_I2C_Read(avl62x1_priv_t *priv, AVL_uint16 uiSlaveAddr,  AVL_puchar pucBuff, AVL_puint16 puiSize)
{
	int nRetCode = 0;
	struct i2c_msg msg[1];

	if(pucBuff == 0 || *puiSize == 0)
	{
		printk("avl6211 read register parameter error !!\n");
		return(1);
	}

	//read real data 
	memset(msg, 0, sizeof(msg));
	msg[0].addr = uiSlaveAddr;
	msg[0].flags |= I2C_M_RD;  //write  I2C_M_RD=0x01
	msg[0].len = *puiSize;
	msg[0].buf = pucBuff;

	nRetCode = i2c_transfer(priv->demod.lnb_i2c, msg, 1);
	if(nRetCode != 1)
	{
		printk("avl6211_readregister reg failure!\n");
		return(1);
	}
	return(0);
}

static int AVL62x1_I2C_Write(avl62x1_priv_t *priv, AVL_uint16 uiSlaveAddr,  AVL_puchar pucBuff, AVL_puint16 puiSize)
{
	int nRetCode = 0;
	struct i2c_msg msg; //construct 2 msgs, 1 for reg addr, 1 for reg value, send together

	if(pucBuff == 0 || *puiSize == 0)
	{
		printk("avl6211 write register parameter error !!\n");
		return(1);
	}

	//write value
	memset(&msg, 0, sizeof(msg));
	msg.addr = uiSlaveAddr;
	msg.flags = 0;	//I2C_M_NOSTART;
	msg.buf = pucBuff;
	msg.len = *puiSize;

	nRetCode = i2c_transfer(priv->demod.lnb_i2c, &msg, 1);
	if(nRetCode < 0)
	{
		printk(" %s: writereg error, errno is %d \n", __FUNCTION__, nRetCode);
		return(1);
	}
	return(0);
}
/*
static int AVL62x1_lnb_get_irq(avl62x1_priv_t *priv)
{
	int nRetCode = 0;
	int level = 0;

	level = gpio_get_value(priv->demod.lnb_int_port);
	if (level)
	{
		dprintk("8304 irq has not incurred! level:%d\n", level);
	}
	else
	{
		dprintk("8304 irq has incurred!level:%d\n", level);
	}

	return level;
}
*/
static int AVL62x1_lnb_read_status(avl62x1_priv_t *priv, unsigned char *pStatus)
{
	AVL_uchar nValue[2];
	AVL_uint16 len;
	int nRetCode = 0;

	nValue[0] = A8304_LNB_STATS_REG_ADD;
	len = 1;

	nRetCode = AVL62x1_I2C_Write(priv, LNB_I2C_SLAVE_ADDR, nValue, &len);

	if (nRetCode != 0)
	{
		printk(" A8304_LNB_I2C_READ_STATUS(): AVL62x1_I2C_Write FAILURE!!!\n");
		return nRetCode;
	}

	nRetCode = AVL62x1_I2C_Read(priv, LNB_I2C_SLAVE_ADDR, pStatus, &len);

	if (nRetCode != 0)
	{
		printk(" A8304_LNB_I2C_READ_STATUS(): AVL62x1_I2C_Read FAILURE!!!\n");
		return nRetCode;
	}

	//dprintk("%s status=0x%x\n", __FUNCTION__, pStatus[0]);

	return 0;
}

/*
static irqreturn_t avl62x1_lnb_isr(int irq, void *dev_id)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)dev_id;
	//queue_work(priv->demod.lnb_wq, &priv->lnb_work);
	return IRQ_HANDLED;
}
*/

#if 0
static void avl62x1_lnb_work(struct work_struct *work)
{
	avl62x1_priv_t *priv = container_of(work, avl62x1_priv_t, lnb_work);
	AVL_uchar nValue[2];
	AVL_uint16 len;
	
	nValue[0] = 0;
	len = 1;
	
	AVL62x1_I2C_Write(priv, LNB_I2C_SLAVE_ADDR, nValue, &len);
	AVL62x1_I2C_Read(priv, LNB_I2C_SLAVE_ADDR, nValue, &len);
	dprintk("%s status=%d\n", __FUNCTION__, nValue[0]);
}
#endif

/*****************************************************************************
 * AVL6211 Frontend Functions
 ******************************************************************************/
static void avl62x1_fe_release(struct dvb_frontend* fe)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	if (priv)
	{
		tcc_gpio_free(priv->demod.gpio_fe_power);
		tcc_gpio_free(priv->demod.gpio_fe_reset);
		tcc_gpio_free(priv->demod.gpio_lnb_power);
		
		tcc_gpio_free(priv->demod.gpio_fe_lock);
		tcc_gpio_free(priv->demod.lnb_int_port);
		kfree(priv);
	}

	dprintk("%s\n", __FUNCTION__);
}

static AVL_ErrorCode avl62x1_fe_init(struct dvb_frontend* fe)
{
	AVL_ErrorCode r = AVL_EC_OK;
	struct AVL62X1_Chip * pAVL_Chip;
	struct AVL_Tuner * pTuner;
	AVL_uint32 ChipID;
	struct AVL62X1_Diseqc_Para Diseqc_para;
	struct AVL62X1_VerInfo ver_info;
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	priv->tune_mode = TUNE_MODE_NORMAL;
	priv->demod.blindscan_state = BLINDSCAN_STATE_STOP;
	
	//tcc_gpio_init(priv->demod.gpio_fe_lock, GPIO_DIRECTION_INPUT);
	//tcc_gpio_init(priv->demod.lnb_int_port, GPIO_DIRECTION_INPUT);
	
	priv->demod.lnb_high_voltage = 0;
	priv->demod.lnb_source22k = USM_FE_22K_FROM_DEMOD;
	priv->demod.lnb_control = 0;

	g_eAV201X_TunerModel = e_AV201X_Model_AV2018;

#if defined(AVL_I2C_DEFINE)
	//Availink BSP Init
	r = AVL_IBSP_Initialize("localhost", 88, nullptr, 0);
	if( AVL_EC_OK != r )
	{
		eprintk("Availink BSP Initialization failed !\n");
		goto error;
	}
#else
	r = AVL_IBSP_Initialize();
	if( AVL_EC_OK != r )
	{
		eprintk("Availink BSP Initialization failed !\n");
		goto error;
	}
#endif

	//Reset Demod
	/*r = AVL_IBSP_Reset();
	if( AVL_EC_OK != r )
	{
		eprintk("Failed to Resed demod via BSP!\n");
		return r;
	}*/
	AVL62x1_Reset(&priv->demod);
	AVL_IBSP_Delay(100);
	AVL62x1_PowerCtrl(&priv->demod, 1);

	pAVL_Chip = &priv->demod.AVLChip;
	pTuner = &priv->demod.Tuner;

	r |= Init_AVL62X1_ChipObject(pAVL_Chip);
	if ( AVL_EC_OK != r ) 
	{
		eprintk("Chip Object Initialization failed !\n");
		goto error;
	}

	r |= AVL62X1_GetChipID(pAVL_Chip->usI2CAddr, &ChipID);
	if (AVL_EC_OK == r)
	{
		dprintk("The Chip ID is 0x%X\n", ChipID);
	}
	else
	{
		eprintk("Get Chip ID failed !\n");
		goto error;
	}

	r |= AVL62X1_Initialize(pAVL_Chip);
	if (AVL_EC_OK != r)
	{
		eprintk("AVL_AVL62X1_Initialize failed !\n");
		goto error;
	}

	dprintk("AVL_AVL62X1_Initialize success !\n");

	r |= IBase_GetVersion_AVL62X1(&ver_info, pAVL_Chip);
	if (AVL_EC_OK != r)
	{
		eprintk("IBase_GetVersion_AVL62X1 failed\n");
		goto error;
	}
	dprintk("FW version %d.%d.%d\n",ver_info.m_Patch.m_Major,ver_info.m_Patch.m_Minor,ver_info.m_Patch.m_Build);
	dprintk("API version %d.%d.%d\n",ver_info.m_API.m_Major,ver_info.m_API.m_Minor,ver_info.m_API.m_Build);

	Diseqc_para.uiToneFrequencyKHz = 22;
	Diseqc_para.eTXGap = AVL62X1_DTXG_15ms;
	Diseqc_para.eTxWaveForm = AVL62X1_DWM_Normal;
	Diseqc_para.eRxTimeout = AVL62X1_DRT_150ms;
	Diseqc_para.eRxWaveForm = AVL62X1_DWM_Normal;

	r |= IDiseqc_Initialize_AVL62X1(&Diseqc_para, pAVL_Chip);
	if (AVL_EC_OK != r)
	{
		eprintk("Diseqc Init failed !\n");
	}

	if (pTuner->fpInitializeFunc != 0)
	{
		r |= AVL62X1_OpenTunerI2C(pAVL_Chip);
		r |= pTuner->fpInitializeFunc(pTuner);
		r |= AVL62X1_CloseTunerI2C(pAVL_Chip);

		if (AVL_EC_OK != r)
		{
			eprintk("Tuner Init failed !\n");
			goto error;
		}
	}

	return (r);
	
error:
	//free_irq(priv->demod.lnb_int_irq, priv);
	//if (priv->demod.lnb_wq) destroy_workqueue(priv->demod.lnb_wq);
	return r;
}

static int avl62x1_fe_sleep(struct dvb_frontend* fe)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	if (priv->demod.blindscan_state != BLINDSCAN_STATE_STOP)
	{
		AVL62X1_BlindScan_Cancel(&priv->demod.AVLChip);
	}

	AVL62x1_PowerCtrl(&priv->demod, 0);

	//dprintk("%s\n", __FUNCTION__);

	return 0;
}
static AVL_ErrorCode avl62x1_fe_lock_tuner(struct AVL_Tuner *pTuner)
{
	AVL_ErrorCode r = AVL_EC_OK;

	if (pTuner->fpLockFunc != 0)
	{
		r |= pTuner->fpLockFunc(pTuner);
	}

	if (pTuner->fpGetLockStatusFunc != 0)
	{
		AVL_uint16 i = 0;
		do 
		{
			r |= pTuner->fpGetLockStatusFunc(pTuner);
			if (pTuner->ucTunerLocked == 1)
			{
				break;
			}
			AVL_IBSP_Delay(20);
		} while (i ++ <= 50);
		dprintk("Tuner lock i=%d ucTunerLocked=%d ret %d \n", i, pTuner->ucTunerLocked, r );
	}
	else
	{
		//no register to indicate tuner lock status
		pTuner->ucTunerLocked = 1;
	}
	
	//dprintk("%s\n", __FUNCTION__);
	return (r);
}

AVL_ErrorCode avl62x1_fe_lock_channel(struct dvb_frontend* fe)
{
	AVL_ErrorCode r = AVL_EC_OK;
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct AVL62X1_Chip * pAVL_Chip;
	struct AVL_Tuner * pTuner;
	struct AVL62X1_CarrierInfo CarrierInfo;
	//struct AVL62X1_StreamInfo StreamInfo;
	enum AVL62X1_LockStatus lock_status;
	AVL_int16 uiCounter = 0;
	AVL_uint16 uiTimeth = 0;
	AVL_uint16 snr = 0;

	pAVL_Chip = &priv->demod.AVLChip;
	pTuner = &priv->demod.Tuner;

	//dprintk("c->frequency=%d, c->symbol_rate=%d.\n", c->frequency, c->symbol_rate);
	pTuner->uiRFFrequencyHz = c->frequency * 1000;
	//pTuner->uiLPFHz = c->symbol_rate * 1000;
	pTuner->uiLPFHz = c->symbol_rate;
	pTuner->ucBlindScanMode = 0;

	memset(&CarrierInfo, 0, sizeof(struct AVL62X1_CarrierInfo));
	CarrierInfo.m_rf_freq_kHz = c->frequency;
	CarrierInfo.m_carrier_freq_offset_Hz = 0;
	//CarrierInfo.m_symbol_rate_Hz = c->symbol_rate * 1000;
	CarrierInfo.m_symbol_rate_Hz = c->symbol_rate;
	CarrierInfo.m_PL_scram_key = AVL62X1_PL_SCRAM_AUTO;

	r |= AVL62X1_Optimize_Carrier(pTuner,&CarrierInfo);

	r |= AVL62X1_OpenTunerI2C(pAVL_Chip);
	
	r |= avl62x1_fe_lock_tuner(pTuner);
	
	r |= AVL62X1_CloseTunerI2C(pAVL_Chip);
	
	if ((AVL_EC_OK != r) || (pTuner->ucTunerLocked != 1))
	{
		eprintk("Tuner lock failed ret(%d) !\n",r );
		return AVL_EC_GENERAL_FAIL;
	}

	//CCM or Unknown signal
	r |= AVL62X1_LockTP(&CarrierInfo, 0, AVL_FALSE, pAVL_Chip);

	//Channel lock time increase while symbol rate decrease.Give the max waiting time for different symbolrates.
	if(c->symbol_rate <= 5000 * 1000)
	{
		uiTimeth = 3000*2;       //The max waiting time is 3000ms
	}
	else if(c->symbol_rate <= 10000 * 1000)
	{
		uiTimeth = 1000*2;        //The max waiting time is 1000ms
	}
	else
	{
		uiTimeth = 500*2;        //The max waiting time is 500ms
	} 
	uiCounter = uiTimeth/10;

	do
	{
		AVL_IBSP_Delay(10);    //Wait 10ms for demod to lock the channel.
		//This function should be called to check the lock status of the demod.
		r = AVL62X1_GetLockStatus(&lock_status, pAVL_Chip);
		//dprintk("Tuner lock r=%d lock_status=%d\n", r, lock_status);
		if ((AVL_EC_OK == r)&&(AVL62X1_STATUS_LOCK == lock_status))
		{
			break;
		}
	}while(--uiCounter);
	
	AVL62X1_GetSNR(&snr, &priv->demod.AVLChip);
	//dprintk("%s(snr = %d)\n", __FUNCTION__, snr);
	
	if(uiCounter <= 0)
	{
		eprintk("Lock channel timed out\n");
		return AVL_EC_GENERAL_FAIL;
	}
	else
	{
		dprintk("Channel locked.\n");
	}

	return (r);
}

static int avl62x1_fe_set_tune(struct dvb_frontend* fe, bool re_tune, unsigned int mode_flags, unsigned int *delay, fe_status_t *status)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	AVL_ErrorCode r;

	//dprintk("%s re_tune=%d\n", __FUNCTION__, re_tune);
	if (re_tune)
	{
		r = avl62x1_fe_lock_channel(fe);
		if (AVL_EC_OK == r)
		{
			if (c->symbol_rate < 5000000)
				priv->demod.wait = 500; // 5000 ms
			else if (c->symbol_rate < 10000000)
				priv->demod.wait = 60; // 600 ms
			else
				priv->demod.wait = 25; // 250ms

			dprintk("%s(freq = %d KHz, symbol_rate = %d Hz)\n", __FUNCTION__, c->frequency, c->symbol_rate);
		}
		else
		{
			priv->demod.wait = 1;

			eprintk("%s(Lock Err = 0x%x, freq = %d KHz, symbol_rate = %d Hz)\n", __FUNCTION__, r, c->frequency, c->symbol_rate);
		}
		*status = 0;
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

static AVL_ErrorCode avl62x1_fe_blindscan(struct dvb_frontend* fe, bool re_tune, unsigned int mode_flags, unsigned int *delay, fe_status_t *status)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct AVL62X1_CarrierInfo CarrierInfo_Temp[16];
	struct AVL62X1_CarrierInfo *pCarrierInfo;
	AVL_ErrorCode r;

	dprintk("%s re_tune=%d, blindscan_state=%d\n", __FUNCTION__, re_tune, priv->demod.blindscan_state);
	*status = 0;
	if (re_tune)
	{
		priv->demod.index = 0;
		priv->demod.cur_freq_100kHz = BS_Start_Freq_KHz/100;
		priv->demod.blindscan_state = BLINDSCAN_STATE_START;
		dprintk("%s(blind scan - reset)\n", __FUNCTION__);
	}
	else
	{
		switch (priv->demod.blindscan_state)
		{
			case BLINDSCAN_STATE_START:
			{
				priv->demod.Tuner.uiRFFrequencyHz = priv->demod.cur_freq_100kHz * 100000;
				priv->demod.Tuner.ucBlindScanMode = 1;
				r = AVL62X1_Optimize_Carrier(&priv->demod.Tuner, 0);

				r |= AVL62X1_OpenTunerI2C(&priv->demod.AVLChip);
				r |= avl62x1_fe_lock_tuner(&priv->demod.Tuner);
				r |= AVL62X1_CloseTunerI2C(&priv->demod.AVLChip);
				AVL_IBSP_Delay(50);
				if (AVL_EC_OK != r)
				{
					eprintk("Tuner Lock failed at %dMHz\n", (AVL_uint32)(priv->demod.Tuner.uiRFFrequencyHz/1000000));
					break;
				}

				////////////////////////////////////BlindScan Start//////////////////////////////////////
				priv->demod.BSPara.m_TunerCenterFreq_100kHz = priv->demod.cur_freq_100kHz;
				priv->demod.BSPara.m_TunerLPF_100kHz = priv->demod.Tuner.uiLPFHz/100000;
				priv->demod.BSPara.m_MinSymRate_kHz = BS_Min_SR_Ksps;
				r |= AVL62X1_BlindScan_Start(&priv->demod.BSPara, &priv->demod.AVLChip);
				if (AVL_EC_OK == r)
				{
					priv->demod.blindscan_state = BLINDSCAN_STATE_CHECK_STATE;
					dprintk("%s(blind scan - start)\n", __FUNCTION__);
				}
				else
				{
					priv->demod.blindscan_state = BLINDSCAN_STATE_CANCEL;
					eprintk("%s(blind scan - start error![%d])\n", __FUNCTION__, r);
				}
				break;
			}
			case BLINDSCAN_STATE_CHECK_STATE:
			{
				r = AVL62X1_BlindScan_GetStatus(&priv->demod.BSInfo, &priv->demod.AVLChip);
				switch (r)
				{
					case AVL_EC_OK:
						if(priv->demod.BSInfo.m_BSFinished != 0)
							//priv->demod.blindscan_state = BLINDSCAN_STATE_GET_PROGRESS;
							priv->demod.blindscan_state = BLINDSCAN_STATE_GET_INFO;
						dprintk("%s(blind scan - get state OK)\n", __FUNCTION__);
						break;
					case AVL_EC_RUNNING:
						dprintk("%s(blind scan - get state Wait)\n", __FUNCTION__);
						break;
					default:
						priv->demod.blindscan_state = BLINDSCAN_STATE_CANCEL;
						eprintk("%s(blind scan - get state error(0x%x)\n", __FUNCTION__, r);
				}
				break;
			}
			case BLINDSCAN_STATE_GET_PROGRESS:
			{
				r = AVL62X1_BlindScan_GetCarrierList(&priv->demod.BSPara, &priv->demod.BSInfo, CarrierInfo_Temp, &priv->demod.AVLChip);
				if (AVL_EC_OK != r)
				{
					eprintk("ERROR: Get BlindScan Result failed\n");
					break;
				}

				priv->demod.cur_freq_100kHz += (AVL_uint32)(priv->demod.BSInfo.m_NextFreqStep_Hz/(AVL_uint32)1e5);
				if (priv->demod.cur_freq_100kHz > BS_Stop_Freq_KHz/100)
				{
					priv->demod.cur_freq_100kHz = BS_Stop_Freq_KHz/100;
				}
				c->frequency = priv->demod.cur_freq_100kHz * 100;
				c->symbol_rate = priv->demod.BSPara.m_MinSymRate_kHz;
				*status = FE_HAS_LOCK;
				if (priv->demod.index < priv->demod.BSInfo.m_NumCarriers)
				{
					priv->demod.blindscan_state = BLINDSCAN_STATE_GET_INFO;
				}
				else if (c->frequency == 100)
				{
					priv->demod.blindscan_state = BLINDSCAN_STATE_STOP;
				}
				else
				{
					priv->demod.blindscan_state = BLINDSCAN_STATE_START;
				}
				dprintk("%s(blind scan - progress %d %d\n", __FUNCTION__, c->frequency, c->symbol_rate);
				break;
			}
			case BLINDSCAN_STATE_GET_INFO:
			{
				r = AVL62X1_BlindScan_GetCarrierList(&priv->demod.BSPara, &priv->demod.BSInfo, CarrierInfo_Temp, &priv->demod.AVLChip);
				if (AVL_EC_OK != r)
				{
					eprintk("ERROR: Get BlindScan Result failed\n");
					break;
				}
				
				priv->demod.cur_freq_100kHz += (AVL_uint32)(priv->demod.BSInfo.m_NextFreqStep_Hz/(AVL_uint32)1e5);
				if (priv->demod.cur_freq_100kHz > BS_Stop_Freq_KHz/100)
				{
					priv->demod.cur_freq_100kHz = BS_Stop_Freq_KHz/100;
				}
				
				if (priv->demod.index < priv->demod.BSInfo.m_NumCarriers)
				{
					pCarrierInfo = &CarrierInfo_Temp[priv->demod.index++];
					c->frequency = pCarrierInfo->m_rf_freq_kHz;
					c->symbol_rate = pCarrierInfo->m_symbol_rate_Hz;
					dprintk("%s(blind scan - get info Index=%d, Freq=%dMHz, Sym=%dKHz)\n",
							__FUNCTION__,
							//AVL_DVBSx_IBlindscanAPI_GetProgress(&priv->demod.BSsetting),
							priv->demod.index,
							pCarrierInfo->m_rf_freq_kHz/1000,
							pCarrierInfo->m_symbol_rate_Hz/1000);
					*status = FE_HAS_LOCK | FE_HAS_SIGNAL;
				}
				if (priv->demod.index == priv->demod.BSInfo.m_NumCarriers)
				{
					priv->demod.index = 0;
					if (priv->demod.cur_freq_100kHz >= BS_Stop_Freq_KHz/100)
					{
						priv->demod.blindscan_state = BLINDSCAN_STATE_STOP;
					}
					else
					{
						priv->demod.blindscan_state = BLINDSCAN_STATE_START;
					}
				}
				else
				{
					//priv->demod.blindscan_state = BLINDSCAN_STATE_GET_PROGRESS;
					priv->demod.blindscan_state = BLINDSCAN_STATE_GET_INFO;
				}
				break;
			}
			case BLINDSCAN_STATE_CANCEL:
				r = AVL62X1_BlindScan_Cancel(&priv->demod.AVLChip);
				*status = FE_TIMEDOUT;
				if (AVL_EC_OK == r)
				{
					dprintk("%s(blind scan - cancel)\n", __FUNCTION__);
				}
				else
				{
					eprintk("%s(blind scan - cancel error![%d])\n", __FUNCTION__, r);
				}
				priv->demod.blindscan_state = BLINDSCAN_STATE_STOP;
				break;
		}
	}
	if (priv->demod.blindscan_state == BLINDSCAN_STATE_STOP)
	{
		*delay = 3 * HZ; // 3s
	}
	else
	{
		*delay = 1;
	}
	return 0;
}

static int avl62x1_fe_tune(struct dvb_frontend* fe, bool re_tune, unsigned int mode_flags, unsigned int *delay, fe_status_t *status)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	if (re_tune || priv->tune_mode == TUNE_MODE_NORMAL)
	{
		if (priv->demod.blindscan_state != BLINDSCAN_STATE_STOP)
		{
			//AVL_DVBSx_IBlindScanAPI_Exit(&priv->demod.AVLChip, &priv->demod.BSsetting);
			AVL62X1_BlindScan_Cancel(&priv->demod.AVLChip);
			priv->demod.blindscan_state = BLINDSCAN_STATE_STOP;
		}
	}

	if (priv->tune_mode == TUNE_MODE_NORMAL)
		avl62x1_fe_set_tune(fe, re_tune, mode_flags, delay, status);
	else
		avl62x1_fe_blindscan(fe, re_tune, mode_flags, delay, status);

	//dprintk("%s\n", __FUNCTION__);

	return 0;
}

static enum dvbfe_algo avl62x1_fe_get_frontend_algo(struct dvb_frontend *fe)
{
	return DVBFE_ALGO_HW;
}

static int avl62x1_fe_get_frontend(struct dvb_frontend *fe, struct dtv_frontend_properties *props)
{
	//avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	//dprintk("%s\n", __FUNCTION__);

	return 0;
}

static int avl62x1_fe_read_status(struct dvb_frontend* fe, fe_status_t* status)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	enum AVL62X1_LockStatus stat = AVL62X1_STATUS_UNLOCK;

	AVL62X1_GetLockStatus(&stat, &priv->demod.AVLChip) ;
	if(stat == AVL62X1_STATUS_LOCK)
	{
		*status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	}
	else
	{
		*status = 0;
	}

	//dprintk("%s(status = %d)\n", __FUNCTION__, *status);

	return  0;
}

static int avl62x1_fe_read_ber(struct dvb_frontend* fe, unsigned int* ber)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	IRx_GetBER_AVL62X1(ber, &priv->demod.AVLChip);
	//dprintk("%s(ber = %d)\n", __FUNCTION__, *ber);

	return 0;
}

/*
static int avl62x1_fe_read_per(struct dvb_frontend* fe, unsigned int* per)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	AVL62X1_GetPER(per, &priv->demod.AVLChip);
	//dprintk("%s(per = %d)\n", __FUNCTION__, *per);

	return 0;
}
*/

static AVL_ErrorCode avl62x1_fe_read_signal_strength(struct dvb_frontend* fe, unsigned short* strength)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	AVL_ErrorCode ret;

	ret = AVL62X1_GetSignalStrength(strength, &priv->demod.AVLChip);

	//dprintk("%s(strength = %d)\n", __FUNCTION__, *strength);

	return ret;
}

static AVL_ErrorCode avl62x1_fe_read_snr(struct dvb_frontend* fe, unsigned short* snr)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	AVL_ErrorCode ret;

	ret = AVL62X1_GetSNR(snr, &priv->demod.AVLChip);
	if(ret == AVL_EC_OK)
		*snr = (*snr) / 100;

	//dprintk("%s(snr = %d)\n", __FUNCTION__, *snr);

	return ret;
}

static int avl62x1_fe_read_ucblocks(struct dvb_frontend* fe, unsigned int* ucblocks)
{
	//avl62x1_priv_t *priv = (v *)fe->demodulator_priv;

	avl62x1_fe_read_ber(fe, ucblocks);

	//dprintk("%s(ucblocks = %d)\n", __FUNCTION__, *ucblocks);

	return 0;
}

static int avl62x1_fe_diseqc_reset_overload(struct dvb_frontend* fe)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	AVL_uchar nValue[2];
	AVL_uint16 len;
	AVL_uchar pStatus = 0;


	AVL62x1_lnb_read_status(priv, &pStatus); 
	if (pStatus & A8304_LNB_STATUS)
	{
		printk("A8304 is disabled,\n");
	}

	if (pStatus & A8304_LNB_GET_CPOK_STATUS)
	{
		printk("A8304 internal charge pump is  operating correctly,\n");
	}

	if (pStatus & A8304_LNB_GET_OC_STATUS)
	{
		printk("LNB output current exceeds the overcurrent threshold,\n");
	}

	if (pStatus & A8304_LNB_GET_PNG_STATUS)
	{
		printk("The A8304 is enabled and the LNB output voltage is either too low or too high,\n");
	}
	else
	{
		printk("The LNB voltage is within the acceptable range\n");
	}

	if (pStatus & A8304_LNB_GET_TDS_STATUS)
	{
		printk("A8304 has detected an overtemperature condition.\n");
	}

	if (pStatus & A8304_LNB_GET_UVLO_STATUS)
	{
		printk("The voltage at the VIN pin or the voltage at the VREG pin is too low\n");
	}

	nValue[0] = A8304_LNB_STATS_REG_ADD;
	nValue[1] = priv->demod.lnb_control;
	len = 2;
	AVL62x1_I2C_Write(priv, LNB_I2C_SLAVE_ADDR, nValue, &len);

	//dprintk("%s\n", __FUNCTION__);

	return  0;
}

static AVL_ErrorCode avl62x1_fe_diseqc_send_master_cmd(struct dvb_frontend* fe, struct dvb_diseqc_master_cmd* cmd)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar ucData[8];
	struct AVL62X1_Diseqc_TxStatus TxStatus;
	int i;

	for(i=0;i<cmd->msg_len;i++)
	{
		ucData[i]=cmd->msg[i];
		//dprintk("%x ",cmd->msg[i]);
	}
	
	r = AVL62X1_IDiseqc_SendModulationData(ucData, cmd->msg_len, &priv->demod.AVLChip);
	if(r != AVL_EC_OK)
	{
		eprintk("%s(Send data Err:0x%x)\n", __FUNCTION__, r);
		return (r);
	}

	i = 100;
	do
	{
		i--;
		AVL_IBSP_Delay(1);
		r = AVL62X1_IDiseqc_GetTxStatus(&TxStatus, &priv->demod.AVLChip);
	}
	while((TxStatus.m_TxDone != 1) && i);
	if(r != AVL_EC_OK)
	{
		eprintk("%s(Output data Err:0x%x)\n", __FUNCTION__, r);
	}

	//dprintk("%s\n", __FUNCTION__);

	return (r);
}

static int avl62x1_fe_diseqc_recv_slave_reply(struct dvb_frontend* fe, struct dvb_diseqc_slave_reply* reply)
{
	//avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	//dprintk("%s\n", __FUNCTION__);

	return  0;
}

static AVL_ErrorCode avl62x1_fe_diseqc_send_burst(struct dvb_frontend* fe, fe_sec_mini_cmd_t minicmd)
{
	#define TONE_COUNT				8

	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	AVL_ErrorCode r = AVL_EC_OK;
 	struct AVL62X1_Diseqc_TxStatus sTxStatus;
	AVL_uchar ucTone = 0;
	int i;

	if(minicmd == SEC_MINI_A)
		ucTone = 1;
	else if(minicmd == SEC_MINI_B)
		ucTone = 0;

  	r = AVL62X1_IDiseqc_SendTone(ucTone, TONE_COUNT, &priv->demod.AVLChip);
	if(r != AVL_EC_OK)
	{
		eprintk("%s(Send tone %d Err:0x%x)\n", __FUNCTION__, ucTone, r);
		return (r);
	}

	i = 100;
    do
    {
    	i--;
		AVL_IBSP_Delay(1);
	    r = AVL62X1_IDiseqc_GetTxStatus(&sTxStatus, &priv->demod.AVLChip);   //Get current status of the Diseqc transmitter data FIFO.
    }
    while((sTxStatus.m_TxDone != 1) && i);			//Wait until operation finished.
    if(r != AVL_EC_OK)
    {
		eprintk("%s(Output tone %d Err:0x%x)\n", __FUNCTION__, ucTone, r);
    }

	//dprintk("%s(minicmd = %d, ret = %d)\n", __FUNCTION__, minicmd, r);

	return (r);
}

static AVL_ErrorCode avl62x1_fe_set_tone(struct dvb_frontend* fe, fe_sec_tone_mode_t tone)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	AVL_ErrorCode r = AVL_EC_OK;

	if(tone == SEC_TONE_ON)
	{
		r = AVL62X1_IDiseqc_Start22K(&priv->demod.AVLChip);
		if(r != AVL_EC_OK)
		{
			eprintk("%s(Diseqc Start Err:0x%x)\n", __FUNCTION__, r);
		}	
	}
	else
	{
		r = AVL62X1_IDiseqc_Stop22K(&priv->demod.AVLChip);
		if(r != AVL_EC_OK)
		{
			eprintk("%s(Diseqc Stop Err:0x%x)\n", __FUNCTION__, r);
		}	
	}

	//dprintk("%s(tone = %d, ret = %d)\n", __FUNCTION__, tone, r);

	return r;
}

static AVL_ErrorCode avl62x1_fe_set_voltage(struct dvb_frontend* fe, fe_sec_voltage_t voltage)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;
	AVL_ErrorCode r = AVL_EC_OK;	
	AVL_uchar nValue[2];
	AVL_uint16 len;
	AVL_uchar pStatus = 0;

	nValue[0] = A8304_LNB_STATS_REG_ADD;
	len = 2;
	
	AVL62x1_lnb_read_status(priv, &pStatus);  //after init ,finded the status reg dis bit=1,should read status reg again! have some difference with datasheet
	if (priv->demod.lnb_source22k == USM_FE_22K_FROM_DEMOD)
	{
		switch (voltage)
	{
			case SEC_VOLTAGE_13:
				nValue[1] = A8304_DATA_13V_OUT_EXTERNAL;

				if (priv->demod.lnb_high_voltage)
				{
					nValue[1] = A8304_DATA_14V_OUT_EXTERNAL;
				}

				break;

			/*
			case SEC_VOLTAGE_14:
				nValue[1] = A8304_DATA_14V_OUT_EXTERNAL;
				break;
			*/

			case SEC_VOLTAGE_18:
				nValue[1] = A8304_DATA_18V_OUT_EXTERNAL;

				if (priv->demod.lnb_high_voltage)
				{
					nValue[1] = A8304_DATA_19V_OUT_EXTERNAL;
				}

			break;

			/*
			case SEC_VOLTAGE_19:
				nValue[1] = A8304_DATA_19V_OUT_EXTERNAL;
				break;
			*/

			default:
				nValue[1] = A8304_DATA_LNBPOWER_OFF;
				break;
		}
	}
	else
	{
		switch (voltage)
		{
			case SEC_VOLTAGE_13:
				nValue[1] = A8304_DATA_13V_OUT_INTERNAL;

				if (priv->demod.lnb_high_voltage)
				{
					nValue[1] = A8304_DATA_14V_OUT_INTERNAL;
				}
				break;
				
			/*	
			case SEC_VOLTAGE_14:
				nValue[1] = A8304_DATA_14V_OUT_INTERNAL;
				break;
			*/

			case SEC_VOLTAGE_18:
				nValue[1] = A8304_DATA_18V_OUT_INTERNAL;

				if (priv->demod.lnb_high_voltage)
				{
					nValue[1] = A8304_DATA_19V_OUT_INTERNAL;
				}
				break;
			/*
			case SEC_VOLTAGE_19:
				nValue[1] = A8304_DATA_19V_OUT_INTERNAL;
				break;
			*/

			default:
				nValue[1] = A8304_DATA_LNBPOWER_OFF;
				break;
		}
	}
		
	priv->demod.lnb_control = nValue[1];
		if(AVL62x1_I2C_Write(priv, LNB_I2C_SLAVE_ADDR, nValue, &len) != 0)
			r = AVL_EC_GENERAL_FAIL;

	AVL62x1_lnb_read_status(priv, &pStatus);  //after init ,finded the status reg dis bit=1,should read status reg again! have some difference with datasheet

	dprintk("%s(voltage = %d, ret = %d)\n", __FUNCTION__, voltage, r);

	return r;
}

static int avl62x1_fe_enable_high_lnb_voltage(struct dvb_frontend* fe, long arg)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	priv->demod.lnb_high_voltage = (int)arg;
	dprintk("%s\n", __FUNCTION__);

	return  0;
}

static int avl62x1_fe_i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	//avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	//dprintk("%s\n", __FUNCTION__);

	return 0;
}

static int avl62x1_fe_set_property(struct dvb_frontend* fe, struct dtv_property* tvp)
{
	avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	switch(tvp->cmd)
	{
	case DTV_CLEAR:
		priv->tune_mode = TUNE_MODE_NORMAL;
		break;
	case DTV_TUNE:
		priv->tune_mode = tvp->u.data;
		break;
	}

	dprintk("%s(CMD = %d)\n", __FUNCTION__, tvp->cmd);

	return 0;
}

static int avl62x1_fe_get_property(struct dvb_frontend* fe, struct dtv_property* tvp)
{
	//avl62x1_priv_t *priv = (avl62x1_priv_t *)fe->demodulator_priv;

	dprintk("%s(CMD = %d)\n", __FUNCTION__, tvp->cmd);

	return 0;
}

#ifdef AUTO_TEST
static int avl62x1_fe_test_event(void *data)
{
	struct dvb_frontend *fe = (struct dvb_frontend*)data;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	unsigned short sg;
	
	dprintk("%s\n", __FUNCTION__);
	msleep(5000);
	dprintk("%s...\n", __FUNCTION__);
	if(avl62x1_fe_init(fe) != AVL_EC_OK){
		eprintk("avl62x1_fe_init failed !\n");
		return -1;
	}
	
	while(1){
		if(g_test == 0){
			msleep(1000);
			continue;
		}
		g_test = 0;
		//c->frequency = 2000*1000;
		//c->symbol_rate = 27500*1000;
		c->frequency = g_freq*1000;
		c->symbol_rate = g_symbol*1000;
	//c->modulation = QPSK;
	if(avl62x1_fe_lock_channel(fe) != AVL_EC_OK){
		eprintk("avl62x1_fe_lock_channel failed !\n");
			//return -1;
	}
	
	if(avl62x1_fe_read_signal_strength(fe, &sg) != AVL_EC_OK){
		eprintk("avl62x1_fe_read_signal_strength failed !\n");
			//return -1;
		}
	}

	return 0;
}
#endif

/*****************************************************************************
 * AVL6211 OPS
 ******************************************************************************/
static struct dvb_frontend_ops avl62x1_fe_ops = {

	.info = {
		.name = "TCC DVB-S2 (AVL62x1)",
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

	.release = avl62x1_fe_release,

	.init = avl62x1_fe_init,
	.sleep = avl62x1_fe_sleep,

	.tune = avl62x1_fe_tune,
	.get_frontend_algo = avl62x1_fe_get_frontend_algo,

	.get_frontend = avl62x1_fe_get_frontend,

	.read_status = avl62x1_fe_read_status,
	.read_ber = avl62x1_fe_read_ber,
	//.read_per = avl62x1_fe_read_per,
	.read_signal_strength = avl62x1_fe_read_signal_strength,
	.read_snr = avl62x1_fe_read_snr,
	.read_ucblocks = avl62x1_fe_read_ucblocks,

	.diseqc_reset_overload = avl62x1_fe_diseqc_reset_overload,
	.diseqc_send_master_cmd = avl62x1_fe_diseqc_send_master_cmd,
	.diseqc_recv_slave_reply = avl62x1_fe_diseqc_recv_slave_reply,
	.diseqc_send_burst = avl62x1_fe_diseqc_send_burst,
	.set_tone = avl62x1_fe_set_tone,
	.set_voltage = avl62x1_fe_set_voltage,
	.enable_high_lnb_voltage = avl62x1_fe_enable_high_lnb_voltage,
	.i2c_gate_ctrl = avl62x1_fe_i2c_gate_ctrl,

	.set_property = avl62x1_fe_set_property,
	.get_property = avl62x1_fe_get_property,
};


extern void AVL_IBSP_SetI2cAdapter( struct i2c_adapter *i2c_handle );
/*****************************************************************************
 * AVL6211 Init
 ******************************************************************************/
static int avl62x1_init(struct device_node *node, avl62x1_priv_t *priv)
{
	struct device_node *adapter_np;

	adapter_np = of_parse_phandle(node, "i2c-parent", 0);
	if (adapter_np)
	{
		priv->demod.i2c = of_find_i2c_adapter_by_node(adapter_np);//i2c_get_adapter(TCC_I2C_CH);
		AVL_IBSP_SetI2cAdapter(priv->demod.i2c);
	}
	
	adapter_np = of_parse_phandle(node, "i2c-lnb-ctrl", 0);
	if (adapter_np)
	{
		priv->demod.lnb_i2c = of_find_i2c_adapter_by_node(adapter_np);//i2c_get_adapter(TCC_I2C_CH);
	}

	priv->demod.gpio_fe_power  = of_get_named_gpio(node, "gpios",   0); // SLEEP
	priv->demod.gpio_fe_lock   = of_get_named_gpio(node, "gpios",   1); // LOCK
	priv->demod.gpio_fe_reset  = of_get_named_gpio(node, "gpios",   2); // RESET
	priv->demod.gpio_lnb_power = of_get_named_gpio(node, "gpios",   3); // MAIN_VEN(lnb_power)
	
	tcc_gpio_init(priv->demod.gpio_fe_power, GPIO_DIRECTION_OUTPUT);
	tcc_gpio_init(priv->demod.gpio_fe_reset, GPIO_DIRECTION_OUTPUT);
	tcc_gpio_init(priv->demod.gpio_lnb_power, GPIO_DIRECTION_OUTPUT);
	
	tcc_gpio_init(priv->demod.gpio_fe_lock, GPIO_DIRECTION_INPUT);
	tcc_gpio_init(priv->demod.lnb_int_port, GPIO_DIRECTION_INPUT);

	of_property_read_u32(node, "config-idx", &priv->demod.config_idx);
	
    priv->demod.lnb_int_port = of_get_named_gpio(node, "int-gpios", 0);
	
	memcpy(&priv->demod.AVLChip, &g_AVL62X1_Chip1, sizeof(struct AVL62X1_Chip));
	memcpy(&priv->demod.Tuner, &gTuner1, sizeof(struct AVL_Tuner));
	
	return 0;
}


/*****************************************************************************
 * AVL6211 Probe
 ******************************************************************************/
static int avl62x1_probe(tcc_fe_priv_t *inst)
{
	avl62x1_priv_t *priv = kzalloc(sizeof(avl62x1_priv_t), GFP_KERNEL);
	int retval;

	if (priv == NULL)
	{
		eprintk("%s(kzalloc fail)\n", __FUNCTION__);
		return -1;
	}

	retval = avl62x1_init(inst->of_node, priv);
	if (retval != 0)
	{
		eprintk("%s(Init Error %d)\n", __FUNCTION__, retval);
		kfree(priv);
		return -1;
	}

	inst->fe.demodulator_priv = priv;

	#ifdef AUTO_TEST
	retval = sysfs_create_group(&inst->fe.dvb->device->kobj, &dvb_attr_group);
	if (retval < 0) {
		printk(KERN_ERR "Cannot register USB SQ sysfs attributes: %d\n",
		       retval);
	}
	//init_timer(&priv->demod.timer);
	//priv->demod.timer.function = avl62x1_fe_test_event;
	//priv->demod.timer.data = (unsigned long) &inst->fe;
	//priv->demod.timer.expires = jiffies + 10*1000;

	//add_timer(&priv->demod.timer);
	kthread_run(avl62x1_fe_test_event, &inst->fe, "av62x1_test");
	#endif

	//dprintk("%s\n", __FUNCTION__);

	return 0;
}


/*****************************************************************************
 * AVL6211 Module Init/Exit
 ******************************************************************************/
static struct tcc_dxb_fe_driver avl62x1_driver = {
	.probe = avl62x1_probe,
	.compatible = "availink,avl62x1",
	.fe_ops = &avl62x1_fe_ops,
};

static int __init avl62x1_module_init(void)
{
	dprintk("%s \n", __FUNCTION__);
	if (tcc_fe_register(&avl62x1_driver) != 0)
		return -EPERM;

	return 0;
}

static void __exit avl62x1_module_exit(void)
{
	dprintk("%s\n", __FUNCTION__);
	tcc_fe_unregister(&avl62x1_driver);
}

module_init(avl62x1_module_init);
module_exit(avl62x1_module_exit);

MODULE_DESCRIPTION("TCC DVBS2 (AVL62x1)");
MODULE_AUTHOR("CE");
MODULE_LICENSE("GPL");
