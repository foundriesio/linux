#ifndef _TCC_ASRC_PCM_H_
#define _TCC_ASRC_PCM_H_

#include <linux/platform_device.h>
#include "tcc_asrc_drv.h"

int tcc_asrc_pcm_drvinit(struct platform_device *pdev);
extern int tcc_pl080_asrc_pcm_isr_ch(struct tcc_asrc_t *asrc, int asrc_pair);

#endif //_TCC_ASRC_PCM_H_
