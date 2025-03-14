/**
 * @file lv_vglite_utils.c
 *
 */

/**
 * MIT License
 *
 * Copyright 2022, 2023 NXP
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

#include "../lv_gpu_acts.h"
#include "lv_vglite_utils.h"

#if LV_USE_GPU_ACTS_VG_LITE
#include "../../../src/core/lv_refr.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**
 * Clean and invalidate cache.
 */
static inline void invalidate_cache(const lv_area_t * area);

/**********************
 *  STATIC VARIABLES
 **********************/
static vg_lite_color_key4_t vgcolorkey = { 0 };

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_res_t lv_vglite_run(const lv_area_t * dest_area)
{
    invalidate_cache(dest_area);

    VG_LITE_ERR_RETURN_INV(vg_lite_flush(), "Flush failed.");

    return LV_RES_OK;
}

lv_res_t lv_vglite_run_finish(const lv_area_t * dest_area)
{
    invalidate_cache(dest_area);

    VG_LITE_ERR_RETURN_INV(vg_lite_finish(), "Finish failed.");

    return LV_RES_OK;
}

lv_res_t lv_vglite_set_color_key(bool enabled)
{
    if(enabled) {
        lv_color_t color = lv_color_chroma_key();

#if LV_COLOR_DEPTH == 32
        vgcolorkey[0].low_r = color.ch.red;
        vgcolorkey[0].low_g = color.ch.green;
        vgcolorkey[0].low_b = color.ch.blue;
        vgcolorkey[0].hign_r = color.ch.red;
        vgcolorkey[0].hign_g = color.ch.green;
        vgcolorkey[0].hign_b = color.ch.blue;
#else
        vgcolorkey[0].low_r = (color.ch.red << 3);
        vgcolorkey[0].low_g = (color.ch.green << 2);
        vgcolorkey[0].low_b = (color.ch.blue << 3);
        vgcolorkey[0].hign_r = (color.ch.red << 3) | 0x7;
        vgcolorkey[0].hign_g = (color.ch.green << 2) | 0x3;
        vgcolorkey[0].hign_b = (color.ch.blue << 3) | 0x7;
#endif

        vgcolorkey[0].enable = 1;
    }
    else {
        vgcolorkey[0].enable = 0;
    }

    return (VG_LITE_SUCCESS == vg_lite_set_color_key(vgcolorkey)) ? LV_RES_OK : LV_RES_INV;
}

lv_res_t lv_vglite_premult_and_swizzle(vg_lite_color_t * vg_col32, lv_color32_t lv_col32, lv_opa_t opa,
                                       vg_lite_buffer_format_t vg_col_format)
{
    if(opa < (lv_opa_t)LV_OPA_MAX) {
        lv_col32.ch.alpha = opa;
    }

    switch(vg_col_format) {
        case VG_LITE_BGRA8888:
            *vg_col32 = lv_col32.full;
            break;
        case VG_LITE_RGBA8888:
            *vg_col32 = ((uint32_t)lv_col32.ch.alpha << 24) | ((uint32_t)lv_col32.ch.blue << 16) |
                        ((uint32_t)lv_col32.ch.green << 8) | (uint32_t)lv_col32.ch.red;
            break;
        case VG_LITE_ABGR8888:
            *vg_col32 = ((uint32_t)lv_col32.ch.red << 24) | ((uint32_t)lv_col32.ch.green << 16) |
                        ((uint32_t)lv_col32.ch.blue << 8) | (uint32_t)lv_col32.ch.alpha;
            break;
        case VG_LITE_ARGB8888:
            *vg_col32 = ((uint32_t)lv_col32.ch.alpha << 24) | ((uint32_t)lv_col32.ch.red << 16) |
                        ((uint32_t)lv_col32.ch.green << 8) | (uint32_t)lv_col32.ch.blue;
            break;
        default:
            return LV_RES_INV;
    }

    return LV_RES_OK;
}

vg_lite_blend_t lv_vglite_get_blend_mode(lv_blend_mode_t lv_blend_mode)
{
    vg_lite_blend_t vg_blend_mode;
    switch(lv_blend_mode) {
        case LV_BLEND_MODE_NORMAL:
            vg_blend_mode = VG_LITE_BLEND_PREMULTIPLY_SRC_OVER; /* OPENVG_BLEND_SRC_OVER */
            break;
        case LV_BLEND_MODE_MULTIPLY:
            vg_blend_mode = OPENVG_BLEND_MULTIPLY;
            break;
        case LV_BLEND_MODE_ADDITIVE:
        case LV_BLEND_MODE_REPLACE:
        case LV_BLEND_MODE_SUBTRACTIVE: /* unsupported */
        default:
            vg_blend_mode = VG_LITE_BLEND_NONE;
            break;
    }
    return vg_blend_mode;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static inline void invalidate_cache(const lv_area_t * area)
{
    lv_disp_t * disp = _lv_refr_get_disp_refreshing();
#if 0
    if(disp->driver->clean_dcache_cb)
        disp->driver->clean_dcache_cb(disp->driver);
#else
    lv_draw_ctx_t * draw_ctx = disp->driver->draw_ctx;
    lv_area_t inv_area;

    if (area != NULL) {
        inv_area.x1 = area->x1 + draw_ctx->buf_area->x1;
        inv_area.y1 = area->y1 + draw_ctx->buf_area->y1;
        inv_area.x2 = area->x2 + draw_ctx->buf_area->x1;
        inv_area.y2 = area->y2 + draw_ctx->buf_area->y1;

        area = &inv_area;
    } else {
        area = draw_ctx->buf_area;
    }

    lv_gpu_acts_set_accl_type(draw_ctx, ACCL_VGLITE, area);
    lv_gpu_acts_flush_dcache(draw_ctx, area);
#endif
}

#endif /*LV_USE_GPU_ACTS_VG_LITE*/
