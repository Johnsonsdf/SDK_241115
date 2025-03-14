/**
 * @file lv_gpu_acts.c
 *
 */

/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include "../lvgl_porting.h"
#include "lv_gpu_acts.h"

#if LV_USE_GPU_ACTS

#include <os_common_api.h>

#if LV_USE_GPU_ACTS_VG_LITE
#  include "vglite/lv_draw_vglite.h"
#endif

#if defined(CONFIG_LVGL_USE_IMG_DECODER_ACTS)
#  include <lvgl/lvgl_img_decoder.h>
#endif

/*********************
 *      DEFINES
 *********************/
/*
 * It is a risk to not wait_for_finish() for decoded raw image
 */
#define USE_NO_WAIT_FOR_RAW_IMG 1

#if LV_USE_GPU_ACTS_VG_LITE
#  define LAYER_MEM_ALIGN_SIZE 64U
#else
#  define LAYER_MEM_ALIGN_SIZE 4U
#endif

#define LAYER_MEM_ALIGN(x) (((uintptr_t)(x) + LAYER_MEM_ALIGN_SIZE - 1) & ~(LAYER_MEM_ALIGN_SIZE - 1))

/**********************
 *      TYPEDEFS
 **********************/
typedef struct lvgl_dcache_mgr {
    uint8_t *buf_start;
    uint8_t *buf_end;
} lvgl_dcache_mgr_t;

typedef struct lvgl_gpu_draw_ctx {
    uint8_t accl_type;
    uint8_t accl_supported;
    uint8_t accl_img_decoded : 1; /* retry accelerating decoded image */

    /* current draw buffer property */
    uint8_t buf_cacheable : 1;
    uint8_t buf_pxbytes;
    lv_area_t accl_areas[ACCL_NUM_HW_TYPES];

    /* inv cache range */
    lvgl_dcache_mgr_t inv_dcache;

    /* draw timeline */
    uint16_t timeline_cnt; /* submit counter */
    uint16_t timeline_val; /* complete counter */
    os_sem timeline_sem;

    /* event synchronization */
    lv_port_gpu_event_cb_t event_cb;
    void * event_data;
    uint16_t event_val;
} lvgl_gpu_draw_ctx_t;

/**********************
 *  GLOBAL PROTOTYPES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int _lv_gpu_init(void);
static void _lv_gpu_clear_cb(lv_disp_drv_t * disp_drv, uint8_t * buf, uint32_t size);
static void _lv_gpu_clean_dcache_cb(lv_disp_drv_t * disp_drv);

static void _lv_gpu_reset_accl_area(lv_area_t * area);

static void _lv_gpu_invalidate_dcache(void);
static void _lv_gpu_cpu_wait_for_finish(lv_draw_ctx_t * draw_ctx);

static void _lv_draw_gpu_wait_for_finish(lv_draw_ctx_t * draw_ctx);
static void _lv_draw_gpu_init_buf(lv_draw_ctx_t * draw_ctx);
static void _lv_draw_gpu_buffer_copy(
        lv_draw_ctx_t * draw_ctx, void * dest_buf, lv_coord_t dest_stride,
        const lv_area_t * dest_area, void * src_buf, lv_coord_t src_stride,
        const lv_area_t * src_area);
#if defined(CONFIG_LVGL_USE_IMG_DECODER_ACTS)
static lv_res_t _lv_draw_gpu_img_encoded(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src);
#endif
static lv_res_t _lv_draw_gpu_img_dsc_decoded(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src_dsc);
static lv_res_t _lv_draw_gpu_img(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const void * src);
static void _lv_draw_gpu_img_decoded(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const uint8_t * src_buf, lv_img_cf_t cf);
static lv_res_t _lv_draw_gpu_letter_internal(
        lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,
        const lv_point_t * pos_p, uint32_t letter);
static void _lv_draw_gpu_letter(
        lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,
        const lv_point_t * pos_p, uint32_t letter);
static void _lv_draw_gpu_blend(
        lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc);

static bool _is_alpha_format(lv_img_cf_t cf);

#if LV_USE_GPU_ACTS_DMA2D || LV_USE_GPU_ACTS_VG_LITE
static struct _lv_draw_layer_ctx_t * _lv_draw_gpu_layer_init(
        lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx,
        lv_draw_layer_flags_t flags);
static void _lv_draw_gpu_layer_adjust(lv_draw_ctx_t * draw_ctx,
        lv_draw_layer_ctx_t * layer_ctx, lv_draw_layer_flags_t flags);
static void _lv_draw_gpu_layer_blend(lv_draw_ctx_t * draw_ctx,
        lv_draw_layer_ctx_t * layer_ctx, const lv_draw_img_dsc_t * draw_dsc);
static void _lv_draw_gpu_layer_destroy(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
static bool gpu_ctx_is_inited = false;
static lvgl_gpu_draw_ctx_t gpu_ctx __in_section_unique(lvgl.noinit.gpu);

/**********************
 *      MACROS
 **********************/
#define GET_GPU_CTX() &gpu_ctx

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_res_t lv_port_gpu_init(lv_disp_drv_t * drv)
{
    _lv_gpu_init();

    drv->clear_cb = _lv_gpu_clear_cb;
    drv->clean_dcache_cb = _lv_gpu_clean_dcache_cb;

    drv->draw_ctx_size = sizeof(lv_draw_gpu_acts_ctx_t);
    drv->draw_ctx_init = lv_draw_gpu_acts_ctx_init;
    drv->draw_ctx_deinit = lv_draw_gpu_acts_ctx_deinit;

    return LV_RES_OK;
}

