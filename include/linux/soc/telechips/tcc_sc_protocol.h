// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/mailbox/mailbox-tcc.h>

#ifndef TCC_SC_PROTOCOL
#define TCC_SC_PROTOCOL


#define TCC_SC_MMC_DATA_READ		(1)
#define TCC_SC_MMC_DATA_WRITE		(2)
#define TCC_SC_FW_INFO_DESC_SIZE	(16) /* in byte */

struct tcc_sc_fw_handle;

struct tcc_sc_fw_xfer {
	struct tcc_sc_mbox_msg tx_mssg;
	u32 tx_cmd_buf_len;
	u32 tx_data_buf_len;
	struct tcc_sc_mbox_msg rx_mssg;
	u32 rx_cmd_buf_len;
	u32 rx_data_buf_len;

	struct list_head node;
	spinlock_t lock;

	u8	status;
#define TCC_SC_FW_XFER_STAT_IDLE	(0x0)
#define TCC_SC_FW_XFER_STAT_TX_PEND	(0x1)
#define TCC_SC_FW_XFER_STAT_TX_START	(0x2)
#define TCC_SC_FW_XFER_STAT_RX_PEND	(0x3)
#define TCC_SC_FW_XFER_STAT_HALT	(0x4)

	void (*complete)(void *args, void *msg);
	void *args;
};

struct tcc_sc_fw_prot_mmc {
	u32 max_segs;
	u32 max_seg_len;
	u32 blk_size;
	u32 max_blk_num;
	u8 speed_mode;
	u8 bus_width;
	u16 reserved;
	u32 clock;
} __attribute__ ((__packed__));

struct tcc_sc_fw_version {
	u16 major;
	u16 minor;
	u32 patch;
	s8 desc[TCC_SC_FW_INFO_DESC_SIZE];
};

struct tcc_sc_fw_mmc_cmd {
	u32			opcode;
	u32			arg;
	u32			resp[4];
	u32			flags;
	s32			error;
	u32			part_num;
};

struct tcc_sc_fw_mmc_data {
	u32		blksz;
	u32		blocks;
	u32		blk_addr;
	s32		error;
	u32		flags;

	u32		sg_len;
	s32		sg_count;
	struct scatterlist	*sg;
};

struct tcc_sc_fw_ufs_cmd {
	u32		datsz;
	u32		blocks;
	u32		lba;
	s32		error;
	u32		dir;
	u32		lun;
	u32		tag;
	u32		op;
	u32		cdb;

	u32		cdb0;
	u32		cdb1;
	u32		cdb2;
	u32		cdb3;

	s32		sg_count;
	struct scatterlist	*sg;
	u32		resp[4];
};

struct tcc_sc_fw_otp_cmd {
	u32		cmd;
	u32		resp[5];
};

struct tcc_sc_fw_mmc_ops {
	void* (*request_command)(const struct tcc_sc_fw_handle *handle,
		struct tcc_sc_fw_mmc_cmd *cmd, struct tcc_sc_fw_mmc_data *data,
		void (*complete)(void *, void *), void *args);
	s32 (*prot_info)(const struct tcc_sc_fw_handle *handle,
		struct tcc_sc_fw_prot_mmc *mmc_info);
	void (*halt_cmd)(const struct tcc_sc_fw_handle *handle,
		void *xfer);
};

struct tcc_sc_fw_ufs_ops {
	s32 (*request_command)(const struct tcc_sc_fw_handle *handle,
		struct tcc_sc_fw_ufs_cmd *cmd,
		void (*complete)(void *, void *), void *args);

};

struct tcc_sc_fw_gpio_ops {
	s32 (*request_gpio)(const struct tcc_sc_fw_handle *handle,
		uint32_t address, uint32_t bit_number,
		uint32_t width, uint32_t value);
	s32 (*request_gpio_no_res)(const struct tcc_sc_fw_handle *handle,
		uint32_t address, uint32_t bit_number,
		uint32_t width, uint32_t value);
};

struct tcc_sc_fw_otp_ops {
	s32 (*get_otp)(const struct tcc_sc_fw_handle *handle,
		struct tcc_sc_fw_otp_cmd *cmd, uint32_t offset);
};

struct tcc_sc_fw_ops {
	struct tcc_sc_fw_mmc_ops *mmc_ops;
	struct tcc_sc_fw_ufs_ops *ufs_ops;
	struct tcc_sc_fw_gpio_ops *gpio_ops;
	struct tcc_sc_fw_otp_ops *otp_ops;
};

struct tcc_sc_fw_handle {
	struct tcc_sc_fw_version version;
	struct tcc_sc_fw_ops ops;
	void *priv;
};

const struct tcc_sc_fw_handle *tcc_sc_fw_get_handle(struct device_node *np);

#endif /* __TCC_SC_PROTOCOL__ */
