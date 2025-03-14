/**
 * @file lv_draw_vglite_blend.c
 *
 */

/**
 * MIT License
 *
 * Copyright 2020-2023 NXP
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next paragraph)
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_vglite_blend.h"

#if LV_USE_GPU_ACTS_VG_LITE
#include "lv_vglite_buf.h"
#include "lv_vglite_utils.h"

/*********************
 *      DEFINES
 *********************/

/* Workaround for vg_lite_blit() filter whose center located outside of image */
#define USE_WORKAROUND_FOR_BLIT_TRANSFORM 1

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**
 * Blit image, with optional opacity.
 *
 * @param[in] dest_area Area with relative coordinates of destination buffer
 * @param[in] src_area Source area with relative coordinates of source buffer
 * @param[in] color color value
 * @param[in] opa Opacity
 * @param[in] chroma_keyed Chroma keyed flag
 * @param[in] blend_mode Blend mode
 *
 * @retval LV_RES_OK Transfer complete
 * @retval LV_RES_INV Error occurred (\see LV_GPU_NXP_VG_LITE_LOG_ERRORS)
 */
static lv_res_t lv_vglite_blit(const lv_area_t * dest_area, const lv_area_t * src_area,
                               lv_color_t color, lv_opa_t opa, bool chroma_keyed,
                               lv_blend_mode_t blend_mode);

/**
 * Creates matrix that translates to origin of given destination area.
 *
 * @param[in] dest_area Area with relative coordinates of destination buffer
 */
static inline void lv_vglite_set_translation_matrix(const lv_area_t * dest_area);

/**
 * Creates matrix that translates to origin of given destination area with transformation (scale or rotate).
 *
 * @param[in] dest_area Area with relative coordinates of destination buffer
 * @param[in] dsc Image descriptor
 */
static inline void lv_vglite_set_transformation_matrix(const lv_area_t * dest_area, const lv_draw_img_dsc_t * dsc);

/**********************
 *  STATIC VARIABLES
 **********************/

static vg_lite_matrix_t vgmatrix;
static vg_lite_filter_t vgfilter;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_res_t lv_gpu_vglite_fill(const lv_area_t * dest_area, lv_color_t color, lv_opa_t opa, lv_blend_mode_t blend_mode)
{
    vg_lite_error_t err = VG_LITE_SUCCESS;
    lv_color32_t col32 = {.full = lv_color_to32(color)}; /*Convert color to RGBA8888*/
    vg_lite_color_t vgcol; /* vglite takes ABGR */
    vg_lite_buffer_t * vgbuf = lv_vglite_get_dest_buf();

    vg_lite_buffer_format_t color_format = VG_LITE_RGBA8888;
    if(lv_vglite_premult_and_swizzle(&vgcol, col32, opa, color_format) != LV_RES_OK)
        VG_LITE_RETURN_INV("Premultiplication and swizzle failed.");

    if(opa >= (lv_opa_t)LV_OPA_MAX && blend_mode == LV_BLEND_MODE_NORMAL) { /*Opaque fill*/
        vg_lite_rectangle_t rect = {
            .x = dest_area->x1,
            .y = dest_area->y1,
            .width = lv_area_get_width(dest_area),
            .height = lv_area_get_height(dest_area)
        };

        err = vg_lite_clear(vgbuf, &rect, vgcol);
        VG_LITE_ERR_RETURN_INV(err, "Clear failed.");

        if(lv_vglite_run(dest_area) != LV_RES_OK)
            VG_LITE_RETURN_INV("Run failed.");
    }
    else {   /*fill with transparency*/

        vg_lite_path_t path;
        int16_t path_data[] = { /*VG rectangular path*/
            VLC_OP_MOVE, dest_area->x1,  dest_area->y1,
            VLC_OP_LINE, dest_area->x2 + 1,  dest_area->y1,
            VLC_OP_LINE, dest_area->x2 + 1,  dest_area->y2 + 1,
            VLC_OP_LINE, dest_area->x1,  dest_area->y2 + 1,
            VLC_OP_LINE, dest_area->x1,  dest_area->y1,
            VLC_OP_END
        };

        /* Choose vglite blend mode based on given lvgl blend mode */
        vg_lite_blend_t vglite_blend_mode = lv_vglite_get_blend_mode(blend_mode);

        err = vg_lite_init_path(&path, VG_LITE_S16, VG_LITE_LOW, sizeof(path_data), path_data,
                                (vg_lite_float_t) dest_area->x1, (vg_lite_float_t) dest_area->y1,
                                ((vg_lite_float_t) dest_area->x2) + 1.0f, ((vg_lite_float_t) dest_area->y2) + 1.0f);
        VG_LITE_ERR_RETURN_INV(err, "Init path failed.");

        /*Draw rectangle*/
        err = vg_lite_draw(vgbuf, &path, VG_LITE_FILL_EVEN_ODD, NULL, vglite_blend_mode, vgcol);
        VG_LITE_ERR_RETURN_INV(err, "Draw rectangle failed.");

        if(lv_vglite_run(dest_area) != LV_RES_OK)
            VG_LITE_RETURN_INV("Run failed.");

        err = vg_lite_clear_path(&path);
        VG_LITE_ERR_RETURN_INV(err, "Clear path failed.");
    }

    return LV_RES_OK;
}

