/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <os_common_api.h>

#if CONFIG_LVGL_DISPLAY_FLUSH_WORKQ_STACKSIZE > 0

static OS_THREAD_STACK_DEFINE(lvgl_workq_stack, CONFIG_LVGL_DISPLAY_FLUSH_WORKQ_STACKSIZE);
static os_work_q lvgl_workq;
static bool lvgl_workq_inited = false;

os_work_q * lvgl_display_get_flush_workq(void)
{
	if (lvgl_workq_inited == false) {
		lvgl_workq_inited = true;

		k_work_queue_start(&lvgl_workq, lvgl_workq_stack,
					K_KERNEL_STACK_SIZEOF(lvgl_workq_stack),
					CONFIG_UISRV_PRIORITY - 1, NULL);
		os_thread_name_set(&lvgl_workq.thread, "lvglworkq");
	}

	return &lvgl_workq;
}

#else

os_work_q * lvgl_display_get_flush_workq(void)
{
	return NULL;
}

#endif /* CONFIG_LVGL_DISPLAY_FLUSH_WORKQ_STACKSIZE > 0 */
