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
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_address.h>

#include "tca_hwdemux.h"
#include "tca_hwdemux_cmd.h"
#include "tca_hwdemux_tsif.h"

//#define SUPPORT_DEBUG_CMHWDEMUX
#if defined(SUPPORT_DEBUG_CMHWDEMUX)
#include "DebugHWDemux.h" //testing f/w of holding at main()
#define CMD_TIMEOUT        msecs_to_jiffies(1000000)
#else
#define CMD_TIMEOUT        msecs_to_jiffies(1000)
#endif

HWDMX_HANDLE hHWDMX;

static DEFINE_MUTEX(tsdmx_mutex);
static FN_UPDATECALLBACK ts_demux_buffer_updated_callback[4] = { NULL, NULL, NULL, NULL};
static FN_NSKCALLBACK nskMsgHandler = NULL;
static int g_interface = 0;
static int g_initCount = 0;

extern unsigned int tcc_chip_rev;

/*****************************************************************************
* Function Name : int tsdmx_lock_on(void)
* Description :
* Arguments :
******************************************************************************/
int tsdmx_lock_on(void)
{
    mutex_lock(&tsdmx_mutex);
    return 0;
}
EXPORT_SYMBOL(tsdmx_lock_on);

/*****************************************************************************
* Function Name : int tsdmx_lock_off(void)
* Description :
* Arguments :
******************************************************************************/
int tsdmx_lock_off(void)
{
    mutex_unlock(&tsdmx_mutex);
    return 0;
}
EXPORT_SYMBOL(tsdmx_lock_off);

/*****************************************************************************
* Function Name : static int TSDMX_Init(struct tcc_tsif_handle *h)
* Description :
* Arguments :
******************************************************************************/
static int TSDMX_Init(struct tcc_tsif_handle *h)
{
    int ret = 0;
    ARG_TSDMXINIT Arg;

    pr_info("[INFO][HWDMX]\n%s:%d:0x%08X:0x%08X:0x%08X port:%u\n",__func__, __LINE__, h->dma_buffer->dma_addr, (unsigned int)h->dma_buffer->v_addr, h->dma_buffer->buf_size, h->port_cfg.tsif_port);
    Arg.uiDMXID  = h->dmx_id;
    Arg.uiMode = HW_DEMUX_NORMAL; //HW_DEMUX_BYPASS    
    Arg.uiTSRingBufAddr = h->dma_buffer->dma_addr;
    Arg.uiTSRingBufSize = h->dma_buffer->buf_size;
    Arg.uiSECRingBufAddr = h->dma_buffer->dma_sec_addr;
    Arg.uiSECRingBufSize = h->dma_buffer->buf_sec_size;
    //Arg.uiChipID = 0; //Default BX
    Arg.uiChipID = 1;

    switch(h->working_mode)
    {
        case 0x00: //tsif for tdmb
            Arg.uiTSIFInterface = HW_DEMUX_SPI;
            break;

        default: //0x01 for tsif of isdbt & dvbt
            if (g_interface == 2)
                Arg.uiTSIFInterface = HW_DEMUX_INTERNAL;
            else if (g_interface == 1)
                Arg.uiTSIFInterface = HW_DEMUX_TSIF_PARALLEL;
            else //if (g_interface == 0)
                Arg.uiTSIFInterface = HW_DEMUX_TSIF_SERIAL;
            break;
    }
    
    tsdmx_lock_on();
    Arg.uiTSIFCh = h->dmx_id;
    Arg.uiTSIFPort = h->port_cfg.tsif_port;

    //TSIf Polarity : TSP-Sync Pulse(Bit0) : 0(hight active), TVEP-Valid Data(Bit1) : 1(high active), TCKP(Bit2) : 0(riging edge of TSCLK)
    Arg.uiTSIFPol = Hw1;    

    //DEMOD_MODULE_LGDT3305
    //Arg.uiTSIFPol |= Hw2;    //TCKP(Bit2) : 1(falling edge of TSCLK)

    ret = TSDMXCMD_Init(&Arg, CMD_TIMEOUT);
    if(ret == 0)
    {
		if (!IS_ERR(hHWDMX.tsif_clk[h->dmx_id]))	
        {
            clk_prepare_enable(hHWDMX.tsif_clk[h->dmx_id]);
			clk_set_rate(hHWDMX.tsif_clk[h->dmx_id], 27000000);
        }

        g_initCount++;
    }

    tsdmx_lock_off();
    return ret;
}

