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
#include <linux/platform_device.h>
#include <linux/of_device.h>


#include "hwdmx_cmd.h"
#include "hwdmx_test.h"
#include "tca_hwdemux_param.h"

#if defined(CONFIG_HWDEMUX_BYPASS_MODE)
#define BYPASS_TEST
#else
#define NORMAL_TEST
#endif

#define HWDMX_NUM (2)
#define TS_PACKET_SIZE (188)
#define SEND_TS_CNT (2)
#define SEND_SECTION_CNT (1)
#define RECEIVE_SIZE (TS_PACKET_SIZE * 32)
#define HWDMX_TEST_DEV_NAME "tcc_hwdmx_test"

#define TS_CC_NUM				(16)
#define SECTION_CC_NUM			(16)

static struct tcc_tsif_handle tTSIF[HWDMX_NUM];
static struct tea_dma_buf tDMA[HWDMX_NUM];

static int majornum;
static struct class *class;
static void *Input_VAddr;
static dma_addr_t Input_PAddr;
static void *TSRev_VAddr;
static dma_addr_t TSRev_PAddr;
static void *SERev_VAddr;
static dma_addr_t SERev_PAddr;

#if defined(CONFIG_HWDEMUX_BYPASS_MODE)
static char ReceivePacket[RECEIVE_SIZE * (SEND_TS_CNT + SEND_SECTION_CNT)];
#else
static char ReceivePacket[RECEIVE_SIZE];
#endif
static char *pReceive;
static unsigned int PrevOffset[HWDMX_NUM];

static unsigned int SystemTime[HWDMX_NUM];



static int dump_packet(unsigned char *pkt, unsigned int size)
{
	unsigned int i = 0;

	pr_info("[INFO][HWDMX] %s\n", __func__);

	for (i = 0 ; i < size ; i++) {
		// print byte
		pr_info("%d : 0x%02x\n", i, pkt[i]);
	}

	return 0;
}

static int dump_packet_cmp(unsigned char *pkt1, unsigned char *pkt2,
		unsigned int size)
{
	unsigned int i = 0;

	pr_info("[INFO][HWDMX] %s\n", __func__);

	for (i = 0 ; i < size ; i++) {
		// index :  packet1 - packet2
		pr_info("%03d : 0x%02x - 0x%02x\n", i, pkt1[i], pkt2[i]);
	}

	return 0;
}