int lv_port_gpu_insert_event(lv_port_gpu_event_cb_t event_cb, void * user_data)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();

    LV_UNUSED(ctx);

#if LV_USE_GPU_ACTS_JPG
    lv_gpu_acts_jpg_wait_for_finish(NULL);
    _lv_gpu_reset_accl_area(&ctx->accl_areas[ACCL_JPG]);
#endif

#if LV_USE_GPU_ACTS_VG_LITE
    lv_draw_vglite_wait_for_finish(NULL);
    _lv_gpu_reset_accl_area(&ctx->accl_areas[ACCL_VGLITE]);
#endif

#if LV_USE_GPU_ACTS_DMA2D
    bool completed;
    uint16_t ret_val;

    lv_port_gpu_wait_event(ctx->event_val);

    unsigned int key = os_irq_lock();

    ret_val = ctx->timeline_cnt;
    completed = (ctx->timeline_val == ctx->timeline_cnt);

    if (completed == false) {
        ctx->event_cb = event_cb;
        ctx->event_data = user_data;
        ctx->event_val = ctx->timeline_cnt;
    }

    os_irq_unlock(key);

    if (completed) {
        event_cb(user_data);
    }

    return ret_val;

#else
    event_cb(user_data);
    return 0;
#endif /* LV_USE_GPU_ACTS_DMA2D */
}

lv_res_t lv_port_gpu_wait_event(int event_id)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();

    while (ctx->event_cb != NULL) {
        os_sem_take(&ctx->timeline_sem, OS_FOREVER);
    }

    return LV_RES_OK;
}

void lv_gpu_acts_timeline_submit(void)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();

    ctx->timeline_cnt++;
}

LV_ATTRIBUTE_VERY_FAST_MEM void lv_gpu_acts_timeline_inc(void)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();

    ctx->timeline_val++;
    if (ctx->event_cb && ctx->timeline_val == ctx->event_val) {
        ctx->event_cb(ctx->event_data);
        ctx->event_cb = NULL;

        os_sem_give(&ctx->timeline_sem);
    }
}

void lv_draw_gpu_acts_ctx_init(lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx)
{
    lv_draw_sw_init_ctx(drv, draw_ctx);
    draw_ctx->init_buf = _lv_draw_gpu_init_buf;
    draw_ctx->draw_img = _lv_draw_gpu_img;
    draw_ctx->draw_img_decoded = _lv_draw_gpu_img_decoded;
    draw_ctx->buffer_copy = _lv_draw_gpu_buffer_copy;
    draw_ctx->draw_letter = _lv_draw_gpu_letter;
    draw_ctx->wait_for_finish = _lv_draw_gpu_wait_for_finish;

#if LV_USE_GPU_ACTS_VG_LITE
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();

    if (ctx->accl_supported & ACCL_VGLITE_BIT) {
        draw_ctx->draw_arc = lv_draw_vglite_arc;
        draw_ctx->draw_rect = lv_draw_vglite_rect;
        draw_ctx->draw_line = lv_draw_vglite_line;
    }
#endif /* LV_USE_GPU_ACTS_VG_LITE */

#if LV_USE_GPU_ACTS_DMA2D || LV_USE_GPU_ACTS_VG_LITE
    draw_ctx->layer_init = _lv_draw_gpu_layer_init;
    draw_ctx->layer_adjust = _lv_draw_gpu_layer_adjust;
    draw_ctx->layer_blend = _lv_draw_gpu_layer_blend;
    draw_ctx->layer_destroy = _lv_draw_gpu_layer_destroy;
    draw_ctx->layer_instance_size = sizeof(lv_draw_gpu_acts_layer_ctx_t);
#endif

    ((lv_draw_sw_ctx_t *)draw_ctx)->blend = _lv_draw_gpu_blend;
}

void lv_draw_gpu_acts_ctx_deinit(lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx)
{
    lv_draw_sw_deinit_ctx(drv, draw_ctx);
}

void lv_gpu_acts_set_accl_type(lv_draw_ctx_t * draw_ctx, uint8_t type, const lv_area_t * area)
{
    static const lv_gpu_acts_wait_finish_t wait_finish_vtbl[ACCL_NUM_HW_TYPES] = {
        [ACCL_CPU] = _lv_gpu_cpu_wait_for_finish,
#if LV_USE_GPU_ACTS_VG_LITE
        [ACCL_VGLITE] = lv_draw_vglite_wait_for_finish,
#endif
#if LV_USE_GPU_ACTS_DMA2D
        [ACCL_DMA2D] = lv_gpu_acts_dma2d_wait_for_finish,
#endif
#if LV_USE_GPU_ACTS_JPG
        [ACCL_JPG] = lv_gpu_acts_jpg_wait_for_finish,
#endif
    };

    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();

    LV_LOG_TRACE("accl_type: %d->%d", ctx->accl_type, type);

    if (draw_ctx == NULL) {
        lv_disp_t *disp = _lv_refr_get_disp_refreshing();
        draw_ctx = disp->driver->draw_ctx;
    }

    /* FIXME:
     * Two ways to handle the dcache:
     * 1) invalidate dcache when draw buffer changed in init_buf() which may
     *    have side effect on performance in the case of always going to the
     *    HW rendering path actually.
     * 2) just record the dcache range then invalidate it only when meet next
     *    ACCL_CPU which may result in normal memory is invalidated if there
     *    are more than 2 address dis-joint draw buffers.
     */
    if (ctx->buf_cacheable) {
        if (type == ACCL_CPU) {
            _lv_gpu_invalidate_dcache();
            area = draw_ctx->buf_area;
        }
        else if (ctx->accl_type == ACCL_CPU) {
            area = draw_ctx->buf_area;
        }
    }

    if (area == NULL || type == ACCL_NONE) {
        area = draw_ctx->buf_area;
    }

    for (int i = ACCL_NUM_HW_TYPES - 1; i >= 0; i--) {
        if (i != type && _lv_area_is_on(&ctx->accl_areas[i], area)) {
            assert(wait_finish_vtbl[i] != NULL);

            LV_STRACE_U32X2(LV_STRACE_ID_WAIT_DRAW, i, type);
            wait_finish_vtbl[i](draw_ctx);
            LV_STRACE_CALL(LV_STRACE_ID_WAIT_DRAW);

            _lv_gpu_reset_accl_area(&ctx->accl_areas[i]);
        }
    }

    ctx->accl_type = type;
}

