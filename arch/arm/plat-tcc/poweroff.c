/****************************************************************************
 * plat-tcc/poweroff.c
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

#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <mach/bsp.h>
#include <mach/io.h>
//#include <mach/system.h>
#include <asm/system_misc.h>
#include <asm/mach/arch.h>

#define nop_delay(x) for(cnt=0 ; cnt<x ; cnt++){ \
		__asm__ __volatile__ ("nop\n"); }

uint32_t restart_reason = 0x776655AA;

static void tcc_pm_power_off(void)
{
#ifdef CONFIG_RTC_DISABLE_ALARM_FOR_PWROFF_STATE		//Disable the RTC Alarm during the power off state
	{
		extern volatile int tca_alarm_disable(unsigned int rtcbaseaddress);
		tca_alarm_disable(io_p2v(HwRTC_BASE));
	}
#endif
#ifdef CONFIG_REGULATOR_DA9062
    {
        extern void da9062_power_off(void);
        da9062_power_off();
    }
#endif

	while(1);
}

static int tcc_reboot_call(struct notifier_block *this, unsigned long code, void *cmd)
{
	/* XXX: convert reboot mode value because USTS register
	 * hold only 8-bit value
	 */
	if (code == SYS_RESTART) {
		if (cmd) {
			if (!strcmp(cmd, "bootloader"))
				restart_reason = 1;	/* fastboot mode */
			else if (!strcmp(cmd, "recovery"))
				restart_reason = 2;	/* recovery mode */
//+[TCCQB] QuickBoot Skip Mode
			else if (!strcmp(cmd, "force_normal"))
				restart_reason = 3;	/* skip quickboot mode */
//-[TCCQB]
//
			else if (!strcmp(cmd, "ramdump"))
				restart_reason = 0xf;
			else
				restart_reason = 0;
		} else
			restart_reason = 0;
	}
	return NOTIFY_DONE;
}

static struct notifier_block tcc_reboot_notifier = {
	.notifier_call = tcc_reboot_call,
};
static int __init tcc_poweroff_init(void)
{
	pm_power_off = tcc_pm_power_off;
	if (machine_desc->restart && machine_desc)
		arm_pm_restart = machine_desc->restart;

	register_reboot_notifier(&tcc_reboot_notifier);

	return 0;
}
__initcall(tcc_poweroff_init);
