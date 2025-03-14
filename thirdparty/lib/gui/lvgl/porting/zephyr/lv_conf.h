/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <linker/linker-defs.h>

/* Memory manager settings */

#define LV_MEMCPY_MEMSET_STD  1

#if !defined(CONFIG_LV_MEM_CUSTOM)

#  define LV_MEM_SIZE    (CONFIG_LV_MEM_SIZE_KILOBYTES * 1024U) /*[bytes]*/
#  define LV_MEM_ADR     0     /*0: unused*/

#elif defined(CONFIG_UI_MEMORY_MANAGER)

#  define LV_MEM_CUSTOM_INCLUDE                   "ui_mem.h"
#  define LV_MEM_CUSTOM_ALLOC(size, ...)          ui_mem_alloc(MEM_GUI, size, ##__VA_ARGS__)
#  define LV_MEM_CUSTOM_REALLOC(ptr, size, ...)   ui_mem_realloc(MEM_GUI, ptr, size, ##__VA_ARGS__)
#  define LV_MEM_CUSTOM_FREE(ptr, ...)            ui_mem_free(MEM_GUI, ptr)

#elif defined(CONFIG_LV_Z_MEM_POOL_HEAP_LIB_C)

#  define LV_MEM_CUSTOM_INCLUDE                   "stdlib.h"
#  define LV_MEM_CUSTOM_ALLOC(size, ...)          malloc(size)
#  define LV_MEM_CUSTOM_REALLOC(ptr, size, ...)   realloc(ptr, size)
#  define LV_MEM_CUSTOM_FREE(ptr, ...)            free(ptr)

#else

#  define LV_MEM_CUSTOM_INCLUDE                   "../../porting/zephyr/lvgl_mem.h"
#  define LV_MEM_CUSTOM_ALLOC(size, ...)          lvgl_malloc(size, ##__VA_ARGS__)
#  define LV_MEM_CUSTOM_REALLOC(ptr, size, ...)   lvgl_realloc(ptr, size, ##__VA_ARGS__)
#  define LV_MEM_CUSTOM_FREE(ptr, ...)            lvgl_free(ptr, ##__VA_ARGS__)

#endif

/* HAL settings */

#define LV_TICK_CUSTOM  1
#if CONFIG_SYS_CLOCK_TICKS_PER_SEC >= 1000
#  define LV_TICK_CUSTOM_INCLUDE        "zephyr.h"
#  define LV_TICK_CUSTOM_SYS_TIME_EXPR  (k_uptime_get_32())
#else
#  define LV_TICK_CUSTOM_INCLUDE        "../../porting/zephyr/lvgl_tick.h"
#  define LV_TICK_CUSTOM_SYS_TIME_EXPR  (lvgl_tick_get())
#endif

/* Misc settings */

#define LV_SPRINTF_CUSTOM 1
#define LV_SPRINTF_INCLUDE "stdio.h"
#define lv_snprintf snprintf
#define lv_vsnprintf vsnprintf

/* Asserts */

#define LV_ASSERT_HANDLER_INCLUDE <sys/__assert.h>
#define LV_ASSERT_HANDLER __ASSERT(0, "LVGL fail");   /*Halt by default*/

/*Will be added where memories needs to be aligned (with -Os data might not be aligned to boundary by default).
 * E.g. __attribute__((aligned(4)))*/

#if defined(CONFIG_LV_USE_GPU_ACTS_VG_LITE)
#  define LV_ATTRIBUTE_MEM_ALIGN __attribute__((aligned(64)))
#else
#  define LV_ATTRIBUTE_MEM_ALIGN __attribute__((aligned(4)))
#endif

/* Tracing */

#if defined(CONFIG_LV_USE_STRACE)

#define LV_STRACE_INCLUDE           "tracing/tracing.h"

#define LV_STRACE_ID_INDEV_TASK     SYS_TRACE_ID_GUI_INDEV_TASK
#define LV_STRACE_ID_REFR_TASK      SYS_TRACE_ID_GUI_REFR_TASK
#define LV_STRACE_ID_UPDATE_LAYOUT  SYS_TRACE_ID_GUI_UPDATE_LAYOUT
#define LV_STRACE_ID_WAIT           SYS_TRACE_ID_GUI_WAIT
#define LV_STRACE_ID_WAIT_DRAW      SYS_TRACE_ID_GUI_WAIT_DRAW

#define LV_STRACE_VOID(id)  sys_trace_void(id)
#define LV_STRACE_U32(id, p1)  sys_trace_u32(id, p1)
#define LV_STRACE_U32X2(id, p1, p2)  sys_trace_u32x2(id, p1, p2)
#define LV_STRACE_U32X3(id, p1, p2, p3)  sys_trace_u32x3(id, p1, p2, p3)
#define LV_STRACE_U32X4(id, p1, p2, p3, p4)  sys_trace_u32x4(id, p1, p2, p3, p4)
#define LV_STRACE_U32X5(id, p1, p2, p3, p4, p5)  sys_trace_u32x5(id, p1, p2, p3, p4, p5)
#define LV_STRACE_U32X6(id, p1, p2, p3, p4, p5, p6)  sys_trace_u32x6(id, p1, p2, p3, p4, p5, p6)
#define LV_STRACE_U32X7(id, p1, p2, p3, p4, p5, p6, p7)  sys_trace_u32x7(id, p1, p2, p3, p4, p5, p6, p7)
#define LV_STRACE_U32X8(id, p1, p2, p3, p4, p5, p6, p7, p8)  sys_trace_u32x8(id, p1, p2, p3, p4, p5, p6, p7, p8)
#define LV_STRACE_U32X9(id, p1, p2, p3, p4, p5, p6, p7, p8, p9)  sys_trace_u32x9(id, p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define LV_STRACE_U32X10(id, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)  sys_trace_u32x10(id, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)

#define LV_STRACE_STRING(id, string)  sys_trace_string(id, string)

#define LV_STRACE_CALL(id)  sys_trace_end_call(id)
#define LV_STRACE_CALL_U32(id, retv)  sys_trace_end_call_u32(id, retv)

#endif /* CONFIG_LV_USE_STRACE */

/* Compiler settings */

#define LV_ATTRIBUTE_VERY_FAST_MEM  __lvgl_func

#endif /* LV_CONF_H */
