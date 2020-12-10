/*
 * Copyright (C) 20010-2020 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef HDCP_SS_H
#define HDCP_SS_H

#define HDCP_IOC_MAGIC 'p'

/*
 * @enum hdcp_event
 * HDCP event to be used for HDCP processing
 */
enum hdcp_event {
	/** Stop HDCP  */
	HDCP_EVENT_STOP = 0,
	/** Start to read Bksv,Bcaps */
	HDCP_EVENT_READ_BKSV_START,
	/** Start to write Aksv,An */
	HDCP_EVENT_WRITE_AKSV_START,
	/** Start to check if Ri is equal to Rj */
	HDCP_EVENT_CHECK_RI_START,
	/** Start 2nd authentication process */
	HDCP_EVENT_SECOND_AUTH_START,
	/** Number of HDCP Event */
	HDCP_EVENT_MAXS,
};
/** Size of KSV */
#define HDCP_KSV_SIZE 5
/** Size of Bstatus */
#define HDCP_STATUS_SIZE 2
/** Size of Ri */
#define HDCP_RI_SIZE 2
/** Size of An */
#define HDCP_AN_SIZE 8
/** Size of SHA1 result */
#define HDCP_SHA1_SIZE 20

/**
 * @struct hdcp_ksv
 * Contains KSV(Key Selection Vector).
 */
struct hdcp_ksv {
	/** KSV */
	unsigned char ksv[HDCP_KSV_SIZE];
};

/**
 * @struct hdcp_status
 * Contains Bstatus.
 */
struct hdcp_status {
	/** Status */
	unsigned char status[HDCP_STATUS_SIZE];
};

/**
 * @struct hdcp_an
 * Contains An(64-bit pseudo-random value).
 */
struct hdcp_an {
	/** An */
	unsigned char an[HDCP_AN_SIZE];
};

/**
 * @struct hdcp_ksv_list
 * Contains one KSV and flag that indicates whether this is the last KSV @n
 * in KSV list or not
 */
struct hdcp_ksv_list {
	/**
	 * Flag that indicates structure contains the last KSV in KSV list. @n
	 * If so, 1; otherwise, 0.
	 */
	unsigned char end;
	/** KSV */
	unsigned char ksv[HDCP_KSV_SIZE];
};

/**
 * @struct hdcp_sha1
 * Contains SHA1 calculated result.
 */
struct hdcp_sha1 {
	/** SHA1 calculated result */
	unsigned char sha1[HDCP_SHA1_SIZE];
};

struct hdcp_ri_info {
	unsigned char ri[2];
	unsigned char rj[2];
	int result;
};

/*** IOW ***/
/**
 * Device requset code to enable/disable HDCP H/W @n
 * 1 to enable, 0 to disable
 */
#define HDCP_IOC_ENABLE_HDCP _IOW(HDCP_IOC_MAGIC, 11, unsigned char)

/** Device requset code to set Bksv  */
#define HDCP_IOC_SET_BKSV _IOW(HDCP_IOC_MAGIC, 12, struct hdcp_ksv)

/** Device requset code to set Bcaps  */
#define HDCP_IOC_SET_BCAPS _IOW(HDCP_IOC_MAGIC, 13, unsigned char)

/**
 * Device requset code to start/stop sending EESS @n
 * 1 to start, 0 to stop
 */
#define HDCP_IOC_SET_ENCRYPTION _IOW(HDCP_IOC_MAGIC, 25, unsigned char)

/**
 * Device requset code to set the result whether Ri and Ri' are match or not@n
 * set 1 if match; otherwise set 0
 */
#define HDCP_IOC_SET_HDCP_RESULT _IOW(HDCP_IOC_MAGIC, 14, unsigned char)

/** Device requset code to set BStatus  */
#define HDCP_IOC_SET_BSTATUS _IOW(HDCP_IOC_MAGIC, 15, struct hdcp_status)

/** Device requset code to set KSV list  */
#define HDCP_IOC_SET_KSV_LIST _IOW(HDCP_IOC_MAGIC, 16, struct hdcp_ksv_list)

/** Device requset code to set Rx SHA1 calculated result  */
#define HDCP_IOC_SET_SHA1 _IOW(HDCP_IOC_MAGIC, 17, struct hdcp_sha1)

#define HDCP_IOC_BLANK _IOW(HDCP_IOC_MAGIC, 50, unsigned int)

#define HDCP_SEC_IOCTL_SET_KEY	_IOW(HDCP_IOC_MAGIC,101,void *)


/*** IOR ***/
/** Device requset code to get hdcp_event */
#define HDCP_IOC_GET_HDCP_EVENT _IOR(HDCP_IOC_MAGIC, 1, enum hdcp_event)

/** Device requset code to get Aksv */
#define HDCP_IOC_GET_AKSV _IOR(HDCP_IOC_MAGIC, 2, struct hdcp_ksv)

/** Device requset code to get An */
#define HDCP_IOC_GET_AN _IOR(HDCP_IOC_MAGIC, 3, struct hdcp_an)

/**
 * Device requset code to get Ri @n
 * The length of Ri is 2 bytes. Ri is contained in LSB 2 bytes.
 */
//#define HDCP_IOC_GET_RI                     _IOR(HDCP_IOC_MAGIC,4,int)
#define HDCP_IOC_GET_RI _IOR(HDCP_IOC_MAGIC, 4, struct hdcp_ri_info)

/**
 * Device request code to get SHA1 result
 * which is the comparison between Rx and Tx. @n
 * 1 if match; 0 if not matched
 */
#define HDCP_IOC_GET_SHA1_RESULT _IOR(HDCP_IOC_MAGIC, 5, int)

/**
 * Device request code to get the state of 1st part of HDCP authentication @n
 * 1 if authenticated; 0 if not authenticated
 */
#define HDCP_IOC_GET_AUTH_STATE _IOR(HDCP_IOC_MAGIC, 6, int)

/**
 * Device request code to get whether HDCP H/W finishes reading KSV or not @n
 * 1 if H/W finishes reading KSV; otherwise 0
 */
#define HDCP_IOC_GET_KSV_LIST_READ_DONE _IOR(HDCP_IOC_MAGIC, 7, unsigned char)

#define HDCP_IOC_GET_RI_REG _IOR(HDCP_IOC_MAGIC, 100, int)


/*** IO ***/
/** Device requset code to start processing HDCP */
#define HDCP_IOC_START_HDCP _IO(HDCP_IOC_MAGIC, 2)

/** Device requset code to stop processing HDCP */
#define HDCP_IOC_STOP_HDCP _IO(HDCP_IOC_MAGIC, 3)

/** Device requset code to notify that one of downstream receivers is illegal */
#define HDCP_IOC_SET_ILLEGAL_DEVICE _IO(HDCP_IOC_MAGIC, 4)

/** Device requset code to notify that repeater is not ready within 5 secs */
#define HDCP_IOC_SET_REPEATER_TIMEOUT _IO(HDCP_IOC_MAGIC, 5)

/** Device requset code to reset HDCP H/W  */
#define HDCP_IOC_RESET_HDCP _IO(HDCP_IOC_MAGIC, 6)

/**
 * Device requset code to notify if Tx is repeater, @n
 * But it has no downstream receiver.
 */
#define HDCP_IOC_SET_KSV_LIST_EMPTY _IO(HDCP_IOC_MAGIC, 7)


#define hdcp_readb hdmi_api_reg_read
#define hdcp_writeb hdmi_api_reg_write
#define hdcp_readl hdmi_api_ddi_reg_read
#define hdcp_writel hdmi_api_ddi_reg_write

#endif /* HDCP_SS_H */
