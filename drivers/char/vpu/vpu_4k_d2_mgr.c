// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2

#include <asm/system_info.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <soc/tcc/pmap.h>

#include "vpu_buffer.h"
#include "vpu_comm.h"
#include "vpu_devices.h"
#include "vpu_4k_d2_mgr_sys.h"
#include "vpu_4k_d2_mgr.h"

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_4K_D2_VMGR: " msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_4K_D2_VMGR: " msg)
#define cmdk(msg...)     V_DBG(VPU_DBG_INFO, "TCC_4K_D2_VMGR [Cmd]: " msg)
#define err(msg...)      V_DBG(VPU_DBG_INFO, "TCC_4K_D2_VMGR [Err]: " msg)

#define VPU_4K_D2_REGISTER_DUMP
#define VPU_4K_D2_DUMP_STATUS

#define DEBUG_VPU_4K_D2_K //To debug vpu drv in usersapce side (e.g. omx)

#if 0 //For test purpose!!
#define FORCED_ERROR
#endif
#ifdef FORCED_ERROR
#define FORCED_ERR_CNT 300
static int forced_error_count = FORCED_ERR_CNT;
#endif

#ifdef VPU_4K_D2_DUMP_STATUS
#define W5_REG_BASE                 0x0000
#define W5_BS_RD_PTR                0x0118
#define W5_BS_WR_PTR                0x011C
#define W5_BS_OPTION                0x0120
#define W5_CMD_BS_PARAM             0x0124

#define W4_VCPU_PDBG_RDATA_REG      0x001C
#define W5_VPU_FIO_CTRL_ADDR        0x0020
#define W5_VPU_FIO_DATA             0x0024
#endif

static unsigned int cntInt_4kd2;	// = 0;

#ifdef DEBUG_VPU_4K_D2_K
struct debug_4k_d2_k_isr_t {
	int ret_code_vmgr_hdr;
	unsigned int vpu_k_isr_cnt_hit;
	unsigned int wakeup_interrupt_cnt;
};
struct debug_4k_d2_k_isr_t vpu_4k_d2_isr_param_debug;
static unsigned int cntwk_4kd2;	// = 0;
#endif

// Control only once!!
static struct _mgr_data_t vmgr_4k_d2_data;
static struct task_struct *kidle_task;	// = NULL;

extern int tcc_vpu_4k_d2_dec(int Op, codec_handle_t *pHandle, void *pParam1,
			     void *pParam2);
extern int tcc_vpu_4k_d2_dec_esc(int Op, codec_handle_t *pHandle,
				 void *pParam1, void *pParam2);
extern int tcc_vpu_4k_d2_dec_ext(int Op, codec_handle_t *pHandle,
				 void *pParam1, void *pParam2);

