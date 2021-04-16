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

#include "dvb_demux.h"
#include "dmxdev.h"

#include "tcc_dmx.h"
#include "tcc_tsif_interface.h"

/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG    "[TCC_DMX]"

static int dmx_debug;

module_param(dmx_debug, int, 0644);
MODULE_PARM_DESC(dmx_debug, "Turn on/off demux debugging (default:off).");
/*****************************************************************************
 * Defines
 ******************************************************************************/
#define MAX_INST 8
//#define TS_SECTION_CHK_MODE //Check if HWDMX work normally

#define TCC_CHECK_SECTION_SIZE
/*****************************************************************************
 * structures
 ******************************************************************************/

/*****************************************************************************
 * Variables
 ******************************************************************************/
static struct tcc_dmx_inst_t *gInst[] = {[0 ... (MAX_INST - 1)] = NULL };

/*****************************************************************************
 * External Functions
 ******************************************************************************/

/*****************************************************************************
 * Functions
 ******************************************************************************/
#ifdef TS_SECTION_CHK_MODE
static void hexdump(unsigned char *addr, unsigned int len)
{
	unsigned char *s = addr, *endPtr =
		(unsigned char *)((unsigned int)addr + len);
	unsigned int i, remainder = len % 16;
	unsigned char ucPrintBuffer[128] = {
		0,
	};
	unsigned char ucWholePrintBuffer[1024] = {
		0,
	};

	sprintf(ucPrintBuffer, "\n Offset	Hex Value	Ascii value\n");
	strcat(ucWholePrintBuffer, ucPrintBuffer);

	// print out 16 byte blocks.
	while (s + 16 <= endPtr) {
		sprintf(ucPrintBuffer, "0x%08lx  ", (long)(s - addr));
		strcat(ucWholePrintBuffer, ucPrintBuffer);

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
		sprintf(ucPrintBuffer, "0x%08lx  ", (long)(s - addr));
		strcat(ucWholePrintBuffer, ucPrintBuffer);

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
	pr_info("%s", ucWholePrintBuffer);
}
#endif

int tcc_dmx_ts_callback(char *p1, int p1_size, char *p2, int p2_size, int devid)
{
	struct dvb_demux *demux;
	struct dmx_frontend *frontend;
	struct tcc_dmx_inst_t *inst;
	int i;

	inst = gInst[devid];

	if (inst == NULL)
		return 0;

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
					pr_err(
					"[ERROR][HWDMX] packet error 1... [%x]\n",
					p1[0]);
					return -1;
				}

				/* no wraparound, dump olddma..newdma */
				dvb_dmx_swfilter_packets(
				demux, p1, p1_size / 188);
			}
			if (p2 != NULL && p2_size > 0) {
				if (p2[0] != 0x47) {
					pr_err(
					"[ERROR][HWDMX] packet error 2... [%x]\n",
					p2[0]);
					return -1;
				}

				dvb_dmx_swfilter_packets(
				demux, p2, p2_size / 188);
			}
		}
	}
	return 0;
}


#ifdef TS_SECTION_CHK_MODE
static void tcc_check_section(struct dvb_demux *demux,
	struct dmx_section_filter *sec,
	struct dvb_demux_filter *dmxfilter,
	struct dvb_demux_feed *feed, char *p,
	int size,
	int section_syntax_indicator)
{
	int i = 0;
	// Check CRC
	unsigned char neq = 0, has_error = 0;
	unsigned int secsize;

	secsize =
	    3 + ((p[1] & 0x0f) << 8) +
	    p[2];
	if (secsize != size) {
		hexdump(p, 64);
		hexdump(p + size - 64,
			64);
	}

	if (feed->feed.sec.check_crc
	    && section_syntax_indicator) {
		feed->feed.sec.crc_val =
		    ~0;
		if (demux->check_crc32
		    (feed, p, size)) {
			hexdump(p, 64);
			hexdump(p +
				size -
				64, 64);
		}
	}

	for (i = 0;
	     i < DVB_DEMUX_MASK_MAX;
	     i++) {
		unsigned char xor =
		    sec->filter_value[i] ^ p[i];
		if (dmxfilter->maskandmode[i] &	xor) {
			has_error = 1;
			break;
		}
		neq |=
		    dmxfilter->maskandnotmode[i] & xor;
	}
	if (dmxfilter->doneq && !neq)
		has_error = 1;

	if (has_error)
		hexdump(p, 64);

}
#endif

