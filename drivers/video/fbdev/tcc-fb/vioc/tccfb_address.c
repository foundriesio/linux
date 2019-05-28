/****************************************************************************
 *    FileName    : tcc_fb_address.c
 *    Description :
 *    ****************************************************************************
 *
 *    TCC Version 1.0
 *    Copyright (c) Telechips, Inc.
 *    ALL RIGHTS RESERVED
 *    *
 ****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/clk.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tccfb_address.h>

#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>

void tccxxx_GetAddress(unsigned char format, unsigned int base_Yaddr,
		       unsigned int src_imgx, unsigned int src_imgy,
		       unsigned int start_x, unsigned int start_y,
		       unsigned int *Y, unsigned int *U, unsigned int *V)
{
	unsigned int Uaddr, Vaddr, Yoffset, UVoffset, start_yPos, start_xPos;

	start_yPos = (start_y >> 1) << 1;
	start_xPos = (start_x >> 1) << 1;
	Yoffset = (src_imgx * start_yPos) + start_xPos;

	// RGB format
	if ((format >= TCC_LCDC_IMG_FMT_RGB332) &&
	    (format <= TCC_LCDC_IMG_FMT_ARGB6666_3)) {
		int Bpp;

		if (format == TCC_LCDC_IMG_FMT_RGB332)
			Bpp = 1;
		else if ((format >= TCC_LCDC_IMG_FMT_RGB444) &&
			 (format <= TCC_LCDC_IMG_FMT_RGB555))
			Bpp = 2;
		else if ((format >= TCC_LCDC_IMG_FMT_RGB888) &&
			 (format <= TCC_LCDC_IMG_FMT_ARGB6666_3))
			Bpp = 4;
		else
			Bpp = 2;

		*Y = base_Yaddr + Yoffset * Bpp;
		return;
	}

	if ((format == TCC_LCDC_IMG_FMT_UYVY) ||
	    (format == TCC_LCDC_IMG_FMT_VYUY) ||
	    (format == TCC_LCDC_IMG_FMT_YUYV) ||
	    (format == TCC_LCDC_IMG_FMT_YVYU))
		Yoffset = 2 * Yoffset;

	*Y = base_Yaddr + Yoffset;

	if (*U == 0 && *V == 0) {
		Uaddr = GET_ADDR_YUV42X_spU(base_Yaddr, src_imgx, src_imgy);
		if (format == TCC_LCDC_IMG_FMT_YUV420SP)
			Vaddr = GET_ADDR_YUV420_spV(Uaddr, src_imgx, src_imgy);
		else
			Vaddr = GET_ADDR_YUV422_spV(Uaddr, src_imgx, src_imgy);
	} else {
		Uaddr = *U;
		Vaddr = *V;
	}

	if ((format == TCC_LCDC_IMG_FMT_YUV420SP) ||
	    (format == TCC_LCDC_IMG_FMT_YUV420ITL0) ||
	    (format == TCC_LCDC_IMG_FMT_YUV420ITL1)) {
		if (format == TCC_LCDC_IMG_FMT_YUV420SP)
			UVoffset =
				((src_imgx * start_yPos) / 4 + start_xPos / 2);
		else
			UVoffset = ((src_imgx * start_yPos) / 2 + start_xPos);
	} else {
		if (format == TCC_LCDC_IMG_FMT_YUV422ITL1)
			UVoffset = ((src_imgx * start_yPos) + start_xPos);
		else
			UVoffset =
				((src_imgx * start_yPos) / 2 + start_xPos / 2);
	}

	*U = Uaddr + UVoffset;
	*V = Vaddr + UVoffset;

	// printk(" ### %s Yoffset = [%d] \n",__func__,Yoffset);
}
EXPORT_SYMBOL(tccxxx_GetAddress);


unsigned int tcc_vioc_display_dt_parse(struct device_node *np,
				       struct tcc_dp_device *dp_data)
{
	int ret = 0;
	unsigned int index = 0, i = 0;
	struct device_node *wmixer_node, *ddc_node, *rdma_node, *wdma_node;

	ddc_node = of_parse_phandle(np, "telechips,disp", 0);

	if (!ddc_node) {
		pr_err("could not find telechips,disp node\n");
		ret = -ENODEV;
	}
	of_property_read_u32_index(np, "telechips,disp", 1, &index);
	dp_data->ddc_info.virt_addr = VIOC_DISP_GetAddress(index);
	dp_data->ddc_info.blk_num = index;
	dp_data->DispNum = get_vioc_index(dp_data->ddc_info.blk_num);
	dp_data->ddc_info.irq_num =
		irq_of_parse_and_map(ddc_node, get_vioc_index(index));

	dp_data->vioc_clock = of_clk_get_by_name(ddc_node, "ddi-clk");
	BUG_ON(dp_data->vioc_clock == NULL);

	if (dp_data->DispNum == get_vioc_index(VIOC_DISP0))
		dp_data->ddc_clock = of_clk_get_by_name(ddc_node, "disp0-clk");
	else if (dp_data->DispNum == get_vioc_index(VIOC_DISP1))
		dp_data->ddc_clock = of_clk_get_by_name(ddc_node, "disp1-clk");
	else
		pr_err("could not find ddc clock. invalid display number:%d\n",
		       dp_data->DispNum);

	BUG_ON(dp_data->ddc_clock == NULL);

	wmixer_node = of_parse_phandle(np, "telechips,wmixer", 0);
	// get wmixer number
	of_property_read_u32_index(np, "telechips,wmixer", 1, &index);
	if (!wmixer_node) {
		pr_err("could not find wmixer node\n");
		ret = -ENODEV;
	} else {
		dp_data->wmixer_info.virt_addr = VIOC_WMIX_GetAddress(index);
		dp_data->wmixer_info.irq_num = irq_of_parse_and_map(
			wmixer_node, get_vioc_index(index));
		dp_data->wmixer_info.blk_num = index;
		pr_info("wmixer %d 0x%p  irq:%d\n", get_vioc_index(index),
			dp_data->wmixer_info.virt_addr,
			dp_data->wmixer_info.irq_num);
	}

	rdma_node = of_parse_phandle(np, "telechips,rdma", 0);
	if (!rdma_node) {
		pr_err("could not find telechips,rdma node\n");
		ret = -ENODEV;
	} else {
		for (i = 0; i < RDMA_MAX_NUM; i++) {
			of_property_read_u32_index(np, "telechips,rdma", i + 1,
						   &index);
			dp_data->rdma_info[i].virt_addr =
				VIOC_RDMA_GetAddress(index);
			dp_data->rdma_info[i].irq_num = irq_of_parse_and_map(
				rdma_node, get_vioc_index(index));
			dp_data->rdma_info[i].blk_num = index;
		}
	}

	wdma_node = of_parse_phandle(np, "telechips,wdma", 0);
	// get wdma number
	of_property_read_u32_index(np, "telechips,wdma", 1, &index);
	if (!wdma_node) {
		pr_err("could not find wdma node\n");
		ret = -ENODEV;
	} else {
		dp_data->wdma_info.virt_addr = VIOC_WDMA_GetAddress(index);
		dp_data->wdma_info.irq_num =
			irq_of_parse_and_map(wdma_node, get_vioc_index(index));
		dp_data->wdma_info.blk_num = index;
		pr_info("wdma %d 0x%p  irq:%d\n", get_vioc_index(index),
			dp_data->wdma_info.virt_addr,
			dp_data->wdma_info.irq_num);
	}

	pr_info("ddc:%d %d, wmixer:%d rdma:%d wdma:%d\n",
		get_vioc_index(dp_data->ddc_info.blk_num), dp_data->DispNum,
		get_vioc_index(dp_data->wmixer_info.blk_num),
		get_vioc_index(dp_data->rdma_info[RDMA_FB].blk_num),
		get_vioc_index(dp_data->wdma_info.blk_num));
	pr_info("ddc:0x%p, wmixer:0x%p rdma:0x%p wdma:0x%p\n",
		dp_data->ddc_info.virt_addr, dp_data->wmixer_info.virt_addr,
		dp_data->rdma_info[RDMA_FB].virt_addr,
		dp_data->wdma_info.virt_addr);

	return 0;
}
EXPORT_SYMBOL(tcc_vioc_display_dt_parse);
