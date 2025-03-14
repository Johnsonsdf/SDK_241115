/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief system low power view
 */

#include <ui_manager.h>
#include <lvgl/lvgl_res_loader.h>
#include "app_ui.h"
#include "widgets/img_number.h"
#include <power_manager.h>
#include <msgbox_cache.h>
#ifdef CONFIG_SYS_WAKELOCK
#include <sys_wakelock.h>
#endif

/* alarm bg view */
enum {
	//BMP_LPOWER_BG = 0,
	BMP_TXT_CHARG = 0,
	BMP_TIP_ICON,
	BMP_TXT_LPOWER,
	BMP_TXT_ENERGY,
	BMP_TXT_PERCENT,
	BMP_CHARG_ICON,
	BMP_TXT_ENERGY_USE,

	NUM_LPOWER_IMGS,
};

enum {
	BTN_TXT_CONFIRM = 0,

	NUM_LPOWER_BTNS,
};

static const uint32_t lpower_bg_bmp_ids[] = {
	//PIC_LPOWER_BG,
	PIC_TXT_CHARG,
	PIC_TIP_ICON,
	PIC_TXT_LPOWER,
	PIC_TXT_ENERGY,
	PIC_TXT_PERCENT,
	PIC_CHARG_ICON,
	PIC_TXT_ENERGY_USE,
};

static const uint32_t lpower_btn_bmp_ids[] = {
	PIC_TXT_CONFIRM,
};

static const uint32_t res_grp_energy_num_id[] = {
	RES_LPOWER_NUM,//res_lpower_num
};

static const uint32_t lpower_num_ids[] = {
	PIC_NUM_0,
	PIC_NUM_1,
	PIC_NUM_2,
	PIC_NUM_3,
	PIC_NUM_4,
	PIC_NUM_5,
	PIC_NUM_6,
	PIC_NUM_7,
	PIC_NUM_8,
	PIC_NUM_9,
};

typedef struct lpower_view_data {
	lv_obj_t *obj_bg;
	lv_obj_t *obj_lpower_bg[NUM_LPOWER_IMGS];
	lv_obj_t *obj_lpower_btn[NUM_LPOWER_BTNS];
	lv_obj_t *obj_bat_val;

	/* lvgl resource */
	lv_img_dsc_t img_dsc_lpower_bg[NUM_LPOWER_IMGS];
	lv_img_dsc_t img_dsc_lpower_btn[NUM_LPOWER_BTNS];
	lv_img_dsc_t img_dsc_lpower_nums[10];

	lv_point_t pt_lpower_bg[NUM_LPOWER_IMGS];
	lv_point_t pt_lpower_btn[NUM_LPOWER_BTNS];

	lvgl_res_scene_t res_scene;
	lvgl_res_group_t resource;
	/* user data */
	uint8_t bat_val;
} lpower_view_data_t;

static lpower_view_data_t *p_lpower_view_data = NULL;

static void _lpower_view_exit(void);

static void _lpower_btn_evt_handler(lv_event_t *e)
{
	lv_event_code_t event = lv_event_get_code(e);
	lv_obj_t *obj = lv_event_get_current_target(e);

	if (!p_lpower_view_data)
		return;

	if (event == LV_EVENT_CLICKED) {
		SYS_LOG_INF("btn clicked\n");
		if (p_lpower_view_data->obj_lpower_btn[BTN_TXT_CONFIRM] == obj)
			msgbox_cache_close(LPOWER_MSGBOX_ID, false);
	} else if (event == LV_EVENT_VALUE_CHANGED) {
		SYS_LOG_INF("Toggled\n");
	}
}

