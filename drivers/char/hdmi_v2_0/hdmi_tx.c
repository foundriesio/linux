/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
#include "include/hdmi_includes.h"
#include "include/hdmi_log.h"
#include "include/irq_handlers.h"
#include "include/proc_fs.h"
#include "include/hdmi_ioctls.h"
#include <asm/bitops.h> // bit macros

#include <linux/of_gpio.h>
#if defined(CONFIG_PM)
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#include "hdmi_api_lib/include/core/irq.h"
#include "hdmi_api_lib/include/phy/phy.h"

#include <linux/clk.h>

int hdmi_log_level = SNPS_WARN;


/** @short License information */
MODULE_LICENSE("GPL v2");
/** @short Author information */
MODULE_AUTHOR("Telechips");
/** @short Device description */
MODULE_DESCRIPTION("HDMI_TX module driver");
/** @short Device version */
MODULE_VERSION("1.0");


//MISC Funcations
extern int dwc_hdmi_misc_register(struct hdmi_tx_dev *dev);
extern int dwc_hdmi_misc_deregister(struct hdmi_tx_dev *dev);
#if defined(CONFIG_TCC_OUTPUT_STARTER)
extern void dwc_hdmi_power_on_core(struct hdmi_tx_dev *dev, int need_link_reset);
extern void dwc_hdmi_power_on(struct hdmi_tx_dev *dev);
#endif

// API Functions
extern void dwc_hdmi_api_register(struct hdmi_tx_dev *dev);
extern void dwc_hdmi_power_on(struct hdmi_tx_dev *dev);
extern void dwc_hdmi_power_off(struct hdmi_tx_dev *dev);



/**
 * @short List of the devices
 * Linked list that contains the installed devices
 */
static LIST_HEAD(devlist_global);

/**
 * @short List of allocated memory
 * Linked list that contains the allocated memory
 */
static struct mem_alloc *alloc_list;

/**
 * @short Map memory blocks
 * @param[in,out] main HDMI TX structure
 * @param[in] Device node pointer
 * @return Return -ENOMEM if one of the blocks is not mapped and 0 if all
 * blocks are mapped successful.
 */
