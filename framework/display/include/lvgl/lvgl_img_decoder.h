/**
 * @file lv_img_decoder_acts.h
 *
 */

/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LVGL_IMG_DECODER_ACTS_H
#define LVGL_IMG_DECODER_ACTS_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <lvgl/lvgl.h>

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
 * Initialize the image decoder on Actions platform
 */
void lvgl_img_decoder_acts_init(void);

/**
 * Decode image data inside the given area
 * @param src pointer to the image src varaible
 * @param area pointer to the image area to read
 * @param stride pointer to store the image stride in pixels of the decoded data
 * @param cf pointer to store the image cf of the decoded data
 * @return pointer to the decoded data
 */
const void * lvgl_img_decoder_acts_read_area(const lv_img_dsc_t * src, const lv_area_t * area,
                                             lv_coord_t * stride, lv_img_cf_t * cf);

/**
 * Submit a background task to decode image data inside the given area
 *
 * The behaviour is implemention decided and may not have any effect.
 *
 * @param src pointer to the image src varaible
 * @param area pointer to the image area to read
 * @return N/A
 */
void lvgl_img_decoder_acts_submit(const lv_img_dsc_t * src, const lv_area_t * area);

/**
 * Cancel all the background decoding task, and release the related resource
 * @return N/A
 */
void lvgl_img_decoder_acts_cancel(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
}
#endif

#endif /* LVGL_IMG_DECODER_ACTS_H */
