/*
 * linux/drivers/spi/tcc_hwdemux_tsif.c
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/sysfs.h>
#include <linux/atomic.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <soc/tcc/pmap.h>

#include "TSDEMUX_sys.h"
#include "tca_hwdemux_param.h"
#include "tcc_hwdemux_tsif_rx.h"

#define DEMUX_SYSFS

#define USE_REV_MEMORY

#define SECTION_DMA_SIZE (1024 * 1024)

#if defined(CONFIG_ARCH_TCC897X)
#define HWDMX_NUM 4
#else
#define HWDMX_NUM 8
#endif

//#define TS_PACKET_CHK_MODE

static DEFINE_SPINLOCK(timer_lock);
static int hwdmx_tsif_num;

#if defined(DEMUX_SYSFS)
struct tcc_tsif_stat
{
	atomic_t pidnum;
	atomic_t secnum;
	unsigned long tsevt;
	unsigned long secevt;
	unsigned long pcrevt;
};
#endif

struct tcc_tsif_pri_handle
{
	int open_cnt;
	struct mutex mutex;
	struct tcc_hwdmx_tsif_rx_handle demux[HWDMX_NUM];
	struct timer_list timer;
#if defined(USE_REV_MEMORY)
	pmap_t pmap_tsif;
	void *mem_base;
#endif
	struct tea_dma_buf *static_dma_buffer[HWDMX_NUM];
	struct tca_tsif_port_config port_cfg[HWDMX_NUM];
	struct device *dev[HWDMX_NUM];
#if defined(DEMUX_SYSFS)
	struct tcc_tsif_stat stat[HWDMX_NUM];
#endif
};
static struct tcc_tsif_pri_handle tsif_ex_pri;

#ifdef TS_PACKET_CHK_MODE
typedef struct
{
	struct ts_packet_chk_t *next;

	unsigned short pid;
	unsigned char cc;

	unsigned long long cnt;
	unsigned int err;
} ts_packet_chk_t;

typedef struct
{
	unsigned long long total_cnt;
	unsigned int total_err;
	unsigned int packet_cnt;

#define DEBUG_CHK_TIME 10 // sec
	unsigned int cur_time;
	unsigned int prev_time;
	unsigned int debug_time;
	struct timeval time;

	ts_packet_chk_t *packet;
} ts_packet_chk_info_t;
ts_packet_chk_info_t *ts_packet_chk_info = NULL;

static void itv_ts_cc_debug(int mod)
{
	if (ts_packet_chk_info != NULL) {
		if (ts_packet_chk_info->packet != NULL) {
			pr_info(
				"\n[INFO][HWDMX] [total:%llu / err:%d (%d sec)]\n", ts_packet_chk_info->total_cnt,
				ts_packet_chk_info->total_err, (ts_packet_chk_info->debug_time * DEBUG_CHK_TIME));

			if (mod) {
				ts_packet_chk_t *tmp = NULL;

				tmp = ts_packet_chk_info->packet;
				do {
					pr_info(
						"[INFO][HWDMX] \t\tpid:0x%04x => cnt:%llu err:%d\n", tmp->pid, tmp->cnt,
						tmp->err);
					tmp = tmp->next;
				} while (tmp != NULL);
			}
		}
	}
}

static void itv_ts_cc_remove(unsigned short pid)
{
	ts_packet_chk_t *tmp = NULL;
	ts_packet_chk_t *tmp_prev = NULL;

	if (ts_packet_chk_info->packet) {
		tmp = ts_packet_chk_info->packet;
		while (1) {
			if (tmp->pid == pid) {
				if (tmp->next == NULL) {
					if (tmp_prev == NULL) { // 1st
						ts_packet_chk_info->packet = NULL;
						kfree(tmp);
					} else { // last
						tmp_prev->next = NULL;
						kfree(tmp);
					}
				} else {
					tmp_prev = tmp;
					tmp = tmp->next;

					ts_packet_chk_info->packet = tmp;
					kfree(tmp_prev);
				}

				break;
			} else {
				tmp_prev = tmp;
				tmp = tmp->next;

				if (tmp == NULL) {
					break;
				}
			}
		}
	}
}

static void itv_ts_cc_check(unsigned char *buf)
{
	unsigned short pid;
	unsigned char cc;

	ts_packet_chk_t *tmp = NULL;

	pid = ((*(unsigned char *)(buf + 1) & 0x1f) << 8) | *(unsigned char *)(buf + 2);
	cc = *(unsigned char *)(buf + 3) & 0x0f;

	if (ts_packet_chk_info != NULL) {
		ts_packet_chk_info->total_cnt++;

		if (ts_packet_chk_info->packet == NULL) {
			tmp = (ts_packet_chk_t *)kmalloc(sizeof(ts_packet_chk_t), GFP_ATOMIC);
			if (tmp == NULL) {
				pr_err("[ERROR][HWDMX] \t ts_packet_chk_t mem alloc err..\n");
			}

			memset(tmp, 0x0, sizeof(ts_packet_chk_t));
			tmp->pid = pid;
			tmp->cc = cc;
			tmp->next = NULL;
			tmp->cnt++;
			ts_packet_chk_info->packet = tmp;
			ts_packet_chk_info->packet_cnt++;

			pr_info(
				"[INFO][HWDMX] \t>>>> create[%d] : 0x%04x / %02d\n", ts_packet_chk_info->packet_cnt,
				tmp->pid, tmp->cc);
		} else {
			unsigned char new = 0;
			unsigned int temp;

			tmp = ts_packet_chk_info->packet;
			while (1) {
				if (tmp->pid == pid) {
					tmp->cnt++;

					if (tmp->pid != 0x1fff) {
						if (cc > tmp->cc) {
							temp = tmp->err;
							tmp->err += ((cc - tmp->cc + 0xf) % 0xf) - 1;
							if (temp != tmp->err) {
								ts_packet_chk_info->total_err += tmp->err - temp;
								pr_info(
									"[INFO][HWDMX] \t(%dmin) pid:0x%04x => cnt:%llu err:%d [%d -> %d]\n",
									ts_packet_chk_info->debug_time, tmp->pid, tmp->cnt, tmp->err,
									tmp->cc, cc);
							}
							tmp->cc = cc;
						} else if (cc < tmp->cc) {
							temp = tmp->err;
							tmp->err += (0x10 - tmp->cc + cc) - 1;
							if (temp != tmp->err) {
								ts_packet_chk_info->total_err += tmp->err - temp;
								pr_info(
									"[INFO][HWDMX] \t(%dmin) pid:0x%04x => cnt:%llu err:%d [%d -> %d]\n",
									ts_packet_chk_info->debug_time, tmp->pid, tmp->cnt, tmp->err,
									tmp->cc, cc);
							}
							tmp->cc = cc;
						}
					}
					break;
				} else {
					tmp = tmp->next;

					if (tmp == NULL) {
						new = 1;
						break;
					}
				}
			}

			if (new) {
				ts_packet_chk_t *tmp_ = NULL;

				tmp = (ts_packet_chk_t *)kmalloc(sizeof(ts_packet_chk_t), GFP_ATOMIC);
				if (tmp == NULL) {
					pr_err("[ERROR][HWDMX] \t ts_packet_chk_t mem alloc err..\n");
				}

				memset(tmp, 0x0, sizeof(ts_packet_chk_t));
				tmp->pid = pid;
				tmp->cc = cc;
				tmp->next = NULL;
				tmp->cnt++;

				ts_packet_chk_info->packet_cnt++;
				tmp_ = ts_packet_chk_info->packet;
				do {
					if (tmp_->next == NULL) {
						tmp_->next = tmp;
						break;
					} else {
						tmp_ = tmp_->next;
					}
				} while (1);

				pr_info(
					"[INFO][HWDMX] \t>>>> create[%d] : 0x%04x / %02d\n",
					ts_packet_chk_info->packet_cnt, tmp->pid, tmp->cc);
			}
		}

		do_gettimeofday(&ts_packet_chk_info->time);
		ts_packet_chk_info->cur_time = ((ts_packet_chk_info->time.tv_sec * 1000) & 0x00ffffff)
			+ (ts_packet_chk_info->time.tv_usec / 1000);
		if ((ts_packet_chk_info->cur_time - ts_packet_chk_info->prev_time)
			> (DEBUG_CHK_TIME * 1000)) {
			// itv_ts_cc_debug(0);
			itv_ts_cc_debug(1);

			ts_packet_chk_info->prev_time = ts_packet_chk_info->cur_time;
			ts_packet_chk_info->debug_time++;
		}
	}
}
#endif //#ifdef TS_PACKET_CHK_MODE

#ifndef USE_REV_MEMORY
static int __maybe_unused rx_dma_buffer_alloc(int devid, struct tea_dma_buf *dma)
{
	dma->buf_size = TSIF_DMA_SIZE;
	dma->v_addr =
		dma_alloc_writecombine(tsif_ex_pri.dev[devid], dma->buf_size, &dma->dma_addr, GFP_KERNEL);
	if (dma->v_addr == NULL)
		return -1;

	dma->buf_sec_size = SECTION_DMA_SIZE;
	dma->v_sec_addr = dma_alloc_writecombine(
		tsif_ex_pri.dev[devid], dma->buf_sec_size, &dma->dma_sec_addr, GFP_KERNEL);
	if (dma->v_sec_addr == NULL) {
		dma_free_writecombine(tsif_ex_pri.dev[devid], dma->buf_size, dma->v_addr, dma->dma_addr);
		return -1;
	}

	pr_info(
		"[INFO][HWDMX] tsif: dma ts buffer alloc @0x%X(Phy=0x%X), size:%d\n",
		(unsigned int)dma->v_addr, (unsigned int)dma->dma_addr, dma->buf_size);
	pr_info(
		"[INFO][HWDMX] tsif: dma sec buffer alloc @0x%X(Phy=0x%X), size:%d\n",
		(unsigned int)dma->v_sec_addr, (unsigned int)dma->dma_sec_addr, dma->buf_sec_size);

	return 0;
}

static int __maybe_unused rx_dma_buffer_free(int devid, struct tea_dma_buf *dma)
{
	pr_info(
		"[INFO][HWDMX] tsif: dma ts buffer free @0x%X(Phy=0x%X), size:%d\n",
		(unsigned int)dma->v_addr, (unsigned int)dma->dma_addr, dma->buf_size);
	pr_info(
		"[INFO][HWDMX] tsif: dma sec buffer free @0x%X(Phy=0x%X), size:%d\n",
		(unsigned int)dma->v_sec_addr, (unsigned int)dma->dma_sec_addr, dma->buf_sec_size);

	if (dma->v_addr)
		dma_free_writecombine(tsif_ex_pri.dev[devid], dma->buf_size, dma->v_addr, dma->dma_addr);

	if (dma->v_sec_addr)
		dma_free_writecombine(
			tsif_ex_pri.dev[devid], dma->buf_sec_size, dma->v_sec_addr, dma->dma_sec_addr);

	return 0;
}
#endif

static int rx_dma_alloc_buffer(int devid)
{
	int __maybe_unused i, __maybe_unused os;
	int size;

	tsif_ex_pri.static_dma_buffer[devid] = kmalloc(sizeof(struct tea_dma_buf), GFP_KERNEL);
	if (tsif_ex_pri.static_dma_buffer[devid] == NULL) {
		dev_err(
			tsif_ex_pri.dev[devid], "[ERROR][HWDMX] %s:%d kmalloc failed\n", __FUNCTION__,
			__LINE__);
		return -ENOMEM;
	}

#ifdef USE_REV_MEMORY
	if (tsif_ex_pri.mem_base == NULL) {
		return -ENOMEM;
	}

	os = SECTION_DMA_SIZE;

	size = (tsif_ex_pri.pmap_tsif.size - os) / HWDMX_NUM;
	for (i = 0; i < devid; i++) {
		os += size;
	}

	if (os + size > tsif_ex_pri.pmap_tsif.size) {
		dev_err(
			tsif_ex_pri.dev[devid], "[ERROR][HWDMX] %s:%d dma buffer is not enough\n", __FUNCTION__,
			__LINE__);
		return -ENOMEM;
	}

	/* ts buffer */
	tsif_ex_pri.static_dma_buffer[devid]->buf_size = (size / 188)*188;
	tsif_ex_pri.static_dma_buffer[devid]->v_addr = tsif_ex_pri.mem_base + os;
	tsif_ex_pri.static_dma_buffer[devid]->dma_addr = tsif_ex_pri.pmap_tsif.base + os;

	/* sec buffer: all demux share one buffer */
	tsif_ex_pri.static_dma_buffer[devid]->buf_sec_size = SECTION_DMA_SIZE;
	tsif_ex_pri.static_dma_buffer[devid]->v_sec_addr = tsif_ex_pri.mem_base;
	tsif_ex_pri.static_dma_buffer[devid]->dma_sec_addr = tsif_ex_pri.pmap_tsif.base;
