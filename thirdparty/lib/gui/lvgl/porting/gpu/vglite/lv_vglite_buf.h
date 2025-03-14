/**
 * @file lv_vglite_buf.h
 *
 */

/**
 * MIT License
 *
 * Copyright 2023 NXP
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

#ifndef LV_VGLITE_BUF_H
#define LV_VGLITE_BUF_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../../src/lv_conf_internal.h"

#if LV_USE_GPU_ACTS_VG_LITE
#include "vg_lite.h"
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
/**
 * Init vglite destination buffer. It will be done once per frame.
 *
 * @param[in] buf Destination buffer address (does not require alignment for VG_LITE_LINEAR mode)
 * @param[in] transp Destination buffer transparent or not
 * @param[in] area Destination buffer area (for width and height)
 * @param[in] stride Stride of destination buffer
 *
 * @retval LV_RES_OK Operation completed
 * @retval LV_RES_INV Error occurred (\see LV_GPU_VG_LITE_LOG_ERRORS)
 */
lv_res_t lv_gpu_vglite_init_buf(const lv_color_t * buf, bool transp, const lv_area_t * area, lv_coord_t stride);

/**
 * Get vglite destination buffer pointer.
 *
 * @retval The vglite destination buffer
 */
vg_lite_buffer_t * lv_vglite_get_dest_buf(void);

/**
 * Get vglite source buffer pointer.
 *
 * @retval The vglite source buffer
 */
vg_lite_buffer_t * lv_vglite_get_src_buf(void);

/**
 * Set vglite destination buffer address only.
 *
 * @param[in] buf Destination buffer address (does not require alignment for VG_LITE_LINEAR mode)
 *
 * @retval LV_RES_OK Operation completed
 * @retval LV_RES_INV Error occurred (\see LV_GPU_VG_LITE_LOG_ERRORS)
 */
lv_res_t lv_vglite_set_dest_buf_ptr(const lv_color_t * buf);

/**
 * Set vglite source buffer address only.
 *
 * @param[in] buf Source buffer address
 *
 * @retval LV_RES_OK Operation completed
 * @retval LV_RES_INV Error occurred (\see LV_GPU_VG_LITE_LOG_ERRORS)
 */
lv_res_t lv_vglite_set_src_buf_ptr(const lv_color_t * buf);

/**
 * Set vglite source buffer. It will be done only if buffer addreess is different.
 *
 * @param[in] buf Source buffer address
 * @param[in] cf Source buffer color format
 * @param[in] area Source buffer area (for width and height)
 * @param[in] stride Stride of source buffer
 *
 * @retval LV_RES_OK Operation completed
 * @retval LV_RES_INV Error occurred (\see LV_GPU_VG_LITE_LOG_ERRORS)
 */
lv_res_t lv_vglite_set_src_buf(const lv_color_t * buf, lv_img_cf_t cf, const lv_area_t * area, lv_coord_t stride);

/**
 * Set vglite buffer.
 *
 * @param[in] vgbuf Address of the VGLite buffer object
 * @param[in] buf Address of the memory for the VGLite buffer
 * @param[in] cf Buffer color format
 * @param[in] area Source buffer area (for width and height)
 * @param[in] stride Stride of source buffer
 *
 * @retval LV_RES_OK Operation completed
 * @retval LV_RES_INV Error occurred (\see LV_GPU_VG_LITE_LOG_ERRORS)
 */
lv_res_t lv_vglite_set_buf(vg_lite_buffer_t * vgbuf, const lv_color_t * buf,
                           lv_img_cf_t cf, const lv_area_t * area, lv_coord_t stride);

/**
 * Set vglite source buffer. It will be done only if buffer addreess is different.
 *
 * @param[in] buf vg lite buffer
 *
 * @retval 1 has alpha
 * @retval 0 no alpha
 */
int lv_vglite_buf_has_alpha(const vg_lite_buffer_t *buf);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_GPU_ACTS_VG_LITE*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_VGLITE_BUF_H*/
