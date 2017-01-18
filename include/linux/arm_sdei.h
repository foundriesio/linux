// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017 Arm Ltd.
#ifndef __LINUX_ARM_SDEI_H
#define __LINUX_ARM_SDEI_H

#include <uapi/linux/arm_sdei.h>

enum sdei_conduit_types {
	CONDUIT_INVALID = 0,
	CONDUIT_SMC,
	CONDUIT_HVC,
};

#include <acpi/ghes.h>

#ifdef CONFIG_ARM_SDE_INTERFACE
#include <asm/sdei.h>
#endif

/* Arch code should override this to set the entry point from firmware... */
#ifndef sdei_arch_get_entry_point
#define sdei_arch_get_entry_point(conduit)	(0)
#endif

/*
 * When an event occurs sdei_event_handler() will call a user-provided callback
 * like this in NMI context on the CPU that received the event.
 */
typedef int (sdei_event_callback)(u32 event, struct pt_regs *regs, void *arg);

/*
 * Register your callback to claim an event. The event must be described
 * by firmware.
 */
int sdei_event_register(u32 event_num, sdei_event_callback *cb, void *arg);

/*
 * Calls to sdei_event_unregister() may return EINPROGRESS. Keep calling
 * it until it succeeds.
 */
int sdei_event_unregister(u32 event_num);

int sdei_event_enable(u32 event_num);

/*
 * You can disable the running event from within the handler. If you do this
 * for a private event, only the local CPU is affected.
 */
int sdei_event_disable(u32 event_num);

/* GHES register/unregister helpers */
int sdei_register_ghes(struct ghes *ghes, sdei_event_callback *normal_cb,
		       sdei_event_callback *critical_cb);
int sdei_unregister_ghes(struct ghes *ghes);

/*
 * get/set the event routing.
 * These calls take linux's cpu numbers and convert them to firmware affinity.
 * !directed causes the event to be delivered to a CPU of the firmware's choice.
 *
 * This information is not saved/restored by the driver.
 * The caller should handle CPU hotplug of its chosen CPU.
 */
int sdei_event_routing_set(u32 event_num, bool directed, int to_cpu);
int sdei_event_routing_get(u32 event_num, bool *directed, int *to_cpu);

/*
 * bind and register an interrupt as an SDE event.
 *
 * this allows you to receive this interrupt as an unmaskable SDE event using
 * the resulting event number. Unregistering this event will unbind the
 * interrupt.
 *
 * If your irq was level triggered, you are still responsible for notifying
 * the device the interrupt has been handled, otherwise you will repeatedly
 * receive the same event. If you can't do this from the NMI-like SDE context,
 * disable the event from your handler. You should schedule some work to notify
 * the device, then re-enable the event.
 *
 * You should not bind a shared interrupt.
 *
 * You must call free_irq() before sdei_interrupt_bind() and re- request_irq()
 * after unregistering the event. synchronize_irq() ensures no CPU is handling
 * this interrupt before we bind it.
 *
 * If you use sdei_event_routing_set() to manipulate the routing of the bound
 * interrupt you should register a CPU hotplug notifier and ensure the
 * interrupt is not routed to a CPU that is being powered off. The irqchip
 * code no longer does this for you and once you use sdei_event_routing_set()
 * firmware doesn't either.
 *
 * You should register a hibnernate resume hook to re-bind and re-register your
 * event on resume from hibernate. Events are unregistered and the SDE interface
 * is reset over hibernate, but bound events may be allocated different event
 * numbers, so the driver makes no attempt to re-bind and re-register them.
 */
int sdei_interrupt_bind_and_register(u32 irq_num, sdei_event_callback *cb,
				     void *arg, u32 *event_num);

#ifdef CONFIG_ARM_SDE_INTERFACE
/* For use by arch code when CPU hotplug notifiers are not appropriate. */
int sdei_mask_local_cpu(void);
int sdei_unmask_local_cpu(void);
#else
static inline int sdei_mask_local_cpu(void) { return 0; }
static inline int sdei_unmask_local_cpu(void) { return 0; }
#endif /* CONFIG_ARM_SDE_INTERFACE */


/*
 * This struct represents an event that has been registered. The driver
 * maintains a list of all events, and which ones are registered. (Private
 * events have one entry in the list, but are registered on each CPU).
 * A pointer to this struct is passed to firmware, and back to the event
 * handler. The event handler can then use this to invoke the registered
 * callback, without having to walk the list.
 *
 * For CPU private events, this structure is per-cpu.
 */
struct sdei_registered_event {
	/* For use by arch code: */
	struct pt_regs          interrupted_regs;

	sdei_event_callback	*callback;
	void			*callback_arg;
	u32			 event_num;
	u8			 priority;
};

/* The arch code entry point should then call this when an event arrives. */
int notrace sdei_event_handler(struct pt_regs *regs,
			       struct sdei_registered_event *arg);

/* arch code may use this to retrieve the extra registers. */
int sdei_api_event_context(u32 query, u64 *result);

#endif /* __LINUX_ARM_SDEI_H */
