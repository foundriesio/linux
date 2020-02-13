/*
 *  tcc_dmx.c
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

#include "demux.h"

#include <linux/module.h>
#include "tsif.h"

/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG "[TCC_DMX]"

static int dmx_debug = 0;

module_param(dmx_debug, int, 0644);
MODULE_PARM_DESC(dmx_debug, "Turn on/off demux debugging (default:off).");

#define dprintk(msg...)                          \
	{                                            \
		if (dmx_debug)                           \
			printk(KERN_INFO LOG_TAG "(D)" msg); \
	}

#define eprintk(msg...)                        \
	{                                          \
		printk(KERN_INFO LOG_TAG " (E) " msg); \
	}

/*****************************************************************************
 * Defines
 ******************************************************************************/
#if CONFIG_DVB_MAX_ADAPTERS
#define MAX_INST CONFIG_DVB_MAX_ADAPTERS
#else
#define MAX_INST 9
#endif
//#define TS_PACKET_CHK_MODE
//#define TS_SECTION_CHK_MODE //Check if HWDMX work normally

/*****************************************************************************
 * structures
 ******************************************************************************/
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
#endif // TS_PACKET_CHK_MODE

/*****************************************************************************
 * Variables
 ******************************************************************************/
static tcc_dmx_inst_t *gInst[] = {[0 ...(MAX_INST - 1)] = NULL};

#ifdef TS_PACKET_CHK_MODE
static ts_packet_chk_info_t *ts_packet_chk_info = NULL;
#endif // TS_PACKET_CHK_MODE

/*****************************************************************************
 * External Functions
 ******************************************************************************/

/*****************************************************************************
 * Functions
 ******************************************************************************/
#ifdef TS_PACKET_CHK_MODE
static void tcc_dmx_ts_cc_debug(int mod)
{
	if (ts_packet_chk_info != NULL) {
		if (ts_packet_chk_info->packet != NULL) {
			eprintk(
				"\n[ERROR][HWDMX] [total:%llu / err:%d (%d sec)]\n", ts_packet_chk_info->total_cnt,
				ts_packet_chk_info->total_err, (ts_packet_chk_info->debug_time * DEBUG_CHK_TIME));

			if (mod) {
				ts_packet_chk_t *tmp = NULL;

				tmp = ts_packet_chk_info->packet;
				do {
					eprintk(
						"[ERROR][HWDMX] \t\tpid:0x%04x => cnt:%llu err:%d\n", tmp->pid, tmp->cnt,
						tmp->err);
					tmp = tmp->next;
				} while (tmp != NULL);
			}
		}
	}
}

static void tcc_dmx_ts_cc_remove(unsigned short pid)
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

static void tcc_dmx_ts_cc_check(unsigned char *buf)
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
				eprintk("[ERROR][HWDMX] \t ts_packet_chk_t mem alloc err..\n");
			}

			memset(tmp, 0x0, sizeof(ts_packet_chk_t));
			tmp->pid = pid;
			tmp->cc = cc;
			tmp->next = NULL;
			tmp->cnt++;
			ts_packet_chk_info->packet = tmp;
			ts_packet_chk_info->packet_cnt++;

			eprintk(
				"[ERROR][HWDMX] \t>>>> create[%d] : 0x%04x / %02d\n",
				ts_packet_chk_info->packet_cnt, tmp->pid, tmp->cc);
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
								eprintk(
									"[ERROR][HWDMX] \t(%dmin) pid:0x%04x => cnt:%llu err:%d [%d -> %d]\n",
									ts_packet_chk_info->debug_time, tmp->pid, tmp->cnt, tmp->err,
									tmp->cc, cc);
							}
							tmp->cc = cc;
						} else if (cc < tmp->cc) {
							temp = tmp->err;
							tmp->err += (0x10 - tmp->cc + cc) - 1;
							if (temp != tmp->err) {
								ts_packet_chk_info->total_err += tmp->err - temp;
								eprintk(
									"[ERROR][HWDMX] \t(%dmin) pid:0x%04x => cnt:%llu err:%d [%d -> %d]\n",
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
					eprintk("[ERROR][HWDMX] \t ts_packet_chk_t mem alloc err..\n");
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

				eprintk(
					"[ERROR][HWDMX] \t>>>> create[%d] : 0x%04x / %02d\n",
					ts_packet_chk_info->packet_cnt, tmp->pid, tmp->cc);
			}
		}

		do_gettimeofday(&ts_packet_chk_info->time);
		ts_packet_chk_info->cur_time = ((ts_packet_chk_info->time.tv_sec * 1000) & 0x00ffffff)
			+ (ts_packet_chk_info->time.tv_usec / 1000);
		if ((ts_packet_chk_info->cur_time - ts_packet_chk_info->prev_time)
			> (DEBUG_CHK_TIME * 1000)) {
			// itv_ts_cc_debug(0);
			tcc_dmx_ts_cc_debug(1);

			ts_packet_chk_info->prev_time = ts_packet_chk_info->cur_time;
			ts_packet_chk_info->debug_time++;
		}
	}
}
#endif //#ifdef TS_PACKET_CHK_MODE

