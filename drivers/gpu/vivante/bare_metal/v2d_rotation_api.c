/*
 * drivers/gpu/vivante/bare_metal/2d_rotation_api.c
 *
 * Vivante 2D Rotation driver with Bare-Metal case
 *
 * Copyright (C) 2009 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "v2d_cmd.h"

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

static int debug = 0;
#define dprintk(msg...)	if(debug) { 	printk("V2D_ROT:  " msg); 	}

//#define PRINTOUT_CMD
#ifdef PRINTOUT_CMD
static void print_log(unsigned int *real_cmd, unsigned int w, unsigned int h, unsigned int rot)
{
	unsigned int szCmd = sizeof(v2d_cmd)/sizeof(unsigned int);
	int i = 0;

	printk(" @@@@@@ v2d_cmd check Start ~~~~~~~~ @@@@@@ :: %d x %d rot(%d: 90/270/h-flip/v-flip) cmd_size(%d) \n", w, h, rot, szCmd);

	for(i=0; i<szCmd; i++)
	{
		printk("[%d] : 0x%08x, \n", i, real_cmd[i]);
	}

	printk(" ###### v2d_cmd check End ~~~~~~~~ ###### \n");
}
#endif

int v2d_proc_rotation(
	unsigned int reg_virtAddr,
	stRot_Info stRotInfo
)
{
	unsigned int rot_regval = 0;
	unsigned int src_w = 0, src_h = 0, src_stride = 0;
	unsigned int dst_w = 0, dst_h = 0, dst_stride = 0;
	unsigned int *remap_cmd = NULL;

	if(stRotInfo.cmd_len < sizeof(v2d_cmd))
	{
		printk("%s-%d :: not enough command buffer size(0x%x < x%x) \n", __func__, __LINE__, stRotInfo.cmd_len, sizeof(v2d_cmd));
		return -1;
	}

#ifdef CONFIG_ARM
	remap_cmd = (unsigned char*)ioremap_nocache((phys_addr_t)stRotInfo.cmd_phy_addr, stRotInfo.cmd_len);
	if(!remap_cmd)
	{
		printk("%s-%d :: cmd_buffer(0x%x - 0x%x) remap error \n", __func__, __LINE__, stRotInfo.cmd_phy_addr, stRotInfo.cmd_len);
		return -1;
	}
#else
	remap_cmd = (unsigned int*)stRotInfo.cmd_phy_addr;
#endif

	memcpy((void*)remap_cmd, v2d_cmd, sizeof(v2d_cmd));

	rot_regval  = rotation_val[stRotInfo.rotation];
#ifdef SUPPORT_VARIABLE_RESOLUTION
	src_w		= stRotInfo.src_resolution.width;
	src_h		= stRotInfo.src_resolution.height;
#else
	src_w		= resolution_info[stRotInfo.src_resolution].width;
	src_h		= resolution_info[stRotInfo.src_resolution].height;
#endif
	src_stride	= src_w * 4;

	if(stRotInfo.rotation == R_90 || stRotInfo.rotation == R_270) {
		dst_w		= src_h;
		dst_h		= src_w;
	}
	else {
		dst_w		= src_w;
		dst_h		= src_h;
	}
	dst_stride	= dst_w * 4;

// source/target buffer
	remap_cmd[TARGET_BUFFER_0] = remap_cmd[TARGET_BUFFER_1] = stRotInfo.dst_phy_addr;
	remap_cmd[SOURCE_BUFFER_0] = remap_cmd[SOURCE_BUFFER_1] = remap_cmd[SOURCE_BUFFER_2] = stRotInfo.src_phy_addr;

// rotation type
	remap_cmd[ROTATION_TYPE_0] = remap_cmd[ROTATION_TYPE_1] = remap_cmd[ROTATION_TYPE_2] = remap_cmd[ROTATION_TYPE_3] = rot_regval;
	remap_cmd[ROTATION_TYPE_4] = remap_cmd[ROTATION_TYPE_5] = remap_cmd[ROTATION_TYPE_6] = remap_cmd[ROTATION_TYPE_7] = rot_regval;
	remap_cmd[ROTATION_TYPE_8] = remap_cmd[ROTATION_TYPE_9] = rot_regval;

// source info.
	remap_cmd[SOURCE_HEIGHT] 	 = src_h;
	remap_cmd[SOURCE_WIDTH] 	 = src_w;
	remap_cmd[SOURCE_STRIDE_0] = remap_cmd[SOURCE_STRIDE_1] = src_stride;
	remap_cmd[SOURCE_SIZE_0]   = remap_cmd[SOURCE_SIZE_1] = remap_cmd[SOURCE_SIZE_2] = (src_h << 16) | src_w;

// target info.
	remap_cmd[TARGET_HEIGHT] 	 = dst_h;
	remap_cmd[TARGET_WIDTH] 	 = dst_w;
	remap_cmd[TARGET_STRIDE_0] = remap_cmd[TARGET_STRIDE_1] = dst_stride;

// specific value according to resolution and rotation.
#ifdef SUPPORT_VARIABLE_RESOLUTION
	remap_cmd[PARTICULAR_0] 	= 0x8100 + (src_w * src_h * 6);
#else
	remap_cmd[PARTICULAR_0] 	= particular_data_0[stRotInfo.src_resolution];
#endif
	remap_cmd[PARTICULAR_1] 	= particular_data_1_4[stRotInfo.rotation][0];
	remap_cmd[PARTICULAR_2] 	= particular_data_1_4[stRotInfo.rotation][1];
	remap_cmd[PARTICULAR_3] 	= particular_data_1_4[stRotInfo.rotation][2];
	remap_cmd[PARTICULAR_4] 	= particular_data_1_4[stRotInfo.rotation][3];

	*((unsigned int *)(reg_virtAddr + 0x654)) = stRotInfo.cmd_phy_addr;
	*((unsigned int *)(reg_virtAddr + 0x658)) = 0xffffffff;

#ifdef PRINTOUT_CMD
	print_log(remap_cmd, src_w, src_h, stRotInfo.rotation);
#endif

#ifdef CONFIG_ARM
	if(remap_cmd)
	{
		iounmap((void*)remap_cmd);
	}
#endif

	return 0;
}




