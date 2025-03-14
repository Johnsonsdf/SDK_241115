/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include "lvgl_porting.h"

/**********************
 *  STATIC FUNCTIONS
 **********************/

static void lvgl_port_disp_ready(void)
{
	lv_disp_t *disp = lv_disp_get_next(NULL);
	while (disp) {
		lv_timer_ready(disp->refr_timer);
		disp = lv_disp_get_next(disp);
	}

	/* Make anim timer ready */
	lv_timer_ready(lv_anim_get_timer());
}

static void lvgl_port_indev_ready(void)
{
	lv_indev_t *indev = lv_indev_get_next(NULL);
	while (indev) {
		lv_timer_ready(indev->driver->read_timer);
		indev = lv_indev_get_next(indev);
	}
}

static void lvgl_port_disp_pause(void)
{
	lv_disp_t *disp = lv_disp_get_next(NULL);
	while (disp) {
		if (disp->refr_timer)
			lv_timer_pause(disp->refr_timer);

		disp = lv_disp_get_next(disp);
	}

	/* Make anim timer pause */
	lv_timer_pause(lv_anim_get_timer());
}

static void lvgl_port_indev_pause(void)
{
	lv_indev_t *indev = lv_indev_get_next(NULL);
	while (indev) {
		lv_timer_pause(indev->driver->read_timer);
		indev = lv_indev_get_next(indev);
	}
}

static void lvgl_port_disp_resume(void)
{
	lv_disp_t *disp = lv_disp_get_next(NULL);
	while (disp) {
		if (disp->refr_timer)
			lv_timer_resume(disp->refr_timer);

		disp = lv_disp_get_next(disp);
	}

	/* Make anim timer resume */
	lv_timer_resume(lv_anim_get_timer());
}

static void lvgl_port_indev_resume(void)
{
	lv_indev_t *indev = lv_indev_get_next(NULL);
	while (indev) {
		lv_timer_resume(indev->driver->read_timer);
		indev = lv_indev_get_next(indev);
	}
}

static void lvgl_port_disp_update_period(uint32_t period)
{
	lv_disp_t *disp = lv_disp_get_next(NULL);
	while (disp) {
		if (disp->refr_timer)
			lv_timer_set_period(disp->refr_timer, period);

		disp = lv_disp_get_next(disp);
	}

	/* Update anim timer period */
	lv_timer_set_period(lv_anim_get_timer(), period);
}

static void lvgl_port_indev_update_period(uint32_t period)
{
	lv_indev_t *indev = lv_indev_get_next(NULL);
	while (indev) {
		lv_timer_set_period(indev->driver->read_timer, period);
		indev = lv_indev_get_next(indev);
	}
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_set_manual(lv_disp_t * disp)
{
	if (disp == NULL)
		disp = lv_disp_get_default();

	if (disp && disp->refr_timer) {
		lv_timer_del(disp->refr_timer);
		disp->refr_timer = NULL;
	}
}

void lv_port_refr_set_manual(void)
{
	lv_disp_t * disp = lv_disp_get_next(NULL);
	while (disp) {
		if (disp->refr_timer) {
			lv_timer_del(disp->refr_timer);
			disp->refr_timer = NULL;
		}

		disp = lv_disp_get_next(disp);
	}

	/* Make anim timer pause */
	lv_timer_pause(lv_anim_get_timer());

	/* Make indev timer pause */
	lvgl_port_indev_pause();
}

void lv_port_refr_now(void)
{
	lv_indev_t *indev = lv_indev_get_next(NULL);
	while (indev) {
		lv_indev_read_timer_cb(indev->driver->read_timer);
		indev = lv_indev_get_next(indev);
	}

	lv_disp_t *disp_def = lv_disp_get_default();
	if (disp_def && disp_def->refr_timer) {
		lv_refr_now(NULL);
	}
	else {
		lv_anim_refr_now();

		lv_disp_t *disp = lv_disp_get_next(NULL);
		while (disp) {
			lv_disp_set_default(disp);
			_lv_disp_refr_timer(NULL);

			disp = lv_disp_get_next(disp);
		}

		lv_disp_set_default(disp_def);
	}
}

void lv_port_refr_ready(void)
{
	lvgl_port_disp_ready();
	lvgl_port_indev_ready();
}

void lv_port_refr_pause(void)
{
	lvgl_port_disp_pause();
	lvgl_port_indev_pause();
}

void lv_port_refr_resume(void)
{
	lvgl_port_disp_resume();
	lvgl_port_indev_resume();
}

void lv_port_update_refr_period(uint32_t period)
{
	lvgl_port_disp_update_period(period);
	lvgl_port_indev_update_period(period);
}
