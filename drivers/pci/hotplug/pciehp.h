/*
 * PCI Express Hot Plug Controller Driver
 *
 * Copyright (C) 1995,2001 Compaq Computer Corporation
 * Copyright (C) 2001 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001 IBM Corp.
 * Copyright (C) 2003-2004 Intel Corporation
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to <greg@kroah.com>, <kristen.c.accardi@intel.com>
 *
 */
#ifndef _PCIEHP_H
#define _PCIEHP_H

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <linux/delay.h>
#include <linux/sched/signal.h>		/* signal_pending() */
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/workqueue.h>

#include "../pcie/portdrv.h"

#define MY_NAME	"pciehp"

extern bool pciehp_poll_mode;
extern int pciehp_poll_time;
extern bool pciehp_debug;

#define dbg(format, arg...)						\
do {									\
	if (pciehp_debug)						\
		printk(KERN_DEBUG "%s: " format, MY_NAME, ## arg);	\
} while (0)
#define err(format, arg...)						\
	printk(KERN_ERR "%s: " format, MY_NAME, ## arg)
#define info(format, arg...)						\
	printk(KERN_INFO "%s: " format, MY_NAME, ## arg)
#define warn(format, arg...)						\
	printk(KERN_WARNING "%s: " format, MY_NAME, ## arg)

#define ctrl_dbg(ctrl, format, arg...)					\
	do {								\
		if (pciehp_debug)					\
			dev_printk(KERN_DEBUG, &ctrl->pcie->device,	\
					format, ## arg);		\
	} while (0)
#define ctrl_err(ctrl, format, arg...)					\
	dev_err(&ctrl->pcie->device, format, ## arg)
#define ctrl_info(ctrl, format, arg...)					\
	dev_info(&ctrl->pcie->device, format, ## arg)
#define ctrl_warn(ctrl, format, arg...)					\
	dev_warn(&ctrl->pcie->device, format, ## arg)

#define SLOT_NAME_SIZE 10

/**
 * struct controller - PCIe hotplug controller
 * @ctrl_lock: serializes writes to the Slot Control register
 * @pcie: pointer to the controller's PCIe port service device
 * @reset_lock: prevents access to the Data Link Layer Link Active bit in the
 *	Link Status register and to the Presence Detect State bit in the Slot
 *	Status register during a slot reset which may cause them to flap
 * @queue: wait queue to wake up on reception of a Command Completed event,
 *	used for synchronous writes to the Slot Control register
 * @slot_cap: cached copy of the Slot Capabilities register
 * @slot_ctrl: cached copy of the Slot Control register
 * @poll_timer: timer to poll for slot events if no IRQ is available,
 *	enabled with pciehp_poll_mode module parameter
 * @cmd_started: jiffies when the Slot Control register was last written;
 *	the next write is allowed 1 second later, absent a Command Completed
 *	interrupt (PCIe r4.0, sec 6.7.3.2)
 * @cmd_busy: flag set on Slot Control register write, cleared by IRQ handler
 *	on reception of a Command Completed event
 * @link_active_reporting: cached copy of Data Link Layer Link Active Reporting
 *	Capable bit in Link Capabilities register; if this bit is zero, the
 *	Data Link Layer Link Active bit in the Link Status register will never
 *	be set and the driver is thus confined to wait 1 second before assuming
 *	the link to a hotplugged device is up and accessing it
 * @notification_enabled: whether the IRQ was requested successfully
 * @power_fault_detected: whether a power fault was detected by the hardware
 *	that has not yet been cleared by the user
 */
struct controller {
	struct mutex ctrl_lock;
	struct pcie_device *pcie;
	struct rw_semaphore reset_lock;
	wait_queue_head_t queue;
	u32 slot_cap;
	u16 slot_ctrl;
	struct task_struct *poll_thread;
	unsigned long cmd_started;	/* jiffies */
	unsigned int cmd_busy:1;
	unsigned int link_active_reporting:1;
	unsigned int notification_enabled:1;
	unsigned int power_fault_detected;
	atomic_t pending_events;
	u8 state;
	struct mutex lock;
	struct delayed_work work;
	struct hotplug_slot *hotplug_slot;
	int request_result;
	wait_queue_head_t requester;
};

/**
 * DOC: Slot state
 *
 * @OFF_STATE: slot is powered off, no subordinate devices are enumerated
 * @BLINKINGON_STATE: slot will be powered on after the 5 second delay,
 *	green led is blinking
 * @BLINKINGOFF_STATE: slot will be powered off after the 5 second delay,
 *	green led is blinking
 * @POWERON_STATE: slot is currently powering on
 * @POWEROFF_STATE: slot is currently powering off
 * @ON_STATE: slot is powered on, subordinate devices have been enumerated
 */
#define OFF_STATE			0
#define BLINKINGON_STATE		1
#define BLINKINGOFF_STATE		2
#define POWERON_STATE			3
#define POWEROFF_STATE			4
#define ON_STATE			5

/**
 * DOC: Flags to request an action from the IRQ thread
 *
 * These are stored together with events read from the Slot Status register,
 * hence must be greater than its 16-bit width.
 *
 * %DISABLE_SLOT: Disable the slot in response to a user request via sysfs or
 *	an Attention Button press after the 5 second delay
 */
#define DISABLE_SLOT		(1 << 16)

#define ATTN_BUTTN(ctrl)	((ctrl)->slot_cap & PCI_EXP_SLTCAP_ABP)
#define POWER_CTRL(ctrl)	((ctrl)->slot_cap & PCI_EXP_SLTCAP_PCP)
#define MRL_SENS(ctrl)		((ctrl)->slot_cap & PCI_EXP_SLTCAP_MRLSP)
#define ATTN_LED(ctrl)		((ctrl)->slot_cap & PCI_EXP_SLTCAP_AIP)
#define PWR_LED(ctrl)		((ctrl)->slot_cap & PCI_EXP_SLTCAP_PIP)
#define HP_SUPR_RM(ctrl)	((ctrl)->slot_cap & PCI_EXP_SLTCAP_HPS)
#define EMI(ctrl)		((ctrl)->slot_cap & PCI_EXP_SLTCAP_EIP)
#define NO_CMD_CMPL(ctrl)	((ctrl)->slot_cap & PCI_EXP_SLTCAP_NCCS)
#define PSN(ctrl)		(((ctrl)->slot_cap & PCI_EXP_SLTCAP_PSN) >> 19)

void pciehp_request(struct controller *ctrl, int action);
void pciehp_handle_button_press(struct controller *ctrl);
void pciehp_handle_disable_request(struct controller *ctrl);
void pciehp_handle_presence_or_link_change(struct controller *ctrl, u32 events);
int pciehp_configure_device(struct controller *ctrl);
int pciehp_unconfigure_device(struct controller *ctrl, bool presence);
void pciehp_queue_pushbutton_work(struct work_struct *work);
struct controller *pcie_init(struct pcie_device *dev);
int pcie_init_notification(struct controller *ctrl);
void pcie_shutdown_notification(struct controller *ctrl);
int pciehp_enable_slot(struct controller *ctrl);
int pciehp_disable_slot(struct controller *ctrl, bool safe_removal);
void pcie_enable_interrupt(struct controller *ctrl);
void pcie_disable_interrupt(struct controller *ctrl);
void pcie_clear_hotplug_events(struct controller *ctrl);
int pciehp_power_on_slot(struct controller *ctrl);
void pciehp_power_off_slot(struct controller *ctrl);
void pciehp_get_power_status(struct controller *ctrl, u8 *status);

void pciehp_set_attention_status(struct controller *ctrl, u8 status);
void pciehp_get_latch_status(struct controller *ctrl, u8 *status);
int pciehp_query_power_fault(struct controller *ctrl);
void pciehp_green_led_on(struct controller *ctrl);
void pciehp_green_led_off(struct controller *ctrl);
void pciehp_green_led_blink(struct controller *ctrl);
bool pciehp_card_present(struct controller *ctrl);
bool pciehp_card_present_or_link_active(struct controller *ctrl);
int pciehp_check_link_status(struct controller *ctrl);
bool pciehp_check_link_active(struct controller *ctrl);
void pciehp_release_ctrl(struct controller *ctrl);

int pciehp_sysfs_enable_slot(struct hotplug_slot *hotplug_slot);
int pciehp_sysfs_disable_slot(struct hotplug_slot *hotplug_slot);
int pciehp_reset_slot(struct hotplug_slot *hotplug_slot, int probe);
int pciehp_get_attention_status(struct hotplug_slot *hotplug_slot, u8 *status);
int pciehp_set_raw_indicator_status(struct hotplug_slot *h_slot, u8 status);
int pciehp_get_raw_indicator_status(struct hotplug_slot *h_slot, u8 *status);

static inline const char *slot_name(struct controller *ctrl)
{
	return hotplug_slot_name(ctrl->hotplug_slot);
}

#endif				/* _PCIEHP_H */