lv_res_t lv_gpu_vglite_blit(lv_area_t * dest_area, const lv_color_t * src_buf,
                            lv_area_t * src_area, lv_coord_t src_stride,
                            lv_img_cf_t src_cf, const lv_draw_sw_blend_dsc_t * dsc)
{
    /* Set src_vgbuf structure. */
    if (LV_RES_OK != lv_vglite_set_src_buf(src_buf, src_cf, src_area, src_stride))
        return LV_RES_INV;

    /* Set vgmatrix. */
    lv_vglite_set_translation_matrix(src_area);

    /* Start blit. */
    return lv_vglite_blit(dest_area, src_area, dsc->color, dsc->opa,
                          src_cf == LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED,
                          dsc->blend_mode);
}

lv_res_t lv_gpu_vglite_blit_transform(lv_area_t * dest_area, const lv_color_t * src_buf,
                                      lv_area_t * src_area, lv_coord_t src_stride,
                                      lv_img_cf_t src_cf, const lv_draw_img_dsc_t * dsc)
{
    /* Set src_vgbuf structure. */
    if (LV_RES_OK != lv_vglite_set_src_buf(src_buf, src_cf, src_area, src_stride))
        return LV_RES_INV;

    /* Set vgmatrix. */
    lv_vglite_set_transformation_matrix(src_area, dsc);

    /* Start blit. */
    return lv_vglite_blit(dest_area, src_area, dsc->recolor, dsc->opa,
                          src_cf == LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED,
                          dsc->blend_mode);
}

