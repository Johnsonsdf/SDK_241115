/*
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>

static void z_work_user_q_main(void *work_q_ptr, void *p2, void *p3)
{
	struct k_work_user_q *work_q = work_q_ptr;
	uint32_t start_time, stop_time;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		struct k_work_user *work;
		k_work_user_handler_t handler;

		work = k_queue_get(&work_q->queue, K_FOREVER);
		if (work == NULL) {
			continue;
		}

		handler = work->handler;
		__ASSERT(handler != NULL, "handler must be provided");

		/* Reset pending state so it can be resubmitted by handler */
		if (atomic_test_and_clear_bit(&work->flags,
					      K_WORK_USER_STATE_PENDING)) {
			handler(work);
			stop_time = k_uptime_get_32();
			if ((stop_time - start_time) > 10) {
				printk("work %p run too long %d ms!!!\n", handler, stop_time - start_time);
			}
		}

		/* Make sure we don't hog up the CPU if the FIFO never (or
		 * very rarely) gets empty.
		 */
		k_yield();
	}
}

void k_work_user_queue_start(struct k_work_user_q *work_q, k_thread_stack_t *stack,
			     size_t stack_size, int prio, const char *name)
{
	k_queue_init(&work_q->queue);

	/* Created worker thread will inherit object permissions and memory
	 * domain configuration of the caller
	 */
	k_thread_create(&work_q->thread, stack, stack_size, z_work_user_q_main,
			work_q, NULL, NULL, prio, K_USER | K_INHERIT_PERMS,
			K_FOREVER);
	k_object_access_grant(&work_q->queue, &work_q->thread);
	if (name != NULL) {
		k_thread_name_set(&work_q->thread, name);
	}

	k_thread_start(&work_q->thread);
}
