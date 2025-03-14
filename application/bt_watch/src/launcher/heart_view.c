/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file heart view
 */
#include <assert.h>
#include "app_ui.h"
#include "heart_view.h"
#include "m_effect.h"
#include <widgets/simple_img.h>
//#include <widgets/anim_img.h>
#ifdef CONFIG_SENSOR_MANAGER
#include <sensor_manager.h>
#endif
#include <view_cache.h>

#include <memory/mem_cache.h>
#ifdef CONFIG_UI_MANAGER
	#include <ui_mem.h>
	#  define BUF_MALLOC(size)        ui_mem_alloc(MEM_RES, size, __func__)
	#  define BUF_FREE(ptr)           ui_mem_free(MEM_RES, ptr)
	#  define BUF_REALLOC(ptr,size)   ui_mem_realloc(MEM_RES,ptr,size,__func__)
	#else
	#  define BUF_MALLOC(size)        lv_mem_alloc(size)
	#  define BUF_FREE(ptr)           lv_mem_free(ptr)
	#  define BUF_REALLOC(ptr,size)   lv_mem_realloc(ptr,size)
#endif

#define MAP_DATA_NUM 81
#define MAP_LINE_WIDTH 2
#define MAP_ALL_WIDTH 330

const uint32_t anim_dsc_id[] = {
	PIC_0, PIC_1, PIC_2, PIC_3, PIC_4, PIC_5, PIC_6, PIC_7, PIC_8, PIC_9,
	PIC_10, PIC_11, PIC_12, PIC_13, PIC_14, PIC_15, PIC_16, PIC_17, PIC_18, PIC_19,
	PIC_20, PIC_21
};

static int32_t heart_preload_inited = 0;

typedef struct heart_view_data {
	lvgl_res_group_t group_anim;
	lv_point_t point_bg;
	lv_point_t point_max;
	lv_point_t point_min;
	lv_img_dsc_t img_dsc_bg;
	lv_img_dsc_t img_dsc_max;
	lv_img_dsc_t img_dsc_min;
	lv_img_dsc_t img_dsc_anim;
	lvgl_res_string_t hr_unit;
	lvgl_res_string_t hr_static;

	lv_font_t font_vf50;
	lv_font_t font_vf30;
	lv_font_t font_vf26;

	lv_obj_t *anim_icon;
	lv_obj_t *hr_num;
	lv_obj_t *max_hr_num;
	lv_obj_t *min_hr_num;
	lv_obj_t *unit_icon;
	lv_obj_t *static_icon;
	lv_obj_t *hr_map;
	lv_chart_series_t * ser;
	void *chart_buf;
	lv_obj_t *hr_map_snapshot;
	lv_img_dsc_t hr_map_imgs;
	lv_timer_t *map_width_timer;
	lv_coord_t width_value;

	lv_timer_t *map_start_timer;

	lv_timer_t *map_timer;
	lv_timer_t *anim_timer;
	lv_coord_t anim_value;
} heart_view_data_t;

static void _unload_resource(heart_view_data_t *data)
{
	lvgl_res_unload_pictures(&data->img_dsc_bg, 1);
	lvgl_res_unload_pictures(&data->img_dsc_max, 1);
	lvgl_res_unload_pictures(&data->img_dsc_min, 1);
	lvgl_res_unload_pictures(&data->img_dsc_anim, 1);
	lvgl_res_unload_group(&data->group_anim);
	lvgl_bitmap_font_close(&data->font_vf50);
	lvgl_bitmap_font_close(&data->font_vf30);
	lvgl_bitmap_font_close(&data->font_vf26);
}

