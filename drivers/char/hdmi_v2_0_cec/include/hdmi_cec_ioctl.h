
/****************************************************************************
hdmi_cec

Copyright (C) 2018 Telechips Inc.
****************************************************************************/
#ifndef _TCC_HDMI_V2_0_CEC_H_
#define _TCC_HDMI_V2_0_CEC_H_

#define CEC_IOC_MAGIC        'c'

/**
 * CEC device request code to set logical address.
 */
#define CEC_IOC_SETLADDR     _IOW(CEC_IOC_MAGIC, 0, unsigned int)
#define CEC_IOC_SENDDATA         _IOW(CEC_IOC_MAGIC, 1, unsigned int)
#define CEC_IOC_RECVDATA         _IOW(CEC_IOC_MAGIC, 2, unsigned int)

#define CEC_IOC_START    _IO(CEC_IOC_MAGIC, 3)
#define CEC_IOC_STOP     _IO(CEC_IOC_MAGIC, 4)

#define CEC_IOC_GETRESUMESTATUS _IOR(CEC_IOC_MAGIC, 5, unsigned int)
#define CEC_IOC_CLRRESUMESTATUS _IOW(CEC_IOC_MAGIC, 6, unsigned int)

#define CEC_IOC_STATUS 			_IOW(CEC_IOC_MAGIC, 7, unsigned int)
#define CEC_IOC_CLEARLADDR     _IOR(CEC_IOC_MAGIC, 8, unsigned int)
#define CEC_IOC_SETPADDR     _IOR(CEC_IOC_MAGIC, 9, unsigned int)

#define CEC_IOC_BLANK                      _IOW(CEC_IOC_MAGIC,50, unsigned int)

#endif /* _TCC_HDMI_V2_0_CEC_H_ */