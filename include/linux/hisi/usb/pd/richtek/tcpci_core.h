/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * Author: TH <tsunghan_tsai@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_RT_TCPCI_CORE_H
#define __LINUX_RT_TCPCI_CORE_H

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/semaphore.h>

#include <linux/hisi/usb/pd/richtek/tcpm.h>
#include <linux/hisi/usb/pd/richtek/tcpci_timer.h>
#include <linux/hisi/usb/pd/richtek/tcpci_config.h>

#ifdef CONFIG_USB_POWER_DELIVERY
#include <linux/hisi/usb/pd/richtek/pd_core.h>
#endif

/* The switch of log message */
#define TYPEC_INFO_ENABLE	1
#define PE_EVENT_DBG_ENABLE	0
#define PE_STATE_INFO_ENABLE	0
#define TCPC_INFO_ENABLE	1
#define TCPC_TIMER_DBG_EN	0
#define TCPC_TIMER_INFO_EN	0
#define PE_INFO_ENABLE		1
#define TCPC_DBG_ENABLE		0
#define DPM_DBG_ENABLE		0
#define PD_ERR_ENABLE		1
#define PE_DBG_ENABLE		0
#define TYPEC_DBG_ENABLE	0

#define TCPC_ENABLE_ANYMSG	(TCPC_DBG_ENABLE | DPM_DBG_ENABLE | \
		PD_ERR_ENABLE | PE_INFO_ENABLE | TCPC_TIMER_INFO_EN\
		| PE_DBG_ENABLE | PE_EVENT_DBG_ENABLE | \
		PE_STATE_INFO_ENABLE | TCPC_INFO_ENABLE | \
		TCPC_TIMER_DBG_EN | TYPEC_DBG_ENABLE | \
		TYPEC_INFO_ENABLE)

#define PE_EVT_INFO_VDM_DIS		0
#define PE_DBG_RESET_VDM_DIS	1

#ifndef BIT
#define BIT(x)	(1 << (x))
#endif

struct tcpc_device;

struct tcpc_desc {
	u8 role_def;
	u8 rp_lvl;
	int notifier_supply_num;
	char *name;
};

/* TCPC Power Register Define */
#define TCPC_REG_POWER_STATUS_EXT_VSAFE0V	BIT(15)	/* extend */
#define TCPC_REG_POWER_STATUS_VBUS_PRES		BIT(2)

/* TCPC Alert Register Define */
#define TCPC_REG_ALERT_EXT_RA_DETACH		(1 << (16 + 5))
#define TCPC_REG_ALERT_EXT_WATCHDOG		(1 << (16 + 2))
#define TCPC_REG_ALERT_EXT_VBUS_80		(1 << (16 + 1))
#define TCPC_REG_ALERT_EXT_WAKEUP		(1 << (16 + 0))

#define TCPC_REG_ALERT_VBUS_DISCNCT BIT(11)
#define TCPC_REG_ALERT_RX_BUF_OVF   BIT(10)
#define TCPC_REG_ALERT_FAULT        BIT(9)
#define TCPC_REG_ALERT_V_ALARM_LO   BIT(8)
#define TCPC_REG_ALERT_V_ALARM_HI   BIT(7)
#define TCPC_REG_ALERT_TX_SUCCESS   BIT(6)
#define TCPC_REG_ALERT_TX_DISCARDED BIT(5)
#define TCPC_REG_ALERT_TX_FAILED    BIT(4)
#define TCPC_REG_ALERT_RX_HARD_RST  BIT(3)
#define TCPC_REG_ALERT_RX_STATUS    BIT(2)
#define TCPC_REG_ALERT_POWER_STATUS BIT(1)
#define TCPC_REG_ALERT_CC_STATUS    BIT(0)
#define TCPC_REG_ALERT_TX_COMPLETE  (TCPC_REG_ALERT_TX_SUCCESS | \
		TCPC_REG_ALERT_TX_DISCARDED | \
		TCPC_REG_ALERT_TX_FAILED)

/* TCPC Behavior Flags */
#define TCPC_FLAGS_RETRY_CRC_DISCARD		BIT(0)
#define TCPC_FLAGS_WAIT_HRESET_COMPLETE		BIT(1)
#define TCPC_FLAGS_CHECK_CC_STABLE		BIT(2)
#define TCPC_FLAGS_LPM_WAKEUP_WATCHDOG		BIT(3)
#define TCPC_FLAGS_CHECK_RA_DETACHE		BIT(4)

enum tcpc_cc_pull {
	TYPEC_CC_RA = 0,
	TYPEC_CC_RP = 1,
	TYPEC_CC_RD = 2,
	TYPEC_CC_OPEN = 3,
	TYPEC_CC_DRP = 4,	/* from Rd */

	TYPEC_CC_RP_DFT = 1,		/* 0x00 + 1 */
	TYPEC_CC_RP_1_5 = 9,		/* 0x08 + 1*/
	TYPEC_CC_RP_3_0 = 17,		/* 0x10 + 1 */

