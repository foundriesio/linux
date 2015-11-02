/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _MSM_VDEC_CTRLS_H_
#define _MSM_VDEC_CTRLS_H_

int vdec_ctrl_init(struct vidc_inst *inst);
void vdec_ctrl_deinit(struct vidc_inst *inst);

int vdec_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl);
int vdec_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl);

#endif /* _MSM_VDEC_CTRLS_H_ */
