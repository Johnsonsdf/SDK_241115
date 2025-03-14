/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <kernel.h>
#include <timeout_q.h>
#include <init.h>
#include <string.h>
#include <pm/pm.h>
#include <pm/state.h>
#include <pm/policy.h>
#include <tracing/tracing.h>

#include "pm_priv.h"

#define PM_STATES_LEN (1 + PM_STATE_SOFT_OFF - PM_STATE_ACTIVE)
#define LOG_LEVEL CONFIG_PM_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(power);

static int post_ops_done = 1;
static struct pm_state_info z_power_state;
static sys_slist_t pm_notifiers = SYS_SLIST_STATIC_INIT(&pm_notifiers);
static struct k_spinlock pm_notifier_lock;

//#define CONFIG_PM_DEBUG
#ifdef CONFIG_PM_DEBUG
static uint32_t timer_start;

static inline void pm_debug_start_timer(void)
{
	timer_start = k_cycle_get_32();
}

static void pm_debug_stop_timer(const char *msg)
{
	uint32_t res =  k_cycle_get_32() - timer_start;
	printk("%s: use %d us\n",msg, k_cyc_to_us_ceil32(res));

}

#else
static inline void pm_debug_start_timer(void) { }
static void pm_debug_stop_timer(const char *msg) { }

#endif


 __weak void
	pm_power_state_exit_post_ops(struct pm_state_info info)
{
	irq_unlock(0);
}

static inline void exit_pos_ops(struct pm_state_info info)
{
	pm_power_state_exit_post_ops(info);
}

 __weak void pm_power_state_set(struct pm_state_info info)
{

}

static inline void pm_state_set(struct pm_state_info info)
{
	pm_power_state_set(info);
}

/*
 * Function called to notify when the system is entering / exiting a
 * power state
 */
static inline void pm_state_notify(bool entering_state)
{
	struct pm_notifier *notifier;
	k_spinlock_key_t pm_notifier_key;
	void (*callback)(enum pm_state state);

	pm_notifier_key = k_spin_lock(&pm_notifier_lock);
	SYS_SLIST_FOR_EACH_CONTAINER(&pm_notifiers, notifier, _node) {
		if (entering_state) {
			callback = notifier->state_entry;
		} else {
			callback = notifier->state_exit;
		}

		if (callback) {
			callback(z_power_state.state);
		}
	}
	k_spin_unlock(&pm_notifier_lock, pm_notifier_key);
}

void pm_system_resume(void)
{
	/*
	 * This notification is called from the ISR of the event
	 * that caused exit from kernel idling after PM operations.
	 *
	 * Some CPU low power states require enabling of interrupts
	 * atomically when entering those states. The wake up from
	 * such a state first executes code in the ISR of the interrupt
	 * that caused the wake. This hook will be called from the ISR.
	 * For such CPU LPS states, do post operations and restores here.
	 * The kernel scheduler will get control after the ISR finishes
	 * and it may schedule another thread.
	 *
	 * Call pm_idle_exit_notification_disable() if this
	 * notification is not required.
	 */
	if (!post_ops_done) {
		post_ops_done = 1;		
		pm_state_notify(false);
		exit_pos_ops(z_power_state);
	}
}

void pm_power_state_force(struct pm_state_info info)
{
	__ASSERT(info.state < PM_STATES_LEN,
		 "Invalid power state %d!", info.state);

	if (info.state == PM_STATE_ACTIVE) {
		return;
	}

	(void)arch_irq_lock();
	z_power_state = info;
	post_ops_done = 0;
	pm_state_notify(true);

	k_sched_lock();
	pm_debug_start_timer();
	/* Enter power state */
	pm_state_set(z_power_state);
	pm_debug_stop_timer("--sleep--");

	pm_system_resume();
	k_sched_unlock();
}

#if CONFIG_PM_DEVICE
static enum pm_state _handle_device_abort(struct pm_state_info info)
{
	LOG_DBG("Some devices didn't enter suspend state!");
	pm_resume_devices();

	z_power_state.state = PM_STATE_ACTIVE;
	return PM_STATE_ACTIVE;
}
#endif

void pm_abort_enter_sleep(void)
{
	post_ops_done = 0; /*for idle handle pm_state_notify exit, standby need notify to exit S3 */
}

