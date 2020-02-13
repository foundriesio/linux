/*
 * Copyright (c) 2018-2020, Telechips Inc
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

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "optee/optee_private.h"
#include "optee/optee_smc.h"

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
	case TEE_CLIENT_PARAM_BUF_IN:
	case TEE_CLIENT_PARAM_BUF_OUT:
	case TEE_CLIENT_PARAM_BUF_INOUT:
		return true;
	default:
		return false;
	}
}

static bool tee_client_is_sec_buf_param(uint32_t type)
{
	switch (type) {
	case TEE_CLIENT_PARAM_SEC_BUF_IN:
	case TEE_CLIENT_PARAM_SEC_BUF_OUT:
	case TEE_CLIENT_PARAM_SEC_BUF_INOUT:
		return true;
	default:
		return false;
	}
}

static int tee_client_match(struct tee_ioctl_version_data *data, const void *vers)
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
			param_nums++;
			continue;
		}

		switch (params->params[i].type) {
		case TEE_CLIENT_PARAM_BUF_IN:
		case TEE_CLIENT_PARAM_SEC_BUF_IN:
			tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
			param_nums++;
			break;
		case TEE_CLIENT_PARAM_BUF_OUT:
		case TEE_CLIENT_PARAM_SEC_BUF_OUT:
			tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
			param_nums++;
			break;
		case TEE_CLIENT_PARAM_BUF_INOUT:
		case TEE_CLIENT_PARAM_SEC_BUF_INOUT:
			tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
			param_nums++;
			break;
		case TEE_CLIENT_PARAM_VALUE_IN:
			tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
			param_nums++;
			break;
		case TEE_CLIENT_PARAM_VALUE_OUT:
			tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;
			param_nums++;
			break;
		case TEE_CLIENT_PARAM_VALUE_INOUT:
			tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT;
			param_nums++;
			break;
		default:
			tee_params[i].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
			break;
		}

		if (tee_client_is_buf_param(params->params[i].type)) {
			struct tee_shm *shm;

			shm = tee_shm_register_for_kern(context,
					(unsigned long)params->params[i].tee_client_memref.buffer,
					params->params[i].tee_client_memref.size, 0);
			if (IS_ERR(shm)) {
				void * va;
				// Try to allocate shm
				shm = tee_shm_alloc(context,
						params->params[i].tee_client_memref.size,
						TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);
				if (IS_ERR(shm))
					goto FREE_EXIT;

				va = tee_shm_get_va(shm, 0);
				if (IS_ERR(va))
					goto FREE_EXIT;

				memcpy(va, params->params[i].tee_client_memref.buffer,
					   params->params[i].tee_client_memref.size);
			}

			tee_params[i].u.memref.shm = shm;
			tee_params[i].u.memref.size = params->params[i].tee_client_memref.size;
		} else if (tee_client_is_sec_buf_param(params->params[i].type)) {
			struct tee_shm *shm;

			shm = tee_client_shm_sdp_register(context,
					(unsigned long)params->params[i].tee_client_memref.buffer,
					params->params[i].tee_client_memref.size);
			if (IS_ERR(shm))
				goto FREE_EXIT;

			tee_params[i].u.memref.shm = shm;
			tee_params[i].u.memref.size = params->params[i].tee_client_memref.size;
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
		if (tee_client_is_buf_param(params->params[i].type) ||
				tee_client_is_sec_buf_param(params->params[i].type)) {
			if (tee_params[i].u.memref.shm)
				tee_shm_free(tee_params[i].u.memref.shm);
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
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT:
			buf = tee_shm_get_va(tee_params[i].u.memref.shm, 0);
			if (buf != ERR_PTR(-EINVAL)) {
				memcpy(params->params[i].tee_client_memref.buffer, buf,
						tee_params[i].u.memref.size);
			}
			params->params[i].tee_client_memref.size = tee_params[i].u.memref.size;
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT:
			tee_shm_free(tee_params[i].u.memref.shm);
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT:
			params->params[i].tee_client_value.a = tee_params[i].u.value.a;
			params->params[i].tee_client_value.b = tee_params[i].u.value.b;
			break;
		default:
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

/*
 * Version information at /proc/optee
 */
struct tee_version {
	char	name[6];
	struct tee_ioctl_version_tcc *ver;
};

