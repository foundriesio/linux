// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */


#ifndef TCC_SNOR_UPDATER_TYPEDEF_H
#define TCC_SNOR_UPDATER_TYPEDEF_H

#ifndef char_t
typedef char char_t;
#endif

#ifndef long_t
typedef long long_t;
#endif


#define LOG_TAG ("SNOR_UPDATE_DRV")

#define MAX_FW_BUF_SIZE		(256U)

#define SNOR_UPDATER_SUCCESS					(0)
#define SNOR_UPDATER_ERR_COMMON					(-1)
#define SNOR_UPDATER_ERR_ARGUMENT				(-2)
#define SNOR_UPDATER_ERR_NOTREADY				(-3)
#define SNOR_UPDATER_ERR_TIMEOUT				(-4)
#define SNOR_UPDATER_ERR_UNKNOWN_CMD			(-5)
#define SNOR_UPDATER_ERR_NACK					(-6)
#define SNOR_UPDATER_ERR_SNOR_FIRMWARE_FAIL		(-7)
#define SNOR_UPDATER_ERR_SNOR_INIT_FAIL			(-8)
#define SNOR_UPDATER_ERR_SNOR_ACCESS_FAIL		(-9)
#define SNOR_UPDATER_ERR_CRC_ERROR				(-10)

enum {
	UPDATE_START = 0x0001,
	UPDATE_READY,
	UPDATE_FW_START,
	UPDATE_FW_READY,
	UPDATE_FW_SEND,
	UPDATE_FW_SEND_ACK,
	UPDATE_FW_DONE,
	UPDATE_FW_COMPLETE,
	UPDATE_DONE,
	UPDATE_COMPLETE,
	MAX_UPDATE_CMD_TYPE,
};

struct snor_updater_wait_queue {
	wait_queue_head_t _cmdQueue;
	int32_t _condition;
	uint32_t reqeustCMD;
	struct tcc_mbox_data receiveMsg;
};

struct snor_updater_device {
	struct platform_device	*pdev;
	struct device *dev;
	struct cdev cdev;
	struct class *class;
	dev_t devnum;
	const char_t *mbox_name;
	struct mbox_chan *mbox_ch;
	int32_t	isOpened;
	struct mutex devMutex;
	struct snor_updater_wait_queue waitQueue;
};

extern int32_t updater_verbose_mode;

#define eprintk(dev, msg, ...)	\
	(dev_err(dev, "[ERROR][%s]%s: " pr_fmt(msg), \
	(const char_t *)LOG_TAG, __func__, ##__VA_ARGS__))
#define wprintk(dev, msg, ...)	\
	(dev_warn(dev, "[WARN][%s]%s: " pr_fmt(msg), \
	(const char_t *)LOG_TAG, __func__, ##__VA_ARGS__))
#define iprintk(dev, msg, ...)	\
	(dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), \
	(const char_t *)LOG_TAG, __func__, ##__VA_ARGS__))
#define dprintk(dev, msg, ...)	\
	{ if (updater_verbose_mode == 1) \
	{ dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), \
	(const char_t *)LOG_TAG, __func__, ##__VA_ARGS__); } }

#endif