#else
	if (rx_dma_buffer_alloc(devid, tsif_ex_pri.static_dma_buffer[devid]) != 0) {
		dev_err(
			tsif_ex_pri.dev[devid], "%s:%d dma alloc(%d) failed\n", __FUNCTION__, __LINE__, devid);
		kfree(tsif_ex_pri.static_dma_buffer[devid]);
		tsif_ex_pri.static_dma_buffer[devid] = NULL;
		return -ENOMEM;
	}
#endif /* USE_REV_MEMORY */

	return 0;
}

static int rx_dma_free_buffer(int devid)
{
	int result = 0;

	if (tsif_ex_pri.static_dma_buffer[devid] == NULL) {
		result = -EFAULT;
		goto out;
	}

#if !defined(USE_REV_MEMORY)
	rx_dma_buffer_free(devid, tsif_ex_pri.static_dma_buffer[devid]);
#endif
	kfree(tsif_ex_pri.static_dma_buffer[devid]);
	tsif_ex_pri.static_dma_buffer[devid] = NULL;

out:
	return result;
}

static int
rx_update_stc(struct tcc_hwdmx_tsif_rx_handle *demux, char *p1, int p1_size, char *p2, int p2_size)
{
	int i;
	int dmxid = demux->tsif_ex_handle.dmx_id;

	for (i = 1; i < MAX_PCR_CNT; i++) {
		if (0 < demux->pcr_pid[i] && demux->pcr_pid[i] < 0x1FFFF) {
			if (p1)
				TSDEMUX_MakeSTC(
					(unsigned char *)p1, p1_size, demux->pcr_pid[i], i + dmxid * MAX_PCR_CNT);
			if (p2)
				TSDEMUX_MakeSTC(
					(unsigned char *)p2, p2_size, demux->pcr_pid[i], i + dmxid * MAX_PCR_CNT);
		}
	}
	return 0;
}

