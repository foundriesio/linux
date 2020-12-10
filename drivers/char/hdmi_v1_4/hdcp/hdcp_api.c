/*
 * Copyright (C) 20010-2020 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "tcc_hdcp_log.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/delay.h>

#include <linux/io.h>
#include <linux/uaccess.h>

#include <dt-bindings/display/tcc897x-vioc.h>
#include <linux/slab.h>

#include <hdmi_1_4_video.h>
#include <hdmi_1_4_audio.h>
#include <hdmi_1_4_hdmi.h>
#include <hdmi_1_4_api.h>

#include <hdmi.h>
#include <regs-hdmi.h>

#include "hdcp_api.h"
#include "hdcp_ss.h"

#define HDCP_API_DEBUG_IRQ 0

/** I2C device address of HDCP Rx port*/
#define HDCP_RX_DEV_ADDR 0x74

/** Ri offset on HDCP Rx port */
#define HDCP_RI_OFFSET 0x08

/** Size of Ri */
#define HDCP_RI_SIZE 2

/**
 * @struct hdcp_struct
 * Structure for processing hdcp
 */
struct hdcp_struct {
	/** Spinlock for synchronizing event */
	spinlock_t lock;

	/** Wait queue */
	wait_queue_head_t waitq;

	/** Contains event that occurs */
	enum hdcp_event event;
};

struct hdcp_struct hdcp_struct;

static void hdcp_reset(void);
static void hdcp_enable(unsigned char enable);
static void hdcp_set_ksv_list(struct hdcp_ksv_list list);
static void hdcp_enable_encryption(unsigned char enable);
static void hdcp_set_result(unsigned char match);
static int hdcp_read_ri(void);
static void hdcp_enable_bluescreen(unsigned char enable);
static void hdcp_set_ksv_list(struct hdcp_ksv_list list);
static void hdcp_set_result(unsigned char enable);
static int hdcp_read_ri(void);

/**
 * Reset the HDCP H/W state machine.
 */
static void hdcp_reset(void)
{
	ILOG("reset !!\n");

	// disable HDCP
	hdcp_writeb(0x00, HDCP_ENC_EN);
	hdcp_writeb(0x00, HDCP_CTRL1);
	hdcp_writeb(0x00, HDCP_CTRL2);

	// set blue screen
	hdcp_enable_bluescreen(1);
	// enable HDCP
	hdcp_writeb(HDCP_ENABLE, HDCP_CTRL1);
	ILOG("hdcp_writeb(HDCP_ENABLE,HDCP_CTRL1);\n");
}

static void hdcp_disable(void)
{
	unsigned char reg;

	// disable HDCP INT
	reg = hdcp_readb(HDMI_SS_INTC_CON);
	hdcp_writeb(reg & ~(1 << HDMI_IRQ_HDCP), HDMI_SS_INTC_CON);

	// disable encryption
	hdcp_enable_encryption(0);

	// clear hdcp ctrl
	hdcp_writeb(0x00, HDCP_CTRL1);
	hdcp_writeb(0x00, HDCP_CTRL2);
}

/**
 * Enable/Disable HDCP H/W.
 * @param enable    [in] 1 to enable, 0 to disable
 */
static void hdcp_enable(unsigned char enable)
{
	unsigned char reg;

	if (enable) {
		ILOG("HDCP ENABLE Start\n");

		// enable all HDCP INT in HDMI_STATUS reg
		reg = hdcp_readb(HDMI_STATUS_EN);
		hdcp_writeb(
			(reg | 1 << HDCP_I2C_INT_NUM
			 | 1 << HDCP_WATCHDOG_INT_NUM
			 | 1 << HDCP_AN_WRITE_INT_NUM
			 | 1 << HDCP_UPDATE_RI_INT_NUM
			 | 1 << HDCP_AUD_FIFO_OVF_EN_NUM),
			HDMI_STATUS_EN);

		// enable HDCP INT
		reg = hdcp_readb(HDMI_SS_INTC_CON);
		hdcp_writeb(
			(reg | (1 << HDMI_IRQ_HDCP) | (1 << HDMI_IRQ_GLOBAL)),
			HDMI_SS_INTC_CON);

		// reset hdcp state machine
		hdcp_reset();
	} else {
		ILOG("HDCP DISABLE\n");

		hdcp_disable();
	}
}