int vmgr_4k_d2_opened(void)
{
	if (atomic_read(&vmgr_4k_d2_data.dev_opened) == 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(vmgr_4k_d2_opened);

#ifdef VPU_4K_D2_DUMP_STATUS
static unsigned int _vpu_4k_d2mgr_FIORead(unsigned int addr)
{
	unsigned int ctrl;
	unsigned int count = 0;
	unsigned int data = 0xffffffff;

	ctrl = (addr & 0xffff);
	ctrl |= (0 << 16);	/* read operation */
	vetc_reg_write(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_CTRL_ADDR, ctrl);
	count = 10000;
	while (count--) {
		ctrl =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_VPU_FIO_CTRL_ADDR);
		if (ctrl & 0x80000000) {
			data =
			    vetc_reg_read(vmgr_4k_d2_data.base_addr,
					  W5_VPU_FIO_DATA);
			break;
		}
	}

	return data;
}

static int _vpu_4k_d2mgr_FIOWrite(unsigned int addr, unsigned int data)
{
	unsigned int ctrl;

	vetc_reg_write(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_DATA, data);
	ctrl = (addr & 0xffff);
	ctrl |= (1 << 16);	/* write operation */
	vetc_reg_write(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_CTRL_ADDR, ctrl);

	return 1;
}

static unsigned int _vpu_4k_d2mgr_ReadRegVCE(unsigned int vce_addr)
{
#define VCORE_DBG_ADDR              0x8300
#define VCORE_DBG_DATA              0x8304
#define VCORE_DBG_READY             0x8308

	int vcpu_reg_addr;
	int udata;

	_vpu_4k_d2mgr_FIOWrite(VCORE_DBG_READY, 0);

	vcpu_reg_addr = vce_addr >> 2;

	_vpu_4k_d2mgr_FIOWrite(VCORE_DBG_ADDR, vcpu_reg_addr + 0x8000);

	while (1) {
		if (_vpu_4k_d2mgr_FIORead(VCORE_DBG_READY) == 1) {
			udata = _vpu_4k_d2mgr_FIORead(VCORE_DBG_DATA);
			break;
		}
	}

	return udata;
}

static void _vpu_4k_d2_dump_status(void)
{
	int rd, wr;
	unsigned int tq, ip, mc, lf;
	unsigned int avail_cu, avail_tu, avail_tc, avail_lf, avail_ip;
	unsigned int ctu_fsm, nb_fsm, cabac_fsm, cu_info, mvp_fsm, tc_busy;
	unsigned int lf_fsm, bs_data, bbusy, fv;
	unsigned int reg_val;
	unsigned int index;
	unsigned int vcpu_reg[31] = { 0, };

	V_DBG(VPU_DBG_REG_DUMP,
	"-------------------------------------------------------------------------------");
	V_DBG(VPU_DBG_REG_DUMP,
	"------                            VCPU STATUS                             -----");
	V_DBG(VPU_DBG_REG_DUMP,
	"-------------------------------------------------------------------------------");

	rd = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_BS_RD_PTR);
	wr = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_BS_WR_PTR);
	V_DBG(VPU_DBG_REG_DUMP,
	"RD_PTR: 0x%08x WR_PTR: 0x%08x BS_OPT: 0x%08x BS_PARAM: 0x%08x",
	    rd, wr, vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_BS_OPTION),
	    vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_CMD_BS_PARAM));

	// --------- VCPU register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] VCPU REG Dump");
	for (index = 0; index < 25; index++) {
		vetc_reg_write(vmgr_4k_d2_data.base_addr, 0x14,
			       (1 << 9) | (index & 0xff));
		vcpu_reg[index] =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x1c);

		if (index < 16) {
			V_DBG(VPU_DBG_REG_DUMP, "0x%08x\t", vcpu_reg[index]);
			if ((index % 4) == 3)
				V_DBG(VPU_DBG_REG_DUMP, "");
		} else {
			switch (index) {
			case 16:
				V_DBG(VPU_DBG_REG_DUMP,
				    "CR0: 0x%08x", vcpu_reg[index]);
				break;
			case 17:
				V_DBG(VPU_DBG_REG_DUMP,
				    "CR1: 0x%08x", vcpu_reg[index]);
				break;
			case 18:
				V_DBG(VPU_DBG_REG_DUMP,
				    "ML:  0x%08x", vcpu_reg[index]);
				break;
			case 19:
				V_DBG(VPU_DBG_REG_DUMP,
				    "MH:  0x%08x", vcpu_reg[index]);
				break;
			case 21:
				V_DBG(VPU_DBG_REG_DUMP,
				    "LR:  0x%08x", vcpu_reg[index]);
				break;
			case 22:
				V_DBG(VPU_DBG_REG_DUMP,
				    "PC:  0x%08x", vcpu_reg[index]);
				break;
			case 23:
				V_DBG(VPU_DBG_REG_DUMP,
				    "SR:  0x%08x", vcpu_reg[index]);
				break;
			case 24:
				V_DBG(VPU_DBG_REG_DUMP,
				    "SSP: 0x%08x", vcpu_reg[index]);
				break;
			}
		}
	}
	V_DBG(VPU_DBG_REG_DUMP, "[-] VCPU REG Dump");
	// --------- BIT register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] BPU REG Dump");
	V_DBG(VPU_DBG_REG_DUMP, "BITPC = 0x%08x",
	    _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x18)));
	V_DBG(VPU_DBG_REG_DUMP, "BIT START=0x%08x, BIT END=0x%08x",
	    _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x11c)),
	    _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x120)));

	V_DBG(VPU_DBG_REG_DUMP, "CODE_BASE           %x",
	    _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x7000 + 0x18)));
	V_DBG(VPU_DBG_REG_DUMP, "VCORE_REINIT_FLAG   %x",
	    _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x7000 + 0x0C)));

	// --------- BIT HEVC Status Dump
	ctu_fsm = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x48));
	nb_fsm = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x4c));
	cabac_fsm = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x50));
	cu_info = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x54));
	mvp_fsm = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x58));
	tc_busy = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x5c));
	lf_fsm = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x60));
	bs_data = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x64));
	bbusy = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x68));
	fv = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x6C));

	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] CTU_X: %4d, CTU_Y: %4d",
		_vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x40)),
		_vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x44)));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] CTU_FSM>   Main: 0x%02x, FIFO: 0x%1x, NB: 0x%02x, DBK: 0x%1x",
		((ctu_fsm >> 24) & 0xff), ((ctu_fsm >> 16) & 0xff),
		((ctu_fsm >> 8) & 0xff), (ctu_fsm & 0xff));
	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] NB_FSM: 0x%02x",
		nb_fsm & 0xff);
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] CABAC_FSM> SAO: 0x%02x, CU: 0x%02x, PU: 0x%02x, TU: 0x%02x, EOS: 0x%02x",
		((cabac_fsm >> 25) & 0x3f), ((cabac_fsm >> 19) & 0x3f),
		((cabac_fsm >> 13) & 0x3f), ((cabac_fsm >> 6) & 0x7f),
		(cabac_fsm & 0x3f));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] CU_INFO value = 0x%04x\n\t\t(l2cb: 0x%1x, cux: %1d, cuy; %1d, pred: %1d, pcm: %1d, wr_done: %1d, par_done: %1d, nbw_done: %1d, dec_run: %1d)",
		cu_info, ((cu_info >> 16) & 0x3),
		((cu_info >> 13) & 0x7), ((cu_info >> 10) & 0x7),
		((cu_info >> 9) & 0x3), ((cu_info >> 8) & 0x1),
		((cu_info >> 6) & 0x3), ((cu_info >> 4) & 0x3),
		((cu_info >> 2) & 0x3),  (cu_info & 0x3));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] MVP_FSM> 0x%02x", mvp_fsm & 0xf);
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] TC_BUSY> tc_dec_busy: %1d, tc_fifo_busy: 0x%02x",
		((tc_busy >> 3) & 0x1), (tc_busy & 0x7));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] LF_FSM>  SAO: 0x%1x, LF: 0x%1x",
		((lf_fsm >> 4) & 0xf), (lf_fsm  & 0xf));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] BS_DATA> ExpEnd=%1d, bs_valid: 0x%03x, bs_data: 0x%03x",
		((bs_data >> 31) & 0x1), ((bs_data >> 16) & 0xfff),
		(bs_data & 0xfff));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] BUS_BUSY> mib_wreq_done: %1d, mib_busy: %1d, sdma_bus: %1d",
		((bbusy >> 2) & 0x1), ((bbusy >> 1) & 0x1), (bbusy & 0x1));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] FIFO_VALID> cu: %1d, tu: %1d, iptu: %1d, lf: %1d, coff: %1d",
		((fv >> 4) & 0x1), ((fv >> 3) & 0x1), ((fv >> 2) & 0x1),
		((fv >> 1) & 0x1), (fv & 0x1));
	V_DBG(VPU_DBG_REG_DUMP, "[-] BPU REG Dump");

	// --------- VCE register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] VCE REG Dump");
	tq = _vpu_4k_d2mgr_ReadRegVCE(0xd0);
	ip = _vpu_4k_d2mgr_ReadRegVCE(0xd4);
	mc = _vpu_4k_d2mgr_ReadRegVCE(0xd8);
	lf = _vpu_4k_d2mgr_ReadRegVCE(0xdc);
	avail_cu =
	    (_vpu_4k_d2mgr_ReadRegVCE(0x11C) >> 16) -
	    (_vpu_4k_d2mgr_ReadRegVCE(0x110) >> 16);
	avail_tu =
	    (_vpu_4k_d2mgr_ReadRegVCE(0x11C) & 0xFFFF) -
	    (_vpu_4k_d2mgr_ReadRegVCE(0x110) & 0xFFFF);
	avail_tc =
	    (_vpu_4k_d2mgr_ReadRegVCE(0x120) >> 16) -
	    (_vpu_4k_d2mgr_ReadRegVCE(0x114) >> 16);
	avail_lf =
	    (_vpu_4k_d2mgr_ReadRegVCE(0x120) & 0xFFFF) -
	    (_vpu_4k_d2mgr_ReadRegVCE(0x114) & 0xFFFF);
	avail_ip =
	    (_vpu_4k_d2mgr_ReadRegVCE(0x124) >> 16) -
	    (_vpu_4k_d2mgr_ReadRegVCE(0x118) >> 16);
	V_DBG(VPU_DBG_REG_DUMP,
	"       TQ            IP              MC             LF      GDI_EMPTY          ROOM");
	V_DBG(VPU_DBG_REG_DUMP,
	"------------------------------------------------------------------------------------------------------------");
	V_DBG(VPU_DBG_REG_DUMP,
	"| %d %04d %04d | %d %04d %04d |  %d %04d %04d | %d %04d %04d | 0x%08x | CU(%d) TU(%d) TC(%d) LF(%d) IP(%d)",
		(tq>>22)&0x07, (tq>>11)&0x3ff, tq&0x3ff,
		(ip>>22)&0x07, (ip>>11)&0x3ff, ip&0x3ff,
		(mc>>22)&0x07, (mc>>11)&0x3ff, mc&0x3ff,
		(lf>>22)&0x07, (lf>>11)&0x3ff, lf&0x3ff,
		_vpu_4k_d2mgr_FIORead(0x88f4),
		/* GDI empty */
		avail_cu, avail_tu, avail_tc, avail_lf, avail_ip);

	/* CU/TU Queue count */
	reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x12C);
	V_DBG(VPU_DBG_REG_DUMP, "[DCIDEBUG] QUEUE COUNT: CU(%5d) TU(%5d) ",
		(reg_val>>16)&0xffff, reg_val&0xffff);
	reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x1A0);
	V_DBG(VPU_DBG_REG_DUMP,
	    "TC(%5d) IP(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);
	reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x1A4);
	V_DBG(VPU_DBG_REG_DUMP, "LF(%5d)", (reg_val>>16)&0xffff);
	V_DBG(VPU_DBG_REG_DUMP,
	"VALID SIGNAL : CU0(%d)  CU1(%d)  CU2(%d) TU(%d) TC(%d) IP(%5d) LF(%5d)\n"
	"               DCI_FALSE_RUN(%d) VCE_RESET(%d) CORE_INIT(%d) SET_RUN_CTU(%d)",
		(reg_val>>6)&1, (reg_val>>5)&1, (reg_val>>4)&1,
		(reg_val>>3)&1, (reg_val>>2)&1, (reg_val>>1)&1,
		(reg_val>>0)&1,
		(reg_val>>10)&1, (reg_val>>9)&1,
		(reg_val>>8)&1, (reg_val>>7)&1);

	V_DBG(VPU_DBG_REG_DUMP,
	"State TQ: 0x%08x IP: 0x%08x MC: 0x%08x LF: 0x%08x",
	    _vpu_4k_d2mgr_ReadRegVCE(0xd0), _vpu_4k_d2mgr_ReadRegVCE(0xd4),
	    _vpu_4k_d2mgr_ReadRegVCE(0xd8), _vpu_4k_d2mgr_ReadRegVCE(0xdc));
	V_DBG(VPU_DBG_REG_DUMP, "BWB[1]: RESPONSE_CNT(0x%08x) INFO(0x%08x)",
	    _vpu_4k_d2mgr_ReadRegVCE(0x194),
	    _vpu_4k_d2mgr_ReadRegVCE(0x198));
	V_DBG(VPU_DBG_REG_DUMP, "BWB[2]: RESPONSE_CNT(0x%08x) INFO(0x%08x)",
	    _vpu_4k_d2mgr_ReadRegVCE(0x194),
	    _vpu_4k_d2mgr_ReadRegVCE(0x198));
	V_DBG(VPU_DBG_REG_DUMP, "DCI INFO");
	V_DBG(VPU_DBG_REG_DUMP, "READ_CNT_0 : 0x%08x",
	    _vpu_4k_d2mgr_ReadRegVCE(0x110));
	V_DBG(VPU_DBG_REG_DUMP, "READ_CNT_1 : 0x%08x",
	    _vpu_4k_d2mgr_ReadRegVCE(0x114));
	V_DBG(VPU_DBG_REG_DUMP, "READ_CNT_2 : 0x%08x",
	    _vpu_4k_d2mgr_ReadRegVCE(0x118));
	V_DBG(VPU_DBG_REG_DUMP, "WRITE_CNT_0: 0x%08x",
	    _vpu_4k_d2mgr_ReadRegVCE(0x11c));
	V_DBG(VPU_DBG_REG_DUMP, "WRITE_CNT_1: 0x%08x",
	    _vpu_4k_d2mgr_ReadRegVCE(0x120));
	V_DBG(VPU_DBG_REG_DUMP, "WRITE_CNT_2: 0x%08x",
	    _vpu_4k_d2mgr_ReadRegVCE(0x124));
	reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x128);
	V_DBG(VPU_DBG_REG_DUMP, "LF_DEBUG_PT: 0x%08x", reg_val & 0xffffffff);
	V_DBG(VPU_DBG_REG_DUMP,
	"cur_main_state %2d, r_lf_pic_deblock_disable %1d, r_lf_pic_sao_disable %1d",
		(reg_val >> 16) & 0x1f, (reg_val >> 15) & 0x1,
		(reg_val >> 14) & 0x1);
	V_DBG(VPU_DBG_REG_DUMP,
	"para_load_done %1d, i_rdma_ack_wait %1d, i_sao_intl_col_done %1d, i_sao_outbuf_full %1d",
		(reg_val >> 13) & 0x1, (reg_val >> 12) & 0x1,
		(reg_val >> 11) & 0x1, (reg_val >> 10) & 0x1);
	V_DBG(VPU_DBG_REG_DUMP,
	"lf_sub_done %1d, i_wdma_ack_wait %1d, lf_all_sub_done %1d, cur_ycbcr %1d, sub8x8_done %2d",
		(reg_val >> 9) & 0x1, (reg_val >> 8) & 0x1,
		(reg_val >> 6) & 0x1, (reg_val >> 4) & 0x1,
		reg_val & 0xf);
	V_DBG(VPU_DBG_REG_DUMP, "[-] VCE REG Dump");
	V_DBG(VPU_DBG_REG_DUMP, "[-] VCE REG Dump");

	V_DBG(VPU_DBG_REG_DUMP,
	"-------------------------------------------------------------------------------");

	{
		unsigned int q_cmd_done_inst, stage0_inst_info,
		    stage1_inst_info, stage2_inst_info, dec_seek_cycle,
		    dec_parsing_cycle, dec_decoding_cycle;
		q_cmd_done_inst
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01E8);
		stage0_inst_info
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01EC);
		stage1_inst_info
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01F0);
		stage2_inst_info
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01F4);
		dec_seek_cycle
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01C0);
		dec_parsing_cycle
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01C4);
		dec_decoding_cycle
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01C8);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_QUEUE_CMD_DONE_INST : 0x%08x",
		       q_cmd_done_inst);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_STAGE0_INSTANCE_INFO : 0x%08x",
		       stage0_inst_info);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_STAGE1_INSTANCE_INFO : 0x%08x",
		       stage1_inst_info);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_STAGE2_INSTANCE_INFO : 0x%08x",
		       stage2_inst_info);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_DEC_SEEK_CYCLE : 0x%08x",
		    dec_seek_cycle);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_DEC_PARSING_CYCLE : 0x%08x",
		       dec_parsing_cycle);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_DEC_DECODING_CYCLE : 0x%08x",
		       dec_decoding_cycle);
	}

	V_DBG(VPU_DBG_REG_DUMP,
	"-------------------------------------------------------------------------------");

	/* SDMA & SHU INFO */
	// -----------------------------------------
	// SDMA registers
	// -----------------------------------------

	{
		//DECODER SDMA INFO
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5000);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_LOAD_CMD    = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5004);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_AUTO_MOD  = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5008);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_START_ADDR  = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x500C);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_END_ADDR   = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5010);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_ENDIAN     = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5014);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_IRQ_CLEAR  = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5018);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_BUSY       = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x501C);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_LAST_ADDR  = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5020);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_SC_BASE_ADDR  = 0x%x", reg_val);

		// -------------------------------------------
		// SHU registers
		// -------------------------------------------

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5400);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_INIT = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5404);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SEEK_NXT_NAL = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5408);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_RD_NAL_ADDR = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x540c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_STATUS = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5410);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_1 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5414);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_2 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5418);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_3 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x541c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_4 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5420);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_5 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5424);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_6 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5428);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_7 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x542c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_8 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5430);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SBYTE_LOW = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5434);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SBYTE_HIGH = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5438);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_ST_PAT_DIS = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5440);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_0 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5444);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_1 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5448);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_2 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x544c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_3 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5450);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_0 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5454);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_1 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5458);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_2 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x545c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_3 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5460);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_0 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5464);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_1 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5468);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x546c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_3 = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5470);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_NBUF_RPTR = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5474);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_NBUF_WPTR = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5478);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_REMAIN_BYTE = 0x%x", reg_val);

		// -----------------------------------------
		// GBU registers
		// -----------------------------------------

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5800);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_INIT = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5804);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_STATUS = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5808);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_TCNT = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x580c);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_TCNT = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c10);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF0_LOW = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c14);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF0_HIGH = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c18);

		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF1_LOW = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c1c);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF1_HIGH = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c20);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF2_LOW = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c24);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF2_HIGH = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c30);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF_RPTR = 0x%x", reg_val);
		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c34);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF_WPTR = 0x%x", reg_val);

		reg_val =
		    vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c28);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_REMAIN_BIT = 0x%x", reg_val);
	}
}
#endif