/*****************************************************************************
* Function Name : static int TSDMX_DeInit(unsigned int uiDMXID)
* Description :
* Arguments :
******************************************************************************/
static int TSDMX_DeInit(unsigned int uiDMXID)
{
    int ret = 0;

    if (g_initCount == 0)
        return ret;

    tsdmx_lock_on();
    g_initCount--;
    if(!IS_ERR(hHWDMX.tsif_clk[uiDMXID])) {
        clk_disable(hHWDMX.tsif_clk[uiDMXID]);
    }
    ret = TSDMXCMD_DeInit(uiDMXID, CMD_TIMEOUT);
    tsdmx_lock_off();
    return ret;
}

/*****************************************************************************
* Function Name : static int TSDMX_ADD_Filter(unsigned int uiDMXID, struct tcc_tsif_filter *feed)
* Description :
* Arguments :
******************************************************************************/
static int TSDMX_ADD_Filter(unsigned int uiDMXID, struct tcc_tsif_filter *feed)
{
    int ret = 0;
    ARG_TSDMX_ADD_FILTER Arg;
    tsdmx_lock_on();
    Arg.uiDMXID = uiDMXID;
    Arg.uiTYPE = feed->f_type;
    Arg.uiPID = feed->f_pid;
    if(feed->f_type == HW_DEMUX_SECTION) {
        if(feed->f_size>16) {
            pr_err("[ERROR][HWDMX]!!! filter size is over 16 then it sets to 16.\n");
            feed->f_size = 16; //HWDMX can support less than 16 bytes filter size.
        }
		
        Arg.uiFSIZE = feed->f_size;
        Arg.uiFID = feed->f_id;
        if(feed->f_size*3 <= 24) {
            Arg.uiTotalIndex = 1;
        } else {
            Arg.uiTotalIndex = 2; //max filter size 16*3=48, 2 add filter commands is enough to send it.
        }
        memset(Arg.uiVectorData, 0x00, 20*4);
        memcpy(((unsigned char*)Arg.uiVectorData), feed->f_comp, feed->f_size);
        memcpy(((unsigned char*)Arg.uiVectorData) + (feed->f_size), feed->f_mask, feed->f_size);
        memcpy(((unsigned char*)Arg.uiVectorData) + (feed->f_size*2), feed->f_mode, feed->f_size);
        Arg.uiVectorIndex = 0;
        for(Arg.uiCurrentIndex=0; Arg.uiCurrentIndex<Arg.uiTotalIndex; Arg.uiCurrentIndex ++) {
            ret = TSDMXCMD_ADD_Filter(&Arg, CMD_TIMEOUT);
            if(ret != 0)
                break;
        }
    } else {
        ret = TSDMXCMD_ADD_Filter(&Arg, CMD_TIMEOUT);
    }
    tsdmx_lock_off();
    return ret;
}

/*****************************************************************************
* Function Name : static int TSDMX_DELETE_Filter(unsigned int uiDMXID, struct tcc_tsif_filter *feed)
* Description :
* Arguments :
******************************************************************************/
static int TSDMX_DELETE_Filter(unsigned int uiDMXID, struct tcc_tsif_filter *feed)
{
    int ret = 0;

    ARG_TSDMX_DELETE_FILTER Arg;
    tsdmx_lock_on();
    Arg.uiDMXID = uiDMXID;
    Arg.uiTYPE = feed->f_type;
    if(feed->f_type == HW_DEMUX_SECTION)
        Arg.uiFID = feed->f_id;
    else
        Arg.uiFID = 0;
    Arg.uiPID = feed->f_pid;

    ret = TSDMXCMD_DELETE_Filter(&Arg, CMD_TIMEOUT);
    tsdmx_lock_off();
    return ret;
}

