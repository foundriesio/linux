/*
 * Copyright (C) Telechips, Inc.
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
#include <linux/module.h>
#include <linux/spi/tcc_tsif.h>
#include <linux/dma-mapping.h>


#include "../tcc-demux-core/tca_hwdemux_tsif.h"
#include "dvb_demux.h"
#include "dmxdev.h"
#include "tcc_dmx.h"
#include "tcc_tsif_interface.h"

#define SECFILTER_TO_TSFILTER

/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG    "[TCC_TSIF_INTERFACE]"

static int tsif_debug = 0;

module_param(tsif_debug, int, 0644);
MODULE_PARM_DESC(tsif_debug, "Turn on/off tsif debugging (default:off).");
/*****************************************************************************
 * Defines
 ******************************************************************************/
#define MAX_INST 8
#define	TSIF_PACKETSIZE             188
#define TSIF_INT_PACKETCOUNT        1       //39	//int packet 13*3
#define TSIF_MAX_PACKETCOUNT        8190    //max packet 0x1fff-1 = 8190 = 2*3*3*5*7*13

/*****************************************************************************
 * structures
 ******************************************************************************/


/*****************************************************************************
 * Variables
 ******************************************************************************/
static tcc_tsif_inst_t *gInst[] = {[0 ... (MAX_INST - 1)] = NULL};


/*****************************************************************************
 * External Functions
 ******************************************************************************/
extern int tcc_hwdmx_tsif_open(struct inode *inode, struct file *filp);
extern int tcc_hwdmx_tsif_release(struct inode *inode, struct file *filp);
extern int tcc_hwdmx_tsif_ioctl_ex(struct file *filp, unsigned int cmd, unsigned long arg);
extern int tcc_hwdmx_tsif_set_external_tsdemux(struct file *filp, int (*decoder)(char *p1, int p1_size, char *p2, int p2_size, int devid), int max_dec_packet, int devid);
extern int tcc_hwdmx_tsif_buffer_flush(struct file *filp, unsigned long x, int size);



