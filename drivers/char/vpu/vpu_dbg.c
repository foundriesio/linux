// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <linux/utsname.h>
#include <linux/crc32.h>
#include <linux/firmware.h>
#include <linux/devcoredump.h>

#include "vpu_dbg.h"


/**
 * enum vpu_fw_dump_type - types of data in the dump file
 * @VPU_FW_DUMP_REGDUMP: Register dump in binary format
 * @VPU_FW_DUMP_DATA: Data dump in binary format
 */
enum vpu_fw_crash_dump_type {
	VPU_FW_DUMP_REGISTERS = 0,
	VPU_FW_DUMP_DATA = 1,
	VPU_FW_CRASH_DUMP_MAX,
};

bool vpu_use_prtk = false;
unsigned int vpu_dbg_mask = VPU_DBG_ERROR;

module_param_named(use_prtk, vpu_use_prtk, bool, 0644);
MODULE_PARM_DESC(use_prtk, "use printk w/ KERN_ERR (default: disable)");

module_param_named(debug_mask, vpu_dbg_mask, uint, 0644);
MODULE_PARM_DESC(debug_mask, "debug output mask");

static inline bool vpu_have_debug_mask(enum vpu_dbg_mask_type dbg_mask)
{
	if(vpu_dbg_mask & dbg_mask)
		return true;
	return false;
}

void V_DEBUG(enum vpu_dbg_mask_type dbg_mask, const char *fn, int ln, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args;

	va_start(args, fmt);
	vaf.va = &args;
	if(vpu_have_debug_mask(dbg_mask))
	{
		pr_debug("[%s:%d] %pV\n", fn, ln, &vaf);
	}
	//trace_vpu_log_dbg(dev, &vaf);
	va_end(args);
}
EXPORT_SYMBOL(V_DEBUG);
