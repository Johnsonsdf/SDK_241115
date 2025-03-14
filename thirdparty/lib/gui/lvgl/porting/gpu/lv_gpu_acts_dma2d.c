/**
 * @file lv_gpu_acts_dma2d.c
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

#if LV_USE_GPU_ACTS_DMA2D
#include <dma2d_hal.h>
#include <display/sw_math.h>

/*********************
 *      DEFINES
 *********************/

#if LV_COLOR_DEPTH == 32
#  define LVGL_DMA2D_COLOR_FORMAT HAL_PIXEL_FORMAT_XRGB_8888
#  define LVGL_DMA2D_COLOR_ALPHA_FORMAT HAL_PIXEL_FORMAT_ARGB_8888
#else
#  define LVGL_DMA2D_COLOR_FORMAT HAL_PIXEL_FORMAT_RGB_565
#  define LVGL_DMA2D_COLOR_ALPHA_FORMAT HAL_PIXEL_FORMAT_ARGB_8565
#endif

#define INDEX_PIXEL_FORMATS \
	(PIXEL_FORMAT_I8 | PIXEL_FORMAT_I4 | PIXEL_FORMAT_I2 | PIXEL_FORMAT_I1)

#define ABMP_BITS_PIXEL_FORMATS \
	(PIXEL_FORMAT_A4 | PIXEL_FORMAT_A4_LE | PIXEL_FORMAT_A2 | \
	 PIXEL_FORMAT_A1 | PIXEL_FORMAT_A1_LE | PIXEL_FORMAT_I4 | \
	 PIXEL_FORMAT_I2 | PIXEL_FORMAT_I1)

/**********************
 *  GLOBAL PROTOTYPES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
LV_ATTRIBUTE_VERY_FAST_MEM static void _lv_gpu_dma2d_xfer_cb(
        hal_dma2d_handle_t *hdma2d, uint16_t cmd_seq, uint32_t error_code);
static lv_res_t _lv_gpu_dma2d_fill(
        lv_draw_ctx_t * draw_ctx, lv_color_t color, lv_opa_t opa);
static lv_res_t _lv_gpu_dma2d_blit(
        lv_draw_ctx_t * draw_ctx, const lv_img_dsc_t * src,
        const lv_area_t * src_area, lv_color_t color, lv_opa_t opa,
        bool is_bitstream);
static lv_res_t _lv_gpu_dma2d_zoom_img_var(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src);
static lv_res_t _lv_gpu_dma2d_rotate_img_var(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src);

/**********************
 *  STATIC VARIABLES
 **********************/
static hal_dma2d_handle_t hdma2d __in_section_unique(lvgl.noinit.gpu);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int lv_gpu_acts_dma2d_init(void)
{
    int res = hal_dma2d_init(&hdma2d, HAL_DMA2D_FULL_MODES);
    if (res == 0) {
        /* initialize hdma2d constant fields */
        hdma2d.layer_cfg[0].alpha_mode = HAL_DMA2D_NO_MODIF_ALPHA;

        hal_dma2d_register_callback(&hdma2d, _lv_gpu_dma2d_xfer_cb);
    }

    return res;
}

void lv_gpu_acts_dma2d_wait_for_finish(lv_draw_ctx_t * draw_ctx)
{
    hal_dma2d_poll_transfer(&hdma2d, -1);
}

void lv_gpu_acts_dma2d_init_buf(lv_draw_ctx_t * draw_ctx, bool screen_transp)
{
    if (screen_transp) {
        hdma2d.output_cfg.color_format = LVGL_DMA2D_COLOR_ALPHA_FORMAT;
        hdma2d.output_cfg.output_pitch = lv_area_get_width(draw_ctx->buf_area) * LV_IMG_PX_SIZE_ALPHA_BYTE;
    } else {
        hdma2d.output_cfg.color_format = LVGL_DMA2D_COLOR_FORMAT;
        hdma2d.output_cfg.output_pitch = lv_area_get_width(draw_ctx->buf_area) * sizeof(lv_color_t);
    }

    hdma2d.layer_cfg[0].color_format = hdma2d.output_cfg.color_format;
    hdma2d.layer_cfg[0].input_pitch = hdma2d.output_cfg.output_pitch;
}

