/*
 *   FileName : hevc_mgr.c
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

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC

#include "hevc_mgr_sys.h"
#include <soc/tcc/pmap.h>
//#include <mach/tcc_fb.h>
#include <linux/of.h>
#include <linux/io.h>
#include "vpu_buffer.h"
#include "vpu_devices.h"
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#define dprintk(msg...) //printk( "TCC_HEVC_MGR: " msg);
#define detailk(msg...) //printk( "TCC_HEVC_MGR: " msg);
#define cmdk(msg...) //printk( "TCC_HEVC_MGR [Cmd]: " msg);
#define err(msg...) printk("TCC_HEVC_MGR [Err]: " msg);
//#define DATA_PRINT_ON
#ifdef DATA_PRINT_ON
    #define DATA_PRT printk
#endif

#define HEVC_REGISTER_DUMP
#define HEVC_DUMP_STATUS
//#define HEVC_REGISTER_INIT

//#define FORCED_ERROR //For test purpose!!
#ifdef FORCED_ERROR
#define FORCED_ERR_CNT 300
static int forced_error_count = FORCED_ERR_CNT;
#endif

#ifdef HEVC_DUMP_STATUS
#define W4_REG_BASE                 0x0000
#define W4_BS_RD_PTR                0x0130
#define W4_BS_WR_PTR                0x0134
#define W4_BS_OPTION                0x012C
#define W4_BS_PARAM                 0x0128

#define W4_VCPU_PDBG_RDATA_REG      0x001C
#define W4_VCPU_FIO_CTRL            0x0020
#define W4_VCPU_FIO_DATA            0x0024
#endif
/////////////////////////////////////////////////////////////////////////////
// Control only once!!
static mgr_data_t hmgr_data;
static struct task_struct *kidle_task = NULL;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
extern void do_gettimeofday(struct timeval *tv);
#endif
extern int tcc_hevc_dec( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );
VpuList_t* hmgr_list_manager(VpuList_t* args, unsigned int cmd);
/////////////////////////////////////////////////////////////////////////////

int hmgr_opened(void)
{
    if(hmgr_data.dev_opened == 0)
        return 0;
    return 1;
}
EXPORT_SYMBOL(hmgr_opened);

#ifdef HEVC_REGISTER_DUMP
#ifdef HEVC_DUMP_STATUS
static unsigned int _hmgr_FIORead(unsigned int addr)
{
    unsigned int ctrl;
    unsigned int count = 0;
    unsigned int data  = 0xffffffff;

    ctrl  = (addr&0xffff);
    ctrl |= (0<<16);    /* read operation */
    vetc_reg_write(hmgr_data.base_addr, W4_VCPU_FIO_CTRL, ctrl);
    count = 10000;
    while (count--) {
        ctrl = vetc_reg_read(hmgr_data.base_addr, W4_VCPU_FIO_CTRL);
        if (ctrl & 0x80000000) {
            data = vetc_reg_read(hmgr_data.base_addr, W4_VCPU_FIO_DATA);
            break;
        }
    }

    return data;
}

static int _hmgr_FIOWrite(unsigned int addr, unsigned int data)
{
    unsigned int ctrl;

    vetc_reg_write(hmgr_data.base_addr, W4_VCPU_FIO_DATA, data);
    ctrl  = (addr&0xffff);
    ctrl |= (1<<16);    /* write operation */
    vetc_reg_write(hmgr_data.base_addr, W4_VCPU_FIO_CTRL, ctrl);

    return 1;
}

static unsigned int _hmgr_ReadRegVCE(unsigned int vce_addr)
{
#define VCORE_DBG_ADDR              0x8300
#define VCORE_DBG_DATA              0x8304
#define VCORE_DBG_READY             0x8308
    int     vcpu_reg_addr;
    int     udata;

    _hmgr_FIOWrite(VCORE_DBG_READY, 0);

    vcpu_reg_addr = vce_addr >> 2;

    _hmgr_FIOWrite(VCORE_DBG_ADDR,vcpu_reg_addr + 0x8000);

    if (_hmgr_FIORead(VCORE_DBG_READY) == 1)
        udata= _hmgr_FIORead(VCORE_DBG_DATA);
    else
        udata = -1;

    return udata;
}

