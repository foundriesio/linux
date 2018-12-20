/*
 *   FileName    : smp_driver.c
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
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