/**
 * Start/Stop sending EESS/OESS packet.
 *
 * @param  enable	[in] 0 to stop sending, 1 to start sending.
 */
static void hdcp_enable_encryption(unsigned char enable)
{
	ILOG("%d\n", enable);

	if (enable) {
		hdcp_writeb(HDCP_EESS_START, HDCP_ENC_EN);
		// disable blue screen
		hdcp_enable_bluescreen(0);
	} else {
		hdcp_writeb(HDCP_EESS_STOP, HDCP_ENC_EN);
		// enable blue screen
		hdcp_enable_bluescreen(1);
	}
}

/**
 * Set the result whether Ri and Ri' are match or not.
 * @param match	[in] 1 if Ri and Ri' are match; Otherwise, 0.
 */
static void hdcp_set_result(unsigned char match)
{
	if (match) {
		// set
		hdcp_writeb(HDCP_RI_MATCH, HDCP_CHECK_RESULT);
		// clear
		hdcp_writeb(0x00, HDCP_CHECK_RESULT);
	} else {
		// set
		hdcp_writeb(HDCP_RI_NOT_MATCH, HDCP_CHECK_RESULT);
		// clear
		hdcp_writeb(0x00, HDCP_CHECK_RESULT);
	}
}

/**
 * Set one KSV in KSV list which read from Rx on 2nd authentication.
 * @param list    [in] One KSV in KSV list and flag.
 */
static void hdcp_set_ksv_list(struct hdcp_ksv_list list)
{
	hdcp_writeb(list.ksv[0], HDCP_KSV_LIST_0);
	hdcp_writeb(list.ksv[1], HDCP_KSV_LIST_1);
	hdcp_writeb(list.ksv[2], HDCP_KSV_LIST_2);
	hdcp_writeb(list.ksv[3], HDCP_KSV_LIST_3);
	hdcp_writeb(list.ksv[4], HDCP_KSV_LIST_4);

	if (list.end) {
		// if last one
		// finish setting KSV_LIST
		hdcp_writeb(
			HDCP_KSV_LIST_END | HDCP_KSV_WRITE_DONE,
			HDCP_KSV_LIST_CON);
	} else {
		// not finishing
		hdcp_writeb(HDCP_KSV_WRITE_DONE, HDCP_KSV_LIST_CON);
	}
}

/**
 * Read Ri' in  HDCP Rx port. The length of Ri' is 2 bytes. @n
 * Stores LSB first.
 * [0 : 0 : Ri'[1] : Ri'[0]]
 * @return Ri' value.
 */
static int hdcp_read_ri(void)
{
	int result;
	struct i2c_adapter *adap;
	struct i2c_msg msgs[2];
	unsigned char buffer[2];
	unsigned char offset = HDCP_RI_OFFSET;

	// adap = i2c_get_adapter(0); // i2c-0: J&K
	// adap = i2c_get_adapter(1); // i2c-1: telechips ALS
	// adap = i2c_get_adapter(2); // i2c-2: pioneer
	// adap = i2c_get_adapter(3); // i2c-3: telechips CLS
	// Modify the value as the number corresponding with support company
	adap = i2c_get_adapter(1); // i2c-1 : telechips ALS
	//adap = i2c_get_adapter(3); // i2c-3 : telechips CLS
	if (!adap) {
		ILOG("i2c_get_adapter(3) error \n");
		return 0;
	}

	// set offset
	msgs[0].addr = HDCP_RX_DEV_ADDR >> 1;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &offset;

	// read data
	msgs[1].addr = HDCP_RX_DEV_ADDR >> 1;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = buffer;

	// read from DDC line
	if (i2c_transfer(adap, msgs, 2) < 0)
		return 0;

	result = (buffer[0] << 8) | buffer[1];

	return result;
}

/**
 * Enable/disable Blue-Screen.
 *
 * @param  enable	[in] 0 to stop sending, 1 to start sending.
 */
