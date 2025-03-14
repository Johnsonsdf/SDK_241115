/**
 * @file lv_draw_vglite.h
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

#ifndef LV_DRAW_VGLITE_H
#define LV_DRAW_VGLITE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../../src/lv_conf_internal.h"

#if LV_USE_GPU_ACTS_VG_LITE
#include "../../../src/draw/sw/lv_draw_sw.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

int lv_draw_vglite_init(void);

void lv_draw_vglite_init_buf(lv_draw_ctx_t * draw_ctx, bool screen_transp);

void lv_draw_vglite_wait_for_finish(lv_draw_ctx_t * draw_ctx);

lv_res_t lv_draw_vglite_buffer_copy(void * dest_buf, lv_coord_t dest_stride, const lv_area_t * dest_area,
                                    void * src_buf, lv_coord_t src_stride, const lv_area_t * src_area);

lv_res_t lv_draw_vglite_img_dsc(lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
                                const lv_area_t * coords, const lv_img_dsc_t * src);

lv_res_t lv_draw_vglite_blend_basic(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc);

lv_res_t lv_draw_vglite_letter(lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,
                               const lv_point_t * pos_p, const lv_area_t * gbox_p, const uint8_t * map_p);

void lv_draw_vglite_line(lv_draw_ctx_t * draw_ctx, const lv_draw_line_dsc_t * dsc, const lv_point_t * point1,
                         const lv_point_t * point2);

void lv_draw_vglite_rect(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords);

void lv_draw_vglite_arc(lv_draw_ctx_t * draw_ctx, const lv_draw_arc_dsc_t * dsc, const lv_point_t * center,
                        uint16_t radius, uint16_t start_angle, uint16_t end_angle);

/**********************
 *      MACROS
 **********************/
#endif /*LV_USE_GPU_ACTS_VG_LITE*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_VGLITE_H*/
