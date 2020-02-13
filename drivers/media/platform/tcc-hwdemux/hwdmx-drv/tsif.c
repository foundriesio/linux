/*
 *  tcc_tsif.c
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

#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <dvb_demux.h>
#include <dmxdev.h>

#include "demux.h"
#include "tsif.h"

/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG "[TCC_TSIF]"

static int tsif_debug = 0;

module_param(tsif_debug, int, 0644);
MODULE_PARM_DESC(tsif_debug, "Turn on/off tsif debugging (default:off).");

#define dprintk(msg...)                          \
	{                                            \
		if (tsif_debug)                          \
			printk(KERN_INFO LOG_TAG "(D)" msg); \
	}

#define eprintk(msg...)                        \
	{                                          \
		printk(KERN_INFO LOG_TAG " (E) " msg); \
	}

/*****************************************************************************
 * Defines
 ******************************************************************************/
//#define	USE_TASKLET_BUFFER_HANDLING
#define HWDMX_SECFILTER_ENABLE
#ifdef HWDMX_SECFILTER_ENABLE
#define HWDMX_SECFILTER_MAX 16
#define SECFILTER_TO_TSFILTER
#endif

#if CONFIG_DVB_MAX_ADAPTERS
#define MAX_INST CONFIG_DVB_MAX_ADAPTERS
#else
#define MAX_INST 8
#endif

/*****************************************************************************
 * structures
 ******************************************************************************/

/*****************************************************************************
 * Variables
 ******************************************************************************/
static tcc_tsif_inst_t *gInst[] = {[0 ...(MAX_INST - 1)] = NULL};

/*****************************************************************************
 * Functions
 ******************************************************************************/
#ifdef	USE_TASKLET_BUFFER_HANDLING
static void tcc_tsif_running_feed(long unsigned int id)
{
	tcc_tsif_priv_t *pHandle;
	tcc_tsif_tasklet_t *pTask;
	unsigned int devid = id & 0xffff;
	unsigned int type = id >> 16;
	char *p1, *p2;
	int p1_size, p2_size;
	int index;
	unsigned long flags;
	struct packet_info *p_info;

	if (gInst[devid] == NULL)
		return;

	pHandle = &gInst[devid]->tsif[0];
	pTask = &pHandle->task[type];

	spin_lock_irqsave(&pTask->ts_packet_list.lock, flags);
	for (index = 0; index < TS_PACKET_LIST; index++) {
		if (list_empty(&pTask->ts_packet_bank))
			break;
		p_info = list_first_entry(&pTask->ts_packet_bank, struct packet_info, list);
		if (p_info == NULL) {
			eprintk("[ERROR][HWDMX] %s(%d):invalid NULL packet !!!\n", __func__, type);
			break;
		}
		list_del(&p_info->list);

		if (p_info->valid != 1) {
			eprintk("[ERROR][HWDMX] %s(%d):invalid packet !!!\n", __func__, type);
			p_info->valid = 0;
			continue;
		}
		p1 = p_info->p1;
		p1_size = p_info->p1_size;
		p2 = p_info->p2;
		p2_size = p_info->p2_size;
		p_info->valid = 0;

		if (type == TSIF_FILTER_TYPE_SECTION) {
			if (p2_size > 0) {
				hwdmx_buffer_flush(&pHandle->demux, (unsigned long)p2, p2_size);
			}
			tcc_dmx_sec_callback((unsigned int)p1, p1_size, p2, p2_size, devid);
		} else {
			if (p1_size > 0) {
				hwdmx_buffer_flush(&pHandle->demux, (unsigned long)p1, p1_size);
			}
			if (p2_size > 0) {
				hwdmx_buffer_flush(&pHandle->demux, (unsigned long)p2, p2_size);
			}
			tcc_dmx_ts_callback(p1, p1_size, p2, p2_size, devid);
		}
	}
	spin_unlock_irqrestore(&pTask->ts_packet_list.lock, flags);
}