	TYPEC_CC_DRP_DFT = 4,		/* 0x00 + 4 */
	TYPEC_CC_DRP_1_5 = 12,		/* 0x08 + 4 */
	TYPEC_CC_DRP_3_0 = 20,		/* 0x10 + 4 */
};

#define TYPEC_CC_PULL_GET_RES(pull)		((pull) & 0x07)
#define TYPEC_CC_PULL_GET_RP_LVL(pull)	(((pull) & 0x18) >> 3)

enum tcpm_transmit_type {
	TCPC_TX_SOP = 0,
	TCPC_TX_SOP_PRIME = 1,
	TCPC_TX_SOP_PRIME_PRIME = 2,
	TCPC_TX_SOP_DEBUG_PRIME = 3,
	TCPC_TX_SOP_DEBUG_PRIME_PRIME = 4,
	TCPC_TX_HARD_RESET = 5,
	TCPC_TX_CABLE_RESET = 6,
	TCPC_TX_BIST_MODE_2 = 7
};

enum tcpm_rx_cap_type {
	TCPC_RX_CAP_SOP = 1 << 0,
	TCPC_RX_CAP_SOP_PRIME = 1 << 1,
	TCPC_RX_CAP_SOP_PRIME_PRIME = 1 << 2,
	TCPC_RX_CAP_SOP_DEBUG_PRIME = 1 << 3,
	TCPC_RX_CAP_SOP_DEBUG_PRIME_PRIME = 1 << 4,
	TCPC_RX_CAP_HARD_RESET = 1 << 5,
	TCPC_RX_CAP_CABLE_RESET = 1 << 6,
};

struct tcpc_ops {
	int (*init)(struct tcpc_device *tcpc, bool sw_reset);
	int (*alert_status_clear)(struct tcpc_device *tcpc, u32 mask);
	int (*fault_status_clear)(struct tcpc_device *tcpc, u8 status);
	int (*get_alert_status)(struct tcpc_device *tcpc, u32 *alert);
	int (*get_power_status)(struct tcpc_device *tcpc, u16 *pwr_status);
	int (*get_fault_status)(struct tcpc_device *tcpc, u8 *status);
	int (*get_cc)(struct tcpc_device *tcpc, int *cc1, int *cc2);
	int (*set_cc)(struct tcpc_device *tcpc, int pull);
	int (*set_polarity)(struct tcpc_device *tcpc, int polarity);
	int (*set_vconn)(struct tcpc_device *tcpc, int enable);

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	int (*set_low_power_mode)(struct tcpc_device *tcpc, bool en, int pull);
#endif /* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_USB_POWER_DELIVERY
	int (*set_msg_header)(struct tcpc_device *tcpc,
			      int power_role, int data_role);
	int (*set_rx_enable)(struct tcpc_device *tcpc, u8 enable);
	int (*get_message)(struct tcpc_device *tcpc, u32 *payload,
			   u16 *head, enum tcpm_transmit_type *type);
	int (*transmit)(struct tcpc_device *tcpc,
			enum tcpm_transmit_type type,
			u16 header, const u32 *data);
	int (*set_bist_test_mode)(struct tcpc_device *tcpc, bool en);
	int (*set_bist_carrier_mode)(struct tcpc_device *tcpc, u8 pattern);

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	int (*retransmit)(struct tcpc_device *tcpc);
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
#endif	/* CONFIG_USB_POWER_DELIVERY */
};

#define TCPC_VBUS_SOURCE_0V		(0)
#define TCPC_VBUS_SOURCE_5V		(5000)

#define TCPC_VBUS_SINK_0V		(0)
#define TCPC_VBUS_SINK_5V		(5000)

#define TCPC_LEGACY_CABLE_CONFIRM	50

struct tcpc_device {
	struct i2c_client *client;
	struct tcpc_ops *ops;
	void *drv_data;
	struct tcpc_desc desc;
	struct device dev;
	struct wakeup_source attach_wake_lock;
	struct wakeup_source dettach_temp_wake_lock;
	/* For tcpc timer & event */
	u32 timer_handle_index;
	struct hrtimer tcpc_timer[PD_TIMER_NR];

	ktime_t last_expire[PD_TIMER_NR];
	struct delayed_work timer_handle_work[2];
	struct mutex access_lock;
	struct mutex typec_lock;
	struct mutex timer_lock;
	struct semaphore timer_enable_mask_lock;
	struct semaphore timer_tick_lock;
	atomic_t pending_event;
	u64 timer_tick;
	u64 timer_enable_mask;
	wait_queue_head_t event_loop_wait_que;
	wait_queue_head_t  timer_wait_que;
	struct task_struct *event_task;
	struct task_struct *timer_task;
	bool timer_thead_stop;
	bool event_loop_thead_stop;

	struct delayed_work	init_work;
	struct srcu_notifier_head evt_nh;

