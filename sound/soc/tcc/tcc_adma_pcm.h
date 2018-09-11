#ifndef _TCC_ADMA_PCM_DT_H_
#define _TCC_ADMA_PCM_DT_H_

#include "tcc_adma.h"

typedef enum {
	TCC_ADMA_I2S_STEREO = 0,
	TCC_ADMA_I2S_7_1CH = 1,
	TCC_ADMA_I2S_9_1CH = 2,
	TCC_ADMA_SPDIF = 3,
	TCC_ADMA_CDIF = 4,
	TCC_ADMA_MAX,
} TCC_ADMA_DEV_TYPE;

struct tcc_adma_info {
	TCC_ADMA_DEV_TYPE dev_type;
	bool tdm_mode;
};

#endif //_TCC_ADMA_PCM_DT_H_
