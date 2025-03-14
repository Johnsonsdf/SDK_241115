/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file roll_bar.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <assert.h>
#include "roll_bar.h"
#include <ui_mem.h>

/*********************
 *      DEFINES
 *********************/
#define ROLL_BAR_HIDDEN_DELAY_TIME 500
#define ROLL_BAR_HIDDEN_ANIM_TIME 500

#define ROLL_BAR_SHOW_DELAY_TIME 0
#define ROLL_BAR_SHOW_ANIM_TIME 0
/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
	lv_obj_t *obj; /*Ext. of ancestor*/
	lv_obj_t *obj_up;
	lv_obj_t *target_obj;
	roll_bar_cb_t cb;
	lv_coord_t min_value;
	uint8_t min_opa;
	uint8_t max_opa;
	uint8_t b_hor_ver;
	bool fade_in_out;
	bool b_auto;
} roll_bar_t;

/**********************
 *  EXTERNAL PROTOTYPES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/


/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void _roll_bar_anim_cb(void * var, int32_t v)
{
	roll_bar_t *roll_bar = (roll_bar_t *)var;
	lv_obj_set_style_bg_opa(roll_bar->obj,v,LV_PART_MAIN);
	lv_obj_set_style_bg_opa(roll_bar->obj_up,v,LV_PART_MAIN);
	if(roll_bar->cb)
		roll_bar->cb(v);
}

static void _roll_bar_fade_in_out(roll_bar_t *roll_bar, bool fade_in)
{
	if(roll_bar->fade_in_out)
	{
		lv_anim_del(roll_bar,_roll_bar_anim_cb);
		lv_anim_t a;
		lv_anim_init(&a);
		lv_anim_set_var(&a, roll_bar);
		if(fade_in)
		{
			lv_anim_set_values(&a, lv_obj_get_style_bg_opa(roll_bar->obj , LV_PART_MAIN), roll_bar->max_opa);
			lv_anim_set_time(&a, ROLL_BAR_SHOW_ANIM_TIME);
			lv_anim_set_delay(&a, ROLL_BAR_SHOW_DELAY_TIME);
		}
		else
		{
			lv_anim_set_values(&a, lv_obj_get_style_bg_opa(roll_bar->obj , LV_PART_MAIN), roll_bar->min_opa);
			lv_anim_set_time(&a, ROLL_BAR_HIDDEN_ANIM_TIME);
			lv_anim_set_delay(&a, ROLL_BAR_HIDDEN_DELAY_TIME);
		}
		lv_anim_set_exec_cb(&a, _roll_bar_anim_cb);
		lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
		lv_anim_start(&a);
	}
}

static void roll_bar_scroll_cb(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	roll_bar_t *roll_bar = lv_event_get_user_data(e);
	lv_obj_t *obj = lv_event_get_current_target(e);
	if(!roll_bar->b_auto)
		return;
	switch (code)
	{
		case LV_EVENT_SCROLL_BEGIN:
		case LV_EVENT_SCROLL_END:
		case LV_EVENT_SCROLL:
			{
				lv_coord_t roll_coord = 0;
				lv_coord_t all_roll_coord = 0;
				lv_coord_t obj_value = 0;
				if(roll_bar->b_hor_ver == ROLL_HOR_MOD)
				{
					roll_coord = lv_obj_get_scroll_top(obj);
					all_roll_coord = roll_coord + lv_obj_get_scroll_bottom(obj);
					obj_value = lv_obj_get_height(roll_bar->obj);
				}
				else
				{
					roll_coord = lv_obj_get_scroll_left(obj);
					all_roll_coord = roll_coord + lv_obj_get_scroll_right(obj);
					obj_value = lv_obj_get_width(roll_bar->obj);
				}
				if(roll_coord < 0)
					roll_coord = 0;
				if(roll_coord > all_roll_coord)
					roll_coord = all_roll_coord;
				lv_coord_t at_value = (roll_bar->min_value + roll_coord * (obj_value - roll_bar->min_value) / all_roll_coord);
				if(roll_bar->b_hor_ver == ROLL_HOR_MOD)
					lv_obj_set_height(roll_bar->obj_up,at_value);
				else
					lv_obj_set_width(roll_bar->obj_up,at_value);
				if(LV_EVENT_SCROLL != code)
				{
					if(LV_EVENT_SCROLL_BEGIN == code)
						_roll_bar_fade_in_out(roll_bar,true);
					else
						_roll_bar_fade_in_out(roll_bar,false);
				}
			}
			break;
		default:
			break;
	}
}

static void roll_bar_obj_cb(lv_event_t * e)
{
	roll_bar_t *roll_bar = lv_event_get_user_data(e);
	lv_event_code_t code = lv_event_get_code(e);
	switch (code)
	{
		case LV_EVENT_DELETE:
			if(roll_bar->target_obj)
				lv_obj_remove_event_cb(roll_bar->target_obj,roll_bar_scroll_cb);
			lv_anim_del(roll_bar,_roll_bar_anim_cb);
			ui_mem_free(MEM_GUI, roll_bar);
			break;
		case LV_EVENT_SIZE_CHANGED:
			{
				lv_coord_t obj_height = lv_obj_get_height(roll_bar->obj);
				lv_coord_t obj_up_height = lv_obj_get_height(roll_bar->obj_up);
				if(roll_bar->min_value > obj_height)
					roll_bar->min_value = obj_height;
				if(obj_up_height > obj_height)
					lv_obj_set_height(roll_bar->obj_up,obj_height);
				lv_obj_set_width(roll_bar->obj_up,lv_obj_get_width(roll_bar->obj));
				break;
			}
		case LV_EVENT_STYLE_CHANGED:
			lv_obj_set_style_radius(roll_bar->obj_up,lv_obj_get_style_radius(roll_bar->obj,LV_PART_MAIN),LV_PART_MAIN);
			break;
		default:
			break;
	}
}

static void roll_bar_parent_del_cb(lv_event_t * e)
{
	roll_bar_t *roll_bar = lv_event_get_user_data(e);
	roll_bar->target_obj = NULL;
	lv_obj_del(roll_bar->obj);
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
lv_obj_t * roll_bar_create(lv_obj_t * obj)
{
	roll_bar_t *roll_bar = ui_mem_alloc(MEM_GUI, sizeof(roll_bar_t), __func__);
	if (!roll_bar) {
		return NULL;
	}
	lv_memset_00(roll_bar,sizeof(roll_bar_t));
	roll_bar->target_obj = obj;
	roll_bar->b_auto = true;
	roll_bar->obj = lv_obj_create(lv_obj_get_parent(roll_bar->target_obj));
	lv_obj_set_style_pad_all(obj,0,LV_PART_MAIN);
	lv_obj_set_style_border_width(obj,0,LV_PART_MAIN);
	lv_obj_set_style_bg_opa(roll_bar->obj,LV_OPA_100,LV_PART_MAIN);
	lv_obj_set_user_data(roll_bar->obj,roll_bar);

	roll_bar->obj_up = lv_obj_create(roll_bar->obj);
	lv_obj_set_style_pad_all(roll_bar->obj_up,0,LV_PART_MAIN);
	lv_obj_set_style_border_width(roll_bar->obj_up,0,LV_PART_MAIN);
	lv_obj_set_style_bg_opa(roll_bar->obj_up,LV_OPA_100,LV_PART_MAIN);

	lv_obj_add_event_cb(roll_bar->target_obj,roll_bar_scroll_cb,LV_EVENT_ALL,roll_bar);
	lv_obj_add_event_cb(roll_bar->target_obj,roll_bar_parent_del_cb,LV_EVENT_DELETE,roll_bar);
	lv_obj_add_event_cb(roll_bar->obj,roll_bar_obj_cb,LV_EVENT_ALL,roll_bar);
	return roll_bar->obj;
}



/*=====================
 * Setter functions
 *====================*/

