// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/phy.h>
#include <linux/delay.h>

MODULE_DESCRIPTION("TCC Marvell PHY driver");
MODULE_AUTHOR("Telechips");
MODULE_LICENSE("GPL");

#define	MMD_ACCESS_CTRL			(unsigned int)(13)
#define MMD_ACCESS_ADDR_DATA	(unsigned int)(14)

#define OPERATION_BIT_SHIFT		(unsigned int)(14)
#define OPERATION_ADDR			(unsigned int)(0x0)
#define OPERATION_RW			(unsigned int)(0x1)

#define MRVL_Q212X_LPSD_FEATURE_ENABLE 0

static int phy_read_c22_to_c45(struct phy_device *phydev, uint16_t dev_addr,
			       u32 reg_addr)
{
	phy_write(phydev, MMD_ACCESS_CTRL,
		  (unsigned
		   short)((unsigned short)(OPERATION_ADDR <<
					   OPERATION_BIT_SHIFT) | dev_addr));
	phy_write(phydev, MMD_ACCESS_ADDR_DATA, (unsigned short)reg_addr);
	phy_write(phydev, MMD_ACCESS_CTRL,
		  (unsigned
		   short)((unsigned short)(OPERATION_RW << OPERATION_BIT_SHIFT)
			  | dev_addr));
	return phy_read(phydev, (u32) MMD_ACCESS_ADDR_DATA);
}

static void phy_write_c22_to_c45(struct phy_device *phydev, uint16_t dev_addr,
				 uint16_t reg_addr, uint16_t data)
{
	phy_write(phydev, MMD_ACCESS_CTRL,
		  (unsigned short)((unsigned short)OPERATION_ADDR + dev_addr));
	phy_write(phydev, MMD_ACCESS_ADDR_DATA, (unsigned short)reg_addr);
	phy_write(phydev, MMD_ACCESS_CTRL,
		  (unsigned short)((unsigned short)OPERATION_RW + dev_addr));
	phy_write(phydev, MMD_ACCESS_ADDR_DATA, (unsigned short)data);

	return;
}

static void softReset(struct phy_device *phydev)
{
	uint16_t regDataAuto =
	    (unsigned short)phy_read_c22_to_c45(phydev, 1, 0x0000);

	regDataAuto |= (unsigned short)((unsigned int)1 << (unsigned int)11);
	phy_write_c22_to_c45(phydev, 1, 0x0000, regDataAuto);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x000C);
	msleep(1);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x06B6);
	regDataAuto &= (unsigned short)(~((unsigned int)1 << (unsigned int)11));
	phy_write_c22_to_c45(phydev, 1, 0x0000, regDataAuto);
	msleep(1);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0030);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0031);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0030);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0001);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0x0900, 0x8000);
	phy_write_c22_to_c45(phydev, 1, 0x0900, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x000C);
}

static void setMasterSlave(struct phy_device *phydev, bool forceMaster)
{
	uint16_t regData = 0;

	regData = (unsigned short)phy_read_c22_to_c45(phydev, 1, 0x0834);
	if (forceMaster) {
		regData |= (unsigned short)0x4000;
	} else {
		regData &= (unsigned short)0xBFFF;
	}

	phy_write_c22_to_c45(phydev, 1, 0x0834, regData);

/*
	if ( ((unsigned int)forceMaster != (unsigned int)0) ){
#ifdef MRVL_Q212X_LPSD_FEATURE_ENABLE
		phy_write_c22_to_c45(phydev, 7, 0x8032, 0x005A);
#endif
	}
	else{
	*/
	{
		phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0064);
	}
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A01);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C01);

}

static uint16_t getMasterSlave(struct phy_device *phydev)
{
	return (unsigned
		short)((unsigned int)((unsigned int)
				  phy_read_c22_to_c45(phydev, 7,
						      0x8001) >> (unsigned int)14) &
		       (unsigned int)0x0001);
}

static bool checkLink(struct phy_device *phydev)
{
	uint16_t retData1, retData2 = 0;

	phy_read_c22_to_c45(phydev, 3, 0x0901);
	retData1 = (unsigned short)phy_read_c22_to_c45(phydev, 3, 0x0901);
	retData2 = (unsigned short)phy_read_c22_to_c45(phydev, 7, 0x8001);
	msleep(1);
	return ((unsigned short)0x0 !=
		((unsigned short)retData1 & (unsigned short)0x0004))
	    && ((unsigned short)0x0 !=
		((unsigned short)retData2 & (unsigned short)0x3000));
	// return ( ((unsigned short)0x0 != ((unsigned short)retData1 & (unsigned int)0x0004)) && ((unsigned int)0x0 != ((unsigned short)retData2 & (unsigned short)0x3000)) );
}

