/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <board_cfg.h>
#include <os_common_api.h>
#include <lvgl.h>
#include <lvgl/porting/lvgl_porting.h>
#include <lvgl/lvgl_bitmap_font.h>
#include <lvgl/lvgl_freetype_font.h>
#include <drivers/input/input_dev.h>
#include <mem_manager.h>
#include <sys_manager.h>
#include <bt_manager.h>
#include <thread_timer.h>
//#include <thread_wdg.h>
#include <sys_wakelock.h>
#include <sensor_manager.h>
#include <partition/partition.h>
#include "app_msg.h"
#include "ota_app.h"

// NOR:A ~ NOR:C --> file_id: 10 ~ 12 (firmware.xml)
#define DEF_FONT22_FILE			"/NOR:C/sans22.font"
#define DEF_VFONT_FILE      	"/NOR:C/vfont.ttf"

static void _disp_vsync_cb(const lv_port_disp_callback_t *callback, uint32_t timestamp);
static void _disp_pm_notify(const struct lv_port_disp_callback *callback, uint32_t pm_state);

static K_SEM_DEFINE(disp_vsync_sem, 0, 1);

static const lv_port_disp_callback_t disp_callback = {
	.vsync = _disp_vsync_cb,
	.pm_notify = _disp_pm_notify,
};

static uint8_t disp_active = 1;

static void _disp_vsync_cb(const lv_port_disp_callback_t *callback, uint32_t timestamp)
{
	k_sem_give(&disp_vsync_sem);
}

static void _disp_pm_notify(const struct lv_port_disp_callback *callback, uint32_t pm_state)
{
	switch (pm_state) {
	case PM_DEVICE_ACTION_EARLY_SUSPEND:
		app_msg_send(APP_NAME, MSG_UI, CMD_SCREEN_OFF);
		break;
	case PM_DEVICE_ACTION_LATE_RESUME:
		app_msg_send(APP_NAME, MSG_UI, CMD_SCREEN_ON);
		break;
	default:
		break;
	}
}

static void _keypad_callback(struct device *dev, struct input_value *val)
{
	static uint32_t last_value = 0;

	SYS_LOG_INF("type: %d, code:%d, value:%d\n", 
		val->keypad.type, val->keypad.code, val->keypad.value);

	if (last_value != val->keypad.value) {
		// screen on
		sys_wake_lock(FULL_WAKE_LOCK);
		sys_wake_unlock(FULL_WAKE_LOCK);

		last_value = val->keypad.value;
	}
}

static void _keypad_init(void)
{
	struct device *onoff_dev;

	onoff_dev = (struct device *)device_get_binding(CONFIG_INPUT_DEV_ACTS_ONOFF_KEY_NAME);
	if (!onoff_dev) {
		SYS_LOG_ERR("cannot found key dev %s", CONFIG_INPUT_DEV_ACTS_ONOFF_KEY_NAME);
		return;
	}

	input_dev_enable(onoff_dev);
	input_dev_register_notify(onoff_dev, _keypad_callback);
}

static void _lvgl_init(void)
{
	lv_port_init();
	lv_port_disp_register_callback(&disp_callback);

	// init font
	lvgl_bitmap_font_init(NULL);
	lvgl_freetype_font_init();
}

static void _anim_callback(void *obj, int32_t val)
{
	lv_obj_set_y(obj, val);
	lv_obj_invalidate(lv_scr_act());
}

static void _lvgl_anim_demo(const char* text)
{
	//static lv_font_t ttf_font;
	//static lv_font_t bmp_font;

	// open ttf font
	//if (lvgl_freetype_font_open(&ttf_font, DEF_VFONT_FILE, 24) < 0) {
		//SYS_LOG_ERR("ttf font not found");
		//return;
	//}

	// open bitmap font
	//if (lvgl_bitmap_font_open(&bmp_font, DEF_FONT22_FILE) < 0) {
		//SYS_LOG_ERR("bitmap font not found");
		//return;
	//}

	lv_obj_t * label = lv_label_create(lv_scr_act());
	lv_obj_set_pos(label, 100, 100);
	lv_obj_set_style_text_color(label, lv_color_make(255, 0, 0), LV_PART_MAIN);
	//lv_obj_set_style_text_font(label, &ttf_font, 0);  // set ttf font
	//lv_obj_set_style_text_font(label, &bmp_font, 0); // set bitmap font
	lv_label_set_text(label, text);

	lv_anim_t anim;
	lv_anim_init(&anim);
	lv_anim_set_var(&anim, label);
	lv_anim_set_values(&anim, 0, CONFIG_PANEL_VER_RES);
	lv_anim_set_time(&anim, 1000);
	lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
	lv_anim_set_playback_time(&anim, 1000);
	lv_anim_set_exec_cb(&anim, _anim_callback);
	lv_anim_start(&anim);

	lv_refr_now(lv_disp_get_default());
}

static int _sensor_callback(int evt_id, sensor_res_t *res)
{
	SYS_LOG_INF("evt_id=%d, handup=%d, step=%d", evt_id, res->handup,
		(uint32_t)res->pedometer.total_steps);

	return 0;
}

static void _sensor_init(void)
{
	sensor_manager_enable(ALGO_ACTIVITY_OUTPUT, 0);
	sensor_manager_enable(ALGO_HANDUP, 0);
	sensor_manager_add_callback(_sensor_callback);
}

static void _process_ui_msg(struct app_msg *msg)
{
	switch (msg->cmd) {
	case CMD_SCREEN_OFF:
		disp_active = 0;
		lv_port_refr_pause();
		break;
	case CMD_SCREEN_ON:
		disp_active = 1;
		lv_port_refr_resume();
		lv_obj_invalidate(lv_scr_act());
		lv_port_refr_ready();
		break;
	default:
		break;
	}
}

int main(void)
{
	// init system
	mem_manager_init();
	system_init();
	bt_manager_init();

	// init app msg
	app_msg_init();

	// init lvgl
	_lvgl_init();

	// ota check and show animation
	if (ota_is_already_running()) {
		SYS_LOG_ERR("Error: ota running!");
	} else if (ota_is_already_done()) {
		_lvgl_anim_demo("Ota done!");
	} else {
		_lvgl_anim_demo("Hello world!");
	}

	// init key device
	_keypad_init();

	// init sensor
	_sensor_init();

	// init ota
	ota_app_init();
	ota_app_init_bluetooth();

	system_ready();

	// start soft watchdog
	//thread_wdg_start();

	while (1) {
		struct app_msg msg;
		uint32_t lv_timeout = lv_task_handler();
		uint32_t tt_timeout = thread_timer_next_timeout();
		uint32_t timeout = MIN(lv_timeout, tt_timeout);

		int rc = poll_msg(&msg, &disp_vsync_sem, timeout);
		switch (rc) {
		case OS_POLL_MSG:
			SYS_LOG_INF("msg type=0x%x, cmd=0x%x", msg.type, msg.cmd);
			switch (msg.type) {
				case MSG_UI:
					_process_ui_msg(&msg);
					break;
				case MSG_OTA:
					ota_app_process();
					break;
			}
			if (msg.callback != NULL) {
				msg.callback(&msg, 0, NULL);
			}
			break;

		case OS_POLL_SEM:
			if (disp_active) {
				lv_port_refr_ready();
			}
			break;
		}

		thread_timer_handle_expired();
	}

	return 0;
}

