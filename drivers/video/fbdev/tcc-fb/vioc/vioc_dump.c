/****************************************************************************
FileName    : kernel/drivers/video/fbdev/tcc-fb/vioc/vioc_dump.c
Description : Sigma Design TV encoder driver

Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/of_address.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_grp_ioctrl.h>
#include <video/tcc/tcc_scaler_ctrl.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/tcc_composite_ioctl.h>
#include <video/tcc/tcc_component_ioctl.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_viqe.h>
#include <video/tcc/vioc_ddicfg.h>
#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/tcc/tca_map_converter.h>
#endif
#ifdef CONFIG_VIOC_DTRC_DECOMP
#include <video/tcc/tca_dtrc_converter.h>
#endif
#include <video/tcc/tca_display_config.h>
#include <video/tcc/tccfb_address.h>

#include <video/tcc/vioc_global.h>

struct vioc_dump_rdma_t {
	unsigned int reg[40];
};
struct vioc_dump_wmix_t {
	unsigned int reg[28];
};
struct vioc_dump_scaler_t {
	unsigned int reg[20];
};

struct vioc_dump_irqconfig_t {
	unsigned int reg[100];
};

struct vioc_dump_mc_t{
	unsigned int reg[100];
};

struct vioc_dump_t {
	struct vioc_dump_rdma_t rdma[4];
	struct vioc_dump_wmix_t wmix;
	struct vioc_dump_scaler_t scaler[6];
	struct vioc_dump_irqconfig_t irqconfig;
	struct vioc_dump_mc_t mc[2];
};

static struct vioc_dump_t *vioc_dump_regs[3];

static struct work_struct vioc_dump_handle_0;
static struct work_struct vioc_dump_handle_1;
static struct work_struct vioc_dump_handle_2;


static int capture_vioc_raw(volatile void __iomem *vioc_preg, unsigned int *raw_reg, unsigned int offset_end)
{
	unsigned int i, offset;

	if(vioc_preg == NULL) {
		for(i = 0, offset = 0; offset < offset_end; i++, offset += 4) {
			raw_reg[i] = 0;
		}
	} else {
		for(i = 0, offset = 0; offset < offset_end; i++, offset += 4) {
			raw_reg[i] = __raw_readl(vioc_preg + offset);
		}
	}
	return 0;
}

static void tca_fb_dump_vioc_status(unsigned int device_id)
{
	unsigned int i, idx, cnt = 0;
	unsigned int *reg;

	struct vioc_dump_t *dump_regs = NULL;

	pr_err("%s %d\r\n", __func__, device_id);
	if(device_id < 3) {
		dump_regs = vioc_dump_regs[device_id];
	}

	if(dump_regs != NULL) {
		pr_err("Dump vioc status %d'th\r\n", device_id);

		/* VIOC RDMA DUMP 0 - 3 */
		for (idx = 0; idx < 4; idx++) {
			pr_err("RDMA :: idx(%d)\n", idx + (device_id * 4));
			reg = &dump_regs->rdma[idx].reg[0];
			//for(cnt = 0; cnt < 0x40; cnt += 0x10) {
			for(i = 0, cnt = 0; cnt < 0x40; i+=4, cnt += 0x10) {
				pr_err("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x \n",
					cnt,
				       	reg[i], reg[i+1], reg[i+2], reg[i+3]);
			}
		}

		/* WMIX DUMP */
		pr_err("WMIX :: idx(%d)\n", device_id);
		reg = &dump_regs->wmix.reg[0];
		for(i = 0, cnt = 0; cnt < 0x70; i+=4, cnt += 0x10) {
			pr_err("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x \n", cnt,
			       reg[i], reg[i+1], reg[i+2], reg[i+3]);
		}

		/* SCALER DUMP 0 - 4 */
		for (idx = 0; idx < 6; idx++) {
			pr_err("SCALER :: idx(%d) \n", idx);
			reg = &dump_regs->scaler[idx].reg[0];
			for(i = 0, cnt = 0; cnt < 0x20; i+=4, cnt += 0x10) {
				pr_err("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x \n", cnt,
				       reg[i], reg[i+1], reg[i+2], reg[i+3]);
			}
		}

		pr_err("MC0 :: \n");
		reg = &dump_regs->mc[0].reg[0];
		for(i = 0, cnt = 0; cnt < 0x80; i+=4, cnt += 0x10) {
			pr_err("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x \n", cnt,
			       reg[i], reg[i+1], reg[i+2], reg[i+3]);
		}
		pr_err("MC1 :: \n");
		reg = &dump_regs->mc[1].reg[0];
		for(i = 0, cnt = 0; cnt < 0x80; i+=4, cnt += 0x10) {
			pr_err("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x \n", cnt,
			       reg[i], reg[i+1], reg[i+2], reg[i+3]);
		}

		/* VIOC_CONFIGURATION DUMP */
		pr_err("IRQCONFIG :: \n");
		reg = &dump_regs->irqconfig.reg[0];
		for(i = 0, cnt = 0; cnt < 0x100; i+=4, cnt += 0x10) {
			pr_err("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x \n", cnt,
			       reg[i], reg[i+1], reg[i+2], reg[i+3]);
		}

	}else {
		pr_err("%s dump_regs is NULL\r\n", __func__);
	}
}

static void tca_fb_dump_disp_0(struct work_struct *work)
{
	tca_fb_dump_vioc_status(0);
}
static void tca_fb_dump_disp_1(struct work_struct *work)
{
	tca_fb_dump_vioc_status(1);
}
static void tca_fb_dump_disp_2(struct work_struct *work)
{
	tca_fb_dump_vioc_status(2);
}

