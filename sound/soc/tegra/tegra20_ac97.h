/*
 * linux/sound/soc/tegra/tegra20_ac97.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _TEGRA_AC97_H
#define _TEGRA_AC97_H

#include "tegra_pcm.h"

/* Tegra DAI ID's */
#define TEGRA_DAI_AC97_PCM	0	/* slot 3:	PCM left channel */
					/* slot 4:	PCM right channel */
#define TEGRA_DAI_AC97_MODEM	1	/* slot 5:	modem line 1 */

					/* slot 11:	touch panel digitizer data */

#define AC97_FIFO_ATN_LVL_NONE		0
#define AC97_FIFO_ATN_LVL_EMPTY		1
#define AC97_FIFO_ATN_LVL_QUART		2
#define AC97_FIFO_ATN_LVL_3QUART	3
#define AC97_FIFO_ATN_LVL_FULL		4

#define AC97_FIFO_TX	0
#define AC97_FIFO_RX	1

#define AC97_SAMPLE_RATES	SNDRV_PCM_RATE_8000_48000

/* AC97 controller */
struct tegra20_ac97 {
	struct clk *dap_mclk;
	struct clk *clk_ac97;
	struct snd_card *card;
	struct tegra_pcm_dma_params capture_dma_data;
	phys_addr_t phys;
	struct tegra_pcm_dma_params playback_dma_data;
	void __iomem *regs;
};

#endif