static int rx_parse_packet(
	struct tcc_hwdmx_tsif_rx_handle *demux, char *pBuffer, int updated_buff_offset,
	int end_buff_offset)
{
	int ret = 0;
	char *p1 = NULL, *p2 = NULL;
	int p1_size = 0, p2_size = 0;
	int packet_th = demux->ts_demux_feed_handle.call_decoder_index;
	if (++demux->ts_demux_feed_handle.index >= packet_th) {
		if (updated_buff_offset > demux->write_buff_offset) {
			p1 = (char *)(pBuffer + demux->write_buff_offset);
			p1_size = updated_buff_offset - demux->write_buff_offset;
		} else if (end_buff_offset == demux->write_buff_offset) {
			p1 = (char *)pBuffer;
			p1_size = updated_buff_offset;
		} else {
			p1 = (char *)(pBuffer + demux->write_buff_offset);
			p1_size = end_buff_offset - demux->write_buff_offset;

			p2 = (char *)pBuffer;
			p2_size = updated_buff_offset;
		}

		if (p1 == NULL || p1_size < 188)
			return 0;

		rx_update_stc(demux, p1, p1_size, p2, p2_size);

		if (demux->ts_demux_feed_handle.ts_demux_decoder(
				p1, p1_size, p2, p2_size, demux->tsif_ex_handle.dmx_id)
			== 0) {
			demux->write_buff_offset = updated_buff_offset;
			demux->ts_demux_feed_handle.index = 0;
		}

#ifdef TS_PACKET_CHK_MODE
		{
			int i;
			if (p1 && p1_size) {
				for (i = 0; i < p1_size; i += 188)
					itv_ts_cc_check(p1 + i);
			}

			if (p2 && p2_size) {
				for (i = 0; i < p2_size; i += 188)
					itv_ts_cc_check(p2 + i);
			}
		}
#endif
	}
	return ret;
}