static void _hmgr_dump_status(void)
{
    int      rd, wr;
    unsigned int    tq, ip, mc, lf, index;
    unsigned int    avail_cu, avail_tu, avail_tc, avail_lf, avail_ip;
    unsigned int    vcpu_reg[31]= {0,};
    unsigned int     ctu_fsm, nb_fsm, cabac_fsm, cu_info, mvp_fsm, tc_busy, lf_fsm, bs_data, bbusy, fv;
    unsigned int    reg_val;
    int i = 0;

    printk("-------------------------------------------------------------------------------\n");
    printk("------                            VCPU STATUS                             -----\n");
    printk("-------------------------------------------------------------------------------\n");
    rd = vetc_reg_read(hmgr_data.base_addr, W4_BS_RD_PTR);
    wr = vetc_reg_read(hmgr_data.base_addr, W4_BS_WR_PTR);
    printk("RD_PTR: 0x%08x WR_PTR: 0x%08x BS_OPT: 0x%08x BS_PARAM: 0x%08x\n",
        rd, wr, vetc_reg_read(hmgr_data.base_addr, W4_BS_OPTION), vetc_reg_read(hmgr_data.base_addr, W4_BS_PARAM));

    // --------- VCPU register Dump
    printk("[+] VCPU REG Dump\n");
    for (index = 0; index < 25; index++) {
        vetc_reg_write(hmgr_data.base_addr, 0x14, (1<<9) | (index & 0xff));
        vcpu_reg[index] = vetc_reg_read(hmgr_data.base_addr, W4_VCPU_PDBG_RDATA_REG);

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
    printk("BITPC = 0x%08x\n", _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x18)));
    for ( i = 0; i < 10; i++)
    {
        printk("BITPC = 0x%08x\n", _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x18)));
    }

    printk("BIT START=0x%08x, BIT END=0x%08x\n", _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x11c)),
        _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x120)) );
    printk("BIT COMMAND 0x%x\n", _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x1FC)));


    // --------- BIT HEVC Status Dump
    ctu_fsm     = _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x48));
    nb_fsm      = _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x4c));
    cabac_fsm   = _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x50));
    cu_info     = _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x54));
    mvp_fsm     = _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x58));
    tc_busy     = _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x5c));
    lf_fsm      = _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x60));
    bs_data     = _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x64));
    bbusy       = _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x68));
    fv          = _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x6C));


    printk("[DEBUG-BPUHEVC] CTU_X: %4d, CTU_Y: %4d\n",  _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x40)), _hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x44)));
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
    tq = _hmgr_ReadRegVCE(0xd0);
    ip = _hmgr_ReadRegVCE(0xd4);
    mc = _hmgr_ReadRegVCE(0xd8);
    lf = _hmgr_ReadRegVCE(0xdc);
    avail_cu = (_hmgr_ReadRegVCE(0x11C)>>16) - (_hmgr_ReadRegVCE(0x110)>>16);
    avail_tu = (_hmgr_ReadRegVCE(0x11C)&0xFFFF) - (_hmgr_ReadRegVCE(0x110)&0xFFFF);
    avail_tc = (_hmgr_ReadRegVCE(0x120)>>16) - (_hmgr_ReadRegVCE(0x114)>>16);
    avail_lf = (_hmgr_ReadRegVCE(0x120)&0xFFFF) - (_hmgr_ReadRegVCE(0x114)&0xFFFF);
    avail_ip = (_hmgr_ReadRegVCE(0x124)>>16) - (_hmgr_ReadRegVCE(0x118)>>16);
    printk("       TQ            IP              MC             LF      GDI_EMPTY          ROOM \n");
    printk("------------------------------------------------------------------------------------------------------------\n");
    printk("| %d %04d %04d | %d %04d %04d |  %d %04d %04d | %d %04d %04d | 0x%08x | CU(%d) TU(%d) TC(%d) LF(%d) IP(%d)\n",
        (tq>>22)&0x07, (tq>>11)&0x3ff, tq&0x3ff,
        (ip>>22)&0x07, (ip>>11)&0x3ff, ip&0x3ff,
        (mc>>22)&0x07, (mc>>11)&0x3ff, mc&0x3ff,
        (lf>>22)&0x07, (lf>>11)&0x3ff, lf&0x3ff,
        _hmgr_FIORead(0x88f4),                      /* GDI empty */
        avail_cu, avail_tu, avail_tc, avail_lf, avail_ip);
    /* CU/TU Queue count */
    reg_val = _hmgr_ReadRegVCE(0x12C);
    printk("[DCIDEBUG] QUEUE COUNT: CU(%5d) TU(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);
    reg_val = _hmgr_ReadRegVCE(0x1A0);
    printk("TC(%5d) IP(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);
    reg_val = _hmgr_ReadRegVCE(0x1A4);
    printk("LF(%5d)\n", (reg_val>>16)&0xffff);
    printk("VALID SIGNAL : CU0(%d)  CU1(%d)  CU2(%d) TU(%d) TC(%d) IP(%5d) LF(%5d)\n"
        "               DCI_FALSE_RUN(%d) VCE_RESET(%d) CORE_INIT(%d) SET_RUN_CTU(%d) \n",
        (reg_val>>6)&1, (reg_val>>5)&1, (reg_val>>4)&1, (reg_val>>3)&1,
        (reg_val>>2)&1, (reg_val>>1)&1, (reg_val>>0)&1,
        (reg_val>>10)&1, (reg_val>>9)&1, (reg_val>>8)&1, (reg_val>>7)&1);

    printk("State TQ: 0x%08x IP: 0x%08x MC: 0x%08x LF: 0x%08x\n",
        _hmgr_ReadRegVCE(0xd0), _hmgr_ReadRegVCE(0xd4), _hmgr_ReadRegVCE(0xd8), _hmgr_ReadRegVCE(0xdc));
    printk("BWB[1]: RESPONSE_CNT(0x%08x) INFO(0x%08x)\n", _hmgr_ReadRegVCE(0x194), _hmgr_ReadRegVCE(0x198));
    printk("BWB[2]: RESPONSE_CNT(0x%08x) INFO(0x%08x)\n", _hmgr_ReadRegVCE(0x194), _hmgr_ReadRegVCE(0x198));
    printk("DCI INFO\n");
    printk("READ_CNT_0 : 0x%08x\n", _hmgr_ReadRegVCE(0x110));
    printk("READ_CNT_1 : 0x%08x\n", _hmgr_ReadRegVCE(0x114));
    printk("READ_CNT_2 : 0x%08x\n", _hmgr_ReadRegVCE(0x118));
    printk("WRITE_CNT_0: 0x%08x\n", _hmgr_ReadRegVCE(0x11c));
    printk("WRITE_CNT_1: 0x%08x\n", _hmgr_ReadRegVCE(0x120));
    printk("WRITE_CNT_2: 0x%08x\n", _hmgr_ReadRegVCE(0x124));
    reg_val = _hmgr_ReadRegVCE(0x128);
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

    printk("-------------------------------------------------------------------------------\n");
}

#endif
#endif

int hmgr_get_close(vputype type)
{
    return hmgr_data.closed[type];
}

int hmgr_get_alive(void)
{
    return hmgr_data.dev_opened;
}

int hmgr_set_close(vputype type, int value, int bfreemem)
{
    if( hmgr_get_close(type) == value ){
        dprintk(" %d was already set into %d. \n", type, value);
        return -1;
    }

    hmgr_data.closed[type] = value;
    if(value == 1){
        hmgr_data.handle[type] = 0x00;

        if(bfreemem)
            vmem_proc_free_memory(type);
    }

    return 0;
}

static void _hmgr_close_all(int bfreemem)
{
    hmgr_set_close(VPU_DEC, 1, bfreemem);
    hmgr_set_close(VPU_DEC_EXT, 1, bfreemem);
    hmgr_set_close(VPU_DEC_EXT2, 1, bfreemem);
    hmgr_set_close(VPU_DEC_EXT3, 1, bfreemem);
    hmgr_set_close(VPU_DEC_EXT4, 1, bfreemem);
}

int hmgr_process_ex(VpuList_t *cmd_list, vputype type, int Op, int *result)
{
    if(hmgr_data.dev_opened == 0)
        return 0;

    printk(" \n process_ex %d - 0x%x \n\n", type, Op);

    if(!hmgr_get_close(type))
    {
        cmd_list->type          = type;
        cmd_list->cmd_type      = Op;
        cmd_list->handle        = hmgr_data.handle[type];
        cmd_list->args          = NULL;
        cmd_list->comm_data     = NULL;
        cmd_list->vpu_result    = result;
        hmgr_list_manager(cmd_list, LIST_ADD);

        return 1;
    }

    return 1;
}

static int _hmgr_internal_handler(void)
{
    int ret, ret_code = RETCODE_SUCCESS;
    int timeout = 200;

    if(hmgr_data.check_interrupt_detection)
    {
        if(hmgr_data.oper_intr > 0)
        {
            detailk("Success 1: hevc operation!! \n");
            ret_code = RETCODE_SUCCESS;
        }
        else
        {
            ret = wait_event_interruptible_timeout(hmgr_data.oper_wq, hmgr_data.oper_intr > 0, msecs_to_jiffies(timeout));

            if(hmgr_is_loadable() > 0)
                ret_code = RETCODE_CODEC_EXIT;
            else
            if(hmgr_data.oper_intr > 0)
            {
                detailk("Success 2: hevc operation!! \n");
#if defined(FORCED_ERROR)
                if(forced_error_count-- <= 0)
                {
                    ret_code = RETCODE_CODEC_EXIT;
                    forced_error_count = FORCED_ERR_CNT;
                    vetc_dump_reg_all(hmgr_data.base_addr, "_hmgr_internal_handler force-timed_out");
                }
                else
#endif
                ret_code = RETCODE_SUCCESS;
            }
            else
            {
                err("[CMD 0x%x][%d]: hevc timed_out(ref %d msec) => oper_intr[%d]!! [%d]th frame len %d\n", hmgr_data.current_cmd, ret, timeout, hmgr_data.oper_intr, hmgr_data.nDecode_Cmd, hmgr_data.szFrame_Len);
                vetc_dump_reg_all(hmgr_data.base_addr, "_hmgr_internal_handler timed_out");
                _hmgr_dump_status();
                ret_code = RETCODE_CODEC_EXIT;
            }
        }

        hmgr_data.oper_intr = 0;
        hmgr_status_clear(hmgr_data.base_addr);
    }

    return ret_code;
}

static int _hmgr_process(vputype type, int cmd, long pHandle, void* args)
{
    int ret = 0;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
    struct timeval t1 , t2;
    int time_gap_ms = 0;
#endif

    hmgr_data.check_interrupt_detection = 0;
    hmgr_data.current_cmd = cmd;

    if( type < VPU_ENC )
    {
        if( cmd != VPU_DEC_INIT && cmd != VPU_DEC_INIT_KERNEL)
        {
            if(hmgr_get_close(type) || hmgr_data.handle[type] == 0x00)
                return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
        }

        if( cmd != VPU_DEC_BUF_FLAG_CLEAR && cmd != VPU_DEC_DECODE && cmd != VPU_DEC_BUF_FLAG_CLEAR_KERNEL && cmd != VPU_DEC_DECODE_KERNEL) {
            cmdk("@@@@@@@@@@ Decoder(%d), command: 0x%x \n", type, cmd);
        }

        switch(cmd)
        {
            case VPU_DEC_INIT:
            case VPU_DEC_INIT_KERNEL:
                {
                    HEVC_INIT_t* arg;

                    arg = (HEVC_INIT_t *)args;
                    hmgr_data.handle[type] = 0x00;

                    arg->gsHevcDecInit.m_RegBaseVirtualAddr = (codec_addr_t)hmgr_data.base_addr;
                    arg->gsHevcDecInit.m_Memcpy             = (void* (*) ( void*, const void*, unsigned int, unsigned int ))vetc_memcpy;
                    arg->gsHevcDecInit.m_Memset             = (void  (*) ( void*, int, unsigned int, unsigned int ))vetc_memset;
                    arg->gsHevcDecInit.m_Interrupt          = (int  (*) ( void ))_hmgr_internal_handler;
                    arg->gsHevcDecInit.m_Ioremap            = (void* (*) ( phys_addr_t, unsigned int ))vetc_ioremap;
                    arg->gsHevcDecInit.m_Iounmap            = (void  (*) ( void* ))vetc_iounmap;
                    arg->gsHevcDecInit.m_reg_read           = (unsigned int (*)(void *, unsigned int))vetc_reg_read;
                    arg->gsHevcDecInit.m_reg_write          = (void (*)(void *, unsigned int, unsigned int))vetc_reg_write;

                    hmgr_data.check_interrupt_detection = 1;
                    dprintk("@@ Dec :: Init In => workbuff 0x%x/0x%x, Reg: 0x%p/0x%x, format : %d, Stream(0x%x/0x%x, 0x%x)\n",
                                arg->gsHevcDecInit.m_BitWorkAddr[PA], arg->gsHevcDecInit.m_BitWorkAddr[VA], hmgr_data.base_addr, arg->gsHevcDecInit.m_RegBaseVirtualAddr,
                                arg->gsHevcDecInit.m_iBitstreamFormat, arg->gsHevcDecInit.m_BitstreamBufAddr[PA], arg->gsHevcDecInit.m_BitstreamBufAddr[VA], arg->gsHevcDecInit.m_iBitstreamBufSize);
                    dprintk("@@ Dec :: Init In => optFlag 0x%x, Userdata(%d), Inter: %d, PlayEn: %d\n",
                                arg->gsHevcDecInit.m_uiDecOptFlags,
                                arg->gsHevcDecInit.m_bEnableUserData, arg->gsHevcDecInit.m_bCbCrInterleaveMode,
                                arg->gsHevcDecInit.m_iFilePlayEnable);

                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (void*)(&arg->gsHevcDecHandle), (void*)(&arg->gsHevcDecInit), (void*)NULL);
                    if( ret != RETCODE_SUCCESS ){
                        printk("@@ Dec :: Init Done with ret(0x%x)\n", ret);
                        if( ret != RETCODE_CODEC_EXIT ){
                            vetc_dump_reg_all(hmgr_data.base_addr, "init failure");
                            //ret = tcc_hevc_dec(cmd, (void*)(&arg->gsHevcDecHandle), (void*)(&arg->gsHevcDecInit), (void*)NULL);
                            //printk("@@ Dec :: Init Done with ret(0x%x)\n", ret);
                            //vetc_dump_reg_all(hmgr_data.base_addr, "init success");
                        }
                    }
                    if( ret != RETCODE_CODEC_EXIT && arg->gsHevcDecHandle != 0) {
                        hmgr_data.handle[type] = arg->gsHevcDecHandle;
                        hmgr_set_close(type, 0, 0);
                        cmdk("@@ Dec :: hmgr_data.handle = 0x%x \n", arg->gsHevcDecHandle);
                    }
                    else{
                        //To free memory!!
                        hmgr_set_close(type, 0, 0);
                        hmgr_set_close(type, 1, 1);
                    }
                    dprintk("@@ Dec :: Init Done Handle(0x%x) \n", arg->gsHevcDecHandle);

            #ifdef CONFIG_VPU_TIME_MEASUREMENT
                    hmgr_data.iTime[type].print_out_index = hmgr_data.iTime[type].proc_base_cnt = 0;
                    hmgr_data.iTime[type].accumulated_proc_time = hmgr_data.iTime[type].accumulated_frame_cnt = 0;
                    hmgr_data.iTime[type].proc_time_30frames = 0;
            #endif
                }
                break;

            case VPU_DEC_SEQ_HEADER:
            case VPU_DEC_SEQ_HEADER_KERNEL:
                {
                    HEVC_SEQ_HEADER_t *arg;
                    int iSize;

                    arg = (HEVC_SEQ_HEADER_t *)args;
                    hmgr_data.szFrame_Len = iSize = (int)arg->stream_size;
                    hmgr_data.check_interrupt_detection = 1;
                    hmgr_data.nDecode_Cmd = 0;
                    dprintk("@@ Dec :: HEVC_DEC_SEQ_HEADER in :: size(%d) \n", arg->stream_size);
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)iSize, (void*)(&arg->gsHevcDecInitialInfo));
                    printk("@@ Dec :: HEVC_DEC_SEQ_HEADER out 0x%x \n res info. %d - %d - %d, %d - %d - %d \n", ret, arg->gsHevcDecInitialInfo.m_iPicWidth, arg->gsHevcDecInitialInfo.m_PicCrop.m_iCropLeft, arg->gsHevcDecInitialInfo.m_PicCrop.m_iCropRight,
                                            arg->gsHevcDecInitialInfo.m_iPicHeight, arg->gsHevcDecInitialInfo.m_PicCrop.m_iCropTop, arg->gsHevcDecInitialInfo.m_PicCrop.m_iCropBottom);
                }
                break;

            case VPU_DEC_REG_FRAME_BUFFER:
            case VPU_DEC_REG_FRAME_BUFFER_KERNEL:
                {
                    HEVC_SET_BUFFER_t *arg;

                    arg = (HEVC_SET_BUFFER_t *)args;
                    dprintk("@@ Dec :: HEVC_DEC_REG_FRAME_BUFFER in :: 0x%x/0x%x \n", arg->gsHevcDecBuffer.m_FrameBufferStartAddr[0], arg->gsHevcDecBuffer.m_FrameBufferStartAddr[1]);
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(&arg->gsHevcDecBuffer), (void*)NULL);
                    dprintk("@@ Dec :: HEVC_DEC_REG_FRAME_BUFFER out \n");
                }
                break;

            case VPU_DEC_DECODE:
            case VPU_DEC_DECODE_KERNEL:
                {
                    HEVC_DECODE_t *arg;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
                    do_gettimeofday( &t1 );
#endif
                    arg = (HEVC_DECODE_t *)args;

                    hmgr_data.szFrame_Len = arg->gsHevcDecInput.m_iBitstreamDataSize;
                    dprintk("@@ Dec :: Dec In => 0x%x - 0x%x, 0x%x, 0x%x - 0x%x, %d, flag: %d  \n", arg->gsHevcDecInput.m_BitstreamDataAddr[PA], arg->gsHevcDecInput.m_BitstreamDataAddr[VA], arg->gsHevcDecInput.m_iBitstreamDataSize,
                            arg->gsHevcDecInput.m_UserDataAddr[PA], arg->gsHevcDecInput.m_UserDataAddr[VA], arg->gsHevcDecInput.m_iUserDataBufferSize,
                            arg->gsHevcDecInput.m_iSkipFrameMode);

                    //DEBUG
                    #ifdef DATA_PRINT_ON
                    {
                        unsigned char* ptr = (unsigned char*)arg->gsHevcDecInput.m_BitstreamDataAddr[VA];
                        int i, datasize = 32;
                        DATA_PRT("====================data = %d\n", arg->gsHevcDecInput.m_iBitstreamDataSize);
                        if (arg->gsHevcDecInput.m_iBitstreamDataSize < 32) {
                            datasize = arg->gsHevcDecInput.m_iBitstreamDataSize;
                        }
                        for (i = 0; i < datasize; i++) {
                            DATA_PRT("0x%02x ", ptr[i]);
                        }
                        DATA_PRT("=====================\n");
                    }
                    #endif

                    hmgr_data.check_interrupt_detection = 1;
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(&arg->gsHevcDecInput), (void*)(&arg->gsHevcDecOutput));

                    dprintk("@@ Dec :: Dec Out => %d - %d - %d, %d - %d - %d \n", arg->gsHevcDecOutput.m_DecOutInfo.m_iDisplayWidth, arg->gsHevcDecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft,
                                    arg->gsHevcDecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropRight, arg->gsHevcDecOutput.m_DecOutInfo.m_iDisplayHeight,
                                    arg->gsHevcDecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropTop, arg->gsHevcDecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom);

                    dprintk("@@ Dec :: Dec Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], OutStatus[%d/%d] \n", ret, arg->gsHevcDecOutput.m_DecOutInfo.m_iPicType,
                                        arg->gsHevcDecOutput.m_DecOutInfo.m_iDispOutIdx, arg->gsHevcDecOutput.m_DecOutInfo.m_iDecodedIdx,
                                        arg->gsHevcDecOutput.m_DecOutInfo.m_iOutputStatus, arg->gsHevcDecOutput.m_DecOutInfo.m_iDecodingStatus);
                    dprintk("@@ Dec :: Dec Out => dec_Idx(%d), 0x%x 0x%x 0x%x / 0x%x 0x%x 0x%x \n", arg->gsHevcDecOutput.m_DecOutInfo.m_iDispOutIdx,
                                (unsigned int)arg->gsHevcDecOutput.m_pDispOut[PA][0], (unsigned int)arg->gsHevcDecOutput.m_pDispOut[PA][1], (unsigned int)arg->gsHevcDecOutput.m_pDispOut[PA][2],
                                (unsigned int)arg->gsHevcDecOutput.m_pDispOut[VA][0], (unsigned int)arg->gsHevcDecOutput.m_pDispOut[VA][1], (unsigned int)arg->gsHevcDecOutput.m_pDispOut[VA][2]);
                    dprintk("@@ Dec :: Dec Out => disp_Idx(%d), 0x%x 0x%x 0x%x / 0x%x 0x%x 0x%x \n", arg->gsHevcDecOutput.m_DecOutInfo.m_iDecodedIdx,
                                (unsigned int)arg->gsHevcDecOutput.m_pCurrOut[PA][0], (unsigned int)arg->gsHevcDecOutput.m_pCurrOut[PA][1], (unsigned int)arg->gsHevcDecOutput.m_pCurrOut[PA][2],
                                (unsigned int)arg->gsHevcDecOutput.m_pCurrOut[VA][0], (unsigned int)arg->gsHevcDecOutput.m_pCurrOut[VA][1], (unsigned int)arg->gsHevcDecOutput.m_pCurrOut[VA][2]);

                    if(arg->gsHevcDecOutput.m_DecOutInfo.m_iDecodingStatus == VPU_DEC_BUF_FULL){
                        err("Buffer full\n");
                        //vetc_dump_reg_all(hmgr_data.base_addr, "Buffer full");
                    }
                    hmgr_data.nDecode_Cmd++;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
                    do_gettimeofday( &t2 );