int vmgr_4k_d2_get_close(vputype type)
{
	return vmgr_4k_d2_data.closed[type];
}

int vmgr_4k_d2_get_alive(void)
{
	return atomic_read(&vmgr_4k_d2_data.dev_opened);
}

int vmgr_4k_d2_set_close(vputype type, int value, int bfreemem)
{
	if (vmgr_4k_d2_get_close(type) == value) {
		dprintk(" %d was already set into %d.", type, value);
		return -1;
	}

	vmgr_4k_d2_data.closed[type] = value;
	if (value == 1) {
		vmgr_4k_d2_data.handle[type] = 0x00;

		if (bfreemem)
			vmem_proc_free_memory(type);
	}

	return 0;
}

static void _vmgr_4k_d2_close_all(int bfreemem)
{
	vmgr_4k_d2_set_close(VPU_DEC, 1, bfreemem);
	vmgr_4k_d2_set_close(VPU_DEC_EXT, 1, bfreemem);
	vmgr_4k_d2_set_close(VPU_DEC_EXT2, 1, bfreemem);
	vmgr_4k_d2_set_close(VPU_DEC_EXT3, 1, bfreemem);
	vmgr_4k_d2_set_close(VPU_DEC_EXT4, 1, bfreemem);
}

int vmgr_4k_d2_process_ex(struct VpuList *cmd_list, vputype type, int Op,
			  int *result)
{
	if (atomic_read(&vmgr_4k_d2_data.dev_opened) == 0)
		return 0;

	err(" process_ex %d - 0x%x", type, Op);

	if (!vmgr_4k_d2_get_close(type)) {
		cmd_list->type = type;
		cmd_list->cmd_type = Op;
		cmd_list->handle = vmgr_4k_d2_data.handle[type];
		cmd_list->args = NULL;
		cmd_list->comm_data = NULL;
		cmd_list->vpu_result = result;
		vmgr_4k_d2_list_manager(cmd_list, LIST_ADD);

		return 1;
	}

	return 1;
}

static int _vmgr_4k_d2_internal_handler(unsigned int reason)
{
	int ret, ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	unsigned long flags;
	unsigned int vint_reason = 0;
	int oper_inst = 0, cnt = 0;
	int timeout = 500;

	if (vmgr_4k_d2_data.check_interrupt_detection) {
		oper_inst = atomic_read(&vmgr_4k_d2_data.oper_intr);
		if (oper_inst & reason) {
			detailk("Success 1: vpu-4k-d2 vp9/hevc operation!!");
			ret_code = RETCODE_SUCCESS;
		} else {
			for (cnt = 0; cnt < timeout; cnt += 5) {
				ret =
				    wait_event_interruptible_timeout(
				     vmgr_4k_d2_data.oper_wq,
				     (oper_inst & reason), msecs_to_jiffies(5));
				if (ret == 0 /*TIMEOUT*/) {
					vint_reason =
					    vetc_reg_read(
					      vmgr_4k_d2_data.base_addr,
					      0x004C
					      /*W5_VPU_VINT_REASON */
					      );

					oper_inst =
					    atomic_read(
					      &vmgr_4k_d2_data.oper_intr);
					oper_inst |= vint_reason;
				} else if (ret >= 0) {
					oper_inst =
					  atomic_read(
					      &vmgr_4k_d2_data.oper_intr);
				} else {
					//-ERESTARTSYS(Interrupted by a signal)
					err("ERESTARTSYS");
					break;
				}

				if (oper_inst & reason) {
					detailk(
					"Success 2: vpu-4k-d2 vp9/hevc operation!!");
				#if defined(FORCED_ERROR)
					if (forced_error_count-- <= 0) {
						ret_code = RETCODE_CODEC_EXIT;
						forced_error_count
						= FORCED_ERR_CNT;
					} else {
#endif
						ret_code = RETCODE_SUCCESS;
						break;
#if defined(FORCED_ERROR)
					}
#endif
				}
			}

			if (cnt >= timeout) {
				err(
				"[CMD 0x%x][%d]: vpu-4k-d2 vp9/hevc timed_out(ref %d msec) => oper_intr[%d]!! [%d]th frame len %d",
					vmgr_4k_d2_data.current_cmd, ret,
					timeout,
					vmgr_4k_d2_data.oper_intr,
					vmgr_4k_d2_data.nDecode_Cmd,
					vmgr_4k_d2_data.szFrame_Len);
				ret_code = RETCODE_CODEC_EXIT;
			}
		}
	}

	if (ret_code == RETCODE_SUCCESS)
		atomic_andnot(reason, &vmgr_4k_d2_data.oper_intr);

	#ifdef DEBUG_VPU_4K_D2_K
	vpu_4k_d2_isr_param_debug.ret_code_vmgr_hdr = ret_code;
	dprintk(
	"[%s] : ret_code (%d), cntInt_4kd2 (%d), cntwk_4kd2 (%d), vmgr_4k_d2_data.oper_intr (%d)",
		__func__, ret_code, cntInt_4kd2, cntwk_4kd2,
		vmgr_4k_d2_data.oper_intr);
	#endif

	V_DBG(VPU_DBG_INTERRUPT, "out (Interrupt option=%d, isr cnt=%d, ev=%d)",
		vmgr_4k_d2_data.check_interrupt_detection,
		cntInt_4kd2,
		ret_code);

	return ret_code;
}

