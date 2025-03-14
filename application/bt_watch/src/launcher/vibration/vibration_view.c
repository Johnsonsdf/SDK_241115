/*
 * Copyright (c) 2022 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file gps view
 */

#include <assert.h>
#include <view_stack.h>
#ifdef CONFIG_SYS_WAKELOCK
#include <sys_wakelock.h>
#endif
#include "app_ui.h"
#include "app_defines.h"
#include "system_app.h"
#include "vibration_view.h"
#include "vibration_ui.h"

enum {
	TXT_VIB_TITLE = 0,
	TXT_LOW,
	TXT_MID,
	TXT_HIGH,	
	NUM_TXTS,
};

const static uint32_t _txt_ids[] = {
	STR_VIB_TITLE,
	STR_LOW,
	STR_MID,
	STR_HIGH,
};

enum {
	BTN_TOP,
	BTN_LOW,
	BTN_MID,
	BTN_HIGH,
	BTN_BUTTOM,
	NUM_BTNS,
};

typedef struct btn_info {
	uint8_t btn_id;
	uint8_t txt_id;
	uint8_t level_id;
}t_btn_info;

const static t_btn_info btn_info_list[] = {
	{BTN_TOP, 0xff, 0xff},
	{BTN_LOW, TXT_LOW, LEVEL_1},
	{BTN_MID, TXT_MID, LEVEL_2},
	{BTN_HIGH, TXT_HIGH, LEVEL_3},
	{BTN_BUTTOM, 0xff, 0xff},
};

static int32_t vib_preload_inited = 0;

typedef struct vib_view_data {
	/* lvgl object */
	lv_obj_t *cont;
	lv_obj_t *lbl[NUM_TXTS];
	lv_obj_t *btn[NUM_BTNS];
	/* ui-editor resource */
	lvgl_res_scene_t res_scene;
	lv_point_t pt_txt[NUM_TXTS];
	lvgl_res_string_t res_txt[NUM_TXTS];
	
	/* lvgl resource */
	lv_style_t small_txt_style[NUM_TXTS];
	lv_style_t normal_txt_style[NUM_TXTS];
	lv_font_t normal_font;
	lv_font_t small_font;
} vib_view_data_t;

static void scroll_event_cb(lv_event_t * e)
{
	view_data_t *view_data = lv_event_get_user_data(e);
	vib_view_data_t *data = view_data->user_data;
	const vibration_view_presenter_t *presenter = view_get_presenter(view_data);

    lv_obj_t * cont = lv_event_get_target(e);
	
    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);
	
    lv_coord_t cont_y_center = cont_a.y1 + lv_area_get_height(&cont_a) / 2;

	//SYS_LOG_INF("scroll_event_cb in, cont_y_center:0x%x", cont_y_center);
    uint32_t i,target_i = 0;

	lv_coord_t diff_y;
	lv_coord_t diff_y_min = LV_COORD_MAX;
	lv_coord_t child_height = 0;

	for(i = 0; i < NUM_BTNS; i++) {
		if(btn_info_list[i].level_id == 0xff)
			continue;
		
        lv_obj_t *child = data->btn[i];
        lv_area_t child_a;
        lv_obj_get_coords(child, &child_a);
		child_height = lv_area_get_height(&child_a);
        lv_coord_t child_y_center = child_a.y1 + child_height/ 2;
        diff_y = child_y_center - cont_y_center;
        diff_y = LV_ABS(diff_y);

		//SYS_LOG_INF("[%d] diff_y: %d", i, diff_y);

		if(diff_y < diff_y_min)
		{
			diff_y_min = diff_y;
			target_i = i;
		}
    }
	
	for(i = 0; i < NUM_BTNS; i++) {
		lv_obj_t *btn = data->btn[i];
		lv_obj_t *label = lv_obj_get_child(btn, 0);
		if (!label) {
			continue;
		}
	
		if(btn_info_list[i].txt_id == 0xff)
				continue;
		
		int txt_id = btn_info_list[i].txt_id;
		if(i == target_i) {
			lv_obj_remove_style(label, &data->small_txt_style[txt_id], LV_PART_MAIN);
			lv_obj_add_style(label, &data->normal_txt_style[txt_id], LV_PART_MAIN);
		} else {
			lv_obj_remove_style(label, &data->normal_txt_style[txt_id], LV_PART_MAIN);
			lv_obj_add_style(label, &data->small_txt_style[txt_id], LV_PART_MAIN);
		}
    }

	lv_obj_scroll_to_y(cont, (target_i-BTN_LOW)*child_height, LV_ANIM_ON);
	
	if (presenter)
		presenter->set_level(btn_info_list[target_i].level_id);
}


