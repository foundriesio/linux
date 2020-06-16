/*
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2013
 * Description: LINUX H/W DEMUX FUNCTIONS
 *
 *   Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __TCC_TSIF_MODULE_HWSET_H__
#define __TCC_TSIF_MODULE_HWSET_H__

#include <linux/types.h>

struct tea_dma_buf
{
	void *v_addr;
	dma_addr_t dma_addr;
	int buf_size; // total size of DMA
	void *v_sec_addr;
	dma_addr_t dma_sec_addr;
	int buf_sec_size; // total size of DMA
};

struct tca_tsif_port_config
{
	unsigned tsif_port;
	unsigned tsif_id;
};

struct tcc_tsif_filter;

typedef int (*dma_alloc_f)(struct tea_dma_buf *tdma, unsigned int size);
typedef void (*dma_free_f)(struct tea_dma_buf *tdma);

typedef int (*FN_UPDATECALLBACK)(
	unsigned int dmxid, unsigned int ftype, unsigned fid, unsigned int value1, unsigned int value2,
	unsigned int bErrCRC);
typedef void (*FN_NSKCALLBACK)(int irq, void *dev, volatile int nReg[]);

struct tcc_tsif_handle
{
	struct tea_dma_buf *dma_buffer;
	unsigned int dmx_id;
	unsigned int dma_intr_packet_cnt;
	unsigned int dma_mode;
	unsigned int serial_mode;
	unsigned int working_mode; // 0:tsif for tdmb, 1:tsif mode for dvbt & isdbt, 2:internal mode
	struct clk *pkt_gen_clk;
	struct tca_tsif_port_config port_cfg;
};

void hwdmx_set_evt_handler(void);
void hwdmx_set_interface_cmd(int iDMXID, int mode); // mode(0: serial, 1: parallel)
int hwdmx_get_interface(int iDMXID);
int hwdmx_start_cmd(struct tcc_tsif_handle *h);
int hwdmx_stop_cmd(struct tcc_tsif_handle *h);
int hwdmx_buffer_updated_callback(struct tcc_tsif_handle *h, FN_UPDATECALLBACK buffer_updated);
int hwdmx_set_pcrpid_cmd(struct tcc_tsif_handle *h, unsigned int pid);
int hwdmx_add_filter_cmd(struct tcc_tsif_handle *h, struct tcc_tsif_filter *feed);
int hwdmx_remove_filter_cmd(struct tcc_tsif_handle *h, struct tcc_tsif_filter *feed);
int hwdmx_input_stream_cmd(unsigned int dmx_id, unsigned int phy_addr, unsigned int size);
int hwdmx_set_cipher_dec_pid_cmd(struct tcc_tsif_handle *h,
		unsigned int numOfPids, unsigned int delete_option, unsigned short *pids);
int hwdmx_set_algo_cmd(struct tcc_tsif_handle *h, int algo,
	int opmode, int residual, int smsg, unsigned int numOfPids, unsigned short *pids);
int hwdmx_set_key_cmd(struct tcc_tsif_handle *h, int keytype, int keymode, int size, void *key);
int hwdmx_set_iv_cmd(struct tcc_tsif_handle *h, int ividx, int size, void *iv);
int hwdmx_run_cipher_cmd(int dmxid, int encmode, int cwsel, int klidx, int keymode);
int hwdmx_set_data_cmd(
	unsigned int dmxid, unsigned char *srcaddr, unsigned char *destaddr, unsigned int srclen);

#endif /*__TCC_TSIF_MODULE_HWSET_H__*/