static int _vmgr_4k_d2_process(vputype type, int cmd, long pHandle, void *args)
{
	int ret = 0;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
	struct timeval t1, t2;
	int time_gap_ms = 0;
#endif

	vmgr_4k_d2_data.check_interrupt_detection = 0;
	vmgr_4k_d2_data.current_cmd = cmd;

	if (type < VPU_ENC) {
		if (cmd != VPU_DEC_INIT && cmd != VPU_DEC_INIT_KERNEL) {
			if (vmgr_4k_d2_get_close(type)
			    || vmgr_4k_d2_data.handle[type] == 0x00)
				return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
		}

		if (cmd != VPU_DEC_BUF_FLAG_CLEAR
		    && cmd != VPU_DEC_DECODE
		    && cmd != VPU_DEC_BUF_FLAG_CLEAR_KERNEL
		    && cmd != VPU_DEC_DECODE_KERNEL)
			cmdk("Decoder(%d), command: 0x%x", type, cmd);

		switch (cmd) {
		case VPU_DEC_INIT:
		case VPU_DEC_INIT_KERNEL:
		{
			VPU_4K_D2_INIT_t *arg;

			arg = (VPU_4K_D2_INIT_t *) args;
			vmgr_4k_d2_data.handle[type] = 0x00;

			arg->gsV4kd2DecInit.m_RegBaseVirtualAddr
				 = (codec_addr_t)vmgr_4k_d2_data.base_addr;
			arg->gsV4kd2DecInit.m_Memcpy
			 = (void *(*) (void *, const void*,
			    unsigned int, unsigned int))vetc_memcpy;
			arg->gsV4kd2DecInit.m_Memset
			 = (void (*) (void *, int,
			      unsigned int, unsigned int))vetc_memset;
			arg->gsV4kd2DecInit.m_Interrupt
			 = (int (*) (unsigned int))_vmgr_4k_d2_internal_handler;
			arg->gsV4kd2DecInit.m_Ioremap
			 = (void *(*) (phys_addr_t, unsigned int))vetc_ioremap;
			arg->gsV4kd2DecInit.m_Iounmap
			 = (void (*) (void *))vetc_iounmap;
			arg->gsV4kd2DecInit.m_reg_read
			= (unsigned int (*)(void *, unsigned int))vetc_reg_read;
			arg->gsV4kd2DecInit.m_reg_write
			 = (void (*)(void *, unsigned int,
			     unsigned int))vetc_reg_write;
			arg->gsV4kd2DecInit.m_Usleep
			= (void (*)(unsigned int, unsigned int))vetc_usleep;

			vmgr_4k_d2_data.check_interrupt_detection = 1;

			vmgr_4k_d2_data.bDiminishInputCopy
			 = (arg->gsV4kd2DecInit.m_uiDecOptFlags
			      & (1 << 26)) ? true : false;
			if (system_rev == 0 /*MPW1*/
			    && arg->gsV4kd2DecInit.m_Reserved[10] == 10) {
				arg->gsV4kd2DecInit.m_uiDecOptFlags
				    |= WAVE5_WTL_ENABLE; //disable map converter
				V_DBG(VPU_DBG_INFO,
				"Dec :: Init In => enable WTL (off the compressed output mode)");
				// to notify this refusal
				arg->gsV4kd2DecInit.m_Reserved[10] = 5;

				//[work-around] reduce total memory size
				// of min. frame buffers
				// Max Bitdepth
				arg->gsV4kd2DecInit.m_Reserved[5] = 8;
				arg->gsV4kd2DecInit.m_uiDecOptFlags
				  |= (1 << 3);	// 10 to 8 bit shit
			}

			dprintk(
			"Dec :: Init In => workbuff 0x%x/0x%x, Reg: 0x%p/0x%x, format : %d, Stream(0x%x/0x%x, 0x%x)",
				arg->gsV4kd2DecInit.m_BitWorkAddr[PA],
				arg->gsV4kd2DecInit.m_BitWorkAddr[VA],
				vmgr_4k_d2_data.base_addr,
				arg->gsV4kd2DecInit.m_RegBaseVirtualAddr,
				arg->gsV4kd2DecInit.m_iBitstreamFormat,
				arg->gsV4kd2DecInit.m_BitstreamBufAddr[PA],
				arg->gsV4kd2DecInit.m_BitstreamBufAddr[VA],
				arg->gsV4kd2DecInit.m_iBitstreamBufSize);

			dprintk(
			"Dec :: Init In => optFlag 0x%x, Userdata(%d), Inter: %d, PlayEn: %d",
				arg->gsV4kd2DecInit.m_uiDecOptFlags,
				arg->gsV4kd2DecInit.m_bEnableUserData,
				arg->gsV4kd2DecInit.m_bCbCrInterleaveMode,
				arg->gsV4kd2DecInit.m_iFilePlayEnable);

			if (vmem_alloc_count(type) <= 0) {
				err(
				"Dec-%d ######################## No Buffer allocation",
					type);
				return RETCODE_FAILURE;
			}

			ret =
			    tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			      (void *)(&arg->gsV4kd2DecHandle),
			      (void *)(&arg->gsV4kd2DecInit),
			      (void *)NULL);
			if (ret != RETCODE_SUCCESS)
				V_DBG(VPU_DBG_SEQUENCE,
				  "Dec :: Init Done with ret(0x%x)", ret);

			if (ret != RETCODE_CODEC_EXIT
				&& arg->gsV4kd2DecHandle != 0) {
				vmgr_4k_d2_data.handle[type] =
					arg->gsV4kd2DecHandle;
				vmgr_4k_d2_set_close(type, 0, 0);
				cmdk(
				"Dec :: vmgr_4k_d2_data.handle = 0x%x",
					arg->gsV4kd2DecHandle);
			} else {
				//To free memory!!
				vmgr_4k_d2_set_close(type, 0, 0);
				vmgr_4k_d2_set_close(type, 1, 1);
			}
			dprintk("Dec :: Init Done Handle(0x%x)",
				arg->gsV4kd2DecHandle);

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			vmgr_4k_d2_data.iTime[type].print_out_index
			 = vmgr_4k_d2_data.iTime[type].proc_base_cnt = 0;
			vmgr_4k_d2_data.iTime[type].accumulated_proc_time
			 = vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt
			 = 0;
			vmgr_4k_d2_data.iTime[type].proc_time_30frames
			 = 0;
#endif
		}
		break;

		case VPU_DEC_SEQ_HEADER:
		case VPU_DEC_SEQ_HEADER_KERNEL:
		{
			void *arg = args;
			int iSize;

			vpu_4K_D2_dec_initial_info_t *gsV4kd2DecInitialInfo
			 = vmgr_4k_d2_data.bDiminishInputCopy
			   ? &((VPU_4K_D2_DECODE_t *)arg)->gsV4kd2DecInitialInfo
			   : &((VPU_4K_D2_SEQ_HEADER_t *)arg)
			       ->gsV4kd2DecInitialInfo;

			vmgr_4k_d2_data.szFrame_Len = iSize
			 = vmgr_4k_d2_data.bDiminishInputCopy
			    ? ((VPU_4K_D2_DECODE_t *)arg)->gsV4kd2DecInput
				    .m_iBitstreamDataSize
			    : (int)((VPU_4K_D2_SEQ_HEADER_t *)arg)->stream_size;

			vmgr_4k_d2_data.check_interrupt_detection = 1;
			vmgr_4k_d2_data.nDecode_Cmd = 0;

			dprintk(
			"Dec :: VPU_4K_D2_DEC_SEQ_HEADER in :: size(%d)",
				iSize);
			ret =
			    tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			    (codec_handle_t *)&pHandle,
			    (vmgr_4k_d2_data.bDiminishInputCopy
			      ? (void *)(&((VPU_4K_D2_DECODE_t *)arg)
				    ->gsV4kd2DecInput) :
			      (void *)iSize),
			      (void *)gsV4kd2DecInitialInfo);

			dprintk(
			"Dec :: VPU_4K_D2_DEC_SEQ_HEADER out 0x%x\n res info. %d - %d - %d, %d - %d - %d",
			    ret,
			    gsV4kd2DecInitialInfo->m_iPicWidth,
			    gsV4kd2DecInitialInfo->m_PicCrop.m_iCropLeft,
			    gsV4kd2DecInitialInfo->m_PicCrop.m_iCropRight,
			    gsV4kd2DecInitialInfo->m_iPicHeight,
			    gsV4kd2DecInitialInfo->m_PicCrop.m_iCropTop,
			    gsV4kd2DecInitialInfo->m_PicCrop.m_iCropBottom);

			vmgr_4k_d2_change_clock(
			    gsV4kd2DecInitialInfo->m_iPicWidth,
			    gsV4kd2DecInitialInfo->m_iPicHeight);
		}
		break;

		case VPU_DEC_REG_FRAME_BUFFER:
		case VPU_DEC_REG_FRAME_BUFFER_KERNEL:
		{
			VPU_4K_D2_SET_BUFFER_t *arg =
			    (VPU_4K_D2_SET_BUFFER_t *) args;

			dprintk(
			"Dec :: VPU_4K_D2_DEC_REG_FRAME_BUFFER in :: 0x%x/0x%x",
			    arg->gsV4kd2DecBuffer.m_FrameBufferStartAddr[0],
			    arg->gsV4kd2DecBuffer.m_FrameBufferStartAddr[1]);

			ret =
			    tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			    (codec_handle_t *)&pHandle,
			    (void *)(&arg->gsV4kd2DecBuffer),
			    (void *)NULL);
			dprintk
			    ("@@ Dec :: VPU_4K_D2_DEC_REG_FRAME_BUFFER out");
		}
		break;

		case VPU_DEC_DECODE:
		case VPU_DEC_DECODE_KERNEL:
		{
			VPU_4K_D2_DECODE_t *arg =
			    (VPU_4K_D2_DECODE_t *) args;

			int hiding_suferframe =
			    arg->gsV4kd2DecInput.m_Reserved[20];
#ifdef CONFIG_VPU_TIME_MEASUREMENT
			do_gettimeofday(&t1);
#endif
rinse_repeat:
			vmgr_4k_d2_data.szFrame_Len =
			    arg->gsV4kd2DecInput.m_iBitstreamDataSize;
			dprintk(
			"Dec: Dec In => 0x%x - 0x%x, 0x%x, 0x%x - 0x%x, %d, flag: %d",
			    arg->gsV4kd2DecInput.m_BitstreamDataAddr[PA],
			    arg->gsV4kd2DecInput.m_BitstreamDataAddr[VA],
			    arg->gsV4kd2DecInput.m_iBitstreamDataSize,
			    arg->gsV4kd2DecInput.m_UserDataAddr[PA],
			    arg->gsV4kd2DecInput.m_UserDataAddr[VA],
			    arg->gsV4kd2DecInput.m_iUserDataBufferSize,
			    arg->gsV4kd2DecInput.m_iSkipFrameMode);

			vmgr_4k_d2_data.check_interrupt_detection = 1;
			ret = tcc_vpu_4k_d2_dec(
				cmd & ~VPU_BASE_OP_KERNEL,
				(codec_handle_t *)&pHandle,
				(void *)&arg->gsV4kd2DecInput,
				(void *)&arg->gsV4kd2DecOutput);

			dprintk("Dec: Dec Out => %d - %d - %d, %d - %d - %d",
			  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayWidth,
			  arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_DisplayCropInfo.m_iCropLeft,
			  arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_DisplayCropInfo.m_iCropRight,
			  arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_iDisplayHeight,
			  arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_DisplayCropInfo.m_iCropTop,
			  arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_DisplayCropInfo.m_iCropBottom);

			dprintk(
			"Dec: Dec Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], OutStatus[%d/%d], POC[%d/%d]",
			  ret,
			  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iPicType,
			  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx,
			  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx,
			  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iOutputStatus,
			  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodingStatus,
			  arg->gsV4kd2DecOutput.m_DecOutInfo.m_Reserved[5],
			  arg->gsV4kd2DecOutput.m_DecOutInfo.m_Reserved[6]);

			switch (arg->gsV4kd2DecOutput.m_DecOutInfo
			.m_iDecodingStatus) {
			case VPU_DEC_BUF_FULL:
				err("[%d] Buffer full", type);
				break;

			case VPU_DEC_VP9_SUPER_FRAME:
				dprintk("Super Frame: sub-frame num: %u",
				   arg->gsV4kd2DecOutput.m_DecOutInfo
				     .m_SuperFrameInfo.m_uiNframes);
				if (hiding_suferframe == 20
					&& arg->gsV4kd2DecOutput.m_DecOutInfo
					   .m_SuperFrameInfo.m_uiNframes <= 2
					&& arg->gsV4kd2DecOutput
					   .m_DecOutInfo.m_iDispOutIdx < 0) {
					goto rinse_repeat;
				}
				break;
			default:
				break;
			}

			vmgr_4k_d2_data.nDecode_Cmd++;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			do_gettimeofday(&t2);
#endif

			vmgr_4k_d2_change_clock(
			    arg->gsV4kd2DecOutput.m_DecOutInfo
				    .m_iDecodedWidth,
			    arg->gsV4kd2DecOutput.m_DecOutInfo
				    .m_iDecodedHeight);
		}
		break;

		case VPU_DEC_GET_OUTPUT_INFO:
		case VPU_DEC_GET_OUTPUT_INFO_KERNEL:
		{
			VPU_4K_D2_DECODE_t *arg =
			    (VPU_4K_D2_DECODE_t *) args;

			dprintk("Dec: VPU_DEC_GET_OUTPUT_INFO");
			ret =
			  tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			  (codec_handle_t *)&pHandle, (void *)arg,
			  (void *)&arg->gsV4kd2DecOutput);

			dprintk("Dec: Dec Out => %d - %d - %d, %d - %d - %d",
			    arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayWidth,
			    arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_DisplayCropInfo.m_iCropLeft,
			    arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_DisplayCropInfo.m_iCropRight,
			    arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_iDisplayHeight,
			    arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_DisplayCropInfo.m_iCropTop,
			    arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_DisplayCropInfo.m_iCropBottom);

			dprintk(
			"Dec: Dec Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], OutStatus[%d/%d]",
			    ret,
			    arg->gsV4kd2DecOutput.m_DecOutInfo.m_iPicType,
			    arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx,
			    arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx,
			    arg->gsV4kd2DecOutput.m_DecOutInfo.m_iOutputStatus,
			    arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_iDecodingStatus);

			dprintk(
			"Dec: Dec Out => dec_Idx(%d), 0x%x 0x%x 0x%x / 0x%x 0x%x 0x%x",
			    arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx,
			    arg->gsV4kd2DecOutput.m_pDispOut[PA][0],
			    arg->gsV4kd2DecOutput.m_pDispOut[PA][1],
			    arg->gsV4kd2DecOutput.m_pDispOut[PA][2],
			    arg->gsV4kd2DecOutput.m_pDispOut[VA][0],
			    arg->gsV4kd2DecOutput.m_pDispOut[VA][1],
			    arg->gsV4kd2DecOutput.m_pDispOut[VA][2]);

			dprintk(
			"Dec: Dec Out => disp_Idx(%d), 0x%x 0x%x 0x%x / 0x%x 0x%x 0x%x",
			    arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx,
			    arg->gsV4kd2DecOutput.m_pCurrOut[PA][0],
			    arg->gsV4kd2DecOutput.m_pCurrOut[PA][1],
			    arg->gsV4kd2DecOutput.m_pCurrOut[PA][2],
			    arg->gsV4kd2DecOutput.m_pCurrOut[VA][0],
			    arg->gsV4kd2DecOutput.m_pCurrOut[VA][1],
			    arg->gsV4kd2DecOutput.m_pCurrOut[VA][2]);

			if (arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodingStatus
				== VPU_DEC_BUF_FULL)
				err("%d: Buffer full", type);
		}
		break;

		case VPU_DEC_BUF_FLAG_CLEAR:
		case VPU_DEC_BUF_FLAG_CLEAR_KERNEL:
		{
			int *arg = (int *)args;

			dprintk("Dec :: DispIdx Clear %d", *arg);
			ret =
			  tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			    (codec_handle_t *)&pHandle, (void *)(arg),
			    (void *)NULL);
		}
		break;

		case VPU_DEC_FLUSH_OUTPUT:
		case VPU_DEC_FLUSH_OUTPUT_KERNEL:
		{
			VPU_4K_D2_DECODE_t *arg =
			    (VPU_4K_D2_DECODE_t *)args;

			dprintk("Dec :: VPU_4K_D2_DEC_FLUSH_OUTPUT !!");
			ret =
			  tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			    (codec_handle_t *)&pHandle,
			    (void *)&arg->gsV4kd2DecInput,
			    (void *)&arg->gsV4kd2DecOutput);
		}
		break;

		case VPU_DEC_CLOSE:
		case VPU_DEC_CLOSE_KERNEL:
		{
			ret =
			  tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			    (codec_handle_t *)&pHandle, (void *)NULL,
			    (void *)NULL
			    /*(&arg->gsV4kd2DecOutput)*/
			    );
			V_DBG(VPU_DBG_SEQUENCE,
				"Dec :: VPU_4K_D2_DEC_CLOSED!!");
			vmgr_4k_d2_set_close(type, 1, 1);
		}
		break;

		case GET_RING_BUFFER_STATUS:
		case GET_RING_BUFFER_STATUS_KERNEL:
		{
			VPU_4K_D2_RINGBUF_GETINFO_t *arg =
			    (VPU_4K_D2_RINGBUF_GETINFO_t *)args;

			ret =
			    tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			    (codec_handle_t *)&pHandle, (void *)NULL,
			    (void *)&arg->gsV4kd2DecRingStatus);
		}
		break;

		case FILL_RING_BUFFER_AUTO:
		case FILL_RING_BUFFER_AUTO_KERNEL:
		{
			VPU_4K_D2_RINGBUF_SETBUF_t *arg =
			    (VPU_4K_D2_RINGBUF_SETBUF_t *)args;

			ret =
			    tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			    (codec_handle_t *)&pHandle,
			    (void *)&arg->gsV4kd2DecInit,
			    (void *)&arg->gsV4kd2DecRingFeed);
			dprintk
			("Dec :: ReadPTR : 0x%08x, WritePTR : 0x%08x",
			    vetc_reg_read(vmgr_4k_d2_data.base_addr,
			    0x120),
			    vetc_reg_read(vmgr_4k_d2_data.base_addr,
			    0x124));
		}
		break;

		case VPU_UPDATE_WRITE_BUFFER_PTR:
		case VPU_UPDATE_WRITE_BUFFER_PTR_KERNEL:
		{
			VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t *arg =
			    (VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t *)args;

			vmgr_4k_d2_data.check_interrupt_detection = 1;
			ret =
			    tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			      (codec_handle_t *)&pHandle,
			      (void *)(arg->iCopiedSize),
			      (void *)(arg->iFlushBuf));
		}
		break;

		case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY:
		case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_KERNEL:
		{
			VPU_4K_D2_SEQ_HEADER_t *arg =
			    (VPU_4K_D2_SEQ_HEADER_t *)args;

			ret =
			    tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			     (codec_handle_t *)&pHandle,
			     (void *)(&arg->gsV4kd2DecInitialInfo), NULL);
		}
		break;

		case VPU_CODEC_GET_VERSION:
		case VPU_CODEC_GET_VERSION_KERNEL:
		{
			VPU_4K_D2_GET_VERSION_t *arg =
			    (VPU_4K_D2_GET_VERSION_t *)args;

			ret =
			    tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
			      (codec_handle_t *)&pHandle,
			      arg->pszVersion, arg->pszBuildData);
			dprintk("Dec: version : %s, build : %s",
				arg->pszVersion, arg->pszBuildData);
		}
		break;

		case VPU_DEC_SWRESET:
		case VPU_DEC_SWRESET_KERNEL:
			ret =
			    tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
					(codec_handle_t *)&pHandle, NULL, NULL);
			break;

		default:
			err("Dec: not supported command(0x%x)", cmd);
			return 0x999;
		}
	}
