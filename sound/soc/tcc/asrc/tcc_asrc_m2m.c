
/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#include <linux/timekeeping.h>

#include "tcc_asrc_drv.h"
#include "tcc_asrc_m2m.h"
#include "tcc_asrc.h"
#include "tcc_pl080.h"

#undef dprintk 
#if 0
#define dprintk(f, a...)	printk("<ASRC M2M>" f, ##a)
#else
#define dprintk(f, a...)
#endif

#define X1_RATIO_SHIFT22				(1 << 22)

#define TX_PERIOD_CNT					(4)
#define RX_PERIOD_CNT					(TX_PERIOD_CNT * (UP_SAMPLING_MAX_RATIO+1))
#define PERIOD_BYTES					(PL080_MAX_TRANSFER_SIZE * TRANSFER_UNIT_BYTES) // bytes
#define TX_BUFFER_BYTES					(PERIOD_BYTES * TX_PERIOD_CNT)	//bytes
#define RX_BUFFER_BYTES					(PERIOD_BYTES * RX_PERIOD_CNT)	//bytes

#define WRITE_TIMEOUT 	(1000)	//ms
#define FIFO_STATUS_CHECK_DELAY 	(30) //us

//#define LLI_DEBUG

#ifdef LLI_DEBUG
static void tcc_pl080_dump_txbuf_lli(struct tcc_asrc_t *asrc, int asrc_pair)
{
	int i;

	for (i=0; i<TX_PERIOD_CNT; i++) {
		printk("pair[%d].txbuf.lli[%d].src_addr : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].txbuf.lli_virt[i].src_addr);
		printk("pair[%d].txbuf.lli[%d].dst_addr : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].txbuf.lli_virt[i].dst_addr);
		printk("pair[%d].txbuf.lli[%d].next_lli : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].txbuf.lli_virt[i].next_lli);
		printk("pair[%d].txbuf.lli[%d].control0 : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].txbuf.lli_virt[i].control0);
		printk("\n");
	}
}

static void tcc_pl080_dump_rxbuf_lli(struct tcc_asrc_t *asrc,  int asrc_pair)
{
	int i;

	for (i=0; i<RX_PERIOD_CNT; i++) {
		printk("pair[%d].rxbuf.lli[%d].src_addr : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].rxbuf.lli_virt[i].src_addr);
		printk("pair[%d].rxbuf.lli[%d].dst_addr : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].rxbuf.lli_virt[i].dst_addr);
		printk("pair[%d].rxbuf.lli[%d].next_lli : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].rxbuf.lli_virt[i].next_lli);
		printk("pair[%d].rxbuf.lli[%d].control0 : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].rxbuf.lli_virt[i].control0);
		printk("\n");
	}
}
#endif

static int tcc_pl080_setup_tx_list(struct tcc_asrc_t *asrc, int asrc_pair, uint32_t size)
{
	int i;
	uint32_t cnt, last_sz;

	cnt = (size / PERIOD_BYTES);
	last_sz = size - (cnt*PERIOD_BYTES);

	if (last_sz == 0) {
		cnt--;
		last_sz = PERIOD_BYTES;
	}

	if (last_sz < TRANSFER_UNIT_BYTES) {
		dprintk("%s last_sz : %d\n", __func__, last_sz);
		last_sz = TRANSFER_UNIT_BYTES;
	}
	dprintk("%s cnt:%d, last_sz:%d\n", __func__, cnt, last_sz);

	for (i=0; i<cnt; i++) {
		asrc->pair[asrc_pair].txbuf.lli_virt[i].src_addr = asrc->pair[asrc_pair].txbuf.phys + (i*PERIOD_BYTES);
		asrc->pair[asrc_pair].txbuf.lli_virt[i].dst_addr = get_fifo_in_phys_addr((uint32_t)asrc->asrc_reg_phys, asrc_pair);
		asrc->pair[asrc_pair].txbuf.lli_virt[i].control0 = 
			tcc_pl080_lli_control_value(
					PL080_MAX_TRANSFER_SIZE, //transfer_size
					TCC_PL080_WIDTH_32BIT, //src_width
					1, //src_incr
					TCC_PL080_WIDTH_32BIT,  //dst_width
					0,  //dst_incr
					TCC_PL080_BSIZE_1, //src_burst_size
					TCC_PL080_BSIZE_1, //dst_burst_size
					0); //irq_en
		asrc->pair[asrc_pair].txbuf.lli_virt[i].next_lli = tcc_asrc_txbuf_lli_phys_address(asrc, asrc_pair, i+1);
	}
		
	asrc->pair[asrc_pair].txbuf.lli_virt[cnt].src_addr = asrc->pair[asrc_pair].txbuf.phys + (cnt*PERIOD_BYTES);
	asrc->pair[asrc_pair].txbuf.lli_virt[cnt].dst_addr = get_fifo_in_phys_addr((uint32_t)asrc->asrc_reg_phys, asrc_pair);
	asrc->pair[asrc_pair].txbuf.lli_virt[cnt].control0 = 
		tcc_pl080_lli_control_value(
				last_sz / TRANSFER_UNIT_BYTES, //transfer_size
				TCC_PL080_WIDTH_32BIT, //src_width
				1, //src_incr
				TCC_PL080_WIDTH_32BIT,  //dst_width
				0,  //dst_incr
				TCC_PL080_BSIZE_1, //src_burst_size
				TCC_PL080_BSIZE_1, //dst_burst_size
				1); //irq_en
	asrc->pair[asrc_pair].txbuf.lli_virt[cnt].next_lli = 0x0;

	return 0;
}

static int tcc_pl080_setup_rx_list(struct tcc_asrc_t *asrc, int asrc_pair)
{
	uint32_t cnt = RX_PERIOD_CNT-1;
	int i;

	for (i=0; i<cnt; i++) {
		asrc->pair[asrc_pair].rxbuf.lli_virt[i].src_addr = get_fifo_out_phys_addr((uint32_t)asrc->asrc_reg_phys, asrc_pair);
		asrc->pair[asrc_pair].rxbuf.lli_virt[i].dst_addr = asrc->pair[asrc_pair].rxbuf.phys + (i*PERIOD_BYTES);
		asrc->pair[asrc_pair].rxbuf.lli_virt[i].control0 = 
			tcc_pl080_lli_control_value(
					PL080_MAX_TRANSFER_SIZE, //transfer_size
					TCC_PL080_WIDTH_32BIT, //src_width
					0, //src_incr
					TCC_PL080_WIDTH_32BIT,  //dst_width
					1,  //dst_incr
					TCC_PL080_BSIZE_1, //src_burst_size
					TCC_PL080_BSIZE_1, //dst_burst_size
					0); //irq_en
		asrc->pair[asrc_pair].rxbuf.lli_virt[i].next_lli = tcc_asrc_rxbuf_lli_phys_address(asrc, asrc_pair, i+1);
	}

	asrc->pair[asrc_pair].rxbuf.lli_virt[cnt].src_addr = get_fifo_out_phys_addr((uint32_t)asrc->asrc_reg_phys, asrc_pair);
	asrc->pair[asrc_pair].rxbuf.lli_virt[cnt].dst_addr = asrc->pair[asrc_pair].rxbuf.phys + (i*PERIOD_BYTES);
	asrc->pair[asrc_pair].rxbuf.lli_virt[cnt].control0 = 
		tcc_pl080_lli_control_value(
				PL080_MAX_TRANSFER_SIZE, //transfer_size
				TCC_PL080_WIDTH_32BIT, //src_width
				0, //src_incr
				TCC_PL080_WIDTH_32BIT,  //dst_width
				1,  //dst_incr
				TCC_PL080_BSIZE_1, //src_burst_size
				TCC_PL080_BSIZE_1, //dst_burst_size
				1); //irq_en
	asrc->pair[asrc_pair].rxbuf.lli_virt[cnt].next_lli = 0x0;

	return 0;
}

static int tcc_asrc_m2m_pair_chk(struct tcc_asrc_t *asrc, int asrc_pair)
{
	if ((asrc_pair >= NUM_OF_ASRC_PAIR) || (asrc_pair < 0)) {
		pr_err("param's pair >= NUM_OF_ASRC_PAIR\n");
		return -EINVAL;
	}

	if (asrc->pair[asrc_pair].hw.path != TCC_ASRC_M2M_PATH) {
		pr_err("pair(%d) is not M2M path\n", asrc_pair);
		return -EINVAL;
	}

	return 0;
}

int tcc_asrc_m2m_start(struct tcc_asrc_t *asrc,
		int asrc_pair,
		enum tcc_asrc_drv_bitwidth_t src_bitwidth,
		enum tcc_asrc_drv_bitwidth_t dst_bitwidth,
		enum tcc_asrc_drv_ch_t channels,
		uint32_t ratio_shift22)
{
	uint32_t chs[] = {2, 4, 6, 8};
	enum tcc_asrc_fifo_mode_t fifo_mode;

	if (tcc_asrc_m2m_pair_chk(asrc, asrc_pair) < 0) {
		return -EINVAL;
	}

	if (asrc->pair[asrc_pair].stat.started) {
		printk("Device is already started\n");
		return -EBUSY;
	}

	dprintk("pair         : %d\n", asrc_pair);
	dprintk("src_bitwidth : %d\n", src_bitwidth);
	dprintk("dst_bitwidth : %d\n", dst_bitwidth);
	dprintk("channels     : %d\n", channels);
	dprintk("ratio        : 0x%08x\n", ratio_shift22);

	if (asrc->pair[asrc_pair].hw.max_channel < chs[channels]) {
		pr_err("param's cfg channels > pair's max_channel\n");
		return -EINVAL;
	}
	if ((ratio_shift22 >> 22) > UP_SAMPLING_MAX_RATIO) {
		pr_err("Ratio is out of range. UP_SAMPLING_MAX_RATIO : %d\n", UP_SAMPLING_MAX_RATIO);
		return -EINVAL;
	}
	if (ratio_shift22 * DN_SAMPLING_MAX_RATIO  < X1_RATIO_SHIFT22) {
		pr_err("Ratio is out of range. DN_SAMPLING_MAX_RATIO : %d\n", DN_SAMPLING_MAX_RATIO);
		return -EINVAL;
	}

#ifdef CONFIG_ARCH_TCC802X
	if (asrc->chip_rev_xx) {
		if (ratio_shift22 < X1_RATIO_SHIFT22) {
			pr_err("TCC802x Rev XX can't use down-sampling\n");
			return -EINVAL;
		}
	}
#endif

	fifo_mode = (channels == TCC_ASRC_NUM_OF_CH_8) ? TCC_ASRC_FIFO_MODE_8CH:
				(channels == TCC_ASRC_NUM_OF_CH_6) ? TCC_ASRC_FIFO_MODE_6CH :
				(channels == TCC_ASRC_NUM_OF_CH_4) ? TCC_ASRC_FIFO_MODE_4CH : TCC_ASRC_FIFO_MODE_2CH;

	asrc->pair[asrc_pair].stat.readable_size = 0;
	asrc->pair[asrc_pair].stat.read_offset = 0;
	asrc->pair[asrc_pair].stat.started = 0;

	dprintk("TCC_ASRC_M2M_PATH Sync Mode\n");

	tcc_asrc_m2m_sync_setup(asrc,
							asrc_pair,
							src_bitwidth,
							fifo_mode,
							dst_bitwidth,
							fifo_mode,
							ratio_shift22);

	asrc->pair[asrc_pair].cfg.tx_bitwidth = src_bitwidth;
	asrc->pair[asrc_pair].cfg.rx_bitwidth = dst_bitwidth;
	asrc->pair[asrc_pair].cfg.ratio_shift22 = ratio_shift22;
	asrc->pair[asrc_pair].cfg.channels = channels;

	asrc->pair[asrc_pair].stat.started = 1;
	dprintk("%s - Device is started(%d)\n", __func__, asrc_pair);

	return 0;
}

int tcc_asrc_m2m_stop(struct tcc_asrc_t *asrc, int asrc_pair)
{
	if (tcc_asrc_m2m_pair_chk(asrc, asrc_pair) < 0) {
		return -EINVAL;
	}

	if (!asrc->pair[asrc_pair].stat.started) {
		return -EINVAL;
	}

	tcc_asrc_stop(asrc, asrc_pair);

	asrc->pair[asrc_pair].stat.readable_size = 0;
	asrc->pair[asrc_pair].stat.read_offset = 0;
	asrc->pair[asrc_pair].stat.started = 0;

	return 0;
}

int tcc_asrc_m2m_volume_gain(struct tcc_asrc_t *asrc, int asrc_pair, uint32_t volume_gain)
{
	if (tcc_asrc_m2m_pair_chk(asrc, asrc_pair) < 0) {
		return -EINVAL;
	}

	if (!asrc->pair[asrc_pair].stat.started) {
		return -EINVAL;
	}

	if (volume_gain < TCC_ASRC_VOL_GAIN_MIN)
		return -EINVAL;

	asrc->pair[asrc_pair].volume_gain = volume_gain;
	tcc_asrc_volume_gain(asrc, asrc_pair);

	return 0;
}

int tcc_asrc_m2m_volume_ramp(struct tcc_asrc_t *asrc, int asrc_pair, uint32_t ramp_gain,
	uint32_t dn_time, uint32_t dn_wait, uint32_t up_time, uint32_t up_wait)
{
	if (tcc_asrc_m2m_pair_chk(asrc, asrc_pair) < 0) {
		return -EINVAL;
	}

	if (!asrc->pair[asrc_pair].stat.started) {
		return -EINVAL;
	}

	if (ramp_gain > TCC_ASRC_VOL_RAMP_GAIN_MAX)
		return -EINVAL;

	if (dn_time < TCC_ASRC_RAMP_64DB_PER_1SAMPLE)
		return -EINVAL;

	if (up_time < TCC_ASRC_RAMP_64DB_PER_1SAMPLE)
		return -EINVAL;

	asrc->pair[asrc_pair].volume_ramp.gain = ramp_gain;
	asrc->pair[asrc_pair].volume_ramp.dn_time = dn_time;
	asrc->pair[asrc_pair].volume_ramp.dn_wait = dn_wait;
	asrc->pair[asrc_pair].volume_ramp.up_time = up_time;
	asrc->pair[asrc_pair].volume_ramp.up_wait = up_wait;

	tcc_asrc_volume_ramp(asrc, asrc_pair);

	return 0;
}

static int __tcc_asrc_m2m_push_data(struct tcc_asrc_t *asrc, int asrc_pair, uint32_t size)
{

	if (tcc_asrc_m2m_pair_chk(asrc, asrc_pair) < 0) {
		return -EINVAL;
	}

	if (!asrc->pair[asrc_pair].stat.started) {
		printk("%s - Device is not started(%d)\n", __func__, asrc_pair);
		return -EIO;
	}

	if (size > TX_BUFFER_BYTES) {
		printk("%s error - size is over %d\n", __func__, TX_BUFFER_BYTES);
		return -EINVAL;
	}

	tcc_pl080_setup_tx_list(asrc, asrc_pair, size);
	tcc_pl080_setup_rx_list(asrc, asrc_pair);

#ifdef LLI_DEBUG
	tcc_pl080_dump_txbuf_lli(asrc, asrc_pair);
	tcc_pl080_dump_rxbuf_lli(asrc, asrc_pair);
#endif

	tcc_asrc_rx_dma_start(asrc, asrc_pair);
	tcc_asrc_tx_dma_start(asrc, asrc_pair);

#ifdef LLI_DEBUG
	tcc_pl080_dump_regs(asrc->pl080_reg);
	tcc_asrc_dump_regs(asrc->asrc_reg);
#endif

	if (!wait_for_completion_io_timeout(&asrc->pair[asrc_pair].comp, msecs_to_jiffies(WRITE_TIMEOUT)))  {
		printk("%s timeout\n", __func__);
		tcc_pl080_dump_regs(asrc->pl080_reg);
		tcc_asrc_dump_regs(asrc->asrc_reg);
		return -ETIME;
	}

	return asrc->pair[asrc_pair].stat.readable_size;
}

int tcc_asrc_m2m_push_data(struct tcc_asrc_t *asrc, int asrc_pair, void *data, uint32_t size)
{
	dprintk("%s - size:%d\n", __func__, size);

	if (size > TX_BUFFER_BYTES) {
		printk("%s error - size is over %d\n", __func__, TX_BUFFER_BYTES);
		return -EINVAL;
	}

	if (data == NULL) {
		pr_err("push_data buf is NULL\n");
		return -EINVAL;
	}

	memcpy(asrc->pair[asrc_pair].txbuf.virt, data, size);

	return __tcc_asrc_m2m_push_data(asrc, asrc_pair, size);
}

static int tcc_asrc_m2m_push_data_from_user(struct tcc_asrc_t *asrc, int asrc_pair, void __user *data, uint32_t size)
{
	dprintk("%s - size:%d\n", __func__, size);

	if (size > TX_BUFFER_BYTES) {
		printk("%s error - size is over %d\n", __func__, TX_BUFFER_BYTES);
		return -EINVAL;
	}

	if (data == NULL) {
		pr_err("push_data buf is NULL\n");
		return -EINVAL;
	}

	if (copy_from_user(asrc->pair[asrc_pair].txbuf.virt, data, size) != 0) {
		pr_err("data read failed\n");
		return -EAGAIN;
	}

	return __tcc_asrc_m2m_push_data(asrc, asrc_pair, size);
}

int tcc_asrc_m2m_pop_data(struct tcc_asrc_t * asrc, int asrc_pair, void *data, uint32_t size)
{
	uint32_t rxsize = (size < asrc->pair[asrc_pair].stat.readable_size) ? size : asrc->pair[asrc_pair].stat.readable_size;
	//int ret;

	dprintk("%s - size:%d\n", __func__, size);

	if (tcc_asrc_m2m_pair_chk(asrc, asrc_pair) < 0) {
		return -EINVAL;
	}

	if (!asrc->pair[asrc_pair].stat.started) {
		printk("%s - Device is not started(%d)\n", __func__, asrc_pair);
		return -EIO;
	}

	if (data == NULL) {
		pr_err("pop_data buf is NULL\n");
		return -EINVAL;
	}

	memcpy(data, asrc->pair[asrc_pair].rxbuf.virt + asrc->pair[asrc_pair].stat.read_offset, rxsize);

	//rxsize -= (uint32_t)ret;

	asrc->pair[asrc_pair].stat.read_offset += rxsize;
	asrc->pair[asrc_pair].stat.readable_size -= rxsize;

	return rxsize;
}

static int tcc_asrc_m2m_pop_data_to_user(struct tcc_asrc_t * asrc, int asrc_pair, void __user *data, uint32_t size)
{
	uint32_t rxsize = (size < asrc->pair[asrc_pair].stat.readable_size) ? size : asrc->pair[asrc_pair].stat.readable_size;
	int ret;

	dprintk("%s - size:%d\n", __func__, size);

	if (tcc_asrc_m2m_pair_chk(asrc, asrc_pair) < 0) {
		return -EINVAL;
	}

	if (!asrc->pair[asrc_pair].stat.started) {
		printk("%s - Device is not started(%d)\n", __func__, asrc_pair);
		return -EIO;
	}

	if (data == NULL) {
		pr_err("pop_data buf is NULL\n");
		return -EINVAL;
	}

	ret = copy_to_user(data, asrc->pair[asrc_pair].rxbuf.virt + asrc->pair[asrc_pair].stat.read_offset, rxsize);
	if(ret < 0){
		return -EFAULT;
	}

	rxsize -= (uint32_t)ret;

	asrc->pair[asrc_pair].stat.read_offset += rxsize;
	asrc->pair[asrc_pair].stat.readable_size -= rxsize;

	return rxsize;
}

static int tcc_asrc_get_info(struct tcc_asrc_t *asrc, int asrc_pair, void __user *data)
{
	struct tcc_asrc_param_t param;

	memset(&param, 0, sizeof(param));

	if (asrc->pair[asrc_pair].hw.path == TCC_ASRC_M2M_PATH) {
		param.pair = asrc_pair;
		param.u.info.m2m_available = 1;
		param.u.info.started = asrc->pair[asrc_pair].stat.started;
		param.u.info.max_channels = (asrc->pair[asrc_pair].hw.max_channel == 2) ? TCC_ASRC_NUM_OF_CH_2 : 
			(asrc->pair[asrc_pair].hw.max_channel == 4) ? TCC_ASRC_NUM_OF_CH_4 : 
			(asrc->pair[asrc_pair].hw.max_channel == 6) ? TCC_ASRC_NUM_OF_CH_6 : TCC_ASRC_NUM_OF_CH_8 ;

		if (asrc->pair[asrc_pair].stat.started) {
			param.u.info.ratio_shift22 = asrc->pair[asrc_pair].cfg.ratio_shift22;
			param.u.info.src_bitwidth = asrc->pair[asrc_pair].cfg.tx_bitwidth;
			param.u.info.dst_bitwidth = asrc->pair[asrc_pair].cfg.rx_bitwidth;
			param.u.info.cur_channels = asrc->pair[asrc_pair].cfg.channels;
		}
	} else {
		param.pair = asrc_pair;
		param.u.info.m2m_available = 0;
	}

	if(copy_to_user(data, &param, sizeof(param)) != 0){
		return -EFAULT;
	}

	return 0;
}

static int tcc_asrc_resources_allocation(struct tcc_asrc_t *asrc, uint32_t idx)
{
	struct device *dev = &asrc->pdev->dev;

	dprintk("TX_BUFFER_BYTES :0x%08x\n", TX_BUFFER_BYTES);
	dprintk("RX_BUFFER_BYTES :0x%08x\n", RX_BUFFER_BYTES);

	spin_lock_init(&asrc->pair[idx].lock);
	mutex_init(&asrc->pair[idx].m);
	init_completion(&asrc->pair[idx].comp);
	init_waitqueue_head(&asrc->pair[idx].wq);

	asrc->pair[idx].txbuf.virt = dma_alloc_coherent(dev, TX_BUFFER_BYTES, &asrc->pair[idx].txbuf.phys, GFP_KERNEL);
	asrc->pair[idx].txbuf.lli_virt = dma_alloc_writecombine(dev, sizeof(struct pl080_lli)*TX_PERIOD_CNT, &asrc->pair[idx].txbuf.lli_phys, GFP_KERNEL);

	asrc->pair[idx].rxbuf.virt = dma_alloc_coherent(dev, RX_BUFFER_BYTES, &asrc->pair[idx].rxbuf.phys, GFP_KERNEL);
	asrc->pair[idx].rxbuf.lli_virt = dma_alloc_writecombine(dev, sizeof(struct pl080_lli)*RX_PERIOD_CNT, &asrc->pair[idx].rxbuf.lli_phys, GFP_KERNEL);

	if (!asrc->pair[idx].txbuf.virt)
		printk("asrc->pair[%d].txbuf.virt failed\n", idx);

	if (!asrc->pair[idx].txbuf.lli_virt)
		printk("asrc->pair[%d].txbuf.lli_virt failed\n", idx);

	if (!asrc->pair[idx].rxbuf.virt)
		printk("asrc->pair[%d].rxbuf.virt failed\n", idx);

	if (!asrc->pair[idx].rxbuf.lli_virt)
		printk("asrc->pair[%d].rxbuf.lli_virt failed\n", idx);

	memset(asrc->pair[idx].txbuf.virt, 0, TX_BUFFER_BYTES);
	memset(asrc->pair[idx].txbuf.lli_virt, 0, sizeof(struct pl080_lli)*TX_PERIOD_CNT);

	memset(asrc->pair[idx].rxbuf.virt, 0, RX_BUFFER_BYTES);
	memset(asrc->pair[idx].rxbuf.lli_virt, 0, sizeof(struct pl080_lli)*RX_PERIOD_CNT);

	dprintk("txbuf(%d) - v:%p, p:0x%llu\n", idx, asrc->pair[idx].txbuf.virt, asrc->pair[idx].txbuf.phys);
	dprintk("txbuf(%d)_lli - v:%p, p:0x%llu\n", idx, asrc->pair[idx].txbuf.lli_virt, asrc->pair[idx].txbuf.lli_phys);
	dprintk("rxbuf(%d) - v:%p, p:%llu\n", idx, asrc->pair[idx].rxbuf.virt, asrc->pair[idx].rxbuf.phys);
	dprintk("rxbuf(%d)_lli - v:%p, p:%llu\n", idx, asrc->pair[idx].rxbuf.lli_virt, asrc->pair[idx].rxbuf.lli_phys);

	return 0;
}

int tcc_pl080_asrc_m2m_txisr_ch(struct tcc_asrc_t *asrc, int asrc_pair)
{
	uint32_t dma_tx_ch = asrc_pair;
	uint32_t dma_rx_ch = asrc_pair+ASRC_RX_DMA_OFFSET;
	uint32_t rx_dst_addr;
	uint32_t pl080_tx_ch_cfg_reg;
	uint32_t pl080_rx_ch_cfg_reg;
	uint32_t asrc_fifo_in_status;
	uint32_t asrc_fifo_out_status;

	dprintk("%s(%d)\n", __func__, asrc_pair);

	if (asrc->pair[asrc_pair].hw.path != TCC_ASRC_M2M_PATH) {
		dprintk("%s - it's not m2m path\n", __func__);
		return -1;
	}
	
	tcc_asrc_tx_dma_halt(asrc, asrc_pair);
	while(1) {
		pl080_tx_ch_cfg_reg = readl(asrc->pl080_reg + PL080_Cx_CONFIG(dma_tx_ch));

		if (!(pl080_tx_ch_cfg_reg & PL080_CONFIG_ACTIVE)) {
			dprintk("DMA Tx De-activated\n");
			break;
		} 	
	}

	while(1) {
		asrc_fifo_in_status = tcc_asrc_get_fifo_in_status(asrc->asrc_reg, asrc_pair);

		if (asrc_fifo_in_status & (1 << 30)) {
			dprintk("FIFO In Cleared\n");
			break;
		} 	
	}

	udelay(FIFO_STATUS_CHECK_DELAY);

	while(1) {
		asrc_fifo_out_status = tcc_asrc_get_fifo_out_status(asrc->asrc_reg, asrc_pair);

		if (asrc_fifo_out_status & (1 << 30)) {
			dprintk("FIFO Out Cleared\n");
			break;
		} 	
	}

	udelay(FIFO_STATUS_CHECK_DELAY);
	
	tcc_asrc_rx_dma_halt(asrc, asrc_pair);

	while(1) {
		pl080_rx_ch_cfg_reg = readl(asrc->pl080_reg + PL080_Cx_CONFIG(dma_rx_ch));

		if (!(pl080_rx_ch_cfg_reg & PL080_CONFIG_ACTIVE)) {
			dprintk("DMA Tx/Rx De-activated\n");
			break;
		} 	
	}

	rx_dst_addr = readl(asrc->pl080_reg + PL080_Cx_DST_ADDR(dma_rx_ch));
	asrc->pair[asrc_pair].stat.readable_size = rx_dst_addr - asrc->pair[asrc_pair].rxbuf.phys;
	asrc->pair[asrc_pair].stat.read_offset = 0;

	dprintk("rx_dst_addr      : 0x%08x\n", rx_dst_addr);
	dprintk("rxbuf.phys   : 0x%llu\n", asrc->pair[asrc_pair].rxbuf.phys);
	dprintk("readable_size : 0x%08x\n", asrc->pair[asrc_pair].stat.readable_size);

	tcc_asrc_rx_dma_stop(asrc, asrc_pair);
	tcc_asrc_tx_dma_stop(asrc, asrc_pair);

	complete(&asrc->pair[asrc_pair].comp);

	return 0;
}

#define _ASRC_M2M_KERNEL_TEST_			(0)

#if _ASRC_M2M_KERNEL_TEST_

#define RD_FILE_PATH	"/opt/data/input_s16le_48k.raw"
#define RD_BUF_SIZE	(1024)

#define WR_FILE_PATH	"/opt/data/output_s16le_96k.raw"
#define WR_BUF_SIZE	(4096)

#define TEST_ASRC_PAIR	(0)

char test_read_buf[RD_BUF_SIZE] = {0};
char test_wr_buf[WR_BUF_SIZE] = {0};

static void tcc_asrc_kernel_test(struct tcc_asrc_t *asrc)
{
	struct file *read_fp = NULL, *write_fp = NULL;
	mm_segment_t old_fs;
	ssize_t read_sz;
	int readable_sz, writable_sz;

	read_fp = filp_open(RD_FILE_PATH, O_RDONLY, S_IRUSR|S_IWUSR);
	if (read_fp == NULL) {
		pr_err("%s open failed\n", RD_FILE_PATH);
		return;
	}

	write_fp = filp_open(WR_FILE_PATH, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
	if (write_fp == NULL) {
		pr_err("%s open failed\n", RD_FILE_PATH);
		return;
	}

	old_fs=get_fs();
	set_fs(get_ds());

	tcc_asrc_m2m_start(asrc,
						TEST_ASRC_PAIR, // Pair0
						TCC_ASRC_16BIT, // SRC Bit width
						TCC_ASRC_16BIT, // DST Bit width
						TCC_ASRC_NUM_OF_CH_2, //Channels
						2*X1_RATIO_SHIFT22); // multiple of 2

	do {
		read_sz = vfs_read(read_fp, test_read_buf, RD_BUF_SIZE, &read_fp->f_pos);

		if (read_sz > 0) {
			readable_sz = tcc_asrc_m2m_push_data(asrc, TEST_ASRC_PAIR, test_read_buf, read_sz);
			writable_sz = tcc_asrc_m2m_pop_data(asrc, TEST_ASRC_PAIR, test_wr_buf, readable_sz);

			if (writable_sz > 0) {
				vfs_write(write_fp, test_wr_buf, writable_sz, &write_fp->f_pos);
			}
		}
	} while (read_sz > 0);

	tcc_asrc_m2m_stop(asrc, TEST_ASRC_PAIR);

	set_fs(old_fs);

	filp_close(read_fp, NULL);
}
#endif /*_ASRC_M2M_KERNEL_TEST_*/

static ssize_t tcc_asrc_read(struct file *flip, char __user *buf, size_t count, loff_t *f_pos)
{
	return count;
}

static ssize_t tcc_asrc_write(struct file *flip, const char __user *buf, size_t count, loff_t *f_pos)
{
#if _ASRC_M2M_KERNEL_TEST_
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)dev_get_drvdata(misc->parent);

	tcc_asrc_kernel_test(asrc);
#endif /*_ASRC_M2M_KERNEL_TEST_*/

	return count;
}

static unsigned int tcc_asrc_poll(struct file *flip, struct poll_table_struct *wait)
{
	return (POLLIN | POLLRDNORM);
}

static long tcc_asrc_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)dev_get_drvdata(misc->parent);
	struct tcc_asrc_param_t param;
	int ret = 0;

	dprintk("%s - %d\n", __func__, __LINE__);

	if (copy_from_user(&param, (const void *)arg, sizeof(struct tcc_asrc_param_t)) != 0) {
		pr_err("paramer reading failed\n");
		return -EAGAIN;
	}

	mutex_lock(&asrc->pair[param.pair].m);
	switch(cmd) {
		case IOCTL_TCC_ASRC_M2M_START:
			ret = tcc_asrc_m2m_start(asrc, param.pair,
					param.u.cfg.src_bitwidth, param.u.cfg.dst_bitwidth,
					param.u.cfg.channels, param.u.cfg.ratio_shift22);
			break;
		case IOCTL_TCC_ASRC_M2M_STOP:
			ret = tcc_asrc_m2m_stop(asrc, param.pair);
			break;
		case IOCTL_TCC_ASRC_M2M_PUSH_PCM:
			dprintk("IOCTL_TCC_ASRC_PUSH_PCM\n");
			dprintk("param.u.pcm.buf : %p\n", param.u.pcm.buf);
			dprintk("param.u.pcm.size: 0x%08x\n", (uint32_t)param.u.pcm.size);
			ret = tcc_asrc_m2m_push_data_from_user(asrc, param.pair, param.u.pcm.buf, param.u.pcm.size);
			break;
		case IOCTL_TCC_ASRC_M2M_POP_PCM:
			dprintk("IOCTL_TCC_ASRC_POP_PCM\n");
			dprintk("param.u.pcm.buf : %p\n", param.u.pcm.buf);
			dprintk("param.u.pcm.size: 0x%08x\n", (uint32_t)param.u.pcm.size);
			ret = tcc_asrc_m2m_pop_data_to_user(asrc, param.pair, param.u.pcm.buf, param.u.pcm.size);
			break;
		case IOCTL_TCC_ASRC_M2M_SET_VOL:
			dprintk("IOCTL_TCC_ASRC_SET_VOL : 0x%x\n", param.u.volume_gain);
			tcc_asrc_m2m_volume_gain(asrc, param.pair, param.u.volume_gain);
			break;
		case IOCTL_TCC_ASRC_M2M_SET_VOL_RAMP:
			dprintk("IOCTL_TCC_ASRC_SET_VOL_RAMP\n");
			tcc_asrc_m2m_volume_ramp(asrc, param.pair,
					param.u.volume_ramp.gain,
					param.u.volume_ramp.dn_time,
					param.u.volume_ramp.dn_wait,
					param.u.volume_ramp.up_time,
					param.u.volume_ramp.up_wait);
			break;
		case IOCTL_TCC_ASRC_M2M_GET_INFO:
			ret = tcc_asrc_get_info(asrc, param.pair, (void*)arg);
			break;
		case IOCTL_TCC_ASRC_M2M_DUMP_REGS:
			tcc_asrc_dump_regs(asrc->asrc_reg);
			break;
		case IOCTL_TCC_ASRC_M2M_DUMP_DMA_REGS:
			tcc_pl080_dump_regs(asrc->pl080_reg);
			break;
		default:
			ret = -EINVAL;
			break;
	}
	mutex_unlock(&asrc->pair[param.pair].m);

	return ret;
}

static int tcc_asrc_open(struct inode *inode, struct file *flip)
{
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)dev_get_drvdata(misc->parent);
	int ret = 0;

	dprintk("%s - %d\n", __func__, __LINE__);
	asrc->m2m_open_cnt++;

	return ret;
}

static int tcc_asrc_release(struct inode *inode, struct file *flip)
{
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)dev_get_drvdata(misc->parent);
	int i;

	dprintk("%s - %d\n", __func__, __LINE__);
	asrc->m2m_open_cnt--;

	if (asrc->m2m_open_cnt == 0) {
		for (i=0; i<NUM_OF_ASRC_PAIR; i++) {
			if (asrc->pair[i].hw.path == TCC_ASRC_M2M_PATH) {
				tcc_asrc_m2m_stop(asrc, i);
			}
		}
	}

	return 0;
}

static const struct file_operations tcc_asrc_fops =
{
	.owner          = THIS_MODULE,
	.read           = tcc_asrc_read,
	.write          = tcc_asrc_write,
	.poll           = tcc_asrc_poll,
	.unlocked_ioctl = tcc_asrc_ioctl,
	.open           = tcc_asrc_open,
	.release        = tcc_asrc_release,
};

static struct miscdevice tcc_asrc_misc_device =
{
	MISC_DYNAMIC_MINOR,
	"tcc_asrc_m2m",
	&tcc_asrc_fops,
};

int tcc_asrc_m2m_drvinit(struct platform_device *pdev)
{
	struct tcc_asrc_t *asrc = platform_get_drvdata(pdev);
	int i;

	for (i=0; i<NUM_OF_ASRC_PAIR; i++) {
		if (asrc->pair[i].hw.path == TCC_ASRC_M2M_PATH) {
			tcc_asrc_resources_allocation(asrc, i);
		}
	}

	dprintk("TX_BUFFER_BYTES : %d\n", TX_BUFFER_BYTES);
	dprintk("RX_BUFFER_BYTES : %d\n", RX_BUFFER_BYTES);

	tcc_asrc_misc_device.parent = &pdev->dev;

	if (misc_register(&tcc_asrc_misc_device)) {
		printk(KERN_WARNING "Couldn't register device .\n");
		return -EBUSY;
	}

	return 0;
}

EXPORT_SYMBOL(tcc_asrc_m2m_start);
EXPORT_SYMBOL(tcc_asrc_m2m_stop);
EXPORT_SYMBOL(tcc_asrc_m2m_push_data);
EXPORT_SYMBOL(tcc_asrc_m2m_pop_data);
EXPORT_SYMBOL(tcc_asrc_m2m_volume_gain);
EXPORT_SYMBOL(tcc_asrc_m2m_volume_ramp);

