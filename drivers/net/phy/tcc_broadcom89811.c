/*
 *	drivers/net/phy/broadcom.c
 *
 *	Broadcom BCM5411, BCM5421 and BCM5461 Gigabit Ethernet
 *	transceivers.
 *
 *	Copyright (c) 2006  Maciej W. Rozycki
 *
 *	Inspired by code written by Amy Fong.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/phy.h>

MODULE_DESCRIPTION("TCC Broadcom PHY driver");
MODULE_AUTHOR("Telechips");
MODULE_LICENSE("GPL");

#define BCM89811_MIICTL		(0x00)
#define BCM89810_AUXCTRL	(0x18)

static int bcm89811_config_init(struct phy_device *phydev)
{
	// -----------begin PHY optimization portion----------
	phy_write(phydev, BCM89811_MIICTL, 0x8000);	// reset

	phy_write(phydev, 0x001E, 0x0028);	//
	phy_write(phydev, 0x001F, 0x0C00);	//

	phy_write(phydev, 0x001E, 0x0312);	//
	phy_write(phydev, 0x001F, 0x030B);	//

	phy_write(phydev, 0x001E, 0x030A);	//
	phy_write(phydev, 0x001F, 0x34C0);	//

	phy_write(phydev, 0x001E, 0x0166);	//
	phy_write(phydev, 0x001F, 0x0020);	//

	phy_write(phydev, 0x001E, 0x012D);	//
	phy_write(phydev, 0x001F, 0x9B52);	//
	phy_write(phydev, 0x001E, 0x012E);	//
	phy_write(phydev, 0x001F, 0xA04d);	//

	phy_write(phydev, 0x001E, 0x0123);	//
	phy_write(phydev, 0x001F, 0x00C0);	//

	phy_write(phydev, 0x001E, 0x0154);	//
	phy_write(phydev, 0x001F, 0x81C4);	//

	phy_write(phydev, 0x001E, 0x0811);	//
	phy_write(phydev, 0x001F, 0x0000);	// 0 or MII_PAD+SETTING * 2048

	//////////////////////////
	phy_write(phydev, 0x001E, 0x01D3);	//
	phy_write(phydev, 0x001F, 0x0000);	// 0 o

	phy_write(phydev, 0x001E, 0x01C1);	//
	phy_write(phydev, 0x001F, 0xA5F7);	//

	phy_write(phydev, 0x001E, 0x0028);	// DSP clk disable
	phy_write(phydev, 0x001F, 0x0400);	//
	/////////////////////////////

	phy_write(phydev, 0x001E, 0x001D);	//
	phy_write(phydev, 0x001F, 0x3411);	//

	phy_write(phydev, 0x001E, 0x0820);	//
	phy_write(phydev, 0x001F, 0x0401);	//
	////////////////////////////////

#if defined(CONFIG_TCC_RGMII_MODE_BCM_89811)
	phy_write(phydev, 0x001E, 0x0045);	//
	phy_write(phydev, 0x001F, 0x0000);	//

	phy_write(phydev, 0x001E, 0x002F);	//
	phy_write(phydev, 0x001F, 0xF1E7);	//
#elif defined(CONFIG_TCC_MII_MODE_BCM_89811)
	phy_write(phydev, 0x001E, 0x002F);	//
	phy_write(phydev, 0x001F, 0xF167);	//

	phy_write(phydev, 0x001E, 0x0045);	//
	phy_write(phydev, 0x001F, 0x0700);	//
#else
//      #error "Not supported Phy Mode"
#endif

#ifdef CONFIG_TCC_BROADCOM_MDI_MASTER_MODE_BCM_89811
	pr_info(" MASTER mode\n");
	phy_write(phydev, BCM89811_MIICTL, 0x0208);
#else
	pr_info(" SLAVE mode\n");
	phy_write(phydev, BCM89811_MIICTL, 0x0200);
#endif

	return 0;
}

int bcm89811_update_link(struct phy_device *phydev)
{
	int status;

	/* Read link and autonegotiation status */
	status = phy_read(phydev, MII_BMSR);

	if (status < 0)
		return status;

	if ((status & BMSR_LSTATUS) == 0)
		phydev->link = 0;
	else
		phydev->link = 1;

	return 0;
}

int bcm89811_read_status(struct phy_device *phydev)
{
	int err;
	static int mdi_mode;
	static int count;
	static int max;

	err = bcm89811_update_link(phydev);
	if (err)
		return err;

#if 0
	if (phydev->link == 0) {
		if (count++ > max) {
			int rand;

			if (mdi_mode) {
				phy_write(phydev, BCM89810_MIICTL, 0x0200);
				mdi_mode = 0;
			} else {
				phy_write(phydev, BCM89810_MIICTL, 0x0208);
				mdi_mode = 1;
			}
			count = 0;
			get_random_bytes_arch(&rand, 1);
			max = rand % 3;
			//printk("mdi_mode(%d), max(%d)\n", mdi_mode, max);
		}
	}
#endif

	phydev->duplex = DUPLEX_FULL;
	phydev->speed = SPEED_100;

	return 0;
}

static int bcm89811_config_aneg(struct phy_device *phydev)
{

	return 0;
}

static struct phy_driver bcm89811_driver = {
	.phy_id = 0xae025020,
	.phy_id_mask = 0xfffffff0,
	.name = "Broadcom BCM89811",
	.features = SUPPORTED_100baseT_Full | SUPPORTED_MII,
	.flags = PHY_POLL,
	.config_init = bcm89811_config_init,
	.read_status = bcm89811_read_status,
	.config_aneg = bcm89811_config_aneg,
	.driver = {.owner = THIS_MODULE},
};

static int __init tcc_broadcom_init(void)
{
	int ret;

	ret = phy_driver_register(&bcm89811_driver);
	if (ret)
		phy_driver_unregister(&bcm89811_driver);

	return ret;
}

static void __exit tcc_broadcom_exit(void)
{
	phy_driver_unregister(&bcm89811_driver);
}

module_init(tcc_broadcom_init);
module_exit(tcc_broadcom_exit);

static struct mdio_device_id __maybe_unused tcc_broadcom_tbl[] = {
	{0xae025020, 0xfffffff0},
	{}
};

MODULE_DEVICE_TABLE(mdio, tcc_broadcom_tbl);
