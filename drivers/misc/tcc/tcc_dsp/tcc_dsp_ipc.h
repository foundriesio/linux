#ifndef _TCC_DSP_IPC_H_
#define _TCC_DSP_IPC_H_

enum {
	TCC_DSP_IPC_ID_NONE = 0,
	TCC_DSP_IPC_ID_ALSA_MAIN,
	TCC_DSP_IPC_ID_ALSA_SUB01,
	TCC_DSP_IPC_ID_ALSA_SUB02,
	TCC_DSP_IPC_ID_CONTROL,
	TCC_DSP_IPC_ID_ALSA_IN_MAIN,
	TCC_DSP_IPC_MAX
};

#define PARAM_MAX	(6)
struct tcc_dsp_ipc_data_t { //MailBox Data
	unsigned int id;
	unsigned int type;
	unsigned int param[PARAM_MAX];
};

typedef void (*tcc_dsp_ipc_handler_t)(struct tcc_dsp_ipc_data_t *ipc_data, void *pdata);

int register_tcc_dsp_ipc(unsigned int ipc_id, tcc_dsp_ipc_handler_t rx_handler, void *handler_pdata, char *name);
int deregister_tcc_dsp_ipc(unsigned int ipc_id);

/* func : tcc_dsp_ipc_send_msg
 * return : remain wait time.
 * 			if return value is 0, timeout occured
 */
int tcc_dsp_ipc_send_msg(struct tcc_dsp_ipc_data_t *p);
int tcc_dsp_ipc_initialize(void);

#endif /*_TCC_DSP_IPC_H_*/