static int tcc_tsif_parse_packet(char *p1, int p1_size, char *p2, int p2_size, int devid)
{
	tcc_tsif_priv_t *pHandle;
	tcc_tsif_tasklet_t *pTask;
	int index, type = (p1_size < 188) ? TSIF_FILTER_TYPE_SECTION : TSIF_FILTER_TYPE_TS;
	struct packet_info *p_info;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];
	pTask = &pHandle->task[type];

	index = pTask->ts_packet_list.current_index;
	p_info = &pTask->ts_packet_list.ts_packet[index];

	if (p_info->valid == 0) {
		p_info->p1 = p1;
		p_info->p1_size = p1_size;

		p_info->p2 = p2;
		p_info->p2_size = p2_size;

		p_info->valid = 1;

		list_add_tail(&p_info->list, &pTask->ts_packet_bank);
		if (++index >= TS_PACKET_LIST)
			index = 0;
		pTask->ts_packet_list.current_index = index;
		tasklet_schedule(&pTask->tsif_tasklet);
	} else {
		eprintk("[ERROR][HWDMX] %s(%d):no space in packet_bank !!![%d]\n", __func__, type, index);
		pTask->ts_packet_list.current_index = 0;
		memset(pTask->ts_packet_list.ts_packet, 0, sizeof(struct packet_info) * TS_PACKET_LIST);
		INIT_LIST_HEAD(&pTask->ts_packet_bank);
	}
	return 0;
}
#else
static int tcc_tsif_parse_packet(char *p1, int p1_size, char *p2, int p2_size, int devid)
{
    tcc_tsif_priv_t *pHandle;
	int type = (p1_size < 188) ? TSIF_FILTER_TYPE_SECTION : TSIF_FILTER_TYPE_TS;
    pHandle = &gInst[devid]->tsif[0];
	if (type == TSIF_FILTER_TYPE_SECTION) {
		if (p2_size > 0) {
			hwdmx_buffer_flush(&pHandle->demux, (unsigned long)p2, p2_size);
		}
		tcc_dmx_sec_callback((unsigned int)p1, p1_size, p2, p2_size, devid);
	} else {
		if (p1_size > 0) {
			hwdmx_buffer_flush(&pHandle->demux, (unsigned long)p1, p1_size);
		}
		if (p2_size > 0) {
			hwdmx_buffer_flush(&pHandle->demux, (unsigned long)p2, p2_size);
		}
		tcc_dmx_ts_callback(p1, p1_size, p2, p2_size, devid);
	}
	return 0;
}
#endif


int tcc_tsif_init(tcc_tsif_inst_t *inst)
{
	struct pinctrl *pinctrl;
	int i;

	inst->tsif = kzalloc(inst->dev_num * sizeof(tcc_tsif_priv_t), GFP_KERNEL);

	if (hwdmx_register(inst->adapter->num, inst->dev) != 0) {
		eprintk("[ERROR][HWDMX] %s:%d hwdmx_register fails\n", __FUNCTION__, __LINE__);
		kfree(inst->tsif);
		return -EFAULT;
	}

	pinctrl = pinctrl_get_select(inst->dev, "active");
	if(IS_ERR(pinctrl))
		pr_info("[INFO][HWDMX] %s : pinctrl active error[0x%p]\n", __func__, pinctrl);

	for (i = 0; i < inst->dev_num; i++) {
		mutex_init(&inst->tsif[i].mutex);
	}

	gInst[inst->adapter->num] = inst;
	dprintk("%s\n", __FUNCTION__);

	return 0;
}

int tcc_tsif_deinit(tcc_tsif_inst_t *inst)
{
	int i;

	hwdmx_unregister(inst->adapter->num);

	gInst[inst->adapter->num] = NULL;

	for (i = 0; i < inst->dev_num; i++) {
		mutex_destroy(&inst->tsif[i].mutex);
	}

	kfree(inst->tsif);

	dprintk("%s\n", __FUNCTION__);

	return 0;
}

int tcc_tsif_start(unsigned int devid)
{
	tcc_tsif_priv_t *pHandle;
    #ifdef	USE_TASKLET_BUFFER_HANDLING
	tcc_tsif_tasklet_t *pTask;
    int type;
    #endif
	int err, ret;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	mutex_lock(&pHandle->mutex);
	ret = pHandle->users;
	pHandle->users++;
	if (pHandle->users == 1) {
		err = hwdmx_start(&pHandle->demux, devid);
		if (err != 0) {
			pHandle->users = 0;
			mutex_unlock(&pHandle->mutex);
			eprintk("[ERROR][HWDMX] Failed to open tsif driver[ID=%d, Err=%d]\n", devid, err);
			return err;
		}

		memset(&pHandle->pcr_table, 0xff, sizeof(pcr_table_t));
		memset(&pHandle->ts_table, 0xff, sizeof(ts_table_t));
		memset(&pHandle->sec_table, 0xff, sizeof(sec_table_t));
		pHandle->sec_table.iNum = 0;
#ifdef	USE_TASKLET_BUFFER_HANDLING
		for (type = 0; type < TSIF_FILTER_TYPE_MAX; type++) {
			pTask = &pHandle->task[type];

			memset(&pTask->ts_packet_list, 0x0, sizeof(struct packet_list));
			INIT_LIST_HEAD(&pTask->ts_packet_bank);
			spin_lock_init(&pTask->ts_packet_list.lock);
			tasklet_init(&pTask->tsif_tasklet, tcc_tsif_running_feed, (type << 16) | devid);
		}
#endif        
		hwdmx_set_external_tsdemux(&pHandle->demux, tcc_tsif_parse_packet);
	}

	mutex_unlock(&pHandle->mutex);

	dprintk("%s(ret = %d)\n", __FUNCTION__, ret);

	return ret;
}

