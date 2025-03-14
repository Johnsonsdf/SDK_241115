/**
 * @file lv_draw_vglite.c
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
#include "lv_draw_vglite.h"

#if LV_USE_GPU_ACTS_VG_LITE
#include <math.h>
#include "lv_draw_vglite_blend.h"
#include "lv_draw_vglite_line.h"
#include "lv_draw_vglite_rect.h"
#include "lv_draw_vglite_arc.h"
#include "lv_draw_vglite_glyphs.h"
#include "lv_vglite_buf.h"

/*********************
 *      DEFINES
 *********************/

#define LV_GPU_ACTS_VG_LITE_DRAW_BORDER 0

#ifndef __unused
#  ifdef _WIN32
#    define __unused
#  else
#    define __unused __attribute__((__unused__))
#  endif
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static lv_res_t lv_draw_vglite_bg(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords);

static lv_res_t lv_draw_vglite_border(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc,
                                      const lv_area_t * coords);

static lv_res_t lv_draw_vglite_outline(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc,
                                       const lv_area_t * coords);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int lv_draw_vglite_init(void)
{
    vg_lite_set_command_buffer_size(LV_GPU_ACTS_VG_LITE_COMMAND_BUFFER_SIZE << 10);

    if (VG_LITE_SUCCESS != vg_lite_init(DEF_SCREEN_W, DEF_SCREEN_H))
        return -1;

    return 0;
}

void lv_draw_vglite_init_buf(lv_draw_ctx_t * draw_ctx, bool screen_transp)
{
    lv_gpu_vglite_init_buf(draw_ctx->buf, screen_transp,
            draw_ctx->buf_area, lv_area_get_width(draw_ctx->buf_area));
}

void lv_draw_vglite_wait_for_finish(lv_draw_ctx_t * draw_ctx)
{
    vg_lite_finish();
}

lv_res_t lv_draw_vglite_buffer_copy(void * dest_buf, lv_coord_t dest_stride, const lv_area_t * dest_area,
                                    void * src_buf, lv_coord_t src_stride, const lv_area_t * src_area)
{
    lv_res_t res = LV_RES_INV;
    vg_lite_error_t err;
    vg_lite_buffer_t dest_vgbuf;
    vg_lite_buffer_t src_vgbuf;
    lv_area_t buf_area = { 0, 0, dest_stride - 1, dest_area->y2, };

    memset(&dest_vgbuf, 0, sizeof(dest_vgbuf));

    res = lv_vglite_set_buf(&dest_vgbuf, dest_buf, LV_IMG_CF_TRUE_COLOR, &buf_area, dest_stride);
    if (res != LV_RES_OK)
        return res;

    buf_area.x2 = src_stride - 1;
    buf_area.y2 = src_area->y2;
    memset(&src_vgbuf, 0, sizeof(src_vgbuf));

    res = lv_vglite_set_buf(&src_vgbuf, src_buf, LV_IMG_CF_TRUE_COLOR, &buf_area, src_stride);
    if (res != LV_RES_OK) {
        vg_lite_unmap(&dest_vgbuf);
        return res;
    }

    vg_lite_matrix_t matrix;
    vg_lite_identity(&matrix);
    vg_lite_translate(src_area->x1, src_area->y1, &matrix);

    /* Set scissor. */
    lv_vglite_set_scissor(dest_area);

    err = vg_lite_blit(&dest_vgbuf, &src_vgbuf, &matrix, VG_LITE_BLEND_NONE, 0, VG_LITE_FILTER_POINT);
    VG_LITE_ERR_RETURN_INV(err, "Blit rectangle failed.");

    if(lv_vglite_run(dest_area) != LV_RES_OK) {
        VG_LITE_RETURN_INV("Run failed.");
    }

    lv_vglite_disable_scissor();

    vg_lite_unmap(&dest_vgbuf);
    vg_lite_unmap(&src_vgbuf);
    return LV_RES_OK;
}