static int rx_updated_callback(
	unsigned int dmxid, unsigned int ftype, unsigned int fid, unsigned int value1,
	unsigned int value2, unsigned int bErrCRC)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = &tsif_ex_pri.demux[dmxid];
	unsigned int uiSTC;
	int ret = 0;

	switch (ftype) {
	case 0: // HW_DEMUX_SECTION
	{
#if defined(DEMUX_SYSFS)
		tsif_ex_pri.stat[dmxid].secevt++;
#endif
		if (demux->ts_demux_feed_handle.is_active != 0 && fid < 0xFF) {
			// printk("0x%x, 0x%x, 0x%x\n", demux->tsif_ex_handle.dma_buffer->v_sec_addr, value1,
			// demux->tsif_ex_handle.dma_buffer->dma_sec_addr);
			ret = demux->ts_demux_feed_handle.ts_demux_decoder(
				(char *)fid, bErrCRC, demux->tsif_ex_handle.dma_buffer->v_sec_addr + value1,
				value2 - value1, demux->tsif_ex_handle.dmx_id);
		}
		break;
	}
	case 1: // HW_DEMUX_TS
	{
#if defined(DEMUX_SYSFS)
		tsif_ex_pri.stat[dmxid].tsevt++;
#endif
		if (demux->ts_demux_feed_handle.is_active != 0) {
			ret = rx_parse_packet(demux, demux->tsif_ex_handle.dma_buffer->v_addr, value1, value2);
		}
		break;
	}
	case 2: // HW_DEMUX_PES
	{
		break;
	}
	case 3: // HW_DEMUX_PCR
	{
#if defined(DEMUX_SYSFS)
		tsif_ex_pri.stat[dmxid].pcrevt++;
#endif
		// bErrCRC is error(ms) for PCR & STC
		// printk("%s: STC Error (%d)ms \n", __func__, bErrCRC);
		uiSTC = (unsigned int)value2 & 0x80000000;
		uiSTC |= (unsigned int)value1 >> 1;
		TSDEMUX_UpdatePCR(uiSTC / 45, dmxid * MAX_PCR_CNT);
		break;
	}
	default: {
		pr_err("[ERROR][HWDMX] Invalid parameter: Filter Type : %d\n", ftype);
		break;
	}
	}

	return ret;
}

