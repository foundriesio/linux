/*
 * SCMI Generic power domain support.
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
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/scmi_protocol.h>

struct scmi_pm_domain {
	struct generic_pm_domain genpd;
	const struct scmi_handle *handle;
	const char *name;
	u32 domain;
};

#define to_scmi_pd(gpd) container_of(gpd, struct scmi_pm_domain, genpd)

static int scmi_pd_power(struct generic_pm_domain *domain, bool power_on)
{
	int ret;
	u32 state, ret_state;
	struct scmi_pm_domain *pd = to_scmi_pd(domain);
	const struct scmi_power_ops *ops = pd->handle->power_ops;

	if (power_on)
		state = SCMI_POWER_STATE_GENERIC_ON;
	else
		state = SCMI_POWER_STATE_GENERIC_OFF;

	ret = ops->state_set(pd->handle, pd->domain, state);
	if (!ret)
		ret = ops->state_get(pd->handle, pd->domain, &ret_state);
	if (!ret && state != ret_state)
		return -EIO;

	return ret;
}

static int scmi_pd_power_on(struct generic_pm_domain *domain)
{
	return scmi_pd_power(domain, true);
}

static int scmi_pd_power_off(struct generic_pm_domain *domain)
{
	return scmi_pd_power(domain, false);
}

static int scmi_pm_domain_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct scmi_pm_domain *scmi_pd;
	struct genpd_onecell_data *scmi_pd_data;
	struct generic_pm_domain **domains;
	int num_domains, i;
	const struct scmi_handle *handle = devm_scmi_handle_get(dev);

	if (IS_ERR_OR_NULL(handle) || !handle->power_ops)
		return -EPROBE_DEFER;

	num_domains = handle->power_ops->num_domains_get(handle);
	if (num_domains < 0) {
		dev_err(dev, "number of domains not found\n");
		return num_domains;
	}

	scmi_pd = devm_kcalloc(dev, num_domains, sizeof(*scmi_pd), GFP_KERNEL);
	if (!scmi_pd)
		return -ENOMEM;

	scmi_pd_data = devm_kzalloc(dev, sizeof(*scmi_pd_data), GFP_KERNEL);
	if (!scmi_pd_data)
		return -ENOMEM;

	domains = devm_kcalloc(dev, num_domains, sizeof(*domains), GFP_KERNEL);
	if (!domains)
		return -ENOMEM;

	for (i = 0; i < num_domains; i++, scmi_pd++) {
		domains[i] = &scmi_pd->genpd;

		scmi_pd->domain = i;
		scmi_pd->handle = handle;
		scmi_pd->name = handle->power_ops->name_get(handle, i);
		scmi_pd->genpd.name = scmi_pd->name;
		scmi_pd->genpd.power_off = scmi_pd_power_off;
		scmi_pd->genpd.power_on = scmi_pd_power_on;

		/*
		 * Treat all power domains as off at boot.
		 *
		 * The SCP firmware itself may have switched on some domains,
		 * but for reference counting purpose, keep it this way.
		 */
		pm_genpd_init(&scmi_pd->genpd, NULL, true);
	}

	scmi_pd_data->domains = domains;
	scmi_pd_data->num_domains = num_domains;

	of_genpd_add_provider_onecell(np, scmi_pd_data);

	return 0;
}

static struct platform_driver scmi_power_domain_driver = {
	.driver	= {
		.name = "scmi-power-domain",
	},
	.probe = scmi_pm_domain_probe,
};
module_platform_driver(scmi_power_domain_driver);

MODULE_AUTHOR("Sudeep Holla <sudeep.holla@arm.com>");
MODULE_DESCRIPTION("ARM SCMI power domain driver");
MODULE_LICENSE("GPL v2");
