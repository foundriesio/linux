// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
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

/*****************************************************************************
 * Defines
 ******************************************************************************/
#define DRV_NAME "hwdmx"

#ifdef CONFIG_iDxB_MAXLINEAR_MXL532C
#define FE_DEV_COUNT 2
#else
#define FE_DEV_COUNT 1
#endif

#define TSIF_DEV_COUNT 1	// the number of tsif/hwdmx
#define DMX_DEV_COUNT 2		// the number of linuxtv demux

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
static int tcc_hwdmx_init(struct tcc_hwdmx_inst_t *inst, struct device *dev)
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
		pr_err(
		"[ERROR][HWDMX] tcc_tsif_init failed\n");
		result = -EFAULT;
		goto tcc_tsif_init_fail;
	}

	inst->dmx.dev_num = DMX_DEV_COUNT;
	inst->dmx.adapter = &inst->adapter;
	if (tcc_dmx_init(&inst->dmx) != 0) {
		pr_err(
		"[ERROR][HWDMX] tcc_dmx_init failed\n");
		result = -EFAULT;
		goto tcc_dmx_init_fail;
	}

	pr_info("[INFO][HWDMX] %s\n", __func__);
	return 0;

tcc_dmx_init_fail:
	tcc_tsif_deinit(&inst->tsif);

tcc_tsif_init_fail:
	tcc_fe_deinit(&inst->fe);

	return result;
}

static void tcc_hwdmx_deinit(struct tcc_hwdmx_inst_t *inst)
{
	if (!IS_ERR(inst->tsif_clk))
		clk_disable(inst->tsif_clk);

	tcc_dmx_deinit(&inst->dmx);

	tcc_tsif_deinit(&inst->tsif);

	tcc_fe_deinit(&inst->fe);

	pr_info("[INFO][HWDMX] %s\n", __func__);
}

/*****************************************************************************
 * hwdmx_drv Prove/Remove
 ******************************************************************************/
static int hwdmx_drv_probe(struct platform_device *pdev)
{
	struct tcc_hwdmx_inst_t *inst;
	int result = 0;

	inst = kzalloc(sizeof(struct tcc_hwdmx_inst_t), GFP_KERNEL);
	if (inst == NULL)
		return -ENOMEM;

	if (dvb_register_adapter
	    (&inst->adapter, pdev->name, THIS_MODULE, &pdev->dev,
	     adapter_nr) < 0) {
		pr_err(
		"[ERROR][HWDMX] (Failed to register dvb adapter)\n");
		result = -ENOMEM;
		goto dvb_register_adapter_fail;
	}

	if (tcc_hwdmx_init(inst, &pdev->dev) != 0) {
		pr_err(
		"[ERROR][HWDMX] tcc_hwdmx_init failed\n");
		result = -EFAULT;
		goto tcc_hwdmx_init_fail;
	}

	inst->tsif_clk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(inst->tsif_clk)) {
		pr_err(
		"[ERROR][HWDMX] TS RX clock not found.\n");
		return PTR_ERR(inst->tsif_clk);
	}

	clk_prepare_enable(inst->tsif_clk);
	clk_set_rate(inst->tsif_clk, 27000000);

	dev_set_drvdata(&pdev->dev, inst);
	pr_info("[INFO][HWDMX] %s\n", __func__);

	return 0;

tcc_hwdmx_init_fail:
	dvb_unregister_adapter(&inst->adapter);

dvb_register_adapter_fail:
	kfree(inst);

	return result;
}

static int hwdmx_drv_remove(struct platform_device *pdev)
{
	struct tcc_hwdmx_inst_t *inst;

	inst = dev_get_drvdata(&pdev->dev);
	if (inst == NULL)
		return 0;

	tcc_hwdmx_deinit(inst);

	dvb_unregister_adapter(&inst->adapter);

	kfree(inst);

	pr_info("[INFO][HWDMX] %s\n", __func__);

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
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = hwdmx_drv_of_match,
#endif
		   },
};

static int __init hwdmx_drv_init(void)
{
	pr_info("[INFO][HWDMX] %s\n", __func__);

	platform_driver_register(&hwdmx_drv);

	return 0;
}

static void __exit hwdmx_drv_exit(void)
{
	platform_driver_unregister(&hwdmx_drv);

	pr_info("[INFO][HWDMX] %s\n", __func__);
}

module_init(hwdmx_drv_init);
module_exit(hwdmx_drv_exit);

MODULE_DESCRIPTION("Hardware Demux Driver");
MODULE_AUTHOR("Telechips");
MODULE_LICENSE("GPL");
