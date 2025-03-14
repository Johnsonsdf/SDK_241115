/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief alarm app view
 */

#include "alarm.h"

#ifdef CONFIG_UI_MANAGER
#include <ui_manager.h>
#endif
#include <view_manager.h>
#include <lvgl/lvgl_res_loader.h>
#include <msgbox_cache.h>
#ifdef CONFIG_SYS_WAKELOCK
#include <sys_wakelock.h>
#endif

/* alarm ring bg */
enum {
	BMP_AL_RING_ICON = 0,
	BMP_AL_RING_NAME,
	BMP_AL_RING_ICON_L,

	NUM_AL_RING_BG_IMGS,
};

static const uint32_t al_ring_bg_bmp_ids[] = {
	PIC_ALARM_RING_ICON,
	PIC_ALARM_RING_NAME,
	PIC_ALARM_RING_I,
};
/* alarm ring btn */
enum {
	BTN_AL_RING_SN = 0,
	BTN_AL_RING_STOP,

	NUM_AL_RING_BTNS,
};

static const uint32_t al_ring_btn_def_ids[] = {
	PIC_BTN_SN,
	PIC_BTN_STOP,
};
static const uint32_t al_ring_btn_sel_ids[] = {
	PIC_BTN_SN_P,
	PIC_BTN_STOP_P,
};

typedef struct al_ring_view_data {
	lv_obj_t *obj_bg;
	lv_obj_t *obj_ring_bg[NUM_AL_RING_BG_IMGS];
	lv_obj_t *obj_ring_btn[NUM_AL_RING_BTNS];

	/* lvgl resource */
	lv_img_dsc_t img_dsc_ring_bg[NUM_AL_RING_BG_IMGS];
	lv_img_dsc_t img_dsc_def_ring_btn[NUM_AL_RING_BTNS];
	lv_img_dsc_t img_dsc_sel_ring_btn[NUM_AL_RING_BTNS];

	lv_point_t pt_ring_bg[NUM_AL_RING_BG_IMGS];
	lv_point_t pt_ring_btn[NUM_AL_RING_BTNS];

	lvgl_res_scene_t res_scene;
} al_ring_view_data_t;

static al_ring_view_data_t *p_al_ring_view_data;

void alarm_calendar_year_display(struct alarm_app_t *alarm, bool is_light)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[5] = {0};

	seg_led_manager_set_timeout_event_start();
	seg_led_display_icon(SLED_COL, false);
	snprintf(diplay_str, sizeof(diplay_str), "%04u", alarm->tm.tm_year);
	seg_led_display_string(SLED_NUMBER1, diplay_str, is_light);
	seg_led_manager_set_timeout_event(20000, NULL);
#endif
}

void alarm_calendar_mon_day_display(struct alarm_app_t *alarm, bool is_light)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[5] = {0};

	seg_led_manager_set_timeout_event_start();
	seg_led_display_icon(SLED_COL, false);
	snprintf(diplay_str, sizeof(diplay_str), "%02u%02u", alarm->tm.tm_mon, alarm->tm.tm_mday);
	seg_led_display_string(SLED_NUMBER1, diplay_str, is_light);
	seg_led_manager_set_timeout_event(20000, NULL);
#endif
}

void alarm_calendar_mon_flash(struct alarm_app_t *alarm, bool is_light)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[3] = {0};

	seg_led_manager_set_timeout_event_start();
	snprintf(diplay_str, sizeof(diplay_str), "%02u", alarm->tm.tm_mon);
	seg_led_display_string(SLED_NUMBER1, diplay_str, is_light);
	seg_led_manager_set_timeout_event(20000, NULL);
#endif
}

void alarm_calendar_day_flash(struct alarm_app_t *alarm, bool is_light)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[3] = {0};

	seg_led_manager_set_timeout_event_start();
	snprintf(diplay_str, sizeof(diplay_str), "%02u", alarm->tm.tm_mday);
	seg_led_display_string(SLED_NUMBER3, diplay_str, is_light);
	seg_led_manager_set_timeout_event(20000, NULL);
#endif
}

