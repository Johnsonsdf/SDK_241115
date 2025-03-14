/**
 * @file lv_draw_vglite_glyphs.h
 *
 */

/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LV_DRAW_VGLITE_GLYPHS_H
#define LV_DRAW_VGLITE_GLYPHS_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../../src/lv_conf_internal.h"

#if LV_USE_GPU_ACTS_VG_LITE
#include "lv_vglite_utils.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Draw line shape with effects
 *
 * @param[in] dest_area Area with relative coordinates of destination buffer
 * @param[in] path_dsc Path infomation
 * @param[in] path_ori The coordinate origin of the path data with relative coordinates to dest buff
 * @param[in] dsc Label description structure (width, rounded ending, opacity, ...)
 *
 * @retval LV_RES_OK Draw completed
 * @retval LV_RES_INV Error occurred (\see LV_GPU_NXP_VG_LITE_LOG_ERRORS)
 */
lv_res_t lv_gpu_vglite_draw_glyphs(const lv_area_t * dest_area, const lv_svgfont_bitmap_dsc_t * path_dsc,
                                   const lv_point_t * path_ori, const lv_draw_label_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_GPU_ACTS_VG_LITE*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_VGLITE_RECT_H*/
