/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lvgl/porting/lvgl_porting.h>
#include <lvgl/lvgl_view.h>
#include <lvgl/lvgl_memory.h>
#include <lvgl/lvgl_virtual_display.h>
#include <ui_surface.h>
#include <ui_service.h>
#include <ui_manager.h>
#include <display/display_hal.h>
#include "lvgl_input_dev.h"
#ifdef CONFIG_GLYPHIX
#include "gx_lvgx.h"
#endif

static void * _lvgl_view_init(surface_t * surface)
{
	lv_disp_t *disp = NULL;

	disp = lvgl_virtual_display_create(surface);
	if (disp == NULL) {
		SYS_LOG_ERR("Failed to create lvgl display");
	} else {
		/* set refr timer period to 1 ms, so it will always run when lv_timer_handler() invoked */
		lv_timer_set_period(disp->refr_timer, 1);
	}

	return disp;
}

static void _lvgl_view_deinit(void * display)
{
	lvgl_virtual_display_destroy(display);
}

static int _lvgl_view_resume(void * display, bool full_invalidate)
{
	lv_disp_t *disp = display;

	disp->driver->refr_paused = 0;

	/* redraw the inv areas if any */
	lv_timer_resume(disp->refr_timer);

	if (full_invalidate)
		lv_obj_invalidate(lv_disp_get_scr_act(disp));

	return 0;
}

static int _lvgl_view_pause(void * display)
{
	lv_disp_t *disp = display;

	disp->driver->refr_paused = 1;
	return 0;
}

static int _lvgl_view_set_default(void * display)
{
	return lvgl_virtual_display_set_default(display);
}

static int _lvgl_view_focus(void * display, bool focus)
{
	if (focus) {
		return lvgl_virtual_display_set_focus(display, false);
	}

	return 0;
}

static int _lvgl_view_refresh(void * display, bool full_refresh)
{
	lv_disp_t *disp = display;

	if (disp->driver->refr_paused == 0) {
		if (full_refresh)
			lv_obj_invalidate(lv_disp_get_scr_act(disp));

		lv_refr_now(disp);
		return 0;
	}

	return -EPERM;
}

static void _lvgl_view_task_handler(void)
{
	/* run indev read_timer() before display refr_timer */
	lv_indev_t *indev = lv_indev_get_next(NULL);
	while (indev) {
		if (indev->driver->read_timer)
			lv_indev_read_timer_cb(indev->driver->read_timer);

		indev = lv_indev_get_next(indev);
	}

	lv_timer_handler();
#ifdef CONFIG_GLYPHIX
	lvgx_process_events();
#endif
}

static const ui_view_gui_callback_t view_callback = {
	.init = _lvgl_view_init,
	.deinit = _lvgl_view_deinit,
	.resume = _lvgl_view_resume,
	.pause = _lvgl_view_pause,
	.set_default = _lvgl_view_set_default,
	.focus = _lvgl_view_focus,
	.refresh = _lvgl_view_refresh,
	.task_handler = _lvgl_view_task_handler,
};

static void _msg_callback(struct app_msg *msg, int res, void *data)
{
	uint32_t opa_format;
	uint32_t trans_format;

	if (LV_COLOR_DEPTH == 16) {
		opa_format = HAL_PIXEL_FORMAT_RGB_565;
		trans_format = HAL_PIXEL_FORMAT_ARGB_8565;
	} else {
		opa_format = (surface_get_max_possible_buffer_count() > 0) ?
					HAL_PIXEL_FORMAT_BGR_888 : HAL_PIXEL_FORMAT_XRGB_8888;
		trans_format = HAL_PIXEL_FORMAT_ARGB_8888;
	}

	lv_port_init();

	/* set anim timer period to 1 ms, so it will always run when lv_timer_handler() invoked */
	lv_timer_set_period(lv_anim_get_timer(), 1);

	/* register view callback */
	ui_service_register_gui_view_callback(UI_VIEW_LVGL, opa_format, trans_format, &view_callback);

	/* register input callback */
	lvgl_input_pointer_init();
}

int lvgl_view_system_init(void)
{
	if (is_in_ui_thread()) {
		_msg_callback(NULL, 0, NULL);
		return 0;
	}

	return ui_message_send_sync2(VIEW_INVALID_ID, MSG_VIEW_NULL, 0, _msg_callback);
}


#ifdef CONFIG_GLYPHIX
static void _lvgx_init_callback(struct app_msg *msg, int res, void *data)
{
    lvgx_init();
}

int lvgx_view_system_init(void)
{
	if (is_in_ui_thread()) {
		_lvgx_init_callback(NULL, 0, NULL);
		return 0;
    }
	return ui_message_send_sync2(VIEW_INVALID_ID, MSG_VIEW_NULL, 0, _lvgx_init_callback);
}
#endif
