/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file roll_bar.h
 *
 */

#ifndef WIDGETS_ROLL_BAR_H_
#define WIDGETS_ROLL_BAR_H_

#ifdef __cplusplus
extern "C" {
#endif


/*********************
 *      REFERENCE
 *********************/
//Attention : Do not use bar user_data
/*
static lv_obj_t *opa_obj = NULL;

void view_roll_bar_cb(int32_t v)
{
	lv_obj_set_style_bg_opa(opa_obj,v,LV_PART_MAIN);
}

void bar_create(lv_obj_t *obj)
{
	lv_obj_t *bar = roll_bar_create(obj);
	lv_obj_set_size(bar,10,100);
	roll_bar_set_color(bar,lv_color_white(),lv_color_hex(0xFF0000));
	roll_bar_set_min(bar,10);
	roll_bar_set_opa(bar,LV_OPA_0,LV_OPA_100);
	lv_obj_set_style_radius(bar,5,LV_PART_MAIN);
	roll_bar_set_fade(bar,true);
	//	close auto roll mod
	//roll_bar_set_auto(bar, false);
	//roll_bar_set_roll_value(bar,50);
	lv_obj_align(bar,LV_ALIGN_CENTER,0,0);
	roll_bar_set_fade_cb(bar,view_roll_bar_cb);

	opa_obj = lv_obj_create(obj);
}
*/

/*********************
 *      INCLUDES
 *********************/
#include <lvgl/lvgl.h>
#include "draw_util.h"

/*********************
 *      DEFINES
 *********************/
enum {
	ROLL_HOR_MOD = 0,
	ROLL_VER_MOD,
};

typedef void (*roll_bar_cb_t)(int32_t v);

/**********************
 *      TYPEDEFS
 **********************/


/**********************
 * GLOBAL PROTOTYPES
 **********************/
lv_obj_t * roll_bar_create(lv_obj_t * parent);

void roll_bar_set_direction(lv_obj_t * obj, uint8_t mod);

void roll_bar_set_opa(lv_obj_t * obj, uint8_t min_opa , uint8_t max_opa);

void roll_bar_set_fade(lv_obj_t * obj, bool fade);

void roll_bar_set_min(lv_obj_t * obj, lv_coord_t min_value);

void roll_bar_set_color(lv_obj_t * obj, lv_color_t color , lv_color_t color_1);

void roll_bar_set_auto(lv_obj_t * obj, bool b_auto);

void roll_bar_set_roll_value(lv_obj_t * obj, lv_coord_t value);

void roll_bar_set_fade_cb(lv_obj_t * obj, roll_bar_cb_t cb);

void roll_bar_set_fade_in_out(lv_obj_t * obj, bool fade_in);

void roll_bar_set_current_opa(lv_obj_t * obj, lv_opa_t value);

/*=====================
 * Getter functions
 *====================*/
lv_obj_t *roll_bar_get_obj(lv_obj_t * obj);

lv_obj_t *roll_bar_get_obj_up(lv_obj_t * obj);

/*=====================
 * Other functions
 *====================*/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* WIDGETS_roll_bar_H_ */
