/*
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2008
 * Description: Telechips Linux H/W Demux Driver
 *
 * Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "hwdmx_cmd.h"
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mailbox/tcc_sp_ipc.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <asm/bitops.h>

#include "tca_hwdemux_param.h"
#include "hwdmx.h"

// clang-format off
/* HWDMX command range: 0x000 ~ 0xFFF*/
#define MAGIC_NUM						0	// for HWDMX
#define HWDMX_START_CMD					SP_CMD(MAGIC_NUM, 0x001)
#define HWDMX_STOP_CMD					SP_CMD(MAGIC_NUM, 0x002)
#define HWDMX_ADD_FILTER_CMD			SP_CMD(MAGIC_NUM, 0x003)
#define HWDMX_DELETE_FILTER_CMD			SP_CMD(MAGIC_NUM, 0x004)
#define HWDMX_GET_STC_CMD				SP_CMD(MAGIC_NUM, 0x005)
#define HWDMX_SET_PCRPID_CMD			SP_CMD(MAGIC_NUM, 0x006)
#define HWDMX_GET_VERSION_CMD			SP_CMD(MAGIC_NUM, 0x007)
#define HWDMX_INPUT_STREAM_CMD			SP_CMD(MAGIC_NUM, 0x008)
#define HWDMX_CHANGE_INTERFACE_CMD		SP_CMD(MAGIC_NUM, 0x009)
#define HWDMX_SET_MODE_CMD				SP_CMD(MAGIC_NUM, 0x00A)
#define HWDMX_SET_KEY_CMD				SP_CMD(MAGIC_NUM, 0x00B)
#define HWDMX_SET_IV_CMD				SP_CMD(MAGIC_NUM, 0x00C)
#define HWDMX_SET_DATA_CMD				SP_CMD(MAGIC_NUM, 0x00D)
#define HWDMX_SET_RUN_CMD				SP_CMD(MAGIC_NUM, 0x00E)
#define HWDMX_SET_KDFDATA_CMD			SP_CMD(MAGIC_NUM, 0x00F)
#define HWDMX_SET_KLDATA_CMD			SP_CMD(MAGIC_NUM, 0x010)
#define HWDMX_GET_KLNRESP_CMD			SP_CMD(MAGIC_NUM, 0x011)
#define HWDMX_SET_MODE_ADDPID_CMD		SP_CMD(MAGIC_NUM, 0x012)
/* Up to 16 of events can be assigned. */
#define HWDMX_WBUF_UPDATED_EVT			SP_EVENT(MAGIC_NUM, 1)
#define HWDMX_STC_RECV_EVT				SP_EVENT(MAGIC_NUM, 2)
#define HWDMX_RBUF_EMPTY_EVT			SP_EVENT(MAGIC_NUM, 3)
/* ... */
#define HWDMX_DEBUG_DATA_EVT			SP_EVENT(MAGIC_NUM, 16)
// clang-format on

#define STC_INTERVAL 1000    // Every 1000ms, CM notifiy STC time to A9
#define PCRUPDATE_RES_MS 200 // PCR Update Resolution [unit ms]

#define DEAULT_AES_128KEYSIZE 16
#define DEAULT_AES_IVSIZE 16

enum
{ // For mode
	NORMAL = 0,
	BYPASS
};

enum
{
	TSIF_SERIAL = 0, // for uiTSIFInterface
	TSIF_PARALLEL,   // for uiTSIFInterface
	SPI,             // for uiTSIFInterface
	INTERNAL         // for uiTSIFInterface - Demuxer ID should be 1
};

// FILTER TYPE
enum
{
	SECTION = 0,
	TS,
	PES,
	PCR,
};

enum
{
	even=2,
	odd=3,
};