lv_res_t lv_gpu_acts_dma2d_copy(
        lv_color_t * dest, int16_t dest_stride, const lv_color_t * src_buf,
        int16_t src_stride, int16_t copy_w, int16_t copy_h)
{
    int res;

    /* configure the output */
    hdma2d.output_cfg.mode = HAL_DMA2D_M2M;
    hdma2d.output_cfg.output_pitch = dest_stride * sizeof(lv_color_t);
    hdma2d.output_cfg.color_format = LVGL_DMA2D_COLOR_FORMAT;
    hal_dma2d_config_output(&hdma2d);

    /* configure the foreground */
    hdma2d.layer_cfg[1].input_pitch = src_stride * sizeof(lv_color_t);
    hdma2d.layer_cfg[1].input_width = copy_w;
    hdma2d.layer_cfg[1].input_height = copy_h;
    hdma2d.layer_cfg[1].alpha_mode = HAL_DMA2D_NO_MODIF_ALPHA;
    hdma2d.layer_cfg[1].color_format = LVGL_DMA2D_COLOR_FORMAT;
    hal_dma2d_config_layer(&hdma2d, HAL_DMA2D_FOREGROUND_LAYER);

    //lv_gpu_acts_set_accl_type(NULL, ACCL_DMA2D, NULL);

    res = hal_dma2d_start(&hdma2d, (uint32_t)src_buf, (uint32_t)dest, copy_w, copy_h);
    if (res >= 0) {
        lv_gpu_acts_timeline_submit();
        return LV_RES_OK;
    }

    /* FIXME: should we invalidate the cache of src_buf ? */

    return LV_RES_INV;
}

lv_res_t lv_gpu_acts_dma2d_draw_img_dsc(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src)
{
    if (need_argb8565_support(draw_ctx) || draw_dsc->blend_mode != LV_BLEND_MODE_NORMAL)
        return LV_RES_INV;

    if (draw_dsc->angle == 0) {
        return _lv_gpu_dma2d_zoom_img_var(draw_ctx, draw_dsc, coords, src);
    } else {
        return _lv_gpu_dma2d_rotate_img_var(draw_ctx, draw_dsc,coords, src);
    }
}

lv_res_t lv_gpu_acts_dma2d_draw_letter(
        lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,
        const lv_area_t * area_p, lv_font_glyph_dsc_t * g, const uint8_t * map_p)
{
    lv_img_dsc_t src;
    src.header.w = g->box_w;
    src.header.h = g->box_h;
    src.data = map_p;

    if (need_argb8565_support(draw_ctx) || dsc->blend_mode != LV_BLEND_MODE_NORMAL)
        return LV_RES_INV;

    switch (g->bpp) {
    case 1:
        src.header.cf = LV_IMG_CF_ALPHA_1BIT;
        break;
    case 2:
        src.header.cf = LV_IMG_CF_ALPHA_2BIT;
        break;
    case 4:
        src.header.cf = LV_IMG_CF_ALPHA_4BIT;
        break;
    case 8:
        src.header.cf = LV_IMG_CF_ALPHA_8BIT;
        break;
    default:
        LV_LOG_WARN("draw_letter: invalid bpp");
        return LV_RES_OK; /*Invalid bpp. Can't render the letter*/
    }

    lv_area_t src_area;
    lv_area_copy(&src_area, draw_ctx->clip_area);
    /* move to relative to the map */
    lv_area_move(&src_area, -area_p->x1, -area_p->y1);

    return _lv_gpu_dma2d_blit(draw_ctx, &src, &src_area, dsc->color, dsc->opa, true);
}