int tcc_dmx_sec_callback(unsigned int fid, int crc_err, char *p, int size,
			 int devid)
{
	struct dvb_demux *demux;
	struct dmx_frontend *frontend;
	struct dvb_demux_filter *dmxfilter;
	struct dvb_demux_feed *feed;
	struct dmx_section_filter *secfilter;
	struct tcc_dmx_inst_t *inst;
	int i;

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
				feed = dmxfilter->feed;
				if (feed->type == DMX_TYPE_SEC
				    && feed->state == DMX_STATE_GO) {
					int section_syntax_indicator = 0;

					section_syntax_indicator =
					    (p[1] & 0x80);
					// don't need to check crc
					if (section_syntax_indicator == 0)
						crc_err = 0;

					if (feed->feed.sec.check_crc
					    && (crc_err == 2)) {
						// crc_err : 0(no crc error),
						// 1(crc_error),
						// 2(hwdmux cannot decide
						// crc_error)  Should
						// check crc error,
						// because hwdemux
						// didn't  check crc
						// result.
						feed->feed.sec.crc_val = ~0;
						crc_err = 0;
					}

					if (feed->feed.sec.check_crc
					    && (crc_err == 2)
					    &&
					    demux->check_crc32(
					    feed, p, size)) {
						crc_err = 1;
#ifdef TS_SECTION_CHK_MODE
						hexdump(p, 64);
						size, crc_err);
#endif
					}

					if (!feed->feed.sec.check_crc
					    || !crc_err) {
#ifdef TS_SECTION_CHK_MODE
						tcc_check_section(
						demux,
						secfilter,
						dmxfilter,
						feed, p, size,
						section_syntax_indicator);
#endif

						feed->cb.sec(p, size, NULL, 0,
							     secfilter);
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
	struct tcc_dmx_priv_t *tcc_dmx =
	(struct tcc_dmx_priv_t *) feed->demux->priv;
	unsigned int devid = tcc_dmx->connected_id;

	tcc_tsif_interface_open(devid);

	if (feed->pid != MAX_PID) {
		if ((feed->type == DMX_TYPE_TS)
		    && (feed->pes_type == DMX_PES_PCR0
			|| feed->pes_type == DMX_PES_PCR1
			|| feed->pes_type == DMX_PES_PCR2
			|| feed->pes_type == DMX_PES_PCR3)) {
			if (feed->ts_type & TS_DEMUX)
				tcc_tsif_interface_set_pcrpid(1, feed, devid);
		} else {
			tcc_tsif_interface_set_pid(feed, devid);
		}
	}

//	pr_info("[INFO][HWDMX] %s(pid = 0x%x)\n", __func__, feed->pid);

	return 0;
}

static int tcc_dmx_stop_feed(struct dvb_demux_feed *feed)
{
	struct tcc_dmx_priv_t *tcc_dmx =
		(struct tcc_dmx_priv_t *) feed->demux->priv;
	unsigned int devid = tcc_dmx->connected_id;

	if (feed->pid != MAX_PID) {
		if ((feed->type == DMX_TYPE_TS)
		    && (feed->pes_type == DMX_PES_PCR0
			|| feed->pes_type == DMX_PES_PCR1
			|| feed->pes_type == DMX_PES_PCR2
			|| feed->pes_type == DMX_PES_PCR3)) {
			if (feed->ts_type & TS_DEMUX)
				tcc_tsif_interface_set_pcrpid(0, feed, devid);
		} else {
			tcc_tsif_interface_remove_pid(feed, devid);
		}
	}

	tcc_tsif_interface_close(devid);

//	pr_info("[INFO][HWDMX] %s(pid = 0x%x)\n", __func__, feed->pid);

	return 0;
}

static int tcc_dmx_get_stc(struct dmx_demux *demux, unsigned int num,
			   u64 *stc, unsigned int *base)
{
	struct dvb_demux *dvbdmx = (struct dvb_demux *)demux->priv;
	struct tcc_dmx_priv_t *tcc_dmx = (struct tcc_dmx_priv_t *) dvbdmx->priv;
	unsigned int devid = tcc_dmx->connected_id;

	*stc = (u64) tcc_tsif_interface_get_stc(num, devid);

	return 0;
}

static int tcc_dmx_write_to_decoder(struct dvb_demux_feed *feed, const u8 *buf,
				    size_t len)
{
	return 0;
}

int tcc_dmx_init(struct tcc_dmx_inst_t *inst)
{
	int i, j;

	inst->dmx =
	kcalloc(
	inst->dev_num,
	inst->dev_num * sizeof(struct tcc_dmx_priv_t),
	GFP_KERNEL);

	for (i = 0; i < inst->dev_num; i++) {
		inst->dmx[i].connected_id = inst->adapter->num;

		/* register demux stuff */
		inst->dmx[i].demux.dmx.capabilities =
		    DMX_TS_FILTERING | DMX_SECTION_FILTERING |
		    DMX_MEMORY_BASED_FILTERING;
		inst->dmx[i].demux.priv = &inst->dmx[i];
		inst->dmx[i].demux.filternum = 256;
		inst->dmx[i].demux.feednum = 256;
		inst->dmx[i].demux.start_feed = tcc_dmx_start_feed;
		inst->dmx[i].demux.stop_feed = tcc_dmx_stop_feed;
		inst->dmx[i].demux.write_to_decoder = tcc_dmx_write_to_decoder;

		if (dvb_dmx_init(&inst->dmx[i].demux) < 0) {
			pr_err("[ERROR][HWDMX] (fail - dvb_dmx_init)\n");
			return -1;
		}

		inst->dmx[i].dmxdev.filternum = 256;
		inst->dmx[i].dmxdev.demux = &inst->dmx[i].demux.dmx;
		inst->dmx[i].dmxdev.capabilities = 0;
		inst->dmx[i].dmxdev.demux->get_stc = tcc_dmx_get_stc;

		if (dvb_dmxdev_init(
			&inst->dmx[i].dmxdev, inst->adapter) < 0) {
			pr_err("[ERROR][HWDMX] (fail - dvb_dmxdev_init)\n");
			return -1;
		}

		for (j = 0; j < 4; j++) {
			inst->dmx[i].fe_hw[j].source = DMX_FRONTEND_0 + i;
			if (inst->dmx[i].demux.dmx.add_frontend(
				&inst->dmx[i].demux.dmx, &inst->dmx[i].fe_hw[j])
			    < 0) {
				pr_err(
				"[ERROR][HWDMX] %s(fail - add_frontend fe_hw[%d])\n",
				__func__, j);
				return -1;
			}
		}

		inst->dmx[i].fe_mem.source = DMX_MEMORY_FE;
		if (inst->dmx[i].demux.dmx.add_frontend(
			&inst->dmx[i].demux.dmx, &inst->dmx[i].fe_mem)
		    < 0) {
			pr_err(
			"%s(fail - add_frontend fe_mem)\n", __func__);
			return -1;
		}

		if (inst->dmx[i].demux.dmx.connect_frontend(
			&inst->dmx[i].demux.dmx, &inst->dmx[i].fe_hw[0])
		    < 0) {
			pr_err(
			"[ERROR][HWDMX] (fail - connect_frontend)\n");
			return -1;
		}
	}

	gInst[inst->adapter->num] = inst;

	pr_info("[INFO][HWDMX] %s\n", __func__);

	return 0;
}

int tcc_dmx_deinit(struct tcc_dmx_inst_t *inst)
{
	int i, j;

	gInst[inst->adapter->num] = NULL;

	for (i = 0; i < inst->dev_num; i++) {
		inst->dmx[i].demux.dmx.remove_frontend(
			&inst->dmx[i].demux.dmx, &inst->dmx[i].fe_mem);
		for (j = 0; j < 4; j++) {
			inst->dmx[i].demux.dmx.remove_frontend(
				&inst->dmx[i].demux.dmx,
				&inst->dmx[i].fe_hw[j]);
		}
		dvb_dmxdev_release(&inst->dmx[i].dmxdev);
		dvb_dmx_release(&inst->dmx[i].demux);
	}

	kfree(inst->dmx);

	pr_info("[INFO][HWDMX] %s", __func__);

	return 0;
}
