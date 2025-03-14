/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**********************
 *      INCLUDES
 **********************/
#include <os_common_api.h>
#include <memory/mem_cache.h>
#include <display/display_hal.h>
#include <ui_mem.h>
#include <lvgl/lvgl.h>
#include <lvgl/porting/lvgl_porting.h>
#include <lvgl/lvgl_memory.h>
#include <lvgl/lvgl_virtual_display.h>

#if defined(CONFIG_TRACING) && defined(CONFIG_UI_SERVICE)
#  include <tracing/tracing.h>
#  include <view_manager.h>
#endif

/**********************
 *      DEFINES
 **********************/
#ifdef _WIN32
#  define USE_GPU_WAIT_ASYNC  0
#else
#  define USE_GPU_WAIT_ASYNC  1
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef struct lvgl_disp_drv_data {
	surface_t *surface;
#if CONFIG_LV_VDB_NUM == 0
	os_sem wait_sem;
#endif

	uint32_t flush_pixel_format;
	ui_region_t flush_area;
	uint8_t flush_idx;
	uint8_t flush_flag;

#if defined(CONFIG_TRACING) && defined(CONFIG_UI_SERVICE)
	uint32_t frame_cnt;
#endif

	lv_disp_drv_t *disp_drv;
} lvgl_disp_drv_data_t;

typedef struct lvgl_async_flush_ctx {
	bool initialized;
	bool enabled;
	bool flushing;
	lv_color_t * flush_buf;
	os_work flush_work;
	os_sem flush_sem;

	lvgl_disp_drv_data_t *disp_data;
} lvgl_async_flush_ctx_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
#if CONFIG_LV_VDB_NUM > 0
static void _lvgl_draw_buf_init_shared_vdb(lv_disp_draw_buf_t * drawbuf);
#endif

static int _lvgl_draw_buf_alloc(lv_disp_drv_t * disp_drv, surface_t * surface);

static void _lvgl_render_start_cb(lv_disp_drv_t * disp_drv);
static void _lvgl_render_area_start_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area);
static void _lvgl_flush_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
static void _lvgl_rounder_cb(lv_disp_drv_t * disp_drv, lv_area_t * area);
static void _lvgl_wait_cb(lv_disp_drv_t * disp_drv);
static void _lvgl_surface_draw_cb(uint32_t event, void * data, void * user_data);

#if USE_GPU_WAIT_ASYNC
static void _lvgl_init_async_flush_ctx(void);
static void _lvgl_flush_async_cb(void * work);
static void _lvgl_flush_async_lowerhalf_cb(os_work * work);
#endif

static uint8_t _lvgl_rotate_flag_from_surface(uint16_t rotation);

#ifdef CONFIG_SURFACE_TRANSFORM_UPDATE
static uint8_t _lvgl_rotate_flag_to_surface(lv_disp_drv_t *disp_drv);
static void _lvgl_rotate_area_to_surface(lv_disp_drv_t *disp_drv,
				ui_region_t *region, const lv_area_t *area);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/

#if CONFIG_LV_VDB_NUM > 0

#if CONFIG_LV_VDB_NUM >= 4
#  define LV_VDB_NUM 4
#elif CONFIG_LV_VDB_NUM >= 2
#  define LV_VDB_NUM 2
#else
#  define LV_VDB_NUM 1
#endif

/* NOTE:
 * (1) depending on chosen color depth buffer may be accessed using uint8_t *,
 * uint16_t * or uint32_t *, therefore buffer needs to be aligned accordingly to
 * prevent unaligned memory accesses.
 * (2) must align each buffer address and size to psram cache line size (32 bytes)
 * if allocated in psram.
 * (3) Verisilicon vg_lite buffer memory requires 64 bytes aligned
 */
#ifdef CONFIG_VG_LITE
#  define BUFFER_ALIGN 64
#else
#  define BUFFER_ALIGN 32
#endif

#ifndef CONFIG_UI_MEM_VDB_SHARE_SURFACE_BUFFER
#if CONFIG_LV_VDB_SIZE <= 0
#  error CONFIG_LV_VDB_SIZE must greater than  0
#endif

