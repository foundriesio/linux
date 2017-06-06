/*
 * SCMI Message Protocol driver header
 *
 * Copyright (C) 2017 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/types.h>

#define SCMI_MAX_STR_SIZE	16

/**
 * struct scmi_revision_info - version information structure
 *
 * @major_ver: Major ABI version. Change here implies risk of backward
 *	compatibility break.
 * @minor_ver: Minor ABI version. Change here implies new feature addition,
 *	or compatible change in ABI.
 * @num_protocols: Number of protocols that are implemented, excluding the
 *	base protocol.
 * @num_agents: Number of agents in the system.
 * @impl_ver: A vendor-specific implementation version.
 * @vendor_id: A vendor identifier(Null terminated ASCII string)
 * @sub_vendor_id: A sub-vendor identifier(Null terminated ASCII string)
 */
struct scmi_revision_info {
	u16 major_ver;
	u16 minor_ver;
	u8 num_protocols;
	u8 num_agents;
	u32 impl_ver;
	char vendor_id[SCMI_MAX_STR_SIZE];
	char sub_vendor_id[SCMI_MAX_STR_SIZE];
};

/**
 * struct scmi_handle - Handle returned to ARM SCMI clients for usage.
 *
 * @dev: pointer to the SCMI device
 * @version: pointer to the structure containing SCMI version information
 */
struct scmi_handle {
	struct device *dev;
	struct scmi_revision_info *version;
};

#if IS_REACHABLE(CONFIG_ARM_SCMI_PROTOCOL)
int scmi_handle_put(const struct scmi_handle *handle);
const struct scmi_handle *scmi_handle_get(struct device *dev);
const struct scmi_handle *devm_scmi_handle_get(struct device *dev);
#else
static inline int scmi_handle_put(const struct scmi_handle *handle)
{
	return 0;
}

static inline const struct scmi_handle *scmi_handle_get(struct device *dev)
{
	return NULL;
}

static inline const struct scmi_handle *devm_scmi_handle_get(struct device *dev)
{
	return NULL;
}
#endif /* CONFIG_ARM_SCMI_PROTOCOL */