lv_res_t lv_gpu_acts_dma2d_blend(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc)
{
    lv_img_dsc_t img_src;
    lv_res_t res = LV_RES_INV;

    if (need_argb8565_support(draw_ctx) || dsc->blend_mode != LV_BLEND_MODE_NORMAL)
        return LV_RES_INV;

    const lv_opa_t * mask;
    if (dsc->mask_buf && dsc->mask_res == LV_DRAW_MASK_RES_TRANSP) return LV_RES_OK;
    else if (dsc->mask_res == LV_DRAW_MASK_RES_FULL_COVER) mask = NULL;
    else mask = dsc->mask_buf;

    if (mask != NULL && dsc->src_buf != NULL) return LV_RES_INV;

    lv_area_t blend_area;
    if (!_lv_area_intersect(&blend_area, dsc->blend_area, draw_ctx->clip_area))
        return LV_RES_OK;

    /* dsc->src_buf and mask will not be valid at the same time */
    if (dsc->src_buf) {
        img_src.header.cf = LV_IMG_CF_TRUE_COLOR;
        img_src.header.w = lv_area_get_width(dsc->blend_area);
        img_src.header.h = lv_area_get_height(&blend_area);
        img_src.data = (uint8_t *)dsc->src_buf + sizeof(lv_color_t) *
                (img_src.header.w * (blend_area.y1 - dsc->blend_area->y1) +
                    (blend_area.x1 - dsc->blend_area->x1));
    } else if (mask) {
        img_src.header.cf = LV_IMG_CF_ALPHA_8BIT;
        img_src.header.w = lv_area_get_width(dsc->mask_area);
        img_src.header.h = lv_area_get_height(&blend_area);
        img_src.data = (uint8_t *)mask +
                img_src.header.w * (blend_area.y1 - dsc->mask_area->y1) +
                    (blend_area.x1 - dsc->mask_area->x1);
    } else {
        img_src.data = NULL;
    }

    const lv_area_t *clip_area_ori = draw_ctx->clip_area;
    draw_ctx->clip_area = &blend_area;

    if (img_src.data) {
        res = _lv_gpu_dma2d_blit(draw_ctx, &img_src, NULL, dsc->color, dsc->opa, false);

        /* FIXME: it seems the mask buf is temporary, so must sync at once */
        if (res == LV_RES_OK && mask != NULL) {
            hal_dma2d_poll_transfer(&hdma2d, -1);
        }
    } else {
        res = _lv_gpu_dma2d_fill(draw_ctx, dsc->color, dsc->opa);
    }

    draw_ctx->clip_area = clip_area_ori;
    return res;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

LV_ATTRIBUTE_VERY_FAST_MEM static void _lv_gpu_dma2d_xfer_cb(
        hal_dma2d_handle_t *hdma2d, uint16_t cmd_seq, uint32_t error_code)
{
    lv_gpu_acts_timeline_inc();
}

static lv_res_t _lv_gpu_dma2d_fill(lv_draw_ctx_t * draw_ctx, lv_color_t color, lv_opa_t opa)
{
    lv_color32_t c32 = { .full = lv_color_to32(color) };
    int16_t fill_w = lv_area_get_width(draw_ctx->clip_area);
    int16_t fill_h = lv_area_get_height(draw_ctx->clip_area);
    int res = 0;

    if ((uint32_t)fill_w * fill_h < LV_GPU_ACTS_DMA2D_SIZE_LIMIT) {
        LV_LOG_INFO("size %ux%u too small", fill_w, fill_h);
        return LV_RES_INV;
    }

    int32_t dest_stride = lv_area_get_width(draw_ctx->buf_area);
    lv_color_t * dest = (lv_color_t *)draw_ctx->buf +
            dest_stride * (draw_ctx->clip_area->y1 - draw_ctx->buf_area->y1) +
                    (draw_ctx->clip_area->x1 - draw_ctx->buf_area->x1);

    /* configure the output */
    hdma2d.output_cfg.mode = (opa >= LV_OPA_MAX) ? HAL_DMA2D_R2M : HAL_DMA2D_M2M_BLEND_FG;
    hal_dma2d_config_output(&hdma2d);

    lv_gpu_acts_set_accl_type(draw_ctx, ACCL_DMA2D, draw_ctx->clip_area);

    if (opa >= LV_OPA_MAX) {
        /* start filling */
        res = hal_dma2d_start(&hdma2d, c32.full, (uint32_t)dest, fill_w, fill_h);
    } else {
        /* configure the background */
        hdma2d.layer_cfg[0].input_width = fill_w;
        hdma2d.layer_cfg[0].input_height = fill_h;
        hal_dma2d_config_layer(&hdma2d, HAL_DMA2D_BACKGROUND_LAYER);

        /* start blending fg */
        c32.ch.alpha = opa;
        res = hal_dma2d_blending_start(&hdma2d, c32.full,
                    (uint32_t)dest, (uint32_t)dest, fill_w, fill_h);
    }

    if (res >= 0) {
        lv_gpu_acts_flush_dcache(draw_ctx, draw_ctx->clip_area);
        lv_gpu_acts_timeline_submit();
        return LV_RES_OK;
    }

    return LV_RES_INV;
}

static lv_res_t _lv_gpu_dma2d_blit(
        lv_draw_ctx_t * draw_ctx, const lv_img_dsc_t * src,
        const lv_area_t * src_area, lv_color_t color, lv_opa_t opa,
        bool is_bitstream)
{
    bool has_alpha = (opa < LV_OPA_MAX) || lv_img_cf_has_alpha(src->header.cf);
    int16_t copy_w = lv_area_get_width(draw_ctx->clip_area);
    int16_t copy_h = lv_area_get_height(draw_ctx->clip_area);
    const uint8_t *src_data = src->data;
    uint32_t src_pitch_bits, src_width_bits;
    uint8_t src_px_size;
    int res = 0;

    int32_t dest_stride = lv_area_get_width(draw_ctx->buf_area);
    lv_color_t *dest = (lv_color_t *)draw_ctx->buf +
            (draw_ctx->clip_area->y1 - draw_ctx->buf_area->y1) * dest_stride +
                (draw_ctx->clip_area->x1 - draw_ctx->buf_area->x1);

    if ((uint32_t)copy_w * copy_h < LV_GPU_ACTS_DMA2D_SIZE_LIMIT) {
        LV_LOG_INFO("size %ux%u too small", copy_w, copy_h);
        return LV_RES_INV;
    }

    /* configure foreground layer */
    hdma2d.layer_cfg[1].color_format = lvgl_img_cf_to_display(src->header.cf, &src_px_size);
    if (hdma2d.layer_cfg[1].color_format == 0)
        return LV_RES_INV;

    src_width_bits = src->header.w * src_px_size;
    src_pitch_bits = is_bitstream ? src_width_bits : ((src_width_bits + 7) & ~0x7);

    if (src_area) {
        uint32_t bit_ofs = src_pitch_bits * src_area->y1 + src_area->x1 * src_px_size;
        uint32_t offset = 0;

        if (hdma2d.layer_cfg[1].color_format & ABMP_BITS_PIXEL_FORMATS) {
            offset = (bit_ofs & 0x7) >> (src_px_size >> 1); /* src_px_size: 1, 2, 4 */
        }

        src_data += bit_ofs / 8;
        hdma2d.layer_cfg[1].input_xofs = offset;
        hdma2d.layer_cfg[1].input_width = lv_area_get_width(src_area);
        hdma2d.layer_cfg[1].input_height = lv_area_get_height(src_area);
    } else {
        hdma2d.layer_cfg[1].input_xofs = 0;
        hdma2d.layer_cfg[1].input_width = src->header.w;
        hdma2d.layer_cfg[1].input_height = src->header.h;
    }

    if (is_bitstream && (src_width_bits & 0x7)) {
        /* Can't describe the bit stream */
        if (hdma2d.layer_cfg[1].input_xofs > 0 || (hdma2d.layer_cfg[1].input_width != src->header.w))
            return LV_RES_INV;

        /* Contiguous bit stream */
        hdma2d.layer_cfg[1].input_pitch = 0;
    } else {
        hdma2d.layer_cfg[1].input_pitch = src_pitch_bits >> 3;
    }

    hdma2d.layer_cfg[1].alpha_mode = HAL_DMA2D_COMBINE_ALPHA;
    hdma2d.layer_cfg[1].input_alpha = ((uint32_t)opa << 24) | (lv_color_to32(color) & 0xFFFFFF);
    hal_dma2d_config_layer(&hdma2d, HAL_DMA2D_FOREGROUND_LAYER);

    if (hdma2d.layer_cfg[1].color_format & INDEX_PIXEL_FORMATS) {
        uint32_t * palette = (uint32_t *)src->data;
        uint32_t palette_size;

        if (src->header.cf == LV_IMG_CF_INDEXED_8BIT_ACTS) {
            palette_size = *palette++;
            if (palette_size == 0 || palette_size > 256) {
                LV_LOG_ERROR("invalid palette size %u", palette_size);
                return LV_RES_INV;
            }

            src_data += palette_size * 4 + 4;
        } else {
            palette_size = 1 << src_px_size;
            src_data += palette_size * 4;
        }

        /* CARE: use original src pointer */
        res = hal_dma2d_clut_load_start(&hdma2d, 1, palette_size, palette);
        if (res >= 0)  {
            lv_gpu_acts_timeline_submit();
        } else {
            return LV_RES_INV;
        }
    }

    /* configure the output */
    hdma2d.output_cfg.mode = has_alpha ? HAL_DMA2D_M2M_BLEND : HAL_DMA2D_M2M;
    hal_dma2d_config_output(&hdma2d);

    /* configure background layer */
    if (has_alpha) {
        hdma2d.layer_cfg[0].input_width = copy_w;
        hdma2d.layer_cfg[0].input_height = copy_h;
        hal_dma2d_config_layer(&hdma2d, HAL_DMA2D_BACKGROUND_LAYER);
    }

    /* handle the cache */
    if (!lvgl_img_buf_is_clean(src_data, 0)) {
        /* Add 1 byte in case that original src_data is not byte aligned */
        mem_dcache_clean(src_data, (hdma2d.layer_cfg[1].input_height *
                src_pitch_bits + 15) / 8);
        mem_dcache_sync();
    }

    lv_gpu_acts_set_accl_type(draw_ctx, ACCL_DMA2D, draw_ctx->clip_area);

    if (has_alpha) {
        res = hal_dma2d_blending_start(&hdma2d, (uint32_t)src_data,
                        (uint32_t)dest, (uint32_t)dest, copy_w, copy_h);
    } else {
        res = hal_dma2d_start(&hdma2d, (uint32_t)src_data, (uint32_t)dest, copy_w, copy_h);
    }

    if (res >= 0) {
        lv_gpu_acts_flush_dcache(draw_ctx, draw_ctx->clip_area);
        lv_gpu_acts_timeline_submit();
        return LV_RES_OK;
    }

    return LV_RES_INV;
}

static lv_res_t _lv_gpu_dma2d_zoom_img_var(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src)
{
    lv_area_t draw_area, src_area;
    lv_res_t res;

    if (lv_gpu_acts_get_img_transformed_area(draw_ctx, draw_dsc, &draw_area, &src_area, coords) == LV_RES_INV)
        return LV_RES_OK;

    if (lv_draw_mask_is_any(&draw_area))
        return LV_RES_INV;

    const lv_area_t *clip_area_ori = draw_ctx->clip_area;
    draw_ctx->clip_area = &draw_area;

    res = _lv_gpu_dma2d_blit(draw_ctx, src, &src_area, draw_dsc->recolor, draw_dsc->opa, false);

    draw_ctx->clip_area = clip_area_ori;
    return res;
}

static lv_res_t _lv_gpu_dma2d_rotate_img_var(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src)
{
    bool has_alpha = (draw_dsc->opa < LV_OPA_MAX) || lv_img_cf_has_alpha(src->header.cf);
    int32_t dest_stride = lv_area_get_width(draw_ctx->buf_area);
    int16_t draw_w, draw_h;
    lv_area_t draw_area;
    uint8_t src_px_size;
    uint32_t src_hal_cf;
    int res;

    if (draw_dsc->zoom < LV_IMG_ZOOM_NONE)
        return LV_RES_INV;

    src_hal_cf = lvgl_img_cf_to_display(src->header.cf, &src_px_size);
    if (!(src_hal_cf & (HAL_PIXEL_FORMAT_RGB_565 |
                HAL_PIXEL_FORMAT_XRGB_8888 | HAL_PIXEL_FORMAT_ARGB_8888)))
        return LV_RES_INV;

    int16_t draw16_x1, draw16_y1, draw16_x2, draw16_y2;
    sw_transform_area16(&draw16_x1, &draw16_y1, &draw16_x2, &draw16_y2,
            coords->x1, coords->y1, coords->x2, coords->y2,
            coords->x1 + draw_dsc->pivot.x, coords->y1 + draw_dsc->pivot.y,
            draw_dsc->angle, draw_dsc->zoom, draw_dsc->zoom, 8);

    draw_area.x1 = draw16_x1;
    draw_area.y1 = draw16_y1;
    draw_area.x2 = draw16_x2;
    draw_area.y2 = draw16_y2;

    if (_lv_area_intersect(&draw_area, draw_ctx->clip_area, &draw_area) == false)
        return LV_RES_OK;

    if (lv_draw_mask_is_any(&draw_area))
        return LV_RES_INV;

    draw_w = lv_area_get_width(&draw_area);
    draw_h = lv_area_get_height(&draw_area);
    if ((uint32_t)draw_w * draw_h < LV_GPU_ACTS_DMA2D_SIZE_LIMIT) {
        LV_LOG_INFO("size %ux%u too small", draw_w, draw_h);
        return LV_RES_INV;
    }

    /* configure the output */
    hdma2d.output_cfg.mode = has_alpha ? HAL_DMA2D_M2M_TRANSFORM_BLEND : HAL_DMA2D_M2M_TRANSFORM;
    hal_dma2d_config_output(&hdma2d);

    /* configure the foreground */
    hdma2d.layer_cfg[1].color_format = src_hal_cf;
    hdma2d.layer_cfg[1].input_pitch = src->header.w * src_px_size / 8;
    hdma2d.layer_cfg[1].input_width = src->header.w;
    hdma2d.layer_cfg[1].input_height = src->header.h;
    hdma2d.layer_cfg[1].alpha_mode = HAL_DMA2D_COMBINE_ALPHA;
    hdma2d.layer_cfg[1].input_alpha = (uint32_t)draw_dsc->opa << 24;
    hal_dma2d_config_layer(&hdma2d, HAL_DMA2D_FOREGROUND_LAYER);

    hdma2d.trans_cfg.angle = draw_dsc->angle;
    hdma2d.trans_cfg.image_x0 = coords->x1;
    hdma2d.trans_cfg.image_y0 = coords->y1;
    hdma2d.trans_cfg.rect.pivot_x = draw_dsc->pivot.x;
    hdma2d.trans_cfg.rect.pivot_y = draw_dsc->pivot.y;
    hdma2d.trans_cfg.rect.scale_x = draw_dsc->zoom;
    hdma2d.trans_cfg.rect.scale_y = draw_dsc->zoom;
    res = hal_dma2d_config_transform(&hdma2d);
    if (res < 0)
        return LV_RES_INV;

    /* handle the cache */
    if (!lvgl_img_buf_is_clean(src->data, 0)) {
        mem_dcache_clean(src->data, (uint32_t)hdma2d.layer_cfg[1].input_pitch *
                hdma2d.layer_cfg[1].input_height);
        mem_dcache_sync();
    }

    lv_color_t *dest = (lv_color_t *)draw_ctx->buf +
            dest_stride * (draw_area.y1 - draw_ctx->buf_area->y1) +
                (draw_area.x1 - draw_ctx->buf_area->x1);

    lv_gpu_acts_set_accl_type(draw_ctx, ACCL_DMA2D, &draw_area);

    res = hal_dma2d_transform_start(&hdma2d, (uint32_t)src->data, (uint32_t)dest,
            draw_area.x1, draw_area.y1, draw_w, draw_h);
    if (res >= 0) {
        lv_gpu_acts_flush_dcache(draw_ctx, &draw_area);
        lv_gpu_acts_timeline_submit();
        return LV_RES_OK;
    }

    return LV_RES_INV;
}

#endif /*LV_USE_GPU_ACTS_DMA2D*/