#if defined(DEMUX_SYSFS)
static ssize_t tcc_hwdmx_tsif_sysfs_show(
		struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;

	for (i = 0; i < HWDMX_NUM; i++) {
		count += sprintf(&buf[count], "%10d ", tsif_ex_pri.demux[i].state);
	}
	count += sprintf(&buf[count], "\n");

	for (i = 0; i < HWDMX_NUM; i++) {
		count += sprintf(&buf[count], "%10d ", atomic_read(&tsif_ex_pri.stat[i].pidnum));
	}
	count += sprintf(&buf[count], "\n");

	for (i = 0; i < HWDMX_NUM; i++) {
		count += sprintf(&buf[count], "%10d ", atomic_read(&tsif_ex_pri.stat[i].secnum));
	}
	count += sprintf(&buf[count], "\n");

	for (i = 0; i < HWDMX_NUM; i++) {
		count += sprintf(&buf[count], "%10lu ", tsif_ex_pri.stat[i].tsevt);
	}
	count += sprintf(&buf[count], "\n");

	for (i = 0; i < HWDMX_NUM; i++) {
		count += sprintf(&buf[count], "%10lu ", tsif_ex_pri.stat[i].secevt);
	}
	count += sprintf(&buf[count], "\n");

	for (i = 0; i < HWDMX_NUM; i++) {
		count += sprintf(&buf[count], "%10lu ", tsif_ex_pri.stat[i].pcrevt);
	}
	count += sprintf(&buf[count], "\n");

	return count;
}

static ssize_t tcc_hwdmx_tsif_sysfs_store(
		struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	return 0;
}

static struct kobj_attribute sc_attrb =
	__ATTR(stat, S_IWUSR|S_IRUGO, tcc_hwdmx_tsif_sysfs_show, tcc_hwdmx_tsif_sysfs_store);
#endif

struct tcc_hwdmx_tsif_rx_handle *tcc_hwdmx_tsif_rx_start(unsigned int devid)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = &tsif_ex_pri.demux[devid];
	struct tea_dma_buf *dma_buffer;
	struct pinctrl *pinctrl = NULL;

	mutex_lock(&(tsif_ex_pri.mutex));

	if (demux->state != 0) {
		mutex_unlock(&(tsif_ex_pri.mutex));
		return NULL;
	}

	if (devid >= HWDMX_NUM) {
		mutex_unlock(&(tsif_ex_pri.mutex));
		return NULL;
	}

	memset(&demux->tsif_ex_handle, 0, sizeof(struct tcc_tsif_handle));
	memcpy(
		(void *)&demux->tsif_ex_handle.port_cfg, (void *)&tsif_ex_pri.port_cfg[devid],
		sizeof(struct tca_tsif_port_config));

	demux->tsif_ex_handle.dmx_id = devid;

	dma_buffer = tsif_ex_pri.static_dma_buffer[devid];
	if (dma_buffer == NULL) {
		mutex_unlock(&(tsif_ex_pri.mutex));
		return NULL;
	}

	pr_info(
		"[INFO][HWDMX] [TSIF-%d] dma ts buffer alloc @0x%X(Phy=0x%X), size:%d\n", devid,
		(unsigned int)dma_buffer->v_addr, (unsigned int)dma_buffer->dma_addr, dma_buffer->buf_size);
	pr_info(
		"[INFO][HWDMX] [TSIF-%d] dma sec buffer alloc @0x%X(Phy=0x%X), size:%d\n", devid,
		(unsigned int)dma_buffer->v_sec_addr, (unsigned int)dma_buffer->dma_sec_addr,
		dma_buffer->buf_sec_size);
	demux->tsif_ex_handle.dma_buffer = dma_buffer;
	demux->tsif_ex_handle.dma_mode = 1; // mpeg2ts mode
	demux->tsif_ex_handle.working_mode = 1;

	if (tsif_ex_pri.dev[demux->tsif_ex_handle.dmx_id] != NULL) {
		pinctrl = pinctrl_get_select(tsif_ex_pri.dev[demux->tsif_ex_handle.dmx_id], "active");
	}