#if LV_COLOR_SCREEN_TRANSP
#  define BUFFER_SIZE (((CONFIG_LV_VDB_SIZE * LV_IMG_PX_SIZE_ALPHA_BYTE) + (BUFFER_ALIGN - 1)) & ~(BUFFER_ALIGN - 1))
#  define NBR_PIXELS_IN_BUFFER (BUFFER_SIZE  / LV_IMG_PX_SIZE_ALPHA_BYTE)
#else
#  define BUFFER_SIZE (((CONFIG_LV_VDB_SIZE * LV_COLOR_SIZE / 8) + (BUFFER_ALIGN - 1)) & ~(BUFFER_ALIGN - 1))
#  define NBR_PIXELS_IN_BUFFER (BUFFER_SIZE * 8 / LV_COLOR_SIZE)
#endif

static uint8_t vdb_buf_0[BUFFER_SIZE] __aligned(BUFFER_ALIGN) __in_section_unique(lvgl.noinit.vdb.0);
#if LV_VDB_NUM >= 2
static uint8_t vdb_buf_1[BUFFER_SIZE] __aligned(BUFFER_ALIGN) __in_section_unique(lvgl.noinit.vdb.1);
#endif
#if LV_VDB_NUM >= 4
static uint8_t vdb_buf_2[BUFFER_SIZE] __aligned(BUFFER_ALIGN) __in_section_unique(lvgl.noinit.vdb.2);
static uint8_t vdb_buf_3[BUFFER_SIZE] __aligned(BUFFER_ALIGN) __in_section_unique(lvgl.noinit.vdb.3);
#endif
#endif /* CONFIG_UI_MEM_VDB_SHARE_SURFACE_BUFFER */

static lv_disp_draw_buf_t g_disp_drawbuf;
static os_sem g_drawbuf_wait_sem;

#endif /* CONFIG_LV_VDB_NUM > 0*/

/* active display with indev attached to */
static lv_disp_t * g_act_disp = NULL;

#if USE_GPU_WAIT_ASYNC
static lvgl_async_flush_ctx_t g_async_ctx;
#endif

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
lv_disp_t * lvgl_virtual_display_create(surface_t * surface)
{
	lv_disp_t *disp = NULL;
	lv_disp_drv_t *disp_drv = NULL;
	lvgl_disp_drv_data_t *drv_data = NULL;

	disp_drv = lv_mem_alloc(sizeof(*disp_drv));
	if (disp_drv == NULL) {
		LV_LOG_ERROR("Failed to alloc driver");
		return NULL;
	}

	lv_disp_drv_init(disp_drv);
	lv_port_gpu_init(disp_drv);

	disp_drv->screen_transp = hal_pixel_format_is_opaque(surface->pixel_format) ? 0 : 1;
#if LV_COLOR_SCREEN_TRANSP == 0
	if (disp_drv->screen_transp) {
		LV_LOG_ERROR("Must enable LV_COLOR_SCREEN_TRANSP to support screen transp");
		goto fail_free_drv;
	}
#endif

	drv_data = lv_mem_alloc(sizeof(*drv_data));
	if (drv_data == NULL) {
		LV_LOG_ERROR("Failed to allocate dirver data");
		goto fail_free_drv;
	}

	memset(drv_data, 0, sizeof(*drv_data));
#if CONFIG_LV_VDB_NUM == 0
	os_sem_init(&drv_data->wait_sem, 0, 1);
#endif
	drv_data->surface = surface;
	drv_data->disp_drv = disp_drv;
	drv_data->flush_pixel_format = lvgl_img_cf_to_display(
			disp_drv->screen_transp ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR, NULL);

	disp_drv->user_data = drv_data;

	if (_lvgl_draw_buf_alloc(disp_drv, surface)) {
		goto fail_free_drv_data;
	}

	disp_drv->hor_res = surface_get_width(surface);
	disp_drv->ver_res = surface_get_height(surface);
	disp_drv->rotated = _lvgl_rotate_flag_from_surface(surface_get_orientation(surface));
#ifndef CONFIG_SURFACE_TRANSFORM_UPDATE
	disp_drv->sw_rotate = (disp_drv->rotated == LV_DISP_ROT_NONE) ? 0 : 1;
#endif

	disp_drv->flush_cb = _lvgl_flush_cb;
	disp_drv->wait_cb = _lvgl_wait_cb;
	disp_drv->render_start_cb = _lvgl_render_start_cb;
	disp_drv->render_area_start_cb = _lvgl_render_area_start_cb;

	if (surface_get_max_possible_buffer_count() == 0) {
		disp_drv->rounder_cb = _lvgl_rounder_cb;
	}

	disp_drv->draw_ctx = lv_mem_alloc(disp_drv->draw_ctx_size);
	if (disp_drv->draw_ctx == NULL) {
		LV_LOG_ERROR("Failed to allocate dirver ctx");
		goto fail_free_draw_buf;
	}

	disp_drv->draw_ctx_init(disp_drv, disp_drv->draw_ctx);

	disp = lv_disp_drv_register(disp_drv);
	if (disp == NULL) {
		LV_LOG_ERROR("Failed to register driver.");
		goto fail_free_draw_ctx;
	}

	lv_disp_set_bg_color(disp, lv_color_black());
	lv_disp_set_bg_opa(disp, disp_drv->screen_transp ? LV_OPA_TRANSP : LV_OPA_COVER);

	/* Skip drawing the empty frame */
	lv_timer_pause(disp->refr_timer);

	surface_register_callback(surface, SURFACE_CB_DRAW, _lvgl_surface_draw_cb, disp);
	surface_set_continuous_draw_count(surface, disp_drv->draw_buf->buf_cnt);

#if USE_GPU_WAIT_ASYNC
	_lvgl_init_async_flush_ctx();
#endif /*USE_GPU_WAIT_ASYNC*/

	LV_LOG_INFO("disp %p created\n", disp);
	return disp;

fail_free_draw_ctx:
	if (disp_drv->draw_ctx_deinit) {
		disp_drv->draw_ctx_deinit(disp_drv, disp_drv->draw_ctx);
	}

	lv_mem_free(disp_drv->draw_ctx);
fail_free_draw_buf:
#if CONFIG_LV_VDB_NUM == 0
	lv_mem_free(disp_drv->draw_buf);
#endif
fail_free_drv_data:
	lv_mem_free(disp_drv->user_data);
fail_free_drv:
	lv_mem_free(disp_drv);
	return NULL;
}

