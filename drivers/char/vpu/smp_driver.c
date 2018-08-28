/*
 *   FileName    : smp_driver.c
 *   Description : vpu CA driver for OPTEE
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved
 *
 * This source code contains confidential information of Telechips.
 * Any unauthorized use without a written permission of Telechips including
 * not limited to re-distribution in source or binary form is strictly prohibited.
 * This source code is provided "AS IS" and nothing contained in this source code
 * shall constitute any express or implied warranty of any kind,
 * including without limitation, any warranty of merchantability,
 * fitness for a particular purpose or non-infringement of any patent,
 * copyright or other third party intellectual property right.
 * No warranty is made, express or implied, regarding the informations accuracy,
 * completeness, or performance.
 * In no event shall Telechips be liable for any claim, damages or other liability
 * arising from, out of or in connection with this source code or the use
 * in the source code.
 * This source code is provided subject to the terms of a Mutual Non-Disclosure
 * Agreement between Telechips and Company.
 *
 */

#include "smp_driver.h"
#include "vpu_comm.h"

static tee_client_context context = NULL;
static int vpu_tee_client_count = 0;

int vpu_optee_open(void) {
    if (context == NULL) {
        struct tee_client_uuid uuid = TA_VPU_UUID;
        int rc = tee_client_open_ta(&uuid, NULL, &context);
        if (rc) {
            printk("[%s] tee_client_open_ta: error = %d (0x%x)\n", __func__, rc, rc);
            return rc;
        }
        printk("[%s] tee_client_open_ta \n", __func__);
    }
    vpu_tee_client_count++;
    printk("[%s] tee client count [%d] \n", __func__, vpu_tee_client_count);
    return 0;
}

int vpu_optee_fw_load(int type) {
    int rc;

    struct tee_client_params params;
    memset(&params, 0, sizeof(params));
    params.params[0].tee_client_value.a = type;
    params.params[0].type = TEE_CLIENT_PARAM_VALUE_IN;

    rc = tee_client_execute_command(context, &params, CMD_CA_FW_LOADING);
    if (rc) {
        printk("[%s] CMD_CA_FW_LOADING error !!\n", __func__);
        tee_client_close_ta(context);
        vpu_tee_client_count = 0;
        context = NULL;
        return -1;
    }
    return 0;
}

int vpu_optee_fw_read(int type) {
    int rc;

    struct tee_client_params params;
    memset(&params, 0, sizeof(params));
    params.params[0].tee_client_value.a = type;
    params.params[0].type = TEE_CLIENT_PARAM_VALUE_IN;

    rc = tee_client_execute_command(context, &params, CMD_CA_FW_READING);
    if (rc) {
        printk("[%s] CMD_CA_FW_READING error !!\n", __func__);
        tee_client_close_ta(context);
        vpu_tee_client_count = 0;
        context = NULL;
        return -1;
    }
    return 0;
}

int vpu_optee_close(void) {
    vpu_tee_client_count--;
    printk("[%s] tee client count [%d]\n", __func__, vpu_tee_client_count);

    if (vpu_tee_client_count <= 0) {
        tee_client_close_ta(context);
        vpu_tee_client_count = 0;
        context = NULL;
        printk("[%s] tee_client_close_ta\n", __func__);
    }
    return 0;
}
