/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) Telechips Inc.
 * (C) COPYRIGHT 2014 - 2015 SYNOPSYS, INC.
 */

#ifndef ESM_HLD_LINUX_IOCTL_H_
#define ESM_HLD_LINUX_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>
#include "../../../hdmi_v2_0/include/hdmi_ioctls.h"

extern struct hdmi_tx_dev *dwc_hdmi_api_get_dev(void);
extern int hdcpAuthGetStatus(struct hdmi_tx_dev *dev);
extern u8 hdcp1p4SwitchSet(struct hdmi_tx_dev *dev);
extern u8 hdcp2p2SwitchSet(struct hdmi_tx_dev *dev);
extern void mc_hdcp_clock_enable(struct hdmi_tx_dev *dev, u8 bit);
extern void hdcp1p4Start(struct hdmi_tx_dev *dev, hdcpParams_t *hdcp);
extern void hdcp1p4Stop(struct hdmi_tx_dev *dev);
extern int hdcp1p4Configure(struct hdmi_tx_dev *dev, hdcpParams_t *hdcp);
extern u8 _HDCP22RegMask(struct hdmi_tx_dev *dev, u8 value);
extern u8 _HDCP22RegMute(struct hdmi_tx_dev *dev, u8 value);
extern void fc_force_output(struct hdmi_tx_dev *dev, int enable);
extern void hdcp2p2Stop(struct hdmi_tx_dev *dev);
extern void hdcp_disable_encryption(struct hdmi_tx_dev *dev, int disable);
extern void hdmi_api_avmute(struct hdmi_tx_dev *dev, int enable);

// ioctl numbers
enum {
	ESM_NR_MIN = 0x10,
	ESM_NR_INIT,
	ESM_NR_MEMINFO,
	ESM_NR_LOAD_CODE,
	ESM_NR_READ_DATA,
	ESM_NR_WRITE_DATA,
	ESM_NR_MEMSET_DATA,
	ESM_NR_READ_HPI,
	ESM_NR_WRITE_HPI,
	HDCP1_NR_INIT,
	HDCP1_NR_START,
	HDCP1_NR_STOP,
	HDCP_NR_GET_STATUS,
	HDCP2_NR_INIT,
	HDCP2_NR_STOP,
	HDCP_NR_SRM,
	ESM_NR_MAX
};

struct esm_ioc_meminfo {
	__u32 hpi_base;
	__u32 code_base;
	__u32 code_size;
	__u32 data_base;
	__u32 data_size;
};

struct esm_ioc_code {
	__u32 len;
	__u8 data[];
};

struct esm_ioc_data {
	__u32 offset;
	__u32 len;
	__u8 data[];
};

struct esm_ioc_hpi_reg {
	__u32 offset;
	__u32 value;
};

struct dwc_hdmi_hdcp_data {
	hdcpParams_t hdcpParam;
};

struct hdcp_ioc_data {
	__u32 status;
};

/*
 * ESM_IOC_INIT: associate file descriptor with the indicated memory.  This
 * must be called before any other ioctl on the file descriptor.
 *
 *   - hpi_base = base address of ESM HPI registers.
 *   - code_base = base address of firmware memory (0 to allocate internally)
 *   - data_base = base address of data memory (0 to allocate internally)
 *   - code_len, data_len = length of firmware and data memory, respectively.
 */
#define ESM_IOC_INIT _IOW('H', ESM_NR_INIT, struct esm_ioc_meminfo)

/*
 * ESM_IOC_MEMINFO: retrieve memory information from file descriptor.
 *
 * Fills out the meminfo struct, returning the values passed to ESM_IOC_INIT
 * except that the actual base addresses of internal allocations (if any) are
 * reported.
 */
#define ESM_IOC_MEMINFO _IOR('H', ESM_NR_MEMINFO, struct esm_ioc_meminfo)

/*
 * ESM_IOC_LOAD_CODE: write the provided buffer to the firmware memory.
 *
 *   - len = number of bytes in data buffer
 *   - data = data to write to firmware memory.
 *
 * This can only be done once (successfully).  Subsequent attempts will
 * return -EBUSY.
 */
#define ESM_IOC_LOAD_CODE _IOW('H', ESM_NR_LOAD_CODE, struct esm_ioc_code)

/*
 * ESM_IOC_READ_DATA: copy from data memory.
 * ESM_IOC_WRITE_DATA: copy to data memory.
 *
 *   - offset = start copying at this byte offset into the data memory.
 *   - len    = number of bytes to copy.
 *   - data   = for write, buffer containing data to copy.
 *              for read, buffer to which read data will be written.
 *
 */
#define ESM_IOC_READ_DATA _IOWR('H', ESM_NR_READ_DATA, struct esm_ioc_data)
#define ESM_IOC_WRITE_DATA _IOW('H', ESM_NR_WRITE_DATA, struct esm_ioc_data)

/*
 * ESM_IOC_MEMSET_DATA: initialize data memory.
 *
 *   - offset  = start initializatoin at this byte offset into the data memory.
 *   - len     = number of bytes to set.
 *   - data[0] = byte value to write to all indicated memory locations.
 */
#define ESM_IOC_MEMSET_DATA _IOW('H', ESM_NR_MEMSET_DATA, struct esm_ioc_data)

/*
 * ESM_IOC_READ_HPI: read HPI register.
 * ESM_IOC_WRITE_HPI: write HPI register.
 *
 *   - offset = byte offset of HPI register to access.
 *   - value  = for write, value to write.
 *              for read, location to which result is stored.
 */
#define ESM_IOC_READ_HPI _IOWR('H', ESM_NR_READ_HPI, struct esm_ioc_hpi_reg)
#define ESM_IOC_WRITE_HPI _IOW('H', ESM_NR_WRITE_HPI, struct esm_ioc_hpi_reg)

/*
 * HDCP1_INIT: initialize HDCP1 process.
 *
 *   - hdcpParams_t  = parameter configuring to prepare HDCP 1.4.
 */
#define HDCP1_INIT _IOW('H', HDCP1_NR_INIT, struct dwc_hdmi_hdcp_data)
#define HDCP1_START _IOW('H', HDCP1_NR_START, struct dwc_hdmi_hdcp_data)
#define HDCP1_STOP _IO('H', HDCP1_NR_STOP)
/*
 * HDCP_GET_STATUS: get HDCP authentication status.
 *
 *   - status  = HDCP authentication status.
 */
#define HDCP_GET_STATUS _IOWR('H', HDCP_NR_GET_STATUS, struct hdcp_ioc_data)
/*
 * HDCP2_INIT: initialize HDCP2 process(switched HDCP 2.2 in HDMI core).
 */
#define HDCP2_INIT _IO('H', HDCP2_NR_INIT)
#define HDCP2_STOP _IO('H', HDCP2_NR_STOP)

#define HDCP_SET_SRM	_IOW('H', HDCP_NR_SRM, struct esm_ioc_code)

#endif