void lvgl_virtual_display_destroy(lv_disp_t * disp)
{
	lv_disp_drv_t *disp_drv = disp->driver;
	lvgl_disp_drv_data_t *drv_data = disp_drv->user_data;

	while (lv_disp_flush_is_finished(disp_drv) == false) {
		_lvgl_wait_cb(disp_drv);
	}

	/* unregister surface callback */
	surface_register_callback(drv_data->surface, SURFACE_CB_DRAW, NULL, NULL);

	if (g_act_disp == disp) {
		lvgl_virtual_display_set_focus(NULL, true);
	}

	lv_disp_remove(disp);

	if (disp_drv->draw_ctx_deinit) {
		disp_drv->draw_ctx_deinit(disp_drv, disp_drv->draw_ctx);
	}

#if CONFIG_LV_VDB_NUM == 0
	lv_mem_free(disp_drv->draw_buf);
#endif
	lv_mem_free(disp_drv->draw_ctx);
	lv_mem_free(disp_drv->user_data);
	lv_mem_free(disp_drv);

	LV_LOG_INFO("disp %p destroyed\n", disp);
}

int lvgl_virtual_display_set_default(lv_disp_t * disp)
{
	/* Set default display */
	lv_disp_set_default(disp);
	return 0;
}

int lvgl_virtual_display_set_focus(lv_disp_t * disp, bool reset_indev)
{
	if (disp == g_act_disp) {
		return 0;
	}

	if (disp == NULL) {
		LV_LOG_INFO("no active display\n");
	}

	/* Retach the input devices */
	lv_indev_t *indev = lv_indev_get_next(NULL);
	while (indev) {
		 /* Reset the indev when focus changed */
		if (reset_indev) {
			lv_indev_reset(indev, NULL);
		} else {
			lv_indev_wait_release(indev);
		}

		indev->driver->disp = disp;
		indev = lv_indev_get_next(indev);

		LV_LOG_INFO("indev %p attached to disp %p\n", indev, disp);
	}

	/* Set default display */
	lv_disp_set_default(disp);

	g_act_disp = disp;
	return 0;
}