#endif
                }
                break;

            case VPU_DEC_BUF_FLAG_CLEAR:
            case VPU_DEC_BUF_FLAG_CLEAR_KERNEL:
                {
                    int *arg;

                    arg = (int *)args;
                    dprintk("@@ Dec :: DispIdx Clear %d \n", *arg);
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(arg), (void*)NULL);
                }
                break;

            case VPU_DEC_FLUSH_OUTPUT:
            case VPU_DEC_FLUSH_OUTPUT_KERNEL:
                {
                    HEVC_DECODE_t *arg;

                    arg = (HEVC_DECODE_t *)args;
                    printk("@@ Dec :: HEVC_DEC_FLUSH_OUTPUT !! \n");
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(&arg->gsHevcDecInput), (void*)(&arg->gsHevcDecOutput));
                }
                break;

            case VPU_DEC_CLOSE:
            case VPU_DEC_CLOSE_KERNEL:
                {
                    //HEVC_DECODE_t *arg;

                    //arg = (HEVC_DECODE_t *)args;
                    hmgr_data.check_interrupt_detection = 1;
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)NULL, (void*)NULL/*(&arg->gsHevcDecOutput)*/);
                    dprintk("@@ Dec :: HEVC_DEC_CLOSED !! \n");
                    hmgr_set_close(type, 1, 1);
                }
                break;

            case GET_RING_BUFFER_STATUS:
            case GET_RING_BUFFER_STATUS_KERNEL:
                {
                    HEVC_RINGBUF_GETINFO_t *arg;

                    arg = (HEVC_RINGBUF_GETINFO_t *)args;
                    hmgr_data.check_interrupt_detection = 1;

                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)NULL, (void*)(&arg->gsHevcDecRingStatus));
                }
                break;
            case FILL_RING_BUFFER_AUTO:
            case FILL_RING_BUFFER_AUTO_KERNEL:
                {
                    HEVC_RINGBUF_SETBUF_t *arg;

                    arg = (HEVC_RINGBUF_SETBUF_t *)args;
                    hmgr_data.check_interrupt_detection = 1;
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(&arg->gsHevcDecInit), (void*)(&arg->gsHevcDecRingFeed));
                    dprintk("@@ Dec :: ReadPTR : 0x%08x, WritePTR : 0x%08x\n", vetc_reg_read(hmgr_data.base_addr, 0x120), vetc_reg_read(hmgr_data.base_addr, 0x124));
                }
                break;

            case VPU_UPDATE_WRITE_BUFFER_PTR:
            case VPU_UPDATE_WRITE_BUFFER_PTR_KERNEL:
                {
                    HEVC_RINGBUF_SETBUF_PTRONLY_t *arg;

                    arg = (HEVC_RINGBUF_SETBUF_PTRONLY_t *)args;
                    hmgr_data.check_interrupt_detection = 1;
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(arg->iCopiedSize), (void*)(arg->iFlushBuf));
                }
                break;

            case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY:
            case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_KERNEL:
                {
                    HEVC_SEQ_HEADER_t *arg;

                    arg = (HEVC_SEQ_HEADER_t *)args;
                    hmgr_data.check_interrupt_detection = 1;
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(&arg->gsHevcDecInitialInfo), NULL);
                }
                break;

            case VPU_CODEC_GET_VERSION:
            case VPU_CODEC_GET_VERSION_KERNEL:
                {
                    HEVC_GET_VERSION_t *arg;

                    arg = (HEVC_GET_VERSION_t *)args;
                    hmgr_data.check_interrupt_detection = 1;
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, arg->pszVersion, arg->pszBuildData);
                    dprintk("@@ Dec :: version : %s, build : %s\n", arg->pszVersion, arg->pszBuildData);
                }
                break;

            case VPU_DEC_SWRESET:
            case VPU_DEC_SWRESET_KERNEL:
                {
                    ret = tcc_hevc_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, NULL, NULL);
                }
                break;

            default:
                {
                    err("@@ Dec :: not supported command(0x%x) \n", cmd);
                    return 0x999;
                }
                break;
        }
    }
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
    else
    {
        err("@@ Enc :: Encoder for HEVC do not support. command(0x%x) \n", cmd);
        return 0x999;
    }
