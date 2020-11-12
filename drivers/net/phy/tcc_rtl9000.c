/*
 * drivers/net/phy/realtek.c
 *
 * Driver for Realtek PHYs
 *
 * Author: Johnson Leung <r58129@freescale.com>
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/phy.h>
#include <linux/module.h>

MODULE_DESCRIPTION("RTL9000AA driver for telechips");
MODULE_AUTHOR("Telechips");
MODULE_LICENSE("GPL");

static int slave_0_master_1 = 2;
module_param(slave_0_master_1, int, 0644 | 0644);
MODULE_PARM_DESC(slave_0_master_1, "Slave mode : 0 , Master mode : 1");

// cat /sys/module/tcc_rtl9000/parameters/slave_0_master_1
// echo 0 > /sys/module/tcc_rtl9000/parameters/slave_0_master_1
// : Set Slave Mode
// echo 1 > /sys/module/tcc_rtl9000/parameters/slave_0_master_1
// : Set Master Mode

static unsigned int set_master;

static int tcc_rtl9000a_vc_patch(struct phy_device *phydev)
{
	unsigned int io_data;
	int timeout_cnt = 0;

	/* PHY_PATCH_REQEUST/LOCK */
	phy_write(phydev, 0x1F, 0xA43);
	phy_write(phydev, 0x1B, 0xB820);
	phy_write(phydev, 0x1C, (unsigned short)((unsigned short)
				phy_read(phydev, 0x1C) |
				(unsigned short)0x0010));
	phy_write(phydev, 0x1B, 0xB830);
	phy_write(phydev, 0x1C, 0x8000);
	phy_write(phydev, 0x1B, 0xB800);
	io_data = (unsigned int)phy_read(phydev, 0x1C);

	while ((unsigned int)io_data != (unsigned int)0x0040) {
		io_data =
		    (unsigned int)((unsigned int)phy_read(phydev, 0x1C) &
				   (unsigned int)0x0040);
		timeout_cnt++;
		if (timeout_cnt == 30000) {
			pr_info
			    ("[%s] occurred timeout to \
			     get valid value. line %d \n",
			     __func__, __LINE__);
			timeout_cnt = 0;
			break;
		}
	}

	phy_write(phydev, 0x1B, 0x8020);
	phy_write(phydev, 0x1C, 0x8001);
	phy_write(phydev, 0x1B, 0xB82E);
	phy_write(phydev, 0x1C, 0x0001);

	// patch code
	phy_write(phydev, 0x1B, 0xB820);
	phy_write(phydev, 0x1C, 0x0290);
	phy_write(phydev, 0x1B, 0xA012);
	phy_write(phydev, 0x1C, 0x0000);
	phy_write(phydev, 0x1B, 0xA014);
	phy_write(phydev, 0x1C, 0x2c03);
	phy_write(phydev, 0x1C, 0x2c0d);
	phy_write(phydev, 0x1C, 0x2c10);
	phy_write(phydev, 0x1C, 0xd710);
	phy_write(phydev, 0x1C, 0x6080);
	phy_write(phydev, 0x1C, 0xd71e);
	phy_write(phydev, 0x1C, 0x7fac);
	phy_write(phydev, 0x1C, 0x26d6);
	phy_write(phydev, 0x1C, 0x3122);
	phy_write(phydev, 0x1C, 0x06a2);
	phy_write(phydev, 0x1C, 0xa208);
	phy_write(phydev, 0x1C, 0x8121);
	phy_write(phydev, 0x1C, 0x2699);
	phy_write(phydev, 0x1C, 0xe0a7);
	phy_write(phydev, 0x1C, 0x0521);
	phy_write(phydev, 0x1C, 0x229a);
	phy_write(phydev, 0x1C, 0xd705);
	phy_write(phydev, 0x1C, 0x3294);
	phy_write(phydev, 0x1C, 0x5401);
	phy_write(phydev, 0x1C, 0x23f9);
	phy_write(phydev, 0x1B, 0xA01A);
	phy_write(phydev, 0x1C, 0x0000);
	phy_write(phydev, 0x1B, 0xA00A);
	phy_write(phydev, 0x1C, 0x0fff);
	phy_write(phydev, 0x1B, 0xA008);
	phy_write(phydev, 0x1C, 0x0fff);
	phy_write(phydev, 0x1B, 0xA006);
	phy_write(phydev, 0x1C, 0x0fff);
	phy_write(phydev, 0x1B, 0xA004);
	phy_write(phydev, 0x1C, 0x03f8);
	phy_write(phydev, 0x1B, 0xA002);
	phy_write(phydev, 0x1C, 0x0298);
	phy_write(phydev, 0x1B, 0xA000);
	phy_write(phydev, 0x1C, 0x76a0);
	phy_write(phydev, 0x1B, 0xB820);
	phy_write(phydev, 0x1C, 0x0210);

	/* RELEASE_PHY_PATCH_REQEUST/LOCK */
	phy_write(phydev, 0x1B, 0x0000);
	phy_write(phydev, 0x1C, 0x0000);
	phy_write(phydev, 0x1B, 0xB82E);
	phy_write(phydev, 0x1C,
		  (unsigned short)((unsigned int)phy_read(phydev, 0x1C) &
				   (unsigned int)
				   0xFFFE));
	phy_write(phydev, 0x1B, 0x8020);
	phy_write(phydev, 0x1C, 0x0000);
	phy_write(phydev, 0x1B, 0xB820);
	phy_write(phydev, 0x1C,
		  (unsigned short)((unsigned int)phy_read(phydev, 0x1C) &
				   (unsigned int)
				   0xFFEF));
	phy_write(phydev, 0x1B, 0xB800);
	io_data = (unsigned int)phy_read(phydev, 0x1C);

	while ((unsigned int)io_data != (unsigned int)0x0000) {
		io_data =
		    (unsigned int)((unsigned int)phy_read(phydev, 0x1C) &
				   (unsigned int)
				   0x0040);
		timeout_cnt++;
		if ((unsigned int)timeout_cnt == (unsigned int)30000) {
			pr_info
			    ("[%s] occurred timeout to \
			     get valid value. line %d \n",
			     __func__, __LINE__);
			timeout_cnt = (int)0;
			break;
		}
	}

	// RTL9000AA_VB setting
	phy_write(phydev, 0x1B, 0xBCC0);
	phy_write(phydev, 0x1C, 0x0149);
	phy_write(phydev, 0x1B, 0xBCC2);
	phy_write(phydev, 0x1C, 0x003A);

	phy_write(phydev, 0x1F, 0x0A5A);
	phy_write(phydev, 0x14, 0x0001);

	// Standby go to Normal mode
	phy_write(phydev, 0x1F, 0x0A43);
	phy_write(phydev, 0x1B, 0xDD00);
	phy_write(phydev, 0x1C, 0x06D3);
	phy_write(phydev, 0x1B, 0xDD20);
	phy_write(phydev, 0x1C, 0x000B);

	phy_write(phydev, 0x1F, 0x0A42);
	return 0;
}