int lvgl_virtual_display_update(lv_disp_t * disp, uint16_t rotation)
{
	lv_disp_drv_t * disp_drv = disp->driver;

	disp_drv->rotated = _lvgl_rotate_flag_from_surface(rotation);
#ifndef CONFIG_SURFACE_TRANSFORM_UPDATE
	disp_drv->sw_rotate = (disp_drv->rotated == LV_DISP_ROT_NONE) ? 0 : 1;
#endif

	lv_disp_drv_update(disp, disp_drv);
	return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#if CONFIG_LV_VDB_NUM > 0
static void _lvgl_draw_buf_init_shared_vdb(lv_disp_draw_buf_t * drawbuf)
{
#ifdef CONFIG_UI_MEM_VDB_SHARE_SURFACE_BUFFER
	uint8_t *bufs[LV_VDB_NUM];
	unsigned int total_size = ui_mem_get_share_surface_buffer_size();
	unsigned int max_buf_size = ((total_size / LV_VDB_NUM) & ~(BUFFER_ALIGN - 1));
	uint8_t px_size = LV_COLOR_SCREEN_TRANSP ? LV_IMG_PX_SIZE_ALPHA_BYTE : (LV_COLOR_SIZE / 8);
	unsigned int max_nbr_pixels = (max_buf_size / px_size);
	unsigned int buf_size;
	unsigned int nbr_pixels;
	int i;

	if (CONFIG_LV_VDB_SIZE > 0 && CONFIG_LV_VDB_SIZE < max_nbr_pixels) {
		nbr_pixels = CONFIG_LV_VDB_SIZE;
		buf_size = (nbr_pixels * px_size + BUFFER_ALIGN - 1) & ~(BUFFER_ALIGN - 1);
	} else {
		nbr_pixels = max_nbr_pixels; /* auto compute LV_VDB_SIZE if 0 */
		buf_size = max_buf_size;
	}

	bufs[0] = ui_mem_get_share_surface_buffer();
	for (i = 1; i < LV_VDB_NUM; i++) {
		bufs[i] = bufs[i - 1] + buf_size;
	}

#else /* CONFIG_UI_MEM_VDB_SHARE_SURFACE_BUFFER */
	uint8_t *bufs[LV_VDB_NUM] = {
		vdb_buf_0,
#if LV_VDB_NUM >= 2
		vdb_buf_1,
#endif
#if LV_VDB_NUM >= 4
		vdb_buf_2,
		vdb_buf_3,
#endif
	};

	unsigned int nbr_pixels = NBR_PIXELS_IN_BUFFER;
#endif /* CONFIG_UI_MEM_VDB_SHARE_SURFACE_BUFFER */

	LV_LOG_INFO("LVGL VDB: size %u, num %u\n", nbr_pixels, LV_VDB_NUM);

	lv_disp_draw_buf_init2(drawbuf, (void **)bufs, LV_VDB_NUM, nbr_pixels);
}
#endif /* CONFIG_LV_VDB_NUM > 0 */

static int _lvgl_draw_buf_alloc(lv_disp_drv_t * disp_drv, surface_t * surface)
{
#if CONFIG_LV_VDB_NUM > 0
	if (g_disp_drawbuf.buf_cnt == 0) {
		_lvgl_draw_buf_init_shared_vdb(&g_disp_drawbuf);
	}

	disp_drv->draw_buf = &g_disp_drawbuf;
	os_sem_init(&g_drawbuf_wait_sem, 0, 1);
	return 0;

#else
	lvgl_disp_drv_data_t *drv_data = disp_drv->user_data;

	if (surface_get_max_possible_buffer_count() == 0) {
		LV_LOG_ERROR("no vdb, must increase CONFIG_SURFACE_MAX_BUFFER_COUNT to use direct mode");
		return -EINVAL;
	}

	if (surface->pixel_format != drv_data->flush_pixel_format) {
		LV_LOG_ERROR("pixel format %x must match in direct mode", surface->pixel_format);
		return -EINVAL;
	}

	disp_drv->draw_buf = lv_mem_alloc(sizeof(lv_disp_draw_buf_t));
	if (disp_drv->draw_buf == NULL) {
		LV_LOG_ERROR("draw buf alloc failed");
		return -ENOMEM;
	}

	LV_LOG_INFO("no vdb, use direct mode");

	graphic_buffer_t *buf1 = surface_get_draw_buffer(surface);
	graphic_buffer_t *buf2 = surface_get_post_buffer(surface);
	LV_ASSERT(buf1 != NULL);

	lv_disp_draw_buf_init(disp_drv->draw_buf,
			buf1->data, (buf2 == NULL || buf2 == buf1) ? NULL : buf2->data,
			disp_drv->hor_res * disp_drv->ver_res);
	disp_drv->direct_mode = 1;

	return 0;
#endif /* CONFIG_LV_VDB_NUM > 0*/
}

static void _lvgl_render_start_cb(lv_disp_drv_t * disp_drv)
{
	lvgl_disp_drv_data_t *drv_data = disp_drv->user_data;

	surface_begin_frame(drv_data->surface);

	if (disp_drv->direct_mode) {
		graphic_buffer_t *drawbuf = NULL;
		surface_begin_draw(drv_data->surface, SURFACE_FIRST_DRAW | SURFACE_LAST_DRAW, &drawbuf);

		LV_ASSERT(drawbuf != NULL && drawbuf->data == disp_drv->draw_buf->buf_act);
	}
}

static void _lvgl_render_area_start_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area)
{
	lvgl_disp_drv_data_t *drv_data = disp_drv->user_data;

#if defined(CONFIG_TRACING) && defined(CONFIG_UI_SERVICE)
	ui_view_context_t *view = drv_data->surface->user_data[SURFACE_CB_POST];

	os_strace_u32x7(SYS_TRACE_ID_VIEW_DRAW, view->entry->id, drv_data->frame_cnt,
			drv_data->flush_idx, area->x1, area->y1, area->x2, area->y2);
#endif /* CONFIG_TRACING */

	if (disp_drv->direct_mode) {
		if (drv_data->flush_idx == 0) {
			drv_data->flush_area.x1 = area->x1;
			drv_data->flush_area.y1 = area->y1;
			drv_data->flush_area.x2 = area->x2;
			drv_data->flush_area.y2 = area->y2;
		} else {
			drv_data->flush_area.x1 = LV_MIN(drv_data->flush_area.x1, area->x1);
			drv_data->flush_area.y1 = LV_MIN(drv_data->flush_area.y1, area->y1);
			drv_data->flush_area.x2 = LV_MAX(drv_data->flush_area.x2, area->x2);
			drv_data->flush_area.y2 = LV_MAX(drv_data->flush_area.y2, area->y2);
		}
	}
}