enum pm_state pm_system_suspend(int32_t ticks)
{
	SYS_PORT_TRACING_FUNC_ENTER(pm, system_suspend, ticks);
	z_power_state = pm_policy_next_state(ticks);
	if (z_power_state.state == PM_STATE_ACTIVE) {
		LOG_DBG("No PM operations done.");
		SYS_PORT_TRACING_FUNC_EXIT(pm, system_suspend, ticks, z_power_state.state);
		return z_power_state.state;
	}
	post_ops_done = 0;

	if (ticks != K_TICKS_FOREVER) {
		/*
		 * Just a sanity check in case the policy manager does not
		 * handle this error condition properly.
		 */
		__ASSERT(z_power_state.min_residency_us >=
			z_power_state.exit_latency_us,
			"min_residency_us < exit_latency_us");

		/*
		 * We need to set the timer to interrupt a little bit early to
		 * accommodate the time required by the CPU to fully wake up.
		 */
		z_set_timeout_expiry(ticks -
		     k_us_to_ticks_ceil32(z_power_state.exit_latency_us), true);
	}
	
#if CONFIG_PM_DEVICE

	bool should_resume_devices = true;
	pm_debug_start_timer();
	switch (z_power_state.state) {
	case PM_STATE_SUSPEND_TO_IDLE:
		__fallthrough;
	case PM_STATE_STANDBY:
		/* low power peripherals. */
		if (pm_low_power_devices()) {
			SYS_PORT_TRACING_FUNC_EXIT(pm, system_suspend,
					ticks, _handle_device_abort(z_power_state));
			return _handle_device_abort(z_power_state);
		}
		break;
	case PM_STATE_SUSPEND_TO_RAM:
		__fallthrough;
	case PM_STATE_SUSPEND_TO_DISK:
		__fallthrough;
	case PM_STATE_SOFT_OFF:
		if (pm_suspend_devices()) {
			SYS_PORT_TRACING_FUNC_EXIT(pm, system_suspend,
					ticks, _handle_device_abort(z_power_state));
			return _handle_device_abort(z_power_state);
		}
		break;
	default:
		should_resume_devices = false;
		break;
	}
	pm_debug_stop_timer("--suspend--");
#endif
	/*
	 * This function runs with interruptions locked but it is
	 * expected the SoC to unlock them in
	 * pm_power_state_exit_post_ops() when returning to active
	 * state. We don't want to be scheduled out yet, first we need
	 * to send a notification about leaving the idle state. So,
	 * we lock the scheduler here and unlock just after we have
	 * sent the notification in pm_system_resume().
	 */
	k_sched_lock();
	pm_debug_start_timer();
	/* Enter power state */
	pm_state_notify(true);
	pm_debug_stop_timer("--notify--");
	pm_debug_start_timer();
	pm_state_set(z_power_state);
	pm_debug_stop_timer("--s3--");

	pm_debug_start_timer();
	/* Wake up sequence starts here */
#if CONFIG_PM_DEVICE
	if (should_resume_devices) {
		/* Turn on peripherals and restore device states as necessary */
		pm_resume_devices();
	}
#endif
	pm_system_resume();
	pm_debug_stop_timer("--resume--");
	k_sched_unlock();
	SYS_PORT_TRACING_FUNC_EXIT(pm, system_suspend, ticks, z_power_state.state);
	return z_power_state.state;
}

void pm_notifier_register(struct pm_notifier *notifier)
{
	k_spinlock_key_t pm_notifier_key = k_spin_lock(&pm_notifier_lock);

	sys_slist_append(&pm_notifiers, &notifier->_node);
	k_spin_unlock(&pm_notifier_lock, pm_notifier_key);
}

int pm_notifier_unregister(struct pm_notifier *notifier)
{
	int ret = -EINVAL;
	k_spinlock_key_t pm_notifier_key;

	pm_notifier_key = k_spin_lock(&pm_notifier_lock);
	if (sys_slist_find_and_remove(&pm_notifiers, &(notifier->_node))) {
		ret = 0;
	}
	k_spin_unlock(&pm_notifier_lock, pm_notifier_key);

	return ret;
}
