/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file text_arc.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <assert.h>
#include <ui_mem.h>
#include "text_arc.h"
#include <widgets/text_canvas.h>
#include <stdio.h>
#include <ui_mem.h>
#include <memory/mem_cache.h>
#ifdef CONFIG_LVGL_USE_FREETYPE_FONT
#include <lvgl/lvgl_freetype_font.h>
#endif

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &text_arc_class


/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
	lv_obj_t *obj;
} text_obj_t;

/**********************
 *  EXTERNAL PROTOTYPES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void text_arc_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void text_arc_event(const lv_obj_class_t * class_p, lv_event_t * e);
static void text_arc_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t text_arc_class = {
	.destructor_cb = text_arc_destructor,
	.event_cb = text_arc_event,
	.constructor_cb = text_arc_constructor,
	.width_def = LV_SIZE_CONTENT,
	.height_def = LV_SIZE_CONTENT,
	.instance_size = sizeof(text_arc_t),
	.base_class = &lv_obj_class,
};

static void text_arc_set_angle(lv_obj_t * obj, int16_t angle , lv_area_t *area)
{
    while(angle >= 3600) angle -= 3600;
    while(angle < 0) angle += 3600;

    lv_img_t * img = (lv_img_t *)obj;

    lv_obj_update_layout(obj);  /*Be sure the object's size is calculated*/
	if(angle == img->angle)
	{
		lv_memcpy(area,&obj->coords,sizeof(*area));
		return;
	}
    lv_coord_t w = lv_obj_get_width(obj);
    lv_coord_t h = lv_obj_get_height(obj);
    lv_area_t a;
    _lv_img_buf_get_transformed_area(&a, w, h, img->angle, img->zoom, &img->pivot);
    a.x1 += obj->coords.x1;
    a.y1 += obj->coords.y1;
    a.x2 += obj->coords.x1;
    a.y2 += obj->coords.y1;
    lv_obj_invalidate_area(obj, &a);

    img->angle = angle;

    /* Disable invalidations because lv_obj_refresh_ext_draw_size would invalidate
     * the whole ext draw area */
    lv_disp_t * disp = lv_obj_get_disp(obj);
    lv_disp_enable_invalidation(disp, false);
    lv_obj_refresh_ext_draw_size(obj);
    lv_disp_enable_invalidation(disp, true);

    _lv_img_buf_get_transformed_area(&a, w, h, img->angle, img->zoom, &img->pivot);
    a.x1 += obj->coords.x1;
    a.y1 += obj->coords.y1;
    a.x2 += obj->coords.x1;
    a.y2 += obj->coords.y1;
    lv_obj_invalidate_area(obj, &a);
	lv_memcpy(area,&a,sizeof(lv_area_t));
}
static void canvas_delete_event_cb(lv_event_t * e)
{
	lv_obj_t *canvas = lv_event_get_target(e);
	lv_img_dsc_t *img_dsc = lv_canvas_get_img(canvas);
	ui_mem_free(MEM_RES,(void *)img_dsc->data);
}

