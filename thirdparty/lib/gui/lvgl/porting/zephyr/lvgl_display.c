/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if !defined(CONFIG_UI_SERVICE) && !defined(CONFIG_DISPLAY_COMPOSER)

/*********************
 *      INCLUDES
 *********************/

#include <zephyr.h>
#include <board.h>
#include <sys/atomic.h>
#include <drivers/display.h>
#include <drivers/display/display_graphics.h>
#include <memory/mem_cache.h>
#include "lvgl_inner.h"

/**********************
 *      TYPEDEFS
 **********************/

typedef struct lvgl_disp_flush_data {
	uint8_t idx;
	lv_area_t area;
	const lv_color_t * buf;
} lvgl_disp_flush_data_t;

typedef struct lvgl_disp_user_data {
	const struct device *disp_dev;    /* display device */
	struct display_callback disp_cb; /* display callback to register */
	struct display_capabilities disp_cap;  /* display capabilities */
	struct k_sem wait_sem;        /* display complete wait sem */
	struct k_sem ready_sem;

	uint8_t pm_state;
	bool initialized;

	bool flushing;
	uint8_t flush_idx;
	atomic_t flush_cnt;

	uint8_t flush_data_wr;
	uint8_t flush_data_rd;
	lvgl_disp_flush_data_t flush_data[CONFIG_LV_VDB_NUM];

	const lv_port_disp_callback_t *user_cb;
} lvgl_disp_user_data_t;

/**********************
 *  STATIC VARIABLES
 **********************/

/* NOTE:
 * 1) depending on chosen color depth buffer may be accessed using uint8_t *,
 * uint16_t * or uint32_t *, therefore buffer needs to be aligned accordingly to
 * prevent unaligned memory accesses.
 * 2) must align each buffer address and size to psram cache line size (32 bytes)
 * if allocated in psram.
 * 3) Verisilicon vg_lite buffer memory requires 64 bytes aligned
 */
#ifdef CONFIG_VG_LITE
#  define BUFFER_ALIGN 64
#else
#  define BUFFER_ALIGN 32
#endif

#define BUFFER_SIZE (((CONFIG_LV_VDB_SIZE * LV_COLOR_DEPTH / 8) + (BUFFER_ALIGN - 1)) & ~(BUFFER_ALIGN - 1))
#define NBR_PIXELS_IN_BUFFER (BUFFER_SIZE * 8 / LV_COLOR_DEPTH)

static uint8_t vdb_buf_0[BUFFER_SIZE] __aligned(BUFFER_ALIGN) __in_section_unique(lvgl.noinit.vdb.0);
#if CONFIG_LV_VDB_NUM > 1
static uint8_t vdb_buf_1[BUFFER_SIZE] __aligned(BUFFER_ALIGN) __in_section_unique(lvgl.noinit.vdb.1);
#endif
#if CONFIG_LV_VDB_NUM > 2
static uint8_t vdb_buf_2[BUFFER_SIZE] __aligned(BUFFER_ALIGN) __in_section_unique(lvgl.noinit.vdb.2);
#endif
#if CONFIG_LV_VDB_NUM > 3
static uint8_t vdb_buf_3[BUFFER_SIZE] __aligned(BUFFER_ALIGN) __in_section_unique(lvgl.noinit.vdb.3);
#endif

static lv_disp_drv_t lv_disp_drv;
static lv_disp_draw_buf_t lv_disp_buf;
static lvgl_disp_user_data_t disp_user_data;

static K_SEM_DEFINE(s_disp_sem, 0, 1);

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void lvgl_flush_cb(lv_disp_drv_t *disp_drv,
		const lv_area_t *area, lv_color_t *color_p);
