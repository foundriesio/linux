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

#define VIOMG_DEBUG 		0
#define dprintk(msg...) 	if(VIOMG_DEBUG) { printk("REAR_CAM: " msg); }


unsigned int viocmg_get_rear_cam_ovp(void)
{
    struct viocmg_soc *viocmg = viocmg_get_soc();
    return viocmg->viocmg_dt_info->rear_cam_ovp;
}
EXPORT_SYMBOL_GPL(viocmg_get_rear_cam_ovp);


unsigned int viocmg_get_rear_cam_vin_rdma_id(void)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        return viocmg->viocmg_dt_info->rear_cam_vin_rdma;
}
EXPORT_SYMBOL_GPL(viocmg_get_rear_cam_vin_rdma_id);

unsigned int viocmg_get_rear_cam_vin_id(void)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        return viocmg->viocmg_dt_info->rear_cam_vin_vin;
}
EXPORT_SYMBOL_GPL(viocmg_get_rear_cam_vin_id);

unsigned int viocmg_get_rear_cam_scaler_id(void)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        return viocmg->viocmg_dt_info->rear_cam_vin_scaler;
}
EXPORT_SYMBOL_GPL(viocmg_get_rear_cam_scaler_id);


unsigned int viocmg_get_rear_cam_vin_wmix_id(void)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        return viocmg->viocmg_dt_info->rear_cam_vin_wmix;
}
EXPORT_SYMBOL_GPL(viocmg_get_rear_cam_vin_wmix_id);

unsigned int viocmg_get_rear_cam_vin_wdma_id(void)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        return viocmg->viocmg_dt_info->rear_cam_vin_wdma;
}
EXPORT_SYMBOL_GPL(viocmg_get_rear_cam_vin_wdma_id);


unsigned int viocmg_get_rear_cam_display_rdma_id(void)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        return viocmg->viocmg_dt_info->rear_cam_display_rdma;
}
EXPORT_SYMBOL_GPL(viocmg_get_rear_cam_display_rdma_id);


void viocmg_set_rear_cam_suspend(unsigned int suspend)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        
        viocmg->reardrv_suspend = suspend;
}
EXPORT_SYMBOL_GPL(viocmg_set_rear_cam_suspend);

void viocmg_set_rear_cam_gearstatus_api(void* api)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();
        
        viocmg->reardrv_get_gearstatus = (unsigned int (*)(void))api;
}
EXPORT_SYMBOL_GPL(viocmg_set_rear_cam_gearstatus_api);

void viocmg_set_rear_cam_dmastatus_api(void* api)
{
        struct viocmg_soc *viocmg = viocmg_get_soc();

        viocmg->reardrv_get_dmastatus = (unsigned int (*)(void))api;
}
EXPORT_SYMBOL_GPL(viocmg_set_rear_cam_dmastatus_api);

static void viocmg_rear_cam_save_wmix(void)
{
    struct viocmg_soc *viocmg = viocmg_get_soc();

    memcpy(&viocmg->rear_cam_backup_components.wmix[0], (void *)(viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->main_display_id)), 0x20);
    memcpy(&viocmg->rear_cam_backup_components.wmix[1], (void *)(viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->rear_cam_vin_wmix)), 0x20);
}

static void viocmg_rear_cam_restore_wmix(void)
{
#if 0	/* please check it */
    void __iomem *source_wmix, *dest_wmix;
    volatile unsigned long source_position, dest_positon;
    struct viocmg_soc *viocmg = viocmg_get_soc();

    source_wmix = viocmg->rear_cam_backup_components.wmix[REARCAM_BK_WMIX_DISPLAY];
    dest_wmix = viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->main_display_id);
    
    // ovp
    dest_wmix->uCTRL = source_wmix->uCTRL;
    
    // position
    source_position = &source_wmix->uPOS0;
    dest_positon = &dest_wmix->uPOS0;
    dest_positon[viocmg->viocmg_dt_info->rear_cam_display_rdma%4].nREG = source_position[viocmg->viocmg_dt_info->rear_cam_display_rdma%4].nREG;
    dprintk(" saved wmix position[%d] = 0x%x\r\n", viocmg->viocmg_dt_info->rear_cam_display_rdma%4, source_position[viocmg->viocmg_dt_info->rear_cam_display_rdma%4].nREG);
    // chroma
    // SKIP    
    
    // alpha
    // SKIP

    memcpy((void *)(viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->rear_cam_vin_wmix)), &viocmg->rear_cam_backup_components.wmix[REARCAM_BK_WMIX_VIN], sizeof(VIOC_WMIX));
    
    VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->main_display_id)));
    VIOC_WMIX_SetUpdate((VIOC_WMIX *)(viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->rear_cam_vin_wmix)));