void alarm_clock_set_display(struct alarm_app_t *alarm)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[5] = {0};

	seg_led_manager_set_timeout_event_start();
	seg_led_display_icon(SLED_COL, true);
	snprintf(diplay_str, sizeof(diplay_str), "%02u%02u", alarm->tm.tm_hour, alarm->tm.tm_min);
	seg_led_display_string(SLED_NUMBER1, diplay_str, true);
	seg_led_manager_set_timeout_event(20000, NULL);
#endif
}

void alarm_clock_display(struct alarm_app_t *alarm, bool need_update)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[5] = {0};

	seg_led_manager_set_timeout_event_start();
	seg_led_display_icon(SLED_P1, false);
	if (need_update)
		alarm_manager_get_time(&alarm->tm);

	snprintf(diplay_str, sizeof(diplay_str), "%02u%02u", alarm->tm.tm_hour, alarm->tm.tm_min);
	seg_led_display_string(SLED_NUMBER1, diplay_str, true);

	if (alarm->reflash_counter % 2)
		seg_led_display_icon(SLED_COL, false);
	else
		seg_led_display_icon(SLED_COL, true);
	seg_led_manager_set_timeout_event(20000, NULL);
#endif
}

void alarm_clock_hour_flash(struct alarm_app_t *alarm, bool is_light)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[3] = {0};

	seg_led_manager_set_timeout_event_start();
	snprintf(diplay_str, sizeof(diplay_str), "%02u", alarm->tm.tm_hour);
	seg_led_display_string(SLED_NUMBER1, diplay_str, is_light);
	seg_led_manager_set_timeout_event(20000, NULL);
#endif
}

void alarm_clock_min_flash(struct alarm_app_t *alarm, bool is_light)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[3] = {0};

	seg_led_manager_set_timeout_event_start();
	snprintf(diplay_str, sizeof(diplay_str), "%02u", alarm->tm.tm_min);
	seg_led_display_string(SLED_NUMBER3, diplay_str, is_light);
	seg_led_manager_set_timeout_event(20000, NULL);
#endif
}

void alarm_onoff_display(struct alarm_app_t *alarm, bool is_light)
{
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_manager_set_timeout_event_start();
	seg_led_display_icon(SLED_COL, false);
	if (alarm->alarm_is_on) {
		seg_led_display_string(SLED_NUMBER1, " ON ", is_light);
	} else {
		seg_led_display_string(SLED_NUMBER1, " OFF", is_light);
	}
	seg_led_manager_set_timeout_event(20000, NULL);
#endif
}

void alarm_ringing_display(struct alarm_app_t *alarm)
{
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_display_icon(SLED_PLAY, true);
	if (memcmp(alarm->dir, "SD:ALARM", strlen("SD:ALARM")) == 0) {
		seg_led_display_icon(SLED_SD, true);
		seg_led_display_icon(SLED_USB, false);
	} else if (memcmp(alarm->dir, "USB:ALARM", strlen("USB:ALARM")) == 0) {
		seg_led_display_icon(SLED_USB, true);
		seg_led_display_icon(SLED_SD, false);
	}
	alarm_clock_display(alarm, true);
	seg_led_manager_set_timeout_event(0, NULL);
	alarm->set_ok = 0;
#ifdef CONFIG_THREAD_TIMER
	thread_timer_start(&alarm->monitor_timer, REFLASH_CLOCK_PERIOD, REFLASH_CLOCK_PERIOD);
#endif
#endif
}

void alarm_ringing_clock_flash(struct alarm_app_t *alarm, bool is_light)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[5] = {0};

	alarm_manager_get_time(&alarm->tm);
	snprintf(diplay_str, sizeof(diplay_str), "%02u%02u", alarm->tm.tm_hour, alarm->tm.tm_min);
	seg_led_display_string(SLED_NUMBER1, diplay_str, is_light);
	seg_led_display_icon(SLED_COL, is_light);
#endif
}

static void _alarm_ring_btn_evt_handler(lv_event_t *e)
{
	lv_event_code_t event = lv_event_get_code(e);
	lv_obj_t *obj = lv_event_get_current_target(e);

	if (!p_al_ring_view_data)
		return;

	if (event == LV_EVENT_CLICKED) {
		if (p_al_ring_view_data->obj_ring_btn[BTN_AL_RING_SN] == obj) {
			alarm_view_snooze_event();
		} else if (p_al_ring_view_data->obj_ring_btn[BTN_AL_RING_STOP] == obj) {
			alarm_view_stop_event();
		}
		SYS_LOG_INF("Clicked obj %p\n", obj);
	}
}