#ifdef TS_SECTION_CHK_MODE
static void hexdump(unsigned char *addr, unsigned int len)
{
	unsigned char *s = addr, *endPtr = (unsigned char *)((unsigned int)addr + len);
	unsigned int i, remainder = len % 16;
	unsigned char ucPrintBuffer[128] = {
		0,
	};
	unsigned char ucWholePrintBuffer[1024] = {
		0,
	};

	sprintf(ucPrintBuffer, "\n Offset        Hex Value         Ascii value\n");
	strcat(ucWholePrintBuffer, ucPrintBuffer);

	// print out 16 byte blocks.
	while (s + 16 <= endPtr) {
		// offset ì¶œë ¥.
		sprintf(ucPrintBuffer, "0x%08lx  ", (long)(s - addr));
		strcat(ucWholePrintBuffer, ucPrintBuffer);
		// 16 bytes ?¨ìœ„ë¡??´ìš© ì¶œë ¥.
		for (i = 0; i < 16; i++) {
			sprintf(ucPrintBuffer, "%02x ", s[i]);
			strcat(ucWholePrintBuffer, ucPrintBuffer);
		}
		sprintf(ucPrintBuffer, " ");
		strcat(ucWholePrintBuffer, ucPrintBuffer);

		for (i = 0; i < 16; i++) {
			if (s[i] >= 32 && s[i] <= 125)
				sprintf(ucPrintBuffer, "%c", s[i]);
			else
				sprintf(ucPrintBuffer, ".");

			strcat(ucWholePrintBuffer, ucPrintBuffer);
		}
		s += 16;
		sprintf(ucPrintBuffer, "\n");
		strcat(ucWholePrintBuffer, ucPrintBuffer);
	}

	// Print out remainder.
	if (remainder) {
		// offset ì¶œë ¥.
		sprintf(ucPrintBuffer, "0x%08lx  ", (long)(s - addr));
		strcat(ucWholePrintBuffer, ucPrintBuffer);

		// 16 bytes ?¨ìœ„ë¡?ì¶œë ¥?˜ê³  ?¨ì? ê²?ì¶œë ¥.
		for (i = 0; i < remainder; i++) {
			sprintf(ucPrintBuffer, "%02x ", s[i]);
			strcat(ucWholePrintBuffer, ucPrintBuffer);
		}
		for (i = 0; i < (16 - remainder); i++) {
			sprintf(ucPrintBuffer, "   ");
			strcat(ucWholePrintBuffer, ucPrintBuffer);
		}

		sprintf(ucPrintBuffer, " ");
		strcat(ucWholePrintBuffer, ucPrintBuffer);
		for (i = 0; i < remainder; i++) {
			if (s[i] >= 32 && s[i] <= 125)
				sprintf(ucPrintBuffer, "%c", s[i]);
			else
				sprintf(ucPrintBuffer, ".");
			strcat(ucWholePrintBuffer, ucPrintBuffer);
		}
		for (i = 0; i < (16 - remainder); i++) {
			sprintf(ucPrintBuffer, " ");
			strcat(ucWholePrintBuffer, ucPrintBuffer);
		}
		sprintf(ucPrintBuffer, "\n");
		strcat(ucWholePrintBuffer, ucPrintBuffer);
	}
	printk("%s", ucWholePrintBuffer);
	return;
}
#endif

