/*
 * Copyright (c) 2018 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lvgl_mem.h"
#include <zephyr.h>
#include <init.h>
#include <sys/sys_heap.h>

#if defined(CONFIG_LV_Z_MEM_POOL_SYS_HEAP) && !defined(CONFIG_UI_MEMORY_MANAGER)

static char lvgl_heap_mem[CONFIG_LV_Z_MEM_POOL_SIZE] __aligned(8) __in_section_unique(lvgl.noinit.malloc);
static struct sys_heap lvgl_heap;
static struct k_spinlock lvgl_heap_lock;

void *lvgl_malloc(size_t size, const char *func)
{
	k_spinlock_key_t key;
	void *ret;

	key = k_spin_lock(&lvgl_heap_lock);
	ret = sys_heap_alloc(&lvgl_heap, size);
	k_spin_unlock(&lvgl_heap_lock, key);

	return ret;
}

void *lvgl_realloc(void *ptr, size_t size, const char *func)
{
	k_spinlock_key_t key;
	void *ret;

	key = k_spin_lock(&lvgl_heap_lock);
	ret = sys_heap_realloc(&lvgl_heap, ptr, size);
	k_spin_unlock(&lvgl_heap_lock, key);

	return ret;
}

void lvgl_free(void *ptr, const char *func)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&lvgl_heap_lock);
	sys_heap_free(&lvgl_heap, ptr);
	k_spin_unlock(&lvgl_heap_lock, key);
}

void lvgl_heap_init(void)
{
	sys_heap_init(&lvgl_heap, &lvgl_heap_mem[0], CONFIG_LV_Z_MEM_POOL_SIZE);
}

#endif /* defined(CONFIG_LV_Z_MEM_POOL_SYS_HEAP) && !defined(CONFIG_UI_MEMORY_MANAGER) */