static FN_UPDATECALLBACK buf_updated_cb[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static int interface[8] = {
	TSIF_SERIAL, TSIF_SERIAL, TSIF_SERIAL, TSIF_SERIAL,
	TSIF_SERIAL, TSIF_SERIAL, TSIF_SERIAL, TSIF_SERIAL,
};
static int session_cnt = 0;
static DECLARE_WAIT_QUEUE_HEAD(waitq_empty_bufevt);
static unsigned long empty_bufevt_received = 0;

static int hwdmx_evt_handler(int cmd, void *rdata, int size)
{
	int dmxid = ((int *)rdata)[0];

	switch (cmd) {
	case HWDMX_WBUF_UPDATED_EVT: {
		int filter_type, filter_id, value1, value2, err_crc;

		if (size != sizeof(int) * 10) {
			pr_err("[ERROR][HWDMX] %s:%d size is wrong\n", __func__, __LINE__);
			return -1;
		}

		filter_type = ((int *)rdata)[1];
		filter_id = ((int *)rdata)[2];

		// ts: end point of updated buffer(offset), sec: start point of updated
		// buffer(offset)
		value1 = ((int *)rdata)[3];

		// ts: end point of buffer(offset), sec: end point of updated buffer(offset)
		value2 = ((int *)rdata)[4] + 1;

		err_crc = ((int *)rdata)[5];

		switch (filter_type) {
		case TS:
			value1++;
			break;

		case SECTION:
			break;

		default:
			break;
		}

		if (buf_updated_cb[dmxid] != NULL) {
			buf_updated_cb[dmxid](dmxid, filter_type, filter_id, value1, value2, err_crc);
		}

		break;
	}

	case HWDMX_STC_RECV_EVT: {
		int stc_base, stc_ext;
		int filter_type = PCR, fid = 0;

		if (size != sizeof(int) * 4) {
			return -1;
		}

		stc_base = ((int *)rdata)[1];
		stc_ext = ((int *)rdata)[2]; // Bit 31: STC Extension
		if (buf_updated_cb[dmxid] != NULL) {
			buf_updated_cb[dmxid](dmxid, filter_type, fid, stc_base, stc_ext, ((int *)rdata)[3]);
		}
		break;
	}

	case HWDMX_RBUF_EMPTY_EVT:
		/* Atomic operation */
		set_bit(0, &empty_bufevt_received);

		wake_up(&waitq_empty_bufevt);
		break;

	case HWDMX_DEBUG_DATA_EVT: {
		char str[9];
		int *p = (unsigned int *)str;

		if (size != sizeof(int) * 7) {
			return -1;
		}

		p[0] = ((int *)rdata)[0];
		p[1] = ((int *)rdata)[1];
		str[8] = 0;
		printk(
			"[HWDMX]%s :[0x%X][0x%X][0x%X][0x%X][0x%X].\n", str, ((int *)rdata)[2],
			((int *)rdata)[3], ((int *)rdata)[4], ((int *)rdata)[5], ((int *)rdata)[6]);
		break;
	}

	default:
		break;
	}

	return 0;
}

void hwdmx_set_evt_handler(void)
{
	sp_set_callback(hwdmx_evt_handler);
}

void hwdmx_set_interface_cmd(int iDMXID, int mode)
{
	int mbox_data[3];
	int i, tsif;

	if (mode == HWDMX_INTERNAL)
		tsif = INTERNAL;
	else if (mode == HWDMX_EXT_PARALLEL)
		tsif = TSIF_PARALLEL;
	else // if (mode == HWDMX_EXT_SERIAL)
		tsif = TSIF_SERIAL;

	if (iDMXID < 0) {
		for (i = 0; i < 8; i++) {
			if (interface[i] != tsif) {
				interface[i] = tsif;
				mbox_data[0] = i;
				mbox_data[1] = tsif;
				mbox_data[2] = 0;
				sp_sendrecv_cmd(HWDMX_CHANGE_INTERFACE_CMD, mbox_data, sizeof(mbox_data), NULL, 0);
			}
		}
	} else if (iDMXID < 8) {
		if (interface[iDMXID] != tsif) {
			interface[iDMXID] = tsif;
			mbox_data[0] = iDMXID;
			mbox_data[1] = tsif;
			mbox_data[2] = 0;
			sp_sendrecv_cmd(HWDMX_CHANGE_INTERFACE_CMD, mbox_data, sizeof(mbox_data), NULL, 0);
		}
	}
}
EXPORT_SYMBOL(hwdmx_set_interface_cmd);

int hwdmx_get_interface(int iDMXID)
{
	if (interface[iDMXID] == INTERNAL) {
		return HWDMX_INTERNAL;
	} else if (interface[iDMXID] == TSIF_PARALLEL) {
		return HWDMX_EXT_PARALLEL;
	} else if (interface[iDMXID] == TSIF_SERIAL) {
		return HWDMX_EXT_SERIAL;
	}
	return -1;
}

int hwdmx_start_cmd(struct tcc_tsif_handle *h)
{
	int result = 0, rsize;
	int tsif;
	int mbox_data[12], mbox_result;

	pr_info(
		"\n[INFO][HWDMX]%s:%d:0x%08x:0x%p:0x%08X port:%u\n", __func__, __LINE__,
		(u32)h->dma_buffer->dma_addr, h->dma_buffer->v_addr, h->dma_buffer->buf_size,
		h->port_cfg.tsif_port);

	if (session_cnt == 0) {
		sp_set_callback(hwdmx_evt_handler);
	}

	switch (h->working_mode) {
	case 0x00: // tsif for tdmb
		tsif = SPI;
		break;

	default: // 0x01 for tsif of isdbt & dvbt
		tsif = interface[h->dmx_id];
		break;
	}

	// TSIf Polarity : TSP-Sync Pulse(Bit0) : 0(hight active), TVEP-Valid
	// Data(Bit1) : 1(high active), TCKP(Bit2) : 0(riging edge of TSCLK)

	// DEMOD_MODULE_LGDT3305
	// Arg.uiTSIFPol |= Hw2;    //TCKP(Bit2) : 1(falling edge of TSCLK)

	mbox_data[0] = h->dmx_id;
	mbox_data[1] = NORMAL;
	mbox_data[2] = 0;
	mbox_data[3] = h->dma_buffer->dma_addr;
	mbox_data[4] = h->dma_buffer->buf_size;
	mbox_data[5] = h->dma_buffer->dma_sec_addr;
	mbox_data[6] = h->dma_buffer->buf_sec_size;
	mbox_data[7] = tsif;
	mbox_data[8] = h->dmx_id;
	mbox_data[9] = h->port_cfg.tsif_port;
	mbox_data[10] = 0x2; // HW1
	mbox_data[11] = 0;
	rsize = sp_sendrecv_cmd(
		HWDMX_START_CMD, mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result == 0) {
		session_cnt++;
	} else {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}

out:
	return result;
}

int hwdmx_stop_cmd(struct tcc_tsif_handle *h)
{
	int result, rsize;
	int mbox_data, mbox_result;

	if (h == NULL) {
		return 0;
	}

	if (session_cnt == 0) {
		return 0;
	}

	mbox_data = (int)h->dmx_id;
	rsize = sp_sendrecv_cmd(
		HWDMX_STOP_CMD, &mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result == 0) {
		buf_updated_cb[h->dmx_id] = NULL;
		session_cnt--;
	} else {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}
	pr_info("[INFO][HWDMX] HWDMX_STOP_CMD Result : %d\n", result);

out:
	return result;
}

int hwdmx_buffer_updated_callback(struct tcc_tsif_handle *h, FN_UPDATECALLBACK buffer_updated)
{
	buf_updated_cb[h->dmx_id] = buffer_updated;
	return 0;
}

int hwdmx_set_pcrpid_cmd(struct tcc_tsif_handle *h, unsigned int pid)
{
	int result = 0, rsize;
	int mbox_data[4], mbox_result;

	mbox_data[0] = h->dmx_id;
	mbox_data[1] = pid;
	mbox_data[2] = STC_INTERVAL & 0xffff;
	mbox_data[3] = PCRUPDATE_RES_MS;
	rsize = sp_sendrecv_cmd(
		HWDMX_SET_PCRPID_CMD, mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}
	pr_info("[INFO][HWDMX] [DEMUX #%d]hwdmx_set_pcrpid(pid=%d)\n", h->dmx_id, pid);

out:
	return result;
}

#if 0
// FIXME: SP f/w for this function must be fixed.
long long hwdmx_get_stc(struct tcc_tsif_handle *h)
{
	// dprintk("[DEMUX #%d]hwdmx_get_stc\n", h->dmx_id);
	int mbox_data[] = {h->dmx_id, STC_INTERVAL};
	long long stc;

	mbox_data[0] = h->dmx_id;
	mbox_data[1] = STC_INTERVAL;
	mbox_data[2] = 0;
	mbox_data[3] = 0;
	sp_sendrecv_cmd(HWDMX_GET_STC_CMD, mbox_data, mbox_data, sizeof(mbox_data));
	stc = *(long long *)(mbox_data + 2);

	return stc;
}
#endif

int hwdmx_add_filter_cmd(struct tcc_tsif_handle *h, struct tcc_tsif_filter *feed)
{
	int result = 0, rsize;
	int mbox_data[5 + (16 * 3 / 4) /* for vector_data */], mbox_result;

	mbox_data[0] = h->dmx_id;
	mbox_data[1] = feed->f_type;
	mbox_data[2] = feed->f_pid;
	switch (feed->f_type) {
	case SECTION:
		if (feed->f_size > 16) {
			pr_err("[ERROR][HWDMX] !!! filter size is over 16 then it sets to 16.\n");
			feed->f_size = 16; // HWDMX can support less than 16 bytes filter size.
		}
#if 0
        {
			int i;
			for (i = 0; i < feed->f_size; i++)
				pr_info(
					"[INFO][HWDMX][%d]C[0x%X]M[0x%X]M[0x%X]\n", i, feed->f_comp[i], feed->f_mask[i],
					feed->f_mode[i]);
		}
#endif
		mbox_data[3] = feed->f_id;
		mbox_data[4] = feed->f_size;
		memcpy((int8_t *)(mbox_data + 5), feed->f_comp, feed->f_size);
		memcpy((int8_t *)(mbox_data + 5) + feed->f_size, feed->f_mask, feed->f_size);
		memcpy((int8_t *)(mbox_data + 5) + feed->f_size * 2, feed->f_mode, feed->f_size);
#if 0
		print_hex_dump_bytes(
			"FltComp: ", DUMP_PREFIX_ADDRESS, feed->f_comp, feed->f_size);
		print_hex_dump_bytes(
			"FltMask: ", DUMP_PREFIX_ADDRESS, feed->f_mask, feed->f_size);
		print_hex_dump_bytes(
			"FltMode: ", DUMP_PREFIX_ADDRESS, feed->f_mode, feed->f_size);
		pr_info("[INFO][HWDMX] %s:%d mbox_size: %d\n", __func__, __LINE__, 20 + feed->f_size * 3);
#endif
		rsize = sp_sendrecv_cmd(
			HWDMX_ADD_FILTER_CMD, mbox_data, 20 + feed->f_size * 3, &mbox_result, sizeof(mbox_result));
		break;

	case TS:
	case PES:
		rsize = sp_sendrecv_cmd(
			HWDMX_ADD_FILTER_CMD, mbox_data, sizeof(int) * 3, &mbox_result, sizeof(mbox_result));
		break;

	default:
		pr_err("[ERROR][HWDMX] [%s:%d] filter type is undefined\n", __func__, __LINE__);
		goto out;
		break;
	}

	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}

	pr_info(
		"[INFO][HWDMX] [DEMUX #%d]hwdmx_add_filter(type=%d, pid=%d)\n", h->dmx_id, feed->f_type,
		feed->f_pid);
out:
	return result;
}

