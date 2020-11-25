/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) Telechips Inc.
 *
 */

#ifndef __HDCP_INTERFACE_H
#define __HDCP_INTERFACE_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define HDCP_IOC_MAGIC			(0xdc)
#define HDCP_IOC_BASE			(0x00)

#define HDCP_PROTOCAL_NONE		(0x00)
#define HDCP_PROTOCAL_HDCP_1_X		(0x01)
#define HDCP_PROTOCAL_HDCP_2_2		(0x02)

#define HDCP_CONTENT_TYPE0		(0x00)
#define HDCP_CONTENT_TYPE1		(0x01)

enum {
	HDCP_STATUS_IDLE = 0,
	HDCP_STATUS_1_x_AUTENTICATED = 0x10,
	HDCP_STATUS_1_x_AUTHENTICATION_FAILED,
	HDCP_STATUS_1_x_ENCRYPTED,
	HDCP_STATUS_1_x_ENCRYPT_DISABLED,
	HDCP_STATUS_2_2_AUTHENTICATED = 0x20,
	HDCP_STATUS_2_2_AUTHENTICATION_FAILED,
	HDCP_STATUS_2_2_ENCRYPTED,
	HDCP_STATUS_2_2_ENCRYPT_DISABLED,
};

/**
 * HDCP_START_AUTHENTICATION
 *
 * Start the HDCP authentication process.
 * The HDCP configuration has been set using ioctl HDCP_CONFIGURE.
 */
#define HDCP_START_AUTHENTICATION	_IO(HDCP_IOC_MAGIC, HDCP_IOC_BASE + 0)

/**
 * HDCP_STOP_AUTHENTICATION
 *
 * Stop the HDCP authentication of the connected display.
 */
#define HDCP_STOP_AUTHENTICATION	_IO(HDCP_IOC_MAGIC, HDCP_IOC_BASE + 1)

/**
 * struct HDCPBcaps
 * @bcaps:	[out] The Bcaps register value.
 * @timeout:	[in] Timeout for the operation in milliseconds.
 */
struct HDCPBcaps {
	__u8 bcaps;
	__u32 timeout;
};

/**
 * HDCP_GET_BCAPS
 *
 * Attempt to retrieve the HDCP Bcaps register form the connected display
 * device. This function shall time out if the connected display device does not
 * support HDCP.
 */
#define HDCP_GET_BCAPS		_IOWR(HDCP_IOC_MAGIC, HDCP_IOC_BASE + 2, \
				      struct HDCPBcaps)

/**
 * struct HDCPBstatus
 * @bstatus:	[out] The Bstatus register value.
 * @timeout:	[in] Timeout for the operation in milliseconds.
 */
struct HDCPBstatus {
	__u16 bstatus;
	__u32 timeout;
};

/**
 * HDCP_GET_BSTATUS
 *
 * Attempt to retrieve the HDCP Bstatus register form the connected display
 * device. This function shall time out if the connected display device does not
 * support HDCP.
 */
#define HDCP_GET_BSTATUS	_IOWR(HDCP_IOC_MAGIC, HDCP_IOC_BASE + 3, \
				      struct HDCPBstatus)

/**
 * struct HDCPSrm
 * @length:	[in] On entry, indicates the number of bytes in the system
 *		     renewability message buffer pointed to by the data.
 * @data:	[in] A pointer to memory containing length of system
 *		     renewability message data.
 */
struct HDCPSrm {
	__u32 length;
	__u8 *data;
};

/**
 * HDCP_SEND_SRM_LIST
 *
 * Pass an SRM to the device.
 */
#define HDCP_SEND_SRM_LIST	_IOW(HDCP_IOC_MAGIC, HDCP_IOC_BASE + 4, \
				     struct HDCPSrm)

/**
 * HDCP_ENCRYPTION_CONTROL
 *
 * Enable ro disable HDCP encryption for this device.
 * @__u32:	[in] If 1 then enable encryption.
 *		     If 0 then disable encryption.
 */
#define HDCP_ENCRYPTION_CONTROL	_IOW(HDCP_IOC_MAGIC, HDCP_IOC_BASE + 5, \
				     __u32)

/**
 * struct HDCPStatus
 * @status:	[out] Current HDCP sttatus value of this device.
 */
struct HDCPStatus {
	__u8 status;
};

/**
 * HDCP_GET_STATUS
 *
 * Query the current HDCP transmitter link state of this device.
 */
#define HDCP_GET_STATUS		_IOR(HDCP_IOC_MAGIC, HDCP_IOC_BASE + 6, \
				     struct HDCPStatus)

/**
 * struct HDCPConfiguration
 * @encryptionStartTime:[in] The time in milliseconds that the device shall wait
 *			     between completing the first phase of
 *			     authentication and starting encryption on the link.
 * @checkPjIntegrity:	[in] If 1 then enable Pj link integrity checking.
 *			     If 0 then disable Pj link integrity checking.
 */
struct HDCPConfiguration {
	__u32 encryptionStartTime;
	__u8 checkPjIntegrity;
};

/**
 * HDCP_CONFIGURE
 *
 * Configure the HDCP parameters that the device is to use for HDCP
 * authentication and operation.
 */
#define HDCP_CONFIGURE		_IOW(HDCP_IOC_MAGIC, HDCP_IOC_BASE + 7, \
				     struct HDCPConfiguration)

/**
 * HDCP_SET_PROTOCAL
 *
 * Set the HDCP protocal to be used.
 * @__u32:	[in] This shall be one of the HDCP_PROTOCAL_* constants.
 */
#define HDCP_SET_PROTOCAL	_IOW(HDCP_IOC_MAGIC, HDCP_IOC_BASE + 8, \
				     __u32)

/**
 * struct HDCP2Version
 * @version:	[out] The HDCP2Version register value.
 * @timeOut:	[in] Timeout for the operation in milliseconds.
 */
struct HDCP2Version {
	__u8 version;
	__u32 timeout;
};

/**
 * HDCP_GET_HDCP2VERSION
 *
 * Attempt to retrieve the HDCP2Version register from the connected display
 * device. This function shall time out if the connected dispaly device does not
 * support HDCP.
 * This ioctl shall be implemented if the device reports HDMI_CAP_V2 extended
 * capabilities or greater and implementation of the HDCP specification 2.2 is
 * also reported in the device capabilities.
 */
#define HDCP_GET_HDCP2VERSION	_IOWR(HDCP_IOC_MAGIC, HDCP_IOC_BASE + 9, \
				      struct HDCP2Version)

/**
 * HDCP_SET_CONTENT_STREAM_TYPE
 *
 * Set the Content Stream Type value.
 * This function shall be implemented if the device reports HDMI_CAP_V2 extended
 * capabilities or greater and implementation of the HDCP specification 2.2 is
 * also reported in the device capabilities.
 * @__u32:	[in] This shall be one of the HDCP_CONTENT_TYPE? constants.
 */
#define HDCP_SET_CONTENT_STREAM_TYPE	_IOW(HDCP_IOC_MAGIC, \
					     HDCP_IOC_BASE + 10, \
					     __u32)


#endif /* __HDCP_INTERFACE_H */