int
of_parse_hdmi_dt(struct hdmi_tx_dev *dev, struct device_node *node){
        int ret = 0;
        const char *dt_string;
        unsigned int of_loop;
        struct device_node *ddibus_np;

	// Map DWC HDMI TX Core
	dev->dwc_hdmi_tx_core_io = of_iomap(node, PROTO_HDMI_TX_CORE);
	if(dev->dwc_hdmi_tx_core_io == NULL){
		pr_err("%s:Unable to map resource\n", __func__);
                ret = -ENODEV;
                goto end_process;

	}

	// Map HDMI TX PHY interface
	dev->hdmi_tx_phy_if_io = of_iomap(node, PROTO_HDMI_TX_PHY); // TXPHY_IF_ADDRESS,
	if(dev->hdmi_tx_phy_if_io == NULL){
		pr_err("%s:Unable to map hdmi_tx_phy_if_base_addr resource\n",
				__func__);
                ret = -ENODEV;
                goto end_process;

	}

	dev->io_bus = of_iomap(node, PROTO_HDMI_TX_IO_BASE);
	if(dev->io_bus == NULL){
	    pr_err("%s:Unable to map io_bus base address resource\n",
	                    __func__);
	    ret = -ENODEV;
	    goto end_process;
	}

        // Find DDI_BUS Node
        ddibus_np = of_find_compatible_node(NULL, NULL, "telechips,ddi_config");
        if(ddibus_np == NULL) {
                pr_err("%s:Unable to map ddibus resource\n",
                                __func__);
                ret = -ENODEV;
                goto end_process;
        }

        // Map DDI_Bus interface
        dev->ddibus_io = of_iomap(ddibus_np, 0);
        if(dev->ddibus_io == NULL){
                pr_err("%s:Unable to map ddibus_io base address resource\n",
                                __func__);
                ret = -ENODEV;
                goto end_process;
        }


		// Parse Interrupt
        for(of_loop = HDMI_IRQ_TX_CORE; of_loop < HDMI_IRQ_TX_MAX; of_loop++) {
                dev->irq[of_loop] = of_irq_to_resource(node, of_loop, NULL);
        }

        // Parse Clock
        for(of_loop = HDMI_CLK_INDEX_PERI; of_loop < HDMI_CLK_INDEX_MAX; of_loop++) {
                dev->clk[of_loop] = of_clk_get(node, of_loop);
                if (IS_ERR(dev->clk[of_loop])) {
                        pr_err("%s:Unable to map hdmi clock (%d)\n",
                                __func__, of_loop);
                        ret = -ENODEV;
                        goto end_process;
                }
        }

        // Parse Clock Freq
        ret = of_property_read_u32(node, "clock-frequency", &dev->clk_freq_apb);
        if(ret < 0) {
                pr_err("%s:Unable to map hdmi pclk frequency\n", __func__);
                ret = -ENODEV;
                goto end_process;
        }


        if(of_property_read_u32(node, "audio_if_selection_ofst", &dev->hdmi_audio_if_sel_ofst) < 0) {
                dev->hdmi_audio_if_sel_ofst = 0xff;
                pr_err("%s:Unable to map audio source offset\n", __func__);
        }

        if(of_property_read_u32(node, "audio_rx_tx_chmux", &dev->hdmi_rx_tx_chmux) < 0) {
                dev->hdmi_rx_tx_chmux =0xff;
                //pr_info("%s: Invalide hdmi rx and tx mux selection register.\r\n", __func__);
        }

        of_property_read_u32(node, "hdmi_phy_type", &dev->hdmi_phy_type);
        if(dev->hdmi_phy_type >= hdmi_phy_type_max) {
                pr_info("%s: Invalide hdmi phy type. set default phy type\r\n", __func__);
                dev->hdmi_phy_type = 0;
        }

        if(of_property_read_u32(node, "hdmi_ref_src_clock", &dev->hdmi_ref_src_clk) < 0) {
                pr_info("%s: Invalide hdmi reference source clock. set xin clock to reference source clock\r\n", __func__);
                dev->hdmi_ref_src_clk = 0;
        }

        // Update the verbose level
        of_property_read_u32(node, "verbose", &dev->verbose);
        if(dev->verbose >= VERBOSE_BASIC)
                        pr_info("%s: verbose level is %d\n", __func__, dev->verbose);
        // Find the hotplug gpio..
        dev->hotplug_gpio = of_get_gpio(node, 0);


        /* Source Product Description */
        dt_string = NULL;
        if(of_property_read_string(node, "vendor_name", &dt_string) == 0 && dt_string != NULL) {
                strncpy(dev->vendor_name, dt_string, sizeof(dev->vendor_name));
        } else {
                strcpy(dev->vendor_name, "TCC");
        }
        dt_string = NULL;
        if(of_property_read_string(node, "product_description", &dt_string) == 0 && dt_string != NULL) {
                strncpy(dev->product_description, dt_string, sizeof(dev->product_description));
        } else {
                strcpy(dev->product_description, "TCC8xxxx");
        }

        if(of_property_read_u32(node, "source_information", &dev->source_information) < 0) {
                dev->source_information = 1;
        }

end_process:
	return ret;
}