static bool getRealTimeLinkStatus(struct phy_device *phydev)
{
	uint16_t retData = 0;

	phy_read_c22_to_c45(phydev, 3, 0x0901);
	retData = (unsigned short)phy_read_c22_to_c45(phydev, 3, 0x0901);
	return ((unsigned short)0x0 !=
		((unsigned short)retData & (unsigned short)0x0004));
}

static bool getLatchedLinkStatus(struct phy_device *phydev)
{
	uint16_t retData =
	    (unsigned short)phy_read_c22_to_c45(phydev, 7, 0x0201);

	return ((unsigned short)0x0 !=
		((unsigned short)retData & (unsigned short)0x0004));

}

static void initQ212X(struct phy_device *phydev)
{
	uint16_t regData = 0;
	unsigned int get_mas_slave;

	msleep(2);
	phy_write_c22_to_c45(phydev, 1, 0x0900, 0x4000);
	phy_write_c22_to_c45(phydev, 7, 0x0200, 0x0000);
	regData = (unsigned short)phy_read_c22_to_c45(phydev, 1, 0x0834);
	regData =
	    (unsigned short)(((unsigned short)regData & (unsigned short)0xFFF0)
			     | (unsigned short)0x0001);

	get_mas_slave = getMasterSlave(phydev);
	if ((unsigned int)get_mas_slave == (unsigned int)0x1) {	// master
		regData &=
		    (unsigned
		     short)(~(unsigned short)((unsigned short)1 <<
					      (unsigned short)14));
		regData |=
		    (unsigned short)((unsigned short)1 << (unsigned short)14);
	} else {		// slave
		regData &=
		    (unsigned
		     short)(~(unsigned short)((unsigned short)1 <<
					      (unsigned short)14));
	}

	phy_write_c22_to_c45(phydev, 1, 0x0834, regData);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x07B5);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x06B6);
	msleep(5);
	phy_write_c22_to_c45(phydev, 3, 0xFFDE, 0x402F);
	phy_write_c22_to_c45(phydev, 3, 0xFE2A, 0x3C3D);
	phy_write_c22_to_c45(phydev, 3, 0xFE34, 0x4040);
	phy_write_c22_to_c45(phydev, 3, 0xFE4B, 0x9337);
	phy_write_c22_to_c45(phydev, 3, 0xFE2A, 0x3C1D);
	phy_write_c22_to_c45(phydev, 3, 0xFE34, 0x0040);
	phy_write_c22_to_c45(phydev, 3, 0xFE0F, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0xFC00, 0x01C0);
	phy_write_c22_to_c45(phydev, 3, 0xFC17, 0x0425);
	phy_write_c22_to_c45(phydev, 3, 0xFC94, 0x5470);
	phy_write_c22_to_c45(phydev, 3, 0xFC95, 0x0055);
	phy_write_c22_to_c45(phydev, 3, 0xFC19, 0x08d8);
	phy_write_c22_to_c45(phydev, 3, 0xFC1A, 0x0110);
	phy_write_c22_to_c45(phydev, 3, 0xFC1B, 0x0a10);
	phy_write_c22_to_c45(phydev, 3, 0xFC3A, 0x2725);
	phy_write_c22_to_c45(phydev, 3, 0xFC61, 0x2627);
	phy_write_c22_to_c45(phydev, 3, 0xFC3B, 0x1612);
	phy_write_c22_to_c45(phydev, 3, 0xFC62, 0x1C12);
	phy_write_c22_to_c45(phydev, 3, 0xFC9D, 0x6367);
	phy_write_c22_to_c45(phydev, 3, 0xFC9E, 0x8060);
	phy_write_c22_to_c45(phydev, 3, 0xFC00, 0x01C8);
	phy_write_c22_to_c45(phydev, 3, 0x8000, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0x8016, 0x0011);
	phy_write_c22_to_c45(phydev, 3, 0xFDA3, 0x1800);
	phy_write_c22_to_c45(phydev, 3, 0xFE02, 0x00C0);
	phy_write_c22_to_c45(phydev, 3, 0xFFDB, 0x0010);
	phy_write_c22_to_c45(phydev, 3, 0xFFF3, 0x0020);
	phy_write_c22_to_c45(phydev, 3, 0xFE40, 0x00A6);
	phy_write_c22_to_c45(phydev, 3, 0xFE60, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0xFE2A, 0x3C3D);
	phy_write_c22_to_c45(phydev, 3, 0xFE4B, 0x9334);
	phy_write_c22_to_c45(phydev, 3, 0xFC10, 0xF600);
	phy_write_c22_to_c45(phydev, 3, 0xFC11, 0x073D);
	phy_write_c22_to_c45(phydev, 3, 0xFC12, 0x000D);
	phy_write_c22_to_c45(phydev, 3, 0xFC13, 0x0010);

	//LPSD feature
	/*
	   if (MRVL_Q212X_LPSD_FEATURE_ENABLE){
	   if (isMaster(phydev)){
	   phy_write_c22_to_c45(phydev, 7, 0x8032, 0x005A);
	   }
	   else{
	   phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0064);
	   }
	   phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A01);
	   phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C01);
	   phy_write_c22_to_c45(phydev, 3, 0x800C, 0x0008);
	   phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0001);
	   phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A1B);
	   phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C1B);
	   phy_write_c22_to_c45(phydev, 7, 0x8032, 0x000B);
	   phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A1C);
	   phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C1C);
	   phy_write_c22_to_c45(phydev, 3, 0xFE04, 0x0016);
	   }
	   else{
	 */
	{
		phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0064);
		phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A01);
		phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C01);
		phy_write_c22_to_c45(phydev, 3, 0x800C, 0x0000);
		phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0002);
		phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A1B);
		phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C1B);
		phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0003);
		phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A1C);
		phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C1C);
		phy_write_c22_to_c45(phydev, 3, 0xFE04, 0x0008);
	}
	softReset(phydev);
}

