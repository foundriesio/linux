/****************************************************************************
Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA

@note Tab size is 8
****************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#include <asm/io.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif
#include <asm/mach-types.h>
#include <asm/uaccess.h>

#include <hpd_internal.h>


static struct hpd_dev *hpd_api_dev = NULL;


void hpd_api_register_dev(struct hpd_dev *dev)
{
        hpd_api_dev = dev;
}
EXPORT_SYMBOL(hpd_api_register_dev);

int hpd_api_get_status(void)
{
        int hotplug_status = 0;
        if(hpd_api_dev != NULL) {
                hotplug_status = hpd_get_status(hpd_api_dev);
        }
        return hotplug_status;

}
EXPORT_SYMBOL(hpd_api_get_status);