#endif

#ifdef CONFIG_VPU_TIME_MEASUREMENT
    time_gap_ms = vetc_GetTimediff_ms(t1, t2);

    if( cmd == VPU_DEC_DECODE || cmd == VPU_ENC_ENCODE ) {
        hmgr_data.iTime[type].accumulated_frame_cnt++;
        hmgr_data.iTime[type].proc_time[hmgr_data.iTime[type].proc_base_cnt] = time_gap_ms;
        hmgr_data.iTime[type].proc_time_30frames += time_gap_ms;
        hmgr_data.iTime[type].accumulated_proc_time += time_gap_ms;
        if(hmgr_data.iTime[type].proc_base_cnt != 0 && hmgr_data.iTime[type].proc_base_cnt % 29 == 0)
        {
            printk("HEVC[%d] :: Dec[%4d] time %2d.%2d / %2d.%2d ms: %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d \n",
                        type,
                        hmgr_data.iTime[type].print_out_index,
                        hmgr_data.iTime[type].proc_time_30frames/30,
                        ((hmgr_data.iTime[type].proc_time_30frames%30)*100)/30,
                        hmgr_data.iTime[type].accumulated_proc_time/hmgr_data.iTime[type].accumulated_frame_cnt,
                        ((hmgr_data.iTime[type].accumulated_proc_time%hmgr_data.iTime[type].accumulated_frame_cnt)*100)/hmgr_data.iTime[type].accumulated_frame_cnt,
                        hmgr_data.iTime[type].proc_time[0], hmgr_data.iTime[type].proc_time[1], hmgr_data.iTime[type].proc_time[2], hmgr_data.iTime[type].proc_time[3], hmgr_data.iTime[type].proc_time[4],
                        hmgr_data.iTime[type].proc_time[5], hmgr_data.iTime[type].proc_time[6], hmgr_data.iTime[type].proc_time[7], hmgr_data.iTime[type].proc_time[8], hmgr_data.iTime[type].proc_time[9],
                        hmgr_data.iTime[type].proc_time[10], hmgr_data.iTime[type].proc_time[11], hmgr_data.iTime[type].proc_time[12], hmgr_data.iTime[type].proc_time[13], hmgr_data.iTime[type].proc_time[14],
                        hmgr_data.iTime[type].proc_time[15], hmgr_data.iTime[type].proc_time[16], hmgr_data.iTime[type].proc_time[17], hmgr_data.iTime[type].proc_time[18], hmgr_data.iTime[type].proc_time[19],
                        hmgr_data.iTime[type].proc_time[20], hmgr_data.iTime[type].proc_time[21], hmgr_data.iTime[type].proc_time[22], hmgr_data.iTime[type].proc_time[23], hmgr_data.iTime[type].proc_time[24],
                        hmgr_data.iTime[type].proc_time[25], hmgr_data.iTime[type].proc_time[26], hmgr_data.iTime[type].proc_time[27], hmgr_data.iTime[type].proc_time[28], hmgr_data.iTime[type].proc_time[29]);
            hmgr_data.iTime[type].proc_base_cnt = 0;
            hmgr_data.iTime[type].proc_time_30frames = 0;
            hmgr_data.iTime[type].print_out_index++;
        }
        else{
            hmgr_data.iTime[type].proc_base_cnt++;
        }
    }