/*****************************************************************************
 * Functions
 ******************************************************************************/
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
	for(index=0; index<TS_PACKET_LIST; index++)
	{
		if(list_empty(&pTask->ts_packet_bank))
			break;
		p_info = list_first_entry(&pTask->ts_packet_bank, struct packet_info, list);
		if(p_info == NULL)
		{
			pr_err("[ERROR][HWDMX]%s(%d):invalid NULL packet !!!\n", __func__, type);
			break;
		}
		list_del(&p_info->list);

		if(p_info->valid != 1)
		{
			pr_err("[ERROR][HWDMX]%s(%d):invalid packet !!!\n", __func__, type);
			p_info->valid = 0;
			continue;
		}
		p1 = p_info->p1;
		p1_size = p_info->p1_size;
		p2 = p_info->p2;
		p2_size = p_info->p2_size;
		p_info->valid = 0;

		if (type == TSIF_FILTER_TYPE_SECTION)
		{
			if (p2_size > 0)
			{
				tcc_hwdmx_tsif_buffer_flush(&pHandle->filp, (unsigned long)p2, p2_size);
			}
			tcc_dmx_sec_callback((unsigned int)p1, p1_size, p2, p2_size, devid);
		}
		else
		{
			if (p1_size > 0)
			{
				tcc_hwdmx_tsif_buffer_flush(&pHandle->filp, (unsigned long)p1, p1_size);
			}
			if (p2_size > 0)
			{
				tcc_hwdmx_tsif_buffer_flush(&pHandle->filp, (unsigned long)p2, p2_size);
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

	if(p_info->valid == 0)
	{
		p_info->p1 = p1;
		p_info->p1_size = p1_size;

		p_info->p2 = p2;
		p_info->p2_size = p2_size;

		p_info->valid = 1;

		list_add_tail(&p_info->list, &pTask->ts_packet_bank);
		if(++index >= TS_PACKET_LIST)
			index = 0;
		pTask->ts_packet_list.current_index = index;
		tasklet_schedule(&pTask->tsif_tasklet);
	}
	else
	{
		pr_err("[ERROR][HWDMX]%s(%d):no space in packet_bank !!![%d]\n", __func__, type, index);
		pTask->ts_packet_list.current_index = 0;
		memset(pTask->ts_packet_list.ts_packet, 0, sizeof(struct packet_info)*TS_PACKET_LIST);
		INIT_LIST_HEAD(&pTask->ts_packet_bank);
	}
	return 0;
}

int tcc_tsif_init(tcc_tsif_inst_t *inst)
{
	int i;

	inst->tsif = kzalloc(inst->dev_num*sizeof(tcc_tsif_priv_t), GFP_KERNEL);
	for (i = 0; i < inst->dev_num; i++)
	{
		mutex_init(&inst->tsif[i].mutex);
	}

	gInst[inst->adapter->num] = inst;

	//pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

	return 0;
}

int tcc_tsif_deinit(tcc_tsif_inst_t *inst)
{
	int i;

	gInst[inst->adapter->num] = NULL;

	for (i = 0; i < inst->dev_num; i++)
	{
		mutex_destroy(&inst->tsif[i].mutex);
	}

	kfree(inst->tsif);

	//pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

	return 0;
}

int tcc_tsif_interface_open(unsigned int devid)
{
	struct inode inode;
	tcc_tsif_priv_t *pHandle;
	tcc_tsif_tasklet_t *pTask;
	int err, ret, type;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	mutex_lock(&pHandle->mutex);
	ret = pHandle->users;
	pHandle->users++;
	if (pHandle->users == 1)
	{
		pHandle->filp.private_data = NULL;
		inode.i_rdev = MKDEV(0, devid);
		err = tcc_hwdmx_tsif_open(&inode, &pHandle->filp);
		if (err != 0)
		{
			pHandle->users = 0;
			mutex_unlock(&pHandle->mutex);
			pr_err("[ERROR][HWDMX]Failed to open tsif driver[ID=%d, Err=%d]\n", devid, err);
			return err;
		}

		memset(&pHandle->pcr_table, 0xff, sizeof(pcr_table_t));
		memset(&pHandle->ts_table, 0xff, sizeof(ts_table_t));
		memset(&pHandle->sec_table, 0xff, sizeof(sec_table_t));

		for (type = 0; type < TSIF_FILTER_TYPE_MAX; type++)
		{
			pTask = &pHandle->task[type];

			memset(&pTask->ts_packet_list, 0x0, sizeof(struct packet_list));
			INIT_LIST_HEAD(&pTask->ts_packet_bank);
			spin_lock_init(&pTask->ts_packet_list.lock);
			tasklet_init(&pTask->tsif_tasklet, tcc_tsif_running_feed, (type << 16) | devid);
		}

		tcc_hwdmx_tsif_set_external_tsdemux(&pHandle->filp, tcc_tsif_parse_packet, 1, 0);
	}

	mutex_unlock(&pHandle->mutex);

	//pr_info("[INFO][HWDMX]%s(ret = %d)\n", __FUNCTION__, ret);

	return ret;
}

int tcc_tsif_interface_close(unsigned int devid)
{
	struct inode inode;
	tcc_tsif_priv_t *pHandle;
	int err, ret, type;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users == 0)
		return -1;

	mutex_lock(&pHandle->mutex);
	pHandle->users--;
	if (pHandle->users == 0)
	{
		tcc_hwdmx_tsif_set_external_tsdemux(&pHandle->filp, tcc_tsif_parse_packet, 0, 0);
		for (type = 0; type < TSIF_FILTER_TYPE_MAX; type++)
		{
			tasklet_kill(&pHandle->task[type].tsif_tasklet);
		}
		inode.i_rdev = MKDEV(0, devid);
		err = tcc_hwdmx_tsif_release(&inode, &pHandle->filp);
		if (err != 0)
		{
			pHandle->users = 1;
			mutex_unlock(&pHandle->mutex);

			pr_err("[ERROR][HWDMX]Failed to close tsif driver[ID=%d, Err=%d]\n", devid, err);
			return err;
		}
	}
	ret = pHandle->users;
	mutex_unlock(&pHandle->mutex);

	//pr_info("[INFO][HWDMX]%s(ret = %d)\n", __FUNCTION__, ret);

	return ret;
}

static int tcc_tsif_hwdmx_can_support(void)
{
	return tca_tsif_can_support();
}

static int tcc_tsif_set_pid_for_hwdmx(tcc_tsif_priv_t *pHandle, struct dvb_demux_feed *feed)
{
	struct tcc_tsif_filter param;
	struct dvb_demux *demux = feed->demux;
	int i, j = MAX_TS_CNT;

	switch (feed->type)
	{
		case DMX_TYPE_TS:
		{
			for (i = 0; i < MAX_TS_CNT; i++)
			{
				if (pHandle->ts_table.iPID[i] == feed->pid)
				{
					pHandle->ts_table.iUsers[i]++;
					break;
				}
				if (i < j && !(pHandle->ts_table.iPID[i] < PID_NULL))
				{
					j = i;
				}
			}
			if (i == MAX_TS_CNT)
			{
				if (j == MAX_TS_CNT)
				{
					pr_err("[ERROR][HWDMX]Failed to add filter[pid = %d]\n", feed->pid);
					return -1;
				}

				pHandle->ts_table.iPID[j] = feed->pid;
				pHandle->ts_table.iUsers[j] = 1;

#ifdef SECFILTER_TO_TSFILTER
				for (i = 0; i < MAX_SEC_CNT; i++) // if section filter for same pid, it must be stop.
				{
					if (pHandle->sec_table.iPID[i] == feed->pid &&
						pHandle->sec_table.iUseSW[i] == 0)
					{
						pHandle->sec_table.iUseSW[i] = 1;

						param.f_type = 0; // HW_DEMUX_SECTION
						param.f_id = i;
						param.f_pid = feed->pid;
						param.f_size = 0;
						if (tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_REMOVE_PID, (unsigned long)&param) != 0)
						{
							pr_err("[ERROR][HWDMX]%s : pid setting error !!!\n", __func__);
						}
					}
				}
#endif
				param.f_type = 1; // HW_DEMUX_TS
				param.f_id = 0;
				param.f_pid = feed->pid;
				param.f_size = 0;
				if (tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_ADD_PID, (unsigned long)&param) != 0)
				{
					pr_err("[ERROR][HWDMX]%s : pid setting error !!!\n", __func__);
					return -1;
				}
			}
			break;
		}
		case DMX_TYPE_SEC:
		{
			int useSW = (demux->dmx.frontend->source == DMX_MEMORY_FE) ? 1 : 0;
#ifdef SECFILTER_TO_TSFILTER
			for (i = 0; i < MAX_TS_CNT && useSW == 0; i++)
			{
				if (pHandle->ts_table.iPID[i] == feed->pid) // if ts filter for same pid, use s/w section filter
				{
					useSW = 1;
				}
			}
#endif
			for (i = 0; i < demux->filternum; i++)
			{
				if (demux->filter[i].state != DMX_STATE_READY)
					continue;
				if (demux->filter[i].type != DMX_TYPE_SEC)
					continue;
				if (demux->filter[i].filter.parent != &feed->feed.sec)
					continue;
				demux->filter[i].state = DMX_STATE_GO;

				pHandle->sec_table.iPID[demux->filter[i].index] = feed->pid;
				pHandle->sec_table.iUseSW[demux->filter[i].index] = useSW;

				if (useSW == 0)
				{
					param.f_type = 0; // HW_DEMUX_SECTION
					param.f_id = demux->filter[i].index;
					param.f_pid = feed->pid;
					param.f_comp = demux->filter[i].filter.filter_value;
					param.f_mask = demux->filter[i].filter.filter_mask;
					param.f_mode = demux->filter[i].filter.filter_mode;
					for (j = DMX_MAX_FILTER_SIZE; j > 0; j--)
					{
						if (param.f_mask[j-1] != 0x0)
						{
							break;
						}
					}
					param.f_size = j;
					if (tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_ADD_PID, (unsigned long)&param) != 0)
					{
						pr_err("[ERROR][HWDMX]%s : pid setting error !!!\n", __func__);
						return -1;
					}
				}
			}
			break;
		}
		case DMX_TYPE_PES:
		default:
		{
			break;
		}
	}

	//pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

	return 0;
}

