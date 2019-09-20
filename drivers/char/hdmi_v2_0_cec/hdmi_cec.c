/****************************************************************************
Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA

NOTE: Tab size is 8
****************************************************************************/
#include"include/hdmi_cec.h"
#include"include/hdmi_cec_misc.h"
#include"include/cec_proc_fs.h"

#include "hdmi_cec_lib/cec.h"
#include "hdmi_cec_lib/cec_access.h"

#include<linux/clk.h>

#if defined(CONFIG_PM)
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

static int hdmi_cec_probe(struct platform_device *pdev){
        int i, ret = -ENOMEM;
        struct cec_device *dev = NULL;

        do {

                if(pdev == NULL) {
                        pr_err("%s: pdev is NULL\r\n", __func__);
                        break;
                }

                dev = (void*)devm_kzalloc(&pdev->dev, sizeof(struct cec_device), GFP_KERNEL);
                if(!dev){
                        pr_err("%s:Could not allocated hdmi_tx_dev\n", __func__);
                        break;
                }

                // Zero the device
                memset(dev, 0, sizeof(struct cec_device));

                // Update the device node
                dev->parent_dev = &pdev->dev;

                dev->device_name = "HDMI_CEC";

                printk("%s:Driver's name '%s' v%s\n", __func__, dev->device_name, HDMI_CEC_VERSION);

                mutex_init(&dev->mutex);

                /* Parse Device Tree */
        	if(pdev->dev.of_node){
        		dev->cec_core_io = of_iomap(pdev->dev.of_node, 0);
                        dev->cec_irq_io = of_iomap(pdev->dev.of_node, 1);
        		dev->cec_clk_sel = of_iomap(pdev->dev.of_node, 2);
        		dev->cec_irq = irq_of_parse_and_map(pdev->dev.of_node,0);
        		dev->cec_wake_up_irq = irq_of_parse_and_map(pdev->dev.of_node,1);

        		for(i = HDMI_CLK_CEC_INDEX_IOBUS; i < HDMI_CLK_CEC_INDEX_MAX; i++) {
        			dev->clk[i] = of_clk_get(pdev->dev.of_node,i);
        		}
        	} else {
        		printk("%s: device node is null!!!",__func__);
        		break;
        	}

                if(dev->cec_core_io == NULL) {
                        pr_err("%s cec_core_io is NULL\r\n", __func__);
                        break;
                }
                if(dev->cec_irq_io == NULL) {
                        pr_err("%s cec_irq_io is NULL\r\n", __func__);
                        break;
                }
                if(dev->cec_clk_sel == NULL) {
                        pr_err("%s cec_clk_sel is NULL\r\n", __func__);
                        break;
                }

                for(i = HDMI_CLK_CEC_INDEX_IOBUS; i < HDMI_CLK_CEC_INDEX_MAX; i++) {
                        if(IS_ERR(dev->clk[i])) {
                                pr_err("%s clk[%d] is not valid\r\n", __func__, i);
                                break;
                        }
                }
                if(i<HDMI_CLK_CEC_INDEX_MAX) {
                        break;
                }

                platform_set_drvdata(pdev, dev);

        	hdmi_cec_misc_register(dev);

        	dev->standby_status = 0;

                /* Set clock source of cec core clock to xtin and sfr clock of cec to xin */
                cec_dev_sel_write(dev, 1);

        	#ifdef CONFIG_PM
        	pm_runtime_set_active(dev->parent_dev);
        	pm_runtime_enable(dev->parent_dev);
        	pm_runtime_get_noresume(dev->parent_dev);  //increase usage_count
        	#endif

        	hdmi_cec_proc_interface_init(dev);

                ret = 0;
        } while(0);

        if(ret < 0) {
                if(dev->cec_clk_sel != NULL)
                	iounmap(dev->cec_clk_sel);
                if(dev->cec_irq_io != NULL)
                	iounmap(dev->cec_irq_io);
                if(dev->cec_core_io  != NULL)
                                iounmap(dev->cec_core_io);

                devm_kfree(dev->parent_dev, dev);
        }

        return ret;
}


/**
 * @short Exit routine - Exit point of the driver
 * @param[in] pdev pointer to the platform device structure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
static int hdmi_cec_remove(struct platform_device *pdev){
        struct cec_device *dev =
                (struct cec_device *)(pdev!=NULL)?platform_get_drvdata(pdev):NULL;

        if(dev != NULL) {
                cec_power_off(dev);

                hdmi_cec_proc_interface_remove(dev);

                hdmi_cec_misc_deregister(dev);
                if(dev->cec_clk_sel != NULL)
        	        iounmap(dev->cec_clk_sel);
                if(dev->cec_irq_io != NULL)
                	iounmap(dev->cec_irq_io);
                if(dev->cec_core_io  != NULL)
                                iounmap(dev->cec_core_io);
                devm_kfree(dev->parent_dev, dev);
        }

        return 0;
}


/**
 * @short of_device_id structure
 */