/*****************************************************************************
* Function Name : static long long TSDMX_GET_STC(unsigned int uiDMXID)
* Description :
* Arguments :
******************************************************************************/
static long long TSDMX_GET_STC(unsigned int uiDMXID)
{
    int ret;
    tsdmx_lock_on();
#if defined(SUPPORT_DEBUG_CMHWDEMUX)
    ret = 0;
#else
    ret = TSDMXCMD_GET_STC(uiDMXID, CMD_TIMEOUT);
#endif
    tsdmx_lock_off();
    return ret;
}

/*****************************************************************************
* Function Name : static int TSDMX_SET_PCR_PID(unsigned int uiDMXID, unsigned int pid)
* Description :
* Arguments :
******************************************************************************/
static int TSDMX_SET_PCR_PID(unsigned int uiDMXID, unsigned int pid)
{
    int ret = 0;

    ARG_TSDMX_SET_PCR_PID Arg;
    tsdmx_lock_on();
    Arg.uiDMXID = uiDMXID;
    Arg.uiPCRPID = pid;
    ret = TSDMXCMD_SET_PCR_PID(&Arg, CMD_TIMEOUT);
    tsdmx_lock_off();
    return ret;
}

/*****************************************************************************
* Function Name : static int TSDMX_GET_Version(unsigned int uiDMXID)
* Description :
* Arguments :
******************************************************************************/
static int TSDMX_GET_Version(unsigned int uiDMXID)
{
    int ret = 0;
#if defined(SUPPORT_DEBUG_CMHWDEMUX)        
    while(1)
    {
        tsdmx_lock_on();
        ret = TSDMXCMD_GET_VERSION(uiDMXID, msecs_to_jiffies(10000)); //Getting f/w version
        tsdmx_lock_off();
        if(ret == 0)
            break;
    }
#else
    tsdmx_lock_on();
    ret = TSDMXCMD_GET_VERSION(uiDMXID, CMD_TIMEOUT); //Getting f/w version
    tsdmx_lock_off();
#endif        
    return ret;
}

/*****************************************************************************
* Function Name : static int TSDMX_SET_InBuffer(unsigned int uiDMXID, unsigned int uiBufferAddr, unsigned int uiBufferSize)
* Description :
* Arguments :
******************************************************************************/
static int TSDMX_SET_InBuffer(unsigned int uiDMXID, unsigned int uiBufferAddr, unsigned int uiBufferSize)
{
    int ret = 0;
    ARG_TSDMX_SET_IN_BUFFER Arg;
    if (g_initCount == 0)
        return ret;
    tsdmx_lock_on();
    Arg.uiDMXID = uiDMXID;
    Arg.uiInBufferAddr = uiBufferAddr;
    Arg.uiInBufferSize = uiBufferSize;
    ret = TSDMXCMD_SET_IN_BUFFER(&Arg, CMD_TIMEOUT);
    tsdmx_lock_off();
    return ret;
}

/*****************************************************************************
* Function Name : static int TSDMX_Set_Interface(unsigned int uiDMXID, unsigned int uiTSIFInterface)
* Description :
* Arguments :
******************************************************************************/
static int TSDMX_Set_Interface(unsigned int uiDMXID, unsigned int uiTSIFInterface)
{
    int ret = 0;

    if (g_interface == uiTSIFInterface) {
        return 0;
    }
    g_interface = uiTSIFInterface;

    if (g_initCount == 0)
        return ret;

    pr_info("[INFO][HWDMX][DEMUX #%d] Set Interface(%d)\n", uiDMXID, uiTSIFInterface);

    tsdmx_lock_on();
    TSDMXCMD_CHANGE_INTERFACE(uiDMXID, uiTSIFInterface, CMD_TIMEOUT);
    tsdmx_lock_off();
    return ret;
}

/*****************************************************************************
* Function Name : static int TSDMX_Get_Interface(unsigned int uiDMXID)
* Description :
* Arguments :
******************************************************************************/
static int TSDMX_Get_Interface(unsigned int uiDMXID)
{
    return g_interface;
}