#if DEFINED_CONFIG_VENC_CNT_12345678
	else {
		err(
		"Enc[%d]: Encoder for VPU-4K-D2 VP9/HEVC do not support. command(0x%x)",
			type, cmd);
		return 0x999;
	}
#endif

#ifdef CONFIG_VPU_TIME_MEASUREMENT
	time_gap_ms = vetc_GetTimediff_ms(t1, t2);

	if (cmd == VPU_DEC_DECODE || cmd == VPU_ENC_ENCODE) {
		vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt++;
		vmgr_4k_d2_data.iTime[type].proc_time[vmgr_4k_d2_data
		 .iTime[type].proc_base_cnt] = time_gap_ms;
		vmgr_4k_d2_data.iTime[type].proc_time_30frames += time_gap_ms;
		vmgr_4k_d2_data.iTime[type].accumulated_proc_time +=
		    time_gap_ms;

		if (vmgr_4k_d2_data.iTime[type].proc_base_cnt != 0
		    && vmgr_4k_d2_data.iTime[type].proc_base_cnt % 29 == 0) {
			V_DBG(VPU_DBG_PERF,
			"VPU-4K-D2 VP9/HEVC[%d] :: Dec[%4d] time %2d.%2d / %2d.%2d ms: %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d",
			  type, vmgr_4k_d2_data.iTime[type].print_out_index,
			  vmgr_4k_d2_data.iTime[type].proc_time_30frames /
			   30,
			   ((vmgr_4k_d2_data.iTime[type].proc_time_30frames %
			   30) * 100) / 30,
			  vmgr_4k_d2_data.iTime[type].accumulated_proc_time /
			   vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt,
			  ((vmgr_4k_d2_data.iTime[type].accumulated_proc_time
			   %vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt)
			   *100) /
			  vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt,
			  vmgr_4k_d2_data.iTime[type].proc_time[0],
			  vmgr_4k_d2_data.iTime[type].proc_time[1],
			  vmgr_4k_d2_data.iTime[type].proc_time[2],
			  vmgr_4k_d2_data.iTime[type].proc_time[3],
			  vmgr_4k_d2_data.iTime[type].proc_time[4],
			  vmgr_4k_d2_data.iTime[type].proc_time[5],
			  vmgr_4k_d2_data.iTime[type].proc_time[6],
			  vmgr_4k_d2_data.iTime[type].proc_time[7],
			  vmgr_4k_d2_data.iTime[type].proc_time[8],
			  vmgr_4k_d2_data.iTime[type].proc_time[9],
			  vmgr_4k_d2_data.iTime[type].proc_time[10],
			  vmgr_4k_d2_data.iTime[type].proc_time[11],
			  vmgr_4k_d2_data.iTime[type].proc_time[12],
			  vmgr_4k_d2_data.iTime[type].proc_time[13],
			  vmgr_4k_d2_data.iTime[type].proc_time[14],
			  vmgr_4k_d2_data.iTime[type].proc_time[15],
			  vmgr_4k_d2_data.iTime[type].proc_time[16],
			  vmgr_4k_d2_data.iTime[type].proc_time[17],
			  vmgr_4k_d2_data.iTime[type].proc_time[18],
			  vmgr_4k_d2_data.iTime[type].proc_time[19],
			  vmgr_4k_d2_data.iTime[type].proc_time[20],
			  vmgr_4k_d2_data.iTime[type].proc_time[21],
			  vmgr_4k_d2_data.iTime[type].proc_time[22],
			  vmgr_4k_d2_data.iTime[type].proc_time[23],
			  vmgr_4k_d2_data.iTime[type].proc_time[24],
			  vmgr_4k_d2_data.iTime[type].proc_time[25],
			  vmgr_4k_d2_data.iTime[type].proc_time[26],
			  vmgr_4k_d2_data.iTime[type].proc_time[27],
			  vmgr_4k_d2_data.iTime[type].proc_time[28],
			  vmgr_4k_d2_data.iTime[type].proc_time[29]);
			vmgr_4k_d2_data.iTime[type].proc_base_cnt = 0;
			vmgr_4k_d2_data.iTime[type].proc_time_30frames = 0;
			vmgr_4k_d2_data.iTime[type].print_out_index++;
		} else {
			vmgr_4k_d2_data.iTime[type].proc_base_cnt++;
		}
	}