#if 0 // don't care
	if(IS_ERR(pinctrl))
		pr_info("[INFO][HWDMX] %s : pinctrl active error[0x%p]\n", __func__, pinctrl);
#endif

	pr_info(
		"[INFO][HWDMX] %s-id[%d]:port[%d]pinctrl[0x%p]\n", __func__, demux->tsif_ex_handle.dmx_id,
		demux->tsif_ex_handle.port_cfg.tsif_port, pinctrl);

	if (hwdmx_start_cmd(&demux->tsif_ex_handle)) {
		pr_err("[ERROR][HWDMX] %s: hwdmx_init error !!!!!\n", __func__);

#if !defined(USE_REV_MEMORY)
		if (dma_buffer != NULL) {
			rx_dma_buffer_free(devid, dma_buffer);
			demux->tsif_ex_handle.dma_buffer = NULL;
		}
#endif
		hwdmx_stop_cmd(&demux->tsif_ex_handle);
		mutex_unlock(&(tsif_ex_pri.mutex));
		return NULL;
	}

	demux->write_buff_offset = 0;
	demux->read_buff_offset = 0;
	demux->end_buff_offset = 0;
	demux->loop_count = 0;

	hwdmx_buffer_updated_callback(&demux->tsif_ex_handle, rx_updated_callback);

	demux->state = 1;

	init_waitqueue_head(&(demux->wait_q));

	tsif_ex_pri.open_cnt++;

	mutex_unlock(&(tsif_ex_pri.mutex));

#ifdef TS_PACKET_CHK_MODE
	if (tsif_ex_pri.open_cnt == 1) {
		ts_packet_chk_info =
			(ts_packet_chk_info_t *)kmalloc(sizeof(ts_packet_chk_info_t), GFP_ATOMIC);
		if (ts_packet_chk_info == NULL) {
			pr_err("[ERROR][HWDMX] \t ts_packet_chk_info_t mem alloc err..\n");
		}
		memset(ts_packet_chk_info, 0x0, sizeof(ts_packet_chk_info_t));
	}
#endif

	return demux;
}

int tcc_hwdmx_tsif_rx_stop(struct tcc_hwdmx_tsif_rx_handle *demux, unsigned int devid)
{
	struct tea_dma_buf *dma_buffer;
	struct pinctrl *pinctrl;

	mutex_lock(&(tsif_ex_pri.mutex));
	if (tsif_ex_pri.open_cnt > 0) {
		if (demux->state == 1) {
			tsif_ex_pri.open_cnt--;

			demux->state = 0;

			dma_buffer = demux->tsif_ex_handle.dma_buffer;

			if (demux->mem)
				vfree(demux->mem);
			demux->mem = NULL;

			demux->write_buff_offset = 0;
			demux->read_buff_offset = 0;
			demux->end_buff_offset = 0;
			demux->loop_count = 0;

			if (tsif_ex_pri.dev[demux->tsif_ex_handle.dmx_id] != NULL) {
				pinctrl = pinctrl_get_select(tsif_ex_pri.dev[demux->tsif_ex_handle.dmx_id], "idle");
			}
#if 0 // don't care
			if(IS_ERR(pinctrl))
				printk("%s : pinctrl idle error[0x%p]\n", __func__, pinctrl);
#endif

			hwdmx_stop_cmd(&demux->tsif_ex_handle);

#if !defined(USE_REV_MEMORY)
			if (dma_buffer != NULL) {
				rx_dma_buffer_free(devid, dma_buffer);
				demux->tsif_ex_handle.dma_buffer = NULL;
			}
#endif /* USE_REV_MEMORY */
		}
	}
	mutex_unlock(&(tsif_ex_pri.mutex));
#ifdef TS_PACKET_CHK_MODE
	if (tsif_ex_pri.open_cnt == 0) {
		ts_packet_chk_t *tmp = NULL;
		ts_packet_chk_t *tmp_ = NULL;

		if (ts_packet_chk_info != NULL) {
			if (ts_packet_chk_info->packet != NULL) {
				itv_ts_cc_debug(1);

				tmp = ts_packet_chk_info->packet;
				do {
					tmp_ = tmp;
					tmp = tmp->next;
					kfree(tmp_);
				} while (tmp != NULL);
			}
			kfree(ts_packet_chk_info);
			ts_packet_chk_info = NULL;
		}
	}
#endif
	return 0;
}