static const struct of_device_id dw_hdmi_cec[] = {
        { .compatible =	"telechips,dw-hdmi-cec" },
        { }
};
MODULE_DEVICE_TABLE(of, dw_hdmi_cec);


#if defined(CONFIG_PM)

int hdmi_cec_suspend(struct device *dev)
{
        int clk_enable_count;
	struct cec_device *hdmi_cec_dev = (struct cec_device *)dev_get_drvdata(dev);
	printk("### %s \n", __func__);

        if(hdmi_cec_dev != NULL) {
                if(hdmi_cec_dev->cec_wakeup_enable) {
                        cec_set_wakeup(hdmi_cec_dev, WAKEUP_MASK_FLAG);
                } else {
                        clk_enable_count = hdmi_cec_dev->clk_enable_count;
                        if(hdmi_cec_dev->clk_enable_count > 0) {
                                hdmi_cec_dev->clk_enable_count = 1;
                        }
                        cec_power_off(hdmi_cec_dev);
                        hdmi_cec_dev->clk_enable_count = clk_enable_count;
                }
	        hdmi_cec_dev->standby_status = 1;
        }

        return 0;
}

int hdmi_cec_resume(struct device *dev)
{
        int clk_enable_count;
	struct cec_device *hdmi_cec_dev = (struct cec_device *)dev_get_drvdata(dev);
	printk("### %s \n", __func__);
        if(hdmi_cec_dev != NULL) {
	        hdmi_cec_dev->standby_status = 0;
                if(hdmi_cec_dev->cec_wakeup_enable) {
                        /* Nothing To Do */
                        cec_check_wake_up_interrupt(hdmi_cec_dev);
                } else {
                        if(hdmi_cec_dev->clk_enable_count > 0) {
                                clk_enable_count = hdmi_cec_dev->clk_enable_count;
                                hdmi_cec_dev->clk_enable_count = 0;
                                cec_power_on(hdmi_cec_dev);
                                hdmi_cec_dev->clk_enable_count = clk_enable_count;
                        }
                }
        }
	return 0;
}

int hdmi_cec_runtime_suspend(struct device *dev)
{
	struct cec_device *hdmi_cec_dev = (struct cec_device *)dev_get_drvdata(dev);
	printk("### %s \n", __func__);
        if(hdmi_cec_dev != NULL) {
	        hdmi_cec_dev->standby_status = 1;
        }
	printk("### %s: finish \n", __func__);

	return 0;
}

int hdmi_cec_runtime_resume(struct device *dev)
{
	struct cec_device *hdmi_cec_dev = (struct cec_device *)dev_get_drvdata(dev);

	printk("### %s \n", __func__);
        if(hdmi_cec_dev != NULL) {
	        hdmi_cec_dev->standby_status = 0;
        }
	printk("### %s: finish \n", __func__);

	return 0;
}

static const struct dev_pm_ops hdmi_cec_pm_ops = {
        .suspend = hdmi_cec_suspend,
        .resume = hdmi_cec_resume,
        .runtime_suspend = hdmi_cec_runtime_suspend,
        .runtime_resume = hdmi_cec_runtime_resume,
};
#endif // CONFIG_PM

/**
 * @short Platform driver structure
 */
static struct platform_driver __refdata dwc_hdmi_cec_pdrv = {
        .remove = hdmi_cec_remove,
        .probe = hdmi_cec_probe,
        .driver = {
                .name = "telechips,dw-hdmi-cec",
                .owner = THIS_MODULE,
                .of_match_table = dw_hdmi_cec,
                #if defined(CONFIG_PM)
                .pm = &hdmi_cec_pm_ops,
                #endif
        },
};

static __init int dwc_hdmi_cec_init(void)
{
        printk("%s \n", __FUNCTION__);
        return platform_driver_register(&dwc_hdmi_cec_pdrv);
}

static __exit void dwc_hdmi_cec_exit(void)
{
        printk("%s \n", __FUNCTION__);
        platform_driver_unregister(&dwc_hdmi_cec_pdrv);
}

module_init(dwc_hdmi_cec_init);
module_exit(dwc_hdmi_cec_exit);

/** @short License information */
MODULE_LICENSE("GPL v2");
/** @short Author information */
MODULE_AUTHOR("Telechips");
/** @short Device description */
MODULE_DESCRIPTION("HDMI_CEC module driver");
/** @short Device version */
MODULE_VERSION("1.0");

