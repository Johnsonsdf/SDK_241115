/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PORTING_ZEPHYR_LVGL_MEM_H_
#define PORTING_ZEPHYR_LVGL_MEM_H_

/*********************
 *      INCLUDES
 *********************/
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lvgl_heap_init(void);

void *lvgl_malloc(size_t size, const char *func);

void *lvgl_realloc(void *ptr, size_t requested_size, const char *func);

void lvgl_free(void *ptr, const char *func);

#ifdef __cplusplus
}
#endif

#endif /* PORTING_ZEPHYR_LVGL_MEM_H_ */