#endif

	return ret;
}

static int _vmgr_4k_d2_proc_exit_by_external(struct VpuList *list, int *result,
					     unsigned int type)
{
	if (!vmgr_4k_d2_get_close(type)
	    && vmgr_4k_d2_data.handle[type] != 0x00) {
		list->type = type;

		if (type >= VPU_ENC)
			list->cmd_type = VPU_ENC_CLOSE;
		else
			list->cmd_type = VPU_DEC_CLOSE;

		list->handle = vmgr_4k_d2_data.handle[type];
		list->args = NULL;
		list->comm_data = NULL;
		list->vpu_result = result;

		err("%s for %d!!", __func__, type);

		vmgr_4k_d2_list_manager(list, LIST_ADD);

		return 1;
	}

	return 0;
}

static void _vmgr_4k_d2_wait_process(int wait_ms)
{
	int max_count = wait_ms / 20;

	while (vmgr_4k_d2_data.cmd_processing) {
		max_count--;
		msleep(20);

		if (max_count <= 0) {
			err("cmd_processing(cmd %d) didn't finish!!",
			    vmgr_4k_d2_data.current_cmd);
			break;
		}
	}
}

static int _vmgr_4k_d2_external_all_close(int wait_ms)
{
	int type;
	int max_count;
	int ret;

	for (type = 0; type < VPU_4K_D2_MAX; type++) {
		if (_vmgr_4k_d2_proc_exit_by_external
		    (&vmgr_4k_d2_data.vList[type], &ret, type)) {
			max_count = wait_ms / 10;

			while (!vmgr_4k_d2_get_close(type)) {
				max_count--;
				usleep_range(0, 1000);	//msleep(10);
			}
		}
	}

	return 0;
}

static int _vmgr_4k_d2_cmd_open(char *str)
{
	int ret = 0;

	dprintk("======> _vmgr_4k_d2_%s_open In!! %d'th",
		str, atomic_read(&vmgr_4k_d2_data.dev_opened));

	vmgr_4k_d2_enable_clock(0, 0);

	if (atomic_read(&vmgr_4k_d2_data.dev_opened) == 0) {
#ifdef FORCED_ERROR
		forced_error_count = FORCED_ERR_CNT;
#endif
#if DEFINED_CONFIG_VENC_CNT_12345678
		vmgr_4k_d2_data.only_decmode = 0;
#else
		vmgr_4k_d2_data.only_decmode = 1;
#endif
		vmgr_4k_d2_data.clk_limitation = 1;
		vmgr_4k_d2_data.cmd_processing = 0;

		vmgr_4k_d2_hw_reset();
		vmgr_4k_d2_enable_irq(vmgr_4k_d2_data.irq);

		ret = vmem_init();
		if (ret < 0)
			err("failed to allocate memory for VPU_4K_D2!! %d",
				ret);

		cntInt_4kd2 = 0;
		#ifdef DEBUG_VPU_4K_D2_K
		cntwk_4kd2 = 0;
		#endif
	}
	atomic_inc(&vmgr_4k_d2_data.dev_opened);

	dprintk("======> _vmgr_4k_d2_%s_open Out!! %d'th",
		str, atomic_read(&vmgr_4k_d2_data.dev_opened));

	return 0;
}

static int _vmgr_4k_d2_cmd_release(char *str)
{
	V_DBG(VPU_DBG_CLOSE, "======> _vmgr_4k_d2_%s_release In!! %d'th", str,
		&vmgr_4k_d2_data.dev_opened);

	if (atomic_read(&vmgr_4k_d2_data.dev_opened) > 0)
		atomic_dec(&vmgr_4k_d2_data.dev_opened);

	if (atomic_read(&vmgr_4k_d2_data.dev_opened) == 0) {
		int type = 0;
		int alive_cnt = 0;

#if 1 // To close whole vpu-4k-d2 vp9/hevc instance
		//when being killed process opened this.
		if (!vmgr_4k_d2_data.bVpu_already_proc_force_closed) {
			vmgr_4k_d2_data.external_proc = 1;
			_vmgr_4k_d2_external_all_close(200);
			vmgr_4k_d2_data.external_proc = 0;
		}
		vmgr_4k_d2_data.bVpu_already_proc_force_closed = false;
#endif

		for (type = 0; type < VPU_4K_D2_MAX; type++) {
			if (vmgr_4k_d2_data.closed[type] == 0)
				alive_cnt++;
		}

		if (alive_cnt)
			V_DBG(VPU_DBG_CLOSE,
				"VPU-4K-D2 VP9/HEVC might be cleared by force.");

		atomic_set(&vmgr_4k_d2_data.oper_intr, 0);
		vmgr_4k_d2_data.cmd_processing = 0;

		_vmgr_4k_d2_close_all(1);

		vmgr_4k_d2_disable_irq(vmgr_4k_d2_data.irq);
		vmgr_4k_d2_BusPrioritySetting(BUS_FOR_NORMAL, 0);

		vmem_deinit();

		vmgr_4k_d2_hw_assert();
		udelay(1000); //1ms
	}

	vmgr_4k_d2_disable_clock(0, 0);

	vmgr_4k_d2_data.nOpened_Count++;

	V_DBG(VPU_DBG_CLOSE,
	"======> _vmgr_4k_d2_%s_release Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d)",
		str, atomic_read(&vmgr_4k_d2_data.dev_opened),
		vmgr_4k_d2_data.nOpened_Count,
		vmgr_4k_d2_get_close(VPU_DEC),
		vmgr_4k_d2_get_close(VPU_DEC_EXT),
		vmgr_4k_d2_get_close(VPU_DEC_EXT2),
		vmgr_4k_d2_get_close(VPU_DEC_EXT3),
		vmgr_4k_d2_get_close(VPU_DEC_EXT4));

	return 0;
}

