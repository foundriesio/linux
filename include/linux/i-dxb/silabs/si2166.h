#ifndef __SI2166_H__
#define __SI2166_H__

#include <linux/i-dxb/silabs/silabs.h>

#define SI2166_DEV_NAME		"si2166"
#define SI2166_DEV_MAJOR	220

typedef struct tag_si2166_tune_params_t
{
	int i_fe_id;
	int i_frequency;
	int i_bandwidth_hz;
	int i_data_slice_id;
	int i_isi_id;
	int i_plp_id;
	int i_t2_lock_mode;

	unsigned int ui_symbol_rate_bps;

	silabs_standard_e e_standard;
	silabs_stream_e e_stream;
	silbas_constel_e e_constel;
	silabs_polar_e e_polar;
	silabs_band_type_e e_band_type;
} si2166_tune_params_t;

#define SI2166_IOCTL_INIT	_IOW('o', 1, silabs_fe_type_e)
#define SI2166_IOCTL_TUNE	_IOW('o', 2, si2166_tune_params_t)

#endif	// __SI2166_H__
