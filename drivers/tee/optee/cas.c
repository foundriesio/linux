/*
 * Copyright (c) 2015, Linaro Limited
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
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "optee_private.h"

struct optee_cas_req {
	struct list_head link;

	bool busy;
	u32 ret;
	size_t size;
	void *data;

	struct completion c;
};

static struct optee_cas_req  *cas_pop_entry(struct optee_supp *cas, int *id)
{
	struct optee_cas_req *req;

	if (cas->req_id != -1)
		return ERR_PTR(-EINVAL);

	if (list_empty(&cas->reqs))
		return NULL;

	req = list_first_entry(&cas->reqs, struct optee_cas_req, link);

	*id = idr_alloc(&cas->idr, req, 1, 0, GFP_KERNEL);
	if (*id < 0)
		return ERR_PTR(-ENOMEM);

	list_del(&req->link);
	req->busy = true;

	return req;
}

static struct optee_cas_req *cas_pop_req(struct optee_supp *cas)
{
	struct optee_cas_req *req;
	int id;

	if (cas->req_id == -1) {
		return ERR_PTR(-EINVAL);
	} else {
		id = cas->req_id;
	}

	req = idr_find(&cas->idr, id);
	if (!req)
		return ERR_PTR(-ENOENT);

	req->busy = false;
	idr_remove(&cas->idr, id);
	cas->req_id = -1;

	return req;
}

/**
 * optee_cas_thrd_req() - request service from cas
 * @ctx:	context doing the request
 * @data:	data
 * @size:	size of data
 *
 * Returns result of operation to be passed to secure world
 */
u32 optee_cas_thrd_req(struct tee_context *ctx, void *data, size_t size)

{
	struct optee *optee = tee_get_drvdata(ctx->teedev);
	struct optee_supp *cas = &optee->cas;
	struct optee_cas_req *req = kzalloc(sizeof(*req), GFP_KERNEL);
	bool interruptable;
	u32 ret;

	if (!req)
		return TEEC_ERROR_OUT_OF_MEMORY;

	init_completion(&req->c);
	req->size = size;
	req->data = data;

	/* Insert the request in the request list */
	mutex_lock(&cas->mutex);
	list_add_tail(&req->link, &cas->reqs);
	mutex_unlock(&cas->mutex);

	/* Tell an eventual waiter there's a new request */
	complete(&cas->reqs_c);

	/*
	 * Wait for supplicant to process and return result, once we've
	 * returned from wait_for_completion(&req->c) successfully we have
	 * exclusive access again.
	 */
	while (wait_for_completion_interruptible(&req->c)) {
		mutex_lock(&cas->mutex);
		interruptable = !cas->ctx;
		if (interruptable) {
			/*
			 * There's no supplicant available and since the
			 * cas->mutex currently is held none can
			 * become available until the mutex released
			 * again.
			 *
			 * Interrupting an RPC to supplicant is only
			 * allowed as a way of slightly improving the user
			 * experience in case the supplicant hasn't been
			 * started yet. During normal operation the supplicant
			 * will serve all requests in a timely manner and
			 * interrupting then wouldn't make sense.
			 */
			interruptable = !req->busy;
			if (!req->busy)
				list_del(&req->link);
		}
		mutex_unlock(&cas->mutex);

		if (interruptable) {
			req->ret = TEEC_ERROR_COMMUNICATION;
			break;
		}
	}

	ret = req->ret;
	kfree(req);

	return ret;
}

/**
 * optee_cas_recv() - receive request for cas
 * @ctx:	context receiving the request
 * @func:	requested function in cas
 * @num_params:	number of elements allocated in @param, updated with number
 *		used elements
 * @param:	space for parameters for @func
 *
 * Returns 0 on success or <0 on failure
 */

int optee_cas_recv(struct tee_context *ctx, void **data, size_t *size)
{
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);
	struct optee_supp *cas = &optee->cas;
	struct optee_cas_req *req = NULL;
	int id;

	while (true) {
		mutex_lock(&cas->mutex);
		req = cas_pop_entry(cas, &id);
		mutex_unlock(&cas->mutex);

		if (req) {
			if (IS_ERR(req))
				return PTR_ERR(req);
			break;
		}

		/*
		 * If we didn't get a request we'll block in
		 * wait_for_completion() to avoid needless spinning.
		 *
		 * This is where supplicant will be hanging most of
		 * the time, let's make this interruptable so we
		 * can easily restart supplicant if needed.
		 */
		if (wait_for_completion_interruptible(&cas->reqs_c))
			return -ERESTARTSYS;
	}

	mutex_lock(&cas->mutex);
	cas->req_id = id;
	mutex_unlock(&cas->mutex);

	*data = req->data;
	*size = req->size;

	return 0;
}

/**
 * optee_cas_send() - send result of request from cas
 * @ctx:	context sending result
 * @ret:	return value of request
 * @num_params:	number of parameters returned
 * @param:	returned parameters
 *
 * Returns 0 on success or <0 on failure.
 */
int optee_cas_send(struct tee_context *ctx, void *data, size_t size)
{
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);
	struct optee_supp *cas = &optee->cas;
	struct optee_cas_req *req;

	mutex_lock(&cas->mutex);
	req = cas_pop_req(cas);
	mutex_unlock(&cas->mutex);

	if (IS_ERR(req)) {
		/* Something is wrong, let supplicant restart. */
		return PTR_ERR(req);
	}

	if (size) {
		if (req->size < size)
			req->ret = TEEC_ERROR_COMMUNICATION;
		else {
			memcpy(req->data, data, size);
			req->size = size;
			req->ret = TEEC_SUCCESS;
		}
	}
	else
		req->ret = TEEC_ERROR_COMMUNICATION;

	/* Let the requesting thread continue */
	complete(&req->c);

	return 0;
}

