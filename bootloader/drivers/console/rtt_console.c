/* rtt_console.c - Console messages to a RAM buffer that is then read by
 * the Segger J-Link debugger
 */

/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <kernel.h>
#include <sys/printk.h>
#include <device.h>
#include <init.h>
#include <SEGGER_RTT.h>

extern void __printk_hook_install(int (*fn)(int));
extern void __stdout_hook_install(int (*fn)(int));

static bool host_present;

/** @brief Wait for fixed period.
 *
 */
static void wait(void)
{
	if (!IS_ENABLED(CONFIG_MULTITHREADING) || k_is_in_isr()) {
		if (IS_ENABLED(CONFIG_RTT_TX_RETRY_IN_INTERRUPT)) {
			k_busy_wait(1000*CONFIG_RTT_TX_RETRY_DELAY_MS);
		}
	} else {
		k_msleep(CONFIG_RTT_TX_RETRY_DELAY_MS);
	}
}

static int rtt_console_out(int character)
{
	char c = (char)character;
	unsigned int cnt;
	int max_cnt = CONFIG_RTT_TX_RETRY_CNT;
	uint32_t key;

	do {
		/* bug fix: can not using mutex in pre-kernel phase */
		key = irq_lock();
		cnt = SEGGER_RTT_WriteNoLock(0, &c, 1);
		irq_unlock(key);

		/* There are two possible reasons for not writing any data to
		 * RTT:
		 * - The host is not connected and not reading the data.
		 * - The buffer got full and will be read by the host.
		 * These two situations are distinguished using the following
		 * algorithm:
		 * At the beginning, the module assumes that the host is active,
		 * so when no data is read, it busy waits and retries.
		 * If, after retrying, the host reads the data, the module
		 * assumes that the host is active. If it fails, the module
		 * assumes that the host is inactive and stores that
		 * information. On next call, only one attempt takes place.
		 * The host is marked as active if the attempt is successful.
		 */
		if (cnt) {
			/* byte processed - host is present. */
			host_present = true;
		} else if (host_present) {
			if (max_cnt) {
				/* bug fix: can not sleep in pre-kernel phase */
				//wait();
				max_cnt--;
				continue;
			} else {
				host_present = false;
			}
		}

		break;
	} while (1);

	return character;
}

static int rtt_console_init(const struct device *d)
{
	ARG_UNUSED(d);

	__printk_hook_install(rtt_console_out);
	__stdout_hook_install(rtt_console_out);

	return 0;
}

SYS_INIT(rtt_console_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