static int tcc_tsif_set_pid(tcc_tsif_priv_t *pHandle, struct dvb_demux_feed *feed)
{
	struct tcc_tsif_pid_param pid_param;
	struct tcc_tsif_param dma_param;
	int i, j = MAX_TS_CNT;

	pid_param.valid_data_cnt = 0;
	j = MAX_TS_CNT;
	for (i = 0; i < MAX_TS_CNT; i++)
	{
		if (pHandle->ts_table.iPID[i] < PID_NULL)
		{
			if (pHandle->ts_table.iPID[i] == feed->pid)
			{
				pHandle->ts_table.iUsers[i]++;
				j = -1;
			}
			pid_param.pid_data[pid_param.valid_data_cnt] = pHandle->ts_table.iPID[i];
			pid_param.valid_data_cnt++;
		}
		else if (i < j)
		{
			j = i;
		}
	}
	if (j == MAX_TS_CNT)
	{
		pr_err("[ERROR][HWDMX]%s : failed to add pid\n", __func__);
		return -1;
	}
	if (j >= 0)
	{
		pHandle->ts_table.iPID[j] = feed->pid;
		pHandle->ts_table.iUsers[j] = 1;
		pid_param.pid_data[pid_param.valid_data_cnt] = feed->pid;
		pid_param.valid_data_cnt++;
	}

	if (tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_SET_PID, (unsigned long)&pid_param) != 0)
	{
		pr_err("[ERROR][HWDMX]%s : pid setting error !!!\n", __func__);
		return -1;
	}
	if (pHandle->is_start_dma == 0)
	{
		dma_param.ts_total_packet_cnt = TSIF_MAX_PACKETCOUNT;
		dma_param.ts_intr_packet_cnt = TSIF_INT_PACKETCOUNT;
		dma_param.dma_mode = DMA_MPEG2TS_MODE;
		dma_param.mode = 0x04;		//SPI_CS_HIGH;	//dibcom(DVBT), TCC35XX
		//dma_param.mode = 0x02;		//SPI_MODE_2;		//sharp(ISDBT)
		//dma_param.mode = 0x01;
		//dma_param.mode = 0x00;
		if (tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_DMA_START, (unsigned long)&dma_param) != 0)
		{
			pr_err("[ERROR][HWDMX]%s : dma setting error !!!\n", __func__);
			return -1;
		}
	}
	pHandle->is_start_dma++;

	//pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

	return 0;
}