int hwdmx_remove_filter_cmd(struct tcc_tsif_handle *h, struct tcc_tsif_filter *feed)
{
	int result = 0, rsize;
	int mbox_data[4], mbox_result;
	int fid;

	pr_info(
		"[INFO][HWDMX][DEMUX #%d]hwdmx_remove_filter(type=%d, pid=%d)\n", h->dmx_id, feed->f_type,
		feed->f_pid);

	fid = (feed->f_type == SECTION) ? feed->f_id : 0;
	mbox_data[0] = h->dmx_id;
	mbox_data[1] = fid;
	mbox_data[2] = feed->f_type;
	mbox_data[3] = feed->f_pid;
	rsize = sp_sendrecv_cmd(
		HWDMX_DELETE_FILTER_CMD, mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}

out:
	return result;
}

int hwdmx_input_stream_cmd(unsigned int dmx_id, unsigned int phy_addr, unsigned int size)
{
	int result = 0, rsize;
	int mbox_data[3], mbox_result;
	static DEFINE_MUTEX(input_stream_mutex);

	// pr_info("[INFO][HWDMX] [DEMUX #%d]hwdmx_input_internal(buffer=[0x%X], size=%d)\n",
	// dmx_id, phy_addr, size);

	if (session_cnt == 0) {
		// pr_err("[ERROR][HWDMX] session count is zero\n");
		return -1;
	}
	mutex_lock(&input_stream_mutex); //for multiple demux, it is critical section
	/* Atomic operation */
	clear_bit(0, &empty_bufevt_received);

	mbox_data[0] = dmx_id;
	mbox_data[1] = phy_addr;
	mbox_data[2] = size;
	rsize = sp_sendrecv_cmd(
		HWDMX_INPUT_STREAM_CMD, mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}

	result =
		wait_event_timeout(waitq_empty_bufevt, empty_bufevt_received == 1, msecs_to_jiffies(2000));
	if (result == 0 && (empty_bufevt_received != 1)) {
		pr_info("[INFO][HWDMX] Timeout\n");
	} else {
		/* The condition (empty_bufevt_received == 1) is met before timeout */
		result = 0;
	}

out:
	mutex_unlock(&input_stream_mutex); //for multiple demux
	return result;
}