#endif

    return ret;
}

static long _hmgr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    CONTENTS_INFO info;
    OPENED_sINFO open_info;

    mutex_lock(&hmgr_data.comm_data.io_mutex);

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
                    if (copy_from_user(&type, (unsigned int*)arg, sizeof(unsigned int)))
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
                    if(NULL == memcpy(&open_info,(OPENED_sINFO*)arg, sizeof(OPENED_sINFO)))
                        ret = -EFAULT;
                } else {
                    if(copy_from_user(&open_info,(OPENED_sINFO*)arg, sizeof(OPENED_sINFO)))
                        ret = -EFAULT;
                }

                if(ret == 0) {
                    if(open_info.opened_cnt != 0)
                        vmem_set_only_decode_mode(open_info.type);
                    ret = 0;
                }
            }

            break;

        case VPU_CHECK_CODEC_STATUS:
        case VPU_CHECK_CODEC_STATUS_KERNEL:
            {
                if(cmd == VPU_CHECK_CODEC_STATUS_KERNEL) {
                    if (NULL == memcpy((int*)arg, hmgr_data.closed, sizeof(hmgr_data.closed)))
                        ret = -EFAULT;
                } else {
                    if (copy_to_user((int*)arg, hmgr_data.closed, sizeof(hmgr_data.closed)))
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
                        if(NULL == memcpy((int*)arg, &iInst, sizeof(INSTANCE_INFO)))
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

        default:
            err("Unsupported ioctl[%d]!!!\n", cmd);
            ret = -EINVAL;
            break;
    }

    mutex_unlock(&hmgr_data.comm_data.io_mutex);

    return ret;
}