int tcc_tsif_interface_set_pid(struct dvb_demux_feed *feed, unsigned int devid)
{
	tcc_tsif_priv_t *pHandle;
	int ret;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users == 0)
		return -1;

	mutex_lock(&pHandle->mutex);

	if (tcc_tsif_hwdmx_can_support())
	{
		ret = tcc_tsif_set_pid_for_hwdmx(pHandle, feed);
	}
	else
	{
		ret = tcc_tsif_set_pid(pHandle, feed);
	}

	mutex_unlock(&pHandle->mutex);

	//pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

	return ret;
}

int tcc_tsif_interface_set_pcrpid(int on, struct dvb_demux_feed *feed, unsigned int devid)
{
	tcc_tsif_priv_t *pHandle;
	struct tcc_tsif_pcr_param param;
	int ret;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users == 0)
		return -1;

	switch (feed->pes_type)
	{
		case DMX_PES_PCR0:
			param.index = 0;
			break;
		case DMX_PES_PCR1:
			param.index = 1;
			break;
		case DMX_PES_PCR2:
			param.index = 2;
			break;
		case DMX_PES_PCR3:
			param.index = 3;
			break;
		default:
			param.index = 0;
			break;
	}

	if (param.index >= MAX_PCR_CNT)
		return -1;

	if (on)
	{
		pHandle->pcr_table.iPID[param.index] = feed->pid;
		param.pcr_pid = feed->pid;
		if (param.index > 0)
			tcc_tsif_interface_set_pid(feed, devid);
	}
	else
	{
		pHandle->pcr_table.iPID[param.index] = PID_NULL;
		param.pcr_pid = PID_NULL;
		if (param.index > 0)
			tcc_tsif_interface_remove_pid(feed, devid);
	}

	mutex_lock(&pHandle->mutex);
	ret = tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_SET_PCRPID, (unsigned long)&param);
	mutex_unlock(&pHandle->mutex);

	//pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

	return ret;
}

