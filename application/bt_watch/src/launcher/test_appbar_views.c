/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <os_common_api.h>
#include <lvgl.h>
#include <ui_manager.h>
#include <view_stack.h>
#include <app_ui.h>

static int _test_view_layout(uint16_t view_id, view_data_t *view_data)
{
	lv_obj_t *scr = lv_disp_get_scr_act(view_data->display);

	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

	if (view_id == TEST_TOP_VIEW1) {
		lv_obj_set_style_bg_color(scr, lv_color_make(255, 0, 0), LV_PART_MAIN);
		lv_obj_set_style_bg_grad_color(scr, lv_color_make(0, 0, 0), LV_PART_MAIN);
		lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, LV_PART_MAIN);
	} else if (view_id == TEST_TOP_VIEW2) {
		lv_obj_set_style_bg_color(scr, lv_color_make(0, 255, 0), LV_PART_MAIN);
		lv_obj_set_style_bg_grad_color(scr, lv_color_make(0, 0, 0), LV_PART_MAIN);
		lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, LV_PART_MAIN);
	} else if (view_id == TEST_TOP_VIEW3) {
		lv_obj_set_style_bg_color(scr, lv_color_make(0, 0, 255), LV_PART_MAIN);
		lv_obj_set_style_bg_grad_color(scr, lv_color_make(0, 0, 0), LV_PART_MAIN);
		lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, LV_PART_MAIN);
	} else {
		lv_obj_set_style_bg_color(scr, lv_color_make(0, 255, 0), LV_PART_MAIN);
	}

	return 0;
}

static int _test_view_handler(uint16_t view_id, uint8_t msg_id, void *msg_data)
{
	view_data_t *view_data = msg_data;

	switch (msg_id) {
	case MSG_VIEW_PRELOAD:
		return ui_view_layout(view_id);
	case MSG_VIEW_LAYOUT:
		return _test_view_layout(view_id, view_data);
	default:
		return 0;
	}
}

static int _test_bar_view_paint(uint16_t view_id, view_data_t *view_data)
{
	lv_obj_t *scr = lv_disp_get_scr_act(view_data->display);
	int focused_view = view_cache_get_focus_view();

	view_set_refresh_en(view_id, true);

	if (focused_view == TEST_TOP_VIEW1) {
		lv_obj_set_style_bg_color(scr, lv_color_make(255, 0, 0), LV_PART_MAIN);
	} else if (focused_view == TEST_TOP_VIEW2) {
		lv_obj_set_style_bg_color(scr, lv_color_make(0, 255, 0), LV_PART_MAIN);
	} else if (focused_view == TEST_TOP_VIEW3) {
		lv_obj_set_style_bg_color(scr, lv_color_make(0, 0, 255), LV_PART_MAIN);
	}

	return 0;
}

int _test_bar_view_handler(uint16_t view_id, uint8_t msg_id, void *msg_data)
{
	view_data_t *view_data = msg_data;

	switch (msg_id) {
	case MSG_VIEW_PRELOAD:
		return ui_view_layout(view_id);
	case MSG_VIEW_LAYOUT:
		return _test_view_layout(view_id, view_data);
	case MSG_VIEW_PAINT:
		return _test_bar_view_paint(view_id, view_data);
	default:
		return 0;
	}
}

VIEW_DEFINE(TEST_TOP_VIEW1, _test_view_handler, NULL,
		NULL, TEST_TOP_VIEW1, NORMAL_ORDER, UI_VIEW_LVGL, DEF_UI_VIEW_WIDTH, DEF_UI_VIEW_HEIGHT - 40);
VIEW_DEFINE(TEST_TOP_VIEW2, _test_view_handler, NULL,
		NULL, TEST_TOP_VIEW2, NORMAL_ORDER, UI_VIEW_LVGL, DEF_UI_VIEW_WIDTH, DEF_UI_VIEW_HEIGHT - 40);
VIEW_DEFINE(TEST_TOP_VIEW3, _test_view_handler, NULL,
		NULL, TEST_TOP_VIEW3, NORMAL_ORDER, UI_VIEW_LVGL, DEF_UI_VIEW_WIDTH, DEF_UI_VIEW_HEIGHT - 40);
VIEW_DEFINE(TEST_BAR_VIEW, _test_bar_view_handler, NULL,
		NULL, TEST_BAR_VIEW, NORMAL_ORDER + 1, UI_VIEW_LVGL, DEF_UI_VIEW_WIDTH, 40);

static void _view_monitor_proc(uint16_t view_id, uint8_t msg_id)
{
	if (msg_id == MSG_VIEW_SCROLL_END)
		ui_view_paint(TEST_BAR_VIEW);
}

static const uint16_t vlist[] = {
	TEST_TOP_VIEW1, TEST_TOP_VIEW2, TEST_TOP_VIEW3,
};

static const view_cache_dsc_t view_cache_dsc = {
	.type = LANDSCAPE,
	.num = ARRAY_SIZE(vlist),
	.vlist = vlist,
	.plist = NULL,
	.cross_vlist = { VIEW_INVALID_ID, VIEW_INVALID_ID },
	.cross_plist = { NULL, NULL },
	.monitor_cb = _view_monitor_proc,
};

void test_appbar_views_enter(void)
{
	view_stack_push_cache(&view_cache_dsc, TEST_TOP_VIEW2);
	ui_view_create(TEST_BAR_VIEW, NULL, UI_CREATE_FLAG_SHOW);
	ui_view_set_pos(TEST_BAR_VIEW, 0, DEF_UI_HEIGHT - 40);
}

void test_appbar_views_exit(void)
{
	ui_view_delete(TEST_BAR_VIEW);
	view_stack_pop();
}