static void _lvgl_flush_cb(lv_disp_drv_t * disp_drv,
		const lv_area_t * area, lv_color_t * color_p)
{
	lvgl_disp_drv_data_t *drv_data = disp_drv->user_data;
	int res = 0;

	if (disp_drv->sw_rotate) {
		mem_dcache_clean(color_p, lv_area_get_size(area) * sizeof(lv_color_t));
		mem_dcache_sync();
	}

	/*Flush the memory dcache*/
	if (disp_drv->clean_dcache_cb)
		disp_drv->clean_dcache_cb(disp_drv);

	if (!disp_drv->direct_mode) {
#if USE_GPU_WAIT_ASYNC
		if (g_async_ctx.enabled) {
			while (g_async_ctx.flushing)
				os_sem_take(&g_async_ctx.flush_sem, OS_FOREVER);
		} else
#endif
		{
			/*Flush the rendered content to the display*/
			lv_draw_wait_for_finish(disp_drv->draw_ctx);
		}
	}

#ifdef CONFIG_SURFACE_TRANSFORM_UPDATE
	drv_data->flush_flag = _lvgl_rotate_flag_to_surface(disp_drv);
	if (drv_data->flush_idx == 0)
		drv_data->flush_flag |= SURFACE_FIRST_DRAW;
#else
	drv_data->flush_flag = (drv_data->flush_idx == 0) ? SURFACE_FIRST_DRAW : 0;
#endif

	if (lv_disp_flush_is_last(disp_drv)) {
		drv_data->flush_flag |= SURFACE_LAST_DRAW;
		drv_data->flush_idx = 0;
#if defined(CONFIG_TRACING) && defined(CONFIG_UI_SERVICE)
		drv_data->frame_cnt++;
#endif
	} else {
		drv_data->flush_idx++;
	}

	LV_LOG_TRACE("lvgl flush: flag %x, buf %p, area (%d %d %d %d), render %u, flush %u\n",
			drv_data->flush_flag, color_p, area->x1, area->y1, area->x2, area->y2,
			disp_drv->draw_buf->rendering_idx, disp_drv->draw_buf->flushing_idx);

	/* FIXME:
	 * wait_for_finish() is already called in LVGL for the last flush for compability.
	 */
	if (disp_drv->direct_mode) {
		if (drv_data->flush_flag & SURFACE_LAST_DRAW) {
			res = surface_end_draw(drv_data->surface, &drv_data->flush_area);
			surface_end_frame(drv_data->surface);
			if (res) {
				LV_LOG_ERROR("surface update failed");
				lv_disp_flush_ready(disp_drv);
			}
		}
	} else {
#ifdef CONFIG_SURFACE_TRANSFORM_UPDATE
		_lvgl_rotate_area_to_surface(disp_drv, &drv_data->flush_area, area);
#else
		ui_region_set(&drv_data->flush_area, area->x1, area->y1, area->x2, area->y2);
#endif

#if USE_GPU_WAIT_ASYNC
		if (g_async_ctx.enabled) {
			g_async_ctx.flushing = true;
			g_async_ctx.flush_buf = color_p;
			g_async_ctx.disp_data = drv_data;
			lv_port_gpu_insert_event(_lvgl_flush_async_cb, &g_async_ctx.flush_work);
		} else
#endif
		{
			res = surface_update(drv_data->surface, drv_data->flush_flag,
						&drv_data->flush_area, color_p, lv_area_get_width(area),
						drv_data->flush_pixel_format);
			if (drv_data->flush_flag & SURFACE_LAST_DRAW) {
				surface_end_frame(drv_data->surface);
			}
		}

		if (res) {
			LV_LOG_ERROR("surface update failed");
			lv_disp_flush_ready(disp_drv);
		}
	}
}

