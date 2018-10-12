#ifndef _TCC_ASRC_M2M_H_
#define _TCC_ASRC_M2M_H_

#include "tcc_asrc_m2m_ioctl.h"
#include "tcc_asrc_drv.h"

extern int tcc_asrc_m2m_drvinit(struct platform_device *pdev);
extern int tcc_pl080_asrc_m2m_txisr_ch(struct tcc_asrc_t *asrc, int asrc_ch);

#endif /*_TCC_ASRC_M2M_H_*/