#define DVR_FEED(f)                                            \
	(((f)->type == DMX_TYPE_TS) && ((f)->feed.ts.is_filtering) \
	 && (((f)->ts_type & (TS_PACKET | TS_DEMUX)) == TS_PACKET))
int tcc_dmx_can_write(int devid)
{
	struct dvb_demux *demux;
	struct dmx_frontend *frontend;
	struct dvb_demux_feed *feed;
	struct dmxdev_filter *dmxdevfilter;
	struct dvb_ringbuffer *buffer;
	tcc_dmx_inst_t *inst;
	int i;
	ssize_t size;
	ssize_t free;
	int dvr_done = 0;

	inst = gInst[devid];
	if (inst == NULL) {
		return 1;
	}

	for (i = 0; i < inst->dev_num; i++) {
		demux = &inst->dmx[i].demux;
		frontend = demux->dmx.frontend;
		if (frontend == NULL) {
			continue;
		}
		if (frontend->source == DMX_MEMORY_FE) {
			continue;
		}

		mutex_lock(&demux->mutex);
		if (demux->users > 0) {
			list_for_each_entry(feed, &demux->feed_list, list_head)
			{
				if ((DVR_FEED(feed)) && (dvr_done++)) {
					continue;
				}
				if (feed->type == DMX_TYPE_SEC) {
					continue;
				}
				if (!feed->feed.ts.is_filtering) {
					continue;
				}
				if (feed->pes_type == DMX_PES_PCR0 || feed->pes_type == DMX_PES_PCR1
					|| feed->pes_type == DMX_PES_PCR2 || feed->pes_type == DMX_PES_PCR3) {
					continue;
				}

				if (feed->ts_type & TS_PACKET) {
					dmxdevfilter = feed->feed.ts.priv;
					if (dmxdevfilter->params.pes.output == DMX_OUT_TAP
						|| dmxdevfilter->params.pes.output == DMX_OUT_TSDEMUX_TAP) {
						buffer = &dmxdevfilter->buffer;
					} else {
						buffer = &dmxdevfilter->dev->dvr_buffer;
					}
					size = buffer->size;
					if (size == 0) {
						continue;
					}
					free = dvb_ringbuffer_free(buffer);
					if ((size >> 3) > free) {
						dprintk("[DEBUG][HWDMX] %s return false\n", __FUNCTION__);
						mutex_unlock(&demux->mutex);
						return 0;
					}
				}
			}
		}
		mutex_unlock(&demux->mutex);
	}
	return 1;
}

int tcc_dmx_ts_callback(char *p1, int p1_size, char *p2, int p2_size, int devid)
{
	struct dvb_demux *demux;
	struct dmx_frontend *frontend;
	int i;
	tcc_dmx_inst_t *inst;

	inst = gInst[devid];

	if (inst == NULL)
		return 0;

#ifdef TS_PACKET_CHK_MODE
	{
		int i;
		if (p1 && p1_size) {
			for (i = 0; i < p1_size; i += 188)
				tcc_dmx_ts_cc_check(p1 + i);
		}

		if (p2 && p2_size) {
			for (i = 0; i < p2_size; i += 188)
				tcc_dmx_ts_cc_check(p2 + i);
		}
	}
#endif
	for (i = 0; i < inst->dev_num; i++) {
		demux = &inst->dmx[i].demux;

		frontend = demux->dmx.frontend;
		if (frontend == NULL)
			continue;

		if (frontend->source == DMX_MEMORY_FE)
			continue;

		if (demux->users > 0) {
			if (p1 != NULL && p1_size > 0) {
				if (p1[0] != 0x47) {
					eprintk("[ERROR][HWDMX] packet error 1... [%x]\n", p1[0]);
					return -1;
				} else {
					/* no wraparound, dump olddma..newdma */
					dvb_dmx_swfilter_packets(demux, p1, p1_size / 188);
				}
			}
			if (p2 != NULL && p2_size > 0) {
				if (p2[0] != 0x47) {
					eprintk("[ERROR][HWDMX] packet error 2... [%x]\n", p2[0]);
					return -1;
				} else {
					dvb_dmx_swfilter_packets(demux, p2, p2_size / 188);
				}
			}
		}
	}
	return 0;
}