static int tcc_hwdmx_test_callback(unsigned int dmxid, unsigned int ftype,
				   unsigned int fid, unsigned int value1,
				   unsigned int value2, unsigned int bErrCRC)
{
	unsigned char *p;

#if 0		// for debug
	pr_info("[INFO][HWDMX] %s(dmxid=%d, ftype=%d, fid=%d, v1=%d, v2=%d)",
		__func__, dmxid, ftype, fid, value1, value2);
#endif

	switch (ftype) {
	case 0:		// HW_DEMUX_SECTION
		{
			if (pReceive && bErrCRC == 0) {
				p = SERev_VAddr + RECEIVE_SIZE * dmxid + value1;
				memcpy(pReceive, p, value2 - value1);
				//p[0] = 0xff;		// p[0] is sync byte
				pReceive += value2 - value1;
			}
			break;
		}
	case 1:		// HW_DEMUX_TS
		{
			if (pReceive) {
				p = TSRev_VAddr + RECEIVE_SIZE *
					(SEND_TS_CNT + SEND_SECTION_CNT) *
					dmxid + PrevOffset[dmxid];

				while (PrevOffset[dmxid] != value1) {
					memcpy(pReceive, p, TS_PACKET_SIZE);
					//p[0] = 0xff;	// p[0] is sync byte
					pReceive += TS_PACKET_SIZE;
					p += TS_PACKET_SIZE;
					PrevOffset[dmxid] += TS_PACKET_SIZE;

					if (PrevOffset[dmxid] == value2) {
						PrevOffset[dmxid] = 0;
						p =
						TSRev_VAddr+
						RECEIVE_SIZE*
						(SEND_TS_CNT+SEND_SECTION_CNT)*
						dmxid;
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
	p[3] |= cc;			// Insert CC
}

static void MakePCRPacket(unsigned char *p, int i)
{
	memcpy(p, PKT_PCR[i], sizeof(PKT_PCR[i]));
}

static int CheckTSPacket(unsigned char *p)
{
	char cc;
	unsigned int cc_cnt = 0;
	unsigned int ts_cnt = 0;
#if defined(BYPASS_TEST)
	unsigned int pcr_cnt = 0;
#endif

	for (cc = 0; cc < TS_CC_NUM; cc++) {
		if (cc != (p[3] & 0xf)) {		// Check CC
			//pr_info("[INFO][HWDMX] %s : cc is wrong\n", __func__);
			cc_cnt++;
		}

		p[3] &= 0xf0;		// Clear CC. PKT_TS's CC is zero
		// Check Packet Data
		if (memcmp(p, PKT_TS, sizeof(PKT_TS)) != 0) {
			// increase ts packet Error Counter
			ts_cnt++;
		}
		//dump_packet_cmp(p, PKT_TS, sizeof(PKT_TS));

		p += TS_PACKET_SIZE;

#if defined(BYPASS_TEST)
		if (memcmp(p, PKT_PCR[cc], sizeof(PKT_PCR[cc])) != 0) {
			//pr_info("[INFO][HWDMX] %s : Wrong PKT_PCR\n",
			//__func__);
			pcr_cnt++;
		}
		//dump_packet_cmp(p, PKT_PCR[cc], sizeof(PKT_PCR[cc]));

		p += TS_PACKET_SIZE;
#endif
	}

#if defined(NORMAL_TEST)
	pr_info("[INFO][HWDMX] %s : cc_cnt = %d, ts_cnt = %d\n",
			__func__, cc_cnt, ts_cnt);

	return (int)ts_cnt;
#else
	pr_info("[INFO][HWDMX] %s: cc_cnt = %d, ts_cnt = %d, pcr_cnt = %d\n",
			__func__, cc_cnt, ts_cnt, pcr_cnt);

	return (int)(ts_cnt + pcr_cnt);
#endif
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
#if defined(NORMAL_TEST)
	param.f_type = 1;	// HW_DEMUX_TS
	param.f_pid = 0x333;
	hwdmx_add_filter_cmd(h, &param);
	hwdmx_set_pcrpid_cmd(h, 0x1ffe);
#endif

	pReceive = ReceivePacket;
	for (cc = 0; cc < TS_CC_NUM; cc++) {
		// Send Packet
		MakeTSPacket(Input_VAddr, cc);
		MakePCRPacket(Input_VAddr + TS_PACKET_SIZE, cc);
		hwdmx_input_stream_cmd(h->dmx_id, (unsigned int)Input_PAddr,
				TS_PACKET_SIZE * SEND_TS_CNT);
	}
	msleep(1000);
	pReceive = 0;

	// Remove Filter
#if defined(NORMAL_TEST)
	hwdmx_set_pcrpid_cmd(h, 0xffff);
	hwdmx_remove_filter_cmd(h, &param);
#endif

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
	unsigned int sec_cnt = 0;

	for (cc = 0; cc < SECTION_CC_NUM; cc++) {
#if defined(NORMAL_TEST)
		if (memcmp(p, &PKT_PAT[5], 32) != 0) {
			// increase Error Counter
			sec_cnt++;
		}
		//dump_packet_cmp(p, &PKT_PAT[5], 32);

		p += 32;
#else
		p[3] &= 0xf0;		// Clear CC
		if (memcmp(p, PKT_PAT, sizeof(PKT_PAT)) != 0) {
			// increase section packet Error Counter
			sec_cnt++;
		}
		//dump_packet_cmp(p, PKT_PAT, sizeof(PKT_PAT));

		p += TS_PACKET_SIZE;
#endif
	}

	pr_info("[INFO][HWDMX] %s : section_cnt = %d\n", __func__, sec_cnt);

	return (int)sec_cnt;
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
#if defined(NORMAL_TEST)
	param.f_type = 0;	// HW_DEMUX_SECTION
	param.f_id = 0;
	param.f_pid = 0x0;
	param.f_comp = &ucValue;
	param.f_mask = &ucMask;
	param.f_mode = &ucMode;
	param.f_size = 1;
	hwdmx_add_filter_cmd(h, &param);
#endif

	pReceive = ReceivePacket;
	for (cc = 0; cc < SECTION_CC_NUM; cc++) {
		// Send Packet
		//MakePATPacket(Input_VAddr + TS_PACKET_SIZE, cc);
		MakePATPacket(Input_VAddr, cc);
		hwdmx_input_stream_cmd(h->dmx_id, (unsigned int)Input_PAddr,
				TS_PACKET_SIZE * SEND_SECTION_CNT);
	}
	msleep(1000);
	pReceive = 0;

	// Remove Filter
#if defined(NORMAL_TEST)
	hwdmx_remove_filter_cmd(h, &param);
#endif

	// Check Packet
	return CheckSectionPacket(ReceivePacket);
}

//////////////////////////////////////////////////////////////////////////
static int tca_hwdemux_test_start(void)
{
	struct tcc_tsif_handle *h;
	int i, iRet;

#if defined(NORMAL_TEST)
	pr_info("[INFO][HWDMX] %s : In (NORMAL)\n", __func__);
#else
	pr_info("[INFO][HWDMX] %s : In (BYPASS)\n", __func__);
#endif
	pr_info("[INFO][HWDMX] %s(ReceviePacket=0x%08x, size=%d)",
		__func__, ReceivePacket, sizeof(ReceivePacket));

	hwdmx_set_interface_cmd(-1, 2);

	for (i = 0; i < HWDMX_NUM; i++) {
		PrevOffset[i] = 0;

		h = &tTSIF[i];

		h->dmx_id = i;
		h->port_cfg.tsif_port = 0;

		h->dma_intr_packet_cnt = 0;
		h->serial_mode = 0;

		tDMA[i].buf_size =
			RECEIVE_SIZE * (SEND_TS_CNT + SEND_SECTION_CNT);
		tDMA[i].v_addr =
			TSRev_VAddr +
			RECEIVE_SIZE *
			(SEND_TS_CNT + SEND_SECTION_CNT) * i;
		tDMA[i].dma_addr =
			TSRev_PAddr +
			RECEIVE_SIZE *
			(SEND_TS_CNT + SEND_SECTION_CNT) * i;
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
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
static const struct file_operations fops = {
	.owner = THIS_MODULE,
};

static int hwdemux_test_probe(struct platform_device *pdev)
{
	int retval;

	pr_info("[INFO][HWDMX] %s\n", __func__);

	retval = register_chrdev(0, HWDMX_TEST_DEV_NAME, &fops);
	if (retval < 0)
		return retval;

	majornum = retval;
	class = class_create(THIS_MODULE, HWDMX_TEST_DEV_NAME);

	Input_VAddr =
		(unsigned char *)dma_alloc_coherent(&pdev->dev,
					TS_PACKET_SIZE * SEND_TS_CNT,
					&Input_PAddr, GFP_KERNEL);
	if (Input_VAddr == NULL) {
		pr_err("[ERROR][HWDMX] Input_VAddr DMA alloc error.\n");
		retval = -ENOMEM;
		goto dma_alloc_fail;
	} else {
		pr_info("[INFO][HWDMX] Input_VAddr DMA alloc @0x%X(Phy=0x%X), size:%d\n",
			Input_VAddr, Input_PAddr, TS_PACKET_SIZE * SEND_TS_CNT);
	}

	TSRev_VAddr =
		(unsigned char *)dma_alloc_coherent(&pdev->dev,
		RECEIVE_SIZE * HWDMX_NUM * (SEND_TS_CNT + SEND_SECTION_CNT),
		&TSRev_PAddr, GFP_KERNEL);
	if (TSRev_VAddr == NULL) {
		pr_err("[ERROR][HWDMX] TSRev_VAddr DMA alloc error.\n");
		retval = -ENOMEM;
		goto dma_alloc_fail;
	} else {
		pr_info(
		"[INFO][HWDMX] TSRev_VAddr DMA alloc @0x%X(Phy=0x%X), size:%d\n",
		TSRev_VAddr, TSRev_PAddr,
		RECEIVE_SIZE * HWDMX_NUM * (SEND_TS_CNT + SEND_SECTION_CNT));
	}

	SERev_VAddr =
		(unsigned char *)dma_alloc_coherent(&pdev->dev,
					RECEIVE_SIZE * HWDMX_NUM,
					&SERev_PAddr, GFP_KERNEL);
	if (SERev_VAddr == NULL) {
		pr_err("[ERROR][HWDMX] SERev_VAddr DMA alloc error.\n");
		retval = -ENOMEM;
		goto dma_alloc_fail;
	} else {
		pr_info(
		"[INFO][HWDMX] SERev_VAddr DMA alloc @0x%X(Phy=0x%X), size:%d\n",
		SERev_VAddr, SERev_PAddr, RECEIVE_SIZE * HWDMX_NUM);
	}

	tca_hwdemux_test_start();

	return 0;

dma_alloc_fail:
	class_destroy(class);
	unregister_chrdev(majornum, HWDMX_TEST_DEV_NAME);

	return retval;
}

static int hwdemux_test_remove(struct platform_device *pdev)
{
	pr_info("[INFO][HWDMX] %s\n", __func__);

	if (Input_VAddr) {
		dma_free_coherent(&pdev->dev, TS_PACKET_SIZE * SEND_TS_CNT,
				Input_VAddr, Input_PAddr);
		Input_VAddr = NULL;
	}

	if (TSRev_VAddr) {
		dma_free_coherent(&pdev->dev, RECEIVE_SIZE * HWDMX_NUM,
				TSRev_VAddr, TSRev_PAddr);
		TSRev_VAddr = NULL;
	}
	if (SERev_VAddr) {
		dma_free_coherent(&pdev->dev, RECEIVE_SIZE * HWDMX_NUM,
				SERev_VAddr, SERev_PAddr);
		SERev_VAddr = NULL;
	}

	class_destroy(class);
	unregister_chrdev(majornum, HWDMX_TEST_DEV_NAME);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id match[] = {
	{
	 .compatible = "telechips,hwdemux_test",
	 },
	{},
};

MODULE_DEVICE_TABLE(of, match);
#endif

static struct platform_driver hwdmx_test = {
	.probe = hwdemux_test_probe,
	.remove = hwdemux_test_remove,
	.driver = {
		   .name = "tcc_hwdemux_test",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = match,
#endif
		   },
};




#if 0
#if 0
module_init(tca_hwdemux_test_start);
#else
device_initcall_sync(tca_hwdemux_test_start);
#endif
module_exit(tca_hwdemux_test_stop);
#endif

module_platform_driver(hwdmx_test);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC HWDMX TEST Driver");
MODULE_LICENSE("GPL");