static void hdcp_enable_bluescreen(unsigned char enable)
{
	unsigned char reg, aviYY;

	ILOG("enable = %d\n", enable);

	aviYY = hdcp_readb(HDMI_AVI_BYTE1);

	if (aviYY & AVI_CS_Y422 || aviYY & AVI_CS_Y444) {
		// YCbCr Color Space
		hdcp_writeb(0x80, HDMI_BLUE_SCREEN_R_1);
		hdcp_writeb(0x80, HDMI_BLUE_SCREEN_B_1);
	} else {
		hdcp_writeb(0x00, HDMI_BLUE_SCREEN_R_1);
		hdcp_writeb(0x00, HDMI_BLUE_SCREEN_B_1);
	}

	reg = hdcp_readb(HDMI_CON_0);

	if (enable) {
		reg |= HDMI_BLUE_SCR_ENABLE;
	} else {
		reg &= ~HDMI_BLUE_SCR_ENABLE;
	}

	hdcp_writeb(reg, HDMI_CON_0);
}

#define AES_KEY_SIZE 16
#define HDCP_KEY_SIZE 288
static void set_aeskey_to_reg(unsigned char *pkey)
{
	unsigned char data[AES_KEY_SIZE];
	unsigned int *plkey;
	unsigned int regl[5] = {
		0,
	};
	int ii;
	unsigned char tmpch;

	memcpy(data, pkey, AES_KEY_SIZE);
	for (ii = 0; ii < (AES_KEY_SIZE / 2); ii++) {
		tmpch = data[ii];
		data[ii] = data[AES_KEY_SIZE - ii - 1];
		data[AES_KEY_SIZE - ii - 1] = tmpch;
	}

	plkey = (unsigned int *)data;

	regl[AESKEY_DATA0] = plkey[0];
	regl[AESKEY_DATA1] = plkey[1] & 0x01;
	regl[AESKEY_HW_0] = plkey[1] >> 1 | (plkey[2] << 31 & 0x80000000);
	regl[AESKEY_HW_1] = plkey[2] >> 1 | (plkey[3] << 31 & 0x80000000);
	regl[AESKEY_HW_2] = plkey[3] >> 1 & 0x7FFFFFFF;

	hdcp_writel(regl[AESKEY_DATA0], DDICFG_HDMI_AESKEY_DATA0);
	hdcp_writel(regl[AESKEY_DATA1], DDICFG_HDMI_AESKEY_DATA1);
	hdcp_writel(regl[AESKEY_HW_0], DDICFG_HDMI_AESKEY_HW0);
	hdcp_writel(regl[AESKEY_HW_1], DDICFG_HDMI_AESKEY_HW1);
	hdcp_writel(regl[AESKEY_HW_2], DDICFG_HDMI_AESKEY_HW2);

	// HDMI AES KEY valid
	hdcp_writel(0x1, DDICFG_HDMI_AESKEY_VALID);
}

/*
 * Set HDCP Device Private Keys. @n
 * To activate HDCP H/W, user should set AES-encrypted HDCP Device Private Keys.@n
 * If user does not set this, HDCP H/W does not work.
 */
static void hdcp_load_data(unsigned char *AES_key, unsigned char *HDCP_key)
{
	int index;
	unsigned char reg;

	set_aeskey_to_reg(AES_key);
	mdelay(10);
	for (index = 0; index < HDCP_KEY_SIZE; index++) {
		hdcp_writeb(HDCP_key[index], HDMI_SS_AES_DATA);
	}
	hdcp_writeb(AES_START, HDMI_SS_AES_START);
	do {
		reg = hdcp_readb(HDMI_SS_AES_START);
		if ((reg & 0x01) == 0)
			break;
	} while (1);
	TRACE;
}

