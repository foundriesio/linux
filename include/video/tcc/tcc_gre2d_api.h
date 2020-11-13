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
#ifndef __GRE2D_API_H__
#define __GRE2D_API_H__

#include <video/tcc/tcc_gre2d_type.h>

/*------------ Graphice engine special application function --------------*/

#define SET_G2D_DMA_INT_ENABLE         0x00000001
#define SET_G2D_DMA_INT_DISABLE        0x00000002

enum G2D_RSP_TYPE {
	G2D_POLLING_TYPE = 0,
	G2D_INTERRUPT_TYPE,
	G2D_CHECK_TYPE,
	G2D_RSP_MAX
};

extern void gre2d_set_dma_interrupt(unsigned int uiFlag);
extern void gre2d_rsp_interrupt(enum G2D_RSP_TYPE rsp_type);


/*
 * gre2d_interrupt_ctrl
 *  graphic engine interrupt control
 *  wr : 1 : write   0 : read
 *  int_irq : interrupt request
 * int_flg : flag bit
 */
enum G2D_INT_TYPE gre2d_interrupt_ctrl(
	unsigned char wr, enum G2D_INT_TYPE flag,
	unsigned char int_irq, unsigned char int_flg);

/*
 * gre2d_ChImgSize
 *  graphic engine image SIZE converter
 *
 * Graphic engine �� image SIZE �� ���� �� �ش�.
 */
extern unsigned char gre2d_ChImgSize(
	unsigned int src0, unsigned int src1, unsigned int src2,
	unsigned int src_w, unsigned int src_h,
	unsigned int str_x, unsigned int str_y, struct G2D_FMT_CTRL src_fmt,
	unsigned int tgt0, unsigned int tgt1, unsigned int tgt2,
	unsigned int tgt_w, unsigned int tgt_h, struct G2D_FMT_CTRL fmt);

/*
 * gre2d_ChImgFmt
 *  graphic engine image format converter
 *
 * Graphic engine �� image format�� ���� �� �ش�.
 */
extern unsigned char gre2d_ChImgFmt(
	unsigned int src0, unsigned int src1, unsigned int src2,
	struct G2D_FMT_CTRL srcfm, unsigned int tgt0, unsigned int tgt1,
	unsigned int tgt2, struct G2D_FMT_CTRL tgtfm,
	unsigned int imgh, unsigned int imgv);

/*
 * gre2d_ImgArith
 *  graphic engine image arithmetic operation
 *
 * Graphic engine �� image arithmetic ���� �� �ش�.
 * RGB888 type �� �����
 */
extern unsigned char gre2d_ImgArithmetic(
	unsigned int src0, unsigned int src1, unsigned int src2,
	struct G2D_FMT_CTRL srcfm, unsigned int  src_w, unsigned int  src_h,
	unsigned int tgt0, unsigned int tgt1, unsigned int tgt2,
	struct G2D_FMT_CTRL tgtfm, unsigned int  dest_w, unsigned int  dest_h,
	unsigned int dest_off_x, unsigned int  dest_off_y,
	enum G2D_ARITH_TYPE arith,
	unsigned char R, unsigned char G, unsigned char B);

/*
 * gre2d_ImgOverlay
 *  graphic engine overlay function
 *
 * Graphic engine �� 2 channel alpha-blending �� chroam-key ����� ���� �Ѵ�.
 *
 * alpha_en �� disable �϶��� 2 channel�� add ���� �Ѵ�.
 */
extern unsigned char gre2d_ImgOverlay(struct G2D_OVERY_FUNC *overlay);

/*
 * re2d_ImgROP
 * graphic engine ROP(Raster operation) function
 *
 * raphic engine �� 1~3���� Input image�� image size, offset, window offset ��
 * rotate, flip, format change ���� ó�� �Ŀ� Rop ���� ����� ������
 *
 * GB888 type �� �����
 */
extern unsigned char gre2d_ImgROP(
	struct G2D_ROP_FUNC rop, enum G2D_EN en_channel);

/*
 * gre2d_ImgRotate
 *  graphic engine image rotate function
 *
 * Graphic engine �� image rotate �� Flip ����� ���� �Ѵ�.
 */