#if USE_GPU_WAIT_ASYNC
static void _lvgl_init_async_flush_ctx(void)
{
	if (g_async_ctx.initialized == false) {
		g_async_ctx.initialized = true;
		g_async_ctx.enabled = (lvgl_display_get_flush_workq() != NULL);
		os_sem_init(&g_async_ctx.flush_sem, 0, 1);
		os_work_init(&g_async_ctx.flush_work, _lvgl_flush_async_lowerhalf_cb);
	}
}

static void  _lvgl_flush_async_cb(void * work)
{
	os_work_submit_to_queue(lvgl_display_get_flush_workq(), work);
}

static void  _lvgl_flush_async_lowerhalf_cb(os_work * work)
{
	lvgl_disp_drv_data_t *drv_data = g_async_ctx.disp_data;
#ifdef CONFIG_SURFACE_TRANSFORM_UPDATE
	uint16_t stride = (drv_data->flush_flag & SURFACE_ROTATED_90) ?
				ui_region_get_height(&drv_data->flush_area) :
				ui_region_get_width(&drv_data->flush_area);
#else
	uint16_t stride = ui_region_get_width(&drv_data->flush_area);
#endif

	LV_LOG_TRACE("async flush (%d): flag %x, buf %p, stride %u, area (%d %d %d %d)\n",
			res, drv_data->flush_flag, g_async_ctx.flush_buf, stride,
			drv_data->flush_area.x1, drv_data->flush_area.y1,
			drv_data->flush_area.x2, drv_data->flush_area.y2);

	int res = surface_update(drv_data->surface, drv_data->flush_flag, &drv_data->flush_area,
				g_async_ctx.flush_buf, stride, drv_data->flush_pixel_format);
	if (res < 0)
		lv_disp_flush_ready(drv_data->disp_drv);

	if (drv_data->flush_flag & SURFACE_LAST_DRAW) {
		surface_end_frame(drv_data->surface);
	}

	g_async_ctx.flushing = false;
	os_sem_give(&g_async_ctx.flush_sem);
}
#endif /* USE_GPU_WAIT_ASYNC */

static uint8_t _lvgl_rotate_flag_from_surface(uint16_t rotation)
{
	LV_ASSERT(rotation <= 270);

	switch (rotation) {
	case 90:
		return LV_DISP_ROT_270;
	case 180:
		return LV_DISP_ROT_180;
	case 270:
		return LV_DISP_ROT_90;
	default:
		return LV_DISP_ROT_NONE;
	}
}

