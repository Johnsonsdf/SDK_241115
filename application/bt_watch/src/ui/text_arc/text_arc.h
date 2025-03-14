/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file text_arc.h
 *
 */

#ifndef _TEXT_ARC_CANVAS_H_
#define _TEXT_ARC_CANVAS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <lvgl/lvgl.h>
//#include "draw_util.h"

/*********************
 *      DEFINES
 *********************/


/**********************
 *      TYPEDEFS
 **********************/

/** Data of text canvas */
typedef struct {
	lv_obj_t obj; /*Ext. of ancestor*/
	void *text_canvas;
	char *text;
	const lv_font_t *font;
	uint16_t radius;
	int16_t angle_st;
	int16_t angle_end;
} text_arc_t;

extern const lv_obj_class_t text_arc_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create an text rollers object
 * @param parent pointer to an object, it will be the parent of the new object
 * @return pointer to the created text rollers object
 */
lv_obj_t * text_arc_create(lv_obj_t * parent);


void text_arc_set_text(lv_obj_t *text_arc , char *text);

void text_arc_set_radian(lv_obj_t *class_p ,uint16_t radius,int16_t angle_st,int16_t angle_end);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* WIDGETS_TEXT_CANVAS_H_ */
