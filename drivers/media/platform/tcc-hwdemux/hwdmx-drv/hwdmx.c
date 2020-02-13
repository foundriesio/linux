/*
 *  hwdmx.c
 *
 *  Written by C2-G1-3T
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.=
 */

#include <linux/platform_device.h>

#include <dvb_frontend.h>
#include <dvb_demux.h>
#include <dmxdev.h>
#include <linux/clk.h>

#include "frontend.h"
#include "demux.h"
#include "tsif.h"

#include "hwdmx.h"

/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG "[HWDMX_DRV]"

static int dev_debug = 1;

module_param(dev_debug, int, 0644);
MODULE_PARM_DESC(dev_debug, "Turn on/off device debugging (default:off).");

#define dprintk(msg...)                          \
	{                                            \
		if (dev_debug)                           \
			printk(KERN_INFO LOG_TAG "(D)" msg); \
	}

#define eprintk(msg...)                        \
	{                                          \
		printk(KERN_INFO LOG_TAG " (E) " msg); \
	}

/*****************************************************************************
 * Defines
 ******************************************************************************/
#define DRV_NAME "hwdmx"

#ifdef CONFIG_iDxB_MAXLINEAR_MXL532C
#define FE_DEV_COUNT 2
#else
#define FE_DEV_COUNT 1
#endif

#define TSIF_DEV_COUNT 1 // the number of tsif/hwdmx
#define DMX_DEV_COUNT 2  // the number of linuxtv demux

/*****************************************************************************
 * structures
 ******************************************************************************/

/*****************************************************************************
 * Variables
 ******************************************************************************/
DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

/*****************************************************************************
 * External Functions
 ******************************************************************************/

/*****************************************************************************
 * Functions
 ******************************************************************************/

/*****************************************************************************
 * hwdmx_drv Init/Deinit
 ******************************************************************************/
static int tcc_hwdmx_init(tcc_hwdmx_inst_t *inst, struct device *dev)
{
	int result = 0;

	inst->fe.dev_num = FE_DEV_COUNT;
	inst->fe.adapter = &inst->adapter;
	inst->fe.dev = dev;
	tcc_fe_init(&inst->fe);

	inst->tsif.dev_num = TSIF_DEV_COUNT;
	inst->tsif.adapter = &inst->adapter;
	inst->tsif.dev = dev;
	if (tcc_tsif_init(&inst->tsif) != 0) {
		eprintk("[ERROR][HWDMX] %s:%d tcc_tsif_init failed\n", __FUNCTION__, __LINE__);
		result = -EFAULT;
		goto tcc_tsif_init_fail;
	}

	inst->dmx.dev_num = DMX_DEV_COUNT;
	inst->dmx.adapter = &inst->adapter;
	if (tcc_dmx_init(&inst->dmx) != 0) {
		eprintk("[ERROR][HWDMX] %s:%d tcc_dmx_init failed\n", __FUNCTION__, __LINE__);
		result = -EFAULT;
		goto tcc_dmx_init_fail;
	}

	dprintk("[DEBUG][HWDMX] %s\n", __FUNCTION__);
	return 0;

tcc_dmx_init_fail:
	tcc_tsif_deinit(&inst->tsif);

tcc_tsif_init_fail:
	tcc_fe_deinit(&inst->fe);

	return result;
}

static void tcc_hwdmx_deinit(tcc_hwdmx_inst_t *inst)
{
	if (!IS_ERR(inst->tsif_clk)) {
		clk_disable(inst->tsif_clk);
	}

	tcc_dmx_deinit(&inst->dmx);

	tcc_tsif_deinit(&inst->tsif);

	tcc_fe_deinit(&inst->fe);

	dprintk("[DEBUG][HWDMX] %s\n", __FUNCTION__);
}

/*****************************************************************************
 * hwdmx_drv Prove/Remove
 ******************************************************************************/
static int hwdmx_drv_probe(struct platform_device *pdev)
{
	tcc_hwdmx_inst_t *inst;
	int result = 0;

	inst = kzalloc(sizeof(tcc_hwdmx_inst_t), GFP_KERNEL);
	if (inst == NULL) {
		eprintk("[ERROR][HWDMX] %s(kzalloc fail)\n", __FUNCTION__);
		return -ENOMEM;
	}

	if (dvb_register_adapter(&inst->adapter, pdev->name, THIS_MODULE, &pdev->dev, adapter_nr) < 0) {
		eprintk("[ERROR][HWDMX] %s(Failed to register dvb adapter)\n", __FUNCTION__);
		eprintk("[ERROR][HWDMX] %s:%d dvb_register_adapter failed\n", __FUNCTION__, __LINE__);
		result = -ENOMEM;
		goto dvb_register_adapter_fail;
	}

	if (tcc_hwdmx_init(inst, &pdev->dev) != 0) {
		eprintk("[ERROR][HWDMX] %s:%d tcc_hwdmx_init failed\n", __FUNCTION__, __LINE__);
		result = -EFAULT;
		goto tcc_hwdmx_init_fail;
	}

	inst->tsif_clk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(inst->tsif_clk)) {
		dev_err(&pdev->dev, "[ERROR][HWDMX] TS RX clock not found.\n");
		return PTR_ERR(inst->tsif_clk);
	}

	clk_prepare_enable(inst->tsif_clk);
	clk_set_rate(inst->tsif_clk, 27000000);

	dev_set_drvdata(&pdev->dev, inst);
	printk("%s\n", __FUNCTION__);

	return 0;

tcc_hwdmx_init_fail:
	dvb_unregister_adapter(&inst->adapter);

dvb_register_adapter_fail:
	kfree(inst);

	return result;
}

static int hwdmx_drv_remove(struct platform_device *pdev)
{
	tcc_hwdmx_inst_t *inst;

	inst = dev_get_drvdata(&pdev->dev);
	if (inst == NULL) {
		return 0;
	}

	tcc_hwdmx_deinit(inst);

	dvb_unregister_adapter(&inst->adapter);

	kfree(inst);

	dprintk("[DEBUG][HWDMX] %s\n", __FUNCTION__);

	return 0;
}

/*****************************************************************************
 * hwdmx drv Module Init/Exit
 ******************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id hwdmx_drv_of_match[] = {
	{
		.compatible = "telechips,tcc89xx-hwdmx-tsif",
	},
	{
		.compatible = "telechips,tcc90xx-hwdmx-tsif",
	},
	{},
};
MODULE_DEVICE_TABLE(of, hwdmx_drv_of_match);
#endif

static struct platform_driver hwdmx_drv = {
	.probe = hwdmx_drv_probe,
	.remove = hwdmx_drv_remove,
	.driver =
		{
			.name = DRV_NAME,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = hwdmx_drv_of_match,
#endif
		},
};

static int __init hwdmx_drv_init(void)
{
	dprintk("[DEBUG][HWDMX] %s\n", __FUNCTION__);

	platform_driver_register(&hwdmx_drv);

	return 0;
}

static void __exit hwdmx_drv_exit(void)
{
	platform_driver_unregister(&hwdmx_drv);

	dprintk("[DEBUG][HWDMX] %s\n", __FUNCTION__);
}

module_init(hwdmx_drv_init);
module_exit(hwdmx_drv_exit);

MODULE_DESCRIPTION("Hardware Demux Driver");
MODULE_AUTHOR("Telechips");
MODULE_LICENSE("GPL");