static irqreturn_t _hmgr_isr_handler(int irq, void *dev_id)
{
    unsigned long flags;

    detailk("_hmgr_isr_handler \n");

    spin_lock_irqsave(&(hmgr_data.oper_lock), flags);
    hmgr_data.oper_intr++;
    spin_unlock_irqrestore(&(hmgr_data.oper_lock), flags);

    wake_up_interruptible(&(hmgr_data.oper_wq));

    return IRQ_HANDLED;
}

static int _hmgr_proc_exit_by_external(struct VpuList *list, int *result, unsigned int type)
{
    if(!hmgr_get_close(type) && hmgr_data.handle[type] != 0x00)
    {
        list->type = type;
        if( type >= VPU_ENC )
            list->cmd_type = VPU_ENC_CLOSE;
        else
            list->cmd_type = VPU_DEC_CLOSE;
        list->handle    = hmgr_data.handle[type];
        list->args      = NULL;
        list->comm_data = NULL;
        list->vpu_result = result;

        printk("_hmgr_proc_exit_by_external for %d!! \n", type);
        hmgr_list_manager(list, LIST_ADD);

        return 1;
    }

    return 0;
}

static int _hmgr_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

    if (!hmgr_data.irq_reged) {
        err("not registered hevc-mgr-irq \n");
    }

    dprintk("_hmgr_open In!! %d'th \n", hmgr_data.dev_opened);

    hmgr_enable_clock();

    if(hmgr_data.dev_opened == 0)
    {
#ifdef FORCED_ERROR
        forced_error_count = FORCED_ERR_CNT;
#endif
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
        hmgr_data.only_decmode = 0;
#else
        hmgr_data.only_decmode = 1;
#endif
        hmgr_data.clk_limitation = 1;
        //hmgr_hw_reset();
        hmgr_data.cmd_processing = 0;

        hmgr_enable_irq(hmgr_data.irq);
        vetc_reg_init(hmgr_data.base_addr);
        if(0 > vmem_init())
	    {
	        err("failed to allocate memory for VPU!! %d \n", ret);
	        return -ENOMEM;
	    }
    }
    hmgr_data.dev_opened++;

    filp->private_data = &hmgr_data;

    return 0;
}

static void _hmgr_wait_process(int wait_ms)
{
    int max_count = wait_ms/20;

    //wait!! in case exceptional processing. ex). sdcard out!!
    while(hmgr_data.cmd_processing)
    {
        max_count--;
        msleep(20);

        if(max_count <= 0)
        {
            err("cmd_processing(cmd %d) didn't finish!! \n", hmgr_data.current_cmd);
            break;
        }
    }
}

static int _hmgr_external_all_close(int wait_ms)
{
    int type = 0;
    int max_count = 0;
    int ret;

    for(type = 0; type < HEVC_MAX; type++)
    {
        if(_hmgr_proc_exit_by_external(&hmgr_data.vList[type], &ret, type))
        {
            max_count = wait_ms/10;
            while(!hmgr_get_close(type))
            {
                max_count--;
                msleep(10);
            }
        }
    }

    return 0;
}