static unsigned int hangup_rel_count;	// = 0;
static long _vmgr_4k_d2_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	int ret = 0;
	CONTENTS_INFO info;
	OPENED_sINFO open_info;

	mutex_lock(&vmgr_4k_d2_data.comm_data.io_mutex);

	switch (cmd) {
	case VPU_SET_CLK:
	case VPU_SET_CLK_KERNEL: {
		if (cmd == VPU_SET_CLK_KERNEL) {
			(void)memcpy(&info, (CONTENTS_INFO *)arg, sizeof(info));
		} else {
			if (copy_from_user(
			    &info, (CONTENTS_INFO *)arg, sizeof(info)))
				ret = -EFAULT;
		}
	}
	break;

	case VPU_GET_FREEMEM_SIZE:
	case VPU_GET_FREEMEM_SIZE_KERNEL:
	{
		unsigned int type;
		unsigned int freemem_sz;

		if (cmd == VPU_GET_FREEMEM_SIZE_KERNEL) {
			(void)memcpy(&type, (unsigned int *)arg,
				sizeof(unsigned int));
		} else {
			if (copy_from_user(&type,
				(unsigned int *)arg,
				sizeof(unsigned int)))
				ret = -EFAULT;
		}

		if (ret == 0) {
			if (type > VPU_MAX)
				type = VPU_DEC;
			freemem_sz = vmem_get_freemem_size(type);

			if (cmd == VPU_GET_FREEMEM_SIZE_KERNEL) {
				(void)memcpy((unsigned int *)arg,
					&freemem_sz,
					sizeof(unsigned int));
			} else {
				if (copy_to_user((unsigned int *)arg,
					&freemem_sz,
					sizeof(unsigned int)))
					ret = -EFAULT;
			}
		}
	}
	break;

	case VPU_HW_RESET:
		vmgr_4k_d2_hw_reset();
	break;

	case VPU_SET_MEM_ALLOC_MODE:
	case VPU_SET_MEM_ALLOC_MODE_KERNEL:
	{
		if (cmd == VPU_SET_MEM_ALLOC_MODE_KERNEL) {
			(void)memcpy(&open_info, (OPENED_sINFO *)arg,
				sizeof(OPENED_sINFO));
		} else {
			if (copy_from_user(&open_info, (OPENED_sINFO *)arg,
				sizeof(OPENED_sINFO)))
				ret = -EFAULT;
		}

		if (ret == 0) {
			if (open_info.opened_cnt != 0)
				vmem_set_only_decode_mode(open_info.type);
		}
	}
	break;

	case VPU_CHECK_CODEC_STATUS:
	case VPU_CHECK_CODEC_STATUS_KERNEL:
	{
		if (cmd == VPU_CHECK_CODEC_STATUS_KERNEL) {
			(void)memcpy((int *)arg, vmgr_4k_d2_data.closed,
				sizeof(vmgr_4k_d2_data.closed));
		} else {
			if (copy_to_user((int *)arg, vmgr_4k_d2_data.closed,
				sizeof(vmgr_4k_d2_data.closed)))
				ret = -EFAULT;
		}
	}
	break;

	case VPU_GET_INSTANCE_IDX:
	case VPU_GET_INSTANCE_IDX_KERNEL:
	{
		INSTANCE_INFO iInst;

		if (cmd == VPU_GET_INSTANCE_IDX_KERNEL) {
			(void)memcpy(&iInst, (int *)arg,
				sizeof(INSTANCE_INFO));
		} else {
			if (copy_from_user(&iInst, (int *)arg,
				sizeof(INSTANCE_INFO)))
				ret = -EFAULT;
		}

		if (ret == 0) {
			if (iInst.type == VPU_ENC)
				venc_get_instance(&iInst.nInstance);
			else
				vdec_get_instance(&iInst.nInstance);

			if (cmd == VPU_GET_INSTANCE_IDX_KERNEL) {
				(void)memcpy((int *)arg, &iInst,
					sizeof(INSTANCE_INFO));
			} else {
				if (copy_to_user((int *)arg, &iInst,
					sizeof(INSTANCE_INFO)))
					ret = -EFAULT;
			}
		}
	}
	break;

	case VPU_CLEAR_INSTANCE_IDX:
	case VPU_CLEAR_INSTANCE_IDX_KERNEL:
	{
		INSTANCE_INFO iInst;

		if (cmd == VPU_CLEAR_INSTANCE_IDX_KERNEL) {
			(void)memcpy(&iInst, (int *)arg,
				sizeof(INSTANCE_INFO));
		} else  {
			if (copy_from_user(&iInst, (int *)arg,
				sizeof(INSTANCE_INFO)))
				ret = -EFAULT;
		}

		if (ret == 0) {
			if (iInst.type == VPU_ENC)
				venc_clear_instance(iInst.nInstance);
			else
				vdec_clear_instance(iInst.nInstance);
		}
	}
	break;

	case VPU_TRY_FORCE_CLOSE:
	case VPU_TRY_FORCE_CLOSE_KERNEL:
	{
		if (!vmgr_4k_d2_data.bVpu_already_proc_force_closed) {
			vmgr_4k_d2_data.external_proc = 1;
			_vmgr_4k_d2_external_all_close(200);
			vmgr_4k_d2_data.external_proc = 0;
			vmgr_4k_d2_data.bVpu_already_proc_force_closed
			= true;
		}
	}
	break;

	case VPU_TRY_CLK_RESTORE:
	case VPU_TRY_CLK_RESTORE_KERNEL:
	{
		vmgr_4k_d2_restore_clock(0,
		atomic_read(&vmgr_4k_d2_data.dev_opened));
	}
	break;

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	case VPU_TRY_OPEN_DEV:
	case VPU_TRY_OPEN_DEV_KERNEL:
		_vmgr_4k_d2_cmd_open("cmd");
		break;

	case VPU_TRY_CLOSE_DEV:
	case VPU_TRY_CLOSE_DEV_KERNEL:
		_vmgr_4k_d2_cmd_release("cmd");
		break;
#endif

	case VPU_TRY_HANGUP_RELEASE:
		hangup_rel_count++;
		V_DBG(VPU_DBG_SEQUENCE,
			" vpu_4k_d2 ===> VPU_TRY_HANGUP_RELEASE %d'th",
			hangup_rel_count);
		break;

#ifdef DEBUG_VPU_4K_D2_K
	case VPU_DEBUG_ISR:
		vpu_4k_d2_isr_param_debug.vpu_k_isr_cnt_hit
		= cntInt_4kd2;
		vpu_4k_d2_isr_param_debug.wakeup_interrupt_cnt
		= cntwk_4kd2;
		if (copy_to_user((void *)arg,
			&vpu_4k_d2_isr_param_debug,
			sizeof(struct debug_4k_d2_k_isr_t)))
			ret = -EFAULT;
		break;
#endif
	default:
		err("Unsupported ioctl[%d]!!!", cmd);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&vmgr_4k_d2_data.comm_data.io_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long _vmgr_4k_d2_compat_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	return _vmgr_4k_d2_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static irqreturn_t _vmgr_4k_d2_isr_handler(int irq, void *dev_id)
{
	unsigned long flags;
	unsigned int reason;

	cntInt_4kd2++;

	if (cntInt_4kd2 % 30 == 0)
		detailk("Interrupt cnt: %d", cntInt_4kd2);


	reason =
		vetc_reg_read(vmgr_4k_d2_data.base_addr,
			0x004C/*W5_VPU_VINT_REASON*/);

	atomic_or(reason, &vmgr_4k_d2_data.oper_intr);
#ifdef DEBUG_VPU_4K_D2_K
	cntwk_4kd2++;
#endif
	wake_up_interruptible(&vmgr_4k_d2_data.oper_wq);

	return IRQ_HANDLED;
}

static int _vmgr_4k_d2_open(struct inode *inode, struct file *filp)
{
	if (!vmgr_4k_d2_data.irq_reged)
		err("not registered vpu-4k-d2 vp9/hevc-mgr-irq");

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	dprintk("enter!! %d'th",
		atomic_read(&vmgr_4k_d2_data.dev_file_opened));
	atomic_inc(&vmgr_4k_d2_data.dev_file_opened);
	dprintk("Out!! %d'th",
		atomic_read(&vmgr_4k_d2_data.dev_file_opened));
#else
	mutex_lock(&vmgr_4k_d2_data.comm_data.file_mutex);
	_vmgr_4k_d2_cmd_open("file");
	mutex_unlock(&vmgr_4k_d2_data.comm_data.file_mutex);
#endif

	filp->private_data = &vmgr_4k_d2_data;
	return 0;
}

static int _vmgr_4k_d2_release(struct inode *inode, struct file *filp)
{
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	dprintk("enter!! %d'th",
		atomic_read(&vmgr_4k_d2_data.dev_file_opened));
	atomic_dec(&vmgr_4k_d2_data.dev_file_opened);
	vmgr_4k_d2_data.nOpened_Count++;

	V_DBG(VPU_DBG_CLOSE, "Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d)",
	     atomic_read(&vmgr_4k_d2_data.dev_file_opened),
	     vmgr_4k_d2_data.nOpened_Count,
	     vmgr_4k_d2_get_close(VPU_DEC),
	     vmgr_4k_d2_get_close(VPU_DEC_EXT),
	     vmgr_4k_d2_get_close(VPU_DEC_EXT2),
	     vmgr_4k_d2_get_close(VPU_DEC_EXT3),
	     vmgr_4k_d2_get_close(VPU_DEC_EXT4));
#else
	mutex_lock(&vmgr_4k_d2_data.comm_data.file_mutex);
	_vmgr_4k_d2_cmd_release("file");
	mutex_unlock(&vmgr_4k_d2_data.comm_data.file_mutex);
#endif

	return 0;
}

struct VpuList *vmgr_4k_d2_list_manager(struct VpuList *args, unsigned int cmd)
{
	struct VpuList *ret = NULL;
	struct VpuList *oper_data = (struct VpuList *) args;

	if (!oper_data) {
		if (cmd == LIST_ADD || cmd == LIST_DEL) {
			V_DBG(VPU_DBG_ERROR, "Data is null, cmd=%d", cmd);
			return NULL;
		}
	}

	if (cmd == LIST_ADD)
		*oper_data->vpu_result = RET0;

	mutex_lock(&vmgr_4k_d2_data.comm_data.list_mutex);

	switch (cmd) {
	case LIST_ADD:
		*oper_data->vpu_result |= RET1;
		list_add_tail(&oper_data->list,
			&vmgr_4k_d2_data.comm_data.main_list);
		vmgr_4k_d2_data.cmd_queued++;
		vmgr_4k_d2_data.comm_data.thread_intr++;
		break;
	case LIST_DEL:
		list_del(&oper_data->list);
		vmgr_4k_d2_data.cmd_queued--;
		break;
	case LIST_IS_EMPTY:
		if (list_empty(&vmgr_4k_d2_data.comm_data.main_list))
			ret = (struct VpuList *)0x1234;
		break;
	case LIST_GET_ENTRY:
		ret = list_first_entry(
				&vmgr_4k_d2_data.comm_data.main_list,
				struct VpuList, list);
		break;
	}

	mutex_unlock(&vmgr_4k_d2_data.comm_data.list_mutex);

	if (cmd == LIST_ADD)
		wake_up_interruptible(&vmgr_4k_d2_data.comm_data.thread_wq);

	return ret;
}

static int _vmgr_4k_d2_operation(void)
{
	int oper_finished;
	struct VpuList *oper_data = NULL;

	while (!vmgr_4k_d2_list_manager(NULL, LIST_IS_EMPTY)) {
		vmgr_4k_d2_data.cmd_processing = 1;
		oper_finished = 1;

		dprintk("%s :: not empty cmd_queued(%d)",
			__func__, vmgr_4k_d2_data.cmd_queued);

		oper_data =
			(struct VpuList *) vmgr_4k_d2_list_manager(
						NULL, LIST_GET_ENTRY);
		if (!oper_data) {
			err("data is null");
			vmgr_4k_d2_data.cmd_processing = 0;
			return 0;
		}

		*oper_data->vpu_result |= RET2;

		dprintk(
		"%s [%d] :: cmd = 0x%x, vmgr_4k_d2_data.cmd_queued(%d)",
		__func__, oper_data->type, oper_data->cmd_type,
		vmgr_4k_d2_data.cmd_queued);

		if (oper_data->type < VPU_4K_D2_MAX) {
			*oper_data->vpu_result |= RET3;

			*oper_data->vpu_result =
			    _vmgr_4k_d2_process(oper_data->type,
					oper_data->cmd_type,
					oper_data->handle,
					oper_data->args);
			oper_finished = 1;
			if (*oper_data->vpu_result != RETCODE_SUCCESS) {
				if ((*oper_data->vpu_result
				    != RETCODE_INSUFFICIENT_BITSTREAM)
				    && (*oper_data->vpu_result !=
				    RETCODE_INSUFFICIENT_BITSTREAM_BUF)) {
					err(
					"vmgr_4k_d2_out[0x%x] :: type = %d, vmgr_4k_d2_data.handle = 0x%x, cmd = 0x%x, frame_len %d",
						*oper_data->vpu_result,
						oper_data->type,
						oper_data->handle,
						oper_data->cmd_type,
						vmgr_4k_d2_data.szFrame_Len);
				}

				if (*oper_data->vpu_result
				    == RETCODE_CODEC_EXIT) {
					vmgr_4k_d2_restore_clock(0,
					 atomic_read(
					 &vmgr_4k_d2_data.dev_opened));
					_vmgr_4k_d2_close_all(1);
				}
			}
		} else {
			err(
			"missed info or unknown command => type = 0x%x, cmd = 0x%x",
			 oper_data->type, oper_data->cmd_type);

			*oper_data->vpu_result = RETCODE_FAILURE;
			oper_finished = 0;
		}

		if (oper_finished) {
			if (oper_data->comm_data != NULL
			    && atomic_read(&vmgr_4k_d2_data.dev_opened) != 0) {
				oper_data->comm_data->count++;
				if (oper_data->comm_data->count != 1) {
					dprintk(
					"poll wakeup count = %d :: type(0x%x) cmd(0x%x)",
					    oper_data->comm_data->count,
					    oper_data->type,
					    oper_data->cmd_type);
				}

				wake_up_interruptible(
					&oper_data->comm_data->wq);
			} else {
				err(
				"Error: abnormal exception or external command was processed!! 0x%p - %d",
				  oper_data->comm_data,
				  atomic_read(&vmgr_4k_d2_data.dev_opened));
			}
		} else {
			err("Error: abnormal exception 2!! 0x%p - %d",
			    oper_data->comm_data,
			    atomic_read(&vmgr_4k_d2_data.dev_opened));
		}

		vmgr_4k_d2_list_manager(oper_data, LIST_DEL);

		vmgr_4k_d2_data.cmd_processing = 0;
	}

	return 0;
}

static int _vmgr_4k_d2_thread(void *kthread)
{
	V_DBG(VPU_DBG_THREAD, "enter");

	do {
		if (vmgr_4k_d2_list_manager(NULL, LIST_IS_EMPTY)) {
			vmgr_4k_d2_data.cmd_processing = 0;

			(void)wait_event_interruptible_timeout(
			    vmgr_4k_d2_data.comm_data.thread_wq,
			    vmgr_4k_d2_data.comm_data.thread_intr > 0,
			    msecs_to_jiffies(50));

			vmgr_4k_d2_data.comm_data.thread_intr = 0;
		} else {
			if (atomic_read(&vmgr_4k_d2_data.dev_opened)
			    || vmgr_4k_d2_data.external_proc) {
				_vmgr_4k_d2_operation();
			} else {
				struct VpuList *oper_data = NULL;

				err("DEL for empty");

				oper_data =
				 vmgr_4k_d2_list_manager(
				 NULL, LIST_GET_ENTRY);
				if (oper_data)
					vmgr_4k_d2_list_manager(
					oper_data, LIST_DEL);
			}
		}
	} while (!kthread_should_stop());

	V_DBG(VPU_DBG_THREAD, "out");

	return 0;
}

static int _vmgr_4k_d2_mmap(struct file *file, struct vm_area_struct *vma)
{
#if defined(CONFIG_TCC_MEM)
	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		err("this address is not allowed");
		return -EAGAIN;
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range
	    (vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start,
	     vma->vm_page_prot)) {
		err("remap_pfn_range failed");
		return -EAGAIN;
	}

	vma->vm_ops = NULL;
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_DONTEXPAND | VM_PFNMAP;

	return 0;
}