int tcc_dmx_sec_callback(unsigned int fid, int crc_err, char *p, int size, int devid)
{
	struct dvb_demux *demux;
	struct dmx_frontend *frontend;
	struct dvb_demux_filter *dmxfilter;
	struct dvb_demux_feed *feed;
	struct dmx_section_filter *secfilter;
	int i;
	tcc_dmx_inst_t *inst;

	inst = gInst[devid];

	if (inst == NULL)
		return 0;

	if (p == NULL || size <= 0)
		return 0;

	for (i = 0; i < inst->dev_num; i++) {
		demux = &inst->dmx[i].demux;

		frontend = demux->dmx.frontend;
		if (frontend == NULL)
			continue;

		if (frontend->source == DMX_MEMORY_FE)
			continue;

		if (demux->users > 0) {
			spin_lock(&demux->lock);

			dmxfilter = &demux->filter[fid];
			if (dmxfilter->state == DMX_STATE_GO) {
				secfilter = &dmxfilter->filter;
#if defined(CONFIG_ARCH_TCC893X)
				if (dmxfilter->doneq) {
					unsigned char xor, neq = 0;
					for (i = 0; i < DVB_DEMUX_MASK_MAX; i++) {
						xor = secfilter->filter_value[i] ^ p[i];
						neq |= dmxfilter->maskandnotmode[i] & xor;
					}
					if (!neq) {
						spin_unlock(&demux->lock);
						return -1; // filter not matched
					}
				}
#endif
				feed = dmxfilter->feed;
				if (feed->type == DMX_TYPE_SEC && feed->state == DMX_STATE_GO) {
					int section_syntax_indicator = 0;
#if 0
                     if(crc_err)
                     {
                        eprintk("[ERROR][HWDMX] [check section]should be checked crc result! : hwdemux:size[%d]crc_result[%d]\n", size, crc_err);
                        hexdump(p, 32);
                     }
#endif
					// check section_syntax_indicator
					// section_syntax_indicator This is a 1-bit indicator. When set to '1', it
					// indicates that the private section follows the  generic section syntax beyond
					// the private_section_length field. When set to '0', it indicates that the
					// private_data_bytes  immediately follow the private_section_length field  by
					// ISO138-1
					section_syntax_indicator = (p[1] & 0x80);
					if (section_syntax_indicator == 0)
						crc_err = 0; // don't need to check crc

					if (feed->feed.sec.check_crc && (crc_err == 2)) {
						// crc_err : 0(no crc error), 1(crc_error), 2(hwdmux cannot decide
						// crc_error)  Should check crc error, because hwdemux didn't  check crc
						// result.
						feed->feed.sec.crc_val = ~0;
						crc_err = 0;
						if (demux->check_crc32(feed, p, size)) {
							crc_err = 1;
#ifdef TS_SECTION_CHK_MODE
							hexdump(p, 64);
							eprintk(
								"[ERROR][HWDMX] [check section] section crc checking error !.section size[%d]crc_result[%d]\n",
								size, crc_err);
#endif
						}
					}

					if (!feed->feed.sec.check_crc || !crc_err) {
#ifdef TS_SECTION_CHK_MODE
						// Check CRC
						unsigned char neq = 0, has_error = 0;
						unsigned int secsize;
						secsize = 3 + ((p[1] & 0x0f) << 8) + p[2];
						if (secsize != size) {
							eprintk(
								"[ERROR][HWDMX] [check section] section size mismatch!-[%d][%d]\n",
								secsize, size);
							hexdump(p, 64);
							hexdump(p + size - 64, 64);
						}
						if (feed->feed.sec.check_crc && section_syntax_indicator) {
							feed->feed.sec.crc_val = ~0;
							if (demux->check_crc32(feed, p, size)) {
								eprintk(
									"[ERROR][HWDMX] [check section] section crc checking error !.section size[%d]\n",
									size);
								eprintk(
									"[ERROR][HWDMX] [check section] filter values [0x%X][0x%X][0x%X][0x%X][0x%X][0x%X].\n",
									secfilter->filter_value[0], secfilter->filter_value[1],
									secfilter->filter_value[2], secfilter->filter_value[3],
									secfilter->filter_value[4], secfilter->filter_value[5]);
								hexdump(p, 64);
								hexdump(p + size - 64, 64);
							}
						}

						for (i = 0; i < DVB_DEMUX_MASK_MAX; i++) {
							unsigned char xor = secfilter->filter_value[i] ^ p[i];
							if (dmxfilter->maskandmode[i] & xor) {
								has_error = 1;
								break;
							}
							neq |= dmxfilter->maskandnotmode[i] & xor;
						}
						if (dmxfilter->doneq && !neq)
							has_error = 1;

						if (has_error) {
							eprintk(
								"[ERROR][HWDMX] [check section] section matching error!.section size[%d] crc[%d] doneq[%d]\n",
								size, feed->feed.sec.check_crc, dmxfilter->doneq);
							eprintk(
								"[ERROR][HWDMX] [check section] filter values [0x%X][0x%X][0x%X][0x%X][0x%X][0x%X].\n",
								secfilter->filter_value[0], secfilter->filter_value[1],
								secfilter->filter_value[2], secfilter->filter_value[3],
								secfilter->filter_value[4], secfilter->filter_value[5]);
							hexdump(p, 64);
						}
#endif
						feed->cb.sec(p, size, NULL, 0, secfilter);
					}
				}
			}

			spin_unlock(&demux->lock);
		}
	}

	return 0;
}

