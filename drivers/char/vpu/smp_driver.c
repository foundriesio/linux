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

#define dprintk(msg...) //printk( "SMP_DRIVER: " msg);
#define detailk(msg...) //printk( "SMP_DRIVER: " msg);
#define cmdk(msg...) //printk( "SMP_DRIVER [Cmd]: " msg);
#define err(msg...) printk("SMP_DRIVER [Err]: "msg);

static tee_client_context context = NULL;
static int vpu_tee_client_count = 0;

int vpu_optee_open(void) {
    if (context == NULL) {
        struct tee_client_uuid uuid = TA_VPU_UUID;
        int rc = tee_client_open_ta(&uuid, NULL, &context);
        if (rc) {
            dprintk("[%s] tee_client_open_ta: error = %d (0x%x), Check uboot and sest img\n", __func__, rc, rc);
            return rc;
        }
        dprintk("[%s] tee_client_open_ta \n", __func__);
    }
    vpu_tee_client_count++;
    dprintk("[%s] tee client count [%d] \n", __func__, vpu_tee_client_count);
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
    dprintk("[%s] tee client count [%d]\n", __func__, vpu_tee_client_count);

    if (vpu_tee_client_count <= 0) {
        tee_client_close_ta(context);
        vpu_tee_client_count = 0;
        context = NULL;
        dprintk("[%s] tee_client_close_ta\n", __func__);
    }
    return 0;
}
// for JPU
static tee_client_context jpu_context = NULL;
static int jpu_tee_client_count = 0;
int jpu_optee_open(void) {
    if (jpu_context == NULL) {
        struct tee_client_uuid uuid = TA_JPU_UUID;
        int rc = tee_client_open_ta(&uuid, NULL, &jpu_context);
        if (rc) {
            printk("[%s] tee_client_open_ta: error = %d (0x%x)\n", __func__, rc, rc);
            return rc;
        }
        printk("[%s] tee_client_open_ta \n", __func__);
    }
    jpu_tee_client_count++;
    printk("[%s] tee client count [%d] \n", __func__, jpu_tee_client_count);
    return 0;
}

int jpu_optee_close(void) {
    jpu_tee_client_count--;
    printk("[%s] tee client count [%d]\n", __func__, jpu_tee_client_count);

    if (jpu_tee_client_count <= 0) {
        tee_client_close_ta(jpu_context);
        jpu_tee_client_count = 0;
        jpu_context = NULL;
        printk("[%s] tee_client_close_ta\n", __func__);
    }
    return 0;
}