lv_res_t lv_draw_vglite_img_dsc(lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
                                const lv_area_t * coords, const lv_img_dsc_t * src)
{
    lv_res_t res = LV_RES_INV;
    lv_area_t map_area_rot;
    lv_area_t blend_area;

    if(draw_dsc->opa <= (lv_opa_t)LV_OPA_MIN)
        return LV_RES_OK;

    if(draw_dsc->blend_mode == LV_BLEND_MODE_SUBTRACTIVE)
        return LV_RES_INV;

    if(lv_vglite_get_dest_buf() == NULL)
        return LV_RES_INV;

    if(draw_dsc->angle || draw_dsc->zoom != LV_IMG_ZOOM_NONE) {
        int32_t w = lv_area_get_width(coords);
        int32_t h = lv_area_get_height(coords);

        _lv_img_buf_get_transformed_area(&map_area_rot, w, h, draw_dsc->angle, draw_dsc->zoom, &draw_dsc->pivot);

        lv_area_move(&map_area_rot, coords->x1, coords->y1);
    }
    else {
        lv_area_copy(&map_area_rot, coords);
    }

    /*Out of mask. There is nothing to draw so the image is drawn successfully.*/
    if (!_lv_area_intersect(&blend_area, draw_ctx->clip_area, &map_area_rot))
        return LV_RES_OK; /*Fully clipped, nothing to do*/

    /*Make the blend area relative to the buffer*/
    lv_area_move(&blend_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    bool has_mask = lv_draw_mask_is_any(&blend_area);
    bool has_recolor = (draw_dsc->recolor_opa > LV_OPA_MIN);

    if(!has_mask && !has_recolor && (src->header.cf == LV_IMG_CF_ETC2_EAC ||
        lv_area_get_size(&blend_area) >= LV_GPU_ACTS_VG_LITE_SIZE_LIMIT)) {
        lv_area_t src_area;
        src_area.x1 = coords->x1 - draw_ctx->buf_area->x1;
        src_area.y1 = coords->y1 - draw_ctx->buf_area->y1;
        src_area.x2 = src_area.x1 + lv_area_get_width(coords) - 1;
        src_area.y2 = src_area.y1 + lv_area_get_height(coords) - 1;
        lv_coord_t src_stride = lv_area_get_width(coords);

        res = lv_gpu_vglite_blit_transform(&blend_area, (void *)src->data, &src_area,
                                           src_stride, src->header.cf, draw_dsc);
        if(res != LV_RES_OK)
            VG_LITE_LOG_TRACE("VG-Lite blit transform failed. Fallback.");
    }

    return res;
}

lv_res_t lv_draw_vglite_blend_basic(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc)
{
    lv_res_t res = LV_RES_INV;

    if(dsc->opa <= (lv_opa_t)LV_OPA_MIN)
        return LV_RES_OK;

    if(dsc->blend_mode == LV_BLEND_MODE_SUBTRACTIVE)
        return LV_RES_INV;

    if(lv_vglite_get_dest_buf() == NULL)
        return LV_RES_INV;

    lv_area_t blend_area;
    /*Let's get the blend area which is the intersection of the area to draw and the clip area*/
    if(!_lv_area_intersect(&blend_area, dsc->blend_area, draw_ctx->clip_area))
        return LV_RES_OK; /*Fully clipped, nothing to do*/

    /*Make the blend area relative to the buffer*/
    lv_area_move(&blend_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    /*Fill/Blend only non masked, normal blended*/
    if(dsc->mask_buf == NULL && lv_area_get_size(&blend_area) >= LV_GPU_ACTS_VG_LITE_SIZE_LIMIT) {
        const lv_color_t * src_buf = dsc->src_buf;

        if(src_buf == NULL) {
            res = lv_gpu_vglite_fill(&blend_area, dsc->color, dsc->opa, dsc->blend_mode);
            if(res != LV_RES_OK)
                VG_LITE_LOG_TRACE("VG-Lite fill failed. Fallback.");
        }
        else {
            lv_area_t src_area;
            src_area.x1 = dsc->blend_area->x1 - draw_ctx->buf_area->x1;
            src_area.y1 = dsc->blend_area->y1 - draw_ctx->buf_area->y1;
            src_area.x2 = src_area.x1 + lv_area_get_width(dsc->blend_area) - 1;
            src_area.y2 = src_area.y1 + lv_area_get_height(dsc->blend_area) - 1;
            lv_coord_t src_stride = lv_area_get_width(dsc->blend_area);

            res = lv_gpu_vglite_blit(&blend_area, src_buf, &src_area, src_stride,
                                     LV_IMG_CF_TRUE_COLOR, dsc);
            if(res != LV_RES_OK)
                VG_LITE_LOG_TRACE("VG-Lite blit failed. Fallback.");
        }
    }

    return res;
}

lv_res_t lv_draw_vglite_letter(lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,
                               const lv_point_t * pos_p, const lv_area_t * gbox_p, const uint8_t * map_p)
{
    if(dsc->blend_mode == LV_BLEND_MODE_SUBTRACTIVE)
        return LV_RES_INV;

    if(lv_vglite_get_dest_buf() == NULL)
        return LV_RES_INV;

    lv_area_t dest_area;
    if(!_lv_area_intersect(&dest_area, gbox_p, draw_ctx->clip_area))
        return LV_RES_OK; /*Fully clipped, nothing to do*/

    /*Make the blend area relative to the buffer*/
    lv_area_move(&dest_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    lv_point_t rel_pos;
    rel_pos.x = pos_p->x - draw_ctx->buf_area->x1;
    rel_pos.y = pos_p->y - draw_ctx->buf_area->y1 + (dsc->font->line_height - dsc->font->base_line);

    return lv_gpu_vglite_draw_glyphs(&dest_area, (lv_svgfont_bitmap_dsc_t *)map_p, &rel_pos, dsc);
}

void lv_draw_vglite_line(lv_draw_ctx_t * draw_ctx, const lv_draw_line_dsc_t * dsc, const lv_point_t * point1,
                         const lv_point_t * point2)
{
    if(dsc->width == 0)
        return;
    if(dsc->opa <= (lv_opa_t)LV_OPA_MIN)
        return;
    if(point1->x == point2->x && point1->y == point2->y)
        return;

    if(dsc->blend_mode == LV_BLEND_MODE_SUBTRACTIVE || lv_vglite_get_dest_buf() == NULL) {
        lv_draw_sw_line(draw_ctx, dsc, point1, point2);
        return;
    }

    lv_area_t rel_clip_area;
    rel_clip_area.x1 = LV_MIN(point1->x, point2->x) - dsc->width / 2;
    rel_clip_area.x2 = LV_MAX(point1->x, point2->x) + dsc->width / 2;
    rel_clip_area.y1 = LV_MIN(point1->y, point2->y) - dsc->width / 2;
    rel_clip_area.y2 = LV_MAX(point1->y, point2->y) + dsc->width / 2;

    bool is_common;
    is_common = _lv_area_intersect(&rel_clip_area, &rel_clip_area, draw_ctx->clip_area);
    if(!is_common)
        return;

    bool mask_any = lv_draw_mask_is_any(&rel_clip_area);
    if(mask_any) {
        lv_draw_sw_line(draw_ctx, dsc, point1, point2);
        return;
    }

    /* Make coordinates relative to the draw buffer */
    lv_area_move(&rel_clip_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    lv_point_t rel_point1 = { point1->x - draw_ctx->buf_area->x1, point1->y - draw_ctx->buf_area->y1 };
    lv_point_t rel_point2 = { point2->x - draw_ctx->buf_area->x1, point2->y - draw_ctx->buf_area->y1 };

    bool done = false;

    done = (lv_gpu_vglite_draw_line(&rel_point1, &rel_point2, &rel_clip_area, dsc) == LV_RES_OK);
    if(!done) {
        VG_LITE_LOG_TRACE("VG-Lite draw line failed. Fallback.");
        lv_draw_sw_line(draw_ctx, dsc, point1, point2);
    }
}

void lv_draw_vglite_rect(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    if(dsc->blend_mode == LV_BLEND_MODE_SUBTRACTIVE || lv_vglite_get_dest_buf() == NULL) {
        lv_draw_sw_rect(draw_ctx, dsc, coords);
        return;
    }

    lv_draw_rect_dsc_t vglite_dsc;

    lv_memcpy(&vglite_dsc, dsc, sizeof(vglite_dsc));
    vglite_dsc.bg_opa = 0;
    vglite_dsc.bg_img_opa = 0;
    vglite_dsc.border_opa = 0;
    vglite_dsc.outline_opa = 0;
#if LV_DRAW_COMPLEX
    /* Draw the shadow with CPU */
    lv_draw_sw_rect(draw_ctx, &vglite_dsc, coords);
    vglite_dsc.shadow_opa = 0;
#endif /*LV_DRAW_COMPLEX*/

#if LV_GPU_ACTS_VG_LITE_DRAW_BORDER
    /* Draw the background */
    vglite_dsc.bg_opa = dsc->bg_opa;
    if(lv_draw_vglite_bg(draw_ctx, &vglite_dsc, coords) != LV_RES_OK)
        lv_draw_sw_rect(draw_ctx, &vglite_dsc, coords);
    vglite_dsc.bg_opa = 0;

    /* Draw the background image
     * It will be done once draw_ctx->draw_img_decoded()
     * callback gets called from lv_draw_sw_rect().
     */
    vglite_dsc.bg_img_opa = dsc->bg_img_opa;
    lv_draw_sw_rect(draw_ctx, &vglite_dsc, coords);
    vglite_dsc.bg_img_opa = 0;

    /* Draw the border */
    vglite_dsc.border_opa = dsc->border_opa;
    if(lv_draw_vglite_border(draw_ctx, &vglite_dsc, coords) != LV_RES_OK)
        lv_draw_sw_rect(draw_ctx, &vglite_dsc, coords);
    vglite_dsc.border_opa = 0;

    /* Draw the outline */
    vglite_dsc.outline_opa = dsc->outline_opa;
    if(lv_draw_vglite_outline(draw_ctx, &vglite_dsc, coords) != LV_RES_OK)
        lv_draw_sw_rect(draw_ctx, &vglite_dsc, coords);
#else
    /* Draw the background */
    vglite_dsc.bg_opa = dsc->bg_opa;
    if(lv_draw_vglite_bg(draw_ctx, &vglite_dsc, coords) == LV_RES_OK)
        vglite_dsc.bg_opa = 0;

    /* Draw the rest */
    vglite_dsc.bg_img_opa = dsc->bg_img_opa;
    vglite_dsc.border_opa = dsc->border_opa;
    vglite_dsc.outline_opa = dsc->outline_opa;
    lv_draw_sw_rect(draw_ctx, &vglite_dsc, coords);
#endif /* LV_GPU_ACTS_VG_LITE_DRAW_BORDER */
}

void lv_draw_vglite_arc(lv_draw_ctx_t * draw_ctx, const lv_draw_arc_dsc_t * dsc, const lv_point_t * center,
                        uint16_t radius, uint16_t start_angle, uint16_t end_angle)
{
    bool done = false;

#if LV_DRAW_COMPLEX
    if(dsc->opa <= (lv_opa_t)LV_OPA_MIN)
        return;
    if(dsc->width == 0)
        return;
    if(start_angle == end_angle)
        return;

    if(dsc->blend_mode == LV_BLEND_MODE_SUBTRACTIVE || lv_vglite_get_dest_buf() == NULL) {
        lv_draw_sw_arc(draw_ctx, dsc, center, radius, start_angle, end_angle);
        return;
    }

    /* Make coordinates relative to the draw buffer */
    lv_point_t rel_center = {center->x - draw_ctx->buf_area->x1, center->y - draw_ctx->buf_area->y1};

    lv_area_t rel_clip_area;
    lv_area_copy(&rel_clip_area, draw_ctx->clip_area);
    lv_area_move(&rel_clip_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    /* Make sure the angle diff is in range [0, 360) */
    while (end_angle < start_angle) end_angle += 360;
    while (end_angle > 360 + start_angle) end_angle -= 360;

    done = (lv_gpu_vglite_draw_arc(&rel_center, (int32_t)radius, (int32_t)start_angle, (int32_t)end_angle,
                                   &rel_clip_area, dsc) == LV_RES_OK);
    if(!done) {
        VG_LITE_LOG_TRACE("VG-Lite draw arc failed. Fallback.");
    }
#endif/*LV_DRAW_COMPLEX*/

    if(!done)
        lv_draw_sw_arc(draw_ctx, dsc, center, radius, start_angle, end_angle);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_res_t lv_draw_vglite_bg(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    if(dsc->bg_opa <= (lv_opa_t)LV_OPA_MIN)
        return LV_RES_INV;

    lv_area_t rel_coords;
    lv_area_copy(&rel_coords, coords);

    /*If the border fully covers make the bg area 1px smaller to avoid artifacts on the corners*/
    if(dsc->border_width > 1 && dsc->border_opa >= (lv_opa_t)LV_OPA_MAX && dsc->radius != 0) {
        rel_coords.x1 += (dsc->border_side & LV_BORDER_SIDE_LEFT) ? 1 : 0;
        rel_coords.y1 += (dsc->border_side & LV_BORDER_SIDE_TOP) ? 1 : 0;
        rel_coords.x2 -= (dsc->border_side & LV_BORDER_SIDE_RIGHT) ? 1 : 0;
        rel_coords.y2 -= (dsc->border_side & LV_BORDER_SIDE_BOTTOM) ? 1 : 0;
    }

    bool has_mask = lv_draw_mask_is_any(&rel_coords);
    if(has_mask)
        return LV_RES_INV;

    /* Make coordinates relative to draw buffer */
    lv_area_move(&rel_coords, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    lv_area_t rel_clip_area;
    lv_area_copy(&rel_clip_area, draw_ctx->clip_area);
    lv_area_move(&rel_clip_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    lv_area_t clipped_coords;
    if(!_lv_area_intersect(&clipped_coords, &rel_coords, &rel_clip_area))
        return LV_RES_OK; /*Fully clipped, nothing to do*/

    lv_grad_dir_t grad_dir = dsc->bg_grad.dir;
    lv_color_t bg_color = (grad_dir == (lv_grad_dir_t)LV_GRAD_DIR_NONE) ?
                          dsc->bg_color : dsc->bg_grad.stops[0].color;
    if(bg_color.full == dsc->bg_grad.stops[1].color.full)
        grad_dir = LV_GRAD_DIR_NONE;

    /*
     * Most simple case: just a plain rectangle (no mask, no radius, no gradient)
     * shall be handled by draw_ctx->blend().
     *
     * Complex case: gradient or radius but no mask.
     */
    if((dsc->radius != 0) || (grad_dir != (lv_grad_dir_t)LV_GRAD_DIR_NONE)) {
        lv_res_t res = lv_gpu_vglite_draw_bg(&rel_coords, &rel_clip_area, dsc);
        if(res != LV_RES_OK)
            VG_LITE_LOG_TRACE("VG-Lite draw bg failed. Fallback.");

        return res;
    }

    return LV_RES_INV;
}

__unused static lv_res_t lv_draw_vglite_border(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc,
                                      const lv_area_t * coords)
{
    if(dsc->border_opa <= (lv_opa_t)LV_OPA_MIN)
        return LV_RES_OK;
    if(dsc->border_width == 0)
        return LV_RES_OK;
    if(dsc->border_post)
        return LV_RES_INV;
    if(dsc->border_side != (lv_border_side_t)LV_BORDER_SIDE_FULL)
        return LV_RES_INV;

    lv_area_t rel_coords;
    lv_coord_t border_width = dsc->border_width;

    /* Move border inwards to align with software rendered border */
    rel_coords.x1 = coords->x1 + ceil(border_width / 2.0f);
    rel_coords.x2 = coords->x2 - floor(border_width / 2.0f);
    rel_coords.y1 = coords->y1 + ceil(border_width / 2.0f);
    rel_coords.y2 = coords->y2 - floor(border_width / 2.0f);

    /* Make coordinates relative to the draw buffer */
    lv_area_move(&rel_coords, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    lv_area_t rel_clip_area;
    lv_area_copy(&rel_clip_area, draw_ctx->clip_area);
    lv_area_move(&rel_clip_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    lv_area_t clipped_coords;
    if(!_lv_area_intersect(&clipped_coords, &rel_coords, &rel_clip_area))
        return LV_RES_OK; /*Fully clipped, nothing to do*/

    lv_res_t res = lv_gpu_vglite_draw_border_generic(&rel_coords, &rel_clip_area, dsc, true);
    if(res != LV_RES_OK)
        VG_LITE_LOG_TRACE("VG-Lite draw border failed. Fallback.");

    return res;
}

__unused static lv_res_t lv_draw_vglite_outline(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc,
                                       const lv_area_t * coords)
{
    if(dsc->outline_opa <= (lv_opa_t)LV_OPA_MIN)
        return LV_RES_OK;
    if(dsc->outline_width == 0)
        return LV_RES_OK;

    /* Move outline outwards to align with software rendered outline */
    lv_coord_t outline_pad = dsc->outline_pad - 1;
    lv_area_t rel_coords;
    rel_coords.x1 = coords->x1 - outline_pad - floor(dsc->outline_width / 2.0f);
    rel_coords.x2 = coords->x2 + outline_pad + ceil(dsc->outline_width / 2.0f);
    rel_coords.y1 = coords->y1 - outline_pad - floor(dsc->outline_width / 2.0f);
    rel_coords.y2 = coords->y2 + outline_pad + ceil(dsc->outline_width / 2.0f);

    /* Make coordinates relative to the draw buffer */
    lv_area_move(&rel_coords, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    lv_area_t rel_clip_area;
    lv_area_copy(&rel_clip_area, draw_ctx->clip_area);
    lv_area_move(&rel_clip_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    lv_area_t clipped_coords;
    if(!_lv_area_intersect(&clipped_coords, &rel_coords, &rel_clip_area))
        return LV_RES_OK; /*Fully clipped, nothing to do*/

    lv_res_t res = lv_gpu_vglite_draw_border_generic(&rel_coords, &rel_clip_area, dsc, false);
    if(res != LV_RES_OK)
        VG_LITE_LOG_TRACE("VG-Lite draw outline failed. Fallback.");

    return res;
}

#endif /*LV_USE_GPU_ACTS_VG_LITE*/