static int of_parse_i2c_mapping(struct hdmi_tx_dev *dev){
        int ret = 0;
        volatile unsigned int val;
        struct device_node *node = NULL;
        volatile void __iomem   *io_i2c_map = NULL;
        unsigned int i2c_mapping_offset, i2c_mapping_mask, i2c_mapping_val;

        do {

                if(dev == NULL) {
                        pr_err("%s hdmi_tx_dev is NULL\r\n", __func__);
                        break;
                }
                if(dev->parent_dev == NULL) {
                        pr_err("%s hdmi_tx_dev->parent_dev is NULL\r\n", __func__);
                        break;
                }

                node = dev->parent_dev->of_node;
                if(node == NULL) {
                        pr_err("%s of_node is NULL\r\n", __func__);
                        break;
                }

                /* Get whether to use DDC */
                if(of_property_read_u32(node, "hdmi_i2c_port_disable", &dev->ddc_disable) < 0) {
                        dev->ddc_disable = 0;
                }

                if(dev->ddc_disable) {
                        pr_info("%s ddc is disabled.. \r\n", __func__);
                        break;
                }

                // Map HDMI TX PHY interface
                io_i2c_map = of_iomap(node, PROTO_HDMI_TX_I2C_MAP);
                if(io_i2c_map == NULL){
                        pr_err("%s:Unable to map i2c mapping resource\n", __func__);
                        ret = -ENODEV;
                        goto end_process;
                }
                ret = of_property_read_u32_index(node, "hdmi_i2c_port_mapping", 0, &i2c_mapping_offset);
                if(ret < 0) {
                        pr_err("%s:Unable to map i2c_mapping_offset\n", __func__);
                        ret = -ENODEV;
                        goto end_process;
                }

                ret = of_property_read_u32_index(node, "hdmi_i2c_port_mapping", 1, &i2c_mapping_mask);
                if(ret < 0) {
                        pr_err("%s:Unable to map i2c_mapping_mask\n", __func__);
                        ret = -ENODEV;
                        goto end_process;
                }

                ret = of_property_read_u32_index(node, "hdmi_i2c_port_mapping", 2, &i2c_mapping_val);
                if(ret < 0) {
                        pr_err("%s:Unable to map i2c_mapping_val\n", __func__);
                        ret = -ENODEV;
                        goto end_process;
                }

                val = ioread32((void*)(io_i2c_map + i2c_mapping_offset));
                val &= ~i2c_mapping_mask;
                val |= i2c_mapping_val;
                iowrite32(val, (void*)(io_i2c_map + i2c_mapping_offset));
                pr_info("hdmi i2c mapping offset(0x%x), mask(0x%x), val(0x%x), read_val(0x%x)\r\n",
                                i2c_mapping_offset,
                                i2c_mapping_mask,
                                i2c_mapping_val,
                                ioread32((void*)(io_i2c_map + i2c_mapping_offset)));
        }while(0);

end_process:

        if(io_i2c_map != NULL)
                iounmap(io_i2c_map);

        return ret;
}


/**
 * @short Release memory blocks
 * @param[in,out] main HDMI TX structure
 * @return Void
 */
void
release_memory_blocks(struct hdmi_tx_dev *dev){
        if(dev->ddibus_io != NULL)
                iounmap(dev->ddibus_io);
	dev->ddibus_io = NULL;

        if(dev->hdmi_tx_phy_if_io != NULL)
                iounmap(dev->hdmi_tx_phy_if_io);
	dev->hdmi_tx_phy_if_io = NULL;

        if(dev->dwc_hdmi_tx_core_io != NULL)
                iounmap(dev->dwc_hdmi_tx_core_io);
	dev->dwc_hdmi_tx_core_io = NULL;
}

/**
 * @short Allocate memory
 * @param[in] info String to associate with memory allocated
 * @param[in] size Size of the memory to allocate
 * @param[in,out] allocated Pointer to the structure that contains the info
 * about the allocation
 * @return Void
 */
void *
alloc_mem(char *info, size_t size, struct mem_alloc *allocated){
        struct mem_alloc *new = NULL;
        int *return_pnt = NULL;

        // first time
        if(alloc_list == NULL){
                alloc_list = kzalloc(sizeof(struct mem_alloc), GFP_KERNEL);
                if(alloc_list == NULL){
                        printk( KERN_ERR "%s:Couldn't create alloc_list\n",
                        __func__);
                        return NULL;
                }
                alloc_list->instance = 0;
                alloc_list->info = "allocation list - instance 0";
                alloc_list->size = 0;
                alloc_list->pointer = NULL;
                alloc_list->last = NULL;
                alloc_list->prev = NULL;
        }

        // alloc pretended memory
        return_pnt = kzalloc(size, GFP_KERNEL);
        if(return_pnt == NULL){
                printk( KERN_ERR "%s:Couldn't allocate memory: %s\n",
                __func__, info);
                return NULL;
        }

        // alloc memory for the infostructure
        new = kzalloc(sizeof(struct mem_alloc), GFP_KERNEL);
        if(new == NULL){
                printk( KERN_ERR "%s:Couldn't allocate memory for the "
                "alloc_mem\n", __func__);
                kfree(return_pnt);
                return NULL;
        }

        new->instance = ++alloc_list->instance;
        new->info = info;
        new->size = size;
        alloc_list->size += size;
        new->pointer = return_pnt;
        if(alloc_list->last == NULL){
                new->prev = alloc_list;	// First instance
        }
        else{
                new->prev = alloc_list->last;
        }
        alloc_list->last = new;
        new->last = new;

        return return_pnt;
}

