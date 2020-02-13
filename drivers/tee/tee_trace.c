
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "tee_trace.h"

#define LOG_MAX_SIZE	(4*1024 - 8)

struct log_queue {
	uint32_t head;
	uint32_t tail;
};

struct tee_trace_data {
	struct log_queue *log;
	char *base;
	uint32_t size;
};

static struct tee_trace_data *trace;

void tee_trace_config(char *base, uint32_t size)
{
	if (!trace || !base || !size)
		return;

	if (!trace->base) {
		trace->base = base;
		trace->size = size - 8;
		trace->log = (struct log_queue *)&base[size-8];
	}
}

int tee_trace_get_log(uint64_t __user addr, uint64_t size)
{
	uint32_t head;
	int wsize = 0;

	if (!trace)
		return -ENOMEM;

	if (!trace->base || !trace->size)
		return -ENOMEM;

	head = trace->log->head;
	if (head >= LOG_MAX_SIZE) {
		pr_err("[ERROR][OPTEE] %s: trace log head overflowed. head:0x%x\n", \
			__func__, head);
		return -EFAULT;
	}

	if (head > trace->log->tail) {
		wsize = head - trace->log->tail;
		if (wsize > size)
			wsize = size;
		if (copy_to_user(addr, &trace->base[trace->log->tail], wsize))
			return -EFAULT;
	}
	else if (trace->log->tail > head) {
		wsize = LOG_MAX_SIZE - trace->log->tail;
		if (wsize > size)
			wsize = size;
		if (copy_to_user(addr, &trace->base[trace->log->tail], wsize))
			return -EFAULT;
	}
	trace->log->tail = trace->log->tail + wsize;

	if (trace->log->tail >= LOG_MAX_SIZE)
		trace->log->tail = 0;

	return wsize;
}

static int __init tee_trace_init(void)
{
	trace = kmalloc(sizeof(struct tee_trace_data), GFP_KERNEL);
	if (!trace)
		return -ENOMEM;

	memset(trace, 0x0, sizeof(struct tee_trace_data));

	return 0;
}

static void __exit tee_trace_exit(void)
{
	if (trace) {
		kfree(trace);
		trace = NULL;
	}
}

module_init(tee_trace_init);
module_exit(tee_trace_exit);