extern unsigned char gre2d_ImgRotate(
	unsigned int src0, unsigned int src1, unsigned int src2,
	struct G2D_FMT_CTRL srcfm, unsigned int src_imgx, unsigned int src_imgy,
	unsigned int tgt0, unsigned int tgt1, unsigned int tgt2,
	struct G2D_FMT_CTRL tgtfm, unsigned int des_imgx,
	unsigned int des_imgy, enum G2D_OP_MODE ch_mode);


/*
 * gre2d_ImgRotate_Ex
 *  graphic engine image rotate function
 *
 * Graphic engine �� image rotate �� Flip ����� ���� �Ѵ�.
 * extension ���� : source offset �� destination offset �߰�
 */
extern unsigned char gre2d_ImgRotate_Ex(
	unsigned int src0, unsigned int src1, unsigned int src2,
	struct G2D_FMT_CTRL srcfm, unsigned int  src_imgx, unsigned int  src_imgy,
	unsigned int img_off_x, unsigned int img_off_y,
	unsigned int Rimg_x, unsigned int Rimg_y,
	unsigned int tgt0, unsigned int tgt1, unsigned int tgt2,
	struct G2D_FMT_CTRL tgtfm, unsigned int  des_imgx, unsigned int  des_imgy,
	unsigned int dest_off_x, unsigned int dest_off_y,
	enum G2D_OP_MODE ch_mode, enum G2D_OP_MODE parallel_ch_mode);



extern void gre2d_waiting_result(enum G2D_EN grp_enalbe);


/* ------------------ SOFTWARE APPLICATION  FUNCTION  ----------------------- */


/*
 * gre2d_RGBxxx2RGB888
 *
 * Graphic engine���� ����� RGB888 ��ȯ�� ���ش�.
 * ZF �� ���� ���� �ؼ� ���� ��Ʈ�� ä���.
 */
//extern unsigned char  gre2d_RGBxxx2RGB888(
//	enum G2D_DATA_FM form, unsigned char Red, unsigned char Green,
//	unsigned char Blue, unsigned char *r_Red,
//	unsigned char *r_Green, unsigned char *r_Blue);


/*
 * gre2d_YUVtoRGB888
 *
 * Graphic engine���� ����� YUV to RGB888�� ��ȯ�� ���ش�.
 * gre2d_Y2R_type �� ���� ���� ��ȯ ���� ���õȴ�.
 */
extern void gre2d_YUVtoRGB888(
	unsigned char Y, unsigned char U, unsigned char V,
	unsigned char *R, unsigned char *G, unsigned char *B);


/*
 * gre2d_RGB888toYUV_y
 *
 * Graphic engine���� ����� RGB888 to YUV�� Y ���� ��� �Ѵ�. ��ȯ�� ���ش�.
 * gre2d_R2Y_type �� ���� ���� ��ȯ ���� ���õȴ�.
 */
extern unsigned int gre2d_RGB888toYUV_y(unsigned int  rgb);


/*
 * gre2d_RGB888toYUV_cb
 *
 * Graphic engine���� ����� RGB888 to YUV�� U ���� ��� �Ѵ�. ��ȯ�� ���ش�.
 * gre2d_R2Y_type �� ���� ���� ��ȯ ���� ���õȴ�.
 */
extern unsigned int gre2d_RGB888toYUV_cb(unsigned int rgb);

/*
 * gre2d_RGB888toYUV_cr
 *
 * Graphic engine���� ����� RGB888 to YUV�� V ���� ��� �Ѵ�. ��ȯ�� ���ش�.
 * gre2d_R2Y_type �� ���� ���� ��ȯ ���� ���õȴ�.
 */
extern unsigned int gre2d_RGB888toYUV_cr(unsigned int rgb);

/*
 * gre2d_RGBxxx2RGB888
 *
 * Graphic engine���� ����� RGB888 ��ȯ�� ���ش�.
 * ZF �� ���� ���� �ؼ� ���� ��Ʈ�� ä���.
 */
extern unsigned char gre2d_RGBxxx2RGB888(enum G2D_DATA_FM form,
	unsigned char Red, unsigned char Green, unsigned char Blue,
	unsigned char *r_Red, unsigned char *r_Green, unsigned char *r_Blue);

#endif // __GRE2D_API_H__