static int tcc_dmx_start_feed(struct dvb_demux_feed *feed)
{
	tcc_dmx_priv_t *tcc_dmx = (tcc_dmx_priv_t *)feed->demux->priv;
	unsigned int devid = tcc_dmx->connected_id;

	if (tcc_tsif_start(devid) == 0) {
#ifdef TS_PACKET_CHK_MODE
		ts_packet_chk_info =
			(ts_packet_chk_info_t *)kmalloc(sizeof(ts_packet_chk_info_t), GFP_ATOMIC);
		if (ts_packet_chk_info == NULL) {
			eprintk("[ERROR][HWDMX] \t ts_packet_chk_info_t mem alloc err..\n");
		}
		memset(ts_packet_chk_info, 0x0, sizeof(ts_packet_chk_info_t));
#endif
	}

	if (feed->pid != MAX_PID) {
		if ((feed->type == DMX_TYPE_TS)
			&& (feed->pes_type == DMX_PES_PCR0 || feed->pes_type == DMX_PES_PCR1
				|| feed->pes_type == DMX_PES_PCR2 || feed->pes_type == DMX_PES_PCR3)) {
			if (feed->ts_type & TS_DEMUX) {
				tcc_tsif_set_pcrpid(1, feed, devid);
			}
		} else {
			tcc_tsif_set_pid(feed, devid);
		}
	}

	dprintk("[DEBUG][HWDMX] %s(pid = 0x%x)\n", __FUNCTION__, feed->pid);

	return 0;
}