static int _load_resource(heart_view_data_t *data)
{
	int32_t ret;

	lvgl_res_scene_t res_scene;
	ret = lvgl_res_load_scene(SCENE_HEART_VIEW, &res_scene, DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
	if (ret < 0) {
		SYS_LOG_ERR("SCENE_HEART_VIEW not found");
		return -ENOENT;
	}
	ret = lvgl_res_load_group_from_scene(&res_scene, ANIM, &data->group_anim);
	if (ret < 0) {
		goto out_exit;
	}

	uint32_t dsc_id = anim_dsc_id[data->anim_value];
	ret = lvgl_res_load_pictures_from_group(&data->group_anim, &dsc_id, &data->img_dsc_anim, NULL, 1);
	if (ret < 0) {
		goto out_exit;
	}

	dsc_id = PIC_BG;
	ret = lvgl_res_load_pictures_from_scene(&res_scene, &dsc_id, &data->img_dsc_bg, &data->point_bg, 1);
	if (ret < 0) {
		goto out_exit;
	}

	dsc_id = MAX_IMG;
	ret = lvgl_res_load_pictures_from_scene(&res_scene, &dsc_id, &data->img_dsc_max, &data->point_max, 1);
	if (ret < 0) {
		goto out_exit;
	}

	dsc_id = MIN_IMG;
	ret = lvgl_res_load_pictures_from_scene(&res_scene, &dsc_id, &data->img_dsc_min, &data->point_min, 1);
	if (ret < 0) {
		goto out_exit;
	}

	dsc_id = HR_UNIT;
	ret = lvgl_res_load_strings_from_scene(&res_scene, &dsc_id, &data->hr_unit, 1);
	if (ret < 0) {
		goto out_exit;
	}

	dsc_id = HR_STATIC;
	ret = lvgl_res_load_strings_from_scene(&res_scene, &dsc_id, &data->hr_static, 1);
	if (ret < 0) {
		goto out_exit;
	}


	ret = lvgl_bitmap_font_open(&data->font_vf50, DEF_VF_50);
	if (ret < 0) {
		goto out_exit;
	}
	ret = lvgl_bitmap_font_open(&data->font_vf30, DEF_VF_30);
	if (ret < 0) {
		goto out_exit;
	}
	ret = lvgl_bitmap_font_open(&data->font_vf26, DEF_VF_26);
	if (ret < 0) {
		goto out_exit;
	}

	SYS_LOG_INF("load resource succeed");
	lvgl_res_unload_scene(&res_scene);
	return ret;

out_exit:
	lvgl_res_unload_group(&data->group_anim);
	_unload_resource(data);
	lvgl_res_unload_scene(&res_scene);
	return ret;
}

static int _heart_view_preload(view_data_t *view_data, bool update)
{
	if (heart_preload_inited == 0) {
		uint32_t id[3] = {PIC_BG,MAX_IMG,MIN_IMG};
		lvgl_res_preload_scene_compact_default_init(SCENE_HEART_VIEW, id, 3,
			NULL, DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
		heart_preload_inited = 1;
	}
	
	return lvgl_res_preload_scene_compact_default(SCENE_HEART_VIEW, HEART_VIEW, update, 0);
}

static void _heart_event_handler(lv_event_t * e)
{
	if(lvgl_click_decision())
	{
		go_to_m_effect();
		lv_obj_remove_event_cb(lv_event_get_user_data(e),_heart_event_handler);
	}
}

static uint32_t max_hr_val = 0;
static uint32_t min_hr_val = 0xFF;
static int _heart_view_paint(view_data_t *view_data)
{
	const heart_view_presenter_t *presenter = view_get_presenter(view_data);
	heart_view_data_t *data = view_data->user_data;

	if (data) {
		SYS_LOG_INF("_heart_view_paint");

		int32_t hr_val = presenter->get_heart_rate();
		if(hr_val)
		{
			lv_label_set_text_fmt(data->hr_num , "%02d", hr_val);
			if(hr_val > max_hr_val)
				max_hr_val = hr_val;
			if(hr_val < min_hr_val)
				min_hr_val = hr_val;
		}
		else
			lv_label_set_text(data->hr_num , "--");

		lv_obj_update_layout(data->hr_num);
		lv_obj_align_to(data->unit_icon,data->hr_num,LV_ALIGN_OUT_RIGHT_BOTTOM,10,0);
		
		if(max_hr_val != 0)
			lv_label_set_text_fmt(data->max_hr_num , "%02d", max_hr_val);
		else
			lv_label_set_text(data->max_hr_num , "--");
		
		if(min_hr_val != 0xFF && min_hr_val != 0)
			lv_label_set_text_fmt(data->min_hr_num , "%02d", min_hr_val);
		else
			lv_label_set_text(data->min_hr_num , "--");
	}

	return 0;
}

static void hr_map_snapshot_free(heart_view_data_t *data)
{
	if(data->chart_buf)
	{
		BUF_FREE(data->chart_buf);
		data->chart_buf = NULL;
	}
}

static void draw_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);

    /*Add the faded area before the lines are drawn*/
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    if(dsc && dsc->part == LV_PART_ITEMS) {
        if(!dsc->p1 || !dsc->p2) return;

        /*Add a line mask that keeps the area below the line*/
        lv_draw_mask_line_param_t line_mask_param;
        lv_draw_mask_line_points_init(&line_mask_param, dsc->p1->x, dsc->p1->y, dsc->p2->x, dsc->p2->y,
                                      LV_DRAW_MASK_LINE_SIDE_BOTTOM);
        int16_t line_mask_id = lv_draw_mask_add(&line_mask_param, NULL);

        /*Add a fade effect: transparent bottom covering top*/
        lv_coord_t h = lv_obj_get_height(obj);
        lv_draw_mask_fade_param_t fade_mask_param;
        lv_draw_mask_fade_init(&fade_mask_param, &obj->coords, LV_OPA_COVER, obj->coords.y1 + h / 8, LV_OPA_TRANSP,
                               obj->coords.y2);
        int16_t fade_mask_id = lv_draw_mask_add(&fade_mask_param, NULL);

        /*Draw a rectangle that will be affected by the mask*/
        lv_draw_rect_dsc_t draw_rect_dsc;
        lv_draw_rect_dsc_init(&draw_rect_dsc);
        draw_rect_dsc.bg_opa = LV_OPA_100;
        draw_rect_dsc.bg_color = dsc->line_dsc->color;

        lv_area_t a;
        a.x1 = dsc->p1->x;
        a.x2 = dsc->p2->x - 1;
        a.y1 = LV_MIN(dsc->p1->y, dsc->p2->y);
        a.y2 = obj->coords.y2;
        lv_draw_rect(dsc->draw_ctx, &draw_rect_dsc, &a);

        /*Remove the masks*/
        lv_draw_mask_free_param(&line_mask_param);
        lv_draw_mask_free_param(&fade_mask_param);
        lv_draw_mask_remove_id(line_mask_id);
        lv_draw_mask_remove_id(fade_mask_id);
    }
}