void lv_gpu_acts_flush_dcache(lv_draw_ctx_t * draw_ctx, const lv_area_t * area)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();

    assert(ctx->accl_type < ACCL_NUM_HW_TYPES);

    if (ctx->accl_type > ACCL_CPU && ctx->buf_cacheable) {
        int stride = lv_area_get_width(draw_ctx->buf_area);
        uint8_t *buf_start = (uint8_t *)draw_ctx->buf +
                ((area->y1 - draw_ctx->buf_area->y1) * stride +
                    (area->x1 - draw_ctx->buf_area->x1)) * ctx->buf_pxbytes;
        uint8_t *buf_end = buf_start + ((area->y2 - area->y1) * stride +
                lv_area_get_width(area)) * ctx->buf_pxbytes;

        if (ctx->inv_dcache.buf_end <= ctx->inv_dcache.buf_start) {
            ctx->inv_dcache.buf_start = buf_start;
            ctx->inv_dcache.buf_end = buf_end;
        }
        else {
            if (ctx->inv_dcache.buf_start > buf_start)
                ctx->inv_dcache.buf_start = buf_start;
            if (ctx->inv_dcache.buf_end < buf_end)
                ctx->inv_dcache.buf_end = buf_end;
        }
    }

    _lv_area_join(&ctx->accl_areas[ctx->accl_type], &ctx->accl_areas[ctx->accl_type], area);
}

lv_res_t lv_gpu_acts_get_img_transformed_area(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        lv_area_t * draw_area, lv_area_t * src_area, const lv_area_t * coords)
{
    if (draw_dsc->zoom != LV_IMG_ZOOM_NONE || draw_dsc->angle != 0) {
        int16_t draw16_x1, draw16_y1, draw16_x2, draw16_y2;
        uint16_t zoom_recp = (draw_dsc->zoom < 2) ? UINT16_MAX :
                LV_IMG_ZOOM_NONE * LV_IMG_ZOOM_NONE / draw_dsc->zoom;

        sw_transform_area16(&draw16_x1, &draw16_y1, &draw16_x2, &draw16_y2,
                coords->x1, coords->y1, coords->x2, coords->y2,
                coords->x1 + draw_dsc->pivot.x,
                coords->y1 + draw_dsc->pivot.y,
                draw_dsc->angle, draw_dsc->zoom, draw_dsc->zoom, 8);

        draw_area->x1 = draw16_x1;
        draw_area->y1 = draw16_y1;
        draw_area->x2 = draw16_x2;
        draw_area->y2 = draw16_y2;

        if (_lv_area_intersect(draw_area, draw_area, draw_ctx->clip_area) == false)
            return LV_RES_INV;

        sw_transform_area16(&draw16_x1, &draw16_y1, &draw16_x2, &draw16_y2,
                draw_area->x1 << 1, draw_area->y1 << 1, draw_area->x2 << 1, draw_area->y2 << 1,
                (coords->x1 + draw_dsc->pivot.x) << 1,
                (coords->y1 + draw_dsc->pivot.y) << 1,
                3600 - draw_dsc->angle, zoom_recp, zoom_recp, 8);

        /* round to nearest point */
        src_area->x1 = (draw16_x1 + 1) >> 1;
        src_area->y1 = (draw16_y1 + 1) >> 1;
        src_area->x2 = (draw16_x2 + 1) >> 1;
        src_area->y2 = (draw16_y2 + 1) >> 1;

        /**
         * FIXME: increase 1 pixel for compensation (will run sw zoom algorithm)
         *
         * In zoom only mode, DMA2D use src to dest mapping.
         *
         * FIXME: LV_USE_GPU_ACTS_VG_LITE will not run into this function.
         */
#if !LV_USE_GPU_ACTS_DMA2D //|| LV_USE_GPU_ACTS_VG_LITE
        lv_area_increase(src_area, 1, 1);
#else
        if (draw_dsc->angle > 0)
            lv_area_increase(src_area, 1, 1);
#endif

        if (_lv_area_intersect(src_area, src_area, coords) == false)
            return LV_RES_INV;

        /* relative to the top-left corner of image */
        lv_area_move(src_area, -coords->x1, -coords->y1);
    }
    else {
        if (_lv_area_intersect(draw_area, coords, draw_ctx->clip_area) == false)
            return LV_RES_INV;

        /* relative to the top-left corner of image */
        src_area->x1 = draw_area->x1 - coords->x1;
        src_area->y1 = draw_area->y1 - coords->y1;
        src_area->x2 = draw_area->x2 - coords->x1;
        src_area->y2 = draw_area->y2 - coords->y1;
    }

    return LV_RES_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static int _lv_gpu_init(void)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();

    if (gpu_ctx_is_inited == false) {
        gpu_ctx_is_inited = true;

        memset(ctx, 0, sizeof(*ctx));
        os_sem_init(&ctx->timeline_sem, 0, 1);

        ctx->accl_supported = ACCL_CPU_BIT;

#if LV_USE_GPU_ACTS_DMA2D
        if (lv_gpu_acts_dma2d_init() == 0) {
            ctx->accl_supported |= ACCL_DMA2D_BIT;
        }
#endif

#if LV_USE_GPU_ACTS_VG_LITE
        /* FIXME: set the maximum possible frame buffer size */
        if (lv_draw_vglite_init() == 0) {
            ctx->accl_supported |= ACCL_VGLITE_BIT;
        }
#endif

#if LV_USE_GPU_ACTS_JPG
        if (lv_gpu_acts_jpg_init() == 0) {
            ctx->accl_supported |= ACCL_JPG_BIT;
        }
#endif
    }

    return 0;
}