int tcc_tsif_stop(unsigned int devid)
{
	tcc_tsif_priv_t *pHandle;
	int err, ret, type;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users == 0)
		return -1;

	mutex_lock(&pHandle->mutex);
	pHandle->users--;
	if (pHandle->users == 0) {
		hwdmx_set_external_tsdemux(&pHandle->demux, NULL);
		for (type = 0; type < TSIF_FILTER_TYPE_MAX; type++) {
			tasklet_kill(&pHandle->task[type].tsif_tasklet);
		}
		err = hwdmx_stop(&pHandle->demux, devid);
		if (err != 0) {
			pHandle->users = 1;
			mutex_unlock(&pHandle->mutex);

			eprintk("[ERROR][HWDMX] Failed to close tsif driver[ID=%d, Err=%d]\n", devid, err);
			return err;
		}
	}
	ret = pHandle->users;
	mutex_unlock(&pHandle->mutex);

	dprintk("%s(ret = %d)\n", __FUNCTION__, ret);

	return ret;
}

int tcc_tsif_set_pid(struct dvb_demux_feed *feed, unsigned int devid)
{
	tcc_tsif_priv_t *pHandle;
	struct tcc_tsif_filter param;
	struct dvb_demux *demux = feed->demux;
	int i, j = MAX_TS_CNT;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users == 0)
		return -1;

	mutex_lock(&pHandle->mutex);
