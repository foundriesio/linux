/*
 *   FileName : vpu_4k_d2_mgr.c
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
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
 *
 */

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2

#include "vpu_4k_d2_mgr_sys.h"
#include <soc/tcc/pmap.h>
//#include <mach/tcc_fb.h>
#include <linux/of.h>
#include <linux/io.h>
#include "vpu_buffer.h"
#include "vpu_devices.h"
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/system_info.h>

#define dprintk(msg...) //printk( "TCC_4K_D2_VMGR: " msg);
#define detailk(msg...) //printk( "TCC_4K_D2_VMGR: " msg);
#define cmdk(msg...) //printk( "TCC_4K_D2_VMGR [Cmd]: " msg);
#define err(msg...) printk("TCC_4K_D2_VMGR [Err]: " msg);

#define VPU_4K_D2_REGISTER_DUMP
#define VPU_4K_D2_DUMP_STATUS
//#define VPU_4K_D2_REGISTER_INIT

//#define FORCED_ERROR //For test purpose!!
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

/////////////////////////////////////////////////////////////////////////////
// Control only once!!
static mgr_data_t vmgr_4k_d2_data;
static struct task_struct *kidle_task = NULL;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
extern void do_gettimeofday(struct timeval *tv);
#endif
extern int tcc_vpu_4k_d2_dec( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );
//extern int tcc_vpu_4k_d2_dec_esc( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );
//extern int tcc_vpu_4k_d2_dec_ext( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );

VpuList_t* vmgr_4k_d2_list_manager(VpuList_t* args, unsigned int cmd);
/////////////////////////////////////////////////////////////////////////////

int vmgr_4k_d2_opened(void)
{
    if(vmgr_4k_d2_data.dev_opened == 0)
        return 0;
    return 1;
}
EXPORT_SYMBOL(vmgr_4k_d2_opened);


#ifdef VPU_4K_D2_DUMP_STATUS
static unsigned int _vpu_4k_d2mgr_FIORead(unsigned int addr)
{
    unsigned int ctrl;
    unsigned int count = 0;
    unsigned int data  = 0xffffffff;

    ctrl  = (addr&0xffff);
    ctrl |= (0<<16);    /* read operation */
    vetc_reg_write(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_CTRL_ADDR, ctrl);
    count = 10000;
    while (count--) {
        ctrl = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_CTRL_ADDR);
        if (ctrl & 0x80000000) {
            data = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_DATA);
            break;
        }
    }

    return data;
}

static int _vpu_4k_d2mgr_FIOWrite(unsigned int addr, unsigned int data)
{
    unsigned int ctrl;

    vetc_reg_write(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_DATA, data);
    ctrl  = (addr&0xffff);
    ctrl |= (1<<16);    /* write operation */
    vetc_reg_write(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_CTRL_ADDR, ctrl);

    return 1;
}

static unsigned int _vpu_4k_d2mgr_ReadRegVCE(unsigned int vce_addr)
{
#define VCORE_DBG_ADDR              0x8300
#define VCORE_DBG_DATA              0x8304
#define VCORE_DBG_READY             0x8308
    int     vcpu_reg_addr;
    int     udata;
#if 0 // unused local variable
    //int     vce_core_base = 0x8000 + 0x1000*vce_core_idx;
    int     vce_core_base = 0x8000;
#endif

    _vpu_4k_d2mgr_FIOWrite(VCORE_DBG_READY, 0);

    vcpu_reg_addr = vce_addr >> 2;

    _vpu_4k_d2mgr_FIOWrite(VCORE_DBG_ADDR, vcpu_reg_addr + 0x8000);

    while (1) {
        if (_vpu_4k_d2mgr_FIORead(VCORE_DBG_READY) == 1) {
            udata= _vpu_4k_d2mgr_FIORead(VCORE_DBG_DATA);
            break;
        }
    }

    return udata;
}

