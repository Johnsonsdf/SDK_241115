/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <app_ui.h>
#include "main_view.h"
#include <widgets/simple_img.h>
#include "applist/applist_ui.h"
#include <view_stack.h>

extern const main_view_presenter_t main_view_presenter;

static const uint32_t pic_on_off_ids[] = {
	PIC_ENABLED,PIC_DISABILITY
};

static const uint32_t anim_set_text_ids[] = {
	ANIM_SET_TEXT_1,ANIM_SET_TEXT_2,ANIM_SET_TEXT_3,ANIM_SET_TEXT_4, ANIM_SET_TEXT_5
};

typedef struct anim_set_view_data {
	lv_obj_t *tv;
	lv_obj_t *on_off_icon[ARRAY_SIZE(anim_set_text_ids)];
	lv_img_dsc_t img_on_off[ARRAY_SIZE(pic_on_off_ids)];
	lv_font_t font;

	lvgl_res_group_t layout_1;
	lvgl_res_group_t layout_2;

	lvgl_res_group_t select_group;
	lvgl_res_scene_t res_scene;
	lvgl_res_string_t res_txt[ARRAY_SIZE(anim_set_text_ids)];
} anim_set_data_t;


static void _anim_set_mode(uint8_t id)
{
	main_view_presenter.set_scroll_mode(id);
}

static void _anim_set_up(anim_set_data_t *data)
{
	uint8_t num = main_view_presenter.get_scroll_mode();

	for(uint8_t i = 0 ; i < ARRAY_SIZE(data->res_txt) ; i++)
	{
		if(num == i)
			simple_img_set_src(data->on_off_icon[i], &data->img_on_off[0]);
		else
			simple_img_set_src(data->on_off_icon[i], &data->img_on_off[1]);
	}
}

static void _anim_set_btn_evt_handler(lv_event_t *e)
{
	lv_event_code_t event = lv_event_get_code(e);
	anim_set_data_t *data = lv_event_get_user_data(e);
	int32_t btn_id = (int32_t)lv_obj_get_user_data(lv_event_get_current_target(e));
	if (event == LV_EVENT_CLICKED) {
		_anim_set_mode(btn_id);
		_anim_set_up(data);
	}
}