static void _lv_gpu_reset_accl_area(lv_area_t * area)
{
    area->x1 = INT16_MAX;
    area->y1 = INT16_MAX;
    area->x2 = 0;
    area->y2 = 0;
}

static void _lv_gpu_invalidate_dcache(void)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();

    if (ctx->inv_dcache.buf_start < ctx->inv_dcache.buf_end) {
        /*
         * FIXME:
         * use mem_dcache_flush() instead of mem_dcache_invalidate() which relax
         * the buffer cache line alignment to avoid invalidating the normal data
         * not in the buffer but in the same cache line.
         */
        mem_dcache_flush(ctx->inv_dcache.buf_start,
                ctx->inv_dcache.buf_end - ctx->inv_dcache.buf_start);
        mem_dcache_sync();
        ctx->inv_dcache.buf_end = NULL;
    }
}

static void _lv_gpu_clear_cb(lv_disp_drv_t * disp_drv, uint8_t * buf, uint32_t size)
{
    lv_gpu_acts_sw_clear(buf, size);
}

static void _lv_gpu_clean_dcache_cb(lv_disp_drv_t * disp_drv)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();

    if (ctx->accl_type == ACCL_CPU && ctx->buf_cacheable) {
        if (disp_drv == NULL) {
           lv_disp_t *disp = _lv_refr_get_disp_refreshing();
           disp_drv = disp->driver;
        }

        lv_gpu_acts_set_accl_type(disp_drv->draw_ctx, ACCL_NONE, NULL);
    }
}

static void _lv_gpu_cpu_wait_for_finish(lv_draw_ctx_t * draw_ctx)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();
    const lv_area_t *cpu_area = &ctx->accl_areas[ACCL_CPU];

    lv_draw_sw_wait_for_finish(draw_ctx);

    if (ctx->buf_cacheable && cpu_area->x1 <= cpu_area->x2) {
        int stride = lv_area_get_width(draw_ctx->buf_area);
        uint8_t *buf = (uint8_t *)draw_ctx->buf +
                ((cpu_area->y1 - draw_ctx->buf_area->y1) * stride +
                    (cpu_area->x1 - draw_ctx->buf_area->x1)) * ctx->buf_pxbytes;
        uint32_t len = ((cpu_area->y2 - cpu_area->y1) * stride +
                lv_area_get_width(cpu_area)) * ctx->buf_pxbytes;

        mem_dcache_clean(buf, len);
        mem_dcache_sync();
    }
}

static void _lv_draw_gpu_wait_for_finish(lv_draw_ctx_t * draw_ctx)
{
    lv_gpu_acts_set_accl_type(draw_ctx, ACCL_CPU, draw_ctx->buf_area);
}

static void _lv_draw_gpu_init_buf(lv_draw_ctx_t * draw_ctx)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();
    bool screen_transp = false;
    int i;

#if LV_COLOR_SCREEN_TRANSP
    lv_disp_t * disp = _lv_refr_get_disp_refreshing();
    screen_transp = (disp->driver->screen_transp == 1);
#endif

    ctx->buf_cacheable = mem_is_cacheable(draw_ctx->buf);
    ctx->buf_pxbytes = screen_transp ? LV_IMG_PX_SIZE_ALPHA_BYTE : sizeof(lv_color_t);

    for (i = ACCL_NUM_HW_TYPES - 1; i >= 0; i--) {
        _lv_gpu_reset_accl_area(&ctx->accl_areas[i]);
    }

#if LV_USE_GPU_ACTS_VG_LITE
    if (ctx->accl_supported & ACCL_VGLITE_BIT) {
        lv_draw_vglite_init_buf(draw_ctx, screen_transp);
    }
#endif

#if LV_USE_GPU_ACTS_DMA2D
    if (ctx->accl_supported & ACCL_DMA2D_BIT) {
        lv_gpu_acts_dma2d_init_buf(draw_ctx, screen_transp);
    }
#endif
}