/*****************************************************************************
* Function Name : static void TSDMX_CMUnloadBinary(void)
* Description : CM Code Un-Loading
* Arguments :
******************************************************************************/
static void TSDMX_CMUnloadBinary(void)
{
    volatile PCM_TSD_CFG pTSDCfg = (volatile PCM_TSD_CFG) hHWDMX.cfg_base;
    BITSET(pTSDCfg->CM_RESET.nREG, Hw1|Hw2); //m3 no reset
    pr_info("[INFO][HWDMX]%s:%d\n",__func__, __LINE__);
}

/*****************************************************************************
* Function Name : static void TSDMX_CMLoadBinary(const char *fw_data, unsigned int fw_size)
* Description : CM Code Loading
* Arguments :
******************************************************************************/
static void TSDMX_CMLoadBinary(const char *fw_data, unsigned int fw_size)
{
    volatile unsigned int * pCodeMem = (volatile unsigned int *) hHWDMX.code_base;
    volatile PCM_TSD_CFG pTSDCfg = (volatile PCM_TSD_CFG) hHWDMX.cfg_base;

#if defined(CONFIG_ARCH_TCC897X)
    BITCSET(pTSDCfg->REMAP0.nREG, 0xFFFFFFFF, 0x00234547);
    BITCSET(pTSDCfg->REMAP1.nREG, 0xFFFFFFFF, 0x89ABCDEF);
#elif defined(CONFIG_ARCH_TCC898X)
    BITCSET(pTSDCfg->REMAP0.nREG, 0xFFFFFFFF, 0x00234541);
    BITCSET(pTSDCfg->REMAP1.nREG, 0xFFFFFFFF, 0x89ABCDEF);
#endif

    TSDMX_CMUnloadBinary();
    if (fw_data && fw_size > 0) {
        memcpy((void *)pCodeMem, (void *)fw_data, fw_size);
    } else {
        pr_info("[INFO][HWDMX]Using previous loading the firmware\n");
    }

    BITCLR(pTSDCfg->CM_RESET.nREG, Hw1|Hw2); //m3 reset
}

/*****************************************************************************
* Function Name : void TSDMXCallback(int irq, void *dev, volatile int *nReg)
* Description :
* Arguments :
******************************************************************************/
void TSDMXCallback(int irq, void *dev, volatile int *nReg)
{
    unsigned int value1, value2;
    unsigned char cmd, dmxid;

    cmd = (nReg[0]&0xFF000000)>>24;
    dmxid = (nReg[0]&0x00FF0000)>>16;

    //if( cmd != 0xE1 && cmd != 0xF1)
    //    pr_info("[INFO][HWDMX]cmd = 0x%X dmxid = 0x%X\n", cmd, dmxid);
    switch(cmd)
    {
        case HW_DEMUX_GET_STC: // DEMUX_GET_STC
            {
                int filterType = 3;
                value1 = nReg[1]; // STC Base
                value2 = nReg[2]; // Bit 31: STC Extension
                if(ts_demux_buffer_updated_callback[dmxid])
                {
                    ts_demux_buffer_updated_callback[dmxid](dmxid, filterType, 0, nReg[1], nReg[2], nReg[3]);
                }
            }
            break;

        case HW_DEMUX_BUFFER_UPDATED: // DEMUX_BUFFER_UPDATED
            {
                unsigned int filterType = (nReg[0]&0x0000FF00)>>8;
                unsigned int filterID = nReg[0]&0x000000FF;
                unsigned int bErrCRC = (nReg[3]&0xFF000000)>>24;
                value1 = nReg[1];     // ts: end point of updated buffer(offset), sec: start point of updated buffer(offset)
                value2 = nReg[2] + 1; // ts: end point of buffer(offset),         sec: end point of updated buffer(offset)
                if (filterType == HW_DEMUX_TS)
                {
                    value1++;
                }
                else
                {
                //    if(filterType == HW_DEMUX_SECTION)
                //        pr_info("[INFO][HWDMX]Section:[0x%X][0x%X][0x%X][0x%X][0x%08X]\n", nReg[0], nReg[1], nReg[2], nReg[3], nReg[4]); 
                }
                if(ts_demux_buffer_updated_callback[dmxid])
                {
                    ts_demux_buffer_updated_callback[dmxid](dmxid, filterType, filterID, value1, value2, bErrCRC);
                }
            }
            break;

        case HW_DEMUX_DEBUG_DATA:
            {
                unsigned char str[9];
                unsigned int *p = (unsigned int *)str;
                p[0] = nReg[1];
                p[1] = nReg[2];
                str[8] = 0;
                pr_info("[INFO][HWDMX]%s :[0x%X][0x%X][0x%X][0x%X][0x%X].\n", str, nReg[3], nReg[4], nReg[5], nReg[6], nReg[7]);
            }
            break;

        default:
            // NSK2 cmd handling.
            if (0xB0 <= cmd && cmd <= 0xCF) {
                if (nskMsgHandler != NULL) {
                    nskMsgHandler(irq, dev, nReg);
                }
            }
            break;
    }
}