#endif
}

static void viocmg_rear_cam_set_rearovp(void)
{
#if 0		/* please check it */
    struct viocmg_soc *viocmg = viocmg_get_soc();

    VIOC_WMIX_SetOverlayPriority(
        viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->main_display_id),
        viocmg->viocmg_dt_info->rear_cam_ovp);
    VIOC_WMIX_SetUpdate(viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->main_display_id));
#endif
}

/*************************************************
 * viocmg_save_wmix_default
 *  save default wmix state. 
 *
 *************************************************/
void viocmg_save_wmix_default(void)
{
#if 0	/* please check it */
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    memcpy(&viocmg->rear_cam_backup_components.wmix[0], (void *)(viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->main_display_id)), 0x20);
    memcpy(&viocmg->rear_cam_backup_components.wmix[1], (void *)(viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->rear_cam_vin_wmix)), 0x20);

   VIOC_WMIX_SetOverlayPriority(&viocmg->rear_cam_backup_components.wmix[0], viocmg->viocmg_dt_info->main_display_ovp);
#endif
}
EXPORT_SYMBOL_GPL(viocmg_save_wmix_default);


void viocmg_set_rear_cam_mode(unsigned int caller_id, unsigned int mode)
{
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    if(caller_id == VIOCMG_CALLERID_REAR_CAM) {
        // Lock  (wmix lock)
        viocmg->viocmg_dt_info->rear_cam_mode = mode;

        dprintk("viocmg_set_rear_cam_mode(0x%x, %d)\r\n", caller_id, mode);
        switch(mode)
        {
                case VIOCMG_REAR_MODE_STOP:
                        if(viocmg->viocmg_dt_info->feature_rear_cam_use_viqe)
                                viocmg_free_viqe(caller_id);
                        viocmg_rear_cam_restore_wmix();
                        break;
                case VIOCMG_REAR_MODE_PREPARE:
                        viocmg_rear_cam_save_wmix();

                        if(viocmg->viocmg_dt_info->feature_rear_cam_use_viqe)
                        {
                                while(viocmg_lock_viqe(caller_id) < 0) {
                                        int timeleft = wait_for_completion_interruptible_timeout(&viocmg->components.viqe.completion, (HZ/10));

                                        dprintk("timeleft = %d, HZ=%d\r\n", timeleft, HZ);

                                        // retry lock viqe..!
                                        // viocmg_lock_viqe(caller_id);
                                }
                                //VIOC_CONFIG_PlugOut(VIOC_VIQE);
                        }
                        break;
                case VIOCMG_REAR_MODE_RUN:
                        viocmg_rear_cam_set_rearovp();
                        break;
        }

        // UnLock
    }
}
EXPORT_SYMBOL_GPL(viocmg_set_rear_cam_mode);


unsigned int viocmg_get_rear_cam_mode(void)
{
    unsigned int result = 0;
    struct viocmg_soc *viocmg = viocmg_get_soc();

    if(viocmg->viocmg_dt_info->feature_rear_cam_enable) {
        if(viocmg->reardrv_suspend) {
                unsigned int wmix_ovp;
                dprintk("viocmg_get_rear_cam_mode rear driver is suspend\r\n");
                VIOC_WMIX_GetOverlayPriority(viocmg->reg.display_wmix + (0x100 * viocmg->viocmg_dt_info->main_display_id), &wmix_ovp);

                dprintk("wmix_ovp = 0x%x, rear ovp = 0x%x\r\n", wmix_ovp, viocmg->viocmg_dt_info->rear_cam_ovp);
                if(wmix_ovp == viocmg->viocmg_dt_info->rear_cam_ovp)// && viocmg->reardrv_get_dmastatus())
                        viocmg->viocmg_dt_info->rear_cam_mode = VIOCMG_REAR_MODE_RUN;
                else
                        viocmg->viocmg_dt_info->rear_cam_mode = VIOCMG_REAR_MODE_STOP;
        }
        dprintk("viocmg_get_rear_cam_mode = %d\r\n", viocmg->viocmg_dt_info->rear_cam_mode);
        result = viocmg->viocmg_dt_info->rear_cam_mode;
    }
    return result;
}
EXPORT_SYMBOL_GPL(viocmg_get_rear_cam_mode);