static int _hmgr_release(struct inode *inode, struct file *filp)
{
    dprintk("_hmgr_release In!! %d'th \n", hmgr_data.dev_opened);

    _hmgr_wait_process(2000);

    if(hmgr_data.dev_opened > 0)
        hmgr_data.dev_opened--;
    if(hmgr_data.dev_opened == 0)
    {
//////////////////////////////////////
        int type = 0, alive_cnt = 0;

#if 1 // To close whole hevc instance when being killed process opened this.
        hmgr_data.external_proc = 1;
        _hmgr_external_all_close(200);
        _hmgr_wait_process(2000);
        hmgr_data.external_proc = 0;
#endif

        for(type=0; type<HEVC_MAX; type++) {
            if( hmgr_data.closed[type] == 0 ){
                alive_cnt++;
            }
        }

        if( alive_cnt )
        {
            // clear instances of hevc by force.
            //TCC_VPU_DEC( 0x40, (void*)NULL, (void*)NULL, (void*)NULL);
            printk("HEVC might be cleared by force. \n");
        }

//////////////////////////////////////
        hmgr_data.oper_intr = 0;
        hmgr_data.cmd_processing = 0;

        _hmgr_close_all(1);
//////////////////////////////////////

        hmgr_disable_irq(hmgr_data.irq);
        hmgr_BusPrioritySetting(BUS_FOR_NORMAL, 0);

		vmem_deinit();
    }

    hmgr_disable_clock();

    hmgr_data.nOpened_Count++;

    printk("_hmgr_release Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d) \n", hmgr_data.dev_opened, hmgr_data.nOpened_Count,
                    hmgr_get_close(VPU_DEC), hmgr_get_close(VPU_DEC_EXT), hmgr_get_close(VPU_DEC_EXT2), hmgr_get_close(VPU_DEC_EXT3), hmgr_get_close(VPU_DEC_EXT4));

    return 0;
}

VpuList_t* hmgr_list_manager(VpuList_t* args, unsigned int cmd)
{
    VpuList_t *ret;

    mutex_lock(&hmgr_data.comm_data.list_mutex);

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
                    list_add_tail(&data->list, &hmgr_data.comm_data.main_list);hmgr_data.cmd_queued++;
                    hmgr_data.comm_data.thread_intr++;
                    wake_up_interruptible(&(hmgr_data.comm_data.thread_wq));
                }
                break;

            case LIST_DEL:
                if(!args)
                {
                    err("DEL :: data is null \n");
                    goto Error;
                }
                data = (VpuList_t*)args;
                list_del(&data->list);hmgr_data.cmd_queued--;
                break;

            case LIST_IS_EMPTY:
                if(list_empty(&hmgr_data.comm_data.main_list))
                    ret =(VpuList_t*)0x1234;
                break;

            case LIST_GET_ENTRY:
                ret =  list_first_entry(&hmgr_data.comm_data.main_list, VpuList_t, list);
                break;
        }
    }

Error:
    mutex_unlock(&hmgr_data.comm_data.list_mutex);

    return ret;
}

//////////////////////////////////////////////////////////////////////////
// HEVC Thread!!
static int _hmgr_operation(void)
{
    int oper_finished;
    VpuList_t *oper_data = NULL;

    while(!hmgr_list_manager(NULL, LIST_IS_EMPTY))
    {
        hmgr_data.cmd_processing = 1;

        oper_finished = 1;
        dprintk("_hmgr_operation :: not empty cmd_queued(%d) \n", hmgr_data.cmd_queued);

        oper_data = (VpuList_t*)hmgr_list_manager(NULL, LIST_GET_ENTRY);
        if(!oper_data) {
            err("data is null \n");
            hmgr_data.cmd_processing = 0;
            return 0;
        }
        *(oper_data->vpu_result) |= RET2;

        dprintk("_hmgr_operation [%d] :: cmd = 0x%x, hmgr_data.cmd_queued(%d) \n", oper_data->type, oper_data->cmd_type, hmgr_data.cmd_queued);

        if(oper_data->type < HEVC_MAX && oper_data != NULL /*&& oper_data->comm_data != NULL*/)
        {
            *(oper_data->vpu_result) |= RET3;

            *(oper_data->vpu_result) = _hmgr_process(oper_data->type, oper_data->cmd_type, oper_data->handle, oper_data->args);
            oper_finished = 1;
            if(*(oper_data->vpu_result) != RETCODE_SUCCESS)
            {
                if( *(oper_data->vpu_result) != RETCODE_INSUFFICIENT_BITSTREAM && *(oper_data->vpu_result) != RETCODE_INSUFFICIENT_BITSTREAM_BUF)
                    err("hmgr_out[0x%x] :: type = %d, hmgr_data.handle = 0x%x, cmd = 0x%x, frame_len %d \n", *(oper_data->vpu_result), oper_data->type, oper_data->handle, oper_data->cmd_type, hmgr_data.szFrame_Len);

                if(*(oper_data->vpu_result) == RETCODE_CODEC_EXIT)
                {
                    int opened_count = hmgr_data.dev_opened;

            #if 1
                    while(opened_count)
                    {
                        hmgr_disable_clock();
                        if(opened_count > 0)
                            opened_count--;
                    }

                    //msleep(1);
                    opened_count = hmgr_data.dev_opened;
                    while(opened_count)
                    {
                        hmgr_enable_clock();
                        if(opened_count > 0)
                            opened_count--;
                    }
            #else
                    hmgr_hw_reset();
            #endif

                    _hmgr_close_all(1);
                }
            }
        }
        else
        {
            printk("_hmgr_operation :: missed info or unknown command => type = 0x%x, cmd = 0x%x,  \n", oper_data->type, oper_data->cmd_type);
            *(oper_data->vpu_result) = RETCODE_FAILURE;
            oper_finished = 0;
        }

        if(oper_finished)
        {
            if(oper_data->comm_data != NULL && hmgr_data.dev_opened != 0)
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
                err("Error: abnormal exception or external command was processed!! 0x%p - %d\n", oper_data->comm_data, hmgr_data.dev_opened);
            }
        }
        else{
            err("Error: abnormal exception 2!! 0x%p - %d\n", oper_data->comm_data, hmgr_data.dev_opened);
        }

        hmgr_list_manager(oper_data, LIST_DEL);

        hmgr_data.cmd_processing = 0;
    }

    return 0;
}

static int _hmgr_thread(void *kthread)
{
    dprintk("_hmgr_thread for dec is running. \n");

    do
    {
//      detailk("_hmgr_thread wait_sleep \n");

        if(hmgr_list_manager(NULL, LIST_IS_EMPTY))
        {
            hmgr_data.cmd_processing = 0;

            //wait_event_interruptible(hmgr_data.comm_data.thread_wq, hmgr_data.comm_data.thread_intr > 0);
            wait_event_interruptible_timeout(hmgr_data.comm_data.thread_wq, hmgr_data.comm_data.thread_intr > 0, msecs_to_jiffies(50));
            hmgr_data.comm_data.thread_intr = 0;
        }
        else
        {
            if(hmgr_data.dev_opened || hmgr_data.external_proc){
                _hmgr_operation();
            }
            else{
                VpuList_t *oper_data = NULL;

                printk("DEL for empty \n");

                oper_data = hmgr_list_manager(NULL, LIST_GET_ENTRY);
                if(oper_data)
                    hmgr_list_manager(oper_data, LIST_DEL);
            }
        }

    } while (!kthread_should_stop());

    return 0;
}