static int q2110_config_init(struct phy_device *phydev)
{
	int ret = 0;
	uint16_t regData = 0;

	pr_info("[%s] ---------.\n", __func__);
	// q2110_setAnegGe(phydev);

	initQ212X(phydev);

	pr_info("phy id before: %08x\n", phydev->phy_id);
	phydev->phy_id = (unsigned int)phy_read_c22_to_c45(phydev, 1, 0x0003);

	pr_info("q2110 phy master(1)/slave(0) : %x\n", getMasterSlave(phydev));
	pr_info("phy id : %08x\n", phydev->phy_id);

	return 0;
}

static int q2110_read_status(struct phy_device *phydev)
{
	int err;
	int ret;

	// Update the link, but return if there
	// was an error
	phydev->link = (int)getLatchedLinkStatus(phydev);
	phydev->speed = SPEED_1000;
	// phydev->speed = SPEED_100;
	phydev->duplex = DUPLEX_FULL;

	if ((unsigned int)phydev->link != (unsigned int)0)
		phydev->state = PHY_RUNNING;
	else
		phydev->state = PHY_NOLINK;

	phydev->adjust_link(phydev->attached_dev);

	return 0;
}

static int q2110_setAnegGe(struct phy_device *phydev)
{

	uint16_t regDataAuto = 0;

	// q2110_init_A0Ge(phydev);

	regDataAuto = (unsigned short)phy_read_c22_to_c45(phydev, 7, 0x0202);

	phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0200);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0a03);
	phy_write_c22_to_c45(phydev, 7, 0x0203, 0x0090);
	/*
	   if (forceMaster == 1){
	   regDataAuto |= 1 << 12;
	   }
	   else{
	   regDataAuto &= ~(1 << 12);
	   }
	 */
	phy_write_c22_to_c45(phydev, 7, 0x0202, regDataAuto);
	phy_write_c22_to_c45(phydev, 7, 0x0200, 0x1200);

	return 0;
}

static int q2110_aneg_done(struct phy_device *phydev)
{
	return 0;
}

static struct phy_driver q2110_driver[1] = {
	{
	 .phy_id = 0x00000980,	// OUI + Model Number + Revision Number
	 .phy_id_mask = 0x00000000,	// Uncertain Revision Number
	 .name = "Marvell Q2110",
	 .features =
	 (unsigned int)SUPPORTED_100baseT_Full | (unsigned int)
	 SUPPORTED_1000baseT_Full | (unsigned int)SUPPORTED_MII,
	 .flags = PHY_POLL,
	 .config_init = &q2110_config_init,
	 .read_status = &q2110_read_status,
	 .config_aneg = &q2110_aneg_done,
	 .aneg_done = &q2110_aneg_done,
	 },
};

module_phy_driver(q2110_driver);

static struct mdio_device_id __maybe_unused tcc_marvell_tbl[] = {
	{0x00000980, 0x00000000},
	{}
};

MODULE_DEVICE_TABLE(mdio, tcc_marvell_tbl);
