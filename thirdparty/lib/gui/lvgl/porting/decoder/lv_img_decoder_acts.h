/**
 * @file lv_img_decoder_acts.h
 *
 */

/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PORTING_DECODER_LV_IMG_DECODER_ACTS_H
#define PORTING_DECODER_LV_IMG_DECODER_ACTS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @cond INTERNAL_HIDDEN
 */

/*********************
 *      INCLUDES
 *********************/
#include "../../src/draw/lv_draw_img.h"
#include "../../src/draw/lv_img_decoder.h"

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
 * @brief Initialize the Actions linear image decoder module
 *
 * @retval N/A
 */
void lv_img_decoder_acts_basic_init(void);

/**
 * @brief Initialize the Actions compression image decoder module
 *
 * @retval N/A
 */
void lv_img_decoder_acts_raw_init(void);

/**
 * @brief Decode the Actions basic rgb color format
 *
 * @param dst_buf store the decoded color
 * @param src_buf store the encoded color
 * @param len number of pixels to decode
 * @param cf the color format of src_buf
 *
 * @retval LV_RES_OK on success else LV_RES_INV
 */
lv_res_t lv_img_decode_acts_rgb_color(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len, lv_img_cf_t cf);

/**********************
 *      MACROS
 **********************/

/**
 * INTERNAL_HIDDEN @endcond
 */

#ifdef __cplusplus
}
#endif

#endif /* PORTING_DECODER_LV_IMG_DECODER_ACTS_H */