static int _anim_set_ui_preload(view_data_t *view_data)
{
	return lvgl_res_preload_scene_compact(SCENE_ANIM_SET_VIEW, NULL, 0, lvgl_res_scene_preload_default_cb_for_view, (void *)ANIM_SET_VIEW,
			DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
}

static void _anim_set_unload_resource(anim_set_data_t *data)
{
	lvgl_res_unload_group(&data->layout_1);
	lvgl_res_unload_group(&data->layout_2);
	lvgl_res_unload_group(&data->select_group);
	lvgl_res_unload_strings(data->res_txt, ARRAY_SIZE(data->res_txt));
	lvgl_res_unload_pictures(data->img_on_off, ARRAY_SIZE(pic_on_off_ids));
	LVGL_FONT_CLOSE(&data->font);
}

static int _anim_set_load_resource(anim_set_data_t *data)
{
	int ret;

	/* scene */
	ret = lvgl_res_load_scene(SCENE_ANIM_SET_VIEW, &data->res_scene,
			DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
	if (ret < 0) {
		SYS_LOG_ERR("SCENE_ANIM_SET_VIEW not found");
		return -ENOENT;
	}

	ret = lvgl_res_load_group_from_scene(&data->res_scene,LAYOUT_RECT_1, &data->layout_1);
	if (ret < 0) {
		goto out_exit;
	}

	ret = lvgl_res_load_group_from_scene(&data->res_scene,LAYOUT_RECT_2, &data->layout_2);
	if (ret < 0) {
		goto out_exit;
	}

	ret = lvgl_res_load_group_from_scene(&data->res_scene,ANIM_SET_SELECT, &data->select_group);
	if (ret < 0) {
		goto out_exit;
	}

	ret = lvgl_res_load_pictures_from_group(&data->select_group, pic_on_off_ids, data->img_on_off,NULL, ARRAY_SIZE(data->img_on_off));
	if (ret < 0) {
		goto out_exit;
	}

	lvgl_res_group_t text_string;
	ret = lvgl_res_load_group_from_scene(&data->res_scene,ANIM_SET_LIST_STRING, &text_string);
	if (ret < 0) {
		goto out_exit;
	}
	ret = lvgl_res_load_strings_from_group(&text_string, anim_set_text_ids, data->res_txt, ARRAY_SIZE(data->res_txt));
	if (ret < 0) {
		goto out_exit;
	}
	lvgl_res_unload_group(&text_string);

	ret = LVGL_FONT_OPEN_DEFAULT(&data->font, DEF_FONT_SIZE_NORMAL);
	if (ret < 0) {
		goto out_exit;
	}

out_exit:
	if (ret < 0) {
		_anim_set_unload_resource(data);
	}
	lvgl_res_unload_scene(&data->res_scene);

	return ret;
}

static int _anim_set_ui_layout(view_data_t *view_data)
{
	lv_obj_t *scr = lv_disp_get_scr_act(view_data->display);
	anim_set_data_t *data = NULL;

	data = app_mem_malloc(sizeof(*data));
	if (!data) {
		return -ENOMEM;
	}

	memset(data, 0, sizeof(*data));

	if (_anim_set_load_resource(data)) {
		app_mem_free(data);
		return -ENOENT;
	}
	lv_coord_t layout_space = data->layout_2.y - data->layout_1.y;
	data->tv = lv_obj_create(scr);
	lv_obj_set_size(data->tv,0,0);
	lv_obj_set_size(data->tv,DEF_UI_VIEW_WIDTH,data->layout_1.y * 2 + layout_space * ARRAY_SIZE(data->res_txt));
	lv_obj_set_style_pad_all(data->tv,0,LV_PART_MAIN);
	lv_obj_set_style_border_width(data->tv,0,LV_PART_MAIN);
	lv_coord_t text_x = data->layout_1.width >> 3;
	for(uint32_t i = 0 ; i < ARRAY_SIZE(data->res_txt) ; i++)
	{
		lv_obj_t *anim_set_icon = lv_obj_create(data->tv);
		lv_obj_set_pos(anim_set_icon,data->layout_1.x,data->layout_1.y + layout_space * i);
		lv_obj_set_size(anim_set_icon,data->layout_1.width,data->layout_1.height);
		lv_obj_set_style_pad_all(anim_set_icon,0,LV_PART_MAIN);
		lv_obj_set_style_border_width(anim_set_icon,0,LV_PART_MAIN);
		lv_obj_set_style_bg_color(anim_set_icon,lv_color_hex(data->layout_1.group_data->sty_data->backgroud),LV_PART_MAIN);
		lv_obj_set_style_bg_opa(anim_set_icon,data->layout_1.group_data->sty_data->opaque,LV_PART_MAIN);
		lv_obj_add_event_cb(anim_set_icon, _anim_set_btn_evt_handler, LV_EVENT_CLICKED, data);
		lv_obj_set_user_data(anim_set_icon,(void *)i);

		lv_obj_t *text_icon = lv_label_create(anim_set_icon);
		lv_obj_align(text_icon,LV_ALIGN_LEFT_MID,text_x,0);
		lv_obj_set_width(text_icon,DEF_UI_WIDTH - text_x * 2);
		lv_label_set_long_mode(text_icon,LV_LABEL_LONG_SCROLL);
		lv_obj_set_style_text_align(text_icon,LV_TEXT_ALIGN_LEFT,LV_PART_MAIN);
		lv_obj_set_style_text_font(text_icon,&data->font,LV_PART_MAIN);
		lv_obj_set_style_text_color(text_icon,data->res_txt[i].color,LV_PART_MAIN);
		lv_label_set_text(text_icon,data->res_txt[i].txt);

		data->on_off_icon[i] = simple_img_create(anim_set_icon);
		lv_obj_align(data->on_off_icon[i],LV_ALIGN_RIGHT_MID,data->select_group.x,0);
	}

	view_data->user_data = data;
	_anim_set_up(data);
	return 0;
}

static int _anim_set_ui_delete(view_data_t *view_data)
{
	anim_set_data_t *data = view_data->user_data;

	if (data) {
		lv_obj_del(data->tv);
		_anim_set_unload_resource(data);
		app_mem_free(data);
		view_data->user_data = NULL;
	} else {
		lvgl_res_preload_cancel_scene(SCENE_ANIM_SET_VIEW);
	}
	lvgl_res_unload_scene_compact(SCENE_ANIM_SET_VIEW);
	return 0;
}

int _anim_set_ui_handler(uint16_t view_id, uint8_t msg_id, void * msg_data)
{
	view_data_t *view_data = view_get_data(view_id);
	switch (msg_id) {
	case MSG_VIEW_PRELOAD:
		return _anim_set_ui_preload(view_data);
	case MSG_VIEW_LAYOUT:
		return _anim_set_ui_layout(view_data);
	case MSG_VIEW_DELETE:
		return _anim_set_ui_delete(view_data);
	default:
		return 0;
	}
	return 0;
}
VIEW_DEFINE(anim_set_ui, _anim_set_ui_handler, NULL, \
		NULL, ANIM_SET_VIEW, NORMAL_ORDER, UI_VIEW_LVGL, DEF_UI_VIEW_WIDTH, DEF_UI_VIEW_HEIGHT);

static const uint32_t pic_list_ids[] = {
	PIC_LIST_1, PIC_LIST_2, PIC_BABYSBREATH, PIC_WATERFALL_FLOW,
	PIC_ROLLER_PLATE, PIC_WONHOT,PIC_CUBE_TURNTABLE
};

static const uint32_t view_set_text_ids[] = {
	VIEW_SET_TEXT_1,VIEW_SET_TEXT_2,VIEW_SET_TEXT_3,
	VIEW_SET_TEXT_4,VIEW_SET_TEXT_5,VIEW_SET_TEXT_6,
	VIEW_SET_TEXT_7
};

typedef struct view_set_view_data {
	lv_obj_t *tv;
	lv_img_dsc_t img_list[ARRAY_SIZE(pic_list_ids)];
	lv_img_dsc_t img_on_off[ARRAY_SIZE(pic_on_off_ids)];
	lv_font_t font;
	lv_obj_t *on_off_icon[ARRAY_SIZE(pic_list_ids)];
	lvgl_res_scene_t res_scene;

	lvgl_res_group_t layout_1;
	lvgl_res_group_t layout_2;

	lvgl_res_group_t img_group;
	lvgl_res_group_t select_group;

	lvgl_res_string_t res_txt[ARRAY_SIZE(view_set_text_ids)];
} view_set_data_t;

static void _view_set_mode(uint8_t id)
{
	uint8_t mode = APPLIST_MODE_LIST;
	switch (id)
	{
		case 0:
			mode = APPLIST_MODE_LIST;
			break;
		case 1:
			mode = APPLIST_MODE_WATERWHEEL;
			break;
		case 2:
			mode = APPLIST_MODE_CELLULAR;
			break;
		case 3:
			mode = APPLIST_MODE_WATERFALL;
			break;
		case 4:
			mode = APPLIST_MODE_TURNTABLE;
			break;
		case 5:
			mode = APPLIST_MODE_WONHOT;
			break;
		case 6:
			mode = APPLIST_MODE_CUBE_TURNTABLE;
			break;
		default:
			break;
	}
	applist_view_presenter.set_view_mode(mode);
}

static void _view_set_up(view_set_data_t *data)
{
	uint8_t mode = applist_view_presenter.get_view_mode();
	uint8_t num = 0xFF;
	switch (mode)
	{
	case APPLIST_MODE_LIST:
		num = 0;
		break;
	case APPLIST_MODE_WATERWHEEL:
		num = 1;
		break;
	case APPLIST_MODE_CELLULAR:
		num = 2;
		break;
	case APPLIST_MODE_WATERFALL:
		num = 3;
		break;
	case APPLIST_MODE_TURNTABLE:
		num = 4;
		break;
	case APPLIST_MODE_WONHOT:
		num = 5;
		break;
	case APPLIST_MODE_CUBE_TURNTABLE:
		num = 6;
	default:
		break;
	}
	for(uint8_t i = 0 ; i < ARRAY_SIZE(pic_list_ids) ; i++)
	{
		if(num == i)
			simple_img_set_src(data->on_off_icon[i], &data->img_on_off[0]);
		else
			simple_img_set_src(data->on_off_icon[i], &data->img_on_off[1]);
	}
}

static void _view_set_btn_evt_handler(lv_event_t *e)
{
	lv_event_code_t event = lv_event_get_code(e);
	int32_t btn_id = (int32_t)lv_obj_get_user_data(lv_event_get_current_target(e));
	view_set_data_t *data = lv_event_get_user_data(e);
	if (event == LV_EVENT_CLICKED) {
		_view_set_mode(btn_id);
		_view_set_up(data);
	}
}

static int _view_set_ui_preload(view_data_t *view_data)
{
	return lvgl_res_preload_scene_compact(SCENE_VIEW_SET_VIEW, NULL, 0, lvgl_res_scene_preload_default_cb_for_view, (void *)VIEW_SET_VIEW,
			DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
}

static void _view_set_unload_resource(view_set_data_t *data)
{
	LVGL_FONT_CLOSE(&data->font);
	lvgl_res_unload_group(&data->layout_1);
	lvgl_res_unload_group(&data->layout_2);
	lvgl_res_unload_group(&data->img_group);
	lvgl_res_unload_group(&data->select_group);
	lvgl_res_unload_strings(data->res_txt, ARRAY_SIZE(data->res_txt));
	lvgl_res_unload_pictures(data->img_list, ARRAY_SIZE(data->img_list));
	lvgl_res_unload_pictures(data->img_on_off, ARRAY_SIZE(data->img_on_off));
}

static int _view_set_ui_load_resource(view_set_data_t *data)
{
	int ret;

	/* scene */
	ret = lvgl_res_load_scene(SCENE_VIEW_SET_VIEW, &data->res_scene,
			DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
	if (ret < 0) {
		SYS_LOG_ERR("SCENE_VIEW_SET_VIEW not found");
		return -ENOENT;
	}

	ret = lvgl_res_load_group_from_scene(&data->res_scene,LAYOUT_RECT_1, &data->layout_1);
	if (ret < 0) {
		goto out_exit;
	}

	ret = lvgl_res_load_group_from_scene(&data->res_scene,LAYOUT_RECT_2, &data->layout_2);
	if (ret < 0) {
		goto out_exit;
	}

	ret = lvgl_res_load_group_from_scene(&data->res_scene,VIEW_SET_IMG, &data->img_group);
	if (ret < 0) {
		goto out_exit;
	}

	ret = lvgl_res_load_pictures_from_group(&data->img_group, pic_list_ids, data->img_list,NULL, ARRAY_SIZE(data->img_list));
	if (ret < 0) {
		goto out_exit;
	}

	ret = lvgl_res_load_group_from_scene(&data->res_scene,VIEW_SET_SELECT, &data->select_group);
	if (ret < 0) {
		goto out_exit;
	}

	ret = lvgl_res_load_pictures_from_group(&data->select_group, pic_on_off_ids, data->img_on_off,NULL, ARRAY_SIZE(data->img_on_off));
	if (ret < 0) {
		goto out_exit;
	}

	lvgl_res_group_t text_string;
	ret = lvgl_res_load_group_from_scene(&data->res_scene,VIEW_SET_LIST_STRING, &text_string);
	if (ret < 0) {
		goto out_exit;
	}
	ret = lvgl_res_load_strings_from_group(&text_string, view_set_text_ids, data->res_txt, ARRAY_SIZE(data->res_txt));
	if (ret < 0) {
		goto out_exit;
	}
	lvgl_res_unload_group(&text_string);

	ret = LVGL_FONT_OPEN_DEFAULT(&data->font, DEF_FONT_SIZE_NORMAL);
	if (ret < 0) {
		goto out_exit;
	}

out_exit:
	if (ret < 0) {
		_view_set_unload_resource(data);
	}
	lvgl_res_unload_scene(&data->res_scene);

	return ret;
}

static int _view_set_ui_layout(view_data_t *view_data)
{
	lv_obj_t *scr = lv_disp_get_scr_act(view_data->display);
	view_set_data_t *data = NULL;

	data = app_mem_malloc(sizeof(*data));
	if (!data) {
		return -ENOMEM;
	}

	memset(data, 0, sizeof(*data));

	if (_view_set_ui_load_resource(data)) {
		app_mem_free(data);
		return -ENOENT;
	}
	lv_coord_t layout_space = data->layout_2.y - data->layout_1.y;
	data->tv = lv_obj_create(scr);
	lv_obj_set_size(data->tv,0,0);
	lv_obj_set_size(data->tv,DEF_UI_VIEW_WIDTH,data->layout_1.y * 2 + layout_space * ARRAY_SIZE(data->img_list));
	lv_obj_set_style_pad_all(data->tv,0,LV_PART_MAIN);
	lv_obj_set_style_border_width(data->tv,0,LV_PART_MAIN);
	lv_coord_t text_x = data->img_group.x + data->img_list[0].header.w + (data->img_list[0].header.w>>2);
	for(uint32_t i = 0 ; i < ARRAY_SIZE(data->img_list) ; i++)
	{
		lv_obj_t *view_set_icon = lv_obj_create(data->tv);
		lv_obj_set_size(view_set_icon,data->layout_1.width,data->layout_1.height);
		lv_obj_set_pos(view_set_icon,data->layout_1.x,data->layout_1.y + layout_space*i);
		lv_obj_set_style_pad_all(view_set_icon,0,LV_PART_MAIN);
		lv_obj_set_style_border_width(view_set_icon,0,LV_PART_MAIN);
		lv_obj_set_style_bg_color(view_set_icon,lv_color_hex(data->layout_1.group_data->sty_data->backgroud),LV_PART_MAIN);
		lv_obj_set_style_bg_opa(view_set_icon,data->layout_1.group_data->sty_data->opaque,LV_PART_MAIN);
		lv_obj_add_event_cb(view_set_icon, _view_set_btn_evt_handler, LV_EVENT_CLICKED, data);
		lv_obj_set_user_data(view_set_icon,(void *)i);

		lv_obj_t *view_img_icon = simple_img_create(view_set_icon);
		lv_obj_align(view_img_icon,LV_ALIGN_LEFT_MID,data->img_group.x,0);
		simple_img_set_src(view_img_icon, &data->img_list[i]);

		lv_obj_t *text_icon = lv_label_create(view_set_icon);
		lv_obj_align(text_icon,LV_ALIGN_LEFT_MID,text_x,0);
		lv_obj_set_width(text_icon,data->layout_1.width - (text_x * 2));
		lv_label_set_long_mode(text_icon,LV_LABEL_LONG_SCROLL);
		lv_obj_set_style_text_align(text_icon,LV_TEXT_ALIGN_LEFT,LV_PART_MAIN);
		lv_obj_set_style_text_font(text_icon,&data->font,LV_PART_MAIN);
		lv_obj_set_style_text_color(text_icon,data->res_txt[i].color,LV_PART_MAIN);
		lv_label_set_text(text_icon,data->res_txt[i].txt);

		data->on_off_icon[i] = simple_img_create(view_set_icon);
		lv_obj_align(data->on_off_icon[i],LV_ALIGN_RIGHT_MID,data->select_group.x,0);
	}
	 
	view_data->user_data = data;
	_view_set_up(data);

	
	return 0;
}

static int _view_set_ui_delete(view_data_t *view_data)
{
	view_set_data_t *data = view_data->user_data;

	if (data) {
		lv_obj_del(data->tv);
		_view_set_unload_resource(data);
		app_mem_free(data);
		view_data->user_data = NULL;
	} else {
		lvgl_res_preload_cancel_scene(SCENE_VIEW_SET_VIEW);
	}
	lvgl_res_unload_scene_compact(SCENE_VIEW_SET_VIEW);
	return 0;
}

int _view_set_ui_handler(uint16_t view_id, uint8_t msg_id, void * msg_data)
{
	view_data_t *view_data = view_get_data(view_id);
	switch (msg_id) {
	case MSG_VIEW_PRELOAD:
		return _view_set_ui_preload(view_data);
	case MSG_VIEW_LAYOUT:
		return _view_set_ui_layout(view_data);
	case MSG_VIEW_DELETE:
		return _view_set_ui_delete(view_data);
	default:
		return 0;
	}
	return 0;
}

VIEW_DEFINE(view_set_ui, _view_set_ui_handler, NULL, \
		NULL, VIEW_SET_VIEW, NORMAL_ORDER, UI_VIEW_LVGL, DEF_UI_VIEW_WIDTH, DEF_UI_VIEW_HEIGHT);

static const uint32_t text_list_ids[] = {
	SETTING_TEXT_1, SETTING_TEXT_2};

typedef struct setting_view_data {
	lv_obj_t *tv;
	lv_font_t font;
	lvgl_res_scene_t res_scene;

	lvgl_res_group_t layout_1;
	lvgl_res_group_t layout_2;

	lvgl_res_string_t res_txt[ARRAY_SIZE(text_list_ids)];
} setting_data_t;

static void _setting_btn_evt_handler(lv_event_t *e)
{
	lv_event_code_t event = lv_event_get_code(e);
	int32_t btn_id = (int32_t)lv_obj_get_user_data(lv_event_get_current_target(e));
	if (event == LV_EVENT_CLICKED) {
		if(btn_id == 0)
			view_stack_push_view(VIEW_SET_VIEW, NULL);
		else if(btn_id == 1)
			view_stack_push_view(ANIM_SET_VIEW, NULL);
	}
}

static void _setting_ui_unload_resource(setting_data_t *data)
{
	lvgl_res_unload_group(&data->layout_1);
	lvgl_res_unload_group(&data->layout_2);
	lvgl_res_unload_strings(data->res_txt, ARRAY_SIZE(data->res_txt));
	LVGL_FONT_CLOSE(&data->font);
}

static int _setting_ui_load_resource(setting_data_t *data)
{
	int ret;

	ret = lvgl_res_load_scene(SCENE_SETTING_VIEW, &data->res_scene,
			DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
	if (ret < 0) {
		SYS_LOG_ERR("SCENE_SETTING_VIEW not found");
		return -ENOENT;
	}

	ret = lvgl_res_load_group_from_scene(&data->res_scene,LAYOUT_RECT_1, &data->layout_1);
	if (ret < 0) {
		goto out_exit;
	}

	ret = lvgl_res_load_group_from_scene(&data->res_scene,LAYOUT_RECT_2, &data->layout_2);
	if (ret < 0) {
		goto out_exit;
	}

	lvgl_res_group_t text_string;
	ret = lvgl_res_load_group_from_scene(&data->res_scene,SETTING_LIST_STRING, &text_string);
	if (ret < 0) {
		goto out_exit;
	}
	ret = lvgl_res_load_strings_from_group(&text_string, text_list_ids, data->res_txt, ARRAY_SIZE(data->res_txt));
	if (ret < 0) {
		goto out_exit;
	}
	lvgl_res_unload_group(&text_string);

	ret = LVGL_FONT_OPEN_DEFAULT(&data->font, DEF_FONT_SIZE_NORMAL);
	if (ret < 0) {
		goto out_exit;
	}

	out_exit:
	if (ret < 0) {
		_setting_ui_unload_resource(data);
	}
	lvgl_res_unload_scene(&data->res_scene);

	return ret;
}

static int _setting_ui_preload(view_data_t *view_data)
{
	return lvgl_res_preload_scene_compact(SCENE_SETTING_VIEW, NULL, 0, lvgl_res_scene_preload_default_cb_for_view, (void *)SETTING_VIEW,
			DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
}

static int _setting_ui_layout(view_data_t *view_data)
{
	lv_obj_t *scr = lv_disp_get_scr_act(view_data->display);
	setting_data_t *data = app_mem_malloc(sizeof(*data));
	if (!data) {
		return -ENOMEM;
	}
	memset(data, 0, sizeof(*data));

	view_data->user_data = data;

	if (_setting_ui_load_resource(data)) {
		app_mem_free(data);
		view_data->user_data = NULL;
		return -ENOENT;
	}
	lv_coord_t layout_space = data->layout_2.y - data->layout_1.y;
	data->tv = lv_obj_create(scr);
	lv_obj_set_size(data->tv,0,0);
	lv_obj_set_size(data->tv,DEF_UI_VIEW_WIDTH,data->layout_1.y * 2 + layout_space * ARRAY_SIZE(data->res_txt));
	lv_obj_set_style_pad_all(data->tv,0,LV_PART_MAIN);
	lv_obj_set_style_border_width(data->tv,0,LV_PART_MAIN);
	for(uint32_t i = 0 ; i < ARRAY_SIZE(data->res_txt) ; i++)
	{
		lv_obj_t *setting_icon = lv_obj_create(data->tv);
		lv_obj_set_pos(setting_icon,data->layout_1.x,data->layout_1.y + layout_space * i);
		lv_obj_set_size(setting_icon,data->layout_1.width,data->layout_1.height);
		lv_obj_set_style_pad_all(setting_icon,0,LV_PART_MAIN);
		lv_obj_set_style_border_width(setting_icon,0,LV_PART_MAIN);
		lv_obj_set_style_radius(setting_icon,20,LV_PART_MAIN);
		lv_obj_set_style_bg_color(setting_icon,lv_color_hex(data->layout_1.group_data->sty_data->backgroud),LV_PART_MAIN);
		lv_obj_set_style_bg_opa(setting_icon,data->layout_1.group_data->sty_data->opaque,LV_PART_MAIN);
		lv_obj_add_event_cb(setting_icon, _setting_btn_evt_handler, LV_EVENT_CLICKED, data);
		lv_obj_set_user_data(setting_icon,(void *)i);

		lv_obj_t *set_text_icon = lv_label_create(setting_icon);
		lv_obj_align(set_text_icon,LV_ALIGN_CENTER,0,0);
		lv_obj_set_width(set_text_icon,data->layout_1.width);
		lv_label_set_long_mode(set_text_icon,LV_LABEL_LONG_SCROLL);
		lv_obj_set_style_text_align(set_text_icon,LV_TEXT_ALIGN_CENTER,LV_PART_MAIN);
		lv_obj_set_style_text_font(set_text_icon,&data->font,LV_PART_MAIN);
		lv_obj_set_style_text_color(set_text_icon,data->res_txt[i].color,LV_PART_MAIN);
		lv_label_set_text(set_text_icon,data->res_txt[i].txt);
	}

	return 0;
}

static int _setting_ui_delete(view_data_t *view_data)
{
	setting_data_t *data = view_data->user_data;

	if (data) {
		lv_obj_del(data->tv);
		_setting_ui_unload_resource(data);
		app_mem_free(data);
		view_data->user_data = NULL;
	} else {
		lvgl_res_preload_cancel_scene(SCENE_SETTING_VIEW);
	}
	lvgl_res_unload_scene_compact(SCENE_SETTING_VIEW);
	return 0;
}

int _setting_ui_handler(uint16_t view_id, uint8_t msg_id, void * msg_data)
{
	view_data_t *view_data = view_get_data(view_id);
	switch (msg_id) {
	case MSG_VIEW_PRELOAD:
		return _setting_ui_preload(view_data);
	case MSG_VIEW_LAYOUT:
		return _setting_ui_layout(view_data);
	case MSG_VIEW_DELETE:
		return _setting_ui_delete(view_data);
	default:
		return 0;
	}
	return 0;
}
VIEW_DEFINE(setting_ui, _setting_ui_handler, NULL, \
		NULL, SETTING_VIEW, NORMAL_ORDER, UI_VIEW_LVGL, DEF_UI_VIEW_WIDTH, DEF_UI_VIEW_HEIGHT);
