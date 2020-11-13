/*
 * Copyright (C) Telechips, Inc.
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
 */
#ifndef __VIQE_H__
#define __VIQE_H__

#define VIQE_QUEUE_MAX_ENTRY (12) //(8)

#define VIQE_FIELD_NEW       (0)
#define VIQE_FIELD_DUP       (1)
#define VIQE_FIELD_SKIP      (2)
#define VIQE_FIELD_BROKEN    (3)

struct viqe_queue_entry_t {
#if 0
	// frame information
	int frame_base0;
	int frame_base1;
	int frame_base2;
	int bottom_field;
	int time_stamp;
	int new_field;
	int Frame_width;
	int Frame_height;
	int crop_top;
	int crop_bottom;
	int crop_left;
	int crop_right;
	int Image_width;
	int Image_height;
	int offset_x;
	int offset_y;
	int frameInfo_interlace;
	int reset_frmCnt;
#else
	struct tcc_lcdc_image_update input_image;
	int new_field;
	int reset_frmCnt;
#endif
};

struct viqe_queue_t {
	// common information
	int ready;
	struct viqe_queue_entry_t curr_entry;

	struct viqe_queue_entry_t entry[VIQE_QUEUE_MAX_ENTRY];
	int w_ptr;
	int r_ptr;

	// frame rate vs. output rate
	// + : input frame rate is greater than output frame rate
	// - : output frame rate is greater than input frame rate
	int inout_rate;

	// hardware releated
	void *p_rdma_ctrl;
	int frame_cnt;
};

extern struct viqe_queue_t *my_queue_create(int num_entry);
extern struct viqe_queue_t *my_queue_delete(struct viqe_queue_t *p_queue);
extern int viqe_queue_push_entry(struct viqe_queue_t *p_queue,
	struct viqe_queue_entry_t new_entry);
extern struct viqe_queue_entry_t *viqe_queue_pop_entry(
	struct viqe_queue_t *p_queue);
extern int viqe_queue_is_empty(struct viqe_queue_t *p_queue);
extern int viqe_queue_is_full(struct viqe_queue_t *p_queue);
extern struct viqe_queue_entry_t *viqe_queue_show_entry(
	struct viqe_queue_t *p_queue);
extern void viqe_queue_show_entry_info(struct viqe_queue_entry_t *p_entry,
	char *prefix);
extern void viqe_render_init(void);
extern void viqe_render_frame(struct tcc_lcdc_image_update *input_image,
	unsigned int field_interval, int curTime, int reset_frmCnt);
//extern void viqe_render_field(struct tcc_lcdc_image_update *input_image,
//	int curTime);
extern void viqe_render_field(int curTime);

#endif /*__VIQE_H__*/