void roll_bar_set_direction(lv_obj_t * obj, uint8_t mod)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	roll_bar->b_hor_ver = mod;
}

void roll_bar_set_opa(lv_obj_t * obj, uint8_t min_opa , uint8_t max_opa)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	roll_bar->max_opa = max_opa;
	roll_bar->min_opa = min_opa;
}

void roll_bar_set_fade(lv_obj_t * obj, bool fade)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	roll_bar->fade_in_out = fade;
}

void roll_bar_set_min(lv_obj_t * obj, lv_coord_t min_value)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	roll_bar->min_value = min_value;
	if(roll_bar->b_hor_ver == ROLL_HOR_MOD)
	{
		lv_coord_t obj_height = lv_obj_get_height(roll_bar->obj);
		if(obj_height == 0)
			lv_obj_update_layout(roll_bar->obj);
		if(min_value > lv_obj_get_height(roll_bar->obj))
			min_value = lv_obj_get_height(roll_bar->obj);
		lv_obj_set_height(roll_bar->obj_up,min_value);
	}
	else
	{
		lv_coord_t obj_width = lv_obj_get_width(roll_bar->obj);
		if(obj_width == 0)
			lv_obj_update_layout(roll_bar->obj);
		if(min_value > lv_obj_get_width(roll_bar->obj))
			min_value = lv_obj_get_width(roll_bar->obj);
		lv_obj_set_width(roll_bar->obj_up,min_value);
	}
}

void roll_bar_set_color(lv_obj_t * obj, lv_color_t color , lv_color_t color_1)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	lv_obj_set_style_bg_color(roll_bar->obj,color,LV_PART_MAIN);
	lv_obj_set_style_bg_color(roll_bar->obj_up,color_1,LV_PART_MAIN);
}

void roll_bar_set_auto(lv_obj_t * obj, bool b_auto)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	roll_bar->b_auto = b_auto;
}

void roll_bar_set_fade_cb(lv_obj_t * obj, roll_bar_cb_t cb)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	roll_bar->cb = cb;
}

void roll_bar_set_roll_value(lv_obj_t * obj, lv_coord_t value)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);

	lv_coord_t obj_value = roll_bar->b_hor_ver == ROLL_HOR_MOD ? lv_obj_get_height(roll_bar->obj) : lv_obj_get_width(roll_bar->obj);
	if(value > 100)
		value = 100;
	if(value < 0)
		value = 0;
	if(obj_value == 0)
		lv_obj_update_layout(roll_bar->obj);
	obj_value = obj_value * value / 100;
	if(obj_value < roll_bar->min_value)
		obj_value = roll_bar->min_value;
	roll_bar->b_hor_ver == ROLL_HOR_MOD ? lv_obj_set_height(roll_bar->obj_up,obj_value) : lv_obj_set_width(roll_bar->obj_up,obj_value);
}

void roll_bar_set_fade_in_out(lv_obj_t * obj, bool fade_in)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	_roll_bar_fade_in_out(roll_bar,fade_in);
}

void roll_bar_set_current_opa(lv_obj_t * obj, lv_opa_t value)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	lv_obj_set_style_bg_opa(roll_bar->obj,value,LV_PART_MAIN);
	lv_obj_set_style_bg_opa(roll_bar->obj_up,value,LV_PART_MAIN);
}

lv_obj_t *roll_bar_get_obj(lv_obj_t * obj)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	return roll_bar->obj;
}

lv_obj_t *roll_bar_get_obj_up(lv_obj_t * obj)
{
	roll_bar_t *roll_bar = lv_obj_get_user_data(obj);
	return roll_bar->obj_up;
}