/**
 * @short Free all memory
 * This was implemented this way so that all memory allocated was de-allocated
 * and to avoid memory leaks.
 * @return Void
 */
void
free_all_mem(void){
        if(alloc_list != NULL){
                while(alloc_list->instance != 0){
                        struct mem_alloc *this;
                        this = alloc_list->last;
                        // cut this from list
                        alloc_list->last = this->prev;
                        alloc_list->instance--;
                        alloc_list->size -= this->size;
                        // free allocated memory
                        kfree(this->pointer);
                        // free this memory
                        printk( KERN_INFO "%s:Freeing: %s\n",
                        __func__, this->info);
                        kfree(this);
                }
                kfree(alloc_list);
                alloc_list = NULL;
        }
}

#if defined(CONFIG_PM)
int hdmi_tx_suspend(struct device *dev)
{
	int backups[2];

        struct hdmi_tx_dev *hdmi_tx_dev = (struct hdmi_tx_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;

	if(hdmi_tx_dev != NULL) {
                pr_info("### hdmi_tx_suspend \r\n");
                if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &hdmi_tx_dev->status)) {
                        if(gpio_is_valid(hdmi_tx_dev->hotplug_gpio)) {
                                hdmi_tx_dev->hotplug_status = 0;
                                dwc_hdmi_tx_set_hotplug_interrupt(hdmi_tx_dev, 0);
                        } else {
                                /* Backup enable val */
                                backups[0] = hdmi_tx_dev->hotplug_irq_enable;
                                hdmi_tx_dev->hotplug_irq_enable = 0;
                                hdmi_hpd_enable(hdmi_tx_dev);

                                /* Restore enable val */
                                hdmi_tx_dev->hotplug_irq_enable = backups[0];
                        }

			/* Backup enable counts */
                        backups[0] = hdmi_tx_dev->hdmi_clock_enable_count;
			backups[1] = hdmi_tx_dev->display_clock_enable_count;
                        if(hdmi_tx_dev->hdmi_clock_enable_count > 0) {
                                hdmi_tx_dev->hdmi_clock_enable_count = 1;
                        }
                        if(hdmi_tx_dev->display_clock_enable_count > 0) {
                                hdmi_tx_dev->display_clock_enable_count = 1;
                        }
                        dwc_hdmi_power_off(hdmi_tx_dev);
			/* Restore enable counts */
                        hdmi_tx_dev->hdmi_clock_enable_count = backups[0];
                        hdmi_tx_dev->display_clock_enable_count = backups[1];
                        set_bit(HDMI_TX_STATUS_SUSPEND_L1, &hdmi_tx_dev->status);
                } else {
			pr_err("%s already suspended\r\n", __func__);
		}
	}
        return 0;
}