static int tcc_rtl9000a_config_aneg(struct phy_device *phydev)
{
	return 0;
}

static int tcc_rtl9000a_config_init(struct phy_device *phydev)
{
	unsigned int ret;
	unsigned int timeout_cnt = 0;

	// pr_info("%s\n", __func__);
#if defined(CONFIG_TCC_RTL9000_SLAVE_MODE)
	set_master = 0;
#else
	set_master = 1;
#endif
	if (((unsigned int)slave_0_master_1 == (unsigned int)0)
	    || ((unsigned int)slave_0_master_1 == (unsigned int)1)) {
		set_master = (unsigned int)slave_0_master_1;
	}
	// Check MDIO interface is ready
	do {
		ret = (unsigned int)phy_read(phydev, 0x0);

		if ((unsigned int)ret == (unsigned int)0x2100)
			break;

		if ((unsigned int)timeout_cnt == (unsigned int)30000) {
			pr_info
		    ("[%s] occurred timeout to get valid value. line %d \n",
			     __func__, __LINE__);
			timeout_cnt = (unsigned int)0;
			break;
		} else
			timeout_cnt++;
	} while (1);

	// Check hard-reset complete
	do {
		ret = (unsigned int)phy_read(phydev, 0x10);
//              ret = phy_read(phydev, 0x16);
		timeout_cnt++;

		if (((unsigned int)ret & (unsigned int)0x0007) ==
		    (unsigned int)0x03)
			break;

		if ((unsigned int)timeout_cnt == (unsigned int)30000) {
			pr_info
		    ("[%s] occurred timeout to get valid value. line %d \n",
			     __func__, __LINE__);
			timeout_cnt = (unsigned int)0;
			break;
		}
	} while (1);

	tcc_rtl9000a_vc_patch(phydev);

	if ((unsigned int)set_master == (unsigned int)1) {
		ret = (unsigned int)phy_read(phydev, 0x9);
		phy_write(phydev, 0x9, (unsigned short)((unsigned int)ret |
					(unsigned int)0x0800));	// Set as Master
		pr_info("RTL9000A] Set Master Mode \n");
	} else {
		ret = (unsigned int)phy_read(phydev, 0x9);
		phy_write(phydev, 0x9, (unsigned short)((unsigned int)ret &
					(unsigned int)0xF7FF));	// Set as Slave
		pr_info("RTL9000A] Set Slave Mode \n");
	}

	phy_write(phydev, 0, (unsigned short)0x8000);	// PHY soft-reset

	while ((unsigned int)ret != (unsigned int)0x2100) {

		timeout_cnt++;
		if ((unsigned int)timeout_cnt == (unsigned int)30000) {
			pr_info
			    ("[%s] occurred timeout to get \
			     valid value. line %d \n",
			     __func__, __LINE__);
			timeout_cnt = (unsigned int)0;
			break;
		}

		ret = (unsigned int)phy_read(phydev, 0x0);
	}

	// Loopback mode
	// ret = phy_read(phydev, 0x0);
	// phy_write(phydev, 0x0, (ret | (1<<14)));
	// pr_info("%s. enable phy loopback mode. \n", __func__);

	ret = (unsigned int)phy_read(phydev, 0xA);	// read PHYSR1
	pr_info("%s. PHYSR1: %08x\n", __func__, ret);

	return 0;
}