static void _cvt_txt_array(lv_point_t *pt, lv_style_t *sty, lv_font_t *font, lvgl_res_string_t *txt, uint32_t num)
{
	int i;

	for (i = 0; i < num; i++) {
		pt[i].x = txt[i].x;
		pt[i].y = txt[i].y;

		lv_style_init(&sty[i]);
		lv_style_set_text_font(&sty[i], font);
		lv_style_set_text_color(&sty[i], txt[i].color);
	}
}


static int _load_resource(vib_view_data_t *data, bool first_layout)
{
	int32_t ret;

	if(first_layout)
	{
		/* load scene */
		ret = lvgl_res_load_scene(SCENE_VIB_VIEW, &data->res_scene, DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
		if (ret < 0) {
			SYS_LOG_ERR("SCENE_VIB_VIEW not found");
			return -ENOENT;
		}

		/* open font */
		if (LVGL_FONT_OPEN_DEFAULT(&data->normal_font, DEF_FONT_SIZE_NORMAL) < 0) {
			SYS_LOG_ERR("font not found");
			return -ENOENT;
		}

		/* open small font */
		if (LVGL_FONT_OPEN_DEFAULT(&data->small_font, DEF_FONT_SIZE_SMALL) < 0) {
			SYS_LOG_ERR("font not found");
			return -ENOENT;
		}

		/* load string */
		lvgl_res_load_strings_from_scene(&data->res_scene, _txt_ids, data->res_txt, NUM_TXTS);

		/* init style */
		_cvt_txt_array(data->pt_txt, data->normal_txt_style, &data->normal_font, data->res_txt, NUM_TXTS);
		_cvt_txt_array(data->pt_txt, data->small_txt_style, &data->small_font, data->res_txt, NUM_TXTS);
	}

	SYS_LOG_INF("load resource succeed");

	return 0;
}

static void _unload_pic_resource(vib_view_data_t *data)
{
	lvgl_res_unload_strings(data->res_txt, NUM_TXTS);
}

static void _unload_resource(vib_view_data_t *data)
{
	lvgl_res_unload_strings(data->res_txt, NUM_TXTS);
	lvgl_res_unload_scene(&data->res_scene);

	LVGL_FONT_CLOSE(&data->normal_font);
	LVGL_FONT_CLOSE(&data->small_font);
}

static int _vib_view_preload(view_data_t *view_data)
{
	if (vib_preload_inited == 0) {
		lvgl_res_preload_scene_compact_default_init(SCENE_VIB_VIEW, NULL, 0,
			NULL, DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
		vib_preload_inited = 1;
	}
	
	return lvgl_res_preload_scene_compact_default(SCENE_VIB_VIEW, VIB_VIEW, 0, 0);
}


static int _vib_view_layout_update(view_data_t *view_data, bool first_layout)
{
	lv_obj_t *scr = lv_disp_get_scr_act(view_data->display);
	vib_view_data_t *data = view_data->user_data;
	const vibration_view_presenter_t *presenter = view_get_presenter(view_data);

	if(first_layout)
	{
		data = app_mem_malloc(sizeof(*data));
		if (!data) {
			return -ENOMEM;
		}
		memset(data, 0, sizeof(*data));

		view_data->user_data = data;
	}

	if (_load_resource(data, first_layout)) {
		app_mem_free(data);
		view_data->user_data = NULL;
		return -ENOENT;
	}
	

	// create label
	if(first_layout)
	{
		/* background */
		data->cont = lv_obj_create(scr);
		lv_obj_set_size(data->cont, LV_PCT(100), LV_PCT(100));
		lv_obj_set_style_bg_color(data->cont, lv_color_make(0, 0, 0), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(data->cont, LV_OPA_COVER, LV_PART_MAIN);
		lv_obj_set_user_data(scr, data);

		/* show title */
		lv_obj_t *title = lv_label_create(data->cont);
		lv_obj_set_pos(title, data->pt_txt[0].x, data->pt_txt[0].y);
		lv_label_set_text(title, data->res_txt[TXT_VIB_TITLE].txt);
		lv_obj_add_style(title, data->normal_txt_style, LV_PART_MAIN);
	
		lv_obj_t * selected_area = lv_obj_create(data->cont);
		lv_obj_set_pos(selected_area, 20, 205);
	    lv_obj_set_size(selected_area, lv_pct(90), lv_pct(15));
		lv_obj_set_style_radius(selected_area, 30, LV_PART_MAIN);
		lv_obj_set_style_bg_color(selected_area, lv_color_make(0x1f, 0x1f, 0x1f), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(selected_area, LV_OPA_COVER, LV_PART_MAIN);
		
	    lv_obj_t * choose_area = lv_obj_create(data->cont);
	    lv_obj_set_size(choose_area, lv_pct(100), lv_pct(60));
	    lv_obj_center(choose_area);
		lv_obj_set_flex_flow(choose_area, LV_FLEX_FLOW_COLUMN);
	    lv_obj_add_event_cb(choose_area, scroll_event_cb, LV_EVENT_SCROLL_END, view_data);
	    lv_obj_set_style_clip_corner(choose_area, true, 0);
	    lv_obj_set_scroll_dir(choose_area, LV_DIR_VER);
	    lv_obj_set_scrollbar_mode(choose_area, LV_SCROLLBAR_MODE_OFF);
		/*create buttons*/
		int i;
		uint8_t cur_level = 0;

		if (presenter) {
			cur_level = presenter->get_level();	
		}
		
		for(i = 0; i < NUM_BTNS; i++) {
			data->btn[i] = lv_btn_create(choose_area);
			lv_obj_set_size(data->btn[i], lv_pct(100), lv_pct(33));
			lv_obj_center(data->btn[i]);

			if(btn_info_list[i].txt_id == 0xff)
				continue;

			int txt_id = btn_info_list[i].txt_id;
			lv_obj_t * label = lv_label_create(data->btn[i]);
			lv_label_set_text(label, data->res_txt[txt_id].txt);
			if(btn_info_list[i].level_id == cur_level) {
				lv_obj_add_style(label, &data->normal_txt_style[txt_id], LV_PART_MAIN);
			} else {
				lv_obj_add_style(label, &data->small_txt_style[txt_id], LV_PART_MAIN);
			}
			lv_obj_center(label);
		}
		uint8_t index = BTN_LOW+cur_level+1;
		if(index >= NUM_BTNS) {
			index = NUM_BTNS - 1;
		}
		lv_obj_scroll_to_view(data->btn[index], LV_ANIM_OFF);
	}

	return 0;
}

static int _vib_view_layout(view_data_t *view_data)
{
	int ret;

	ret = _vib_view_layout_update(view_data, 1);
	if(ret < 0)
	{
		return ret;
	}

#ifdef CONFIG_SYS_WAKELOCK
	sys_wake_lock(FULL_WAKE_LOCK);
#endif

	lv_refr_now(view_data->display);
	SYS_LOG_INF("_vib_view_layout");

	return 0;
}

static int _vib_view_delete(view_data_t *view_data)
{
	vib_view_data_t *data = view_data->user_data;
	int i;

	if (data) {
		for(i=0;i<NUM_TXTS;i++)
		{
			lv_style_reset(&data->normal_txt_style[i]);
			lv_style_reset(&data->small_txt_style[i]);
		}
		lv_obj_del(data->cont);

		_unload_resource(data);
		app_mem_free(data);
		view_data->user_data = NULL;

#ifdef CONFIG_SYS_WAKELOCK
		sys_wake_unlock(FULL_WAKE_LOCK);
#endif
	} else {
		lvgl_res_preload_cancel();
	}

	lvgl_res_unload_scene_compact(SCENE_VIB_VIEW);
	return 0;
}

static int _vib_view_updated(view_data_t* view_data)
{
	int ret;

	ret = _vib_view_layout_update(view_data, 0);	
	return ret;
	
}

static int _vib_view_focus_changed(view_data_t *view_data, bool focused)
{
	vib_view_data_t *data = view_data->user_data;
	const vibration_view_presenter_t *presenter = view_get_presenter(view_data);

	if (focused) 
	{
		if(!lvgl_res_scene_is_loaded(SCENE_VIB_VIEW))
		{
			lvgl_res_preload_scene_compact_default(SCENE_VIB_VIEW, VIB_VIEW, 1, 0);
		}

		if (presenter) {
			presenter->set_freq(VIB_PWM_FREQ);
			presenter->start();
		}
	}
	else
	{
		if(data)
		{
			_unload_pic_resource(data);
		}
		lvgl_res_preload_cancel_scene(SCENE_VIB_VIEW);
		lvgl_res_unload_scene_compact(SCENE_VIB_VIEW);

		if (presenter)
			presenter->stop();
	}

	return 0;
}

int _vib_view_handler(uint16_t view_id, uint8_t msg_id, void * msg_data)
{
	view_data_t *view_data = msg_data;

	switch (msg_id) {
	case MSG_VIEW_PRELOAD:
		return _vib_view_preload(view_data);
	case MSG_VIEW_LAYOUT:
		return _vib_view_layout(view_data);
	case MSG_VIEW_DELETE:
		return _vib_view_delete(view_data);
	case MSG_VIEW_FOCUS:
		return _vib_view_focus_changed(view_data, 1);
	case MSG_VIEW_DEFOCUS:
		return _vib_view_focus_changed(view_data, 0);
	case MSG_VIEW_UPDATE:
		return _vib_view_updated(view_data);		
	default:
		return 0;
	}
	return 0;
}

VIEW_DEFINE(vib_view, _vib_view_handler, NULL, \
		NULL, VIB_VIEW, NORMAL_ORDER, UI_VIEW_LVGL, DEF_UI_WIDTH, DEF_UI_HEIGHT);

