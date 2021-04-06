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
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#include "hwdmx_cmd.h"
#include "hwdmx_test.h"
#include "tca_hwdemux_param.h"


#define HWDMX_NUM (2)		// 4
#define TS_PACKET_SIZE (188)
#define SEND_TS_CNT (3)
#define RECEIVE_SIZE (TS_PACKET_SIZE * 30)

static struct tcc_tsif_handle tTSIF[HWDMX_NUM];
static struct tea_dma_buf tDMA[HWDMX_NUM];

static void *Input_VAddr;
static dma_addr_t Input_PAddr;
static void *TSRev_VAddr;
static dma_addr_t TSRev_PAddr;
static void *SERev_VAddr;
static dma_addr_t SERev_PAddr;

static char ReceivePacket[RECEIVE_SIZE];
static char *pReceive;
static unsigned int PrevOffset[HWDMX_NUM];

static unsigned int SystemTime[HWDMX_NUM];

static int tcc_hwdmx_test_callback(unsigned int dmxid, unsigned int ftype,
				   unsigned int fid, unsigned int value1,
				   unsigned int value2, unsigned int bErrCRC)
{
	unsigned char *p;

	switch (ftype) {
	case 0:		// HW_DEMUX_SECTION
		{
			if (pReceive && bErrCRC == 0) {
				p = SERev_VAddr + RECEIVE_SIZE * dmxid + value1;
				memcpy(pReceive, p, value2 - value1);
				p[0] = 0xff;
				pReceive += value2 - value1;
			}
			break;
		}
	case 1:		// HW_DEMUX_TS
		{
			if (pReceive) {
				p = TSRev_VAddr + RECEIVE_SIZE * dmxid +
				    PrevOffset[dmxid];
				while (PrevOffset[dmxid] != value1) {
					memcpy(pReceive, p, TS_PACKET_SIZE);
					p[0] = 0xff;
					pReceive += TS_PACKET_SIZE;
					p += TS_PACKET_SIZE;
					PrevOffset[dmxid] += TS_PACKET_SIZE;
					if (PrevOffset[dmxid] == value2) {
						PrevOffset[dmxid] = 0;
						p = TSRev_VAddr +
						    RECEIVE_SIZE * dmxid;
					}
				}
			}
			break;
		}
	case 2:		// HW_DEMUX_PES
		{
			break;
		}
	case 3:		// HW_DEMUX_PCR
		{
			unsigned int uiSTC = (unsigned int)value2 & 0x80000000;

			uiSTC |= (unsigned int)value1 >> 1;
			SystemTime[dmxid] = uiSTC;
			break;
		}
	default:{
			pr_err("[ERR][HWDMX]Invalid Type : %d\n", ftype);
			break;
		}
	}
	return 0;
}

static void PacketInitialize(unsigned char *p)
{
	int i;

	memset(p, 0x0, TS_PACKET_SIZE * SEND_TS_CNT);
	for (i = 0; i < SEND_TS_CNT; i++) {
		memcpy(p, PKT_NULL, sizeof(PKT_NULL));
		p += TS_PACKET_SIZE;
	}
}

//////////////////////////////////////////////////////////////////////////
static void MakeTSPacket(unsigned char *p, char cc)
{
	memcpy(p, PKT_TS, sizeof(PKT_TS));
	p[3] |= cc;
}

static void MakePCRPacket(unsigned char *p, int i)
{
	memcpy(p, PKT_PCR[i], sizeof(PKT_PCR[i]));
}

static int CheckTSPacket(unsigned char *p)
{
	char cc;

	for (cc = 0; cc < 16; cc++) {
		if (cc != (p[3] & 0xf))
			break;

		p[3] &= 0xf0;
		if (memcmp(p, PKT_TS, sizeof(PKT_TS)) != 0)
			break;

		p += TS_PACKET_SIZE;
	}
	return (int)cc;
}

static int TSFilterTest(struct tcc_tsif_handle *h)
{
	struct tcc_tsif_filter param;
	char cc;

	// Initialize Send Packet
	PacketInitialize(Input_VAddr);

	// Initialize Receive Packet
	memset(ReceivePacket, 0xff, sizeof(ReceivePacket));

	// Add Filter
	param.f_type = 1;	// HW_DEMUX_TS
	param.f_pid = 0x333;
	hwdmx_add_filter_cmd(h, &param);
	hwdmx_set_pcrpid_cmd(h, 0x1ffe);

	pReceive = ReceivePacket;
	for (cc = 0; cc < 16; cc++) {
		// Send Packet
		MakeTSPacket(Input_VAddr, cc);
		MakePCRPacket(Input_VAddr + TS_PACKET_SIZE, cc);
		hwdmx_input_stream_cmd(h->dmx_id, (unsigned int)Input_PAddr,
					TS_PACKET_SIZE * SEND_TS_CNT);
	}
	msleep(1000);
	pReceive = 0;

	// Remove Filter
	hwdmx_set_pcrpid_cmd(h, 0xffff);
	hwdmx_remove_filter_cmd(h, &param);

	// Check Packet
	return CheckTSPacket(ReceivePacket);
}

//////////////////////////////////////////////////////////////////////////
static void MakePATPacket(unsigned char *p, char cc)
{
	memcpy(p, PKT_PAT, sizeof(PKT_PAT));
	p[3] |= cc;
}

static int CheckSectionPacket(unsigned char *p)
{
	char cc;

	for (cc = 0; cc < 16; cc++) {
		if (memcmp(p, &PKT_PAT[5], 32) != 0)
			break;

		p += 32;
	}
	return (int)cc;
}