static void hr_map_snapshot_up(heart_view_data_t *data)
{
	lv_obj_t *scr = lv_scr_act();
	data->hr_map = lv_chart_create(scr);
	lv_obj_set_size(data->hr_map, MAP_ALL_WIDTH, 128);
	lv_chart_set_type(data->hr_map, LV_CHART_TYPE_LINE);
	lv_obj_add_event_cb(data->hr_map, draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
	lv_chart_set_point_count(data->hr_map,MAP_DATA_NUM);
	lv_chart_set_div_line_count(data->hr_map,0,0);
	lv_obj_set_style_line_opa(data->hr_map,LV_OPA_100,LV_PART_ITEMS);
	lv_obj_set_style_line_width(data->hr_map,MAP_LINE_WIDTH,LV_PART_ITEMS);

	data->ser = lv_chart_add_series(data->hr_map, lv_color_hex(0XFF0000), LV_CHART_AXIS_PRIMARY_Y);
	if (data->ser == NULL)
		return;

	lv_chart_set_range(data->hr_map,LV_CHART_AXIS_PRIMARY_Y,MAP_LINE_WIDTH,200);
	for(uint32_t i = 0; i < MAP_DATA_NUM; i++) {
		data->ser->y_points[i] = lv_rand(80, 140);
	}
	//test
	uint32_t j = lv_rand(0, 50);
	uint32_t enj = lv_rand(j, 50);
	for(; j < enj; j++)
		data->ser->y_points[j] = 0;
	lv_chart_refresh(data->hr_map);
	uint32_t chart_buf_size = lv_snapshot_buf_size_needed(data->hr_map,LV_IMG_CF_TRUE_COLOR_ALPHA);
	if(data->chart_buf == NULL)
		data->chart_buf = BUF_MALLOC(chart_buf_size);
	else
	{
		hr_map_snapshot_free(data);
		data->chart_buf = BUF_MALLOC(chart_buf_size);
	}
	if(data->chart_buf)
	{
		uint32_t chart_buf_size = lv_snapshot_buf_size_needed(data->hr_map,LV_IMG_CF_TRUE_COLOR_ALPHA);
		lv_snapshot_take_to_buf(data->hr_map,LV_IMG_CF_TRUE_COLOR_ALPHA,&data->hr_map_imgs,data->chart_buf,chart_buf_size);
		mem_dcache_clean(data->chart_buf,chart_buf_size);
		mem_dcache_sync();
		simple_img_set_src(data->hr_map_snapshot, &data->hr_map_imgs);
		//lv_obj_invalidate(data->hr_map_snapshot);
	}
	lv_obj_del(data->hr_map);
}

static void anim_timer_cb(lv_timer_t *v)
{
	heart_view_data_t *data = v->user_data;

	data->anim_value++;
	if(data->anim_value >= ARRAY_SIZE(anim_dsc_id))
		data->anim_value = 0;
	lvgl_res_unload_pictures(&data->img_dsc_anim, 1);
	if (lvgl_res_load_pictures_from_group(&data->group_anim, &anim_dsc_id[data->anim_value], &data->img_dsc_anim, NULL, 1) < 0) {
		SYS_LOG_ERR("heart anim load ERROR");
	}
	simple_img_set_src(data->anim_icon, &data->img_dsc_anim);
}

static void map_timer_cb(lv_timer_t *v)
{
	heart_view_data_t *data = v->user_data;
    hr_map_snapshot_up(data);
}

static void anim_timer_begin(heart_view_data_t *data)
{
	if(data->anim_timer == NULL)
		data->anim_timer = lv_timer_create(anim_timer_cb , 100 , data);
	if(data->map_timer == NULL)
		data->map_timer = lv_timer_create(map_timer_cb , 5000 , data);
}

static void anim_timer_over(heart_view_data_t *data)
{
	if(data->anim_timer)
	{
		lv_timer_del(data->anim_timer);
		data->anim_timer = NULL;
	}
	if(data->map_timer)
	{
		lv_timer_del(data->map_timer);
		data->map_timer = NULL;
	}
}

static void map_width_timer_over(heart_view_data_t *data)
{
	if(data->map_width_timer)
	{
		lv_timer_del(data->map_width_timer);
		data->map_width_timer = NULL;
	}
}

static void map_width_timer_cb(lv_timer_t *v)
{
	heart_view_data_t *data = v->user_data;
	lv_obj_set_width(data->hr_map_snapshot,data->width_value);
	data->width_value+=50;
	if(data->width_value > MAP_ALL_WIDTH)
	{
		lv_obj_set_width(data->hr_map_snapshot,MAP_ALL_WIDTH);
		map_width_timer_over(data);
	}
}

static void map_width_timer_begin(heart_view_data_t *data)
{
	if(data->map_width_timer == NULL)
	{
		data->width_value = 0;
		lv_obj_set_width(data->hr_map_snapshot,0);
		data->map_width_timer = lv_timer_create(map_width_timer_cb , 40 , data);
	}
}

static void map_start_timer_cb(lv_timer_t *v)
{
	heart_view_data_t *data = v->user_data;
	if(!view_is_scrolling(HEART_VIEW))
	{
		hr_map_snapshot_up(data);
		map_width_timer_begin(data);
	}
	lv_timer_del(data->map_start_timer);
	data->map_start_timer = NULL;
}

static int _heart_view_layout(view_data_t *view_data)
{
	lv_obj_t *scr = lv_disp_get_scr_act(view_data->display);
	heart_view_data_t *data = view_data->user_data;

	data = app_mem_malloc(sizeof(*data));
	if (!data) {
		return -ENOMEM;
	}

	view_data->user_data = data;
	memset(data, 0, sizeof(*data));

	if (_load_resource(data)) {
		app_mem_free(data);
		view_data->user_data = NULL;
		return -ENOENT;
	}

	lv_obj_t *obj = lv_obj_create(scr);
	lv_obj_set_size(obj,DEF_UI_VIEW_WIDTH,DEF_UI_VIEW_HEIGHT);
	lv_obj_set_style_pad_all(obj,0,LV_PART_MAIN);
	lv_obj_set_style_border_width(obj,0,LV_PART_MAIN);
	lv_obj_add_flag(obj,LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(obj, _heart_event_handler, LV_EVENT_SHORT_CLICKED, obj);
	
	lv_obj_t *bj_img = simple_img_create(obj);
	lv_obj_set_pos(bj_img,data->point_bg.x,data->point_bg.y);
	simple_img_set_src(bj_img, &data->img_dsc_bg);

	data->hr_map_snapshot = simple_img_create(bj_img);
	lv_obj_set_pos(data->hr_map_snapshot, 16, 16);

	data->anim_icon = simple_img_create(obj);
	lv_obj_set_pos(data->anim_icon,data->group_anim.x,data->group_anim.y);
	simple_img_set_src(data->anim_icon, &data->img_dsc_anim);
	anim_timer_begin(data);

	data->hr_num = lv_label_create(obj);
	lv_obj_align_to(data->hr_num,data->anim_icon,LV_ALIGN_OUT_RIGHT_TOP,10,2);
	lv_obj_set_style_text_align(data->hr_num,LV_TEXT_ALIGN_LEFT,LV_PART_MAIN);
	lv_obj_set_style_text_font(data->hr_num,&data->font_vf50,LV_PART_MAIN);
	lv_obj_set_style_text_color(data->hr_num,lv_color_white(),LV_PART_MAIN);

	data->unit_icon = lv_label_create(obj);
	lv_obj_set_style_text_align(data->unit_icon,LV_TEXT_ALIGN_LEFT,LV_PART_MAIN);
	lv_obj_set_style_text_font(data->unit_icon,&data->font_vf26,LV_PART_MAIN);
	lv_obj_set_style_text_color(data->unit_icon,data->hr_unit.color,LV_PART_MAIN);
	lv_label_set_text(data->unit_icon,data->hr_unit.txt);

	lv_obj_t *max_img = simple_img_create(obj);
	lv_obj_set_pos(max_img,data->point_max.x,data->point_max.y);
	simple_img_set_src(max_img, &data->img_dsc_max);

	data->max_hr_num = lv_label_create(obj);
	lv_obj_align_to(data->max_hr_num,max_img,LV_ALIGN_OUT_RIGHT_TOP,10,-5);
	lv_obj_set_style_text_align(data->max_hr_num,LV_TEXT_ALIGN_LEFT,LV_PART_MAIN);
	lv_obj_set_style_text_font(data->max_hr_num,&data->font_vf30,LV_PART_MAIN);
	lv_obj_set_style_text_color(data->max_hr_num,lv_color_white(),LV_PART_MAIN);

	lv_obj_t *min_img = simple_img_create(obj);
	lv_obj_set_pos(min_img,data->point_min.x,data->point_min.y);
	simple_img_set_src(min_img, &data->img_dsc_min);

	data->min_hr_num = lv_label_create(obj);
	lv_obj_align_to(data->min_hr_num,min_img,LV_ALIGN_OUT_RIGHT_TOP,10,-5);
	lv_obj_set_style_text_align(data->min_hr_num,LV_TEXT_ALIGN_LEFT,LV_PART_MAIN);
	lv_obj_set_style_text_font(data->min_hr_num,&data->font_vf30,LV_PART_MAIN);
	lv_obj_set_style_text_color(data->min_hr_num,lv_color_white(),LV_PART_MAIN);

	data->static_icon = lv_label_create(obj);
	lv_obj_align(data->static_icon,LV_ALIGN_TOP_MID,0,data->hr_static.y);
	lv_obj_set_style_text_align(data->static_icon,LV_TEXT_ALIGN_CENTER,LV_PART_MAIN);
	lv_obj_set_style_text_font(data->static_icon,&data->font_vf26,LV_PART_MAIN);
	lv_obj_set_style_text_color(data->static_icon,data->hr_static.color,LV_PART_MAIN);
	lv_label_set_text_fmt(data->static_icon,data->hr_static.txt,79);
	
	_heart_view_paint(view_data);
	SYS_LOG_INF("_heart_view_layout");
	
	return 0;
}

static void _heart_view_start(void)
{
	view_data_t *view_data = view_get_data(HEART_VIEW);
	heart_view_data_t *data = view_data->user_data;
	if(data->map_start_timer == NULL)
		data->map_start_timer = lv_timer_create(map_start_timer_cb , 100 , data);	//optimize
}

static void _heart_view_end(heart_view_data_t *data)
{
	if(data->map_start_timer)
	{	
		lv_timer_del(data->map_start_timer);
		data->map_start_timer = NULL;
	}
	map_width_timer_over(data);
	simple_img_set_src(data->hr_map_snapshot,NULL);
	hr_map_snapshot_free(data);
}

static int _heart_view_delete(view_data_t *view_data)
{
	heart_view_data_t *data = view_data->user_data;

	if (data) {
		lv_obj_t *scr = lv_disp_get_scr_act(view_data->display);
		anim_timer_over(data);
		_heart_view_end(data);
		lv_obj_clean(scr);
		_unload_resource(data);
		app_mem_free(data);
		view_data->user_data = NULL;
	} else {
		lvgl_res_preload_cancel_scene(SCENE_HEART_VIEW);
	}

	lvgl_res_unload_scene_compact(SCENE_HEART_VIEW);
	return 0;
}

static int _heart_view_focus_changed(view_data_t *view_data, bool focused)
{
	heart_view_data_t *data = view_data->user_data;

	if (focused) 
	{
		if(data)
        {
#ifdef CONFIG_SENSOR_MANAGER	
			sensor_manager_enable(IN_HEARTRATE, 0);
#endif
			_heart_view_preload(view_data, true);
			_load_resource(data);
			anim_timer_begin(data);
		}
	}
	else
	{
#ifdef CONFIG_SENSOR_MANAGER
		sensor_manager_disable(IN_HEARTRATE, 0);
#endif
        if(data)
        {
			lv_obj_set_width(data->hr_map_snapshot,0);
            _unload_resource(data);
			anim_timer_over(data);
			_heart_view_end(data);
        }
        
		lvgl_res_preload_cancel_scene(SCENE_HEART_VIEW);
		lvgl_res_unload_scene_compact(SCENE_HEART_VIEW);
	}

	return 0;
}

int _heart_view_handler(uint16_t view_id, uint8_t msg_id, void * msg_data)
{
	//LV_LOG_ERROR("msg_id = %d,view_id = %d,focus_id = %d",msg_id,view_id,view_cache_get_focus_view());
	view_data_t *view_data = msg_data;
	int ret = 0;
	switch (msg_id) {
	case MSG_VIEW_PRELOAD:
		return _heart_view_preload(view_data, false);
	case MSG_VIEW_LAYOUT:
		ret =  _heart_view_layout(view_data);
		if(!view_is_scrolling(HEART_VIEW))
			_heart_view_start();
		return ret;
	case MSG_VIEW_DELETE:
#ifdef CONFIG_SENSOR_MANAGER
		sensor_manager_disable(IN_HEARTRATE, 0);
#endif
		return _heart_view_delete(view_data);
	case MSG_VIEW_PAINT:
		return _heart_view_paint(view_data);
	case MSG_VIEW_FOCUS:
		return _heart_view_focus_changed(view_data, true);
	case MSG_VIEW_DEFOCUS:
		return _heart_view_focus_changed(view_data, false);
	case MSG_VIEW_SCROLL_END:
		_heart_view_start();
		return 0;
	default:
		return 0;
	}
	return 0;
}

VIEW_DEFINE(heart_view, _heart_view_handler, NULL, \
		NULL, HEART_VIEW, NORMAL_ORDER, UI_VIEW_LVGL, DEF_UI_VIEW_WIDTH, DEF_UI_VIEW_HEIGHT);