static int tcc_dmx_stop_feed(struct dvb_demux_feed *feed)
{
	tcc_dmx_priv_t *tcc_dmx = (tcc_dmx_priv_t *)feed->demux->priv;
	unsigned int devid = tcc_dmx->connected_id;

	if (feed->pid != MAX_PID) {
		if ((feed->type == DMX_TYPE_TS)
			&& (feed->pes_type == DMX_PES_PCR0 || feed->pes_type == DMX_PES_PCR1
				|| feed->pes_type == DMX_PES_PCR2 || feed->pes_type == DMX_PES_PCR3)) {
			if (feed->ts_type & TS_DEMUX) {
				tcc_tsif_set_pcrpid(0, feed, devid);
			}
		} else {
			tcc_tsif_remove_pid(feed, devid);
		}
	}

	if (tcc_tsif_stop(devid) == 0) {
#ifdef TS_PACKET_CHK_MODE
		ts_packet_chk_t *tmp = NULL;
		ts_packet_chk_t *tmp_ = NULL;

		if (ts_packet_chk_info != NULL) {
			if (ts_packet_chk_info->packet != NULL) {
				tcc_dmx_ts_cc_debug(1);

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
#endif
	}

	dprintk("[DEBUG][HWDMX] %s(pid = 0x%x)\n", __FUNCTION__, feed->pid);

	return 0;
}

static int tcc_dmx_get_stc(struct dmx_demux *demux, unsigned int num, u64 *stc, unsigned int *base)
{
	struct dvb_demux *dvbdmx = (struct dvb_demux *)demux->priv;
	tcc_dmx_priv_t *tcc_dmx = (tcc_dmx_priv_t *)dvbdmx->priv;
	unsigned int devid = tcc_dmx->connected_id;

	return tcc_tsif_get_stc(num, devid, stc);
}

static int tcc_dmx_write_to_decoder(struct dvb_demux_feed *feed, const u8 *buf, size_t len)
{
	return 0;
}

int tcc_dmx_init(tcc_dmx_inst_t *inst)
{
	int i, j;

	inst->dmx = kzalloc(inst->dev_num * sizeof(tcc_dmx_priv_t), GFP_KERNEL);

	for (i = 0; i < inst->dev_num; i++) {
		inst->dmx[i].connected_id = inst->adapter->num;

		/* register demux stuff */
		inst->dmx[i].demux.dmx.capabilities =
			DMX_TS_FILTERING | DMX_SECTION_FILTERING | DMX_MEMORY_BASED_FILTERING;
		inst->dmx[i].demux.priv = &inst->dmx[i];
		inst->dmx[i].demux.filternum = 256;
		inst->dmx[i].demux.feednum = 256;
		inst->dmx[i].demux.start_feed = tcc_dmx_start_feed;
		inst->dmx[i].demux.stop_feed = tcc_dmx_stop_feed;
		inst->dmx[i].demux.write_to_decoder = tcc_dmx_write_to_decoder;

		if (dvb_dmx_init(&inst->dmx[i].demux) < 0) {
			eprintk("[ERROR][HWDMX] %s(fail - dvb_dmx_init)\n", __FUNCTION__);
			return -1;
		}

		inst->dmx[i].dmxdev.filternum = 256;
		inst->dmx[i].dmxdev.demux = &inst->dmx[i].demux.dmx;
		inst->dmx[i].dmxdev.capabilities = 0;
		inst->dmx[i].dmxdev.demux->get_stc = tcc_dmx_get_stc;

		if (dvb_dmxdev_init(&inst->dmx[i].dmxdev, inst->adapter) < 0) {
			eprintk("[ERROR][HWDMX] %s(fail - dvb_dmxdev_init)\n", __FUNCTION__);
			return -1;
		}

		for (j = 0; j < 4; j++) {
			inst->dmx[i].fe_hw[j].source = DMX_FRONTEND_0 + i;
			if (inst->dmx[i].demux.dmx.add_frontend(&inst->dmx[i].demux.dmx, &inst->dmx[i].fe_hw[j])
				< 0) {
				eprintk("[ERROR][HWDMX] %s(fail - add_frontend fe_hw[%d])\n", __FUNCTION__, j);
				return -1;
			}
		}

		inst->dmx[i].fe_mem.source = DMX_MEMORY_FE;
		if (inst->dmx[i].demux.dmx.add_frontend(&inst->dmx[i].demux.dmx, &inst->dmx[i].fe_mem)
			< 0) {
			eprintk("[ERROR][HWDMX] %s(fail - add_frontend fe_mem)\n", __FUNCTION__);
			return -1;
		}

		if (inst->dmx[i].demux.dmx.connect_frontend(&inst->dmx[i].demux.dmx, &inst->dmx[i].fe_hw[0])
			< 0) {
			eprintk("[ERROR][HWDMX] %s(fail - connect_frontend)\n", __FUNCTION__);
			return -1;
		}
	}

	gInst[inst->adapter->num] = inst;

	dprintk("[DEBUG][HWDMX] %s\n", __FUNCTION__);

	return 0;
}

int tcc_dmx_deinit(tcc_dmx_inst_t *inst)
{
	int i, j;

	gInst[inst->adapter->num] = NULL;

	for (i = 0; i < inst->dev_num; i++) {
		inst->dmx[i].demux.dmx.remove_frontend(&inst->dmx[i].demux.dmx, &inst->dmx[i].fe_mem);
		for (j = 0; j < 4; j++) {
			inst->dmx[i].demux.dmx.remove_frontend(&inst->dmx[i].demux.dmx, &inst->dmx[i].fe_hw[j]);
		}
		dvb_dmxdev_release(&inst->dmx[i].dmxdev);
		dvb_dmx_release(&inst->dmx[i].demux);
	}

	kfree(inst->dmx);

	dprintk("[DEBUG][HWDMX] %s", __FUNCTION__);

	return 0;
}