/*****************************************************************************
* Function Name : void tca_tsif_set_interface(int mode)
* Description :
* Arguments : mode(0: serial, 1: parallel, 2: internal)
******************************************************************************/
void tca_tsif_set_interface(int mode)
{
    TSDMX_Set_Interface(0, mode);
}
EXPORT_SYMBOL(tca_tsif_set_interface);

/*****************************************************************************
* Function Name : int tca_tsif_get_interface(void)
* Description :
* Arguments : return value (0: serial, 1: parallel, 2: internal)
******************************************************************************/
int tca_tsif_get_interface(void)
{
    return TSDMX_Get_Interface(0);
}
EXPORT_SYMBOL(tca_tsif_get_interface);

/*****************************************************************************
* Function Name : int tca_tsif_init(struct tcc_tsif_handle *h)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_init(struct tcc_tsif_handle *h)
{
    if (h && !IS_ERR(hHWDMX.cmb_clk)) {
        if (g_initCount == 0) {
            clk_prepare_enable(hHWDMX.cmb_clk);
			clk_set_rate(hHWDMX.cmb_clk,  hHWDMX.cmb_clk_rate);
					

            TSDMX_Prepare();
            #if defined(SUPPORT_DEBUG_CMHWDEMUX)
            TSDMX_CMLoadBinary(DebugHWDemux, sizeof(DebugHWDemux));
            #else
            TSDMX_CMLoadBinary(h->fw_data, h->fw_size);
            #endif
            msleep(100); //Wait for CM Booting
            if (TSDMX_GET_Version(h->dmx_id) != 0) //Getting f/w version
                return 1;        
        }
        return TSDMX_Init(h);
    }
    return 1;
}
EXPORT_SYMBOL(tca_tsif_init);

/*****************************************************************************
* Function Name : void tca_tsif_clean(struct tcc_tsif_handle *h)
* Description :
* Arguments :
******************************************************************************/
void tca_tsif_clean(struct tcc_tsif_handle *h)
{
    int ret = 0;

    if (h) {
        ts_demux_buffer_updated_callback[h->dmx_id] = NULL;
        ret = TSDMX_DeInit(h->dmx_id);
        if (g_initCount == 0) {
            TSDMX_CMUnloadBinary();
            TSDMX_Release();

            #if !defined(SUPPORT_DEBUG_CMHWDEMUX)
            if(!IS_ERR(hHWDMX.cmb_clk)) {
                clk_disable(hHWDMX.cmb_clk);
            }
            #endif
        }
    }
    pr_info("[INFO][HWDMX]TSDMX_DeInit Command Result : %d\n", ret);
    
}
EXPORT_SYMBOL(tca_tsif_clean);