static void _alarm_ring_create_img_array(lv_obj_t *par, lv_obj_t **pobj, lv_point_t *pt,
										lv_img_dsc_t *img, uint32_t num)
{
	for (int i = 0; i < num; i++) {
		pobj[i] = lv_img_create(par);
		lv_img_set_src(pobj[i], &img[i]);
		lv_obj_set_pos(pobj[i], pt[i].x, pt[i].y);
	}
}

static void _alarm_ring_create_btn(lv_obj_t *par, lv_obj_t **pobj, lv_point_t *pt,
										lv_img_dsc_t *def, lv_img_dsc_t *sel)
{
	*pobj = lv_imgbtn_create(par);
	lv_obj_set_pos(*pobj, pt->x, pt->y);
	lv_obj_set_size(*pobj, def->header.w, def->header.h);
	lv_obj_add_event_cb(*pobj, _alarm_ring_btn_evt_handler, LV_EVENT_ALL, NULL);

	lv_imgbtn_set_src(*pobj, LV_IMGBTN_STATE_RELEASED, NULL, def, NULL);
	lv_imgbtn_set_src(*pobj, LV_IMGBTN_STATE_PRESSED, NULL, sel, NULL);
	lv_imgbtn_set_src(*pobj, LV_IMGBTN_STATE_CHECKED_RELEASED, NULL, def, NULL);
	lv_imgbtn_set_src(*pobj, LV_IMGBTN_STATE_CHECKED_PRESSED, NULL, sel, NULL);
}

static void _alarm_ring_unload_resource(al_ring_view_data_t *data)
{
	lvgl_res_unload_pictures(data->img_dsc_ring_bg, NUM_AL_RING_BG_IMGS);
	lvgl_res_unload_pictures(data->img_dsc_def_ring_btn, NUM_AL_RING_BTNS);
	lvgl_res_unload_pictures(data->img_dsc_sel_ring_btn, NUM_AL_RING_BTNS);

	lvgl_res_unload_scene(&data->res_scene);
}

static int _alarm_ring_load_resource(al_ring_view_data_t *data)
{
	int ret;

	/* scene */
	ret = lvgl_res_load_scene(SCENE_ALARM_RING_VIEW, &data->res_scene,
			DEF_STY_FILE, DEF_RES_FILE, DEF_STR_FILE);
	if (ret < 0) {
		SYS_LOG_ERR("SCENE_ALARM_SET_VIEW not found");
		return -ENOENT;
	}

	/* bg picture */
	ret = lvgl_res_load_pictures_from_scene(&data->res_scene, al_ring_bg_bmp_ids, data->img_dsc_ring_bg, data->pt_ring_bg, NUM_AL_RING_BG_IMGS);
	if (ret < 0) {
		goto out_exit;
	}
	/* btn picture */
	ret = lvgl_res_load_pictures_from_scene(&data->res_scene, al_ring_btn_def_ids, data->img_dsc_def_ring_btn, data->pt_ring_btn, NUM_AL_RING_BTNS);
	if (ret < 0) {
		goto out_exit;
	}
	ret = lvgl_res_load_pictures_from_scene(&data->res_scene, al_ring_btn_sel_ids, data->img_dsc_sel_ring_btn, data->pt_ring_btn, NUM_AL_RING_BTNS);
	if (ret < 0) {
		goto out_exit;
	}
	return ret;
out_exit:
	if (ret < 0) {
		_alarm_ring_unload_resource(data);
	}

	return ret;
}

void alarm_view_init(void)
{
	if (p_al_ring_view_data) {
		SYS_LOG_WRN("view data exist\n");
		return;
	}

	p_al_ring_view_data = app_mem_malloc(sizeof(*p_al_ring_view_data));
	if (!p_al_ring_view_data) {
		return;
	}
	memset(p_al_ring_view_data, 0, sizeof(*p_al_ring_view_data));

#ifdef CONFIG_SYS_WAKELOCK
	sys_wake_lock(FULL_WAKE_LOCK);
#endif

	msgbox_cache_popup(ALARM_MSGBOX_ID, NULL);

	SYS_LOG_INF("ok\n");
}