#ifdef HWDMX_SECFILTER_ENABLE
	switch (feed->type) {
	case DMX_TYPE_TS: {
#endif
		for (i = 0; i < MAX_TS_CNT; i++) {
			if (pHandle->ts_table.iPID[i] == feed->pid) {
				pHandle->ts_table.iUsers[i]++;
				break;
			}
			if (i < j && !(pHandle->ts_table.iPID[i] < PID_NULL)) {
				j = i;
			}
		}
		if (i == MAX_TS_CNT) {
			if (j == MAX_TS_CNT) {
				eprintk("[ERROR][HWDMX] Failed to add filter[pid = %d]\n", feed->pid);
				return -1;
			}

			pHandle->ts_table.iPID[j] = feed->pid;
			pHandle->ts_table.iUsers[j] = 1;

#ifdef SECFILTER_TO_TSFILTER
			for (i = 0; i < MAX_SEC_CNT; i++) // if section filter for same pid, it must be stop.
			{
				if (pHandle->sec_table.iPID[i] == feed->pid && pHandle->sec_table.iUseSW[i] == 0) {
					pHandle->sec_table.iUseSW[i] = 1;

					param.f_type = FILTER_TYPE_SECTION;
					param.f_id = i;
					param.f_pid = feed->pid;
					param.f_size = 0;
					if (hwdmx_remove_pid(&pHandle->demux, &param) != 0) {
						eprintk(
							"[ERROR][HWDMX] %s:%d : pid[%d] remove error\n", __func__, __LINE__,
							feed->pid);
					}
					pHandle->sec_table.iNum--;
				}
			}
#endif
			param.f_type = FILTER_TYPE_TS;
			param.f_id = 0;
			param.f_pid = feed->pid;
			param.f_size = 0;
			if (hwdmx_add_pid(&pHandle->demux, &param) != 0) {
				eprintk(
					"[ERROR][HWDMX] %s:%d : pid[%d] ts setting error\n", __func__, __LINE__,
					feed->pid);
				return -1;
			}
		}
#ifdef HWDMX_SECFILTER_ENABLE
		break;
	}
	case DMX_TYPE_SEC: {
		int useSW = (demux->dmx.frontend->source == DMX_MEMORY_FE) ? 1 : 0;
#ifdef SECFILTER_TO_TSFILTER
		for (i = 0; i < MAX_TS_CNT && useSW == 0; i++) {
			if (pHandle->ts_table.iPID[i]
				== feed->pid) // if ts filter for same pid, use s/w section filter
			{
				pHandle->ts_table.iUsers[i]++;
				useSW = 1;
				break;
			}
		}
		if (useSW == 0) {
			int filterNum = 0;
			for (i = 0; i < demux->filternum; i++) {
				if (demux->filter[i].state != DMX_STATE_READY)
					continue;
				if (demux->filter[i].type != DMX_TYPE_SEC)
					continue;
				if (demux->filter[i].filter.parent != &feed->feed.sec)
					continue;
				filterNum++;
			}
			if (pHandle->sec_table.iNum + filterNum > HWDMX_SECFILTER_MAX) {
				for (i = 0; i < MAX_TS_CNT; i++) {
					if (!(pHandle->ts_table.iPID[i] < PID_NULL)) {
						break;
					}
				}
				if (i == MAX_TS_CNT) {
					eprintk("[ERROR][HWDMX] Failed to add filter[pid = %d]\n", feed->pid);
					return -1;
				}
				pHandle->ts_table.iPID[i] = feed->pid;
				pHandle->ts_table.iUsers[i] = filterNum;

				param.f_type = 1; // HW_DEMUX_TS
				param.f_id = 0;
				param.f_pid = feed->pid;
				param.f_size = 0;
				if (hwdmx_add_pid(&pHandle->demux, &param) != 0) {
					eprintk(
						"[ERROR][HWDMX] %s:%d : pid[%d] ts setting error\n", __func__, __LINE__,
						feed->pid);
					return -1;
				}
				useSW = 1;
			}
		}
#endif
		for (i = 0; i < demux->filternum; i++) {
			if (demux->filter[i].state != DMX_STATE_READY)
				continue;
			if (demux->filter[i].type != DMX_TYPE_SEC)
				continue;
			if (demux->filter[i].filter.parent != &feed->feed.sec)
				continue;
			demux->filter[i].state = DMX_STATE_GO;

			pHandle->sec_table.iPID[demux->filter[i].index] = feed->pid;
			pHandle->sec_table.iUseSW[demux->filter[i].index] = useSW;

			if (useSW == 0) {
				param.f_type = 0; // HW_DEMUX_SECTION
				param.f_id = demux->filter[i].index;
				param.f_pid = feed->pid;
				param.f_comp = demux->filter[i].filter.filter_value;
				param.f_mask = demux->filter[i].filter.filter_mask;
				param.f_mode = demux->filter[i].filter.filter_mode;
				for (j = DMX_MAX_FILTER_SIZE; j > 0; j--) {
					if (param.f_mask[j - 1] != 0x0) {
						break;
					}
				}
				param.f_size = j;
				if (hwdmx_add_pid(&pHandle->demux, &param) != 0) {
					eprintk(
						"[ERROR][HWDMX] %s:%d : pid[%d] section setting error\n", __func__,
						__LINE__, feed->pid);
					return -1;
				}
				pHandle->sec_table.iNum++;
			}
		}
		break;
	}
	case DMX_TYPE_PES:
	default: {
		break;
	}
	}
#endif
	mutex_unlock(&pHandle->mutex);

	dprintk("%s\n", __FUNCTION__);

	return 0;
}

int tcc_tsif_remove_pid(struct dvb_demux_feed *feed, unsigned int devid)
{
	tcc_tsif_priv_t *pHandle;
	struct tcc_tsif_filter param;
	struct dvb_demux *demux = feed->demux;
	int i, j;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users == 0)
		return -1;

	mutex_lock(&pHandle->mutex);