static int SectionFilterTest(struct tcc_tsif_handle *h)
{
	struct tcc_tsif_filter param;
	char cc;
	unsigned char ucValue = 0x0;
	unsigned char ucMask = 0xff;
	unsigned char ucMode = 0xff;

	// Initialize Send Packet
	PacketInitialize(Input_VAddr);

	// Initialize Receive Packet
	memset(ReceivePacket, 0xff, sizeof(ReceivePacket));

	// Add Filter
	param.f_type = 0;	// HW_DEMUX_SECTION
	param.f_id = 0;
	param.f_pid = 0x0;
	param.f_comp = &ucValue;
	param.f_mask = &ucMask;
	param.f_mode = &ucMode;
	param.f_size = 1;
	hwdmx_add_filter_cmd(h, &param);

	pReceive = ReceivePacket;
	for (cc = 0; cc < 16; cc++) {
		// Send Packet
		MakePATPacket(Input_VAddr + TS_PACKET_SIZE, cc);
		hwdmx_input_stream_cmd(h->dmx_id, (unsigned int)Input_PAddr,
					TS_PACKET_SIZE * SEND_TS_CNT);
	}
	msleep(1000);
	pReceive = 0;

	// Remove Filter
	hwdmx_remove_filter_cmd(h, &param);

	// Check Packet
	return CheckSectionPacket(ReceivePacket);
}

//////////////////////////////////////////////////////////////////////////
static int __init tca_hwdemux_test_start(void)
{
	struct tcc_tsif_handle *h;
	int i, iRet;

	pr_info("[INFO][HWDMX] %s\n", __func__);

	Input_VAddr =
	    (unsigned char *)dma_alloc_coherent(0, TS_PACKET_SIZE * SEND_TS_CNT,
						&Input_PAddr, GFP_KERNEL);
	TSRev_VAddr =
	    (unsigned char *)dma_alloc_coherent(0, RECEIVE_SIZE * HWDMX_NUM,
						&TSRev_PAddr, GFP_KERNEL);
	SERev_VAddr =
	    (unsigned char *)dma_alloc_coherent(0, RECEIVE_SIZE * HWDMX_NUM,
						&SERev_PAddr, GFP_KERNEL);

	hwdmx_set_interface_cmd(-1, 2);

	for (i = 0; i < HWDMX_NUM; i++) {
		PrevOffset[i] = 0;

		h = &tTSIF[i];

		h->dmx_id = i;
		h->port_cfg.tsif_port = 0;

		h->dma_intr_packet_cnt = 0;
		h->serial_mode = 0;

		tDMA[i].buf_size = RECEIVE_SIZE;
		tDMA[i].v_addr = TSRev_VAddr + RECEIVE_SIZE * i;
		tDMA[i].dma_addr = TSRev_PAddr + RECEIVE_SIZE * i;
		tDMA[i].buf_sec_size = RECEIVE_SIZE;
		tDMA[i].v_sec_addr = SERev_VAddr + RECEIVE_SIZE * i;
		tDMA[i].dma_sec_addr = SERev_PAddr + RECEIVE_SIZE * i;

		h->dma_mode = 1;
		h->working_mode = 1;
		h->dma_buffer = &tDMA[i];

		hwdmx_start_cmd(h);
		hwdmx_buffer_updated_callback(h, tcc_hwdmx_test_callback);
	}

	for (i = 0; i < HWDMX_NUM; i++) {
		SystemTime[i] = 0;

		pr_info("[INFO][HWDMX] HWDMX#%d TS/PCR Filter Check...\n", i);
		iRet = TSFilterTest(&tTSIF[i]);
		pr_info("[INFO][HWDMX] HWDMX#%d TS Filter Result(%d)\n", i,
			iRet);
		pr_info("[INFO][HWDMX] HWDMX#%d PCR Filter Result(%d)\n", i,
			SystemTime[i]);

		pr_info("[INFO][HWDMX] HWDMX#%d Section Filter Check...\n", i);
		iRet = SectionFilterTest(&tTSIF[i]);
		pr_info("[INFO][HWDMX] HWDMX#%d Section Filter Result(%d)\n", i,
			iRet);

//		tca_tsif_clean(&tTSIF[i]);
	}

	return 0;
}

static void __exit tca_hwdemux_test_stop(void)
{
	pr_info("[INFO][HWDMX] %s\n", __func__);

#if 0
	int i;
	pr_info("[INFO][HWDMX] %s\n", __func__);
	for (i = HWDMX_NUM - 1; i >= 0; i--)
		tca_tsif_clean(&tTSIF[i]);
#endif
	if (Input_VAddr)
		dma_free_coherent(0, TS_PACKET_SIZE * SEND_TS_CNT, Input_VAddr,
				  Input_PAddr);
	if (TSRev_VAddr)
		dma_free_coherent(0, RECEIVE_SIZE * HWDMX_NUM, TSRev_VAddr,
				  TSRev_PAddr);
	if (SERev_VAddr)
		dma_free_coherent(0, RECEIVE_SIZE * HWDMX_NUM, SERev_VAddr,
				  SERev_PAddr);
}

#if 0
module_init(tca_hwdemux_test_start);
#else
device_initcall_sync(tca_hwdemux_test_start);
#endif

module_exit(tca_hwdemux_test_stop);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC HWDMX TEST Driver");
MODULE_LICENSE("GPL");