static void _lpower_create_btn(lv_obj_t *par, lv_obj_t **pobj, lv_point_t *pt,
										lv_img_dsc_t *def, lv_img_dsc_t *sel)
{
	*pobj = lv_imgbtn_create(par);
	lv_obj_set_pos(*pobj, pt->x, pt->y);
	lv_obj_set_size(*pobj, def->header.w, def->header.h);
	lv_obj_add_event_cb(*pobj, _lpower_btn_evt_handler, LV_EVENT_ALL, NULL);

	lv_imgbtn_set_src(*pobj, LV_IMGBTN_STATE_RELEASED, NULL, def, NULL);
	lv_imgbtn_set_src(*pobj, LV_IMGBTN_STATE_PRESSED, NULL, sel, NULL);
	lv_imgbtn_set_src(*pobj, LV_IMGBTN_STATE_CHECKED_RELEASED, NULL, sel, NULL);
	lv_imgbtn_set_src(*pobj, LV_IMGBTN_STATE_CHECKED_PRESSED, NULL, def, NULL);

	lv_obj_set_ext_click_area(*pobj, lv_obj_get_style_width(*pobj, LV_PART_MAIN));
}

static void _lpower_create_img_array(lv_obj_t *par, lv_obj_t **pobj, lv_point_t *pt,
										lv_img_dsc_t *img, uint32_t num)
{
	for (int i = 0; i < num; i++) {
		pobj[i] = lv_img_create(par);
		lv_img_set_src(pobj[i], &img[i]);
		lv_obj_set_pos(pobj[i], pt[i].x, pt[i].y);
	}
}

static void _lpower_unload_resource(lpower_view_data_t *data)
{
	lvgl_res_unload_pictures(data->img_dsc_lpower_bg, NUM_LPOWER_IMGS);
	lvgl_res_unload_pictures(data->img_dsc_lpower_btn, NUM_LPOWER_BTNS);
	lvgl_res_unload_pictures(data->img_dsc_lpower_nums, ARRAY_SIZE(data->img_dsc_lpower_nums));

	lvgl_res_unload_scene(&data->res_scene);
}