static void text_arc_up(text_arc_t *text_arc)
{
	lv_obj_t *obj = &text_arc->obj;
	lv_obj_clean(obj);
	lv_draw_label_dsc_t label_draw_dsc;
	lv_draw_label_dsc_init(&label_draw_dsc);
	lv_obj_init_draw_label_dsc(obj, LV_PART_MAIN, &label_draw_dsc);
	const lv_font_t *font = label_draw_dsc.font;
	char *text = text_arc->text;
	if(font == NULL || text == NULL)
		return;
	uint32_t ofs = 0;
	uint32_t letter = _lv_txt_encoded_next(text, &ofs);
	uint32_t letter_next = 0;
	uint16_t letter_w;
	const uint16_t letter_space = 0; /* add space for special characters */
	const uint16_t letter_h = lv_font_get_line_height(font) + letter_space * 2;
	const lv_img_cf_t cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
	const uint8_t px_size = LV_IMG_PX_SIZE_ALPHA_BYTE;
	int16_t angle = text_arc->angle_st;

	lv_draw_label_dsc_t draw_dsc;
	lv_draw_label_dsc_init(&draw_dsc);
	draw_dsc.font = font;
	draw_dsc.color = label_draw_dsc.color;
	#ifdef CONFIG_LVGL_USE_FREETYPE_FONT
	lvgl_freetype_force_bitmap((lv_font_t *)font, 1);
	#endif
	lv_area_t obj_area = {
		.x1 = LV_COORD_MAX,
		.x2 = LV_COORD_MIN,
		.y1 = LV_COORD_MAX,
		.y2 = LV_COORD_MIN,
	};
	for (; letter != 0; letter = letter_next, angle += text_arc->angle_end) {
		/* copy the text */
		char tmp_str[8] = {0};
		if (ofs >= sizeof(tmp_str)) {
			LV_LOG_ERROR("increase buffer to hold text frag");
			break;
		}
		memcpy(tmp_str, text, ofs);
		tmp_str[ofs] = '\0';

		/* move to next position */
		text += ofs;
		ofs = 0;

		letter_next = _lv_txt_encoded_next(text, &ofs);
		letter_w = lv_font_get_glyph_width(font, letter, letter_next);

		/* add space for letter if exceed font height */
		letter_w += 2 * letter_space;

		/* align the width for GPU accelerating */
		if (px_size == 2) letter_w = (letter_w + 1) & ~0x1;
		else if (px_size == 3) letter_w = (letter_w + 3) & ~0x3;

		/* allocate the buffer for letter */
		uint32_t letter_buf_size = (uint32_t)letter_w * letter_h * px_size;
		void *buf = ui_mem_alloc(MEM_RES,letter_buf_size,__func__);
		if (buf == NULL) {
			LV_LOG_ERROR("letter buffer alloc failed");
			break;
		}
		memset(buf, 0, letter_buf_size);

		/* compute the positon of letter */
		lv_point_t pivot;
		pivot.x = text_arc->radius + letter_w / 2 + ((lv_trigo_sin(angle) * text_arc->radius) >> LV_TRIGO_SHIFT);
		pivot.y = text_arc->radius + letter_h / 2 - ((lv_trigo_cos(angle) * text_arc->radius) >> LV_TRIGO_SHIFT);

		/* create canvas to hold letter */
		lv_obj_t *canvas = lv_canvas_create(obj);
		if (canvas == NULL) {
			LV_LOG_ERROR("canvas obj create failed");
			break;
		}

		lv_canvas_set_buffer(canvas, buf, letter_w, letter_h, cf);
		lv_canvas_draw_text(canvas, letter_space, letter_space, letter_w, &draw_dsc, tmp_str);
		mem_dcache_clean(buf, letter_buf_size);
		mem_dcache_sync();
		lv_obj_set_pos(canvas, pivot.x - letter_w / 2, pivot.y - letter_h / 2);
		lv_obj_add_event_cb(canvas, canvas_delete_event_cb, LV_EVENT_DELETE, NULL);
		lv_area_t area = {0};
		text_arc_set_angle(canvas, angle * 10 , &area);
		if(area.x1 < obj_area.x1)
			obj_area.x1 = area.x1;
		if(area.x2 > obj_area.x2)
			obj_area.x2 = area.x2;
		if(area.y1 < obj_area.y1)
			obj_area.y1 = area.y1;
		if(area.y2 > obj_area.y2)
			obj_area.y2 = area.y2;
	}
	lv_obj_set_size(obj,lv_area_get_width(&obj_area),lv_area_get_height(&obj_area));
	for(lv_coord_t i = 0 ; i < lv_obj_get_child_cnt(obj) ; i++)
	{
		lv_obj_t *canvas = lv_obj_get_child(obj,i);
		lv_obj_set_pos(canvas, lv_obj_get_x(canvas) - obj_area.x1, lv_obj_get_y(canvas) - obj_area.y1);
	}
	#ifdef CONFIG_LVGL_USE_FREETYPE_FONT
	lvgl_freetype_force_bitmap((lv_font_t *)font, 0);
	#endif
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t *text_arc_create(lv_obj_t * parent)
{
	LV_LOG_INFO("begin");
	lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
	lv_obj_class_init_obj(obj);
	return obj;
}

void text_arc_set_text(lv_obj_t *class_p , char *text)
{
	LV_UNUSED(class_p);
	text_arc_t *text_arc = (text_arc_t *)class_p;
	if(text_arc->text)
	{
		if(strcmp(text_arc->text,text) == 0)
			return;
		lv_mem_free(text_arc->text);
	}
	uint32_t text_num = strlen(text) + 1;
	text_arc->text = lv_mem_alloc(text_num);
	if(text_arc->text == NULL)
	{
		LV_LOG_ERROR("text alloc error");
		return;
	}
	lv_memcpy(text_arc->text,text,text_num);
	text_arc_up(text_arc);
	lv_obj_invalidate(class_p);
}

void text_arc_set_radian(lv_obj_t *class_p ,uint16_t radius,int16_t angle_st,int16_t angle_end)
{
	LV_UNUSED(class_p);
	text_arc_t *text_arc = (text_arc_t *)class_p;
	text_arc->angle_st = angle_st;
	text_arc->angle_end = angle_end;
	text_arc->radius = radius;
	text_arc_up(text_arc);
	lv_obj_invalidate(class_p);
}

static void text_arc_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
	LV_UNUSED(class_p);

	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * obj = lv_event_get_target(e);
	//text_arc_t * roller = (text_arc_t *)obj;
	/*Ancestor events will be called during drawing*/
	if (code == LV_EVENT_DRAW_MAIN) {
		lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);
        lv_area_t obj_area;
		lv_obj_get_coords(obj, &obj_area);
        lv_draw_rect_dsc_t obj_dsc;
        lv_draw_rect_dsc_init(&obj_dsc);
        lv_obj_init_draw_rect_dsc(obj, LV_PART_MAIN, &obj_dsc);
		lv_draw_rect(draw_ctx, &obj_dsc, &obj_area);
	}
}

static void text_arc_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
	lv_obj_set_scroll_dir(obj,LV_DIR_NONE);
}

static void text_arc_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
	LV_UNUSED(class_p);
}