static int tcc_tsif_remove_pid_for_hwdmx(tcc_tsif_priv_t *pHandle, struct dvb_demux_feed *feed)
{
	struct tcc_tsif_filter param;
	struct dvb_demux *demux = feed->demux;
	int i, j;

	switch (feed->type)
	{
		case DMX_TYPE_TS:
		{
			for (i = 0; i < MAX_TS_CNT; i++)
			{
				if (pHandle->ts_table.iPID[i] == feed->pid)
				{
					pHandle->ts_table.iUsers[i]--;
					break;
				}
			}
			if (i == MAX_TS_CNT)
			{
				pr_err("[ERROR][HWDMX]Failed to remove filter[pid = %d]\n", feed->pid);
				return -1;
			}
			if (pHandle->ts_table.iUsers[i] == 0)
			{
				pHandle->ts_table.iPID[i] = PID_NULL;

				param.f_type = TSIF_FILTER_TYPE_TS;
				param.f_id = 0;
				param.f_pid = feed->pid;
				param.f_size = 0;
				if (tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_REMOVE_PID, (unsigned long)&param) != 0 && param.f_pid != 0x2000)
				{
					pr_err("[ERROR][HWDMX]%s : pid setting error !!!\n", __func__);
				}
#ifdef SECFILTER_TO_TSFILTER
				for (i = 0; i < demux->filternum; i++) // if section filter for same pid, it must be stop.
				{
					if (pHandle->sec_table.iPID[demux->filter[i].index] == feed->pid &&
						pHandle->sec_table.iUseSW[demux->filter[i].index] != 0)
					{
						pHandle->sec_table.iUseSW[demux->filter[i].index] = 0;

						param.f_type = 0; // HW_DEMUX_SECTION
						param.f_id = demux->filter[i].index;
						param.f_pid = feed->pid;
						param.f_comp = demux->filter[i].filter.filter_value;
						param.f_mask = demux->filter[i].filter.filter_mask;
						param.f_mode = demux->filter[i].filter.filter_mode;
						for (j = DMX_MAX_FILTER_SIZE; j > 0; j--)
						{
							if (param.f_mask[j-1] != 0x0)
							{
								break;
							}
						}
						param.f_size = j;
						if (tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_ADD_PID, (unsigned long)&param) != 0)
						{
							pr_err("[ERROR][HWDMX]%s : pid setting error !!!\n", __func__);
						}
					}
				}
#endif
			}
			break;
		}
		case DMX_TYPE_SEC:
		{
			for (i = 0; i<demux->filternum; i++)
			{
				if (demux->filter[i].state == DMX_STATE_GO && demux->filter[i].filter.parent == &feed->feed.sec)
				{
					demux->filter[i].state = DMX_STATE_READY;
					pHandle->sec_table.iPID[demux->filter[i].index] = PID_NULL;
					if (pHandle->sec_table.iUseSW[demux->filter[i].index] == 0)
					{
						param.f_type = TSIF_FILTER_TYPE_SECTION;
						param.f_id = demux->filter[i].index;
						param.f_pid = feed->pid;
						param.f_size = 0;
						if (tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_REMOVE_PID, (unsigned long)&param) != 0)
						{
							pr_err("[ERROR][HWDMX]%s : pid setting error !!!\n", __func__);
							return -1;
						}
					}
				}
			}
			break;
		}
		case DMX_TYPE_PES:
		default:
		{
			break;
		}
	}

	//pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

	return 0;
}

static int tcc_tsif_remove_pid(tcc_tsif_priv_t *pHandle, struct dvb_demux_feed *feed)
{
	struct tcc_tsif_pid_param pid_param;
	int i;

	pid_param.valid_data_cnt = 0;
	for (i = 0; i < MAX_TS_CNT; i++)
	{
		if (pHandle->ts_table.iPID[i] == feed->pid)
		{
			pHandle->ts_table.iUsers[i]--;
			if (pHandle->ts_table.iUsers[i] == 0)
			{
				pHandle->ts_table.iPID[i] = PID_NULL;
			}
		}
		if (pHandle->ts_table.iPID[i] < PID_NULL)
		{
			pid_param.pid_data[pid_param.valid_data_cnt] = pHandle->ts_table.iPID[i];
			pid_param.valid_data_cnt++;
		}
	}
	if (tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_SET_PID, (unsigned long)&pid_param) != 0)
	{
		pr_err("[ERROR][HWDMX]%s : pid setting error !!!\n", __func__);
		return -1;
	}

	pHandle->is_start_dma--;
	if (pHandle->is_start_dma == 0)
	{
		if(tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_DMA_STOP, 0)!=0)
		{
			pr_err("[ERROR][HWDMX]%s : dma setting error !!!\n", __func__);
			return -1;
		}
	}

	//pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

	return 0;
}

int tcc_tsif_interface_remove_pid(struct dvb_demux_feed *feed, unsigned int devid)
{
	tcc_tsif_priv_t *pHandle;
	int ret;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users == 0)
		return -1;

	mutex_lock(&pHandle->mutex);

	if (tcc_tsif_hwdmx_can_support())
	{
		ret = tcc_tsif_remove_pid_for_hwdmx(pHandle, feed);
	}
	else
	{
		ret = tcc_tsif_remove_pid(pHandle, feed);
	}

	mutex_unlock(&pHandle->mutex);

	//pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

	return 0;
}

unsigned int tcc_tsif_interface_get_stc(unsigned int index, unsigned int devid)
{
	tcc_tsif_priv_t *pHandle;
	unsigned int stc = index;

	if (gInst[devid] == NULL)
		return -1;

	pHandle = &gInst[devid]->tsif[0];

	if (pHandle->users == 0)
		return -1;

	mutex_lock(&pHandle->mutex);
	tcc_hwdmx_tsif_ioctl_ex(&pHandle->filp, IOCTL_TSIF_GET_STC, (unsigned long)&stc);
	mutex_unlock(&pHandle->mutex);

	return stc;
}