static void _lv_draw_gpu_buffer_copy(
        lv_draw_ctx_t * draw_ctx, void * dest_buf, lv_coord_t dest_stride,
        const lv_area_t * dest_area, void * src_buf, lv_coord_t src_stride,
        const lv_area_t * src_area)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();
    lv_color_t * dest_bufc = dest_buf;
    lv_color_t * src_bufc = src_buf;
    lv_res_t res = LV_RES_INV;

    LV_UNUSED(ctx);

    /*Got the first pixel of each buffer*/
    dest_bufc += dest_stride * dest_area->y1 + dest_area->x1;
    src_bufc += src_stride * src_area->y1 + src_area->x1;

#if LV_USE_GPU_ACTS_DMA2D
    if (ctx->accl_supported & ACCL_DMA2D_BIT) {
        res = lv_gpu_acts_dma2d_copy(dest_bufc, dest_stride, src_bufc, src_stride,
                lv_area_get_width(dest_area), lv_area_get_height(dest_area));
    }
#endif

#if LV_USE_GPU_ACTS_VG_LITE
    if ((res != LV_RES_OK) && (ctx->accl_supported & ACCL_VGLITE_BIT)) {
        res = lv_draw_vglite_buffer_copy(dest_buf, dest_stride, dest_area,
                src_buf, src_stride, src_area);
    }
#endif

    if (res != LV_RES_OK) {
        lv_gpu_acts_sw_copy(dest_bufc, dest_stride, src_bufc, src_stride,
                lv_area_get_width(dest_area), lv_area_get_height(dest_area));

        /* FIXME: should we handle the cache ? */
    }
}

#if defined(CONFIG_LVGL_USE_IMG_DECODER_ACTS)
static lv_res_t _lv_draw_gpu_img_encoded(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();
    lv_draw_img_dsc_t tmp_draw_dsc;
    lv_area_t draw_area, tmp_coords;
    lv_img_dsc_t tmp_src;
    lv_coord_t stride = 0;
    lv_img_cf_t cf = LV_IMG_CF_UNKNOWN;
    lv_res_t res = LV_RES_INV;

    if (lv_gpu_acts_get_img_transformed_area(draw_ctx, draw_dsc,
                &draw_area, &tmp_coords, coords) == LV_RES_INV) {
        return LV_RES_OK;
    }

    tmp_src.data = lvgl_img_decoder_acts_read_area(src, &tmp_coords, &stride, &cf);
    if (tmp_src.data == NULL)
        return LV_RES_INV;

    tmp_src.header.cf = cf;
    tmp_src.header.w = stride;
    tmp_src.header.h = lv_area_get_height(&tmp_coords);

    memcpy(&tmp_draw_dsc, draw_dsc, sizeof(tmp_draw_dsc));
    tmp_draw_dsc.pivot.x = tmp_draw_dsc.pivot.x - tmp_coords.x1;
    tmp_draw_dsc.pivot.y = tmp_draw_dsc.pivot.y - tmp_coords.y1;

    lv_area_set_width(&tmp_coords, stride);
    lv_area_move(&tmp_coords, coords->x1, coords->y1);

#if LV_USE_GPU_ACTS_VG_LITE
    if (ctx->accl_supported & ACCL_VGLITE_BIT) {
        res = lv_draw_vglite_img_dsc(draw_ctx, &tmp_draw_dsc, &tmp_coords, &tmp_src);
        if (res == LV_RES_OK) {
            if (USE_NO_WAIT_FOR_RAW_IMG == 0) {
                lv_draw_vglite_wait_for_finish(draw_ctx);
            }

            return res;
        }
    }
#endif

#if LV_USE_GPU_ACTS_DMA2D
    if (ctx->accl_supported & ACCL_DMA2D_BIT) {
        res = lv_gpu_acts_dma2d_draw_img_dsc(draw_ctx, &tmp_draw_dsc, &tmp_coords, &tmp_src);
        if (res == LV_RES_OK) {
            if (USE_NO_WAIT_FOR_RAW_IMG == 0) {
                lv_gpu_acts_dma2d_wait_for_finish(draw_ctx);
            }

            return res;
        }
    }
#endif

    /* fallback to CPU */
    return lv_gpu_acts_sw_draw_img_dsc(draw_ctx, &tmp_draw_dsc, &tmp_coords, &tmp_src);
}
#endif /* CONFIG_LVGL_USE_IMG_DECODER_ACTS */

static lv_res_t _lv_draw_gpu_img_dsc_decoded(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src_dsc)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();
    lv_res_t res = LV_RES_INV;

    LV_UNUSED(ctx);
    LV_UNUSED(res);

#if LV_USE_GPU_ACTS_VG_LITE
    if (ctx->accl_supported & ACCL_VGLITE_BIT) {
        res = lv_draw_vglite_img_dsc(draw_ctx, draw_dsc, coords, src_dsc);
        if (res == LV_RES_OK) return res;
    }
#endif

#if LV_USE_GPU_ACTS_DMA2D
    if (ctx->accl_supported & ACCL_DMA2D_BIT) {
        res = lv_gpu_acts_dma2d_draw_img_dsc(draw_ctx, draw_dsc, coords, src_dsc);
        if (res == LV_RES_OK) return res;
    }
#endif

    /* fallback to CPU */
    return lv_gpu_acts_sw_draw_img_dsc(draw_ctx, draw_dsc, coords, src_dsc);
}