static void lvgl_rounder_cb(lv_disp_drv_t * disp_drv, lv_area_t * area);
static void lvgl_wait_cb(lv_disp_drv_t *disp_drv);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void _set_rendering_buffers(lv_disp_drv_t *disp_drv)
{
	lvgl_disp_user_data_t *drv_data = disp_drv->user_data;
	const struct device *display_dev = drv_data->disp_dev;

	display_get_capabilities(display_dev, &drv_data->disp_cap);

	disp_drv->hor_res = drv_data->disp_cap.x_resolution;
	disp_drv->ver_res = drv_data->disp_cap.y_resolution;
	disp_drv->draw_buf = &lv_disp_buf;
	switch (drv_data->disp_cap.current_orientation) {
	case DISPLAY_ORIENTATION_ROTATED_90:
		disp_drv->rotated = LV_DISP_ROT_270;
		break;
	case DISPLAY_ORIENTATION_ROTATED_180:
		disp_drv->rotated = LV_DISP_ROT_180;
		break;
	case DISPLAY_ORIENTATION_ROTATED_270:
		disp_drv->rotated = LV_DISP_ROT_90;
		break;
	case DISPLAY_ORIENTATION_NORMAL:
	default:
		disp_drv->rotated = LV_DISP_ROT_NONE;
		break;
	}

	if (disp_drv->rotated == LV_DISP_ROT_NONE) {
		disp_drv->direct_mode = (((uint32_t)disp_drv->hor_res * disp_drv->ver_res) <= NBR_PIXELS_IN_BUFFER);
	} else {
		disp_drv->sw_rotate = 1;
	}

	LV_LOG_USER("Display INFO:");
	LV_LOG_USER("resolution  : %d x %d", disp_drv->hor_res, disp_drv->ver_res);
	LV_LOG_USER("rotated     : %d (sw_rotate %d)", disp_drv->rotated, disp_drv->sw_rotate);
	LV_LOG_USER("render mode : direct_mode %d, buf_size %d, buf_cnt %d",
				disp_drv->direct_mode, NBR_PIXELS_IN_BUFFER, CONFIG_LV_VDB_NUM);

	uint8_t *bufs[CONFIG_LV_VDB_NUM] = {
		vdb_buf_0,
#if CONFIG_LV_VDB_NUM > 1
		vdb_buf_1,
#endif
#if CONFIG_LV_VDB_NUM > 2
		vdb_buf_2,
#endif
#if CONFIG_LV_VDB_NUM > 3
		vdb_buf_3,
#endif
	};

	lv_disp_draw_buf_init2(disp_drv->draw_buf, (void **)bufs, CONFIG_LV_VDB_NUM, NBR_PIXELS_IN_BUFFER);
}

static void _display_flush(lvgl_disp_user_data_t *drv_data)
{
	lvgl_disp_flush_data_t *flush_data = &drv_data->flush_data[drv_data->flush_data_rd];
	struct display_buffer_descriptor desc;
	const lv_color_t *flush_buf = flush_data->buf;
	uint16_t line_length;
	uint32_t stride;

	desc.width = lv_area_get_width(&flush_data->area);
	desc.height = lv_area_get_height(&flush_data->area);

	if (lv_disp_drv.direct_mode) {
		stride = lv_disp_drv.hor_res;
		flush_buf += flush_data->area.y1 * stride + flush_data->area.x1;
	} else {
		stride = desc.width;
	}

	line_length = stride * sizeof(lv_color_t);
	desc.buf_size = line_length * desc.height;

#ifdef CONFIG_SOC_SERIES_LARK
	desc.pitch = stride;
	desc.pixel_format = (LV_COLOR_DEPTH == 16) ?
			PIXEL_FORMAT_RGB_565 : PIXEL_FORMAT_XRGB_8888;
#else
	desc.pitch = line_length;
	desc.pixel_format = (LV_COLOR_DEPTH == 16) ?
			PIXEL_FORMAT_BGR_565 : PIXEL_FORMAT_XRGB_8888;
#endif

	drv_data->flushing = true;
	if (++drv_data->flush_data_rd >= ARRAY_SIZE(drv_data->flush_data)) {
		drv_data->flush_data_rd = 0;
	}

	display_write(drv_data->disp_dev, flush_data->area.x1, flush_data->area.y1, &desc, flush_buf);
}

static void _display_vsync_cb(const struct display_callback *callback, uint32_t timestamp)
{
	lvgl_disp_user_data_t *drv_data =
			CONTAINER_OF(callback, lvgl_disp_user_data_t, disp_cb);
	lvgl_disp_flush_data_t *flush_data = &drv_data->flush_data[drv_data->flush_data_rd];

	if (!drv_data->initialized) {
		drv_data->initialized = true;
		k_sem_give(&drv_data->ready_sem);
	}

	if (drv_data->flushing == false && atomic_get(&drv_data->flush_cnt) > 0 && flush_data->idx == 0) {
		_display_flush(drv_data);
	}

	lvgl_port_indev_pointer_scan();

	if (drv_data->user_cb && drv_data->user_cb->vsync) {
		drv_data->user_cb->vsync(drv_data->user_cb, timestamp);
	}
}

static void _display_complete_cb(const struct display_callback *callback)
{
	lvgl_disp_user_data_t *drv_data =
			CONTAINER_OF(callback, lvgl_disp_user_data_t, disp_cb);
	lvgl_disp_flush_data_t *flush_data = &drv_data->flush_data[drv_data->flush_data_rd];

	drv_data->flushing = false;

	if (atomic_dec(&drv_data->flush_cnt) > 1 && flush_data->idx > 0) {
		_display_flush(drv_data);
	}

	lv_disp_flush_ready(&lv_disp_drv);
	k_sem_give(&drv_data->wait_sem);
}

