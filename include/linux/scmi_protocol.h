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

/**
 * struct scmi_handle - Handle returned to ARM SCMI clients for usage.
 *
 * @dev: pointer to the SCMI device
 */
struct scmi_handle {
	struct device *dev;
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
