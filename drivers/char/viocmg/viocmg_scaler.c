
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
#include <linux/uaccess.h>

#include <asm/io.h>
#include <asm/div64.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

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


unsigned int viocmg_scaler_get_max(void)
{
    return VIOC_SC3;
}
EXPORT_SYMBOL(viocmg_scaler_get_max);
    

// find unused scaler.
unsigned int viocmg_scaler_get_unlocked_scaler(void)
{
    struct viocmg_soc *viocmg = viocmg_get_soc();
    
    unsigned int i, scaler_max = viocmg_scaler_get_max();

    mutex_lock(&viocmg->mutex_lock);
    
    for(i=0;i<scaler_max;i++) {
        if(!viocmg->components.scaler[i].owner)
            break;
    }

    mutex_unlock(&viocmg->mutex_lock);

    return i;
}
EXPORT_SYMBOL(viocmg_scaler_get_unlocked_scaler);


unsigned int viocmg_scaler_get_owner(unsigned int scaler_id)
{
    struct viocmg_soc *viocmg = viocmg_get_soc();
        
    return viocmg->components.scaler[scaler_id].owner;
}
EXPORT_SYMBOL(viocmg_scaler_get_owner);


int viocmg_scaler_lock(unsigned int caller_id, unsigned int scaler_id)
{
    int result = -1;
    struct viocmg_soc *viocmg = viocmg_get_soc();
        
    mutex_lock(&viocmg->mutex_lock);

    if(!viocmg->components.scaler[scaler_id].owner) {
        viocmg->components.scaler[scaler_id].owner = caller_id;
        result = 0;
    }
    
    mutex_unlock(&viocmg->mutex_lock);

    return result;
}
EXPORT_SYMBOL(viocmg_scaler_lock);

int viocmg_scaler_unlock(unsigned int caller_id, unsigned int scaler_id)
{
    int result = -1;
    struct viocmg_soc *viocmg = viocmg_get_soc();
        
    mutex_lock(&viocmg->mutex_lock);

    if(viocmg->components.scaler[scaler_id].owner == caller_id) {
        viocmg->components.scaler[scaler_id].owner = 0;
        result = 0;
    }
    
    mutex_unlock(&viocmg->mutex_lock);

    return result;
}
EXPORT_SYMBOL(viocmg_scaler_unlock);