/*****************************************************************************
* Function Name : int tca_tsif_start(struct tcc_tsif_handle *h)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_start(struct tcc_tsif_handle *h)
{
    int ret = 0;

    if (h) {
        ret = TSDMX_Init(h);
    }
    return ret;
}
EXPORT_SYMBOL(tca_tsif_start);

/*****************************************************************************
* Function Name : int tca_tsif_stop(struct tcc_tsif_handle *h)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_stop(struct tcc_tsif_handle *h)
{
    int ret = 0;

    if (h) {
        ret = TSDMX_DeInit(h->dmx_id);
    }
    return ret;
}
EXPORT_SYMBOL(tca_tsif_stop);

/*****************************************************************************
* Function Name : int tca_tsif_register_pids(struct tcc_tsif_handle *h, unsigned int *pids, unsigned int count)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_register_pids(struct tcc_tsif_handle *h, unsigned int *pids, unsigned int count)
{
    pr_info("[INFO][HWDMX][DEMUX #%d]tca_tsif_register_pids\n", h->dmx_id);
    return 0;
}
EXPORT_SYMBOL(tca_tsif_register_pids);

/*****************************************************************************
* Function Name : int tca_tsif_can_support(void)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_can_support(void)
{
    return 1;
}
EXPORT_SYMBOL(tca_tsif_can_support);

/*****************************************************************************
* Function Name : int tca_tsif_buffer_updated_callback(struct tcc_tsif_handle *h, FN_UPDATECALLBACK buffer_updated)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_buffer_updated_callback(struct tcc_tsif_handle *h, FN_UPDATECALLBACK buffer_updated)
{
    ts_demux_buffer_updated_callback[h->dmx_id] = buffer_updated;
    return 0;
}
EXPORT_SYMBOL(tca_tsif_buffer_updated_callback);

/*****************************************************************************
* Function Name : int tca_tsif_set_nskcallback(FN_NSKCALLBACK _nskMsgHandler)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_set_nskcallback(FN_NSKCALLBACK _nskMsgHandler)
{
    nskMsgHandler = _nskMsgHandler;
    return 0;
}
EXPORT_SYMBOL(tca_tsif_set_nskcallback);

/*****************************************************************************
* Function Name : int tca_tsif_set_pcrpid(struct tcc_tsif_handle *h, unsigned int pid)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_set_pcrpid(struct tcc_tsif_handle *h, unsigned int pid)
{
    pr_info("[INFO][HWDMX][DEMUX #%d]tca_tsif_set_pcrpid(pid=%d)\n", h->dmx_id, pid);
    return TSDMX_SET_PCR_PID(h->dmx_id, pid);
}
EXPORT_SYMBOL(tca_tsif_set_pcrpid);

/*****************************************************************************
* Function Name : long long tca_tsif_get_stc(struct tcc_tsif_handle *h)
* Description :
* Arguments :
******************************************************************************/
long long tca_tsif_get_stc(struct tcc_tsif_handle *h)
{
//    pr_info("[INFO][HWDMX][DEMUX #%d]tca_tsif_get_stc\n", h->dmx_id);
    return TSDMX_GET_STC(h->dmx_id);
}
EXPORT_SYMBOL(tca_tsif_get_stc);

/*****************************************************************************
* Function Name : int tca_tsif_add_filter(struct tcc_tsif_handle *h, struct tcc_tsif_filter *feed)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_add_filter(struct tcc_tsif_handle *h, struct tcc_tsif_filter *feed)
{
    pr_info("[INFO][HWDMX][DEMUX #%d]tca_tsif_add_filter(type=%d, pid=%d)\n", h->dmx_id, feed->f_type, feed->f_pid);
    return TSDMX_ADD_Filter(h->dmx_id, feed);
}
EXPORT_SYMBOL(tca_tsif_add_filter);

/*****************************************************************************
* Function Name : int tca_tsif_remove_filter(struct tcc_tsif_handle *h, struct tcc_tsif_filter *feed)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_remove_filter(struct tcc_tsif_handle *h, struct tcc_tsif_filter *feed)
{
    pr_info("[INFO][HWDMX][DEMUX #%d]tca_tsif_remove_filter(type=%d, pid=%d)\n", h->dmx_id, feed->f_type, feed->f_pid);
    return TSDMX_DELETE_Filter(h->dmx_id, feed);
}
EXPORT_SYMBOL(tca_tsif_remove_filter);

/*****************************************************************************
* Function Name : int tca_tsif_input_internal(unsigned int dmx_id, unsigned int phy_addr, unsigned int size)
* Description :
* Arguments :
******************************************************************************/
int tca_tsif_input_internal(unsigned int dmx_id, unsigned int phy_addr, unsigned int size)
{
    //pr_info("[INFO][HWDMX][DEMUX #%d]tca_tsif_input_internal(buffer=[0x%X], size=%d)\n", dmx_id, phy_addr, size);
    return TSDMX_SET_InBuffer(dmx_id, phy_addr, size);
}
EXPORT_SYMBOL(tca_tsif_input_internal);

