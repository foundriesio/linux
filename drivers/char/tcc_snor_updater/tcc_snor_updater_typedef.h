/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef TCC_SNOR_UPDATER_TYPEDEF_H
#define TCC_SNOR_UPDATER_TYPEDEF_H

#define LOG_TAG ("SNOR_UPDATE_DRV")

#define MAX_FW_BUF_SIZE		(256)

#define SNOR_UPDATER_SUCCESS					(0)		/* Success */
#define SNOR_UPDATER_ERR_COMMON					(-1)		/* common error*/
#define SNOR_UPDATER_ERR_ARGUMENT				(-2)		/* Invalid argument */
#define SNOR_UPDATER_ERR_NOTREADY				(-3)		/* MBOX is not ready. Other processor is not ready yet.*/
#define SNOR_UPDATER_ERR_TIMEOUT				(-4)		/* Other process is not responding. */
#define SNOR_UPDATER_ERR_UNKNOWN_CMD			(-5)
#define SNOR_UPDATER_ERR_NACK					(-6)
#define SNOR_UPDATER_ERR_SNOR_FIRMWARE_FAIL		(-7)
#define SNOR_UPDATER_ERR_SNOR_INIT_FAIL			(-8)
#define SNOR_UPDATER_ERR_SNOR_ACCESS_FAIL		(-9)
#define SNOR_UPDATER_ERR_CRC_ERROR				(-10)

typedef enum {
	UPDATE_START =0x0001,
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
}UpdateCmdType;

typedef struct _snor_updater_wait_queue{
	wait_queue_head_t _cmdQueue;
	int _condition;
	unsigned int reqeustCMD;
	struct tcc_mbox_data receiveMsg;
}snor_updater_wait_queue;


struct snor_updater_device
{
	struct platform_device	*pdev;
	struct device *dev;
	struct cdev cdev;
	struct class *class;
	dev_t devnum;
	const char *mbox_name;
	struct mbox_chan *mbox_ch;
	int	isOpened;
	struct mutex devMutex;			/* common mutex*/
	snor_updater_wait_queue waitQueue;
 };

typedef char update_char;

extern int updater_verbose_mode;

#define eprintk(dev, msg, ...)	dev_err(dev, "[ERROR][%s]%s: " pr_fmt(msg), (const update_char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define wprintk(dev, msg, ...)	dev_warn(dev, "[WARN][%s]%s: " pr_fmt(msg), (const update_char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define iprintk(dev, msg, ...)	dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), (const update_char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define dprintk(dev, msg, ...)	do { if(updater_verbose_mode == 1) { dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), (const update_char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__); } } while(0)

#endif