static lv_res_t _lv_draw_gpu_img(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const void * src)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();
    const lv_img_dsc_t * img_src = src;

    /* Avoid meaningless invoking draw_img_decoded() */
    ctx->accl_img_decoded = 0;

    if (lv_img_src_get_type(src) != LV_IMG_SRC_VARIABLE) {
        ctx->accl_img_decoded = 1;
        return LV_RES_INV;
    }

    if (draw_dsc->recolor_opa > LV_OPA_MIN && !_is_alpha_format(img_src->header.cf))
        return LV_RES_INV;

    if (img_src->header.cf == LV_IMG_CF_RAW ||
        img_src->header.cf == LV_IMG_CF_RAW_ALPHA) {
        ctx->accl_img_decoded = 1;

#if LV_USE_GPU_ACTS_JPG
        if (ctx->accl_supported & ACCL_JPG_BIT) {
            lv_res_t res = lv_gpu_acts_jpg_draw_img_dsc(draw_ctx, draw_dsc, coords, img_src);
            if (res == LV_RES_OK) return res;
        }
#endif

#if LV_USE_GPU_ACTS_SW_DECODER
        lv_res_t res = lv_gpu_acts_decompress_draw_img_dsc(draw_ctx, draw_dsc, coords, img_src);
        if (res == LV_RES_OK) return res;
#endif

#if defined(CONFIG_LVGL_USE_IMG_DECODER_ACTS)
        return _lv_draw_gpu_img_encoded(draw_ctx, draw_dsc, coords, img_src);
#else
        return LV_RES_INV;
#endif
    }

    return _lv_draw_gpu_img_dsc_decoded(draw_ctx, draw_dsc, coords, src);
}

static void _lv_draw_gpu_img_decoded(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const uint8_t * src_buf, lv_img_cf_t cf)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();
    lv_res_t res = LV_RES_INV;

    if (ctx->accl_img_decoded && (draw_dsc->recolor_opa <= LV_OPA_MIN || _is_alpha_format(cf))) {
        const lv_img_dsc_t src_dsc = {
            .header = {
                .cf = cf,
                .w = lv_area_get_width(coords),
                .h = lv_area_get_height(coords),
            },
            .data = src_buf,
        };

        res = _lv_draw_gpu_img_dsc_decoded(draw_ctx, draw_dsc, coords, &src_dsc);
    }

    if (res == LV_RES_INV)
        lv_draw_sw_img_decoded(draw_ctx, draw_dsc, coords, src_buf, cf);
}

static lv_res_t _lv_draw_gpu_letter_internal(
        lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,
        const lv_point_t * pos_p, uint32_t letter)
{
    lv_res_t res = LV_RES_INV;

#if LV_USE_GPU_ACTS_DMA2D || LV_USE_GPU_ACTS_VG_LITE
    if (lv_draw_mask_is_any(draw_ctx->clip_area)) {
        return LV_RES_INV;
    }

    lv_font_glyph_dsc_t g;
    if (lv_font_get_glyph_dsc(dsc->font, &g, letter, '\0') == false) {
        return LV_RES_INV;
    }

    /*Don't draw anything if the character is empty. E.g. space*/
    if ((g.box_h == 0) || (g.box_w == 0)) return LV_RES_OK;
    /*Can't handle img font and subpixel */
    if (g.resolved_font->subpx) return LV_RES_INV;

    lv_area_t g_area;
    g_area.x1 = pos_p->x + g.ofs_x;
    g_area.y1 = pos_p->y + (dsc->font->line_height - dsc->font->base_line) - g.box_h - g.ofs_y;
    g_area.x2 = g_area.x1 + g.box_w - 1;
    g_area.y2 = g_area.y1 + g.box_h - 1;

    /*If the letter is completely out of mask don't draw it*/
    lv_area_t clip_area;
    if (_lv_area_intersect(&clip_area, &g_area, draw_ctx->clip_area) == false) return LV_RES_OK;

    const uint8_t * map_p = lv_font_get_glyph_bitmap(g.resolved_font, letter);
    if (map_p == NULL) {
        return LV_RES_OK;
    }

    const lv_area_t *clip_area_ori = draw_ctx->clip_area;
    draw_ctx->clip_area = &clip_area;

    if (g.bpp == LV_IMGFONT_BPP) {
#if LV_USE_IMGFONT
        lv_draw_img_dsc_t img_dsc;
        lv_draw_img_dsc_init(&img_dsc);
        img_dsc.opa = dsc->opa;
        img_dsc.blend_mode = dsc->blend_mode;
        lv_draw_img(draw_ctx, &img_dsc, &g_area, map_p);
#else
        LV_LOG_ERROR("LV_USE_IMGFONT disabled");
#endif

        res = LV_RES_OK;
        goto out_restore_clip_area;
    }

#if LV_USE_GPU_ACTS_VG_LITE
    if (g.bpp == LV_SVGFONT_BPP) {
        lv_draw_vglite_letter(draw_ctx, dsc, pos_p, &g_area, map_p);
        res = LV_RES_OK;
        goto out_restore_clip_area;
    }
#endif

#if LV_USE_GPU_ACTS_DMA2D
    res = lv_gpu_acts_dma2d_draw_letter(draw_ctx, dsc, &g_area, &g, map_p);
#endif

out_restore_clip_area:
    /* CARE: must restore the clip area */
    draw_ctx->clip_area = clip_area_ori;
#endif /*LV_USE_GPU_ACTS_DMA2D || LV_USE_GPU_ACTS_VG_LITE*/

    return res;
}