/*****************************************************************************
* Function Name : int tca_hwdemux_cipher_Set_Algorithm(stHWDEMUXCIPHER_ALGORITHM *pARG)
* Description :
* Arguments :
******************************************************************************/
int tca_hwdemux_cipher_Set_Algorithm(stHWDEMUXCIPHER_ALGORITHM *pARG)
{
    int ret = 0;
//   pr_info("[INFO][HWDMX]%s, %d  \n", __func__, __LINE__);
    tsdmx_lock_on();
    HWDEMUXCIPHERCMD_Set_Algorithm(pARG, CMD_TIMEOUT);
    tsdmx_lock_off();
//    pr_info("[INFO][HWDMX]%s, %d  \n", __func__, __LINE__);
    return ret;
}
EXPORT_SYMBOL(tca_hwdemux_cipher_Set_Algorithm);

/*****************************************************************************
* Function Name : int tca_hwdemux_cipher_Set_Key(stHWDEMUXCIPHER_KEY *pARG)
* Description :
* Arguments :
******************************************************************************/
int tca_hwdemux_cipher_Set_Key(stHWDEMUXCIPHER_KEY *pARG)
{
    unsigned int uiTotalIndex=0, i;
    int ret = 0;
//    pr_info("[INFO][HWDMX]%s, %d  \n", __func__, __LINE__);
    tsdmx_lock_on();
    if(pARG->uLength<=6*4)        //6*4 : ("6 = uMBOX_TX2~uMBOX_TX7", "4 = uMBOX_TX2 = 4byte")
        uiTotalIndex = 1;
    else 
        uiTotalIndex = 2;

    for(i=0; i<uiTotalIndex; i++)
    {
        ret = HWDEMUXCIPHERCMD_Set_Key(pARG, uiTotalIndex, (i+1), CMD_TIMEOUT);
    }
    tsdmx_lock_off();
//    pr_info("[INFO][HWDMX]%s, %d  \n", __func__, __LINE__);
    return ret;

}
EXPORT_SYMBOL(tca_hwdemux_cipher_Set_Key);

/*****************************************************************************
* Function Name : int tca_hwdemux_cipher_Set_IV(stHWDEMUXCIPHER_VECTOR *pARG)
* Description :
* Arguments :
******************************************************************************/
int tca_hwdemux_cipher_Set_IV(stHWDEMUXCIPHER_VECTOR *pARG)
{
    int ret = 0;
//    pr_info("[INFO][HWDMX]%s, %d  \n", __func__, __LINE__);
    tsdmx_lock_on();
    ret = HWDEMUXCIPHERCMD_Set_IV(pARG, CMD_TIMEOUT);
    tsdmx_lock_off();
//    pr_info("[INFO][HWDMX]%s, %d  \n", __func__, __LINE__);
    return ret;
}
EXPORT_SYMBOL(tca_hwdemux_cipher_Set_IV);

/*****************************************************************************
* Function Name : int tca_hwdemux_cipher_Cipher_Run(stHWDEMUXCIPHER_EXECUTE *pARG)
* Description :
* Arguments :
******************************************************************************/
int tca_hwdemux_cipher_Cipher_Run(stHWDEMUXCIPHER_EXECUTE *pARG)
{
    int ret = 0;
//   pr_info("[INFO][HWDMX]%s, %d  \n", __func__, __LINE__);
    tsdmx_lock_on();
    ret = HWDEMUXCIPHERCMD_Cipher_Run(pARG, CMD_TIMEOUT);
    tsdmx_lock_off();
//   pr_info("[INFO][HWDMX]%s, %d  \n", __func__, __LINE__);
    return ret;
}
EXPORT_SYMBOL(tca_hwdemux_cipher_Cipher_Run);

/*****************************************************************************
 * tcc_hwdemux Prove/Remove
 ******************************************************************************/