int __maybe_unused
tcc_hwdmx_tsif_rx_buffer_flush(struct tcc_hwdmx_tsif_rx_handle *demux, unsigned long addr, int len)
{
	unsigned long offset;

#ifdef USE_REV_MEMORY
	return 0;
#endif

	if ((unsigned long)demux->tsif_ex_handle.dma_buffer->v_addr <= addr) {
		offset = addr - (unsigned long)demux->tsif_ex_handle.dma_buffer->v_addr;
		if (offset < demux->tsif_ex_handle.dma_buffer->buf_size) {
			addr = demux->tsif_ex_handle.dma_buffer->dma_addr;
			dma_sync_single_for_cpu(0, addr + offset, len, DMA_FROM_DEVICE);
			return 0;
		}
	}
	if ((unsigned long)demux->tsif_ex_handle.dma_buffer->v_sec_addr <= addr) {
		offset = addr - (unsigned long)demux->tsif_ex_handle.dma_buffer->v_sec_addr;
		if (offset < demux->tsif_ex_handle.dma_buffer->buf_sec_size) {
			addr = demux->tsif_ex_handle.dma_buffer->dma_sec_addr;
			dma_sync_single_for_cpu(0, addr + offset, len, DMA_FROM_DEVICE);
			return 0;
		}
	}
	return -1;
}

int tcc_hwdmx_tsif_rx_set_external_tsdemux(
	struct tcc_hwdmx_tsif_rx_handle *demux,
	int (*decoder)(char *p1, int p1_size, char *p2, int p2_size, int devid))
{
	if (decoder == 0) {
		// turn off external decoding
		// memset(&demux->ts_demux_feed_handle, 0x0, sizeof(struct ts_demux_feed_struct));
		demux->ts_demux_feed_handle.is_active = 0;
		return 0;
	}
	if (demux->ts_demux_feed_handle.call_decoder_index != 1
		|| demux->ts_demux_feed_handle.is_active == 0) {
		demux->ts_demux_feed_handle.ts_demux_decoder = decoder;
		demux->ts_demux_feed_handle.index = 0;
		// every max_dec_packet calling isr, call decoder
		demux->ts_demux_feed_handle.call_decoder_index = 1;
		demux->ts_demux_feed_handle.is_active = 1;
		// printk("%s::%d::max_dec_packet[%d]int_packet[%d]\n", __func__, __LINE__,
		// demux->ts_demux_feed_handle.call_decoder_index,
		// demux->tsif_ex_handle.dma_intr_packet_cnt);
	}
	return 0;
}

int tcc_hwdmx_tsif_rx_add_pid(
	struct tcc_hwdmx_tsif_rx_handle *demux, struct tcc_hwdmx_tsif_rx_filter_param *param)
{
#if defined(DEMUX_SYSFS)
	int dmxid = demux->tsif_ex_handle.dmx_id;
	if (param->f_type == FILTER_TYPE_SECTION) {
		atomic_inc(&tsif_ex_pri.stat[dmxid].secnum);
	}
	else { // FILTER_TYPE_TS
		atomic_inc(&tsif_ex_pri.stat[dmxid].pidnum);
	}
#endif
	return hwdmx_add_filter_cmd(&demux->tsif_ex_handle, (struct tcc_tsif_filter *)param);
}

int tcc_hwdmx_tsif_rx_remove_pid(
	struct tcc_hwdmx_tsif_rx_handle *demux, struct tcc_hwdmx_tsif_rx_filter_param *param)
{
#if defined(DEMUX_SYSFS)
	int dmxid = demux->tsif_ex_handle.dmx_id;
	if (param->f_type == FILTER_TYPE_SECTION) {
		atomic_dec(&tsif_ex_pri.stat[dmxid].secnum);
	}
	else { // FILTER_TYPE_TS
		atomic_dec(&tsif_ex_pri.stat[dmxid].pidnum);
	}
#endif
	return hwdmx_remove_filter_cmd(&demux->tsif_ex_handle, (struct tcc_tsif_filter *)param);
}

int tcc_hwdmx_tsif_rx_set_pcr_pid(
	struct tcc_hwdmx_tsif_rx_handle *demux, unsigned int index, unsigned int pcr_pid)
{
	int dmxid = demux->tsif_ex_handle.dmx_id;

	if (index >= MAX_PCR_CNT)
		return -EFAULT;

	if (pcr_pid < 0x1FFF) {
		TSDEMUX_Open(index + dmxid * MAX_PCR_CNT);
	}
	demux->pcr_pid[index] = pcr_pid;
	if (index == 0) {
		return hwdmx_set_pcrpid_cmd(&demux->tsif_ex_handle, demux->pcr_pid[index]);
	}
	return 0;
}