static int tcc_rtl9000a_update_link(struct phy_device *phydev)
{
	int status;

	status = phy_read(phydev, MII_BMSR);

	if ((int)status < (int)0)
		return status;

	if (((unsigned int)status & (unsigned int)BMSR_LSTATUS) ==
	    (unsigned int)0)
		phydev->link = (int)0;
	else
		phydev->link = (int)1;

	if (((unsigned int)status & (unsigned int)BMSR_JCD) ==
	    (unsigned int)BMSR_JCD)
		pr_info("%s. ========= phydev jabber detected\n", __func__);

	// pr_info("%s. BMSR : %08x\n", __func__, status);
	// pr_info("%s. Local receiver status : %08x\n", __func__,
	// (phy_read(phydev, 0xA) & (1<<13)));
	// pr_info("%s. Remote receiver status : %08x\n", __func__,
	// (phy_read(phydev, 0xA) & (1<<12)));
//
	// pr_info("%s. phydev->link: %08x\n", __func__, phydev->link);
	return 0;
}

static int tcc_rtl9000a_read_status(struct phy_device *phydev)
{
	int err, ret;

	err = tcc_rtl9000a_update_link(phydev);
	if ((unsigned int)err != (unsigned int)0)
		return err;

	phydev->duplex = (int)DUPLEX_FULL;
	phydev->speed = (int)SPEED_100;
	// phydev->speed        = SPEED_10;

#if 1
	if ((unsigned int)slave_0_master_1 != (unsigned int)set_master) {
		set_master = (unsigned int)slave_0_master_1;

		if ((unsigned int)set_master == (unsigned int)1) {
			ret = (int)phy_read(phydev, 0x9);
			phy_write(phydev, 0x9, (unsigned short)
					((unsigned int)ret |
					 (unsigned int)0x0800));
			// Set as Master
			pr_info("RTL9000A] Set Master Mode \n");
		}
		if ((unsigned int)set_master == (unsigned int)0) {
			ret = (int)phy_read(phydev, 0x9);
			phy_write(phydev, 0x9, (unsigned short)
					((unsigned int)ret &
					 (unsigned int)0xF7FF));
			// Set as Slave
			pr_info("RTL9000A] Set Slave Mode \n");
		}
	}
#endif

	phydev->adjust_link(phydev->attached_dev);
	return 0;
}

static struct phy_driver rtl9000a_driver[1] = {
	{
	 .phy_id = 0x001ccb00,
	 .name = "RTL9000A PHY",
	 .phy_id_mask = 0x00ffff00,
	 .features = PHY_BASIC_FEATURES,
	 // .flags               = PHY_HAS_INTERRUPT,
	 .flags = PHY_POLL,
	 .config_aneg = &tcc_rtl9000a_config_aneg,
	 .read_status = &tcc_rtl9000a_read_status,
	 // .read_status = &genphy_read_status,
	 .config_init = &tcc_rtl9000a_config_init,
	 // .driver              = { .owner = THIS_MODULE,},
	 },
};

module_phy_driver(rtl9000a_driver);

static struct mdio_device_id __maybe_unused tcc_rtl9000a_tbl[] = {
	{0x001ccb00, 0x00ffff00},
	{}
};

MODULE_DEVICE_TABLE(mdio, tcc_rtl9000a_tbl);