static const struct file_operations _vmgr_4k_d2_fops = {
	.open = _vmgr_4k_d2_open,
	.release = _vmgr_4k_d2_release,
	.mmap = _vmgr_4k_d2_mmap,
	.unlocked_ioctl = _vmgr_4k_d2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = _vmgr_4k_d2_compat_ioctl,
#endif
};

static struct miscdevice _vmgr_4k_d2_misc_device = {
	MISC_DYNAMIC_MINOR,
	VPU_4K_D2_MGR_NAME,
	&_vmgr_4k_d2_fops,
};

int vmgr_4k_d2_probe(struct platform_device *pdev)
{
	int ret;
	int type;
	unsigned long int_flags;
	struct resource *resource = NULL;

	if (pdev->dev.of_node == NULL)
		return -ENODEV;

	dprintk("hmgr initializing!!");
	memset(&vmgr_4k_d2_data, 0, sizeof(struct _mgr_data_t));
	for (type = 0; type < VPU_4K_D2_MAX; type++)
		vmgr_4k_d2_data.closed[type] = 1;

	vmgr_4k_d2_init_variable();
	atomic_set(&vmgr_4k_d2_data.oper_intr, 0);
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	atomic_set(&vmgr_4k_d2_data.dev_file_opened, 0);
#endif

	vmgr_4k_d2_data.irq = platform_get_irq(pdev, 0);
	vmgr_4k_d2_data.nOpened_Count = 0;
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		dev_err(&pdev->dev, "missing phy memory resource");
		return -1;
	}
	resource->end += 1;

	vmgr_4k_d2_data.base_addr
	= devm_ioremap(&pdev->dev, resource->start,
		resource->end - resource->start);
	dprintk(
	"============> VPU-4K-D2 VP9/HEVC base address [0x%x -> 0x%p], irq num [%d]",
		resource->start,
		vmgr_4k_d2_data.base_addr,
		vmgr_4k_d2_data.irq - 32);

	vmgr_4k_d2_get_clock(pdev->dev.of_node);
	vmgr_4k_d2_get_reset(pdev->dev.of_node);

	init_waitqueue_head(&vmgr_4k_d2_data.comm_data.thread_wq);
	init_waitqueue_head(&vmgr_4k_d2_data.oper_wq);

	mutex_init(&vmgr_4k_d2_data.comm_data.list_mutex);
	mutex_init(&(vmgr_4k_d2_data.comm_data.io_mutex));
	mutex_init(&(vmgr_4k_d2_data.comm_data.file_mutex));

	INIT_LIST_HEAD(&vmgr_4k_d2_data.comm_data.main_list);
	INIT_LIST_HEAD(&vmgr_4k_d2_data.comm_data.wait_list);

	ret = vmem_config();
	if (ret < 0) {
		err("unable to configure memory for VPU!! %d", ret);
		return -ENOMEM;
	}

	vmgr_4k_d2_init_interrupt();
	int_flags = vmgr_4k_d2_get_int_flags();
	ret = vmgr_4k_d2_request_irq(
			vmgr_4k_d2_data.irq, _vmgr_4k_d2_isr_handler,
			int_flags, VPU_4K_D2_MGR_NAME, &vmgr_4k_d2_data);
	if (ret)
		err("to aquire vpu-4k-d2-vp9/hevc-dec-irq");

	vmgr_4k_d2_data.irq_reged = 1;
	vmgr_4k_d2_disable_irq(vmgr_4k_d2_data.irq);

	kidle_task = kthread_run(_vmgr_4k_d2_thread, NULL, "v4K-D2_th");
	if (IS_ERR(kidle_task)) {
		err("unable to create thread!!");
		kidle_task = NULL;
		return -1;
	}
	dprintk("success :: thread created!!");

	_vmgr_4k_d2_close_all(1);

	if (misc_register(&_vmgr_4k_d2_misc_device)) {
		pr_info(
		       "VPU-4K-D2 VP9/HEVC Manager: Couldn't register device.");
		return -EBUSY;
	}

	vmgr_4k_d2_enable_clock(0, 1);
	vmgr_4k_d2_disable_clock(0, 1);

	return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_probe);

int vmgr_4k_d2_remove(struct platform_device *pdev)
{
	misc_deregister(&_vmgr_4k_d2_misc_device);

	if (kidle_task) {
		kthread_stop(kidle_task);
		kidle_task = NULL;
	}

	devm_iounmap(&pdev->dev, vmgr_4k_d2_data.base_addr);
	if (vmgr_4k_d2_data.irq_reged) {
		vmgr_4k_d2_free_irq(vmgr_4k_d2_data.irq, &vmgr_4k_d2_data);
		vmgr_4k_d2_data.irq_reged = 0;
	}

	vmgr_4k_d2_put_clock();
	vmgr_4k_d2_put_reset();
	vmem_deinit();

	pr_info("success :: hmgr thread stopped!!");

	return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_remove);

#if defined(CONFIG_PM)
int vmgr_4k_d2_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i, open_count = 0;

	if (atomic_read(&vmgr_4k_d2_data.dev_opened) != 0) {
		pr_info("\n vpu_4k_d2: suspend In DEC(%d/%d/%d/%d/%d)\n",
			vmgr_4k_d2_get_close(VPU_DEC),
			vmgr_4k_d2_get_close(VPU_DEC_EXT),
			vmgr_4k_d2_get_close(VPU_DEC_EXT2),
			vmgr_4k_d2_get_close(VPU_DEC_EXT3),
			vmgr_4k_d2_get_close(VPU_DEC_EXT4));

		_vmgr_4k_d2_external_all_close(200);

		open_count = atomic_read(&vmgr_4k_d2_data.dev_opened);

		for (i = 0; i < open_count; i++)
			vmgr_4k_d2_disable_clock(0, 0);

		pr_info("vpu_4k_d2: suspend Out DEC(%d/%d/%d/%d/%d)\n",
			vmgr_4k_d2_get_close(VPU_DEC),
			vmgr_4k_d2_get_close(VPU_DEC_EXT),
			vmgr_4k_d2_get_close(VPU_DEC_EXT2),
			vmgr_4k_d2_get_close(VPU_DEC_EXT3),
			vmgr_4k_d2_get_close(VPU_DEC_EXT4));
	}

	return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_suspend);

int vmgr_4k_d2_resume(struct platform_device *pdev)
{
	int i, open_count = 0;

	if (atomic_read(&vmgr_4k_d2_data.dev_opened) != 0) {
		open_count = atomic_read(&vmgr_4k_d2_data.dev_opened);

		for (i = 0; i < open_count; i++)
			vmgr_4k_d2_enable_clock(0, 0);

		pr_info("\n vpu_4k_d2: resume\n\n");
	}

	return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_resume);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu_4k_d2 vp9/hevc manager");
MODULE_LICENSE("GPL");

#endif