int hdcp_api_cmd_process(unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case HDCP_IOC_GET_HDCP_EVENT: {
		// unsigned long irqflags;
		int ret;
		// ILOG("ioctl(HDCP_IOC_GET_HDCP_EVENT) - Start\n");
		// wait event
		// wait_event_interruptible(hdcp_struct.waitq, hdcp_struct.event
		// != 0);

		// If copy_to_user() is executed during spin_lock, it becomes
		// may sleep. Therefore, spin_lock does not use. - 191105
		// spin_lock_irqsave(&hdcp_struct.lock, irqflags);

		// copy to user
		ret = copy_to_user(
			(void *)arg, (const void *)&hdcp_struct.event,
			sizeof(hdcp_struct.event));

		if (ret < 0) {
			// spin_unlock_irqrestore(&hdcp_struct.lock,
			// irqflags);
			return -EFAULT;
		}

		// clear event
		hdcp_struct.event = 0;

		// spin_unlock_irqrestore(&hdcp_struct.lock, irqflags);

		// ILOG("ioctl(HDCP_IOC_GET_HDCP_EVENT) - End\n");

		break;
	}
	// start HDCP
	case HDCP_IOC_START_HDCP: {
		ILOG("ioctl(HDCP_IOC_START_HDCP)\n");
		break;
	}
	// stop HDCP
	case HDCP_IOC_STOP_HDCP: {
		unsigned long irqflags;
		ILOG("ioctl(HDCP_IOC_STOP_HDCP)\n");

		// add event HDCP_EVENT_STOP
		spin_lock_irqsave(&hdcp_struct.lock, irqflags);
		hdcp_struct.event |= (1 << HDCP_EVENT_STOP);
		spin_unlock_irqrestore(&hdcp_struct.lock, irqflags);

		ILOG("Set HDCP_STOP Event in HDCP_IOC_STOP_HDCP!!!!!!\n");
		// wake up
		// wake_up_interruptible(&hdcp_struct.waitq);
		ILOG("HDCP_IOC_STOP_HDCP Done!!!!!!\n");
		break;
	}
	case HDCP_IOC_ENABLE_HDCP: {
		int ret;
		unsigned char enable;

		ILOG("ioctl(HDCP_IOC_ENABLE_HDCP) Start\n");

		// copy from user
		ret = copy_from_user(
			(void *)&enable, (const void *)arg, sizeof(enable));
		if (ret < 0)
			return -EFAULT;
		// enable HDCP
		hdcp_enable(enable);

		ILOG("ioctl(HDCP_IOC_ENABLE_HDCP) End\n");

		break;
	}
	case HDCP_IOC_SET_BKSV: {
		struct hdcp_ksv Bksv;
		int ret;

		ILOG("ioctl(HDCP_IOC_SET_BKSV)\n");

		// get arg
		ret = copy_from_user(
			(void *)&Bksv, (const void *)arg, sizeof(Bksv));
		if (ret < 0)
			return -EFAULT;

		// set bksv
		hdcp_writeb(Bksv.ksv[0], HDCP_BKSV_0);
		hdcp_writeb(Bksv.ksv[1], HDCP_BKSV_1);
		hdcp_writeb(Bksv.ksv[2], HDCP_BKSV_2);
		hdcp_writeb(Bksv.ksv[3], HDCP_BKSV_3);
		hdcp_writeb(Bksv.ksv[4], HDCP_BKSV_4);

		ILOG("Bksv[0:4] = 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x \n",
		     Bksv.ksv[0], Bksv.ksv[1], Bksv.ksv[2], Bksv.ksv[3],
		     Bksv.ksv[4]);
		break;
	}
	case HDCP_IOC_SET_BCAPS: {
		int ret;
		unsigned char Bcaps;

		ILOG("ioctl(HDCP_IOC_SET_BCAPS)\n");

		// copy from user
		ret = copy_from_user(
			(void *)&Bcaps, (const void *)arg, sizeof(Bcaps));
		if (ret < 0)
			return -EFAULT;

		ILOG("BCaps = 0x%02x\n", Bcaps);
		// set bcaps
		hdcp_writeb(Bcaps, HDCP_BCAPS);

		break;
	}

	case HDCP_IOC_GET_AKSV: {
		struct hdcp_ksv Aksv;
		int ret;

		ILOG("ioctl(HDCP_IOC_GET_AKSV)\n");

		// get Aksv
		Aksv.ksv[0] = hdcp_readb(HDCP_AKSV_0);
		Aksv.ksv[1] = hdcp_readb(HDCP_AKSV_1);
		Aksv.ksv[2] = hdcp_readb(HDCP_AKSV_2);
		Aksv.ksv[3] = hdcp_readb(HDCP_AKSV_3);
		Aksv.ksv[4] = hdcp_readb(HDCP_AKSV_4);

		ILOG("Aksv[0:4] = 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x \n",
		     Aksv.ksv[0], Aksv.ksv[1], Aksv.ksv[2], Aksv.ksv[3],
		     Aksv.ksv[4]);

		// copy to user
		ret = copy_to_user(
			(void *)arg, (const void *)&Aksv, sizeof(Aksv));
		if (ret < 0)
			return -EFAULT;

		break;
	}
	case HDCP_IOC_GET_AN: {
		struct hdcp_an An;
		int ret;

		ILOG("ioctl(HDCP_IOC_GET_AN)\n");

		// get an
		An.an[0] = hdcp_readb(HDCP_AN_0);
		An.an[1] = hdcp_readb(HDCP_AN_1);
		An.an[2] = hdcp_readb(HDCP_AN_2);
		An.an[3] = hdcp_readb(HDCP_AN_3);
		An.an[4] = hdcp_readb(HDCP_AN_4);
		An.an[5] = hdcp_readb(HDCP_AN_5);
		An.an[6] = hdcp_readb(HDCP_AN_6);
		An.an[7] = hdcp_readb(HDCP_AN_7);

		ILOG("An[0:7] = 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x \n",
		     An.an[0], An.an[1], An.an[2], An.an[3], An.an[4], An.an[5],
		     An.an[6], An.an[7]);

		// put to user
		ret = copy_to_user((void *)arg, (const void *)&An, sizeof(An));
		if (ret < 0)
			return -EFAULT;

		break;
	}
	case HDCP_IOC_GET_RI: // only use if state is in first auth
	{
		int ret;
		unsigned int result;
		unsigned char ri0, ri1;
		int match;

		struct infoRi {
			unsigned char ri[2];
			unsigned char rj[2];
			int res;
		};

		struct infoRi info;

		ILOG("ioctl(HDCP_IOC_GET_RI)\n");

		// get rj from RX
		result = hdcp_read_ri();

		// get ri from TX
		ri0 = hdcp_readb(HDCP_RI_0);
		ri1 = hdcp_readb(HDCP_RI_1);

		// comparison
		if (((result >> 8) & 0xFF) == ri0 && (result & 0xFF) == ri1) {
			// ILOG("Ri is Matched!!!\n");
			// ILOG("Ri = 0x%02x%02x\n",ri0,ri1);
			// ILOG("Rj = 0x%02x%02x\n",(result>>8) &
			// 0xFF,(result) & 0xFF);
			match = 1;
		} else {
			ILOG("Ri not Matched!!!\n");
			ILOG("Ri = 0x%02x%02x\n", ri0, ri1);
			ILOG("Rj = 0x%02x%02x\n", (result >> 8) & 0xFF,
			     (result)&0xFF);
			match = 0;
		}

		info.ri[0] = ri0;
		info.ri[1] = ri1;
		info.rj[0] = result >> 8;
		info.rj[1] = result & 0xFF;
		info.res = match;

		// copy to user
		// if ( (ret = copy_to_user((void*)arg,(const
		// void*)&match,sizeof(match))) < 0)
		ret = copy_to_user(
			(void *)arg, (const void *)&info, sizeof(info));
		if (ret < 0)
			return -EFAULT;

		break;
	}
	case HDCP_IOC_GET_RI_REG: {
		int ret;
		unsigned int result;
		unsigned char ri0, ri1;

		ILOG("ioctl(HDCP_IOC_GET_RI_REG)\n");

		// get ri from TX
		ri0 = hdcp_readb(HDCP_RI_0);
		ri1 = hdcp_readb(HDCP_RI_1);

		result = 0;
		result = ri0;
		result = ((result << 8) | ri1) & 0xFFFF;

		// copy to user
		ret = copy_to_user(
			(void *)arg, (const void *)&result, sizeof(result));
		if (ret < 0)
			return -EFAULT;
		break;
	}
	case HDCP_IOC_GET_AUTH_STATE: {
		int ret;
		int result = 1;

		ILOG("ioctl(HDCP_IOC_GET_AUTH_STATE)\n");

		// check AUTH state
		if (!(hdcp_readb(HDMI_STATUS) & (1 << HDCP_AUTHEN_ACK_NUM)))
			result = 0;

		// copy to user
		ret = copy_to_user(
			(void *)arg, (const void *)&result, sizeof(result));
		if (ret < 0)
			return -EFAULT;
		break;
	}
	case HDCP_IOC_SET_HDCP_RESULT: {
		int ret;
		unsigned char match = 1;

		ILOG("ioctl(HDCP_IOC_SET_HDCP_RESULT)\n");

		// copy form user
		ret = copy_from_user(
			(void *)&match, (const void *)arg, sizeof(match));
		if (ret < 0)
			return -EFAULT;

		// set result
		hdcp_set_result(match);

		break;
	}
	case HDCP_IOC_SET_ENCRYPTION: {
		int ret;
		unsigned char tmp;
		unsigned char reg;

		ILOG("ioctl(HDCP_IOC_SET_ENCRYPTION) ON\n");

		// copy form user
		ret = copy_from_user(
			(void *)&tmp, (const void *)arg, sizeof(tmp));
		if (ret < 0)
			return -EFAULT;

		// set encryption
		if (tmp) {
			// check AUTH state
			int loop = 1;

			while (loop) {
				reg = hdcp_readb(HDMI_STATUS);
				ILOG("HDMI_STATUS = 0x%02x\n", reg);
				if (!(reg & (1 << HDCP_AUTHEN_ACK_NUM)))
					loop++;
				else
					loop = 0;

				if (loop > 200)
					loop = 0;

				mdelay(1);
			}

			reg = hdcp_readb(HDMI_STATUS);
			ILOG("HDMI_STATUS = 0x%02x, loop = %d\n", reg, loop);

			// enable encryption
			hdcp_enable_encryption(1);
		} else {
			// disable encryption
			hdcp_enable_encryption(0);

		}
		break;
	}
	case HDCP_IOC_SET_BSTATUS: {
		struct hdcp_status Bstatus;
		int ret;

		ILOG("ioctl(HDCP_IOC_SET_BSTATUS)\n");

		// get arg
		ret = copy_from_user(
			(void *)&Bstatus.status, (const void *)arg,
			sizeof(Bstatus));
		if (ret < 0)
			return -EFAULT;

		hdcp_writeb(Bstatus.status[0], HDCP_BSTATUS_0);
		hdcp_writeb(Bstatus.status[1], HDCP_BSTATUS_1);

		break;
	}
	case HDCP_IOC_SET_KSV_LIST: {
		struct hdcp_ksv_list list;
		int ret;

		ILOG("ioctl(HDCP_IOC_SET_KSV_LIST)\n");

		// get size arg;
		ret = copy_from_user(
			(void *)&list, (const void *)arg, sizeof(list));
		if (ret < 0)
			return -EFAULT;

		hdcp_set_ksv_list(list);

		break;
	}
	case HDCP_IOC_SET_SHA1: {
		struct hdcp_sha1 rx_sha1;
		int index, ret;

		ILOG("ioctl(HDCP_IOC_SET_SHA1)\n");

		// get arg;
		ret = copy_from_user(
			(void *)&rx_sha1, (const void *)arg, sizeof(rx_sha1));
		if (ret < 0)
			return -EFAULT;

		// set sha1
		for (index = 0; index < HDCP_SHA1_SIZE; index++)
			hdcp_writeb(
				rx_sha1.sha1[index], HDCP_SHA1_00 + 4 * index);

		break;
	}
	case HDCP_IOC_GET_SHA1_RESULT: {
		int ret;
		int result = 1;

		ILOG("ioctl(HDCP_IOC_GET_SHA1_RESULT)\n");

		if (hdcp_readb(HDCP_SHA_RESULT) & HDCP_SHA1_VALID_READY) {
			if (!(hdcp_readb(HDCP_SHA_RESULT) & HDCP_SHA1_VALID))
				result = 0;
		} else
			result = 0;

		// reset
		hdcp_writeb(0x00, HDCP_SHA_RESULT);

		// copy to user
		ret = copy_to_user(
			(void *)arg, (const void *)&result, sizeof(result));
		if (ret < 0)
			return -EFAULT;

		break;
	}
	case HDCP_IOC_GET_KSV_LIST_READ_DONE: {
		int ret;
		unsigned char reg;

		reg = hdcp_readb(HDCP_KSV_LIST_CON) & HDCP_KSV_LIST_READ_DONE;

		// copy to user
		ret = copy_to_user(
			(void *)arg, (const void *)&reg, sizeof(reg));
		if (ret < 0)
			return -EFAULT;

		break;
	}
	case HDCP_IOC_SET_ILLEGAL_DEVICE: {
		// set
		hdcp_writeb(HDCP_REVOCATION_SET, HDCP_CTRL2);
		// clear
		hdcp_writeb(0x00, HDCP_CTRL2);

		break;
	}
	case HDCP_IOC_SET_REPEATER_TIMEOUT: {
		unsigned char reg;

		reg = hdcp_readb(HDCP_CTRL1);
		// set
		hdcp_writeb(reg | HDCP_TIMEOUT, HDCP_CTRL1);
		// clear
		hdcp_writeb(reg, HDCP_CTRL1);
	}
	case HDCP_IOC_RESET_HDCP: // reset hdcp state machine
	{
		hdcp_reset();
		break;
	}
	case HDCP_IOC_SET_KSV_LIST_EMPTY: {
		hdcp_writeb(HDCP_KSV_LIST_EMPTY, HDCP_KSV_LIST_CON);
		break;
	}
	case HDCP_SEC_IOCTL_SET_KEY: {
		int ret;
		unsigned char *regData;
		TRACE;
		regData = kmalloc(16+288, GFP_KERNEL); //288+16 means keycontents plus key size
		if (regData == NULL) {
			return -ENOMEM;
		}
		if (copy_from_user(
			    regData, (const void *)arg,
			    16+288)) {
			return -EFAULT;
		}
		//print_hex_dump_bytes("DATA1: ", DUMP_PREFIX_NONE, &regData[0], 16);
		//print_hex_dump_bytes("DATA2: ", DUMP_PREFIX_NONE, &regData[16], 288);
		hdcp_load_data(&regData[0], &regData[16]);
		kfree(regData);
		break;
	}
	
	default:
		TRACE;
		return -EINVAL;
	}

	return 0;
}

