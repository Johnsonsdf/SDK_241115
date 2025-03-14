/**
 * @file lv_gpu_acts_sw.c
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
#include "lv_gpu_acts.h"

#if LV_USE_GPU_ACTS
#include <display/sw_draw.h>
#include <display/sw_rotate.h>
#include <display/ui_memsetcpy.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL PROTOTYPES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static lv_res_t _lv_gpu_sw_draw_img_var(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src);
static lv_res_t _lv_gpu_sw_draw_transform_img_var(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_gpu_acts_sw_clear(uint8_t * buf, uint32_t size)
{
    bool screen_transp = false;

#if LV_COLOR_SCREEN_TRANSP
    lv_disp_t * disp = _lv_refr_get_disp_refreshing();
    screen_transp = (disp->driver->screen_transp == 1);
#endif

    size = screen_transp ? (size * LV_IMG_PX_SIZE_ALPHA_BYTE) : (size * sizeof(lv_color_t));

    mem_dcache_flush(buf, size);

    ui_memset(buf, 0, size);
    ui_memsetcpy_wait_finish(-1);
}

void lv_gpu_acts_sw_copy(
        lv_color_t * dest, int16_t dest_stride, const lv_color_t * src,
        int16_t src_stride, int16_t copy_w, int16_t copy_h)
{
    ui_memcpy2d(dest, dest_stride * sizeof(lv_color_t), src,
            src_stride * sizeof(lv_color_t), copy_w * sizeof(lv_color_t), copy_h);
    ui_memsetcpy_wait_finish(-1);
}

lv_res_t lv_gpu_acts_sw_draw_img_dsc(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src)
{
    if (need_argb8565_support(draw_ctx))
        return LV_RES_INV;

    if (draw_dsc->blend_mode != LV_BLEND_MODE_NORMAL)
        return LV_RES_INV;

    bool transform = (draw_dsc->angle != 0 || draw_dsc->zoom != LV_IMG_ZOOM_NONE) ? true : false;
    if (transform) {
        return _lv_gpu_sw_draw_transform_img_var(draw_ctx, draw_dsc, coords, src);
    } else {
        return _lv_gpu_sw_draw_img_var(draw_ctx, draw_dsc, coords, src);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_res_t _lv_gpu_sw_draw_img_var(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src)
{
    lv_res_t res = LV_RES_OK;
    uint8_t *dest;
    int32_t dest_pitch;
    const uint8_t *src_data;
    int32_t src_pitch;
    uint8_t src_bpp;
    uint32_t src_bofs;
    lv_area_t draw_area;
    int16_t w, h;
    uint32_t color32 = 0;

    if (_lv_area_intersect(&draw_area, coords, draw_ctx->clip_area) == false)
        return LV_RES_OK;

    if (lv_draw_mask_is_any(&draw_area))
        return LV_RES_INV;

    if (src->header.cf >= LV_IMG_CF_ALPHA_1BIT && src->header.cf <= LV_IMG_CF_ALPHA_8BIT) {
        uint8_t opa = (draw_dsc->opa < LV_OPA_MAX) ? draw_dsc->opa : LV_OPA_COVER;
        color32 = (lv_color_to32(draw_dsc->recolor) & 0xffffff) | ((uint32_t)opa << 24);
    } else if (draw_dsc->opa < LV_OPA_MAX) {
        return LV_RES_INV;
    }

    src_bpp = lv_img_cf_get_px_size(src->header.cf);
    src_pitch = (src->header.w * src_bpp + 7) >> 3;

    src_bofs = (draw_area.x1 - coords->x1) * src_bpp;
    src_data = src->data + src_pitch * (draw_area.y1 - coords->y1) + (src_bofs >> 3);
    src_bofs &= 0x7;

    dest_pitch = lv_area_get_width(draw_ctx->buf_area) * sizeof(lv_color_t);
    dest = (uint8_t *)draw_ctx->buf + dest_pitch * (draw_area.y1 - draw_ctx->buf_area->y1) +
                    (draw_area.x1 - draw_ctx->buf_area->x1) * sizeof(lv_color_t);

    w = lv_area_get_width(&draw_area);
    h = lv_area_get_height(&draw_area);

    lv_gpu_acts_set_accl_type(draw_ctx, ACCL_CPU, &draw_area);

    if (src->header.cf == LV_IMG_CF_TRUE_COLOR) {
        int16_t copy_len = w * sizeof(lv_color_t);

        for (int j = h; j > 0; j--) {
            memcpy(dest, src_data, copy_len);
            src_data += src_pitch;
            dest += dest_pitch;
        }

        goto out_exit;
    }

#if LV_COLOR_DEPTH == 16
    if (src->header.cf == LV_IMG_CF_ARGB_8565) {
        sw_blend_argb8565_over_rgb565(dest, src_data, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf  == LV_IMG_CF_ARGB_8888) {
        sw_blend_argb8888_over_rgb565(dest, src_data, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_ARGB_6666) {
        sw_blend_argb6666_over_rgb565(dest, src_data, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_ARGB_1555) {
        sw_blend_argb1555_over_rgb565(dest, src_data, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_ALPHA_8BIT) {
        sw_blend_a8_over_rgb565(dest, src_data, color32, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_ALPHA_4BIT) {
        sw_blend_a4_over_rgb565(dest, src_data, color32, dest_pitch, src_pitch, src_bofs, w, h);
    } else if (src->header.cf == LV_IMG_CF_ALPHA_2BIT) {
        sw_blend_a2_over_rgb565(dest, src_data, color32, dest_pitch, src_pitch, src_bofs, w, h);
    } else if (src->header.cf == LV_IMG_CF_ALPHA_1BIT) {
        sw_blend_a1_over_rgb565(dest, src_data, color32, dest_pitch, src_pitch, src_bofs, w, h);
    } else if (src->header.cf == LV_IMG_CF_INDEXED_8BIT_ACTS) {
        uint32_t palette_size = *(uint32_t *)src->data;
        if (palette_size == 0 || palette_size > 256) {
            LV_LOG_ERROR("invalid palette size %u", palette_size);
            return LV_RES_INV;
        }

        src_data += palette_size * 4 + 4;
        sw_blend_index8_over_rgb565(dest, src_data, (uint32_t *)src->data + 1, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_INDEXED_8BIT) {
        src_data += 256 * 4;
        sw_blend_index8_over_rgb565(dest, src_data, (uint32_t *)src->data, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_INDEXED_4BIT) {
        src_data += 16 * 4;
        sw_blend_index4_over_rgb565(dest, src_data, (uint32_t *)src->data, dest_pitch, src_pitch, src_bofs, w, h);
    } else {
        res = LV_RES_INV;
    }
#else /* LV_COLOR_DEPTH == 32 */
    if (src->header.cf == LV_IMG_CF_ARGB_8888) {
        sw_blend_argb8888_over_argb8888(dest, src_data, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_ARGB_8565) {
        sw_blend_argb8565_over_argb8888(dest, src_data, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_ARGB_6666) {
        sw_blend_argb6666_over_argb8888(dest, src_data, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_ARGB_1555) {
        sw_blend_argb1555_over_argb8888(dest, src_data, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_ALPHA_8BIT) {
        sw_blend_a8_over_argb8888(dest, src_data, color32, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_ALPHA_4BIT) {
        sw_blend_a4_over_argb8888(dest, src_data, color32, dest_pitch, src_pitch, src_bofs, w, h);
    } else if (src->header.cf == LV_IMG_CF_ALPHA_2BIT) {
        sw_blend_a2_over_argb8888(dest, src_data, color32, dest_pitch, src_pitch, src_bofs, w, h);
    } else if (src->header.cf == LV_IMG_CF_ALPHA_1BIT) {
        sw_blend_a1_over_argb8888(dest, src_data, color32, dest_pitch, src_pitch, src_bofs, w, h);
    } else if (src->header.cf == LV_IMG_CF_INDEXED_8BIT_ACTS) {
        uint32_t palette_size = *(uint32_t *)src->data;
        if (palette_size == 0 || palette_size > 256) {
            LV_LOG_ERROR("invalid palette size %u", palette_size);
            return LV_RES_INV;
        }

        src_data += palette_size * 4 + 4;
        sw_blend_index8_over_argb8888(dest, src_data, (uint32_t *)src->data + 1, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_INDEXED_8BIT) {
        src_data += 256 * 4;
        sw_blend_index8_over_argb8888(dest, src_data, (uint32_t *)src->data, dest_pitch, src_pitch, w, h);
    } else if (src->header.cf == LV_IMG_CF_INDEXED_4BIT) {
        src_data += 16 * 4;
        sw_blend_index4_over_argb8888(dest, src_data, (uint32_t *)src->data, dest_pitch, src_pitch, src_bofs, w, h);
    } else {
        res = LV_RES_INV;
    }
#endif /* LV_COLOR_DEPTH == 16 */

out_exit:
    if (res == LV_RES_OK) {
        lv_gpu_acts_flush_dcache(draw_ctx, &draw_area);
    }

    return res;
}

static lv_res_t _lv_gpu_sw_draw_transform_img_var(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src)
{
    sw_matrix_t matrix;
    uint32_t color32 = 0;
    void (* transform_fn)(void *, const void *, uint16_t, uint16_t, uint16_t, uint16_t,
            int16_t, int16_t, uint16_t, uint16_t, const sw_matrix_t *) = NULL;
    void (* transform_a8_fn)(void *, const void *, uint32_t, uint16_t, uint16_t, uint16_t,
            uint16_t, int16_t, int16_t, uint16_t, uint16_t, const sw_matrix_t *) = NULL;
    void (* transform_index_fn)(void *, const void *, const uint32_t *, uint16_t, uint16_t,
            uint16_t, uint16_t, int16_t, int16_t, uint16_t, uint16_t, const sw_matrix_t *) = NULL;
    const uint32_t * palette = (uint32_t *)src->data;
    const uint8_t * src_data = src->data;
    int32_t src_pitch;

    if (lv_draw_mask_is_any(draw_ctx->clip_area))
        return LV_RES_INV;

    if (src->header.cf == LV_IMG_CF_ALPHA_8BIT) {
        uint8_t opa = (draw_dsc->opa < LV_OPA_MAX) ? draw_dsc->opa : LV_OPA_COVER;
        color32 = (lv_color_to32(draw_dsc->recolor) & 0xffffff) | ((uint32_t)opa << 24);
    } else if (draw_dsc->opa < LV_OPA_MAX) {
        return LV_RES_INV;
    }

    src_pitch = (src->header.w * lv_img_cf_get_px_size(src->header.cf) + 7) >> 3;

#if LV_COLOR_DEPTH == 16
    if (src->header.cf  == LV_IMG_CF_ARGB_8565) {
        transform_fn = sw_transform_argb8565_over_rgb565;
    } else if (src->header.cf == LV_IMG_CF_ARGB_8888) {
        transform_fn = sw_transform_argb8888_over_rgb565;
    } else if (src->header.cf == LV_IMG_CF_ARGB_6666) {
        transform_fn = sw_transform_argb6666_over_rgb565;
    } else if (src->header.cf == LV_IMG_CF_ALPHA_8BIT) {
        transform_a8_fn = sw_transform_a8_over_rgb565;
    } else if (src->header.cf == LV_IMG_CF_RGB_565) {
        transform_fn = sw_transform_rgb565_over_rgb565;
    } else if (src->header.cf == LV_IMG_CF_RGB_888) {
        transform_fn = sw_transform_rgb888_over_rgb565;
    } else if (src->header.cf == LV_IMG_CF_XRGB_8888) {
        transform_fn = sw_transform_xrgb8888_over_rgb565;
    } else if (src->header.cf == LV_IMG_CF_INDEXED_8BIT_ACTS) {
        uint32_t palette_size = *palette++;
        if (palette_size == 0 || palette_size > 256) {
            LV_LOG_ERROR("invalid palette size %u", palette_size);
            return LV_RES_INV;
        }

        src_data += palette_size * 4 + 4;
        transform_index_fn = sw_transform_index8_over_rgb565;
    } else if (src->header.cf == LV_IMG_CF_INDEXED_8BIT) {
        src_data += 256 * 4;
        transform_index_fn = sw_transform_index8_over_rgb565;
    } else if (src->header.cf == LV_IMG_CF_INDEXED_4BIT) {
        src_data += 16 * 4;
        transform_index_fn = sw_transform_index4_over_rgb565;
    }

#else /* LV_COLOR_DEPTH == 32 */
    if (src->header.cf == LV_IMG_CF_ARGB_8888) {
        transform_fn = sw_transform_argb8888_over_argb8888;
    } else if (src->header.cf  == LV_IMG_CF_ARGB_8565) {
        transform_fn = sw_transform_argb8565_over_argb8888;
    } else if (src->header.cf  == LV_IMG_CF_ARGB_6666) {
        transform_fn = sw_transform_argb6666_over_argb8888;
    } else if (src->header.cf == LV_IMG_CF_ALPHA_8BIT) {
        transform_a8_fn = sw_transform_a8_over_argb8888;
    } else if (src->header.cf == LV_IMG_CF_RGB_565) {
        transform_fn = sw_transform_rgb565_over_argb8888;
    } else if (src->header.cf == LV_IMG_CF_RGB_888) {
        transform_fn = sw_transform_rgb888_over_argb8888;
    } else if (src->header.cf == LV_IMG_CF_XRGB_8888) {
        transform_fn = sw_transform_xrgb8888_over_argb8888;
    } else if (src->header.cf == LV_IMG_CF_INDEXED_8BIT_ACTS) {
        uint32_t palette_size = *palette++;
        if (palette_size == 0 || palette_size > 256) {
            LV_LOG_ERROR("invalid palette size %u", palette_size);
            return LV_RES_INV;
        }

        src_data += palette_size * 4 + 4;
        transform_index_fn = sw_transform_index8_over_argb8888;
    } else if (src->header.cf == LV_IMG_CF_INDEXED_8BIT) {
        src_data += 256 * 4;
        transform_index_fn = sw_transform_index8_over_argb8888;
    } else if (src->header.cf == LV_IMG_CF_INDEXED_4BIT) {
        src_data += 16 * 4;
        transform_index_fn = sw_transform_index4_over_argb8888;
    }
#endif /* LV_COLOR_DEPTH == 16 */

    if (transform_fn || transform_a8_fn || transform_index_fn) {
        lv_color_t *dest = (lv_color_t *)draw_ctx->buf +
                lv_area_get_width(draw_ctx->buf_area) * (draw_ctx->clip_area->y1 - draw_ctx->buf_area->y1) +
                    (draw_ctx->clip_area->x1 - draw_ctx->buf_area->x1);

        sw_transform_config(0, 0, draw_dsc->pivot.x, draw_dsc->pivot.y,
                draw_dsc->angle, draw_dsc->zoom, draw_dsc->zoom, 8, &matrix);

        /* FIXME: the real draw area is not computed */
        lv_gpu_acts_set_accl_type(draw_ctx, ACCL_CPU, draw_ctx->clip_area);

        if (transform_fn) {
            transform_fn(dest, src_data, lv_area_get_width(draw_ctx->buf_area) * sizeof(lv_color_t),
                    src_pitch, src->header.w, src->header.h,
                    draw_ctx->clip_area->x1 - coords->x1, draw_ctx->clip_area->y1 - coords->y1,
                    lv_area_get_width(draw_ctx->clip_area), lv_area_get_height(draw_ctx->clip_area),
                    &matrix);
        } else if (transform_index_fn) {
            transform_index_fn(dest, src_data, palette, lv_area_get_width(draw_ctx->buf_area) * sizeof(lv_color_t),
                    src_pitch, src->header.w, src->header.h,
                    draw_ctx->clip_area->x1 - coords->x1, draw_ctx->clip_area->y1 - coords->y1,
                    lv_area_get_width(draw_ctx->clip_area), lv_area_get_height(draw_ctx->clip_area),
                    &matrix);
        } else {
            transform_a8_fn(dest, src_data, color32,
                    lv_area_get_width(draw_ctx->buf_area) * sizeof(lv_color_t),
                    src_pitch, src->header.w, src->header.h,
                    draw_ctx->clip_area->x1 - coords->x1, draw_ctx->clip_area->y1 - coords->y1,
                    lv_area_get_width(draw_ctx->clip_area), lv_area_get_height(draw_ctx->clip_area),
                    &matrix);
        }

        lv_gpu_acts_flush_dcache(draw_ctx, draw_ctx->clip_area);
        return LV_RES_OK;
    }

    return LV_RES_INV;
}

#endif/*LV_USE_GPU_ACTS*/