#ifdef CONFIG_SURFACE_TRANSFORM_UPDATE
static uint8_t _lvgl_rotate_flag_to_surface(lv_disp_drv_t * disp_drv)
{
	static const uint8_t flags[] = {
		0, SURFACE_ROTATED_270, SURFACE_ROTATED_180, SURFACE_ROTATED_90,
	};

	return flags[disp_drv->rotated];
}

static void _lvgl_rotate_area_to_surface(lv_disp_drv_t * disp_drv,
				ui_region_t * region, const lv_area_t * area)
{
	switch (disp_drv->rotated) {
	case LV_DISP_ROT_90: /* 270 degree clockwise rotation */
		region->x1 = area->y1;
		region->y1 = disp_drv->hor_res - area->x2 - 1;
		region->x2 = area->y2;
		region->y2 = disp_drv->hor_res - area->x1 - 1;
		break;
	case LV_DISP_ROT_180: /* 180 degree clockwise rotation */
		region->x1 = disp_drv->hor_res - area->x2 - 1;
		region->y1 = disp_drv->ver_res - area->y2 - 1;
		region->x2 = disp_drv->hor_res - area->x1 - 1;
		region->y2 = disp_drv->ver_res - area->y1 - 1;
		break;
	case LV_DISP_ROT_270: /* 90 degree clockwise rotation */
		region->x1 = disp_drv->ver_res - area->y2 - 1;
		region->y1 = area->x1;
		region->x2 = disp_drv->ver_res - area->y1 - 1;
		region->y2 = area->x2;
		break;
	case LV_DISP_ROT_NONE:
	default:
		ui_region_set(region, area->x1, area->y1, area->x2, area->y2);
		break;
	}
}
#endif /* CONFIG_SURFACE_TRANSFORM_UPDATE */

static void _lvgl_rounder_cb(lv_disp_drv_t * disp_drv, lv_area_t * area)
{
	/*
	 * (1) Some LCD DDIC require even pixel alignment, so set even area if possible
	 * for framebuffer-less refresh mode.
	 * (2) Haraware may run faster on aligned pixels, so set horizontal even if possible
	 */
	if (!(disp_drv->hor_res & 0x1)) {
		area->x1 &= ~0x1;
		area->x2 |= 0x1;
	}

	/* Framebuffer-less refresh mode must meet LCD DDIC pixel algnment */
	if (!(disp_drv->ver_res & 0x1)) {
		area->y1 &= ~0x1;
		area->y2 |= 0x1;
	}
}

static void _lvgl_wait_cb(lv_disp_drv_t * disp_drv)
{
	LV_LOG_TRACE("lvgl wait: render %u, flush %u\n",
			disp_drv->draw_buf->rendering_idx, disp_drv->draw_buf->flushing_idx);

#if CONFIG_LV_VDB_NUM == 0
	lvgl_disp_drv_data_t *drv_data = disp_drv->user_data;
	os_sem_take(&drv_data->wait_sem, OS_FOREVER);
#else
	os_sem_take(&g_drawbuf_wait_sem, OS_FOREVER);
#endif
}

static void _lvgl_surface_draw_cb(uint32_t event, void * data, void * user_data)
{
	lv_disp_t *disp = user_data;
	lv_disp_drv_t *disp_drv = disp->driver;

	if (event == SURFACE_EVT_DRAW_READY) {
		lv_disp_flush_ready(disp_drv);
		LV_LOG_TRACE("lvgl ready: render %u, flush %u\n",
				disp_drv->draw_buf->rendering_idx, disp_drv->draw_buf->flushing_idx);

#if CONFIG_LV_VDB_NUM == 0
		lvgl_disp_drv_data_t *drv_data = disp_drv->user_data;
		os_sem_give(&drv_data->wait_sem);
#else
		os_sem_give(&g_drawbuf_wait_sem);
#endif
	} else if (event == SURFACE_EVT_DRAW_COVER_CHECK) {
		surface_cover_check_data_t *cover_check = data;

		/* direct mode: buffer copy will be done in lv_refr.c */
		if (disp_drv->direct_mode || (
			disp->inv_areas[0].x1 <= cover_check->area->x1 &&
			disp->inv_areas[0].y1 <= cover_check->area->y1 &&
			disp->inv_areas[0].x2 >= cover_check->area->x2 &&
			disp->inv_areas[0].y2 >= cover_check->area->y2)) {
			cover_check->covered = true;
		} else {
			cover_check->covered = false;
		}
	}
}