void fb_dump_vioc_init(void)
{
	pr_err("%s \r\n", __func__);
	vioc_dump_regs[0] = kmalloc(sizeof(struct vioc_dump_t), GFP_KERNEL);
	vioc_dump_regs[1] = kmalloc(sizeof(struct vioc_dump_t), GFP_KERNEL);
	vioc_dump_regs[2] = kmalloc(sizeof(struct vioc_dump_t), GFP_KERNEL);

	INIT_WORK(&vioc_dump_handle_0, tca_fb_dump_disp_0);
	INIT_WORK(&vioc_dump_handle_1, tca_fb_dump_disp_1);
	INIT_WORK(&vioc_dump_handle_2, tca_fb_dump_disp_2);
}
EXPORT_SYMBOL(fb_dump_vioc_init);

void fb_dump_vioc_deinit(void)
{
	pr_err("%s \r\n", __func__);
	cancel_work_sync(&vioc_dump_handle_0);
	cancel_work_sync(&vioc_dump_handle_1);
	cancel_work_sync(&vioc_dump_handle_2);
	if(vioc_dump_regs[0] != NULL) {
		kfree(vioc_dump_regs[0]);
	}
	if(vioc_dump_regs[1] != NULL) {
		kfree(vioc_dump_regs[1]);
	}
	if(vioc_dump_regs[2] != NULL) {
		kfree(vioc_dump_regs[2]);
	}
	vioc_dump_regs[0] = NULL;
	vioc_dump_regs[1] = NULL;
	vioc_dump_regs[2] = NULL;
}
EXPORT_SYMBOL(fb_dump_vioc_deinit);

void fb_dump_vioc_status(unsigned int device_id)
{
	unsigned int idx = 0;
	struct vioc_dump_t *dump_regs = NULL;
	volatile void __iomem *vioc_reg;

	if(device_id < 3) {
		dump_regs = vioc_dump_regs[device_id];
	}

	if(dump_regs != NULL) {

		for (idx = 0; idx < 4; idx++) {
			capture_vioc_raw(VIOC_RDMA_GetAddress(idx + (device_id * 4)), &dump_regs->rdma[idx].reg[0], 0x40);
		#if 0
			{
				unsigned int i, cnt;
				unsigned int *reg = &dump_regs->rdma[idx].reg[0];
				for(i = 0, cnt = 0; cnt < 0x40; i+=4, cnt += 0x10) {
					pr_err("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x \n",
						cnt,
						reg[i], reg[i+1], reg[i+2], reg[i+3]);
				}

			}
		#endif
		}

		/* WMIX DUMP */
		vioc_reg = VIOC_WMIX_GetAddress(device_id);
		if(vioc_reg != NULL) {
			capture_vioc_raw(vioc_reg, &dump_regs->wmix.reg[0], 0x70);
		}else {
			pr_err("%s wmix is NULL\r\n", __func__);
			memset(dump_regs->wmix.reg, 0, sizeof(dump_regs->wmix.reg));
		}

		/* SCALER DUMP 0 - 4 */
		for (idx = 0; idx < 6; idx++)  {
			vioc_reg = VIOC_SC_GetAddress(idx);
			if(vioc_reg != NULL) {
				capture_vioc_raw(vioc_reg, &dump_regs->scaler[idx].reg[0], 0x20);
			} else {
				pr_err("%s Scaler id[%d] is NULL\r\n", __func__, idx);
				memset(dump_regs->scaler[idx].reg, 0, sizeof(dump_regs->scaler[idx].reg));
			}
		}

		/* VIOC_CONFIGURATION DUMP */
		vioc_reg = VIOC_IREQConfig_GetAddress();
		if(vioc_reg != NULL) {
			capture_vioc_raw(vioc_reg, &dump_regs->irqconfig.reg[0], 0x100);
		}else {
			pr_err("%s ireqconfig is NULL\r\n", __func__);
			memset(dump_regs->irqconfig.reg, 0, sizeof(dump_regs->irqconfig.reg));
		}

		/* Capture MC */
		vioc_reg = VIOC_MC_GetAddress(0);
		if(vioc_reg != NULL) {
			capture_vioc_raw(vioc_reg,  &dump_regs->mc[0].reg[0], 0x80);
		}else {
			pr_err("%s mc0 is NULL\r\n", __func__);
			memset(dump_regs->mc[0].reg, 0, sizeof(dump_regs->mc[0].reg));
		}

		vioc_reg = VIOC_MC_GetAddress(1);
		if(vioc_reg != NULL) {
			capture_vioc_raw(vioc_reg,  &dump_regs->mc[0].reg[0], 0x80);
		}else {
			pr_err("%s mc1 is NULL\r\n", __func__);
			memset(dump_regs->mc[1].reg, 0, sizeof(dump_regs->mc[1].reg));
		}
		pr_err("%s finish\r\n", __func__);

		switch(device_id) {
			case 0:
				schedule_work(&vioc_dump_handle_0);
				break;
			case 1:
				schedule_work(&vioc_dump_handle_1);
				break;
			case 2:
				schedule_work(&vioc_dump_handle_2);
				break;
		}
	} else {
		pr_err("%s dump_regs is NULL\r\n", __func__);
	}
}
EXPORT_SYMBOL(fb_dump_vioc_status);


