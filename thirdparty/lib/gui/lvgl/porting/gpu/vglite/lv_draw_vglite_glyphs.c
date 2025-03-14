/**
 * @file lv_draw_vglite_glyphs.c
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

#include "lv_draw_vglite_glyphs.h"

#if LV_USE_GPU_ACTS_VG_LITE
#include "lv_vglite_buf.h"
#include <math.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_res_t lv_gpu_vglite_draw_glyphs(const lv_area_t * dest_area, const lv_svgfont_bitmap_dsc_t * path_dsc,
                                   const lv_point_t * path_ori, const lv_draw_label_dsc_t * dsc)
{
    vg_lite_error_t err = VG_LITE_SUCCESS;
    vg_lite_path_t path;
    vg_lite_color_t vgcol; /* vglite takes ABGR */
    vg_lite_buffer_t * vgbuf = lv_vglite_get_dest_buf();

    /* Choose vglite blend mode based on given lvgl blend mode */
    vg_lite_blend_t vglite_blend_mode = VG_LITE_BLEND_NONE;
    if(dsc->opa < (lv_opa_t)LV_OPA_MAX || dsc->blend_mode != LV_BLEND_MODE_NORMAL)
        vglite_blend_mode = lv_vglite_get_blend_mode(dsc->blend_mode);

    /*** Init path ***/
    err = vg_lite_init_path(&path, VG_LITE_FP32, VG_LITE_HIGH, path_dsc->path_len, path_dsc->path_data,
                            path_dsc->path_box[0], path_dsc->path_box[1],
                            path_dsc->path_box[2] + 1.0f, path_dsc->path_box[3] + 1.0f);
    VG_LITE_ERR_RETURN_INV(err, "Init path failed.");

    vg_lite_matrix_t matrix;
    vg_lite_identity(&matrix);
    vg_lite_translate((vg_lite_float_t)path_ori->x + path_dsc->shift_x,
                      (vg_lite_float_t)path_ori->y + path_dsc->shift_y, &matrix);
    vg_lite_scale(path_dsc->scale, path_dsc->scale, &matrix);

    lv_color32_t col32 = { .full = lv_color_to32(dsc->color) }; /*Convert color to RGBA8888*/
    vg_lite_buffer_format_t color_format = VG_LITE_RGBA8888;
    if(lv_vglite_premult_and_swizzle(&vgcol, col32, dsc->opa, color_format) != LV_RES_OK)
        VG_LITE_RETURN_INV("Premultiplication and swizzle failed.");

    /* Set scissor. */
    lv_vglite_set_scissor(dest_area);

    /*** Draw glyphs ***/
    err = vg_lite_draw(vgbuf, &path, VG_LITE_FILL_NON_ZERO, &matrix, vglite_blend_mode, vgcol);
    if(err != VG_LITE_SUCCESS)
        VG_LITE_ERR_RETURN_INV(err, "Draw line failed.");

    if(lv_vglite_run(dest_area) != LV_RES_OK)
        VG_LITE_RETURN_INV("Run failed.");

    /* Disable scissor. */
    lv_vglite_disable_scissor();

    err = vg_lite_clear_path(&path);
    VG_LITE_ERR_RETURN_INV(err, "Clear path failed.");

    return LV_RES_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /*LV_USE_GPU_ACTS_VG_LITE*/
