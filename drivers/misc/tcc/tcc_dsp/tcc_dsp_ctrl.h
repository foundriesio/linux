#ifndef _TCC_DSP_CTRL_H_
#define _TCC_DSP_CTRL_H_

#include "tcc_dsp_ipc.h"

#define DSP_CTRL_MSG_FLAG_OFFSET	(24)
#define DSP_CTRL_MSG_FLAG_MASK		(0xff << DSP_CTRL_MSG_FLAG_OFFSET)
#define DSP_CTRL_MSG_SINGLE_FLAG	(1 << DSP_CTRL_MSG_FLAG_OFFSET)
#define DSP_CTRL_MSG_BULK_FLAG		(2 << DSP_CTRL_MSG_FLAG_OFFSET)
#define DSP_CTRL_MSG_ACK_FLAG		(4 << DSP_CTRL_MSG_FLAG_OFFSET)

#define DSP_CTRL_MSG_TYPE_OFFSET	(0)
#define DSP_CTRL_MSG_TYPE_MASK		(0xffff << DSP_CTRL_MSG_TYPE_OFFSET)

enum { // DSP_CTRL MSG TYPE
	TCC_DSP_AM3D_SET_PARAM = 0,
	TCC_DSP_AM3D_GET_PARAM,
	TCC_DSP_AM3D_SET_PARAM_TABLE,
	TCC_DSP_AM3D_GET_PARAM_TABLE,
	TCC_DSP_AM3D_CREATE_AND_ADD_FILTER,
	TCC_DSP_AM3D_SELECT_FILETER_BY_INDEX,
	TCC_DSP_CONTROL_SET_PARAM = 10,
	TCC_DSP_CONTROL_GET_PARAM,
};

//////////////////// Control Parameter ///////////////////////////

enum{//Control Parmeter Index
	TCC_DSP_CTL_IDX_EARLY_FM_ONOFF = 0,
	TCC_DSP_CTL_IDX_SET_DEBUG_INPUT,
	TCC_DSP_CTL_IDX_END
};


enum{//Control Parmeter Set Debug Input
	TCC_DSP_CTL_IDX_SET_DEBUG_INPUT_NONE = 0,
	TCC_DSP_CTL_IDX_SET_DEBUG_INPUT_ENHANCED_AUDIO,
	TCC_DSP_CTL_IDX_SET_DEBUG_INPUT_AUDIO0, 
	TCC_DSP_CTL_IDX_SET_DEBUG_INPUT_END
};

////////////////////////////////////////////////////////////////

#define SINGLE_MSG_MAX	(PARAM_MAX - 1)
struct tcc_ctrl_msg_t {
	unsigned int seq_no;
	union {
		struct {
			unsigned int phys_addr; // bulk
			unsigned int size; 
		} bulk;
		struct {
			unsigned int value[SINGLE_MSG_MAX];
		} single;
	} u;
} __attribute__((packed));

struct tcc_dsp_ctrl_t {
	struct device *dev;
	unsigned int seq_no;
	struct completion wait;
	struct mutex lock;
	unsigned int *cur_single_msgbuf;
};

int tcc_dsp_ctrl_init(struct tcc_dsp_ctrl_t *p, struct device *dev);
int tcc_am3d_set_param(struct tcc_dsp_ctrl_t *p, struct tcc_am3d_param_t *param);
int tcc_am3d_get_param(struct tcc_dsp_ctrl_t *p, struct tcc_am3d_param_t *param);
int tcc_am3d_set_param_tbl(struct tcc_dsp_ctrl_t *p, struct tcc_am3d_param_tbl_t *param_tbl); 
int tcc_am3d_get_param_tbl(struct tcc_dsp_ctrl_t *p, struct tcc_am3d_param_tbl_t *param_tbl);

int tcc_control_set_param(struct tcc_dsp_ctrl_t *p, struct tcc_control_param_t *param);
int tcc_control_get_param(struct tcc_dsp_ctrl_t *p, struct tcc_control_param_t *param);
#endif /*_TCC_DSP_CTRL_H_*/
