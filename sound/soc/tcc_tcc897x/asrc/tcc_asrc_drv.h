#ifndef _TCC_ASRC_DRV_H_
#define _TCC_ASRC_DRV_H_

#include "tcc_pl080.h"
#include "tcc_asrc.h"
#include "tcc_asrc_m2m.h"

//#define DEBUG

#ifdef DEBUG
#define dprintk(fmt, args...)  printk("<ASRC> "fmt, ## args)
#else
#define dprintk(fmt, args...)  do { } while (0)
#endif

#define NUM_OF_ASRC_PAIR				(4)
#define NUM_OF_AUX_PERI_CLKS			(4)

enum tcc_asrc_drv_path_t {
	TCC_ASRC_M2M_PATH = 0,
	TCC_ASRC_M2P_PATH = 1,
	TCC_ASRC_P2M_PATH = 2,
	TCC_ASRC_P2P_PATH = 3,
	TCC_ASRC_M2P_MIXER_PATH = 4,
};

enum tcc_asrc_drv_mode_t {
	TCC_ASRC_SYNC_MODE	= 0,
	TCC_ASRC_ASYNC_MODE = 1,
};

struct tcc_pl080_buf_t {
	void *virt;
	dma_addr_t phys;

	struct pl080_lli *lli_virt;
	struct pl080_lli *lli_phys;
};

struct tcc_asrc_t {
	struct platform_device *pdev;
    void __iomem *asrc_reg;
    struct clk *aux_pclk[NUM_OF_AUX_PERI_CLKS];
    struct clk *asrc_hclk;
    int asrc_irq;

    void __iomem *pl080_reg;
    int pl080_irq;
    struct clk *pl080_hclk;

#ifdef CONFIG_ARCH_TCC802X
	bool chip_rev_xx;
#endif
	int m2m_open_cnt;
	//dma buffers
	struct {
		struct tcc_pl080_buf_t txbuf;
		struct tcc_pl080_buf_t rxbuf;
		struct {
			uint32_t max_channel;
			enum tcc_asrc_drv_path_t path;
		} hw;
		struct {
			enum tcc_asrc_drv_bitwidth_t tx_bitwidth;
			enum tcc_asrc_drv_bitwidth_t rx_bitwidth;
			enum tcc_asrc_drv_ch_t channels;
			uint32_t ratio_shift22; // | Int(31~22) | Fraction(21~0) |
		} cfg;
		struct {
			uint32_t started;
			uint32_t readable_size;
			uint32_t read_offset;
			uint32_t writable_size;
			uint32_t write_offset;
		} stat;

		spinlock_t lock;
		struct mutex m;
		wait_queue_head_t wq;
		struct completion comp;
	} pair[NUM_OF_ASRC_PAIR];
};

extern int tcc_asrc_volume_gain(struct tcc_asrc_t *asrc, uint32_t asrc_ch, uint32_t gain);
extern int tcc_asrc_volume_ramp(struct tcc_asrc_t *asrc, uint32_t asrc_ch, uint32_t gain, 
		uint32_t dn_time, uint32_t dn_wait,
		uint32_t up_time, uint32_t up_wait);

extern int tcc_asrc_tx_dma_start(struct tcc_asrc_t *asrc, int asrc_ch);
extern int tcc_asrc_tx_dma_stop(struct tcc_asrc_t *asrc, int asrc_ch);
extern int tcc_asrc_tx_dma_halt(struct tcc_asrc_t *asrc, int asrc_ch);

extern int tcc_asrc_rx_dma_start(struct tcc_asrc_t *asrc, int asrc_ch);
extern int tcc_asrc_rx_dma_stop(struct tcc_asrc_t *asrc, int asrc_ch);
extern int tcc_asrc_rx_dma_halt(struct tcc_asrc_t *asrc, int asrc_ch);

extern int tcc_asrc_m2m_sync_setup(struct tcc_asrc_t *asrc, 
		int asrc_ch, 
		enum tcc_asrc_fifo_fmt_t tx_fmt, 
		enum tcc_asrc_fifo_mode_t tx_mode,
		enum tcc_asrc_fifo_fmt_t rx_fmt, 
		enum tcc_asrc_fifo_mode_t rx_mode,
		uint32_t ratio_shift22);

#endif /*_TCC_ASRC_DRV_H_*/