int hwdmx_set_cipher_dec_pid_cmd(struct tcc_tsif_handle *h,
		unsigned int numOfPids, unsigned int delete_option, unsigned short *pids)
{
	int result = 0, rsize;
	int mbox_data[3 + 4], mbox_result;

	mbox_data[0] = h->dmx_id;
	mbox_data[1] = numOfPids;
	mbox_data[2] = delete_option;
	if(numOfPids>0)
		memcpy(mbox_data + 3, (unsigned char *)pids, numOfPids*2);
	
	rsize = sp_sendrecv_cmd(
			HWDMX_SET_MODE_ADDPID_CMD, mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}

out:
	return result;
}

int hwdmx_set_algo_cmd(struct tcc_tsif_handle *h, int algo,
	int opmode, int residual, int smsg, unsigned int numOfPids, unsigned short *pids)
{
	int result = 0, rsize;
	int mbox_data[6 + 4], mbox_result;

	mbox_data[0] = h->dmx_id;
	mbox_data[1] = opmode;
	mbox_data[2] = algo;
	mbox_data[3] = residual;
	mbox_data[4] = smsg;
	mbox_data[5] = numOfPids;
	if(numOfPids>0)
		memcpy(mbox_data + 6, (unsigned char *)pids, numOfPids*2);

	rsize = sp_sendrecv_cmd(
		HWDMX_SET_MODE_CMD, mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(hwdmx_set_algo_cmd);

int hwdmx_set_key_cmd(struct tcc_tsif_handle *h, int keytype, int keymode, int size, void *key)
{
	int result = 0, rsize;
	int mbox_data[4 + 32 /* For 256bit key length */], mbox_result;

	mbox_data[0] = h->dmx_id;
	mbox_data[1] = keytype;
	mbox_data[2] = size;
	mbox_data[3] = keymode;
	memcpy(mbox_data + 4, key, size);

	rsize = sp_sendrecv_cmd(
		HWDMX_SET_KEY_CMD, mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(hwdmx_set_key_cmd);

int hwdmx_set_iv_cmd(struct tcc_tsif_handle *h, int ividx, int size, void *iv)
{
	int result = 0, rsize;
	int mbox_data[3 + 16 /* IV length */], mbox_result;

	mbox_data[0] = h->dmx_id;
	mbox_data[1] = ividx;
	mbox_data[2] = size;
	memcpy(mbox_data + 3, iv, size);
	rsize = sp_sendrecv_cmd(
		HWDMX_SET_IV_CMD, mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}
	pr_info("[INFO][HWDMX] %s, %d  \n", __func__, __LINE__);

out:
	return result;
}
EXPORT_SYMBOL(hwdmx_set_iv_cmd);

int  hwdmx_setCw(struct tcc_tsif_handle *h/*unsigned int _tunerId*/, unsigned int _descramblingAlgorithm,
		unsigned int _opt, unsigned char *_evenKey, unsigned char *_oddKey,
		unsigned char *_iv, unsigned int _numOfPids, unsigned short *_pids)
{

	hwdmx_set_algo_cmd(h, _descramblingAlgorithm, _opt, 0, 0, _numOfPids, _pids);
	hwdmx_set_key_cmd(h, even, 0, DEAULT_AES_128KEYSIZE, _evenKey);
	hwdmx_set_key_cmd(h, odd, 0, DEAULT_AES_128KEYSIZE, _oddKey);
	hwdmx_set_iv_cmd(h, 1, DEAULT_AES_IVSIZE, _iv);

	return 0;
}


int hwdmx_set_data_cmd(
	unsigned int dmxid, unsigned char *srcaddr, unsigned char *destaddr, unsigned int srclen)
{
	int result = 0, rsize;
	int mbox_data[4], mbox_result;

	mbox_data[0] = dmxid;
	mbox_data[1] = (int)srcaddr;
	mbox_data[2] = (int)destaddr;
	mbox_data[3] = srclen;
	rsize = sp_sendrecv_cmd(
		HWDMX_SET_DATA_CMD, mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(hwdmx_set_data_cmd);

int hwdmx_run_cipher_cmd(int dmxid, int encmode, int cwsel, int klidx, int keymode)
{
	int result = 0, rsize;
	int mbox_data[5], mbox_result;

	mbox_data[0] = dmxid;
	mbox_data[1] = encmode;
	mbox_data[2] = cwsel;
	mbox_data[3] = klidx;
	mbox_data[4] = keymode;
	rsize = sp_sendrecv_cmd(
		HWDMX_SET_MODE_CMD, mbox_data, sizeof(mbox_data), &mbox_result, sizeof(mbox_result));
	if (rsize < 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] sp_sendrecv_cmd error\n", __func__, __LINE__);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		pr_err("[ERROR][HWDMX] [%s:%d] SP returned an error: %d\n", __func__, __LINE__, result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(hwdmx_run_cipher_cmd);
