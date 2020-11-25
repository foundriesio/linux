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
#ifndef __HDMI_1_4_HPD_H__
#define __HDMI_1_4_HPD_H__

#define VERSION "4.14_1.1.0"

struct hpd_dev {
        struct device *pdev;

        /** Misc Device */
        struct miscdevice *misc;
        struct work_struct tx_hotplug_handler;

        /** Hot Plug */
        int hotplug_gpio;
        int hotplug_irq;
        int hotplug_irq_enabled;

        /* This variable represent real hotplug status */
        int hotplug_real_status;
        /* This variable represent virtual hotplug status
         * If hotplug_locked was 0, It represent hotplug_real_status
         * On the other hand, If hotplug_locked was 1, it represent
         * hotplug_real_status at the time of hotplug_locked was set to 1 */
        int hotplug_status;


        int hotplug_locked;

        int suspend;
        int runtime_suspend;

        /** support poll */
        wait_queue_head_t poll_wq;
        int prev_hotplug_status;

        struct mutex mutex;

};

int hpd_get_status(struct hpd_dev *dev);
void hpd_api_register_dev(struct hpd_dev *dev);


#endif /* __HDMI_1_4_HPD_H__ */
