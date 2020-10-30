// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __TCC_SC_PROTOCOL__
#define __TCC_SC_PROTOCOL__

#define TCC_SC_MMC_DATA_READ	1
#define TCC_SC_MMC_DATA_WRITE	2
#define TCC_SC_FW_INFO_DESC_SIZE	16 /* in byte */

struct tcc_sc_fw_handle;

struct tcc_sc_fw_prot_mmc {
	u32 max_segs;
	u32 max_seg_len;
	u32 blk_size;
	u32 max_blk_num;
};

struct tcc_sc_fw_version {
	u16 major;
	u16 minor;
	u32 patch;
	char desc[TCC_SC_FW_INFO_DESC_SIZE];
};

struct tcc_sc_fw_mmc_cmd {
	u32			opcode;
	u32			arg;
	u32			resp[4];
	unsigned int		flags;
	int			error;

	unsigned int part_num;
};

struct tcc_sc_fw_mmc_data {
	unsigned int		blksz;
	unsigned int		blocks;
	unsigned int		blk_addr;
	int			error;
	unsigned int		flags;

	unsigned int		sg_len;
	int			sg_count;
	struct scatterlist	*sg;
};

struct tcc_sc_fw_ufs_cmd {
	unsigned int		datsz;
	unsigned int		blocks;
	unsigned int		lba;
	int			error;
	unsigned int		dir;
	unsigned int		lun;
	unsigned int		tag;
	unsigned int		op;
	unsigned int		cdb;

	unsigned int		cdb0;
	unsigned int		cdb1;
	unsigned int		cdb2;
	unsigned int		cdb3;

	int			sg_count;
	struct scatterlist	*sg;
	unsigned int		resp[4];
};

struct tcc_sc_fw_mmc_ops {
	int (*request_command)(const struct tcc_sc_fw_handle *handle,
			struct tcc_sc_fw_mmc_cmd *cmd, struct tcc_sc_fw_mmc_data *data);
	int (*prot_info)(const struct tcc_sc_fw_handle *handle,
		struct tcc_sc_fw_prot_mmc *mmc_info);
};

struct tcc_sc_fw_ufs_ops {
	int (*request_command)(const struct tcc_sc_fw_handle *handle,
			struct tcc_sc_fw_ufs_cmd *cmd);
};

struct tcc_sc_fw_gpio_ops {
	int (*request_gpio)(const struct tcc_sc_fw_handle *handle,
			uint32_t address, uint32_t bit_number, uint32_t width, uint32_t value);
	int (*request_gpio_multi)(const struct tcc_sc_fw_handle *handle, struct tcc_sc_gpio_req_data* gpio_req_data);
};

struct tcc_sc_fw_ops {
	struct tcc_sc_fw_mmc_ops *mmc_ops;
	struct tcc_sc_fw_ufs_ops *ufs_ops;
	struct tcc_sc_fw_gpio_ops *gpio_ops;
};

struct tcc_sc_fw_handle {
	struct tcc_sc_fw_version version;
	struct tcc_sc_fw_ops ops;
	void *priv;
};

const struct tcc_sc_fw_handle *tcc_sc_fw_get_handle(struct device_node *np);

#endif /* __TCC_SC_PROTOCOL__ */
