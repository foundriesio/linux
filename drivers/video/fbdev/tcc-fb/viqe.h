/*
 * Copyright (C) 2008-2019 Telechips Inc.
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
#ifndef	_MY_QUEUE_H_

#define		VIQE_QUEUE_MAX_ENTRY	12//8

typedef	struct	_my_queue_entry {
#if 0
	//	frame information
	int			frame_base0;
	int			frame_base1;
	int			frame_base2;
	int			bottom_field;
	int			time_stamp;
	int			new_field;
	int 			Frame_width;
	int 			Frame_height;
	int 			crop_top;
	int 			crop_bottom;
	int 			crop_left;
	int 			crop_right;
	int			Image_width;
	int 			Image_height;
	int 			offset_x;
	int 			offset_y;
	int 			frameInfo_interlace;
	int 			reset_frmCnt;
#else
	struct tcc_lcdc_image_update input_image;
	int 		new_field;
	int			reset_frmCnt;

#endif
}	viqe_queue_entry_t;

typedef	struct	_my_queue {
	// common information
	int					ready;
	viqe_queue_entry_t	curr_entry;

	viqe_queue_entry_t	entry[VIQE_QUEUE_MAX_ENTRY];
	int					w_ptr;
	int					r_ptr;

	// frame rate vs. output rate
	int					inout_rate;	// + : input frame rate is greater than output frame rate
									// - : output frame rate is greater than input frame rate

	// hardware releated
	void *				p_rdma_ctrl;
	int					frame_cnt;
}	viqe_queue_t;

extern	viqe_queue_t *	my_queue_create (int num_entry);
extern	viqe_queue_t *	my_queue_delete (viqe_queue_t *p_queue);
extern	int		viqe_queue_push_entry (viqe_queue_t *p_queue, viqe_queue_entry_t new_entry);
extern	viqe_queue_entry_t *		viqe_queue_pop_entry (viqe_queue_t *p_queue);
extern	int							viqe_queue_is_empty (viqe_queue_t *p_queue);
extern	int							viqe_queue_is_full (viqe_queue_t *p_queue);
extern	viqe_queue_entry_t *		viqe_queue_show_entry (viqe_queue_t *p_queue);
extern	void						viqe_queue_show_entry_info (viqe_queue_entry_t *p_entry, char *prefix);
extern 	void						viqe_render_init (void);
extern 	void						viqe_render_frame (struct tcc_lcdc_image_update *input_image, unsigned int field_interval, int curTime,int reset_frmCnt);
//extern	void						viqe_render_field (struct tcc_lcdc_image_update *input_image, int curTime);
extern	void						viqe_render_field (int curTime);


#define		VIQE_FIELD_NEW		0
#define 	VIQE_FIELD_DUP		1
#define 	VIQE_FIELD_SKIP 	2
#define		VIQE_FIELD_BROKEN	3


#endif