#ifdef HWDMX_SECFILTER_ENABLE
	switch (feed->type) {
	case DMX_TYPE_TS: {
#endif
		for (i = 0; i < MAX_TS_CNT; i++) {
			if (pHandle->ts_table.iPID[i] == feed->pid) {
				pHandle->ts_table.iUsers[i]--;
				break;
			}
		}
		if (i == MAX_TS_CNT) {
			eprintk("[ERROR][HWDMX] Failed to remove filter[pid = %d]\n", feed->pid);
			return -1;
		}
		if (pHandle->ts_table.iUsers[i] == 0) {
			pHandle->ts_table.iPID[i] = PID_NULL;

			param.f_type = TSIF_FILTER_TYPE_TS;
			param.f_id = 0;
			param.f_pid = feed->pid;
			param.f_size = 0;
			if (hwdmx_remove_pid(&pHandle->demux, &param) != 0) {
				if (param.f_pid != 0x2000) {
					eprintk(
						"[ERROR][HWDMX] %s:%d : pid[%d] ts remove error\n", __func__, __LINE__,
						feed->pid);
				}
			}
		}
#ifdef HWDMX_SECFILTER_ENABLE
		break;
	}
	case DMX_TYPE_SEC: {
#ifdef SECFILTER_TO_TSFILTER
		for (j = 0; j < MAX_TS_CNT; j++) {
			if (pHandle->ts_table.iPID[j] == feed->pid) {
				break;
			}
		}
#endif
		for (i = 0; i < demux->filternum; i++) {
			if (demux->filter[i].state == DMX_STATE_GO
				&& demux->filter[i].filter.parent == &feed->feed.sec) {
				demux->filter[i].state = DMX_STATE_READY;
				pHandle->sec_table.iPID[demux->filter[i].index] = PID_NULL;
				if (pHandle->sec_table.iUseSW[demux->filter[i].index] == 0) {
					param.f_type = TSIF_FILTER_TYPE_SECTION;
					param.f_id = demux->filter[i].index;
					param.f_pid = feed->pid;
					param.f_size = 0;
					if (hwdmx_remove_pid(&pHandle->demux, &param) != 0) {
						eprintk(
							"[ERROR][HWDMX] %s:%d : pid[%d] section remove error\n", __func__,
							__LINE__, feed->pid);
						return -1;
					}
					pHandle->sec_table.iNum--;
				}
#ifdef SECFILTER_TO_TSFILTER
				else {
					if (j != MAX_TS_CNT) {
						pHandle->ts_table.iUsers[j]--;
						if (pHandle->ts_table.iUsers[j] == 0) {
							pHandle->ts_table.iPID[j] = PID_NULL;

							param.f_type = TSIF_FILTER_TYPE_TS;
							param.f_id = 0;
							param.f_pid = feed->pid;
							param.f_size = 0;
							if (hwdmx_remove_pid(&pHandle->demux, &param) != 0) {
								if (param.f_pid != 0x2000) {
									eprintk(
										"[ERROR][HWDMX] %s:%d : pid[%d] ts remove error\n",
										__func__, __LINE__, feed->pid);
								}
							}
							j = MAX_TS_CNT;
						}
					}
				}
#endif
			}
		}
		break;
	}
	case DMX_TYPE_PES:
	default: {
		break;
	}
	}
#endif
	mutex_unlock(&pHandle->mutex);

	dprintk("%s\n", __FUNCTION__);

	return 0;
}

int tcc_tsif_set_pcrpid(int on, struct dvb_demux_feed *feed, unsigned int devid)
{
	tcc_tsif_priv_t *pHandle;
	int ret;
	unsigned int index, pcr_pid;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users == 0)
		return -1;

	switch (feed->pes_type) {
	case DMX_PES_PCR0:
		index = 0;
		break;
	case DMX_PES_PCR1:
		index = 1;
		break;
	case DMX_PES_PCR2:
		index = 2;
		break;
	case DMX_PES_PCR3:
		index = 3;
		break;
	default:
		index = 0;
		break;
	}

	if (index >= MAX_PCR_CNT)
		return -1;

	if (on) {
		pHandle->pcr_table.iPID[index] = feed->pid;
		pcr_pid = feed->pid;
		if (index > 0) {
			tcc_tsif_set_pid(feed, devid);
		}
	} else {
		pHandle->pcr_table.iPID[index] = PID_NULL;
		pcr_pid = PID_NULL;
		if (index > 0) {
			tcc_tsif_remove_pid(feed, devid);
		}
	}

	mutex_lock(&pHandle->mutex);

	ret = hwdmx_set_pcr_pid(&pHandle->demux, index, pcr_pid);

	mutex_unlock(&pHandle->mutex);

	dprintk("[DEBUG][HWDMX] %s\n", __FUNCTION__);

	return ret;
}

int tcc_tsif_get_stc(unsigned int index, unsigned int devid, u64 *stc)
{
	tcc_tsif_priv_t *pHandle;
	int ret = -1;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users == 0)
		return -1;

	mutex_lock(&pHandle->mutex);

	ret = hwdmx_get_stc(&pHandle->demux, index, stc);

	mutex_unlock(&pHandle->mutex);

	return ret;
}

int tcc_tsif_can_write(unsigned int devid)
{
	tcc_tsif_priv_t *pHandle;

	if (gInst[devid] == NULL) {
		return 0;
	}

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users > 0) {
		return tcc_dmx_can_write(devid);
	}
	return 1;
}