//////////////////////////////////////////////////////////////////////////
static int _hmgr_mmap(struct file *file, struct vm_area_struct *vma)
{
#if defined(CONFIG_TCC_MEM)
    if(range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0){
        printk(KERN_ERR  "_hmgr_mmap: this address is not allowed \n");
        return -EAGAIN;
    }
#endif

    vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
    if(remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot))
    {
        printk("_hmgr_mmap :: remap_pfn_range failed\n");
        return -EAGAIN;
    }

    vma->vm_ops     = NULL;
    vma->vm_flags   |= VM_IO;
    vma->vm_flags   |= VM_DONTEXPAND | VM_PFNMAP;

    return 0;
}

static struct file_operations _hmgr_fops = {
    .open               = _hmgr_open,
    .release            = _hmgr_release,
    .mmap               = _hmgr_mmap,
    .unlocked_ioctl     = _hmgr_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl       = _hmgr_ioctl,
#endif
};

static struct miscdevice _hmgr_misc_device =
{
    MISC_DYNAMIC_MINOR,
    HMGR_NAME,
    &_hmgr_fops,
};

int hmgr_probe(struct platform_device *pdev)
{
    int ret;
    int type = 0;
    unsigned long int_flags;
    struct resource *res = NULL;

    if (pdev->dev.of_node == NULL) {
        return -ENODEV;
    }

    dprintk("hmgr initializing!! \n");
    memset(&hmgr_data, 0, sizeof(mgr_data_t));
    for(type=0; type<HEVC_MAX; type++) {
        hmgr_data.closed[type] = 1;
    }

    hmgr_init_variable();

    hmgr_data.irq = platform_get_irq(pdev, 0);
    hmgr_data.nOpened_Count = 0;
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "missing phy memory resource\n");
        return -1;
    }
    res->end += 1;
    //hmgr_data.base_addr = regs->start;
    hmgr_data.base_addr = devm_ioremap(&pdev->dev, res->start, res->end - res->start);
    dprintk("============> HEVC base address [0x%x -> 0x%p], irq num [%d] \n", res->start, hmgr_data.base_addr, hmgr_data.irq - 32);

    hmgr_get_clock(pdev->dev.of_node);

    spin_lock_init(&(hmgr_data.oper_lock));
//  spin_lock_init(&(hmgr_data.comm_data.lock));

    init_waitqueue_head(&hmgr_data.comm_data.thread_wq);
    init_waitqueue_head(&hmgr_data.oper_wq);

    mutex_init(&hmgr_data.comm_data.list_mutex);
    mutex_init(&(hmgr_data.comm_data.io_mutex));

    INIT_LIST_HEAD(&hmgr_data.comm_data.main_list);
    INIT_LIST_HEAD(&hmgr_data.comm_data.wait_list);

    hmgr_init_interrupt();
    int_flags = hmgr_get_int_flags();
    ret = hmgr_request_irq(hmgr_data.irq, _hmgr_isr_handler, int_flags, HMGR_NAME, &hmgr_data);
    if (ret) {
        err("to aquire hevc-dec-irq \n");
    }
    hmgr_data.irq_reged = 1;
    hmgr_disable_irq(hmgr_data.irq);

    kidle_task = kthread_run(_hmgr_thread, NULL, "vHEVC_th");
    if( IS_ERR(kidle_task) )
    {
        err("unable to create thread!! \n");
        kidle_task = NULL;
        return -1;
    }
    dprintk("success :: thread created!! \n");

    _hmgr_close_all(1);

    if (misc_register(&_hmgr_misc_device))
    {
        printk(KERN_WARNING "HEVC Manager: Couldn't register device.\n");
        return -EBUSY;
    }

    hmgr_enable_clock();
    hmgr_disable_clock();

    return 0;
}
EXPORT_SYMBOL(hmgr_probe);

int hmgr_remove(struct platform_device *pdev)
{
    misc_deregister(&_hmgr_misc_device);

    if( kidle_task ){
        kthread_stop(kidle_task);
        kidle_task = NULL;
    }

    devm_iounmap(&pdev->dev, hmgr_data.base_addr);
    if( hmgr_data.irq_reged ){
        hmgr_free_irq(hmgr_data.irq, &hmgr_data);
        hmgr_data.irq_reged = 0;
    }

    hmgr_put_clock();

    printk("success :: hmgr thread stopped!! \n");

    return 0;
}
EXPORT_SYMBOL(hmgr_remove);

#if defined(CONFIG_PM)
int hmgr_suspend(struct platform_device *pdev, pm_message_t state)
{
    int i, open_count = 0;

    if(hmgr_data.dev_opened != 0)
    {
        printk(" \n hevc: suspend In DEC(%d/%d/%d/%d/%d) \n", hmgr_get_close(VPU_DEC), hmgr_get_close(VPU_DEC_EXT), hmgr_get_close(VPU_DEC_EXT2), hmgr_get_close(VPU_DEC_EXT3), hmgr_get_close(VPU_DEC_EXT4));

        _hmgr_external_all_close(200);

        open_count = hmgr_data.dev_opened;
        for(i=0; i<open_count; i++) {
            hmgr_disable_clock();
        }
        printk("hevc: suspend Out DEC(%d/%d/%d/%d/%d) \n\n", hmgr_get_close(VPU_DEC), hmgr_get_close(VPU_DEC_EXT), hmgr_get_close(VPU_DEC_EXT2), hmgr_get_close(VPU_DEC_EXT3), hmgr_get_close(VPU_DEC_EXT4));
    }

    return 0;
}
EXPORT_SYMBOL(hmgr_suspend);

int hmgr_resume(struct platform_device *pdev)
{
    int i, open_count = 0;

    if(hmgr_data.dev_opened != 0){

        open_count = hmgr_data.dev_opened;

        for(i=0; i<open_count; i++) {
            hmgr_enable_clock();
        }
        printk("\n hevc: resume \n\n");
    }

    return 0;
}
EXPORT_SYMBOL(hmgr_resume);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc manager");
MODULE_LICENSE("GPL");

#endif