static void _vpu_4k_d2_dump_status(void)
{
    int      rd, wr;
    unsigned int    tq, ip, mc, lf;
    unsigned int    avail_cu, avail_tu, avail_tc, avail_lf, avail_ip;
    unsigned int     ctu_fsm, nb_fsm, cabac_fsm, cu_info, mvp_fsm, tc_busy, lf_fsm, bs_data, bbusy, fv;
    unsigned int    reg_val;
    unsigned int    index;
    unsigned int    vcpu_reg[31]= {0,};

    printk("-------------------------------------------------------------------------------\n");
    printk("------                            VCPU STATUS                             -----\n");
    printk("-------------------------------------------------------------------------------\n");
    rd = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_BS_RD_PTR);
    wr = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_BS_WR_PTR);
    printk("RD_PTR: 0x%08x WR_PTR: 0x%08x BS_OPT: 0x%08x BS_PARAM: 0x%08x\n",
        rd, wr, vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_BS_OPTION), vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_CMD_BS_PARAM));

    // --------- VCPU register Dump
    printk("[+] VCPU REG Dump\n");
    for (index = 0; index < 25; index++) {
        vetc_reg_write(vmgr_4k_d2_data.base_addr, 0x14, (1<<9) | (index & 0xff));
        vcpu_reg[index] = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x1c);

        if (index < 16) {
            printk("0x%08x\t",  vcpu_reg[index]);
            if ((index % 4) == 3) printk("\n");
        }
        else {
            switch (index) {
                case 16: printk("CR0: 0x%08x\t", vcpu_reg[index]); break;
                case 17: printk("CR1: 0x%08x\n", vcpu_reg[index]); break;
                case 18: printk("ML:  0x%08x\t", vcpu_reg[index]); break;
                case 19: printk("MH:  0x%08x\n", vcpu_reg[index]); break;
                case 21: printk("LR:  0x%08x\n", vcpu_reg[index]); break;
                case 22: printk("PC:  0x%08x\n", vcpu_reg[index]);break;
                case 23: printk("SR:  0x%08x\n", vcpu_reg[index]);break;
                case 24: printk("SSP: 0x%08x\n", vcpu_reg[index]);break;
            }
        }
    }
    printk("[-] VCPU REG Dump\n");
    // --------- BIT register Dump
    printk("[+] BPU REG Dump\n");
    printk("BITPC = 0x%08x\n", _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x18)));
    printk("BIT START=0x%08x, BIT END=0x%08x\n", _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x11c)),
            _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x120)) );

    printk("CODE_BASE           %x \n", _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x7000 + 0x18)));
    printk("VCORE_REINIT_FLAG   %x \n", _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x7000 + 0x0C)));

    // --------- BIT HEVC Status Dump
    ctu_fsm     = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x48));
    nb_fsm      = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x4c));
    cabac_fsm   = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x50));
    cu_info     = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x54));
    mvp_fsm     = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x58));
    tc_busy     = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x5c));
    lf_fsm      = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x60));
    bs_data     = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x64));
    bbusy       = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x68));
    fv          = _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x6C));

    printk("[DEBUG-BPUHEVC] CTU_X: %4d, CTU_Y: %4d\n",  _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x40)), _vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x44)));
    printk("[DEBUG-BPUHEVC] CTU_FSM>   Main: 0x%02x, FIFO: 0x%1x, NB: 0x%02x, DBK: 0x%1x\n", ((ctu_fsm >> 24) & 0xff), ((ctu_fsm >> 16) & 0xff), ((ctu_fsm >> 8) & 0xff), (ctu_fsm & 0xff));
    printk("[DEBUG-BPUHEVC] NB_FSM: 0x%02x\n", nb_fsm & 0xff);
    printk("[DEBUG-BPUHEVC] CABAC_FSM> SAO: 0x%02x, CU: 0x%02x, PU: 0x%02x, TU: 0x%02x, EOS: 0x%02x\n", ((cabac_fsm>>25) & 0x3f), ((cabac_fsm>>19) & 0x3f), ((cabac_fsm>>13) & 0x3f), ((cabac_fsm>>6) & 0x7f), (cabac_fsm & 0x3f));
    printk("[DEBUG-BPUHEVC] CU_INFO value = 0x%04x \n\t\t(l2cb: 0x%1x, cux: %1d, cuy; %1d, pred: %1d, pcm: %1d, wr_done: %1d, par_done: %1d, nbw_done: %1d, dec_run: %1d)\n", cu_info,
            ((cu_info>> 16) & 0x3), ((cu_info>> 13) & 0x7), ((cu_info>> 10) & 0x7), ((cu_info>> 9) & 0x3), ((cu_info>> 8) & 0x1), ((cu_info>> 6) & 0x3), ((cu_info>> 4) & 0x3), ((cu_info>> 2) & 0x3), (cu_info & 0x3));
    printk("[DEBUG-BPUHEVC] MVP_FSM> 0x%02x\n", mvp_fsm & 0xf);
    printk("[DEBUG-BPUHEVC] TC_BUSY> tc_dec_busy: %1d, tc_fifo_busy: 0x%02x\n", ((tc_busy >> 3) & 0x1), (tc_busy & 0x7));
    printk("[DEBUG-BPUHEVC] LF_FSM>  SAO: 0x%1x, LF: 0x%1x\n", ((lf_fsm >> 4) & 0xf), (lf_fsm  & 0xf));
    printk("[DEBUG-BPUHEVC] BS_DATA> ExpEnd=%1d, bs_valid: 0x%03x, bs_data: 0x%03x\n", ((bs_data >> 31) & 0x1), ((bs_data >> 16) & 0xfff), (bs_data & 0xfff));
    printk("[DEBUG-BPUHEVC] BUS_BUSY> mib_wreq_done: %1d, mib_busy: %1d, sdma_bus: %1d\n", ((bbusy >> 2) & 0x1), ((bbusy >> 1) & 0x1) , (bbusy & 0x1));
    printk("[DEBUG-BPUHEVC] FIFO_VALID> cu: %1d, tu: %1d, iptu: %1d, lf: %1d, coff: %1d\n\n", ((fv >> 4) & 0x1), ((fv >> 3) & 0x1), ((fv >> 2) & 0x1), ((fv >> 1) & 0x1), (fv & 0x1));
    printk("[-] BPU REG Dump\n");

    // --------- VCE register Dump
    printk("[+] VCE REG Dump\n");
    tq = _vpu_4k_d2mgr_ReadRegVCE(0xd0);
    ip = _vpu_4k_d2mgr_ReadRegVCE(0xd4);
    mc = _vpu_4k_d2mgr_ReadRegVCE(0xd8);
    lf = _vpu_4k_d2mgr_ReadRegVCE(0xdc);
    avail_cu = (_vpu_4k_d2mgr_ReadRegVCE(0x11C)>>16) - (_vpu_4k_d2mgr_ReadRegVCE(0x110)>>16);
    avail_tu = (_vpu_4k_d2mgr_ReadRegVCE(0x11C)&0xFFFF) - (_vpu_4k_d2mgr_ReadRegVCE(0x110)&0xFFFF);
    avail_tc = (_vpu_4k_d2mgr_ReadRegVCE(0x120)>>16) - (_vpu_4k_d2mgr_ReadRegVCE(0x114)>>16);
    avail_lf = (_vpu_4k_d2mgr_ReadRegVCE(0x120)&0xFFFF) - (_vpu_4k_d2mgr_ReadRegVCE(0x114)&0xFFFF);
    avail_ip = (_vpu_4k_d2mgr_ReadRegVCE(0x124)>>16) - (_vpu_4k_d2mgr_ReadRegVCE(0x118)>>16);
    printk("       TQ            IP              MC             LF      GDI_EMPTY          ROOM \n");
    printk("------------------------------------------------------------------------------------------------------------\n");
    printk("| %d %04d %04d | %d %04d %04d |  %d %04d %04d | %d %04d %04d | 0x%08x | CU(%d) TU(%d) TC(%d) LF(%d) IP(%d)\n",
            (tq>>22)&0x07, (tq>>11)&0x3ff, tq&0x3ff,
            (ip>>22)&0x07, (ip>>11)&0x3ff, ip&0x3ff,
            (mc>>22)&0x07, (mc>>11)&0x3ff, mc&0x3ff,
            (lf>>22)&0x07, (lf>>11)&0x3ff, lf&0x3ff,
            _vpu_4k_d2mgr_FIORead(0x88f4),                      /* GDI empty */
            avail_cu, avail_tu, avail_tc, avail_lf, avail_ip);

    /* CU/TU Queue count */
    reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x12C);
    printk("[DCIDEBUG] QUEUE COUNT: CU(%5d) TU(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);
    reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x1A0);
    printk("TC(%5d) IP(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);
    reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x1A4);
    printk("LF(%5d)\n", (reg_val>>16)&0xffff);
    printk("VALID SIGNAL : CU0(%d)  CU1(%d)  CU2(%d) TU(%d) TC(%d) IP(%5d) LF(%5d)\n"
        "               DCI_FALSE_RUN(%d) VCE_RESET(%d) CORE_INIT(%d) SET_RUN_CTU(%d) \n",
        (reg_val>>6)&1, (reg_val>>5)&1, (reg_val>>4)&1, (reg_val>>3)&1,
        (reg_val>>2)&1, (reg_val>>1)&1, (reg_val>>0)&1,
        (reg_val>>10)&1, (reg_val>>9)&1, (reg_val>>8)&1, (reg_val>>7)&1);

    printk("State TQ: 0x%08x IP: 0x%08x MC: 0x%08x LF: 0x%08x\n",
        _vpu_4k_d2mgr_ReadRegVCE(0xd0), _vpu_4k_d2mgr_ReadRegVCE(0xd4), _vpu_4k_d2mgr_ReadRegVCE(0xd8), _vpu_4k_d2mgr_ReadRegVCE(0xdc));
    printk("BWB[1]: RESPONSE_CNT(0x%08x) INFO(0x%08x)\n", _vpu_4k_d2mgr_ReadRegVCE(0x194), _vpu_4k_d2mgr_ReadRegVCE(0x198));
    printk("BWB[2]: RESPONSE_CNT(0x%08x) INFO(0x%08x)\n", _vpu_4k_d2mgr_ReadRegVCE(0x194), _vpu_4k_d2mgr_ReadRegVCE(0x198));
    printk("DCI INFO\n");
    printk("READ_CNT_0 : 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x110));
    printk("READ_CNT_1 : 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x114));
    printk("READ_CNT_2 : 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x118));
    printk("WRITE_CNT_0: 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x11c));
    printk("WRITE_CNT_1: 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x120));
    printk("WRITE_CNT_2: 0x%08x\n", _vpu_4k_d2mgr_ReadRegVCE(0x124));
    reg_val = _vpu_4k_d2mgr_ReadRegVCE(0x128);
    printk("LF_DEBUG_PT: 0x%08x\n", reg_val & 0xffffffff);
    printk("cur_main_state %2d, r_lf_pic_deblock_disable %1d, r_lf_pic_sao_disable %1d\n",
        (reg_val >> 16) & 0x1f,
        (reg_val >> 15) & 0x1,
        (reg_val >> 14) & 0x1);
    printk("para_load_done %1d, i_rdma_ack_wait %1d, i_sao_intl_col_done %1d, i_sao_outbuf_full %1d\n",
        (reg_val >> 13) & 0x1,
        (reg_val >> 12) & 0x1,
        (reg_val >> 11) & 0x1,
        (reg_val >> 10) & 0x1);
    printk("lf_sub_done %1d, i_wdma_ack_wait %1d, lf_all_sub_done %1d, cur_ycbcr %1d, sub8x8_done %2d\n",
        (reg_val >> 9) & 0x1,
        (reg_val >> 8) & 0x1,
        (reg_val >> 6) & 0x1,
        (reg_val >> 4) & 0x1,
        reg_val & 0xf);
    printk("[-] VCE REG Dump\n");
    printk("[-] VCE REG Dump\n");

    printk("-------------------------------------------------------------------------------\n");

    {
        unsigned int q_cmd_done_inst, stage0_inst_info, stage1_inst_info, stage2_inst_info, dec_seek_cycle, dec_parsing_cycle, dec_decoding_cycle;
        q_cmd_done_inst = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01E8);
        stage0_inst_info = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01EC);
        stage1_inst_info = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01F0);
        stage2_inst_info = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01F4);
        dec_seek_cycle = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01C0);
        dec_parsing_cycle = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01C4);
        dec_decoding_cycle = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01C8);
        printk("W5_RET_QUEUE_CMD_DONE_INST : 0x%08x\n", q_cmd_done_inst);
        printk("W5_RET_STAGE0_INSTANCE_INFO : 0x%08x\n", stage0_inst_info);
        printk("W5_RET_STAGE1_INSTANCE_INFO : 0x%08x\n", stage1_inst_info);
        printk("W5_RET_STAGE2_INSTANCE_INFO : 0x%08x\n", stage2_inst_info);
        printk("W5_RET_DEC_SEEK_CYCLE : 0x%08x\n", dec_seek_cycle);
        printk("W5_RET_DEC_PARSING_CYCLE : 0x%08x\n", dec_parsing_cycle);
        printk("W5_RET_DEC_DECODING_CYCLE : 0x%08x\n", dec_decoding_cycle);
    }

    printk("-------------------------------------------------------------------------------\n");

    /* SDMA & SHU INFO */
    // -----------------------------------------------------------------------------
    // SDMA registers
    // -----------------------------------------------------------------------------

    {
        //DECODER SDMA INFO
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5000);
        printk("SDMA_LOAD_CMD    = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5004);
        printk("SDMA_AUTO_MOD  = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5008);
        printk("SDMA_START_ADDR  = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x500C);
        printk("SDMA_END_ADDR   = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5010);
        printk("SDMA_ENDIAN     = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5014);
        printk("SDMA_IRQ_CLEAR  = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5018);
        printk("SDMA_BUSY       = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x501C);
        printk("SDMA_LAST_ADDR  = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5020);
        printk("SDMA_SC_BASE_ADDR  = 0x%x\n",reg_val);

        // -----------------------------------------------------------------------------
        // SHU registers
        // -----------------------------------------------------------------------------

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5400);
        printk("SHU_INIT = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5404);
        printk("SHU_SEEK_NXT_NAL = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5408);
        printk("SHU_RD_NAL_ADDR = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x540c);
        printk("SHU_STATUS = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5410);
        printk("SHU_GBYTE_1 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5414);
        printk("SHU_GBYTE_2 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5418);
        printk("SHU_GBYTE_3 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x541c);
        printk("SHU_GBYTE_4 = 0x%x\n",reg_val);


        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5420);
        printk("SHU_GBYTE_5 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5424);
        printk("SHU_GBYTE_6 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5428);
        printk("SHU_GBYTE_7 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x542c);
        printk("SHU_GBYTE_8 = 0x%x\n",reg_val);


        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5430);
        printk("SHU_SBYTE_LOW = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5434);
        printk("SHU_SBYTE_HIGH = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5438);
        printk("SHU_ST_PAT_DIS = 0x%x\n",reg_val);


        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5440);
        printk("SHU_SHU_NBUF0_0 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5444);
        printk("SHU_SHU_NBUF0_1 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5448);
        printk("SHU_SHU_NBUF0_2 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x544c);
        printk("SHU_SHU_NBUF0_3 = 0x%x\n",reg_val);


        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5450);
        printk("SHU_SHU_NBUF1_0 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5454);
        printk("SHU_SHU_NBUF1_1 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5458);
        printk("SHU_SHU_NBUF1_2 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x545c);
        printk("SHU_SHU_NBUF1_3 = 0x%x\n",reg_val);


        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5460);
        printk("SHU_SHU_NBUF2_0 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5464);
        printk("SHU_SHU_NBUF2_1 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5468);
        printk("SHU_SHU_NBUF2_2 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x546c);
        printk("SHU_SHU_NBUF2_3 = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5470);
        printk("SHU_NBUF_RPTR = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5474);
        printk("SHU_NBUF_WPTR = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5478);
        printk("SHU_REMAIN_BYTE = 0x%x\n",reg_val);

        // -----------------------------------------------------------------------------
        // GBU registers
        // -----------------------------------------------------------------------------

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5800);
        printk("GBU_INIT = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5804);
        printk("GBU_STATUS = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5808);
        printk("GBU_TCNT = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x580c);
        printk("GBU_TCNT = 0x%x\n",reg_val);

                reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5c10);
        printk("GBU_WBUF0_LOW = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5c14);
        printk("GBU_WBUF0_HIGH = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5c18);

        printk("GBU_WBUF1_LOW = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5c1c);
        printk("GBU_WBUF1_HIGH = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5c20);
        printk("GBU_WBUF2_LOW = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5c24);
        printk("GBU_WBUF2_HIGH = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5c30);
        printk("GBU_WBUF_RPTR = 0x%x\n",reg_val);
        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5c34);
        printk("GBU_WBUF_WPTR = 0x%x\n",reg_val);

        reg_val = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_REG_BASE + 0x5c28);
        printk("GBU_REMAIN_BIT = 0x%x\n",reg_val);
    }
}
#endif


int vmgr_4k_d2_get_close(vputype type)
{
    return vmgr_4k_d2_data.closed[type];
}

int vmgr_4k_d2_get_alive(void)
{
    return vmgr_4k_d2_data.dev_opened;
}

int vmgr_4k_d2_set_close(vputype type, int value, int bfreemem)
{
    if( vmgr_4k_d2_get_close(type) == value ){
        dprintk(" %d was already set into %d. \n", type, value);
        return -1;
    }

    vmgr_4k_d2_data.closed[type] = value;
    if(value == 1){
        vmgr_4k_d2_data.handle[type] = 0x00;

        if(bfreemem)
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

int vmgr_4k_d2_process_ex(VpuList_t *cmd_list, vputype type, int Op, int *result)
{
    if(vmgr_4k_d2_data.dev_opened == 0)
        return 0;

    printk(" \n process_ex %d - 0x%x \n\n", type, Op);

    if(!vmgr_4k_d2_get_close(type))
    {
        cmd_list->type          = type;
        cmd_list->cmd_type      = Op;
        cmd_list->handle        = vmgr_4k_d2_data.handle[type];
        cmd_list->args          = NULL;
        cmd_list->comm_data     = NULL;
        cmd_list->vpu_result    = result;
        vmgr_4k_d2_list_manager(cmd_list, LIST_ADD);

        return 1;
    }

    return 1;
}

static int _vmgr_4k_d2_internal_handler(void)
{
    int ret, ret_code = RETCODE_SUCCESS;
    int timeout = 200;

    if(vmgr_4k_d2_data.check_interrupt_detection)
    {
        if(vmgr_4k_d2_data.oper_intr > 0)
        {
            detailk("Success 1: vpu-4k-d2 vp9/hevc operation!! \n");
            ret_code = RETCODE_SUCCESS;
        }
        else
        {
            ret = wait_event_interruptible_timeout(vmgr_4k_d2_data.oper_wq, vmgr_4k_d2_data.oper_intr > 0, msecs_to_jiffies(timeout));

            if(vmgr_4k_d2_is_loadable() > 0)
                ret_code = RETCODE_CODEC_EXIT;
            else
            if(vmgr_4k_d2_data.oper_intr > 0)
            {
                detailk("Success 2: vpu-4k-d2 vp9/hevc operation!! \n");
            #if defined(FORCED_ERROR)
                if(forced_error_count-- <= 0)
                {
                    ret_code = RETCODE_CODEC_EXIT;
                    forced_error_count = FORCED_ERR_CNT;
                    //vetc_dump_reg_all(vmgr_4k_d2_data.base_addr, "_vmgr_4k_d2_internal_handler force-timed_out");
                    //_vpu_4k_d2_dump_status();
                }
                else
            #endif
                ret_code = RETCODE_SUCCESS;
            }
            else
            {
                err("[CMD 0x%x][%d]: vpu-4k-d2 vp9/hevc timed_out(ref %d msec) => oper_intr[%d]!! [%d]th frame len %d\n", vmgr_4k_d2_data.current_cmd, ret, timeout, vmgr_4k_d2_data.oper_intr, vmgr_4k_d2_data.nDecode_Cmd, vmgr_4k_d2_data.szFrame_Len);
                //vetc_dump_reg_all(vmgr_4k_d2_data.base_addr, "_vmgr_4k_d2_internal_handler timed_out");
                //_vpu_4k_d2_dump_status();
                ret_code = RETCODE_CODEC_EXIT;
            }
        }

        vmgr_4k_d2_data.oper_intr = 0;
    }

    return ret_code;
}

static int _vmgr_4k_d2_process(vputype type, int cmd, long pHandle, void* args)
{
    int ret = 0;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
    struct timeval t1 , t2;
    int time_gap_ms = 0;
#endif

    vmgr_4k_d2_data.check_interrupt_detection = 0;
    vmgr_4k_d2_data.current_cmd = cmd;

    if (type < VPU_ENC)
    {
        if (cmd != VPU_DEC_INIT && cmd != VPU_DEC_INIT_KERNEL) {
            if(vmgr_4k_d2_get_close(type) || vmgr_4k_d2_data.handle[type] == 0x00)
                return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
        }

        if (cmd != VPU_DEC_BUF_FLAG_CLEAR && cmd != VPU_DEC_DECODE &&
            cmd != VPU_DEC_BUF_FLAG_CLEAR_KERNEL && cmd != VPU_DEC_DECODE_KERNEL) {
            cmdk("@@@@@@@@@@ Decoder(%d), command: 0x%x \n", type, cmd);
        }

        switch(cmd)
        {
            case VPU_DEC_INIT:
            case VPU_DEC_INIT_KERNEL:
            {
                VPU_4K_D2_INIT_t* arg;

                arg = (VPU_4K_D2_INIT_t *)args;
                vmgr_4k_d2_data.handle[type] = 0x00;

                arg->gsV4kd2DecInit.m_RegBaseVirtualAddr = (codec_addr_t)vmgr_4k_d2_data.base_addr;
                arg->gsV4kd2DecInit.m_Memcpy            = (void* (*) ( void*, const void*, unsigned int, unsigned int))vetc_memcpy;
                arg->gsV4kd2DecInit.m_Memset            = (void  (*) ( void*, int, unsigned int, unsigned int))vetc_memset;
                arg->gsV4kd2DecInit.m_Interrupt         = (int  (*) ( void ))_vmgr_4k_d2_internal_handler;
                arg->gsV4kd2DecInit.m_Ioremap           = (void* (*) ( phys_addr_t, unsigned int))vetc_ioremap;
                arg->gsV4kd2DecInit.m_Iounmap           = (void  (*) ( void* ))vetc_iounmap;
                arg->gsV4kd2DecInit.m_reg_read          = (unsigned int (*)(void *, unsigned int))vetc_reg_read;
                arg->gsV4kd2DecInit.m_reg_write         = (void (*)(void *, unsigned int, unsigned int))vetc_reg_write;

                vmgr_4k_d2_data.check_interrupt_detection = 1;

                vmgr_4k_d2_data.bDiminishInputCopy = (arg->gsV4kd2DecInit.m_uiDecOptFlags & (1 << 26)) ? true : false;
                if (system_rev == 0 /* MPW1 */ && arg->gsV4kd2DecInit.m_Reserved[10] == 10) {
                    arg->gsV4kd2DecInit.m_uiDecOptFlags |= WAVE5_WTL_ENABLE; // disable map converter
                    printk("@@ Dec :: Init In => enable WTL (off the compressed output mode)\n");
                    arg->gsV4kd2DecInit.m_Reserved[10] = 5; // to notify this refusal

                    // [work-around] reduce total memory size of min. frame buffers
                    arg->gsV4kd2DecInit.m_Reserved[5] = 8; // Max Bitdepth
                    arg->gsV4kd2DecInit.m_uiDecOptFlags |= (1 << 3); // 10 to 8 bit shit
                }

                //vpu_optee_fw_read(TYPE_VPU_4K_D2);
                dprintk("@@ Dec :: Init In => workbuff 0x%x/0x%x, Reg: 0x%p/0x%x, format : %d, Stream(0x%x/0x%x, 0x%x)\n",
                            arg->gsV4kd2DecInit.m_BitWorkAddr[PA], arg->gsV4kd2DecInit.m_BitWorkAddr[VA],
                            vmgr_4k_d2_data.base_addr, arg->gsV4kd2DecInit.m_RegBaseVirtualAddr,
                            arg->gsV4kd2DecInit.m_iBitstreamFormat, arg->gsV4kd2DecInit.m_BitstreamBufAddr[PA],
                            arg->gsV4kd2DecInit.m_BitstreamBufAddr[VA], arg->gsV4kd2DecInit.m_iBitstreamBufSize);

                dprintk("@@ Dec :: Init In => optFlag 0x%x, Userdata(%d), Inter: %d, PlayEn: %d\n",
                            arg->gsV4kd2DecInit.m_uiDecOptFlags,
                            arg->gsV4kd2DecInit.m_bEnableUserData, arg->gsV4kd2DecInit.m_bCbCrInterleaveMode,
                            arg->gsV4kd2DecInit.m_iFilePlayEnable);

                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, (void*)(&arg->gsV4kd2DecHandle), (void*)(&arg->gsV4kd2DecInit), (void*)NULL);
                if( ret != RETCODE_SUCCESS ){
                    printk("@@ Dec :: Init Done with ret(0x%x)\n", ret);
                    if( ret != RETCODE_CODEC_EXIT ){
                        //vetc_dump_reg_all(vmgr_4k_d2_data.base_addr, "init failure");
                        //ret = tcc_vpu_4k_d2_dec(cmd, (void*)(&arg->gsV4kd2DecHandle), (void*)(&arg->gsV4kd2DecInit), (void*)NULL);
                        //printk("@@ Dec :: Init Done with ret(0x%x)\n", ret);
                        //vetc_dump_reg_all(vmgr_4k_d2_data.base_addr, "init success");
                    }
                }
                if( ret != RETCODE_CODEC_EXIT && arg->gsV4kd2DecHandle != 0) {
                    vmgr_4k_d2_data.handle[type] = arg->gsV4kd2DecHandle;
                    vmgr_4k_d2_set_close(type, 0, 0);
                    cmdk("@@ Dec :: vmgr_4k_d2_data.handle = 0x%x \n", arg->gsV4kd2DecHandle);
                }
                else{
                    //To free memory!!
                    vmgr_4k_d2_set_close(type, 0, 0);
                    vmgr_4k_d2_set_close(type, 1, 1);
                }
                dprintk("@@ Dec :: Init Done Handle(0x%x) \n", arg->gsV4kd2DecHandle);

        #ifdef CONFIG_VPU_TIME_MEASUREMENT
                vmgr_4k_d2_data.iTime[type].print_out_index = vmgr_4k_d2_data.iTime[type].proc_base_cnt = 0;
                vmgr_4k_d2_data.iTime[type].accumulated_proc_time = vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt = 0;
                vmgr_4k_d2_data.iTime[type].proc_time_30frames = 0;
        #endif
            }
            break;

            case VPU_DEC_SEQ_HEADER:
            case VPU_DEC_SEQ_HEADER_KERNEL:
            {
                void* arg = args;
                int iSize;

                vpu_4K_D2_dec_initial_info_t *gsV4kd2DecInitialInfo = vmgr_4k_d2_data.bDiminishInputCopy
                    ? &((VPU_4K_D2_DECODE_t *)arg)->gsV4kd2DecInitialInfo
                    : &((VPU_4K_D2_SEQ_HEADER_t *)arg)->gsV4kd2DecInitialInfo;

                vmgr_4k_d2_data.szFrame_Len = iSize = vmgr_4k_d2_data.bDiminishInputCopy
                    ? ((VPU_4K_D2_DECODE_t *)arg)->gsV4kd2DecInput.m_iBitstreamDataSize
                    : (int)((VPU_4K_D2_SEQ_HEADER_t *)arg)->stream_size;

                vmgr_4k_d2_data.check_interrupt_detection = 1;
                vmgr_4k_d2_data.nDecode_Cmd = 0;

                dprintk("@@ Dec :: VPU_4K_D2_DEC_SEQ_HEADER in :: size(%d) \n", iSize);
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL,
                                        &pHandle,
                                        (vmgr_4k_d2_data.bDiminishInputCopy
                                            ? (void*)(&((VPU_4K_D2_DECODE_t *)arg)->gsV4kd2DecInput) : (void*)iSize),
                                        (void*)gsV4kd2DecInitialInfo);

                dprintk("@@ Dec :: VPU_4K_D2_DEC_SEQ_HEADER out 0x%x \n res info. %d - %d - %d, %d - %d - %d \n", ret,
                        gsV4kd2DecInitialInfo->m_iPicWidth,
                        gsV4kd2DecInitialInfo->m_PicCrop.m_iCropLeft,
                        gsV4kd2DecInitialInfo->m_PicCrop.m_iCropRight,
                        gsV4kd2DecInitialInfo->m_iPicHeight,
                        gsV4kd2DecInitialInfo->m_PicCrop.m_iCropTop,
                        gsV4kd2DecInitialInfo->m_PicCrop.m_iCropBottom);

                vmgr_4k_d2_change_clock(gsV4kd2DecInitialInfo->m_iPicWidth, gsV4kd2DecInitialInfo->m_iPicHeight);
            }
            break;

            case VPU_DEC_REG_FRAME_BUFFER:
            case VPU_DEC_REG_FRAME_BUFFER_KERNEL:
            {
                VPU_4K_D2_SET_BUFFER_t *arg = (VPU_4K_D2_SET_BUFFER_t *)args;

                dprintk("@@ Dec :: VPU_4K_D2_DEC_REG_FRAME_BUFFER in :: 0x%x/0x%x \n",
                        arg->gsV4kd2DecBuffer.m_FrameBufferStartAddr[0], arg->gsV4kd2DecBuffer.m_FrameBufferStartAddr[1]);

                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, (void*)(&arg->gsV4kd2DecBuffer), (void*)NULL);
                dprintk("@@ Dec :: VPU_4K_D2_DEC_REG_FRAME_BUFFER out \n");
            }
            break;

            case VPU_DEC_DECODE:
            case VPU_DEC_DECODE_KERNEL:
            {
                VPU_4K_D2_DECODE_t *arg = (VPU_4K_D2_DECODE_t *)args;

                int hiding_suferframe = arg->gsV4kd2DecInput.m_Reserved[20];
            #ifdef CONFIG_VPU_TIME_MEASUREMENT
                do_gettimeofday(&t1);
            #endif
            rinse_repeat:
                vmgr_4k_d2_data.szFrame_Len = arg->gsV4kd2DecInput.m_iBitstreamDataSize;
                dprintk("@@ Dec: Dec In => 0x%x - 0x%x, 0x%x, 0x%x - 0x%x, %d, flag: %d  \n",
                        arg->gsV4kd2DecInput.m_BitstreamDataAddr[PA], arg->gsV4kd2DecInput.m_BitstreamDataAddr[VA],
                        arg->gsV4kd2DecInput.m_iBitstreamDataSize,
                        arg->gsV4kd2DecInput.m_UserDataAddr[PA], arg->gsV4kd2DecInput.m_UserDataAddr[VA],
                        arg->gsV4kd2DecInput.m_iUserDataBufferSize,
                        arg->gsV4kd2DecInput.m_iSkipFrameMode);

                vmgr_4k_d2_data.check_interrupt_detection = 1;
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, (void *)&arg->gsV4kd2DecInput, (void *)&arg->gsV4kd2DecOutput);

                dprintk("@@ Dec: Dec Out => %d - %d - %d, %d - %d - %d \n",
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayWidth,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropRight,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayHeight,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropTop,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom);

                dprintk("@@ Dec: Dec Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], OutStatus[%d/%d], POC[%d/%d] \n", ret,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iPicType,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx, arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iOutputStatus, arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodingStatus,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_Reserved[5], arg->gsV4kd2DecOutput.m_DecOutInfo.m_Reserved[6]);

                switch (arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodingStatus) {
                    case VPU_DEC_BUF_FULL:
                    err("[%d] Buffer full\n", type);
                    //vetc_dump_reg_all(vmgr_4k_d2_data.base_addr, "Buffer full");
                        break;

                    case VPU_DEC_VP9_SUPER_FRAME:
                        dprintk("Super Frame: sub-frame num: %u\n", arg->gsV4kd2DecOutput.m_DecOutInfo.m_SuperFrameInfo.m_uiNframes);
                        if (hiding_suferframe == 20
                                && arg->gsV4kd2DecOutput.m_DecOutInfo.m_SuperFrameInfo.m_uiNframes <= 2
                                && arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx < 0) {
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
				vmgr_4k_d2_change_clock(arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedWidth, arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedHeight);
            }
            break;

            case VPU_DEC_GET_OUTPUT_INFO:
            case VPU_DEC_GET_OUTPUT_INFO_KERNEL:
            {
                VPU_4K_D2_DECODE_t *arg = (VPU_4K_D2_DECODE_t *)args;

                dprintk("@@ Dec: VPU_DEC_GET_OUTPUT_INFO \n");
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, (void *)arg, (void *)&arg->gsV4kd2DecOutput);

                dprintk("@@ Dec: Dec Out => %d - %d - %d, %d - %d - %d \n",
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayWidth,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropRight,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayHeight,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropTop,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom);

                dprintk("@@ Dec: Dec Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], OutStatus[%d/%d] \n", ret,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iPicType,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx, arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx,
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iOutputStatus, arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodingStatus);

                dprintk("@@ Dec: Dec Out => dec_Idx(%d), 0x%x 0x%x 0x%x / 0x%x 0x%x 0x%x \n",
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx,
                        arg->gsV4kd2DecOutput.m_pDispOut[PA][0],
                        arg->gsV4kd2DecOutput.m_pDispOut[PA][1],
                        arg->gsV4kd2DecOutput.m_pDispOut[PA][2],
                        arg->gsV4kd2DecOutput.m_pDispOut[VA][0],
                        arg->gsV4kd2DecOutput.m_pDispOut[VA][1],
                        arg->gsV4kd2DecOutput.m_pDispOut[VA][2]);

                dprintk("@@ Dec: Dec Out => disp_Idx(%d), 0x%x 0x%x 0x%x / 0x%x 0x%x 0x%x \n",
                        arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx,
                        arg->gsV4kd2DecOutput.m_pCurrOut[PA][0],
                        arg->gsV4kd2DecOutput.m_pCurrOut[PA][1],
                        arg->gsV4kd2DecOutput.m_pCurrOut[PA][2],
                        arg->gsV4kd2DecOutput.m_pCurrOut[VA][0],
                        arg->gsV4kd2DecOutput.m_pCurrOut[VA][1],
                        arg->gsV4kd2DecOutput.m_pCurrOut[VA][2]);

                if(arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodingStatus == VPU_DEC_BUF_FULL){
                    err("%d: Buffer full\n", type);
                    //vetc_dump_reg_all(vmgr_4k_d2_data.base_addr, "Buffer full");
                }
            }
            break;

            case VPU_DEC_BUF_FLAG_CLEAR:
            case VPU_DEC_BUF_FLAG_CLEAR_KERNEL:
            {
                int *arg = (int *)args;
                dprintk("@@ Dec :: DispIdx Clear %d \n", *arg);
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, (void*)(arg), (void*)NULL);
            }
            break;

            case VPU_DEC_FLUSH_OUTPUT:
            case VPU_DEC_FLUSH_OUTPUT_KERNEL:
            {
                VPU_4K_D2_DECODE_t *arg = (VPU_4K_D2_DECODE_t *)args;
                printk("@@ Dec :: VPU_4K_D2_DEC_FLUSH_OUTPUT !! \n");
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, (void *)&arg->gsV4kd2DecInput, (void *)&arg->gsV4kd2DecOutput);
            }
            break;

            case VPU_DEC_CLOSE:
            case VPU_DEC_CLOSE_KERNEL:
            {
                //VPU_4K_D2_DECODE_t *arg = (VPU_4K_D2_DECODE_t *)args;
                //vmgr_4k_d2_data.check_interrupt_detection = 1;
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, (void *)NULL, (void *)NULL/*(&arg->gsV4kd2DecOutput)*/);
                dprintk("@@ Dec :: VPU_4K_D2_DEC_CLOSED !! \n");
                vmgr_4k_d2_set_close(type, 1, 1);
            }
            break;

            case GET_RING_BUFFER_STATUS:
            case GET_RING_BUFFER_STATUS_KERNEL:
            {
                VPU_4K_D2_RINGBUF_GETINFO_t *arg = (VPU_4K_D2_RINGBUF_GETINFO_t *)args;
                //vmgr_4k_d2_data.check_interrupt_detection = 1;

                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, (void *)NULL, (void *)&arg->gsV4kd2DecRingStatus);
            }
            break;

            case FILL_RING_BUFFER_AUTO:
            case FILL_RING_BUFFER_AUTO_KERNEL:
            {
                VPU_4K_D2_RINGBUF_SETBUF_t *arg = (VPU_4K_D2_RINGBUF_SETBUF_t *)args;
                //vmgr_4k_d2_data.check_interrupt_detection = 1;
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, (void *)&arg->gsV4kd2DecInit, (void *)&arg->gsV4kd2DecRingFeed);
                dprintk("@@ Dec :: ReadPTR : 0x%08x, WritePTR : 0x%08x\n",
                        vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x120), vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x124));
            }
            break;

            case VPU_UPDATE_WRITE_BUFFER_PTR:
            case VPU_UPDATE_WRITE_BUFFER_PTR_KERNEL:
            {
                VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t *arg = (VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t *)args;
                //vmgr_4k_d2_data.check_interrupt_detection = 1;
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, (void*)(arg->iCopiedSize), (void*)(arg->iFlushBuf));
            }
            break;

            case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY:
            case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_KERNEL:
            {
                VPU_4K_D2_SEQ_HEADER_t *arg = (VPU_4K_D2_SEQ_HEADER_t *)args;
                //vmgr_4k_d2_data.check_interrupt_detection = 1;
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, (void*)(&arg->gsV4kd2DecInitialInfo), NULL);
            }
            break;

            case VPU_CODEC_GET_VERSION:
            case VPU_CODEC_GET_VERSION_KERNEL:
            {
                VPU_4K_D2_GET_VERSION_t *arg = (VPU_4K_D2_GET_VERSION_t *)args;
                //vmgr_4k_d2_data.check_interrupt_detection = 1;
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, arg->pszVersion, arg->pszBuildData);
                dprintk("@@ Dec: version : %s, build : %s\n", arg->pszVersion, arg->pszBuildData);
            }
            break;

            case VPU_DEC_SWRESET:
            case VPU_DEC_SWRESET_KERNEL:
            {
                ret = tcc_vpu_4k_d2_dec(cmd & ~VPU_BASE_OP_KERNEL, &pHandle, NULL, NULL);
            }
            break;

            default:
            {
                err("@@ Dec: not supported command(0x%x) \n", cmd);
                return 0x999;
            }
            break;
        }
    }
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
    else
    {
        err("@@ Enc[%d]: Encoder for VPU-4K-D2 VP9/HEVC do not support. command(0x%x) \n", type, cmd);
        return 0x999;
    }
#endif

#ifdef CONFIG_VPU_TIME_MEASUREMENT
    time_gap_ms = vetc_GetTimediff_ms(t1, t2);

    if( cmd == VPU_DEC_DECODE || cmd == VPU_ENC_ENCODE ) {
        vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt++;
        vmgr_4k_d2_data.iTime[type].proc_time[vmgr_4k_d2_data.iTime[type].proc_base_cnt] = time_gap_ms;
        vmgr_4k_d2_data.iTime[type].proc_time_30frames += time_gap_ms;
        vmgr_4k_d2_data.iTime[type].accumulated_proc_time += time_gap_ms;
        if(vmgr_4k_d2_data.iTime[type].proc_base_cnt != 0 && vmgr_4k_d2_data.iTime[type].proc_base_cnt % 29 == 0)
        {
            printk("VPU-4K-D2 VP9/HEVC[%d] :: Dec[%4d] time %2d.%2d / %2d.%2d ms: %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d \n",
                        type,
                        vmgr_4k_d2_data.iTime[type].print_out_index,
                        vmgr_4k_d2_data.iTime[type].proc_time_30frames/30,
                        ((vmgr_4k_d2_data.iTime[type].proc_time_30frames%30)*100)/30,
                        vmgr_4k_d2_data.iTime[type].accumulated_proc_time/vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt,
                        ((vmgr_4k_d2_data.iTime[type].accumulated_proc_time%vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt)*100)/vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt,
                        vmgr_4k_d2_data.iTime[type].proc_time[0], vmgr_4k_d2_data.iTime[type].proc_time[1], vmgr_4k_d2_data.iTime[type].proc_time[2], vmgr_4k_d2_data.iTime[type].proc_time[3], vmgr_4k_d2_data.iTime[type].proc_time[4],
                        vmgr_4k_d2_data.iTime[type].proc_time[5], vmgr_4k_d2_data.iTime[type].proc_time[6], vmgr_4k_d2_data.iTime[type].proc_time[7], vmgr_4k_d2_data.iTime[type].proc_time[8], vmgr_4k_d2_data.iTime[type].proc_time[9],
                        vmgr_4k_d2_data.iTime[type].proc_time[10], vmgr_4k_d2_data.iTime[type].proc_time[11], vmgr_4k_d2_data.iTime[type].proc_time[12], vmgr_4k_d2_data.iTime[type].proc_time[13], vmgr_4k_d2_data.iTime[type].proc_time[14],
                        vmgr_4k_d2_data.iTime[type].proc_time[15], vmgr_4k_d2_data.iTime[type].proc_time[16], vmgr_4k_d2_data.iTime[type].proc_time[17], vmgr_4k_d2_data.iTime[type].proc_time[18], vmgr_4k_d2_data.iTime[type].proc_time[19],
                        vmgr_4k_d2_data.iTime[type].proc_time[20], vmgr_4k_d2_data.iTime[type].proc_time[21], vmgr_4k_d2_data.iTime[type].proc_time[22], vmgr_4k_d2_data.iTime[type].proc_time[23], vmgr_4k_d2_data.iTime[type].proc_time[24],
                        vmgr_4k_d2_data.iTime[type].proc_time[25], vmgr_4k_d2_data.iTime[type].proc_time[26], vmgr_4k_d2_data.iTime[type].proc_time[27], vmgr_4k_d2_data.iTime[type].proc_time[28], vmgr_4k_d2_data.iTime[type].proc_time[29]);
            vmgr_4k_d2_data.iTime[type].proc_base_cnt = 0;
            vmgr_4k_d2_data.iTime[type].proc_time_30frames = 0;
            vmgr_4k_d2_data.iTime[type].print_out_index++;
        }
        else{
            vmgr_4k_d2_data.iTime[type].proc_base_cnt++;
        }
    }
#endif

    return ret;
}

static int _vmgr_4k_d2_proc_exit_by_external(struct VpuList *list, int *result, unsigned int type)
{
    if(!vmgr_4k_d2_get_close(type) && vmgr_4k_d2_data.handle[type] != 0x00)
    {
        list->type = type;
        if( type >= VPU_ENC )
            list->cmd_type = VPU_ENC_CLOSE;
        else
            list->cmd_type = VPU_DEC_CLOSE;
        list->handle    = vmgr_4k_d2_data.handle[type];
        list->args      = NULL;
        list->comm_data = NULL;
        list->vpu_result = result;

        printk("_vmgr_4k_d2_proc_exit_by_external for %d!! \n", type);
        vmgr_4k_d2_list_manager(list, LIST_ADD);

        return 1;
    }

    return 0;
}

static void _vmgr_4k_d2_wait_process(int wait_ms)
{
    int max_count = wait_ms/20;

    //wait!! in case exceptional processing. ex). sdcard out!!
    while(vmgr_4k_d2_data.cmd_processing)
    {
        max_count--;
        msleep(20);

        if(max_count <= 0)
        {
            err("cmd_processing(cmd %d) didn't finish!! \n", vmgr_4k_d2_data.current_cmd);
            break;
        }
    }
}

static int _vmgr_4k_d2_external_all_close(int wait_ms)
{
    int type = 0;
    int max_count = 0;
    int ret;

    for(type = 0; type < VPU_4K_D2_MAX; type++)
    {
        if(_vmgr_4k_d2_proc_exit_by_external(&vmgr_4k_d2_data.vList[type], &ret, type))
        {
            max_count = wait_ms/10;
            while(!vmgr_4k_d2_get_close(type))
            {
                max_count--;
                msleep(10);
            }
        }
    }

    return 0;
}
static long _vmgr_4k_d2_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    CONTENTS_INFO info;
    OPENED_sINFO open_info;

    mutex_lock(&vmgr_4k_d2_data.comm_data.io_mutex);

    switch(cmd)
    {
        case VPU_SET_CLK:
        case VPU_SET_CLK_KERNEL:
            if(cmd == VPU_SET_CLK_KERNEL) {
                if (NULL == memcpy(&info,(CONTENTS_INFO*)arg,sizeof(info)))
                    ret = -EFAULT;
            } else {
                if(copy_from_user(&info,(CONTENTS_INFO*)arg,sizeof(info)))
                    ret = -EFAULT;
            }
            break;

        case VPU_GET_FREEMEM_SIZE:
        case VPU_GET_FREEMEM_SIZE_KERNEL:
            {
                unsigned int type;
                unsigned int freemem_sz;
                if(cmd == VPU_GET_FREEMEM_SIZE_KERNEL) {
                    if (NULL == memcpy(&type, (unsigned int*)arg, sizeof(unsigned int)))
                        ret = -EFAULT;
                } else {
                    if(copy_from_user(&type, (unsigned int*)arg, sizeof(unsigned int)))
                        ret = -EFAULT;
                }

                if(ret == 0) {
                    if(type > VPU_MAX)
                        type = VPU_DEC;
                    freemem_sz = vmem_get_freemem_size(type);

                    if (cmd == VPU_GET_FREEMEM_SIZE_KERNEL) {
                        if (NULL == memcpy((unsigned int*)arg, &freemem_sz, sizeof(unsigned int)))
                            ret = -EFAULT;
                    } else {
                        if (copy_to_user((unsigned int*)arg, &freemem_sz, sizeof(unsigned int)))
                            ret = -EFAULT;
                    }
                }
            }
            break;

        case VPU_HW_RESET:
            break;

        case VPU_SET_MEM_ALLOC_MODE:
        case VPU_SET_MEM_ALLOC_MODE_KERNEL:
            {
                if(cmd == VPU_SET_MEM_ALLOC_MODE_KERNEL) {
                    if (NULL == memcpy(&open_info,(OPENED_sINFO*)arg, sizeof(OPENED_sINFO)))
                        ret = -EFAULT;
                } else {
                    if(copy_from_user(&open_info,(OPENED_sINFO*)arg, sizeof(OPENED_sINFO)))
                        ret = -EFAULT;
                }

                if(ret == 0) {
                    if(open_info.opened_cnt != 0)
                        vmem_set_only_decode_mode(open_info.type);
                }
            }
            break;

        case VPU_CHECK_CODEC_STATUS:
        case VPU_CHECK_CODEC_STATUS_KERNEL:
            {
                if(cmd == VPU_CHECK_CODEC_STATUS_KERNEL) {
                    if (NULL == memcpy((int*)arg, vmgr_4k_d2_data.closed, sizeof(vmgr_4k_d2_data.closed)))
                        ret = -EFAULT;
                } else {
                    if(copy_to_user((int*)arg, vmgr_4k_d2_data.closed, sizeof(vmgr_4k_d2_data.closed)))
                        ret = -EFAULT;
                }
            }
            break;

        case VPU_GET_INSTANCE_IDX:
        case VPU_GET_INSTANCE_IDX_KERNEL:
            {
                INSTANCE_INFO iInst;

                if(cmd == VPU_GET_INSTANCE_IDX_KERNEL) {
                    if (NULL == memcpy(&iInst, (int*)arg, sizeof(INSTANCE_INFO)))
                        ret = -EFAULT;
                } else {
                    if (copy_from_user(&iInst, (int*)arg, sizeof(INSTANCE_INFO)))
                        ret = -EFAULT;
                }

                if(ret == 0) {
                    if( iInst.type == VPU_ENC )
                        venc_get_instance(&iInst.nInstance);
                    else
                        vdec_get_instance(&iInst.nInstance);

                    if(cmd == VPU_GET_INSTANCE_IDX_KERNEL) {
                        if (NULL == memcpy((int*)arg, &iInst, sizeof(INSTANCE_INFO)))
                            ret = -EFAULT;
                    } else {
                        if (copy_to_user((int*)arg, &iInst, sizeof(INSTANCE_INFO)))
                            ret = -EFAULT;
                    }
                }
            }
            break;

        case VPU_CLEAR_INSTANCE_IDX:
        case VPU_CLEAR_INSTANCE_IDX_KERNEL:
            {
                INSTANCE_INFO iInst;
                if(cmd == VPU_CLEAR_INSTANCE_IDX_KERNEL) {
                    if (NULL == memcpy(&iInst, (int*)arg, sizeof(INSTANCE_INFO)))
                        ret = -EFAULT;
                } else {
                    if (copy_from_user(&iInst, (int*)arg, sizeof(INSTANCE_INFO)))
                        ret = -EFAULT;
                }

                if(ret == 0) {
                    if( iInst.type == VPU_ENC )
                        venc_clear_instance(iInst.nInstance);
                    else
                        vdec_clear_instance(iInst.nInstance);
                }
            }
            break;


		case VPU_TRY_FORCE_CLOSE:
		case VPU_TRY_FORCE_CLOSE_KERNEL:
		{
            //tcc_vpu_4k_d2_dec_esc(1, 0,0,0);
			if(!vmgr_4k_d2_data.bVpu_already_proc_force_closed)
			{
				_vmgr_4k_d2_wait_process(200);
				vmgr_4k_d2_data.external_proc = 1;
				_vmgr_4k_d2_external_all_close(200);
				vmgr_4k_d2_data.external_proc = 0;
				vmgr_4k_d2_data.bVpu_already_proc_force_closed = true;
			}
		}
		break;

		case VPU_TRY_CLK_RESTORE:
		case VPU_TRY_CLK_RESTORE_KERNEL:
		{
			vmgr_4k_d2_restore_clock(0, vmgr_4k_d2_data.dev_opened);
		}
		break;

        default:
            err("Unsupported ioctl[%d]!!!\n", cmd);
            ret = -EINVAL;
            break;
    }

    mutex_unlock(&vmgr_4k_d2_data.comm_data.io_mutex);

    return ret;
}

#ifdef CONFIG_COMPAT
static long _vmgr_4k_d2_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return _vmgr_4k_d2_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static unsigned int cntInt_4kd2 = 0;
static irqreturn_t _vmgr_4k_d2_isr_handler(int irq, void *dev_id)
{
    unsigned long flags;

	cntInt_4kd2++;

	if(cntInt_4kd2%30 == 0){
		detailk("_vmgr_4k_d2_isr_handler %d \n", cntInt_4kd2);
	}

    spin_lock_irqsave(&(vmgr_4k_d2_data.oper_lock), flags);
    vmgr_4k_d2_data.oper_intr++;
    spin_unlock_irqrestore(&(vmgr_4k_d2_data.oper_lock), flags);

    wake_up_interruptible(&(vmgr_4k_d2_data.oper_wq));

    return IRQ_HANDLED;
}

static int _vmgr_4k_d2_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

    if (!vmgr_4k_d2_data.irq_reged) {
        err("not registered vpu-4k-d2 vp9/hevc-mgr-irq \n");
    }

    dprintk("_vmgr_4k_d2_open In!! %d'th \n", vmgr_4k_d2_data.dev_opened);

    vmgr_4k_d2_enable_clock(0, 0);

    if(vmgr_4k_d2_data.dev_opened == 0)
    {
#ifdef FORCED_ERROR
        forced_error_count = FORCED_ERR_CNT;
#endif
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
        vmgr_4k_d2_data.only_decmode = 0;
#else
        vmgr_4k_d2_data.only_decmode = 1;
#endif
        vmgr_4k_d2_data.clk_limitation = 1;
        //vmgr_4k_d2_hw_reset();
        vmgr_4k_d2_data.cmd_processing = 0;

        vmgr_4k_d2_enable_irq(vmgr_4k_d2_data.irq);
        //vetc_reg_init(vmgr_4k_d2_data.base_addr);
        if(0 > vmem_init())
	    {
	        err("failed to allocate memory for VPU_4K_D2!! %d \n", ret);
	        return -ENOMEM;
	    }

		cntInt_4kd2 = 0;
    }
    vmgr_4k_d2_data.dev_opened++;

    filp->private_data = &vmgr_4k_d2_data;
	dprintk("_vmgr_4k_d2_open Out!! %d'th \n", vmgr_4k_d2_data.dev_opened);

    return 0;
}

static int _vmgr_4k_d2_release(struct inode *inode, struct file *filp)
{
    dprintk("_vmgr_4k_d2_release In!! %d'th \n", vmgr_4k_d2_data.dev_opened);

	if(!vmgr_4k_d2_data.bVpu_already_proc_force_closed)
	{
		_vmgr_4k_d2_wait_process(2000);
	}
    if(vmgr_4k_d2_data.dev_opened > 0)
        vmgr_4k_d2_data.dev_opened--;
    if(vmgr_4k_d2_data.dev_opened == 0)
    {
//////////////////////////////////////
        int type = 0, alive_cnt = 0;

#if 1 // To close whole vpu-4k-d2 vp9/hevc instance when being killed process opened this.
	if(!vmgr_4k_d2_data.bVpu_already_proc_force_closed)
	{
		vmgr_4k_d2_data.external_proc = 1;
		_vmgr_4k_d2_external_all_close(200);
		_vmgr_4k_d2_wait_process(2000);
		vmgr_4k_d2_data.external_proc = 0;
	}
	vmgr_4k_d2_data.bVpu_already_proc_force_closed = false;
#endif

        for(type=0; type<VPU_4K_D2_MAX; type++) {
            if( vmgr_4k_d2_data.closed[type] == 0 ){
                alive_cnt++;
            }
        }

        if( alive_cnt )
        {
            // clear instances of vpu-4k-d2 vp9/hevc by force.
            //TCC_VPU_DEC( 0x40, (void*)NULL, (void*)NULL, (void*)NULL);
            printk("VPU-4K-D2 VP9/HEVC might be cleared by force. \n");
        }

//////////////////////////////////////
        vmgr_4k_d2_data.oper_intr = 0;
        vmgr_4k_d2_data.cmd_processing = 0;

        _vmgr_4k_d2_close_all(1);
//////////////////////////////////////

        vmgr_4k_d2_disable_irq(vmgr_4k_d2_data.irq);
        vmgr_4k_d2_BusPrioritySetting(BUS_FOR_NORMAL, 0);

		vmem_deinit();
    }

    vmgr_4k_d2_disable_clock(0, 0);

    vmgr_4k_d2_data.nOpened_Count++;
    printk("_vmgr_4k_d2_release Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d) \n", vmgr_4k_d2_data.dev_opened, vmgr_4k_d2_data.nOpened_Count,
                    vmgr_4k_d2_get_close(VPU_DEC), vmgr_4k_d2_get_close(VPU_DEC_EXT), vmgr_4k_d2_get_close(VPU_DEC_EXT2), vmgr_4k_d2_get_close(VPU_DEC_EXT3), vmgr_4k_d2_get_close(VPU_DEC_EXT4));

    return 0;
}

VpuList_t* vmgr_4k_d2_list_manager(VpuList_t* args, unsigned int cmd)
{
    VpuList_t *ret;

    mutex_lock(&vmgr_4k_d2_data.comm_data.list_mutex);

    {
        VpuList_t* data = NULL;
        ret = NULL;

    /*
        data = (VpuList_t *)args;

        if(cmd == LIST_ADD)
            printk("cmd = %d - 0x%x \n", cmd, data->cmd_type);
        else
            printk("cmd = %d \n", cmd);
    */
        switch(cmd)
        {
            case LIST_ADD:
                {
                    if(!args)
                    {
                        err("ADD :: data is null \n");
                        goto Error;
                    }

                    data = (VpuList_t*)args;
                    *(data->vpu_result) |= RET1;
                    list_add_tail(&data->list, &vmgr_4k_d2_data.comm_data.main_list);vmgr_4k_d2_data.cmd_queued++;
                    vmgr_4k_d2_data.comm_data.thread_intr++;
                    wake_up_interruptible(&(vmgr_4k_d2_data.comm_data.thread_wq));
                }
                break;

            case LIST_DEL:
                if(!args)
                {
                    err("DEL :: data is null \n");
                    goto Error;
                }
                data = (VpuList_t*)args;
                list_del(&data->list);vmgr_4k_d2_data.cmd_queued--;
                break;

            case LIST_IS_EMPTY:
                if(list_empty(&vmgr_4k_d2_data.comm_data.main_list))
                    ret =(VpuList_t*)0x1234;
                break;

            case LIST_GET_ENTRY:
                ret =  list_first_entry(&vmgr_4k_d2_data.comm_data.main_list, VpuList_t, list);
                break;
        }
    }

Error:
    mutex_unlock(&vmgr_4k_d2_data.comm_data.list_mutex);

    return ret;
}

//////////////////////////////////////////////////////////////////////////
// VPU-4K-D2 VP9/HEVC Thread!!
static int _vmgr_4k_d2_operation(void)
{
    int oper_finished;
    VpuList_t *oper_data = NULL;

    while(!vmgr_4k_d2_list_manager(NULL, LIST_IS_EMPTY))
    {
        vmgr_4k_d2_data.cmd_processing = 1;

        oper_finished = 1;
        dprintk("_vmgr_4k_d2_operation :: not empty cmd_queued(%d) \n", vmgr_4k_d2_data.cmd_queued);

        oper_data = (VpuList_t*)vmgr_4k_d2_list_manager(NULL, LIST_GET_ENTRY);
        if(!oper_data) {
            err("data is null \n");
            vmgr_4k_d2_data.cmd_processing = 0;
            return 0;
        }
        *(oper_data->vpu_result) |= RET2;

        dprintk("_vmgr_4k_d2_operation [%d] :: cmd = 0x%x, vmgr_4k_d2_data.cmd_queued(%d) \n", oper_data->type, oper_data->cmd_type, vmgr_4k_d2_data.cmd_queued);

        if(oper_data->type < VPU_4K_D2_MAX && oper_data != NULL /*&& oper_data->comm_data != NULL*/)
        {
            *(oper_data->vpu_result) |= RET3;

            *(oper_data->vpu_result) = _vmgr_4k_d2_process(oper_data->type, oper_data->cmd_type, oper_data->handle, oper_data->args);
            oper_finished = 1;
            if(*(oper_data->vpu_result) != RETCODE_SUCCESS)
            {
                if( *(oper_data->vpu_result) != RETCODE_INSUFFICIENT_BITSTREAM && *(oper_data->vpu_result) != RETCODE_INSUFFICIENT_BITSTREAM_BUF)
                    err("vmgr_4k_d2_out[0x%x] :: type = %d, vmgr_4k_d2_data.handle = 0x%x, cmd = 0x%x, frame_len %d \n", *(oper_data->vpu_result), oper_data->type, oper_data->handle, oper_data->cmd_type, vmgr_4k_d2_data.szFrame_Len);

                if(*(oper_data->vpu_result) == RETCODE_CODEC_EXIT)
                {
                	vmgr_4k_d2_restore_clock(0, vmgr_4k_d2_data.dev_opened);
					_vmgr_4k_d2_close_all(1);
                }
            }
        }
        else
        {
            printk("_vmgr_4k_d2_operation :: missed info or unknown command => type = 0x%x, cmd = 0x%x,  \n", oper_data->type, oper_data->cmd_type);
            *(oper_data->vpu_result) = RETCODE_FAILURE;
            oper_finished = 0;
        }

        if(oper_finished)
        {
            if(oper_data->comm_data != NULL && vmgr_4k_d2_data.dev_opened != 0)
            {
                //unsigned long flags;
                //spin_lock_irqsave(&(oper_data->comm_data->lock), flags);
                oper_data->comm_data->count += 1;
                if(oper_data->comm_data->count != 1){
                    dprintk("poll wakeup count = %d :: type(0x%x) cmd(0x%x) \n",oper_data->comm_data->count, oper_data->type, oper_data->cmd_type);
                }
                //spin_unlock_irqrestore(&(oper_data->comm_data->lock), flags);
                wake_up_interruptible(&(oper_data->comm_data->wq));
            }
            else{
                err("Error: abnormal exception or external command was processed!! 0x%p - %d\n", oper_data->comm_data, vmgr_4k_d2_data.dev_opened);
            }
        }
        else{
            err("Error: abnormal exception 2!! 0x%p - %d\n", oper_data->comm_data, vmgr_4k_d2_data.dev_opened);
        }

        vmgr_4k_d2_list_manager(oper_data, LIST_DEL);

        vmgr_4k_d2_data.cmd_processing = 0;
    }

    return 0;
}

static int _vmgr_4k_d2_thread(void *kthread)
{
    dprintk("_vmgr_4k_d2_thread for dec is running. \n");

    do
    {
//      detailk("_vmgr_4k_d2_thread wait_sleep \n");

        if(vmgr_4k_d2_list_manager(NULL, LIST_IS_EMPTY))
        {
            vmgr_4k_d2_data.cmd_processing = 0;

            //wait_event_interruptible(vmgr_4k_d2_data.comm_data.thread_wq, vmgr_4k_d2_data.comm_data.thread_intr > 0);
            wait_event_interruptible_timeout(vmgr_4k_d2_data.comm_data.thread_wq, vmgr_4k_d2_data.comm_data.thread_intr > 0, msecs_to_jiffies(50));
            vmgr_4k_d2_data.comm_data.thread_intr = 0;
        }
        else
        {
            if(vmgr_4k_d2_data.dev_opened || vmgr_4k_d2_data.external_proc){
                _vmgr_4k_d2_operation();
            }
            else{
                VpuList_t *oper_data = NULL;

                printk("DEL for empty \n");

                oper_data = vmgr_4k_d2_list_manager(NULL, LIST_GET_ENTRY);
                if(oper_data)
                    vmgr_4k_d2_list_manager(oper_data, LIST_DEL);
            }
        }

    } while (!kthread_should_stop());

    return 0;
}

//////////////////////////////////////////////////////////////////////////
static int _vmgr_4k_d2_mmap(struct file *file, struct vm_area_struct *vma)
{
#if defined(CONFIG_TCC_MEM)
    if(range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0){
        printk(KERN_ERR  "_vmgr_4k_d2_mmap: this address is not allowed \n");
        return -EAGAIN;
    }
#endif

    vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
    if(remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot))
    {
        printk("_vmgr_4k_d2_mmap :: remap_pfn_range failed\n");
        return -EAGAIN;
    }

    vma->vm_ops     = NULL;
    vma->vm_flags   |= VM_IO;
    vma->vm_flags   |= VM_DONTEXPAND | VM_PFNMAP;

    return 0;
}

static struct file_operations _vmgr_4k_d2_fops = {
    .open               = _vmgr_4k_d2_open,
    .release            = _vmgr_4k_d2_release,
    .mmap               = _vmgr_4k_d2_mmap,
    .unlocked_ioctl     = _vmgr_4k_d2_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl       = _vmgr_4k_d2_compat_ioctl,
#endif
};

static struct miscdevice _vmgr_4k_d2_misc_device =
{
    MISC_DYNAMIC_MINOR,
    VPU_4K_D2_MGR_NAME,
    &_vmgr_4k_d2_fops,
};

int vmgr_4k_d2_probe(struct platform_device *pdev)
{
    int ret;
    int type = 0;
    unsigned long int_flags;
    struct resource *res = NULL;

    if (pdev->dev.of_node == NULL) {
        return -ENODEV;
    }

    dprintk("hmgr initializing!! \n");
    memset(&vmgr_4k_d2_data, 0, sizeof(mgr_data_t));
    for(type=0; type<VPU_4K_D2_MAX; type++) {
        vmgr_4k_d2_data.closed[type] = 1;
    }

    vmgr_4k_d2_init_variable();

    vmgr_4k_d2_data.irq = platform_get_irq(pdev, 0);
    vmgr_4k_d2_data.nOpened_Count = 0;
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "missing phy memory resource\n");
        return -1;
    }
    res->end += 1;
    //vmgr_4k_d2_data.base_addr = regs->start;
    vmgr_4k_d2_data.base_addr = devm_ioremap(&pdev->dev, res->start, res->end - res->start);
    dprintk("============> VPU-4K-D2 VP9/HEVC base address [0x%x -> 0x%p], irq num [%d] \n", res->start, vmgr_4k_d2_data.base_addr, vmgr_4k_d2_data.irq - 32);

    vmgr_4k_d2_get_clock(pdev->dev.of_node);

    spin_lock_init(&(vmgr_4k_d2_data.oper_lock));
//  spin_lock_init(&(vmgr_4k_d2_data.comm_data.lock));

    init_waitqueue_head(&vmgr_4k_d2_data.comm_data.thread_wq);
    init_waitqueue_head(&vmgr_4k_d2_data.oper_wq);

    mutex_init(&vmgr_4k_d2_data.comm_data.list_mutex);
    mutex_init(&(vmgr_4k_d2_data.comm_data.io_mutex));

    INIT_LIST_HEAD(&vmgr_4k_d2_data.comm_data.main_list);
    INIT_LIST_HEAD(&vmgr_4k_d2_data.comm_data.wait_list);

    vmgr_4k_d2_init_interrupt();
    int_flags = vmgr_4k_d2_get_int_flags();
    ret = vmgr_4k_d2_request_irq(vmgr_4k_d2_data.irq, _vmgr_4k_d2_isr_handler, int_flags, VPU_4K_D2_MGR_NAME, &vmgr_4k_d2_data);
    if (ret) {
        err("to aquire vpu-4k-d2-vp9/hevc-dec-irq \n");
    }
    vmgr_4k_d2_data.irq_reged = 1;
    vmgr_4k_d2_disable_irq(vmgr_4k_d2_data.irq);

    kidle_task = kthread_run(_vmgr_4k_d2_thread, NULL, "v4K-D2_th");
    if( IS_ERR(kidle_task) )
    {
        err("unable to create thread!! \n");
        kidle_task = NULL;
        return -1;
    }
    dprintk("success :: thread created!! \n");

    _vmgr_4k_d2_close_all(1);

    if (misc_register(&_vmgr_4k_d2_misc_device))
    {
        printk(KERN_WARNING "VPU-4K-D2 VP9/HEVC Manager: Couldn't register device.\n");
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

    if( kidle_task ){
        kthread_stop(kidle_task);
        kidle_task = NULL;
    }

    devm_iounmap(&pdev->dev, vmgr_4k_d2_data.base_addr);
    if( vmgr_4k_d2_data.irq_reged ){
        vmgr_4k_d2_free_irq(vmgr_4k_d2_data.irq, &vmgr_4k_d2_data);
        vmgr_4k_d2_data.irq_reged = 0;
    }

    vmgr_4k_d2_put_clock();

    printk("success :: hmgr thread stopped!! \n");

    return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_remove);

#if defined(CONFIG_PM)
int vmgr_4k_d2_suspend(struct platform_device *pdev, pm_message_t state)
{
    int i, open_count = 0;

    if(vmgr_4k_d2_data.dev_opened != 0)
    {
        printk(" \n vpu_4k_d2: suspend In DEC(%d/%d/%d/%d/%d) \n", vmgr_4k_d2_get_close(VPU_DEC), vmgr_4k_d2_get_close(VPU_DEC_EXT),
			vmgr_4k_d2_get_close(VPU_DEC_EXT2), vmgr_4k_d2_get_close(VPU_DEC_EXT3), vmgr_4k_d2_get_close(VPU_DEC_EXT4));

        _vmgr_4k_d2_external_all_close(200);

        open_count = vmgr_4k_d2_data.dev_opened;
        for(i=0; i<open_count; i++) {
            vmgr_4k_d2_disable_clock(0, 0);
        }
        printk("vpu_4k_d2: suspend Out DEC(%d/%d/%d/%d/%d) \n\n", vmgr_4k_d2_get_close(VPU_DEC), vmgr_4k_d2_get_close(VPU_DEC_EXT),
			vmgr_4k_d2_get_close(VPU_DEC_EXT2), vmgr_4k_d2_get_close(VPU_DEC_EXT3), vmgr_4k_d2_get_close(VPU_DEC_EXT4));
    }

    return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_suspend);

int vmgr_4k_d2_resume(struct platform_device *pdev)
{
    int i, open_count = 0;

    if(vmgr_4k_d2_data.dev_opened != 0){

        open_count = vmgr_4k_d2_data.dev_opened;

        for(i=0; i<open_count; i++) {
            vmgr_4k_d2_enable_clock(0, 0);
        }
        printk("\n vpu_4k_d2: resume \n\n");
    }

    return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_resume);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu_4k_d2 vp9/hevc manager");
MODULE_LICENSE("GPL");

#endif