static void _alarm_view_exit(void)
{
	if (!p_al_ring_view_data)
		return;

	if (p_al_ring_view_data->obj_bg) {
		_alarm_ring_unload_resource(p_al_ring_view_data);
		lv_obj_del(p_al_ring_view_data->obj_bg);
	}

#ifdef CONFIG_SYS_WAKELOCK
	sys_wake_unlock(FULL_WAKE_LOCK);
#endif

	app_mem_free(p_al_ring_view_data);
	p_al_ring_view_data = NULL;

	SYS_LOG_INF("ok\n");
}

void alarm_view_deinit(void)
{
	if (p_al_ring_view_data) {
		msgbox_cache_close(ALARM_MSGBOX_ID, false);
		SYS_LOG_INF("ok\n");
	}
}

void * alarm_msgbox_popup_cb(uint16_t msgbox_id, uint8_t msg_id, void *msg_data, void *user_data)
{
	if (p_al_ring_view_data == NULL || msgbox_id != ALARM_MSGBOX_ID)
		return NULL;

	SYS_LOG_INF("msg_id %d\n", msg_id);

	if (msg_id == MSG_MSGBOX_PAINT) {
		return p_al_ring_view_data->obj_bg;
	}

	if (msg_id == MSG_MSGBOX_CANCEL || msg_id == MSG_MSGBOX_CLOSE) {
		goto exit_view;
	}

	if (msg_id != MSG_MSGBOX_POPUP) {
		return p_al_ring_view_data->obj_bg;
	}

	if (_alarm_ring_load_resource(p_al_ring_view_data)) {
		goto exit_view;
	}

	/* create bg image */
	p_al_ring_view_data->obj_bg = lv_img_create((lv_obj_t *)msg_data);
	if (p_al_ring_view_data->obj_bg == NULL) {
		SYS_LOG_ERR("obj_bg create faild\n");
		_alarm_ring_unload_resource(p_al_ring_view_data);
		goto exit_view;
	}

	lv_obj_set_pos(p_al_ring_view_data->obj_bg, 0, 0);
	lv_obj_set_size(p_al_ring_view_data->obj_bg, DEF_UI_WIDTH, DEF_UI_HEIGHT);
	lv_obj_set_style_bg_color(p_al_ring_view_data->obj_bg, lv_color_make(0x3b, 0x3b, 0x3b), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(p_al_ring_view_data->obj_bg, LV_OPA_COVER, LV_PART_MAIN);

	_alarm_ring_create_img_array(p_al_ring_view_data->obj_bg, p_al_ring_view_data->obj_ring_bg,
		p_al_ring_view_data->pt_ring_bg, p_al_ring_view_data->img_dsc_ring_bg, NUM_AL_RING_BG_IMGS);

	lv_obj_clear_flag(p_al_ring_view_data->obj_bg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
	/* create alarm ring btn image */
	_alarm_ring_create_btn(p_al_ring_view_data->obj_bg, &p_al_ring_view_data->obj_ring_btn[BTN_AL_RING_SN],
		&p_al_ring_view_data->pt_ring_btn[BTN_AL_RING_SN],&p_al_ring_view_data->img_dsc_def_ring_btn[BTN_AL_RING_SN],
		&p_al_ring_view_data->img_dsc_sel_ring_btn[BTN_AL_RING_SN]);
	lv_obj_add_flag(p_al_ring_view_data->obj_ring_btn[BTN_AL_RING_SN], LV_OBJ_FLAG_CHECKABLE);
	_alarm_ring_create_btn(p_al_ring_view_data->obj_bg, &p_al_ring_view_data->obj_ring_btn[BTN_AL_RING_STOP],
		&p_al_ring_view_data->pt_ring_btn[BTN_AL_RING_STOP], &p_al_ring_view_data->img_dsc_def_ring_btn[BTN_AL_RING_STOP],
		&p_al_ring_view_data->img_dsc_sel_ring_btn[BTN_AL_RING_STOP]);
	lv_obj_add_flag(p_al_ring_view_data->obj_ring_btn[BTN_AL_RING_STOP], LV_OBJ_FLAG_CHECKABLE);

	return p_al_ring_view_data->obj_bg;
exit_view:
	_alarm_view_exit();
	return NULL;
}