static void _display_pm_notify_cb(const struct display_callback *callback, uint32_t pm_action)
{
	lvgl_disp_user_data_t *drv_data =
			CONTAINER_OF(callback, lvgl_disp_user_data_t, disp_cb);

	switch (pm_action) {
	case PM_DEVICE_ACTION_LATE_RESUME:
		drv_data->pm_state = LV_PORT_PM_STATE_ACTIVE;
		break;
	case PM_DEVICE_ACTION_LOW_POWER:
		 drv_data->pm_state = LV_PORT_PM_STATE_IDLE;
		 break;
	case PM_DEVICE_ACTION_EARLY_SUSPEND:
	default:
		drv_data->pm_state = LV_PORT_PM_STATE_OFF;
		while (atomic_get(&drv_data->flush_cnt) > 0) {
			k_msleep(2);
		}
		break;
	}

	if (drv_data->user_cb && drv_data->user_cb->pm_notify) {
		drv_data->user_cb->pm_notify(drv_data->user_cb, drv_data->pm_state);
	}
}

static void lvgl_flush_cb(lv_disp_drv_t *disp_drv,
		const lv_area_t *area, lv_color_t *color_p)
{
	lvgl_disp_user_data_t *drv_data = disp_drv->user_data;
	lvgl_disp_flush_data_t *flush_data = &drv_data->flush_data[drv_data->flush_data_wr];

	if (drv_data->pm_state == LV_PORT_PM_STATE_OFF) {
		lv_disp_flush_ready(disp_drv);
		goto out_exit;
	}

	if (disp_drv->sw_rotate && disp_drv->rotated != LV_DISP_ROT_NONE) {
		mem_dcache_clean(color_p, lv_area_get_size(area) * sizeof(lv_color_t));
		mem_dcache_sync();
	}

	/*Flush the memory dcache*/
	if (disp_drv->clean_dcache_cb) disp_drv->clean_dcache_cb(disp_drv);

	if (disp_drv->direct_mode) {
		if (drv_data->flush_idx == 0) {
			memcpy(&flush_data->area, area, sizeof(*area));
		} else {
			_lv_area_join(&flush_data->area, &flush_data->area, area);
		}

		if (lv_disp_flush_is_last(disp_drv)) {
			lvgl_rounder_cb(disp_drv, &flush_data->area);
		}
	} else {
		/*Flush the rendered content to the display*/
		lv_draw_wait_for_finish(disp_drv->draw_ctx);

		memcpy(&flush_data->area, area, sizeof(*area));
	}

	if (!disp_drv->direct_mode || lv_disp_flush_is_last(disp_drv)) {
		uint8_t flush_idx = disp_drv->direct_mode ? 0 : drv_data->flush_idx;

		flush_data->buf = color_p;
		flush_data->idx = flush_idx;

		if (++drv_data->flush_data_wr >= ARRAY_SIZE(drv_data->flush_data)) {
			drv_data->flush_data_wr = 0;
		}

		if (atomic_inc(&drv_data->flush_cnt) == 0 && flush_idx > 0) {
			_display_flush(drv_data);
		}
	}

out_exit:
	drv_data->flush_idx = lv_disp_flush_is_last(disp_drv) ? 0 : (drv_data->flush_idx + 1);
}

static void lvgl_wait_cb(lv_disp_drv_t *disp_drv)
{
	lvgl_disp_user_data_t *drv_data = disp_drv->user_data;

	if (k_sem_take(&drv_data->wait_sem, K_FOREVER)) {
		LV_LOG_ERROR("flush timeout");
	}
}

