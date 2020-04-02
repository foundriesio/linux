// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, Telechips Inc
 */


/**
 * tee_trace_config
 *
 * @base: [in] Shared memory base address for trace log.
 * @size: [in] Shared memory size for trace log.
 */
void tee_trace_config(char *base, uint32_t size);

/**
 * tee_trace_get_log
 *
 * @addr: [out]user memory address for get trace log
 * @size: [in]user memory size for get trace log
 *
 * @result:
 *    result > 0: copied buffer size
 *    result < 0: error
 */
int tee_trace_get_log(uint64_t __user addr, uint64_t size);