static int tcc_hwdemux_probe(struct platform_device *pdev)
{
    pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

    hHWDMX.code_base  = of_iomap(pdev->dev.of_node, 0); // Code Addr
    hHWDMX.mbox0_base = of_iomap(pdev->dev.of_node, 1); // MailBox 0 Addr
    hHWDMX.mbox1_base = of_iomap(pdev->dev.of_node, 2); // MailBox 1 Addr
    hHWDMX.cfg_base   = of_iomap(pdev->dev.of_node, 3); // CFG Addr

    hHWDMX.irq        = platform_get_irq(pdev, 0); // MailBox 0 IRQ
    hHWDMX.cmb_clk    = of_clk_get(pdev->dev.of_node, 0); // CM Bus Clock
    hHWDMX.tsif_clk[0] = of_clk_get(pdev->dev.of_node, 1); // TSIF0 Clock
    hHWDMX.tsif_clk[1] = of_clk_get(pdev->dev.of_node, 2); // TSIF1 Clock
    hHWDMX.tsif_clk[2] = of_clk_get(pdev->dev.of_node, 3); // TSIF2 Clock
    hHWDMX.tsif_clk[3] = of_clk_get(pdev->dev.of_node, 4); // TSIF3 Clock

    of_property_read_u32(pdev->dev.of_node, "clock-frequency", &hHWDMX.cmb_clk_rate);

    pr_info("[INFO][HWDMX]CODE(0x%08x), MBOX0(0x%08x), MBOX1(0x%08x), CFG(0x%08x)\n",
            (unsigned int)hHWDMX.code_base, (unsigned int)hHWDMX.mbox0_base, (unsigned int)hHWDMX.mbox1_base, (unsigned int)hHWDMX.cfg_base);
    pr_info("[INFO][HWDMX]IRQ(%d), CLK(Err:%d, Rate:%d Hz)\n", hHWDMX.irq, IS_ERR(hHWDMX.cmb_clk), hHWDMX.cmb_clk_rate);
	pr_info("[INFO][HWDMX]TSIF CLK(%d, %d, %d, %d Hz)\n",hHWDMX.tsif_clk[0], hHWDMX.tsif_clk[1], hHWDMX.tsif_clk[2], hHWDMX.tsif_clk[3]);

    return 0;
}

static int tcc_hwdemux_remove(struct platform_device *pdev)
{
    pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

    if (!IS_ERR(hHWDMX.tsif_clk[3])) {
        clk_put(hHWDMX.tsif_clk[3]);
    }
    if (!IS_ERR(hHWDMX.tsif_clk[2])) {
        clk_put(hHWDMX.tsif_clk[2]);
    }
    if (!IS_ERR(hHWDMX.tsif_clk[1])) {
        clk_put(hHWDMX.tsif_clk[1]);
    }
    if (!IS_ERR(hHWDMX.tsif_clk[0])) {
        clk_put(hHWDMX.tsif_clk[0]);
    }
    if (!IS_ERR(hHWDMX.cmb_clk)) {
        clk_put(hHWDMX.cmb_clk);
    }
    return 0;
}

/*****************************************************************************
 * tcc_hwdemux Module Init/Exit
 ******************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id tcc_hwdemux_of_match[] = {
    {.compatible = "telechips,hwdemux", },
    { },
};
MODULE_DEVICE_TABLE(of, tcc_hwdemux_of_match);
#endif

static struct platform_driver tcc_hwdemux = {
    .probe    = tcc_hwdemux_probe,
    .remove   = tcc_hwdemux_remove,
    .driver   = {
        .name    = "tcc_hwdemux",
        .owner   = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = tcc_hwdemux_of_match,
#endif
    },
};

static int __init tcc_hwdemux_init(void)
{
    pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);

    platform_driver_register(&tcc_hwdemux);

    return 0;
}

static void __exit tcc_hwdemux_exit(void)
{
    platform_driver_unregister(&tcc_hwdemux);

    pr_info("[INFO][HWDMX]%s\n", __FUNCTION__);
}

module_init(tcc_hwdemux_init);
module_exit(tcc_hwdemux_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC CM Bus Driver");
MODULE_LICENSE("GPL");