static void lvgl_rounder_cb(lv_disp_drv_t * disp_drv, lv_area_t * area)
{
	lvgl_disp_user_data_t *drv_data = disp_drv->user_data;
	struct display_capabilities *cap = &drv_data->disp_cap;

	/* Some LCD driver IC require even pixel alignment, so set even area if possible */

	if (disp_drv->rotated == LV_DISP_ROT_90 || disp_drv->rotated == LV_DISP_ROT_270) {
		if (cap->screen_info & SCREEN_INFO_X_ALIGNMENT_WIDTH) {
			area->y1 = 0;
			area->y2 = cap->x_resolution - 1;
		} else if (!(cap->x_resolution & 0x1)) {
			area->y1 &= ~0x1;
			area->y2 |= 0x1;
		}

		if (cap->screen_info & SCREEN_INFO_Y_ALIGNMENT_HEIGHT) {
			area->x1 = 0;
			area->x2 = cap->y_resolution - 1;
		} else if (!(cap->y_resolution & 0x1)) {
			area->x1 &= ~0x1;
			area->x2 |= 0x1;
		}
	} else {
		if (cap->screen_info & SCREEN_INFO_X_ALIGNMENT_WIDTH) {
			area->x1 = 0;
			area->x2 = cap->x_resolution - 1;
		} else if (!(cap->x_resolution & 0x1)) {
			area->x1 &= ~0x1;
			area->x2 |= 0x1;
		}

		if (cap->screen_info & SCREEN_INFO_Y_ALIGNMENT_HEIGHT) {
			area->y1 = 0;
			area->y2 = cap->y_resolution - 1;
		} else if (!(cap->y_resolution & 0x1)) {
			area->y1 &= ~0x1;
			area->y2 |= 0x1;
		}
	}
}

static void lvgl_set_rendering_cb(lv_disp_drv_t *disp_drv)
{
	lv_port_gpu_init(disp_drv);

	disp_drv->wait_cb = lvgl_wait_cb;
	disp_drv->flush_cb = lvgl_flush_cb;

	if (disp_drv->direct_mode == 0) {
		disp_drv->rounder_cb = lvgl_rounder_cb;
	}
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int lvgl_port_disp_init(void)
{
	lvgl_disp_user_data_t *drv_data = &disp_user_data;

	drv_data->disp_dev = device_get_binding(CONFIG_LCD_DISPLAY_DEV_NAME);
	if (drv_data->disp_dev == NULL) {
		LV_LOG_ERROR("Display device not found.");
		return -ENODEV;
	}

	drv_data->pm_state = LV_PORT_PM_STATE_ACTIVE;
	drv_data->flush_idx = 0;
	drv_data->flushing = false;
	drv_data->flush_data_wr = 0;
	drv_data->flush_data_rd = 0;
	atomic_set(&drv_data->flush_cnt, 0);

	k_sem_init(&drv_data->wait_sem, 0, 1);
	k_sem_init(&drv_data->ready_sem, 0, 1);

	drv_data->disp_cb.vsync = _display_vsync_cb;
	drv_data->disp_cb.complete = _display_complete_cb;
	drv_data->disp_cb.pm_notify = _display_pm_notify_cb,
	display_register_callback(drv_data->disp_dev, &drv_data->disp_cb);
	//display_blanking_off(drv_data->disp_dev);

	lv_disp_drv_init(&lv_disp_drv);
	lv_disp_drv.user_data = (void *)drv_data;

	_set_rendering_buffers(&lv_disp_drv);
	lvgl_set_rendering_cb(&lv_disp_drv);

	if (lv_disp_drv_register(&lv_disp_drv) == NULL) {
		LV_LOG_ERROR("Failed to register display device.");
		return -EPERM;
	}

	/* wait display ready */
	k_sem_take(&drv_data->ready_sem, K_FOREVER);
	return 0;
}

lv_res_t lv_port_disp_register_callback(const lv_port_disp_callback_t *callback)
{
	lv_disp_t * disp = lv_disp_get_default();
	lvgl_disp_user_data_t *drv_data = disp->driver->user_data;

	if (drv_data) {
		drv_data->user_cb = callback;
	}

	return LV_RES_OK;
}

lv_res_t lv_port_disp_flush_wait(void)
{
	lv_disp_t * disp = lv_disp_get_default();

	while (lv_disp_flush_is_finished(disp->driver) == false) {
		lvgl_wait_cb(disp->driver);
	}

	return LV_RES_OK;
}

lv_res_t lv_port_disp_read_to_buf(void *buf, uint32_t buf_size)
{
	return LV_RES_INV;
}

void * lv_port_disp_ovelay_prepare(void)
{
	return NULL;
}

lv_res_t lv_port_disp_ovelay_unprepare(void)
{
	return LV_RES_INV;
}

lv_res_t lv_port_disp_overlay_flush(const lv_color_t *bg_col, const lv_area_t *bg_area,
                                    const lv_color_t *fg_col, const lv_area_t *fg_area,
                                    lv_opa_t fg_opa)
{
	return LV_RES_INV;
}

lv_res_t lv_port_disp_flush_screen(void)
{
	return LV_RES_INV;
}

#endif /* !defined(CONFIG_UI_SERVICE) && !defined(CONFIG_DISPLAY_COMPOSER) */
