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

struct scmi_handle;

/**
 * struct scmi_perf_ops - represents the various operations provided
 *	by SCMI Performance Protocol
 *
 * @limits_set: sets limits on the performance level of a domain
 * @limits_get: gets limits on the performance level of a domain
 * @level_set: sets the performance level of a domain
 * @level_get: gets the performance level of a domain
 * @limits_notify_enable: requests notifications from the platform for changes
 *	in the allowed maximum and minimum performance levels
 * @level_notify_enable: requests notifications from the platform when the
 *	performance level for a domain changes in value
 */
struct scmi_perf_ops {
	int (*limits_set)(const struct scmi_handle *, u32, u32, u32);
	int (*limits_get)(const struct scmi_handle *, u32, u32 *, u32 *);
	int (*level_set)(const struct scmi_handle *, u32, u32);
	int (*level_get)(const struct scmi_handle *, u32, u32 *);
	int (*limits_notify_enable)(const struct scmi_handle *, u32, bool);
	int (*level_notify_enable)(const struct scmi_handle *, u32, bool);
	int (*device_domain_id)(struct device *);
	int (*get_transition_latency)(struct device *);
	int (*add_opps_to_device)(struct device *);
	int (*freq_set)(const struct scmi_handle *, u32, unsigned long);
	int (*freq_get)(const struct scmi_handle *, u32, unsigned long *);
};

/**
 * struct scmi_handle - Handle returned to ARM SCMI clients for usage.
 *
 * @dev: pointer to the SCMI device
 * @version: pointer to the structure containing SCMI version information
 * @perf_ops: pointer to set of performance protocol operations
 */
struct scmi_handle {
	struct device *dev;
	struct scmi_revision_info *version;
	struct scmi_perf_ops *perf_ops;
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
