/****************************************************************************
Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA

NOTE: Tab size is 8
****************************************************************************/
#ifndef _TCC_HDMI_V2_0_CEC_H_
#define _TCC_HDMI_V2_0_CEC_H_

#define CEC_IOC_MAGIC        'c'

/**
 * CEC device request code to set logical address.
 */
#define CEC_IOC_SETLADDR        _IOW(CEC_IOC_MAGIC, 0x00, unsigned int)
#define CEC_IOC_SENDDATA        _IOW(CEC_IOC_MAGIC, 0x01, unsigned int)
#define CEC_IOC_RECVDATA        _IOW(CEC_IOC_MAGIC, 0x02, unsigned int)

#define CEC_IOC_START           _IO(CEC_IOC_MAGIC, 0x03)
#define CEC_IOC_STOP            _IO(CEC_IOC_MAGIC, 0x04)
#define CEC_IOC_GETRESUMESTATUS _IOR(CEC_IOC_MAGIC, 0x05, unsigned int)
#define CEC_IOC_CLRRESUMESTATUS _IOW(CEC_IOC_MAGIC, 0x06, unsigned int)

#define CEC_IOC_STATUS 		_IOW(CEC_IOC_MAGIC, 0x07, unsigned int)
#define CEC_IOC_CLEARLADDR      _IOR(CEC_IOC_MAGIC, 0x08, unsigned int)
#define CEC_IOC_SETPADDR        _IOR(CEC_IOC_MAGIC, 0x09, unsigned int)

#define CEC_IOC_SET_WAKEUP      _IOW(CEC_IOC_MAGIC,0x10, int)

#define CEC_IOC_BLANK           _IOW(CEC_IOC_MAGIC, 0x32, unsigned int)

/* New Interfaces */
#define MAX_CEC_BUFFER          16
struct cec_transmit_data {
        int size;
        unsigned char buff[MAX_CEC_BUFFER];
};

#define CEC_IOC_SENDDATA_EX             _IOW(CEC_IOC_MAGIC, 0x81, struct cec_transmit_data)
#define CEC_IOC_RECVDATA_EX             _IOW(CEC_IOC_MAGIC, 0x82, struct cec_transmit_data)
#define CEC_IOC_GETRESUMESTATUS_EX      _IOR(CEC_IOC_MAGIC, 0x85, unsigned int)

#endif /* _TCC_HDMI_V2_0_CEC_H_ */
