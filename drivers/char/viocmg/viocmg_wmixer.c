/****************************************************************************
 * One line to give the program's name and a brief idea of what it does.
 * Copyright (C) 2013 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 * ****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/cpufreq.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
#include <mach/io.h>
#include <mach/bsp.h>
#include <mach/vioc_outcfg.h>
#include <mach/vioc_rdma.h>
#include <mach/vioc_wdma.h>
#include <mach/vioc_wmix.h>
#include <mach/vioc_disp.h>
#include <mach/vioc_global.h>
#include <mach/vioc_vin.h>
#include <mach/vioc_viqe.h>
#include <mach/vioc_config.h>
#include <mach/vioc_api.h>
#include <mach/tcc_fb.h>
#include <mach/tccfb_ioctrl.h>
#include <mach/vioc_scaler.h>
#include <mach/tca_lcdc.h>
#else
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_vin.h>
#include <video/tcc/vioc_viqe.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/tca_lcdc.h>
#endif
#include <soc/tcc/pmap.h>
#include <video/tcc/viocmg.h>

#define VIOMG_DEBUG 		1
#define dprintk(format, msg...) 	if(VIOMG_DEBUG) { printk("[VIOCMG_WMIX] "format"", ##msg); }
#define dfnprintk(format, msg...)     if(VIOMG_DEBUG) { printk("[VIOCMG_WMIX] %s: "format"", __func__,  ##msg); }


static int viocmg_get_backup_wmix(unsigned int caller_id, unsigned int wmix_id)
{
    int result = -1;
    unsigned int rear_cam_vin_wmix_id = viocmg_get_rear_cam_vin_wmix_id();
    unsigned int rear_cam_display_wmix_id = viocmg_get_main_display_id();

    if(caller_id != VIOCMG_CALLERID_REAR_CAM) {
        if(wmix_id == rear_cam_display_wmix_id) {
            result = 0;
        }
        else if(wmix_id == rear_cam_vin_wmix_id) {
            result = 1;
        }
    }

    return result;
}

#if 0
int viocmg_set_wmix_position(unsigned int caller_id, unsigned int wmix_id, unsigned int wmix_channel, unsigned int nX, unsigned int nY)
{
    int result = -1;
    int backup_wmix = -1;
    
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);

    if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
        backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
    }

    if(backup_wmix < 0) {
        dfnprintk(" hw (%d, %d)\r\n", nX, nY);
        VIOC_WMIX_SetPosition((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), wmix_channel, nX, nY);
        VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));
    }
    else {
        // backup ovp
        dfnprintk(" backup[%d] (%d, %d)\r\n", backup_wmix, nX, nY);
        VIOC_WMIX_SetPosition(&viocmg->rear_cam_backup_components.wmix[backup_wmix], wmix_channel, nX, nY);
    }
    result = 0;
    
    //mutex_unlock(&viocmg->mutex_wmix);    

    return result;

}
EXPORT_SYMBOL(viocmg_set_wmix_position);


void viocmg_get_wmix_position(unsigned int caller_id, unsigned int wmix_id, unsigned int wmix_channel, unsigned int *nX, unsigned int *nY)
{
    int backup_wmix = -1;
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);
    
    if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
        backup_wmix = viocmg_get_backup_wmix(0, wmix_id);
    }
        
    if(backup_wmix < 0) {
        VIOC_WMIX_GetPosition((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), wmix_channel, nX, nY);
        dfnprintk(" hw (%d, %d)\r\n", *nX, *nY);
    }
    else {
        // backup ovp
        VIOC_WMIX_GetPosition(&viocmg->rear_cam_backup_components.wmix[backup_wmix], wmix_channel, nX, nY);
        dfnprintk(" backup[%d] (%d, %d)\r\n", backup_wmix, *nX, *nY);
    }
    
    //mutex_unlock(&viocmg->mutex_wmix);    
}
EXPORT_SYMBOL(viocmg_get_wmix_position);

#endif

int viocmg_set_wmix_ovp(unsigned int caller_id, unsigned int wmix_id, unsigned int ovp)
{
    int result = -1;
    int backup_wmix = -1;
    
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);

    if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
        backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
    }

    if(backup_wmix < 0) {
        dfnprintk(" (caller 0x%x) hw (%d)\r\n", caller_id, ovp);
        VIOC_WMIX_SetOverlayPriority((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), ovp);
        VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));
    }
    else {
        // force rear ovp
        VIOC_WMIX_SetOverlayPriority((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), viocmg_get_rear_cam_ovp());
        // backup ovp
        dfnprintk(" (caller 0x%x) backup (%d)\r\n", caller_id, ovp);
        VIOC_WMIX_SetOverlayPriority(&viocmg->rear_cam_backup_components.wmix[backup_wmix], ovp);
    }
    result = 0;
    
    //mutex_unlock(&viocmg->mutex_wmix);    

    return result;

}
EXPORT_SYMBOL(viocmg_set_wmix_ovp);


void viocmg_get_wmix_ovp(unsigned int wmix_id, unsigned int *ovp)
{
    int backup_wmix = -1;
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);
    
    if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
        backup_wmix = viocmg_get_backup_wmix(0, wmix_id);
    }
        
    if(backup_wmix < 0) {
        VIOC_WMIX_GetOverlayPriority((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), ovp);
        dfnprintk(" hw (%d)\r\n", *ovp);
    }
    else {
        // backup ovp
        VIOC_WMIX_GetOverlayPriority(&viocmg->rear_cam_backup_components.wmix[backup_wmix], ovp);
        dfnprintk(" backup[%d] (%d)\r\n", backup_wmix, *ovp);
    }
    
    //mutex_unlock(&viocmg->mutex_wmix);    

    return ovp;
}
EXPORT_SYMBOL(viocmg_get_wmix_ovp);

#if 0
static struct wmix_enable_entry* viocmg_find_chromakey_enable_entry(unsigned int wmix_id, unsigned int layer)
{

    struct list_head* next;
    struct wmix_enable_entry *wmix_enable_entry;
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    list_for_each(next, &viocmg->wmix_enable_lists.chromakey.list) {
        wmix_enable_entry = list_entry(next, struct wmix_enable_entry, list);
        if(wmix_enable_entry->id == wmix_id && wmix_enable_entry->layer == layer) break;
        wmix_enable_entry = NULL;
    }

    return wmix_enable_entry;
}


static struct wmix_enable_entry* viocmg_find_chromakey_enable_entry_with_caller_id(unsigned int caller_id, unsigned int wmix_id, unsigned int layer)
{

    struct list_head* next;
    struct wmix_enable_entry *wmix_enable_entry;
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    list_for_each(next, &viocmg->wmix_enable_lists.chromakey.list) {
        wmix_enable_entry = list_entry(next, struct wmix_enable_entry, list);
        if(wmix_enable_entry->caller_id == caller_id && wmix_enable_entry->id == wmix_id && wmix_enable_entry->layer == layer) break;
        wmix_enable_entry = NULL;
    }

    return wmix_enable_entry;
}


static int viocmg_add_chromakey_enable_entry(unsigned int caller_id, unsigned int wmix_id, unsigned int layer)
{
    int result = -1;

    struct wmix_enable_entry *wmix_enable_entry;
    struct viocmg_soc *viocmg = viocmg_get_soc();

    wmix_enable_entry = viocmg_find_chromakey_enable_entry_with_caller_id(caller_id, wmix_id, layer);

    if(wmix_enable_entry) {
        // alread existed.
        dprintk(" viocmg_add_chromakey_enable_entry already enabled (WMIX(%d), Layer(%d)\r\n", wmix_id, layer);
        result = 0;
    }
    else {
        wmix_enable_entry = kzalloc(sizeof(struct wmix_enable_entry), GFP_KERNEL);

        if(!wmix_enable_entry) {
            printk("out of memory\r\n");
        }
        else {
            wmix_enable_entry->caller_id = caller_id;
            wmix_enable_entry->id = wmix_id;
            wmix_enable_entry->layer = layer;
            list_add_tail(&wmix_enable_entry->list, &viocmg->wmix_enable_lists.chromakey.list);
            result = 0;
            dprintk(" viocmg_add_chromakey_enable_entry add enabled (WMIX(%d), Layer(%d)\r\n", wmix_id, layer);
        }
    }

    return result;
}

static int viocmg_remove_chromakey_enable_entry(unsigned int caller_id, unsigned int wmix_id, unsigned int layer)
{
    int result = -1;

    struct wmix_enable_entry *wmix_enable_entry;

    wmix_enable_entry = viocmg_find_chromakey_enable_entry_with_caller_id(caller_id, wmix_id, layer);

    if(wmix_enable_entry) {
        dprintk(" viocmg_remove_chromakey_enable_entry remove (WMIX(%d), Layer(%d)\r\n", wmix_id, layer);
        list_del(&wmix_enable_entry->list);
        kfree(wmix_enable_entry);
        result = 0;
    }
    else {
        // already remove..!
        dprintk(" viocmg_remove_chromakey_enable_entry already remove (WMIX(%d), Layer(%d)\r\n", wmix_id, layer);
        result = 0;
    }

    return result;
}





int viocmg_set_wmix_chromakey_enable(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int enable)
{
    int result = -1;
    int backup_wmix = -1;
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);
    dprintk(" viocmg_set_wmix_chromakey_enable viocmg=0x%x\r\n", (unsigned int)viocmg);
    
    if(enable) {
        if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
            backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
        }

        if(backup_wmix < 0){
            dprintk(" viocmg_set_wmix_chromakey_enable wmix=0x%x\r\n", (unsigned int)viocmg->reg.display_wmix);
            VIOC_WMIX_SetChromaKeyEnable((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, 1);
            VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));
        }
        else {
            // Backup
            VIOC_WMIX_SetChromaKeyEnable(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, 1);
        }
        
        if(caller_id != VIOCMG_CALLERID_REAR_CAM)
                viocmg_add_chromakey_enable_entry(caller_id, wmix_id, layer);
    }
    else {
        unsigned int remained_owner = 0;

        if(caller_id == VIOCMG_CALLERID_REAR_CAM) {
            remained_owner = 0;
        }
        else {
            struct wmix_enable_entry *wmix_enable_entry;
            viocmg_remove_chromakey_enable_entry(caller_id, wmix_id, layer);
            wmix_enable_entry = viocmg_find_chromakey_enable_entry(wmix_id, layer);

             if(wmix_enable_entry) remained_owner = 1;

        }

        if(!remained_owner) {
            if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
                backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
            }

            if(backup_wmix < 0)  {
                dprintk(" viocmg_set_wmix_chromakey_enable disable chroma\r\n");
                VIOC_WMIX_SetChromaKeyEnable((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, 0);
                VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));

            }
            else {
                // Backup
                VIOC_WMIX_SetChromaKeyEnable(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, 0);
            }
        }

    }
   
    
    //mutex_unlock(&viocmg->mutex_wmix); 

    result = 0;
    
    return result;
}

EXPORT_SYMBOL(viocmg_set_wmix_chromakey_enable);



void viocmg_get_wmix_chromakey_enable(unsigned int wmix_id, unsigned int layer, unsigned int *enable)
{
    int backup_wmix = -1;
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);

    if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
        backup_wmix = viocmg_get_backup_wmix(0, wmix_id);
    }

    
    if(backup_wmix < 0) {
        VIOC_WMIX_GetChromaKeyEnable((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, enable);
        dfnprintk(" hw (%d)\r\n", *enable);
    }
    else {
        // backup ovp
        VIOC_WMIX_GetChromaKeyEnable(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, enable);
        dfnprintk(" backup[%d] (%d)\r\n", backup_wmix, *enable);
    }

    //mutex_unlock(&viocmg->mutex_wmix); 

    return enable;
}

EXPORT_SYMBOL(viocmg_get_wmix_chromakey_enable);




int viocmg_set_wmix_chromakey(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int enable, unsigned int nKeyR, unsigned int nKeyG, unsigned int nKeyB, unsigned int nKeyMaskR, unsigned int nKeyMaskG, unsigned int nKeyMaskB)
{
    int result = -1;
    int backup_wmix = -1;
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);

    if(enable) {
        if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
            backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
        }

        if(backup_wmix < 0){
            VIOC_WMIX_SetChromaKey((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, 1, nKeyR, nKeyG, nKeyB, nKeyMaskR, nKeyMaskG, nKeyMaskB);
            VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));
        }
        else {
            // Backup
            VIOC_WMIX_SetChromaKey(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, 1, nKeyR, nKeyG, nKeyB, nKeyMaskR, nKeyMaskG, nKeyMaskB);
        }
        
        if(caller_id != VIOCMG_CALLERID_REAR_CAM)
                viocmg_add_chromakey_enable_entry(caller_id, wmix_id, layer);
    }
    else {
        unsigned int remained_owner = 0;

        if(caller_id == VIOCMG_CALLERID_REAR_CAM) {
            remained_owner = 0;
        }
        else {
            struct wmix_enable_entry *wmix_enable_entry;
            viocmg_remove_chromakey_enable_entry(caller_id, wmix_id, layer);
            wmix_enable_entry = viocmg_find_chromakey_enable_entry(wmix_id, layer);

             if(wmix_enable_entry) remained_owner = 1;

        }

        if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
            backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
        }
        if(backup_wmix < 0)  {
            VIOC_WMIX_SetChromaKeyValue((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, nKeyR, nKeyG, nKeyB, nKeyMaskR, nKeyMaskG, nKeyMaskB);
            VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));
        }
        else {
            VIOC_WMIX_SetChromaKeyValue(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, nKeyR, nKeyG, nKeyB, nKeyMaskR, nKeyMaskG, nKeyMaskB);
        }
        

        if(!remained_owner) {
            if(backup_wmix < 0)  {
                VIOC_WMIX_SetChromaKeyEnable((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, 0);
                VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));

            }
            else {
                // Backup
                VIOC_WMIX_SetChromaKeyEnable(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, 0);
            }
        }

    }
   
    
    //mutex_unlock(&viocmg->mutex_wmix); 

    result = 0;
    
    return result;
}

EXPORT_SYMBOL(viocmg_set_wmix_chromakey);





int viocmg_set_wmix_overlayalphavaluecontrol(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int region, unsigned int acon0, unsigned int acon1 )
{
    int result = -1;
    int backup_wmix = -1;
    
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);

    if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
        backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
    }

    if(backup_wmix < 0) {
        VIOC_API_WMIX_SetOverlayAlphaValueControl((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, region, acon0, acon1);
        VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));
    }
    else {
        // backup ovp
        VIOC_API_WMIX_SetOverlayAlphaValueControl(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, region, acon0, acon1);
    }
    result = 0;
    
    //mutex_unlock(&viocmg->mutex_wmix);    

    return result;

}
EXPORT_SYMBOL(viocmg_set_wmix_overlayalphavaluecontrol);

int viocmg_set_wmix_overlayalphacolorcontrol(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int region, unsigned int ccon0, unsigned int ccon1)
{
    int result = -1;
    int backup_wmix = -1;
    
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);

    if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
        backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
    }

    if(backup_wmix < 0) {
        VIOC_API_WMIX_SetOverlayAlphaColorControl((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, region, ccon0, ccon1);
        VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));
    }
    else {
        // backup ovp
        VIOC_API_WMIX_SetOverlayAlphaColorControl(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, region, ccon0, ccon1);
    }
    result = 0;
    
    //mutex_unlock(&viocmg->mutex_wmix);    

    return result;

}
EXPORT_SYMBOL(viocmg_set_wmix_overlayalphacolorcontrol);

int viocmg_set_wmix_overlayalpharopmode(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int opmode )
{
    int result = -1;
    int backup_wmix = -1;
    
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);

    if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
        backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
    }

    if(backup_wmix < 0) {
        VIOC_API_WMIX_SetOverlayAlphaROPMode((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, opmode);
        VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));
    }
    else {
        // backup ovp
        VIOC_API_WMIX_SetOverlayAlphaROPMode(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, opmode);
    }
    result = 0;
    
    //mutex_unlock(&viocmg->mutex_wmix);    

    return result;

}
EXPORT_SYMBOL(viocmg_set_wmix_overlayalpharopmode);


int viocmg_set_wmix_overlayalphaselection(unsigned int caller_id, unsigned int wmix_id, unsigned int layer,unsigned int asel )
{
    int result = -1;
    int backup_wmix = -1;
    
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);

    if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
        backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
    }

    if(backup_wmix < 0) {
        VIOC_API_WMIX_SetOverlayAlphaSelection((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, asel);
        VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));
    }
    else {
        // backup ovp
        VIOC_API_WMIX_SetOverlayAlphaSelection(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, asel);
    }
    result = 0;
    
    //mutex_unlock(&viocmg->mutex_wmix);    

    return result;

}
EXPORT_SYMBOL(viocmg_set_wmix_overlayalphaselection);



int viocmg_set_wmix_overlayalphavalue(unsigned int caller_id, unsigned int wmix_id, unsigned int layer, unsigned int alpha0, unsigned int alpha1 )
{
    int result = -1;
    int backup_wmix = -1;
    
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    //mutex_lock(&viocmg->mutex_wmix);

    if(viocmg_get_rear_cam_mode() > VIOCMG_REAR_MODE_STOP) {
        backup_wmix = viocmg_get_backup_wmix(caller_id, wmix_id);
    }

    if(backup_wmix < 0) {
        VIOC_API_WMIX_SetOverlayAlphaValue((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)), layer, alpha0, alpha1);
        VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * wmix_id)));
    }
    else {
        // backup ovp
        VIOC_API_WMIX_SetOverlayAlphaValue(&viocmg->rear_cam_backup_components.wmix[backup_wmix], layer, alpha0, alpha1);
    }
    result = 0;
    
    //mutex_unlock(&viocmg->mutex_wmix);    

    return result;

}
EXPORT_SYMBOL(viocmg_set_wmix_overlayalphavalue);
#endif



void viocmg_normal_restore(unsigned int caller_id, unsigned int wmix_id, void* wmix_data)
{
    //VIOC_WMIX_SetUpdate(pWMIXBase);

}
EXPORT_SYMBOL(viocmg_normal_restore);