int tcc_hwdmx_tsif_rx_get_stc(struct tcc_hwdmx_tsif_rx_handle *demux, unsigned int index, u64 *stc)
{
	int dmxid = demux->tsif_ex_handle.dmx_id;
	unsigned long flags = 0;

	index += (dmxid * MAX_PCR_CNT);

	spin_lock_irqsave(&timer_lock, flags);
	*stc = (u64)TSDEMUX_GetSTC(index);
	spin_unlock_irqrestore(&timer_lock, flags);

	return 0;
}

int tcc_hwdmx_tsif_rx_set_cipher_dec_pid(struct tcc_hwdmx_tsif_rx_handle *demux, unsigned int numOfPids,
				unsigned int delete_option, unsigned short *pids)
{
	hwdmx_set_cipher_dec_pid_cmd(&demux->tsif_ex_handle, numOfPids, delete_option, pids);
	return 0;
}

int tcc_hwdmx_tsif_rx_set_mode(struct tcc_hwdmx_tsif_rx_handle *demux, int algo, int opmode,
	int residual, int smsg, unsigned int numOfPids, unsigned short *pids)
{
	hwdmx_set_algo_cmd(&demux->tsif_ex_handle, algo, opmode, residual, smsg, numOfPids, pids);
	return 0;
}

int tcc_hwdmx_tsif_rx_set_key(struct tcc_hwdmx_tsif_rx_handle *demux,
				int keytype, int keymode, int size, void *key)
{
	hwdmx_set_key_cmd(&demux->tsif_ex_handle, keytype, keymode, size, key);
	return 0;
}

int tcc_hwdmx_tsif_rx_register(int devid, struct device *dev)
{
#ifdef CONFIG_OF
	if (dev != NULL) {
		of_property_read_u32(dev->of_node, "tsif-port", &tsif_ex_pri.port_cfg[devid].tsif_port);
		pr_info("[INFO][HWDMX] tsif_port: %d\n", tsif_ex_pri.port_cfg[devid].tsif_port);
	} else {
		if(tsif_ex_pri.port_cfg[devid].tsif_port == 0xF) {
			tsif_ex_pri.port_cfg[devid].tsif_port = 1;
		}
	}
#endif
	tsif_ex_pri.port_cfg[devid].tsif_id = devid;
	tsif_ex_pri.dev[devid] = dev;
	hwdmx_tsif_num++;

	if (rx_dma_alloc_buffer(devid) != 0) {
		dev_err(dev, "[ERROR][HWDMX] %s:%d rx_dma_alloc buffer failed\n", __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

int tcc_hwdmx_tsif_rx_unregister(int devid)
{
	int ret = 0;
	ret = rx_dma_free_buffer(devid);
	hwdmx_tsif_num--;

	return ret;
}

int tcc_hwdmx_tsif_rx_init(struct device *dev)
{
    int i;
	int ret = 0;

	memset(&tsif_ex_pri, 0, sizeof(struct tcc_tsif_pri_handle));
	for(i=0; i<HWDMX_NUM; i++)
	{
		tsif_ex_pri.port_cfg[i].tsif_port = 0xF;
	}

	mutex_init(&(tsif_ex_pri.mutex));

#ifdef USE_REV_MEMORY
	/* This function returns 1 on success, and 0 on failure. Weird function design. */
	if (pmap_get_info("tsif", &tsif_ex_pri.pmap_tsif) != 1) {
		ret = -EFAULT;
		goto out;
	}

	tsif_ex_pri.mem_base = ioremap_nocache(tsif_ex_pri.pmap_tsif.base, tsif_ex_pri.pmap_tsif.size);
	if (tsif_ex_pri.mem_base == NULL) {
		ret = -EFAULT;
		goto out;
	}
	pr_info(
		"[INFO][HWDMX] tsif_ex: pmap(PA: 0x%08x, VA: 0x%p, SIZE: 0x%x)\n",
		tsif_ex_pri.pmap_tsif.base, tsif_ex_pri.mem_base, tsif_ex_pri.pmap_tsif.size);
#endif /* USE_REV_MEMORY */

#if defined(DEMUX_SYSFS)
	ret = sysfs_create_file(&dev->kobj, &sc_attrb.attr);
#endif

out:
	return ret;
}

int tcc_hwdmx_tsif_rx_deinit(struct device *dev)
{
	int ret = 0;

#if defined(DEMUX_SYSFS)
	sysfs_remove_file(&dev->kobj, &sc_attrb.attr);
#endif

#ifdef USE_REV_MEMORY
	if (tsif_ex_pri.mem_base == NULL) {
		ret = -EFAULT;
	}
	iounmap(tsif_ex_pri.mem_base);
#endif /* USE_REV_MEMORY */

	mutex_destroy(&(tsif_ex_pri.mutex));

	return ret;
}