int hdcp_api_status_chk(unsigned char flag)
{
	unsigned int event = 0;

#if (HDCP_API_DEBUG_IRQ) // debug
	// ILOG("HDCP INT :");
	if (flag & (1 << HDCP_I2C_INT_NUM)) {
		ILOG(" I2C_INT\n");
	}
	if (flag & (1 << HDCP_AN_WRITE_INT_NUM)) {
		ILOG(" AN_WRITE_INT\n");
	}
	if (flag & (1 << HDCP_UPDATE_RI_INT_NUM)) {
		ILOG(" UPDATE_RI_INT\n");
	}
	if (flag & (1 << HDCP_WATCHDOG_INT_NUM)) {
		ILOG(" WATCHDOG_INT\n");
	}
	if (flag & (1 << HDCP_AUD_FIFO_OVF_EN_NUM)) {
		ILOG(" AUDIO BUFFER OVERFLOW\n");
	}
	// ILOG("\n");
#endif

	// processing interrupt
	// I2C INT
	if (flag & (1 << HDCP_I2C_INT_NUM)) {
		event |= (1 << HDCP_EVENT_READ_BKSV_START);
		// clear pending
		hdcp_writeb((1 << HDCP_I2C_INT_NUM), HDMI_STATUS);
		hdcp_writeb(0x00, HDCP_I2C_INT);
	}
	// AN INT
	if (flag & (1 << HDCP_AN_WRITE_INT_NUM)) {
		event |= (1 << HDCP_EVENT_WRITE_AKSV_START);
		// clear pending
		hdcp_writeb((1 << HDCP_AN_WRITE_INT_NUM), HDMI_STATUS);
		hdcp_writeb(0x00, HDCP_AN_INT);
	}
	// RI INT
	if (flag & (1 << HDCP_UPDATE_RI_INT_NUM)) {
		// clear pending
		hdcp_writeb((1 << HDCP_UPDATE_RI_INT_NUM), HDMI_STATUS);
		hdcp_writeb(0x00, HDCP_RI_INT);
		event |= (1 << HDCP_EVENT_CHECK_RI_START);
	}
	// WATCHDOG INT
	if (flag & (1 << HDCP_WATCHDOG_INT_NUM)) {
		event |= (1 << HDCP_EVENT_SECOND_AUTH_START);
		// clear pending
		hdcp_writeb((1 << HDCP_WATCHDOG_INT_NUM), HDMI_STATUS);
		hdcp_writeb(0x00, HDCP_WDT_INT);
	}
	// HDCP_AUD_FIFO_OVF_EN_NUM
	if (flag & (1 << HDCP_AUD_FIFO_OVF_EN_NUM)) {
		// clear pending
		hdcp_writeb((1 << HDCP_AUD_FIFO_OVF_EN_NUM), HDMI_STATUS);
	}

	// set event
	spin_lock(&hdcp_struct.lock);
	hdcp_struct.event |= event;
	spin_unlock(&hdcp_struct.lock);

	//		ILOG("-->wake_up event: %08X, [hdcp_struct.event:
	//%08X]\n", event, tmp_event);

#if (HDCP_API_DEBUG_IRQ) // debug
	/** Stop HDCP  */
	if (event & (1 << HDCP_EVENT_STOP))
		ILOG("  >(HDCP_EVENT_STOP)\n");
#if 0
		/** Start HDCP */
		if (event & (1<<HDCP_EVENT_START))
			ILOG("  >(HDCP_EVENT_START)\n");
#endif /* 0 */

	/** Start to read Bksv,Bcaps */
	if (event & (1 << HDCP_EVENT_READ_BKSV_START))
		ILOG("  >(HDCP_EVENT_READ_BKSV_START)\n");
	/** Start to write Aksv,An */
	if (event & (1 << HDCP_EVENT_WRITE_AKSV_START))
		ILOG("  >(HDCP_EVENT_WRITE_AKSV_START)\n");
	/** Start to check if Ri is equal to Rj */
	if (event & (1 << HDCP_EVENT_CHECK_RI_START))
		ILOG("  >(HDCP_EVENT_CHECK_RI_START)\n");
	/** Start 2nd authentication process */
	if (event & (1 << HDCP_EVENT_SECOND_AUTH_START))
		ILOG("  >(HDCP_EVENT_SECOND_AUTH_START)\n");
#endif

	// wake up
	wake_up_interruptible(&hdcp_struct.waitq);

	return 1;
}
EXPORT_SYMBOL(hdcp_api_status_chk);

unsigned int hdcp_api_poll_chk(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	// poll_wait
	poll_wait(file, &hdcp_struct.waitq, wait);

	// if event is not zero
	if (hdcp_struct.event != 0) {
		mask |= (POLLIN | POLLRDNORM);

		// ILOG("  POLLIN - mask = %d\n", mask);
	}

	return mask;
}

int hdcp_api_open(void)
{
	ILOG("<open>\n");

	return 0;
}

int hdcp_api_close(void)
{
	ILOG("<close>\n");

	hdcp_disable();
	hdmi_api_reg_hdcp_callback((hdcp_callback)NULL);

	return 0;
}

int hdcp_api_initialize(void)
{
	init_waitqueue_head(&hdcp_struct.waitq);
	spin_lock_init(&hdcp_struct.lock);

	hdmi_api_reg_hdcp_callback((hdcp_callback)&hdcp_api_status_chk);

	ILOG("<init>\n");

	return 0;
}
