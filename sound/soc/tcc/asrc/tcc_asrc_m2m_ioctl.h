#ifndef _TCC_ASRC_M2M_IOCTL_H_
#define _TCC_ASRC_M2M_IOCTL_H_

#define NUM_OF_ASRC_PAIR				(4)
#define UP_SAMPLING_MAX_RATIO			(8)
#define DN_SAMPLING_MAX_RATIO			(7)

enum tcc_asrc_drv_bitwidth_t {
	TCC_ASRC_16BIT = 0,
	TCC_ASRC_24BIT = 1,
};

enum tcc_asrc_drv_ch_t {
	TCC_ASRC_NUM_OF_CH_2 = 0,
	TCC_ASRC_NUM_OF_CH_4 = 1,
	TCC_ASRC_NUM_OF_CH_6 = 2,
	TCC_ASRC_NUM_OF_CH_8 = 3,
};

enum tcc_asrc_peri_t {
	TCC_ASRC_PERI_DAI0 = 0,
	TCC_ASRC_PERI_DAI1 = 1,
	TCC_ASRC_PERI_DAI2 = 2,
	TCC_ASRC_PERI_DAI3 = 3,
};

struct tcc_asrc_param_t {
	int pair;	//0 ~ 3
	union {
		struct {
			enum tcc_asrc_drv_bitwidth_t src_bitwidth;
			enum tcc_asrc_drv_bitwidth_t dst_bitwidth;
			enum tcc_asrc_drv_ch_t channels;
			uint32_t ratio_shift22; // | Int(31~22) | Fraction(21~0) |
		} cfg;
		struct {
			uint32_t *buf; // point of pcm data
			uint32_t size; // bytes
		} pcm;
		uint32_t volume_gain; // | Sign(23) | Int(22~20) | Fractionc(19~0) |
		struct { 
			uint32_t gain; // -0.125 * ramp_gain dB
			uint32_t up_time;
			uint32_t dn_time;
			uint32_t up_wait;
			uint32_t dn_wait;
		} volume_ramp;
		struct {
			enum tcc_asrc_drv_bitwidth_t src_bitwidth;
			enum tcc_asrc_drv_bitwidth_t dst_bitwidth;
			enum tcc_asrc_drv_ch_t cur_channels;
			enum tcc_asrc_drv_ch_t max_channels;
			uint32_t ratio_shift22; // | Int(31~22) | Fraction(21~0) |
			uint32_t started;
			uint32_t m2m_available;
		} info;
	} u;
};

#define IOCTL_TCC_ASRC_M2M_MAGIC				('S')
#define IOCTL_TCC_ASRC_M2M_START				_IO(IOCTL_TCC_ASRC_M2M_MAGIC, 0)
#define IOCTL_TCC_ASRC_M2M_STOP 				_IO(IOCTL_TCC_ASRC_M2M_MAGIC, 1)
#define IOCTL_TCC_ASRC_M2M_PUSH_PCM				_IO(IOCTL_TCC_ASRC_M2M_MAGIC, 2)
#define IOCTL_TCC_ASRC_M2M_POP_PCM				_IO(IOCTL_TCC_ASRC_M2M_MAGIC, 3)
#define IOCTL_TCC_ASRC_M2M_SET_VOL				_IO(IOCTL_TCC_ASRC_M2M_MAGIC, 4)
#define IOCTL_TCC_ASRC_M2M_SET_VOL_RAMP			_IO(IOCTL_TCC_ASRC_M2M_MAGIC, 5)
#define IOCTL_TCC_ASRC_M2M_GET_INFO				_IO(IOCTL_TCC_ASRC_M2M_MAGIC, 6)

#define IOCTL_TCC_ASRC_M2M_DUMP_REGS			_IO(IOCTL_TCC_ASRC_M2M_MAGIC, 98)
#define IOCTL_TCC_ASRC_M2M_DUMP_DMA_REGS		_IO(IOCTL_TCC_ASRC_M2M_MAGIC, 99)

#endif /*_TCC_ASRC_M2M_IOCTL_H_*/
