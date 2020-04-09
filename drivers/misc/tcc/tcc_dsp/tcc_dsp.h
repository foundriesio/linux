#ifndef _TCC_DSP_H_
#define _TCC_DSP_H_

#include <asm/ioctl.h>

#define CMD_TYPE    0x00010000
#define INFO_TYPE   0x00020000

#define ALSA_OUT_BUF_PARAM 1
#define ALSA_IN_BUF_PARAM 2
#define ALSA_HW_PARAM 3
 
#define ALSA_PLAYBACK_STOP 100
#define ALSA_PLAYBACK_START 101
 
#define DSP_PCM_FORMAT_S16_LE   1
#define DSP_PCM_FORMAT_S24_LE   2

#define TCC_AM3D_MAGIC	'A'
#define IOCTL_TCC_AM3D_SET_PARAM				_IO(TCC_AM3D_MAGIC ,0)
#define IOCTL_TCC_AM3D_GET_PARAM				_IO(TCC_AM3D_MAGIC ,1)
#define IOCTL_TCC_AM3D_SET_PARAM_TABLE			_IO(TCC_AM3D_MAGIC ,2)
#define IOCTL_TCC_AM3D_GET_PARAM_TABLE			_IO(TCC_AM3D_MAGIC ,3)

#define IOCTL_TCC_CONTROL_SET_PARAM				_IO(TCC_AM3D_MAGIC ,10)
#define IOCTL_TCC_CONTROL_GET_PARAM				_IO(TCC_AM3D_MAGIC ,11)

struct tcc_control_param_t {
    int tcc_ctl_index;
    int tcc_ctl_param;
    int tcc_ctl_subinfo;
    int tcc_ctl_value;
    int tcc_ctl_status;
} __attribute__((packed));

struct tcc_am3d_param_t {
	int32_t	eEffect;//eZirene_Effect 
	int32_t	eParm;
	int32_t	eChannelMask;
	int32_t	iValue;
	int32_t	retStatus; //eZireneStatus  
} __attribute__((packed));

#define PARAM_TBL_MAX	(10)
struct tcc_am3d_param_tbl_t {
	int32_t iNumOfParameters;
	struct tcc_am3d_param_t tbl[PARAM_TBL_MAX];
} __attribute__((packed));

#endif
