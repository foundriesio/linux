// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <dvb_frontend.h>

#include "frontend.h"

/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG "[TCC_FE]"

static int fe_debug;

module_param(fe_debug, int, 0644);
MODULE_PARM_DESC(fe_debug, "Turn on/off frontend debugging (default:off).");

/*****************************************************************************
 * Defines
 ******************************************************************************/
#if CONFIG_DVB_MAX_ADAPTERS
#define MAX_INST CONFIG_DVB_MAX_ADAPTERS
#else
#define MAX_INST 8
#endif

/*****************************************************************************
 * Structures
 ******************************************************************************/

/*****************************************************************************
 * Variables
 ******************************************************************************/
static struct tcc_fe_inst_t *gInst[] = {[0 ... (MAX_INST - 1)] = NULL };

/*****************************************************************************
 * External Functions
 ******************************************************************************/

/*****************************************************************************
 * Functions
 ******************************************************************************/

/*****************************************************************************
 * TCC FE Register/Unregister
 ******************************************************************************/
int tcc_fe_register(struct tcc_dxb_fe_driver *pdrv)
{
	struct tcc_fe_inst_t *inst;
	struct tcc_fe_priv_t *fe;
	int i, j, ret = -1;
	struct device_node *node;

	pdrv->fe = kcalloc(
	MAX_INST,
	MAX_INST * sizeof(struct tcc_fe_priv_t *),
	GFP_KERNEL);

	for (j = 0; j < MAX_INST; j++) {
		inst = gInst[j];
		if (inst == NULL)
			continue;

		node =
		    of_find_compatible_node(inst->dev->of_node, NULL,
					    pdrv->compatible);
		if (node == NULL)
			continue;

		pr_info(
		"[%s] inst->dev_num(%d) inst[%d]\n",
		     __func__, inst->dev_num, j);

		for (i = 0; i < inst->dev_num; i++) {
			fe = &inst->fe[i];
			if (fe->isUsing == 0) {
				fe->fe.id = i;
				fe->fe.demodulator_priv = NULL;
				fe->of_node = node;
				memcpy(&fe->fe.ops, pdrv->fe_ops,
				       sizeof(struct dvb_frontend_ops));
				if (dvb_register_frontend
				    (inst->adapter, &fe->fe) == 0) {
					if (pdrv->probe(fe) == 0) {
						pdrv->fe[j] = fe;
						fe->isUsing = 1;
						ret = 0;
					} else {
						pr_err(
						"[%d/%d]probe error!\n",
						     i, inst->dev_num);
						dvb_unregister_frontend
							(&fe->fe);
						dvb_frontend_detach(&fe->fe);
						break;
					}
				} else {
					pr_err(
					"[%d/%d]registration failed!\n",
					     i, inst->dev_num);
					dvb_frontend_detach(&fe->fe);
					break;
				}
			}
		}
	}

	if (ret != 0)
		kfree(pdrv->fe);

	return ret;
}
EXPORT_SYMBOL(tcc_fe_register);

int tcc_fe_unregister(struct tcc_dxb_fe_driver *pdrv)
{
	struct tcc_fe_priv_t *fe;
	int i;

	for (i = 0; i < MAX_INST; i++) {
		fe = pdrv->fe[i];
		if (fe && fe->isUsing != 0) {
			dvb_unregister_frontend(&fe->fe);
			dvb_frontend_detach(&fe->fe);
			fe->isUsing = 0;
		}
	}

	kfree(pdrv->fe);

	return 0;
}
EXPORT_SYMBOL(tcc_fe_unregister);

/*****************************************************************************
 * TCC FE Init/Deinit
 ******************************************************************************/
int tcc_fe_init(struct tcc_fe_inst_t *inst)
{
	inst->fe = kcalloc
		(inst->dev_num,
		inst->dev_num * sizeof(struct tcc_fe_priv_t), GFP_KERNEL);

	gInst[inst->adapter->num] = inst;

	return 0;
}

int tcc_fe_deinit(struct tcc_fe_inst_t *inst)
{
	gInst[inst->adapter->num] = NULL;

	kfree(inst->fe);

	return 0;
}