static int _lpower_load_resource(lpower_view_data_t *data)
{
	int ret;

	/* scene */
	ret = lvgl_res_load_scene(SCENE_LOW_POWER_VIEW, &data->res_scene,
			DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
	if (ret < 0) {
		SYS_LOG_ERR("SCENE_LOW_POWER_VIEW not found");
		return -ENOENT;
	}

	/* background picture */
	ret = lvgl_res_load_pictures_from_scene(&data->res_scene, lpower_bg_bmp_ids, data->img_dsc_lpower_bg, data->pt_lpower_bg, NUM_LPOWER_IMGS);
	if (ret < 0) {
		goto out_exit;
	}
	/* btn picture */
	ret = lvgl_res_load_pictures_from_scene(&data->res_scene, lpower_btn_bmp_ids, data->img_dsc_lpower_btn, data->pt_lpower_btn, NUM_LPOWER_BTNS);
	if (ret < 0) {
		goto out_exit;
	}
	ret = lvgl_res_load_group_from_scene(&data->res_scene, res_grp_energy_num_id[0], &data->resource);
	if (ret < 0) {
		goto out_exit;
	}
	ret = lvgl_res_load_pictures_from_group( &data->resource, lpower_num_ids,
			data->img_dsc_lpower_nums, NULL, ARRAY_SIZE(lpower_num_ids));

	lvgl_res_unload_group(&data->resource);

	return 0;

out_exit:
	if (ret < 0) {
		_lpower_unload_resource(data);
	}

	return ret;
}

static void _lpower_view_paint(lpower_view_data_t *data)
{
	SYS_LOG_INF("paint");

#ifdef CONFIG_POWER
	data->bat_val = power_manager_get_battery_capacity();
#endif
	img_number_set_value(data->obj_bat_val, data->bat_val, 2);
}

void system_create_low_power_view(void)
{
	if (p_lpower_view_data) {
		SYS_LOG_WRN("view data exist\n");
		msgbox_cache_paint(LPOWER_MSGBOX_ID, 0);
		return;
	}

	p_lpower_view_data = app_mem_malloc(sizeof(*p_lpower_view_data));
	if (!p_lpower_view_data) {
		return;
	}

	memset(p_lpower_view_data, 0, sizeof(*p_lpower_view_data));

#ifdef CONFIG_SYS_WAKELOCK
	sys_wake_lock(FULL_WAKE_LOCK);
#endif

	msgbox_cache_popup(LPOWER_MSGBOX_ID, p_lpower_view_data);

	SYS_LOG_INF("create lpower view\n");
}

void system_delete_low_power_view(void)
{
	msgbox_cache_close(LPOWER_MSGBOX_ID, false);
}

static void _lpower_view_exit(void)
{
	if (p_lpower_view_data == NULL)
		return;

	if (p_lpower_view_data->obj_bg) {
		_lpower_unload_resource(p_lpower_view_data);
		lv_obj_del(p_lpower_view_data->obj_bg);
	}

#ifdef CONFIG_SYS_WAKELOCK
	sys_wake_unlock(FULL_WAKE_LOCK);
#endif

	app_mem_free(p_lpower_view_data);
	p_lpower_view_data = NULL;

	SYS_LOG_INF("ok\n");
}

void * lpower_msgbox_popup_cb(uint16_t msgbox_id, uint8_t msg_id, void *msg_data, void *user_data)
{
	if (!p_lpower_view_data || msgbox_id != LPOWER_MSGBOX_ID)
		return NULL;

	SYS_LOG_INF("msg_id %d\n", msg_id);

	if (msg_id == MSG_MSGBOX_PAINT) {
		_lpower_view_paint(p_lpower_view_data);
		return p_lpower_view_data->obj_bg;
	}

	if (msg_id == MSG_MSGBOX_CANCEL || msg_id == MSG_MSGBOX_CLOSE) {
		goto exit_view;
	}

	if (msg_id != MSG_MSGBOX_POPUP) {
		return p_lpower_view_data->obj_bg;
	}

	/* create lpower bg image */
	if (_lpower_load_resource(p_lpower_view_data)) {
		SYS_LOG_ERR("res load failed");
		goto exit_view;
	}

	/* create bg image */
	p_lpower_view_data->obj_bg = lv_img_create((lv_obj_t *)msg_data);
	if (p_lpower_view_data->obj_bg == NULL) {
		SYS_LOG_ERR("obj_bg create err\n");
		_lpower_unload_resource(p_lpower_view_data);
		goto exit_view;
	}

	lv_obj_set_pos(p_lpower_view_data->obj_bg, 0, 0);
	lv_obj_set_size(p_lpower_view_data->obj_bg, DEF_UI_WIDTH, DEF_UI_HEIGHT);
	lv_obj_set_style_bg_color(p_lpower_view_data->obj_bg, lv_color_make(0x3b, 0x3b, 0x3b), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(p_lpower_view_data->obj_bg, LV_OPA_COVER, LV_PART_MAIN);

	_lpower_create_img_array(p_lpower_view_data->obj_bg, p_lpower_view_data->obj_lpower_bg, p_lpower_view_data->pt_lpower_bg, p_lpower_view_data->img_dsc_lpower_bg, NUM_LPOWER_IMGS);

	_lpower_create_btn(p_lpower_view_data->obj_bg, &p_lpower_view_data->obj_lpower_btn[BTN_TXT_CONFIRM], &p_lpower_view_data->pt_lpower_btn[BTN_TXT_CONFIRM], p_lpower_view_data->img_dsc_lpower_btn, p_lpower_view_data->img_dsc_lpower_btn);

	lv_obj_add_flag(p_lpower_view_data->obj_lpower_btn[BTN_TXT_CONFIRM], LV_OBJ_FLAG_CLICKABLE);

	p_lpower_view_data->obj_bat_val = img_number_create(p_lpower_view_data->obj_bg);
	lv_obj_set_pos(p_lpower_view_data->obj_bat_val, p_lpower_view_data->resource.x, p_lpower_view_data->resource.y);
	lv_obj_set_size(p_lpower_view_data->obj_bat_val, p_lpower_view_data->resource.width, p_lpower_view_data->resource.height);
	img_number_set_src(p_lpower_view_data->obj_bat_val, p_lpower_view_data->img_dsc_lpower_nums, 10);
	img_number_set_align(p_lpower_view_data->obj_bat_val, LV_ALIGN_RIGHT_MID);

	_lpower_view_paint(p_lpower_view_data);

	return p_lpower_view_data->obj_bg;
exit_view:
	_lpower_view_exit();
	return NULL;
}