lv_res_t lv_gpu_vglite_buffer_copy(lv_color_t * dest_buf, const lv_area_t * dest_area, lv_coord_t dest_stride,
                                   const lv_color_t * src_buf, const lv_area_t * src_area, lv_coord_t src_stride)
{
    vg_lite_error_t err = VG_LITE_SUCCESS;

    /* Set src_vgbuf structure. */
    vg_lite_buffer_t src_vgbuf;
    memset(&src_vgbuf, 0, sizeof(src_vgbuf));
    lv_vglite_set_buf(&src_vgbuf, src_buf, LV_IMG_CF_TRUE_COLOR, src_area, src_stride);

    /* Set dest_vgbuf structure. */
    vg_lite_buffer_t dest_vgbuf;
    memset(&dest_vgbuf, 0, sizeof(dest_vgbuf));
    lv_vglite_set_buf(&dest_vgbuf, dest_buf, LV_IMG_CF_TRUE_COLOR, dest_area, dest_stride);

    vg_lite_rectangle_t rect = {
        .x = src_area->x1, /* start x */
        .y = src_area->y1, /* start y */
        .width = lv_area_get_width(src_area), /* width */
        .height = lv_area_get_height(src_area) /* height */
    };

    /* Set scissor. */
    lv_vglite_set_scissor(dest_area);

    /* Set vgmatrix. */
    lv_vglite_set_translation_matrix(dest_area);

    err = vg_lite_blit_rect(&dest_vgbuf, &src_vgbuf, &rect, &vgmatrix,
                            VG_LITE_BLEND_NONE, 0, VG_LITE_FILTER_POINT);
    if(err != VG_LITE_SUCCESS) {
        LV_LOG_ERROR("Blit rectangle failed.");
        /* Disable scissor. */
        lv_vglite_disable_scissor();

        return LV_RES_INV;
    }

    if(lv_vglite_run(dest_area) != LV_RES_OK) {
        LV_LOG_ERROR("Run failed.");
        /* Disable scissor. */
        lv_vglite_disable_scissor();

        return LV_RES_INV;
    }

    /* Disable scissor. */
    lv_vglite_disable_scissor();

    return LV_RES_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_res_t lv_vglite_blit(const lv_area_t * dest_area, const lv_area_t * src_area,
                               lv_color_t color, lv_opa_t opa, bool chroma_keyed,
                               lv_blend_mode_t blend_mode)
{
    vg_lite_error_t err = VG_LITE_SUCCESS;
    vg_lite_buffer_t * dst_vgbuf = lv_vglite_get_dest_buf();
    vg_lite_buffer_t * src_vgbuf = lv_vglite_get_src_buf();
    vg_lite_color_t vgcol = 0xFFFFFFFFU; /* vglite takes ABGR */
    vg_lite_blend_t blend = VG_LITE_BLEND_NONE;

    if(chroma_keyed && lv_vglite_set_color_key(true) == LV_RES_INV) {
        return LV_RES_INV;
    }

    if(chroma_keyed || opa < LV_OPA_MAX || blend_mode != LV_BLEND_MODE_NORMAL ||
        lv_vglite_buf_has_alpha(src_vgbuf)) {
        src_vgbuf->transparency_mode = VG_LITE_IMAGE_TRANSPARENT;

        /* Choose vglite blend mode based on given lvgl blend mode */
        blend = lv_vglite_get_blend_mode(blend_mode);

        if(src_vgbuf->format == VG_LITE_A8 || opa < (lv_opa_t)LV_OPA_MAX) {
            src_vgbuf->image_mode = VG_LITE_MULTIPLY_IMAGE_MODE;

            lv_color32_t col32;
            col32.full = (src_vgbuf->format == VG_LITE_A8) ? lv_color_to32(color) : 0xFFFFFFFFU;

            vgcol = ((uint32_t)opa << 24) | ((uint32_t)col32.ch.blue << 16) |
                    ((uint32_t)col32.ch.green << 8) | col32.ch.red;
        }
    }

    /* Set scissor. */
    lv_vglite_set_scissor(dest_area);

    /* For VG_LITE_RGBA8888_ETC2_EAC, there maybe padded pixels. */
    vg_lite_rectangle_t src_rect = { 0, 0, lv_area_get_width(src_area), lv_area_get_height(src_area) };

#if USE_WORKAROUND_FOR_BLIT_TRANSFORM
    if (vgfilter != VG_LITE_FILTER_POINT && blend == VG_LITE_BLEND_NONE) {
        vg_lite_path_t path;
        int16_t path_data[13] = {
            VLC_OP_MOVE, 0, 0,
            VLC_OP_LINE, src_rect.width, 0,
            VLC_OP_LINE, src_rect.width, src_rect.height,
            VLC_OP_LINE, 0, src_rect.height,
            VLC_OP_END,
        };

        err = vg_lite_init_path(&path, VG_LITE_S16, VG_LITE_MEDIUM, sizeof(path_data),
                path_data, 0, 0, src_rect.width, src_rect.height);
        VG_LITE_ERR_RETURN_INV(err, "Init path failed.");

        err = vg_lite_draw_pattern(dst_vgbuf, &path, VG_LITE_FILL_EVEN_ODD, &vgmatrix,
                    src_vgbuf, &vgmatrix, blend, VG_LITE_PATTERN_PAD, 0, vgcol, vgfilter);
    } else {
        err = vg_lite_blit_rect(dst_vgbuf, src_vgbuf, &src_rect, &vgmatrix, blend, vgcol, vgfilter);
    }

#else
    err = vg_lite_blit_rect(dst_vgbuf, src_vgbuf, &src_rect, &vgmatrix, blend, vgcol, vgfilter);
#endif /* USE_WORKAROUND_FOR_BLIT_TRANSFORM */

    VG_LITE_ERR_RETURN_INV(err, "Blit rectangle failed.");

    if(lv_vglite_run(dest_area) != LV_RES_OK) {
        VG_LITE_RETURN_INV("Run failed.");
    }

    /* Disable scissor. */
    lv_vglite_disable_scissor();

    if(chroma_keyed) {
        lv_vglite_set_color_key(false);
    }

    return LV_RES_OK;
}

static inline void lv_vglite_set_translation_matrix(const lv_area_t * dest_area)
{
    vg_lite_identity(&vgmatrix);
    vg_lite_translate((vg_lite_float_t)dest_area->x1, (vg_lite_float_t)dest_area->y1, &vgmatrix);

    vgfilter = VG_LITE_FILTER_POINT;
}

static inline void lv_vglite_set_transformation_matrix(const lv_area_t * dest_area, const lv_draw_img_dsc_t * dsc)
{
    lv_vglite_set_translation_matrix(dest_area);

    bool has_scale = (dsc->zoom != LV_IMG_ZOOM_NONE);
    bool has_rotation = (dsc->angle != 0);

    if(has_scale || has_rotation) {
        vg_lite_translate(dsc->pivot.x, dsc->pivot.y, &vgmatrix);
        if(has_rotation)
            vg_lite_rotate(dsc->angle / 10.0f, &vgmatrix);   /* angle is 1/10 degree */
        if(has_scale) {
            vg_lite_float_t scale = 1.0f * dsc->zoom / LV_IMG_ZOOM_NONE;
            vg_lite_scale(scale, scale, &vgmatrix);
        }
        vg_lite_translate(0.0f - dsc->pivot.x, 0.0f - dsc->pivot.y, &vgmatrix);

        vgfilter = VG_LITE_FILTER_BI_LINEAR;
    }
}

#endif /*LV_USE_GPU_ACTS_VG_LITE*/
