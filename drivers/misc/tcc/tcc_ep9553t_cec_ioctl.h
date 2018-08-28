#ifndef _TCC_EP9553T_CEC_IOCTL_H_
#define _TCC_EP9553T_CEC_IOCTL_H_

#include <asm/ioctl.h>

//IOCTL
#define IOCTL_CEC_SET_PHYSICAL_ADDRESS		_IO('C', 0)
#define IOCTL_CEC_GET_PHYSICAL_ADDRESS		_IO('C', 1)

#define IOCTL_CEC_SET_DEVICE_TYPE1			_IO('C', 2)
#define IOCTL_CEC_GET_DEVICE_TYPE1			_IO('C', 3)
                                            
#define IOCTL_CEC_SET_DEVICE_TYPE2			_IO('C', 4)
#define IOCTL_CEC_GET_DEVICE_TYPE2			_IO('C', 5)
                                            
#define IOCTL_CEC_SET_POWER_STATUS			_IO('C', 6)
#define IOCTL_CEC_GET_POWER_STATUS			_IO('C', 7)

enum CEC_POWER_STATUS {
	CEC_POWER_STATUS_ON = 0,
	CEC_POWER_STATUS_STANDBY = 1,
	CEC_POWER_STATUS_TRANSITION_FROM_STANDBY_TO_ON = 2,
	CEC_POWER_STATUS_TRANSITION_FROM_ON_TO_STANDBY= 3,
};
                                            
#define IOCTL_CEC_SET_LANGUAGE				_IO('C', 8)
#define IOCTL_CEC_GET_LANGUAGE				_IO('C', 9)
                                            
#define IOCTL_CEC_SET_VENDOR_ID				_IO('C', 10)
#define IOCTL_CEC_GET_VENDOR_ID				_IO('C', 11)
                                            
#define IOCTL_CEC_SET_OSD_NAME				_IO('C', 12)
#define IOCTL_CEC_GET_OSD_NAME				_IO('C', 13)
                                            
#define IOCTL_CEC_SET_CURRENT_SOURCE		_IO('C', 14)
#define IOCTL_CEC_GET_CURRENT_SOURCE		_IO('C', 15)
                                            
#define IOCTL_CEC_GET_COLLECTED_INFO		_IO('C', 16)
#define IOCTL_CEC_GET_COLLECTED_OSD_NAME	_IO('C', 17)

#define IOCTL_CEC_ALLOC_LOGICAL_ADDRESS		_IO('C', 100)
#define IOCTL_CEC_TX_MSG					_IO('C', 101)
#define IOCTL_CEC_RX_MSG					_IO('C', 102)

#define CEC_TX_DATA_MAX			(14)
struct tcc_cec_msg_tx_t {
	char dest_addr;
	char size; // opcode + data
	char opcode;
	char data[CEC_TX_DATA_MAX];
} __attribute__((packed));

#define CEC_RX_DATA_MAX			(14)
struct tcc_cec_msg_rx_t {
	char src_addr;
	char size; //opcode + data
	char opcode;
	char data[CEC_RX_DATA_MAX];
	char msg_type;
} __attribute__((packed));

enum RX_MSG_TYPE {
	RX_MSG_TYPE_BROADCAST = 0x0F
};

#define OSD_NAME_MAX		(14)
#define CURRENT_SOURCE_MAX	(8)

enum CEC_DEV_TYPE {
	DEV_TYPE_TV	= 0,
	DEV_TYPE_RECORDING_DEVICE,
	DEV_TYPE_RESERVED,
	DEV_TYPE_TUNER,
	DEV_TYPE_PLAYBACK_DEVICE,
	DEV_TYPE_AUDIO_SYSTEM,
	DEV_TYPE_PURE_CEC_SWITCH,
	DEV_TYPE_VIDEO_PROCESSOR,
};

enum CEC_LOGICAL_ADDRESS {
	CEC_LA_TV 				= 0,
	CEC_LA_RECODING_DEVICE1	= 1,
	CEC_LA_RECODING_DEVICE2	= 2,
	CEC_LA_TUNER1 			= 3,
	CEC_LA_PLAYBACK_DEVICE1	= 4,
	CEC_LA_AUDIO_SYSTEM		= 5,
	CEC_LA_TUNER2 			= 6,
	CEC_LA_TUNER3 			= 7,
	CEC_LA_PLAYBACK_DEVICE2	= 8,
	CEC_LA_RECODING_DEVICE3	= 9,
	CEC_LA_TUNER4 			= 10,
	CEC_LA_PLAYBACK_DEVICE3	= 11,
	CEC_LA_RESERVED1		= 12,
	CEC_LA_RESERVED2		= 13,
	CEC_LA_SPECIFIC_USE		= 14,
	CEC_LA_UNREGISTERED		= 15,
};

struct cec_collected_info {
	unsigned int logical_addr; //param
	struct {
		unsigned char device_type;
		unsigned char hdmi_port;
		unsigned char reserved;
		unsigned char vendor_id[3];
		unsigned char power_status;
		union {
			char deck;
			char tuner[8];
			char audio;
		} u;
	} info;
} __attribute__((packed));

struct cec_collected_osd_name {
	unsigned int logical_addr; //param
	char osd_name[OSD_NAME_MAX]; //return
};

#endif /*_TCC_EP9553T_CEC_IOCTL_H_*/
