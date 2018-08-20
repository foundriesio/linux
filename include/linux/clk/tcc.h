/****************************************************************************
 * linux/clk/tcc.h
 * Copyright (C) 2014 Telechips Inc.
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

struct _tcc_clk_data {
	const char	*name;
	const char	*parent_name;
	unsigned int	idx;
	unsigned int	flags;
};

struct tcc_clks_type {
	const char	*parent_name;
	unsigned int	clks_num;
	struct _tcc_clk_data *data;
	unsigned int	data_num;
	unsigned int	common_flags;
};

struct tcc_ckc_ops {
	/* pmu pwdn */
	int (*ckc_pmu_pwdn)(int id, bool pwdn);
	int (*ckc_is_pmu_pwdn)(int id);

	/* software reset */
	int (*ckc_swreset)(int id, bool reset);

	/* isoip top */
	int (*ckc_isoip_top_pwdn)(int id, bool pwdn);
	int (*ckc_is_isoip_top_pwdn)(int id);

	/* isoip ddi */
	int (*ckc_isoip_ddi_pwdn)(int id, bool pwdn);
	int (*ckc_is_isoip_ddi_pwdn)(int id);

	/* isoip gpu */
	int (*ckc_isoip_gpu_pwdn)(int id, bool pwdn);
	int (*ckc_is_isoip_gpu_pwdn)(int id);

	/* pll */
	int (*ckc_pll_set_rate)(int id, unsigned long rate);
	unsigned long (*ckc_pll_get_rate)(int id);

	/* clkctrl */
	int (*ckc_clkctrl_enable)(int id);
	int (*ckc_clkctrl_disable)(int id);
	int (*ckc_clkctrl_set_rate)(int id, unsigned long rate);
	unsigned long (*ckc_clkctrl_get_rate)(int id);
	int (*ckc_is_clkctrl_enabled)(int id);

	/* peripheral */
	int (*ckc_peri_enable)(int id);
	int (*ckc_peri_disable)(int id);
	int (*ckc_peri_set_rate)(int id, unsigned long rate);
	unsigned long (*ckc_peri_get_rate)(int id);
	int (*ckc_is_peri_enabled)(int id);

	/* display bus */
	int (*ckc_ddibus_pwdn)(int id, bool pwdn);
	int (*ckc_is_ddibus_pwdn)(int id);
	int (*ckc_ddibus_swreset)(int id, bool reset);

	/* graphic bus */
	int (*ckc_gpubus_pwdn)(int id, bool pwdn);
	int (*ckc_is_gpubus_pwdn)(int id);
	int (*ckc_gpubus_swreset)(int id, bool reset);

	/* io bus */
	int (*ckc_iobus_pwdn)(int id, bool pwdn);
	int (*ckc_is_iobus_pwdn)(int id);
	int (*ckc_iobus_swreset)(int id, bool reset);

	/* video bus */
	int (*ckc_vpubus_pwdn)(int id, bool pwdn);
	int (*ckc_is_vpubus_pwdn)(int id);
	int (*ckc_vpubus_swreset)(int id, bool reset);

	/* hsio bus */
	int (*ckc_hsiobus_pwdn)(int id, bool pwdn);
	int (*ckc_is_hsiobus_pwdn)(int id);
	int (*ckc_hsiobus_swreset)(int id, bool reset);

	/* g2d bus */
	int (*ckc_g2dbus_pwdn)(int id, bool pwdn);
	int (*ckc_is_g2dbus_pwdn)(int id);
	int (*ckc_g2dbus_swreset)(int id, bool reset);

	/* cortex-m bus */
	int (*ckc_cmbus_pwdn)(int id, bool pwdn);
	int (*ckc_is_cmbus_pwdn)(int id);
	int (*ckc_cmbus_swreset)(int id, bool reset);
};

extern void tcc_ckc_set_ops(struct tcc_ckc_ops *ops);
extern void tcc_ckctest_set_ops(struct tcc_ckc_ops *ops);