int jpu_optee_command(int Op, void *pstInst, long lInstSize) {

    int rc;
    int command;
    struct tee_client_params params;

    memset(&params, 0, sizeof(params));


    dprintk("[KERNEL:%s] -----------------------------------------------------\n", __func__);
    dprintk("[KERNEL:%s] -----------------------------------------------------\n", __func__);
    switch (Op) {
    case JPU_DEC_INIT:
        {
            dprintk("[KERNEL:%s]        CMD_CA_JPU_DEC_INIT\n", __func__);
            command = CMD_CA_JPU_DEC_INIT;
            params.params[0].tee_client_memref.buffer = (long)pstInst;
            params.params[0].tee_client_memref.size = lInstSize;
            params.params[0].type = TEE_CLIENT_PARAM_BUF_INOUT;
        }
        break;
#if defined(JPU_C6)
    case JPU_DEC_SEQ_HEADER:
        {
            command = CMD_CA_JPU_DEC_SEQ_HEADER;

            dprintk("[KERNEL:%s]        CMD_CA_JPU_DEC_SEQ_HEADER(0x%x)\n", __func__, command);
            params.params[0].tee_client_memref.buffer = (long)pstInst;
            params.params[0].tee_client_memref.size = lInstSize;
            params.params[0].type = TEE_CLIENT_PARAM_BUF_INOUT;
        }
        break;
    case JPU_DEC_REG_FRAME_BUFFER:
        {
            command = CMD_CA_JPU_DEC_REG_FRAME_BUFFER;

            dprintk("[KERNEL:%s]        CMD_CA_JPU_DEC_REG_FRAME_BUFFER(0x%x)\n", __func__, command);
            params.params[0].tee_client_memref.buffer = (long)pstInst;
            params.params[0].tee_client_memref.size = lInstSize;
            params.params[0].type = TEE_CLIENT_PARAM_BUF_INOUT;
        }
        break;
#endif
    case JPU_DEC_DECODE:
        {
            command = CMD_CA_JPU_DEC_DECODE;

            dprintk("[KERNEL:%s]        CMD_CA_JPU_DEC_DECODE 001(0x%x)\n", __func__, command);
            params.params[0].tee_client_memref.buffer = (long)pstInst;
            params.params[0].tee_client_memref.size = lInstSize;
            params.params[0].type = TEE_CLIENT_PARAM_BUF_INOUT;
        }
        break;
    case JPU_DEC_GET_ROI_INFO:
        {
            command = CMD_CA_JPU_DEC_GET_ROI_INFO;

            dprintk("[KERNEL:%s]        CMD_CA_JPU_DEC_GET_ROI_INFO(0x%x)\n", __func__, command);
            params.params[0].tee_client_memref.buffer = (long)pstInst;
            params.params[0].tee_client_memref.size = lInstSize;
            params.params[0].type = TEE_CLIENT_PARAM_BUF_INOUT;
        }
        break;
    case JPU_CODEC_GET_VERSION:
        {
            dprintk("[KERNEL:%s]         CMD_CA_JPU_CODEC_GET_VERSION\n", __func__);
            command = CMD_CA_JPU_CODEC_GET_VERSION;
            params.params[0].tee_client_memref.buffer = (long)pstInst;
            params.params[0].tee_client_memref.size = lInstSize;
            params.params[0].type = TEE_CLIENT_PARAM_BUF_INOUT;
        }
        break;
    case JPU_DEC_CLOSE:
    default:
        {
            command = CMD_CA_JPU_DEC_CLOSE;

            dprintk("[KERNEL:%s]        CMD_CA_JPU_DEC_CLOSE(0x%x)\n", __func__, command);
            params.params[0].tee_client_memref.buffer = (long)pstInst;
            params.params[0].tee_client_memref.size = lInstSize;
            params.params[0].type = TEE_CLIENT_PARAM_BUF_INOUT;
        }
        break;
    }
    dprintk("[KERNEL:%s] -----------------------------------------------------\n", __func__);
    dprintk("[KERNEL:%s] -----------------------------------------------------\n", __func__);

    rc = tee_client_execute_command(jpu_context, &params, command);
    if (rc) {
        err("[KERNEL:%s] #####################################################\n", __func__);
        err("[KERNEL:%s] #####################################################\n", __func__);
        err("[KERNEL:%s]        commandID [%d(%x)] Error: %d(%x) !!\n", __func__, command, command, rc, rc);
        err("[KERNEL:%s] #####################################################\n", __func__);
        err("[KERNEL:%s] #####################################################\n", __func__);
        tee_client_close_ta(jpu_context);
        jpu_tee_client_count = 0;
        jpu_context = NULL;
        return -1;
    }


    switch (Op) {
    case JPU_DEC_INIT:
        break;
    case JPU_DEC_SEQ_HEADER:
        break;
    case JPU_DEC_REG_FRAME_BUFFER:
        break;
    case JPU_DEC_DECODE:
        break;
    case JPU_DEC_CLOSE:
        break;
    case JPU_DEC_GET_ROI_INFO:
        break;
    case JPU_CODEC_GET_VERSION:
        break;
    default:
        break;
    }

    dprintk("[KERNEL:%s] -----------------------------------------------------\n", __func__);
    dprintk("[KERNEL:%s] -----------------------------------------------------\n", __func__);
    dprintk("[KERNEL:%s]       commandID 0x%x rc %d", __func__, command, rc);
    dprintk("[KERNEL:%s] -----------------------------------------------------\n", __func__);
    dprintk("[KERNEL:%s] -----------------------------------------------------\n", __func__);
    return 0;
}
EXPORT_SYMBOL(jpu_optee_command);