int hdmi_tx_resume(struct device *dev)
{
        int backups[3];
        struct hdmi_tx_dev *hdmi_tx_dev = (struct hdmi_tx_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;

        // Nothing..!!
        printk("### %s \n", __func__);
        if(hdmi_tx_dev != NULL) {
                if(test_bit(HDMI_TX_STATUS_SUSPEND_L1, &hdmi_tx_dev->status)) {
                        if(!test_bit(HDMI_TX_STATUS_SUSPEND_L0, &hdmi_tx_dev->status)) {
                                if(gpio_is_valid(hdmi_tx_dev->hotplug_gpio)) {
                                        hdmi_tx_dev->hotplug_status = hdmi_tx_dev->hotplug_real_status = gpio_get_value(hdmi_tx_dev->hotplug_gpio);
                                        dwc_hdmi_tx_set_hotplug_interrupt(hdmi_tx_dev, 1);
                                } else {
					hdmi_hpd_enable(hdmi_tx_dev);
				}


                                of_parse_i2c_mapping(hdmi_tx_dev);

				/* Clear suspend status for enable closkc */
				clear_bit(HDMI_TX_STATUS_SUSPEND_L1, &hdmi_tx_dev->status);

                                if(hdmi_tx_dev->display_clock_enable_count > 0) {
					/* HDMI driver should enable clocks */

                                        /* Backup enable counts */
		                        backups[0] = hdmi_tx_dev->hdmi_clock_enable_count;
					backups[1] = hdmi_tx_dev->display_clock_enable_count;

                                        /* Backup phy alive */
                                        if(test_bit(HDMI_TX_STATUS_PHY_ALIVE, &hdmi_tx_dev->status)) {
                                                backups[2] = 1;
                                        } else {
                                                backups[2] = 0;
                                        }
                                        clear_bit(HDMI_TX_STATUS_PHY_ALIVE, &hdmi_tx_dev->status);

                                        hdmi_tx_dev->display_clock_enable_count = 0;
                                        if(hdmi_tx_dev->hdmi_clock_enable_count == 0) {
						/* HDMI Link was disabled */
                                                hdmi_tx_dev->hdmi_clock_enable_count = -1;
                                        } else {
						/* HDMI Link was enabled */
                                                hdmi_tx_dev->hdmi_clock_enable_count = 0;
                                        }
                                        dwc_hdmi_power_on(hdmi_tx_dev);

                                        /* Restore enable counts */
		                        hdmi_tx_dev->hdmi_clock_enable_count = backups[0];
		                        hdmi_tx_dev->display_clock_enable_count = backups[1];

                                        /* Restore phy alive */
                                        if(backups[2]) {
                                                set_bit(HDMI_TX_STATUS_PHY_ALIVE, &hdmi_tx_dev->status);
                                        } else {
                                                clear_bit(HDMI_TX_STATUS_PHY_ALIVE, &hdmi_tx_dev->status);
                                        }

                                }
                        }
                }
        }
        return 0;
}

int hdmi_tx_runtime_suspend(struct device *dev)
{
        struct hdmi_tx_dev *hdmi_tx_dev = (struct hdmi_tx_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;

        pr_info("### hdmi_tx_runtime_suspend\r\n");
        if(hdmi_tx_dev != NULL) {
		mutex_lock(&hdmi_tx_dev->mutex);
                set_bit(HDMI_TX_STATUS_SUSPEND_L0, &hdmi_tx_dev->status);
                hdmi_tx_suspend(dev);
                mutex_unlock(&hdmi_tx_dev->mutex);
        }
        return 0;
}

int hdmi_tx_runtime_resume(struct device *dev)
{
        struct hdmi_tx_dev *hdmi_tx_dev = (struct hdmi_tx_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
        pr_info("## hdmi_tx_runtime_resume\r\n");
        if(hdmi_tx_dev != NULL) {
                mutex_lock(&hdmi_tx_dev->mutex);
                clear_bit(HDMI_TX_STATUS_SUSPEND_L0, &hdmi_tx_dev->status);
                hdmi_tx_resume(dev);
                mutex_unlock(&hdmi_tx_dev->mutex);
        }
        return 0;
}

static void send_hdmi_output_event(struct work_struct *work)
{
        char *u_events[2];
        char u_event_name[16];
        struct hdmi_tx_dev *dev = container_of(work, struct hdmi_tx_dev, hdmi_output_event_work);
        if(dev != NULL) {
                snprintf(u_event_name, sizeof(u_event_name), "SWITCH_STATE=%d", test_bit(HDMI_TX_STATUS_OUTPUT_ON, &dev->status)?1:0);
                u_events[0] = u_event_name;
                u_events[1] = NULL;
                pr_info("%s u_event(%s)\r\n", __func__, u_event_name);
                kobject_uevent_env(&dev->parent_dev->kobj, KOBJ_CHANGE, u_events);
        }
}

#if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
static void hdmi_tx_restore_hotpug_work(struct work_struct *work)
{
        struct hdmi_tx_dev *dev = container_of((struct delayed_work *)work, struct hdmi_tx_dev, hdmi_restore_hotpug_work);
        if(dev != NULL) {
                pr_info("%s restore hotplug_status\r\n", __func__);
                dev->hotplug_status = dev->hotplug_real_status;
        }
}
#endif

static const struct dev_pm_ops hdmi_tx_pm_ops = {
        .suspend = hdmi_tx_suspend,
        .resume = hdmi_tx_resume,
        .runtime_suspend = hdmi_tx_runtime_suspend,
        .runtime_resume = hdmi_tx_runtime_resume,
};
#endif // CONFIG_PM

/**
 * @short Initialization routine - Entry point of the driver
 * @param[in] pdev pointer to the platform device structure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
static int
hdmi_tx_init(struct platform_device *pdev){
        int ret = 0;
        struct hdmi_tx_dev *dev = NULL;

        pr_info("****************************************\n");
        pr_info("%s:HDMI driver %s\n", __func__, HDMI_DRV_VERSION);
        pr_info("****************************************\n");
        dev = alloc_mem("HDMI TX Device", sizeof(struct hdmi_tx_dev), NULL);
        if(dev == NULL){
                pr_err("%s:Could not allocated hdmi_tx_dev\n", __func__);
                return -ENOMEM;
        }

        // Zero the device
        memset(dev, 0, sizeof(struct hdmi_tx_dev));

        // Update the device node
        dev->parent_dev = &pdev->dev;
        dev->device_name = "HDMI_TX";
        pr_info("%s:Driver's name '%s'\n", __func__, dev->device_name);

        spin_lock_init(&dev->slock);
        spin_lock_init(&dev->slock_power);

        mutex_init(&dev->mutex);

        INIT_WORK(&dev->hdmi_output_event_work, send_hdmi_output_event);
        #if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
        INIT_DELAYED_WORK(&dev->hdmi_restore_hotpug_work, hdmi_tx_restore_hotpug_work);
        #endif

        dev->videoParam = (void*)devm_kzalloc(dev->parent_dev, sizeof(videoParams_t), GFP_KERNEL);
        dev->audioParam = (void*)devm_kzalloc(dev->parent_dev, sizeof(audioParams_t), GFP_KERNEL);
        dev->productParam = (void*)devm_kzalloc(dev->parent_dev, sizeof(productParams_t), GFP_KERNEL);
        dev->drmParm = (void*)devm_kzalloc(dev->parent_dev, sizeof(DRM_Packet_t), GFP_KERNEL);

        // always data enable polarity is 1.
        dev->hdmi_tx_ctrl.data_enable_polarity = 1;
        dev->hdmi_tx_ctrl.csc_on = 1;
        dev->hdmi_tx_ctrl.audio_on = 1;

        // Initialize IRQ Enable List..
        INIT_LIST_HEAD(&dev->irq_enable_entry.list);

        // wait queue of poll
        init_waitqueue_head(&dev->poll_wq);

        // Map memory blocks
        if(of_parse_hdmi_dt(dev, pdev->dev.of_node) < 0){
                pr_info("%s:Map memory blocks failed\n", __func__);
                ret = -ENOMEM;
                goto free_mem;
        }

        if(of_parse_i2c_mapping(dev) < 0){
                pr_info("%s:Map i2c mapping failed\n", __func__);
                ret = -ENOMEM;
                goto free_mem;
        }

        // Enable All Interrupts
        dwc_init_interrupts(dev);

        // Proc file system
        pr_info("%s:Init proc file system @ /proc/hdmi_tx\n", __func__);
        proc_interface_init(dev);

        // Now that everything is fine, let's add it to device list
        list_add_tail(&dev->devlist, &devlist_global);

        dwc_hdmi_api_register(dev);

        // Register MISC Driver.
        dwc_hdmi_misc_register(dev);

        #if defined(CONFIG_PM)
        pm_runtime_set_active(dev->parent_dev);
        pm_runtime_enable(dev->parent_dev);
        pm_runtime_get_noresume(dev->parent_dev);
        #endif

        // Enable SCDC CHECK..
        //set_bit(HDMI_TX_STATUS_SCDC_CHECK, &dev->status);


        #if defined(CONFIG_TCC_OUTPUT_STARTER)
        if(dev->ddibus_io != NULL)
        {
        	unsigned int val = ioread32(dev->ddibus_io+0x10);
        	if(val & (1 << 15)) {
        		//pr_info("HDMI already Enabled\r\n");
        		dwc_hdmi_power_on_core(dev, 0);
        	}
        	else  {
        		//pr_info("HDMI needs Enable\r\n");
        		dwc_hdmi_power_on(dev);
        	}
        }
        #endif

        pr_info("%s done!!\r\n", __func__);
        return ret;

free_mem:
        if(dev->videoParam != NULL) {
                devm_kfree(dev->parent_dev, dev->videoParam);
                dev->videoParam = NULL;
        }
        if(dev->audioParam != NULL) {
                devm_kfree(dev->parent_dev, dev->audioParam);
                dev->audioParam = NULL;
        }
        if(dev->productParam != NULL) {
                devm_kfree(dev->parent_dev, dev->productParam);
                dev->productParam = NULL;
        }
        if(dev->drmParm != NULL) {
                devm_kfree(dev->parent_dev, dev->drmParm);
                dev->drmParm = NULL;
        }
        release_memory_blocks(dev);
        free_all_mem();
        return ret;
}


/**
 * @short Exit routine - Exit point of the driver
 * @param[in] pdev pointer to the platform device structure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
static int
hdmi_tx_exit(struct platform_device *pdev){
        struct hdmi_tx_dev *dev;
        struct list_head *list;

        pr_info("**************************************\n");
        pr_info("%s:Removing HDMI module\n", __func__);
        pr_info("**************************************\n");

        while(!list_empty(&devlist_global)){
                list = devlist_global.next;
                list_del(list);
                dev = list_entry(list, struct hdmi_tx_dev, devlist);

                if(dev == NULL){
                        continue;
                }

                if(dev->videoParam != NULL) {
                        devm_kfree(dev->parent_dev, dev->videoParam);
                        dev->videoParam = NULL;
                }

                if(dev->audioParam != NULL) {
                        devm_kfree(dev->parent_dev, dev->audioParam);
                        dev->audioParam = NULL;
                }

                if(dev->productParam != NULL) {
                        devm_kfree(dev->parent_dev, dev->productParam);
                        dev->productParam = NULL;
                }

                #if defined(CONFIG_PM)
                pm_runtime_disable(dev->parent_dev);
                #endif


                pr_info("%s:Remove proc file system\n", __func__);
                proc_interface_remove(dev);

                dwc_deinit_interrupts(dev);

                // Deregister HDMI Misc driver.
                dwc_hdmi_misc_deregister(dev);

                // Release memory blocks
                pr_info("%s:Release memory blocks\n", __func__);
                release_memory_blocks(dev);

                free_all_mem();
        }
        return 0;
}

/**
 * @short of_device_id structure
 */
static const struct of_device_id dw_hdmi_tx[] = {
        { .compatible =	"telechips,dw-hdmi-tx" },
        { }
};
MODULE_DEVICE_TABLE(of, dw_hdmi_tx);

/**
 * @short Platform driver structure
 */
static struct platform_driver __refdata dwc_hdmi_tx_pdrv = {
        .remove = hdmi_tx_exit,
        .probe = hdmi_tx_init,
        .driver = {
                .name = "telechips,dw-hdmi-tx",
                .owner = THIS_MODULE,
                .of_match_table = dw_hdmi_tx,
                #if defined(CONFIG_PM)
                .pm = &hdmi_tx_pm_ops,
                #endif
        },
};

static __init int dwc_hdmi_init(void)
{
        return platform_driver_register(&dwc_hdmi_tx_pdrv);
}

static __exit void dwc_hdmi_exit(void)
{
        printk("%s \n", __func__);
        platform_driver_unregister(&dwc_hdmi_tx_pdrv);
}

module_init(dwc_hdmi_init);
module_exit(dwc_hdmi_exit);