	/* For TCPC TypeC */
	u8 typec_state;
	u8 typec_role;
	u8 typec_attach_old;
	u8 typec_attach_new;
	u8 typec_local_cc;
	u8 typec_local_rp_level;
	u8 typec_remote_cc[2];
	u8 typec_remote_rp_level;
	u8 typec_wait_ps_change;
	bool typec_polarity;
	bool typec_drp_try_timeout;
	bool typec_lpm;
	bool typec_cable_only;
	bool typec_power_ctrl;

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	bool typec_legacy_cable;
	u8 typec_legacy_cable_suspect;
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

#ifdef CONFIG_USB_POWER_DELIVERY
	/* Event */
	u8 pd_event_count;
	u8 pd_event_head_index;
	u8 pd_msg_buffer_allocated;

	u8 pd_last_vdm_msg_id;
	bool pd_pending_vdm_event;
	bool pd_pending_vdm_good_crc;
	bool pd_postpone_vdm_timeout;

	pd_msg_t pd_last_vdm_msg;
	pd_event_t pd_vdm_event;

	pd_msg_t pd_msg_buffer[PD_MSG_BUF_SIZE];
	pd_event_t pd_event_ring_buffer[PD_EVENT_BUF_SIZE];

	bool pd_wait_pe_idle;
	bool pd_hard_reset_event_pending;
	bool pd_wait_hard_reset_complete;
	bool pd_wait_pr_swap_complete;
	bool pd_wait_error_recovery;
	bool pd_ping_event_pending;
	u8 pd_bist_mode;
	u8 pd_transmit_state;
	int pd_wait_vbus_once;

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	bool pd_discard_pending;
#endif

	u8 tcpc_flags;

	pd_port_t pd_port;
#endif /* CONFIG_USB_POWER_DELIVERY */
	u8 vbus_level:2;
	u8 irq_enabled:1;

	struct notifier_block dpm_nb;
};

#define to_tcpc_device(obj) container_of(obj, struct tcpc_device, dev)

#define RT_DBG_INFO	    hisilog_info
#define RT_DBG_ERR      hisilog_err

#if TYPEC_DBG_ENABLE
#define TYPEC_DBG(format, args...)		\
	RT_DBG_INFO("[TPC-D]" format, ##args)
#else
#define TYPEC_DBG(format, args...)
#endif /* TYPEC_DBG_ENABLE */

#if TYPEC_INFO_ENABLE
#define TYPEC_INFO(format, args...)	\
	RT_DBG_INFO("TPC-I:" format, ##args)
#else
#define TYPEC_INFO(format, args...)
#endif /* TYPEC_INFO_ENABLE */

#if TCPC_INFO_ENABLE
#define TCPC_INFO(format, args...)	\
	RT_DBG_INFO("[TCPC-I]" format, ##args)
#else
#define TCPC_INFO(foramt, args...)
#endif /* TCPC_INFO_ENABLE */

#if TCPC_DBG_ENABLE
#define TCPC_DBG(format, args...)	\
	RT_DBG_INFO("[TCPC-D]" format, ##args)
#else
#define TCPC_DBG(format, args...)
#endif /* TCPC_DBG_ENABLE */

#define TCPC_ERR(format, args...)	\
	RT_DBG_ERR("[TCPC-E]" format, ##args)

#define DP_ERR(format, args...)	\
	RT_DBG_ERR("[DP-E]" format, ##args)

#if DPM_DBG_ENABLE
#define DPM_DBG(format, args...)	\
	RT_DBG_INFO("DPM-D:" format, ##args)
#else
#define DPM_DBG(format, args...)
#endif /* DPM_DBG_ENABLE */

#if PD_ERR_ENABLE
#define PD_ERR(format, args...) \
	RT_DBG_ERR("PD-E:" format, ##args)
#else
#define PD_ERR(format, args...)
#endif /* PD_ERR_ENABLE */

#if PE_INFO_ENABLE
#define PE_INFO(format, args...)	\
	RT_DBG_INFO("PE:" format, ##args)
#else
#define PE_INFO(format, args...)
#endif /* PE_INFO_ENABLE */

#if PE_EVENT_DBG_ENABLE
#define PE_EVT_INFO(format, args...) \
	RT_DBG_INFO("PE-E:" format, ##args)
#else
#define PE_EVT_INFO(format, args...)
#endif /* PE_EVENT_DBG_ENABLE */

#if PE_DBG_ENABLE
#define PE_DBG(format, args...)	\
	RT_DBG_INFO("PE:" format, ##args)
#else
#define PE_DBG(format, args...)
#endif /* PE_DBG_ENABLE */

#if PE_STATE_INFO_ENABLE
#define PE_STATE_INFO(format, args...) \
	RT_DBG_INFO("PE:" format, ##args)
#else
#define PE_STATE_INFO(format, args...)
#endif /* PE_STATE_IFNO_ENABLE */

#endif /* #ifndef __LINUX_RT_TCPCI_CORE_H */