/* 0:os, 1:supp, 2:teec lib, 3:ca lib */
static struct tee_version tee_proc_ver[] = {
	{"teeos", NULL},
	{"suppl", NULL},
	{"teec ", NULL},
	{"calib", NULL},
};

void tee_set_version(unsigned int cmd, struct tee_ioctl_version_tcc *ver)
{
	int idx;
	switch (cmd) {
	case TEE_IOC_SUPP_VERSION:
		idx = 1;
		break;
	case TEE_IOC_CLIENT_VERSION:
		idx = 2;
		break;
	case TEE_IOC_CALIB_VERSION:
		idx = 3;
		break;
	case TEE_IOC_OS_VERSION:
		idx = 0;
		break;
	default:
		return;
	}

	if (tee_proc_ver[idx].ver) {
		pr_info("[INFO][OPTEE] %s version already set\n", tee_proc_ver[idx].name);
		return;
	}

	tee_proc_ver[idx].ver = kmalloc(sizeof(struct tee_ioctl_version_tcc), GFP_KERNEL);
	if (tee_proc_ver[idx].ver)
		memcpy(tee_proc_ver[idx].ver, ver, sizeof(struct tee_ioctl_version_tcc));
}

static void *tee_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *tee_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void tee_stop(struct seq_file *m, void *v)
{
}

static int tee_show(struct seq_file *m, void *v)
{
	int idx;

	for (idx=0 ; idx < ARRAY_SIZE(tee_proc_ver) ; idx++) {
		if (tee_proc_ver[idx].ver) {
			seq_printf(m, "%s: v%d.%d.%d",
				tee_proc_ver[idx].name, tee_proc_ver[idx].ver->major,
				tee_proc_ver[idx].ver->minor, tee_proc_ver[idx].ver->tcc_rev);
			if (tee_proc_ver[idx].ver->date) {
				seq_printf(m, " - %04x/%02x/%02x %02x:%02x:%02x",
					(uint32_t)((tee_proc_ver[idx].ver->date>>40)&0xFFFF),
					(uint32_t)((tee_proc_ver[idx].ver->date>>32)&0xFF),
					(uint32_t)((tee_proc_ver[idx].ver->date>>24)&0xFF),
					(uint32_t)((tee_proc_ver[idx].ver->date>>16)&0xFF),
					(uint32_t)((tee_proc_ver[idx].ver->date>>8)&0xFF),
					(uint32_t)((tee_proc_ver[idx].ver->date)&0xFF));
			}
			seq_printf(m, "\n");
		}
		else
			seq_printf(m, "%s: Not Implemented\n", tee_proc_ver[idx].name);
	}

	return 0;
}

static const struct seq_operations tee_op = {
	.start	= tee_start,
	.next	= tee_next,
	.stop	= tee_stop,
	.show	= tee_show
};

static int tee_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &tee_op);
}

static const struct file_operations proc_tee_operations = {
	.open		= tee_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init tee_proc_init(void)
{
	proc_create("optee", 0, NULL, &proc_tee_operations);
	return 0;
}

__initcall(tee_proc_init);

/**
 * Allocate Dynamic Secure Media Path Area.
 */
int tee_alloc_dynanic_smp(int id, uint32_t base, uint32_t size)
{
	struct arm_smccc_res res;

	arm_smccc_smc(OPTEE_SMC_CALL_ALLOC_DYNAMIC_SMP, \
		id, base, size, 0, 0, 0, 0, &res);

	if (res.a0)
		pr_err("\x1b[31m[ERROR][OPTEE] %s: cmd:0x%x, failed to allcate dynamic smp: res:0x%lx", \
			__func__, OPTEE_SMC_CALL_ALLOC_DYNAMIC_SMP, res.a0);

	return (int)res.a0;
}

/**
 * Release Dynamic Secure Media Path Area.
 */
int tee_free_dynanic_smp(int id, uint32_t base, uint32_t size)
{
	struct arm_smccc_res res;

	arm_smccc_smc(OPTEE_SMC_CALL_FREE_DYNAMIC_SMP, \
		id, base, size, 0, 0, 0, 0, &res);

	if (res.a0)
		pr_err("\x1b[31m[ERROR][OPTEE] %s: xmd:0x%x, failed to release dynamic smp: res:0x%lx", \
			__func__, OPTEE_SMC_CALL_FREE_DYNAMIC_SMP, res.a0);

	return (int)res.a0;
}
