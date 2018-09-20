/*
 * Copyright (c) 2015-2016, Linaro Limited
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/tee_drv.h>
#include <linux/vmalloc.h>

static void uuid_to_octets(uint8_t *d, const struct tee_client_uuid *s)
{
    d[0] = s->time_low >> 24;
    d[1] = s->time_low >> 16;
    d[2] = s->time_low >> 8;
    d[3] = s->time_low;
    d[4] = s->time_mid >> 8;
    d[5] = s->time_mid;
    d[6] = s->time_hi_and_version >> 8;
    d[7] = s->time_hi_and_version;
    memcpy(d + 8, s->clock_seq_and_node, sizeof(s->clock_seq_and_node));
}

static bool tee_client_is_buf_param(uint32_t type)
{
    switch (type) {
        case TEE_CLIENT_PARAM_BUF_IN :
        case TEE_CLIENT_PARAM_BUF_OUT :
        case TEE_CLIENT_PARAM_BUF_INOUT :
            return true;
        default :
            return false;
    }
}

static int tee_client_match(struct tee_ioctl_version_data *data,
							const void *vers)
{
	return !!1;
}

static int tee_client_set_params(struct tee_context * context,
							 	 struct tee_client_params * params,
								 struct tee_param * tee_params,
								 uint32_t *num_params)
{
    int i;
	int param_nums = 0;
	int res = -EINVAL;

	if (!context || !params || !tee_params || !num_params)
		return res;

    for (i = 0; i < TEE_CLIENT_PARAM_NUM; i++) {
        if (params->params[i].type == TEE_CLIENT_PARAM_NONE) {
			tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
            continue;
        }

        switch (params->params[i].type) {
            case TEE_CLIENT_PARAM_BUF_IN :
				tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
				param_nums++;
                break;
            case TEE_CLIENT_PARAM_BUF_OUT :
				tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
				param_nums++;
                break;
            case TEE_CLIENT_PARAM_BUF_INOUT :
				tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
				param_nums++;
                break;
            case TEE_CLIENT_PARAM_VALUE_IN :
				tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
				param_nums++;
                break;
            case TEE_CLIENT_PARAM_VALUE_OUT :
				tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;
				param_nums++;
                break;
            case TEE_CLIENT_PARAM_VALUE_INOUT :
				tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT;
				param_nums++;
                break;
            default :
				tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
                break;
        }

		if (tee_client_is_buf_param(params->params[i].type)) {
			struct tee_shm *shm;
			uint8_t *buf;
			long buf_size;

			buf_size = params->params[i].tee_client_memref.size;
			shm = tee_shm_alloc(context,
								buf_size,
								TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);
			if (IS_ERR(shm))
				goto FREE_EXIT;

			buf = tee_shm_get_va(shm, 0);
			if (IS_ERR(buf)) {
				tee_shm_free(shm);
				goto FREE_EXIT;
			}

			memcpy(buf,
				   params->params[i].tee_client_memref.buffer,
				   buf_size);

			tee_params[i].u.memref.shm = shm;
			tee_params[i].u.memref.size = buf_size;
        } else {
            tee_params[i].u.value.a = params->params[i].tee_client_value.a;
            tee_params[i].u.value.b = params->params[i].tee_client_value.b;
        }
    }

	*num_params = param_nums;

	res = 0;

    return res;
FREE_EXIT:
    for (i = 0; i < TEE_CLIENT_PARAM_NUM; i++) {
        switch (params->params[i].type) {
            case TEE_CLIENT_PARAM_BUF_IN :
            case TEE_CLIENT_PARAM_BUF_OUT :
            case TEE_CLIENT_PARAM_BUF_INOUT :
                if (tee_params[i].u.memref.shm) {
                    tee_shm_free(tee_params[i].u.memref.shm);
                }
                break;
            default :
                break;
        }
    }

    return res;
}

static int tee_client_get_params(struct tee_client_params *params,
							 	 struct tee_param *tee_params)
{
    int i;
	uint8_t *buf;

	if (!params || !tee_params)
		return -1;

    for (i = 0; i < TEE_CLIENT_PARAM_NUM; i++) {
		if (tee_params[i].attr == TEE_IOCTL_PARAM_ATTR_TYPE_NONE)
			continue;

		switch (tee_params[i].attr) {
			case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT :
			case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT :
				buf = tee_shm_get_va(tee_params[i].u.memref.shm, 0);
				memcpy(params->params[i].tee_client_memref.buffer, buf,
					   tee_params[i].u.memref.size);
				params->params[i].tee_client_memref.size = tee_params[i].u.memref.size;
			case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT :
				tee_shm_free(tee_params[i].u.memref.shm);
				break;
			case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT :
			case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT :
				params->params[i].tee_client_value.a = tee_params[i].u.value.a;
				params->params[i].tee_client_value.b = tee_params[i].u.value.b;
				break;
			default :
				break;
		}
	}

    return 0;
}

int tee_client_open_ta(struct tee_client_uuid *uuid,
					   struct tee_client_params *params,
					   tee_client_context *context)
{
	struct tee_ioctl_open_session_arg arg;
	struct tee_client_context *ta_context;
	struct tee_ioctl_version_data vers = {
		.impl_id = TEE_OPTEE_CAP_TZ,
		.impl_caps = TEE_IMPL_ID_OPTEE,
		.gen_caps = TEE_GEN_CAP_GP,
	};
	struct tee_param tee_params[4];
	void * params_ptr = NULL;
	uint32_t params_nums;
	int rc;

	if (!uuid)
		return -EINVAL;

	if (!context)
		return -EINVAL;

	ta_context = vmalloc(sizeof(struct tee_client_context));
	if (!ta_context)
		return -ENOMEM;

	memset(ta_context, 0, sizeof(struct tee_client_context));

	ta_context->ctx = tee_client_open_context(NULL, tee_client_match,
											  NULL, &vers);
	if (IS_ERR(ta_context->ctx)) {
		vfree(ta_context);
		return -EINVAL;
	}

	memset(&arg, 0, sizeof(arg));
	uuid_to_octets((uint8_t *)&arg.uuid, uuid);
	arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;

	arg.num_params = 0;
	if (params) {
		memset(tee_params, 0, sizeof(tee_params));
		if (tee_client_set_params(ta_context->ctx,
								  params, tee_params,
								  &params_nums)) {
			vfree(ta_context);
			return -EINVAL;
		}

		arg.num_params = params_nums;
		params_ptr = tee_params;
	}

	rc = tee_client_open_session(ta_context->ctx, &arg, params_ptr);
	if (!rc && arg.ret) {
		vfree(ta_context);
		return arg.ret;
	}

	if (params)
		tee_client_get_params(params, tee_params);

	ta_context->session = arg.session;
	ta_context->session_initalized = true;

	*context = ta_context;

	return arg.ret;
}
EXPORT_SYMBOL_GPL(tee_client_open_ta);

int tee_client_execute_command(tee_client_context context,
						   	   struct tee_client_params *params,
							   int command)
{
	struct tee_ioctl_invoke_arg arg;
	struct tee_param tee_params[4];
	void *params_ptr = NULL;
	int params_nums;
	int rc;

	if (!context)
		return -EINVAL;

	if (!context->session_initalized)
		return -EINVAL;

	memset(&arg, 0, sizeof(arg));

	arg.num_params = 0;
	if (params) {
		memset(tee_params, 0, sizeof(tee_params));
		if (tee_client_set_params(context->ctx,
								  params, tee_params,
								  &params_nums)) {
			return -EINVAL;
		}

		arg.num_params = params_nums;
		params_ptr = tee_params;
	}

	arg.func = command;
	arg.session = context->session;

	rc = tee_client_invoke_func(context->ctx, &arg, params_ptr);
	if (rc)
		return arg.ret;

	if (params)
		tee_client_get_params(params, tee_params);

	return arg.ret;
}
EXPORT_SYMBOL_GPL(tee_client_execute_command);

void tee_client_close_ta(tee_client_context context)
{
	if (!context)
		return;

	if (!context->session_initalized)
		return;

	tee_client_close_session(context->ctx, context->session);
	tee_client_close_context(context->ctx);

	vfree(context);

	return;
}
EXPORT_SYMBOL_GPL(tee_client_close_ta);