static void _lv_draw_gpu_letter(
        lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,
        const lv_point_t * pos_p, uint32_t letter)
{
    lv_res_t res = _lv_draw_gpu_letter_internal(draw_ctx, dsc, pos_p, letter);
    if (res != LV_RES_OK)
        lv_draw_sw_letter(draw_ctx, dsc, pos_p, letter);
}

static void _lv_draw_gpu_blend(
        lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc)
{
    lvgl_gpu_draw_ctx_t *ctx = GET_GPU_CTX();
    lv_res_t res = LV_RES_INV;
    lv_area_t blend_area;

    LV_UNUSED(ctx);
    LV_UNUSED(res);

    if (dsc->opa <= (lv_opa_t)LV_OPA_MIN)
        return;

    if (!_lv_area_intersect(&blend_area, dsc->blend_area, draw_ctx->clip_area))
        return;

#if LV_USE_GPU_ACTS_VG_LITE
    if (ctx->accl_supported & ACCL_VGLITE_BIT) {
        res = lv_draw_vglite_blend_basic(draw_ctx, dsc);
        if (res == LV_RES_OK) return;
    }
#endif

#if LV_USE_GPU_ACTS_DMA2D
    if (ctx->accl_supported & ACCL_DMA2D_BIT) {
        res = lv_gpu_acts_dma2d_blend(draw_ctx, dsc);
        if (res == LV_RES_OK) return;
    }
#endif

    /* fallback to CPU */
    lv_gpu_acts_set_accl_type(draw_ctx, ACCL_CPU, draw_ctx->clip_area);
    lv_draw_sw_blend_basic(draw_ctx, dsc); /* LVGL sw blending */
    lv_gpu_acts_flush_dcache(draw_ctx, &blend_area);
}

static bool _is_alpha_format(lv_img_cf_t cf)
{
    if ((cf >= LV_IMG_CF_ALPHA_1BIT && cf <= LV_IMG_CF_ALPHA_8BIT) ||
        (cf >= LV_IMG_CF_ALPHA_1BIT_LE && cf <= LV_IMG_CF_ALPHA_4BIT_LE))
        return true;

    return false;
}

#if LV_USE_GPU_ACTS_DMA2D || LV_USE_GPU_ACTS_VG_LITE
struct _lv_draw_layer_ctx_t * _lv_draw_gpu_layer_init(
        struct _lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx,
        lv_draw_layer_flags_t flags)
{
    if(LV_COLOR_SCREEN_TRANSP == 0 && (flags & LV_DRAW_LAYER_FLAG_HAS_ALPHA)) {
        LV_LOG_WARN("Rendering this widget needs LV_COLOR_SCREEN_TRANSP 1");
        return NULL;
    }

    /* handle previous cache ops */
    _lv_gpu_clean_dcache_cb(NULL);
    _lv_gpu_invalidate_dcache();

    lv_draw_sw_layer_ctx_t * layer_sw_ctx = (lv_draw_sw_layer_ctx_t *) layer_ctx;
    lv_draw_gpu_acts_layer_ctx_t * gpu_layer_ctx = (lv_draw_gpu_acts_layer_ctx_t *)layer_ctx;
    uint32_t px_size = flags & LV_DRAW_LAYER_FLAG_HAS_ALPHA ? LV_IMG_PX_SIZE_ALPHA_BYTE : sizeof(lv_color_t);

    if(flags & LV_DRAW_LAYER_FLAG_CAN_SUBDIVIDE) {
        layer_sw_ctx->buf_size_bytes = LV_LAYER_SIMPLE_BUF_SIZE;
        uint32_t full_size = lv_area_get_size(&layer_sw_ctx->base_draw.area_full) * px_size;
        if(layer_sw_ctx->buf_size_bytes > full_size) layer_sw_ctx->buf_size_bytes = full_size;
        layer_sw_ctx->base_draw.buf = lv_mem_alloc(LAYER_MEM_ALIGN(layer_sw_ctx->buf_size_bytes) + LAYER_MEM_ALIGN_SIZE);
        if(layer_sw_ctx->base_draw.buf == NULL) {
            LV_LOG_WARN("Cannot allocate %"LV_PRIu32" bytes for layer buffer. Allocating %"LV_PRIu32" bytes instead. (Reduced performance)",
                        (uint32_t)layer_sw_ctx->buf_size_bytes, (uint32_t)LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE * px_size);
            layer_sw_ctx->buf_size_bytes = LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE;
            layer_sw_ctx->base_draw.buf = lv_mem_alloc(LAYER_MEM_ALIGN(layer_sw_ctx->buf_size_bytes) + LAYER_MEM_ALIGN_SIZE);
            if(layer_sw_ctx->base_draw.buf == NULL) {
                return NULL;
            }
        }

        gpu_layer_ctx->buf_ofs = LAYER_MEM_ALIGN(layer_sw_ctx->base_draw.buf) - (uintptr_t)layer_sw_ctx->base_draw.buf;
        layer_sw_ctx->base_draw.buf = (uint8_t *)layer_sw_ctx->base_draw.buf + gpu_layer_ctx->buf_ofs;

        layer_sw_ctx->base_draw.area_act = layer_sw_ctx->base_draw.area_full;
        layer_sw_ctx->base_draw.area_act.y2 = layer_sw_ctx->base_draw.area_full.y1;
        lv_coord_t w = lv_area_get_width(&layer_sw_ctx->base_draw.area_act);
        layer_sw_ctx->base_draw.max_row_with_alpha = layer_sw_ctx->buf_size_bytes / w / LV_IMG_PX_SIZE_ALPHA_BYTE;
        layer_sw_ctx->base_draw.max_row_with_no_alpha = layer_sw_ctx->buf_size_bytes / w / sizeof(lv_color_t);
    }
    else {
        layer_sw_ctx->base_draw.area_act = layer_sw_ctx->base_draw.area_full;
        layer_sw_ctx->buf_size_bytes = lv_area_get_size(&layer_sw_ctx->base_draw.area_full) * px_size;
        layer_sw_ctx->base_draw.buf = lv_mem_alloc(LAYER_MEM_ALIGN(layer_sw_ctx->buf_size_bytes) + LAYER_MEM_ALIGN_SIZE);
        if(layer_sw_ctx->base_draw.buf == NULL) {
            return NULL;
        }

        gpu_layer_ctx->buf_ofs = LAYER_MEM_ALIGN(layer_sw_ctx->base_draw.buf) - (uintptr_t)layer_sw_ctx->base_draw.buf;
        layer_sw_ctx->base_draw.buf = (uint8_t *)layer_sw_ctx->base_draw.buf + gpu_layer_ctx->buf_ofs;

        lv_memset_00(layer_sw_ctx->base_draw.buf, layer_sw_ctx->buf_size_bytes);
        layer_sw_ctx->has_alpha = flags & LV_DRAW_LAYER_FLAG_HAS_ALPHA ? 1 : 0;

        draw_ctx->buf = layer_sw_ctx->base_draw.buf;
        draw_ctx->buf_area = &layer_sw_ctx->base_draw.area_act;
        draw_ctx->clip_area = &layer_sw_ctx->base_draw.area_act;

        lv_disp_t * disp_refr = _lv_refr_get_disp_refreshing();
        disp_refr->driver->screen_transp = flags & LV_DRAW_LAYER_FLAG_HAS_ALPHA ? 1 : 0;

        /* init new draw buffer */
        _lv_draw_gpu_init_buf(draw_ctx);
    }

    mem_dcache_clean(layer_sw_ctx->base_draw.buf, layer_sw_ctx->buf_size_bytes);
    mem_dcache_sync();

    return layer_ctx;
}

static void _lv_draw_gpu_layer_adjust(lv_draw_ctx_t * draw_ctx,
        lv_draw_layer_ctx_t * layer_ctx, lv_draw_layer_flags_t flags)
{
   /* handle previous cache ops */
    _lv_gpu_clean_dcache_cb(NULL);
    _lv_gpu_invalidate_dcache();

    lv_draw_sw_layer_adjust(draw_ctx, layer_ctx, flags);

    /* init new draw buffer */
    _lv_draw_gpu_init_buf(draw_ctx);
}

static void _lv_draw_gpu_layer_blend(lv_draw_ctx_t * draw_ctx,
        lv_draw_layer_ctx_t * layer_ctx, const lv_draw_img_dsc_t * draw_dsc)
{
    /* handle previous cache ops */
    _lv_gpu_clean_dcache_cb(NULL);
    _lv_gpu_invalidate_dcache();

    lv_draw_sw_layer_ctx_t * layer_sw_ctx = (lv_draw_sw_layer_ctx_t *) layer_ctx;

    lv_img_dsc_t img;
    img.data = draw_ctx->buf;
    img.header.always_zero = 0;
    img.header.w = lv_area_get_width(draw_ctx->buf_area);
    img.header.h = lv_area_get_height(draw_ctx->buf_area);
    img.header.cf = layer_sw_ctx->has_alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;

    /*Restore the original draw_ctx*/
    draw_ctx->buf = layer_ctx->original.buf;
    draw_ctx->buf_area = layer_ctx->original.buf_area;
    draw_ctx->clip_area = layer_ctx->original.clip_area;
    lv_disp_t * disp_refr = _lv_refr_get_disp_refreshing();
    disp_refr->driver->screen_transp = layer_ctx->original.screen_transp;

    /* init new draw buffer */
    _lv_draw_gpu_init_buf(draw_ctx);

    /* Blend the layer */
    lv_draw_img(draw_ctx, draw_dsc, &layer_ctx->area_act, &img);
    lv_img_cache_invalidate_src(&img);
}

static void _lv_draw_gpu_layer_destroy(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx)
{
    lv_draw_gpu_acts_layer_ctx_t * gpu_layer_ctx = (lv_draw_gpu_acts_layer_ctx_t *)layer_ctx;
    lv_mem_free((uint8_t *)layer_ctx->buf - gpu_layer_ctx->buf_ofs);
}

#endif /*LV_USE_GPU_ACTS_DMA2D || LV_USE_GPU_ACTS_VG_LITE*/

#else /*LV_USE_GPU_ACTS*/

lv_res_t lv_port_gpu_init(lv_disp_drv_t * drv)
{
    return LV_RES_OK;
}

int lv_port_gpu_insert_event(lv_port_gpu_event_cb_t event_cb, void * user_data)
{
    if (event_cb)
        event_cb(user_data);

    return 0;
}

lv_res_t lv_port_gpu_wait_event(int event_id)
{
    return LV_RES_OK;
}

#endif /*LV_USE_GPU_ACTS*/
