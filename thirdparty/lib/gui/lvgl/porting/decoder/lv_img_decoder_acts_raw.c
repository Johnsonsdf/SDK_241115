/**
 * @file lv_img_decoder_acts_raw.c
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

#include "lv_img_decoder_acts.h"

#if LV_USE_GPU_ACTS_JPG && !defined(CONFIG_LVGL_USE_IMG_DECODER_ACTS)

#ifdef CONFIG_JPEG_HAL
#  include <jpeg_hal.h>
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_res_t _img_decoder_acts_raw_info(lv_img_decoder_t * decoder, const void * src,
									       lv_img_header_t * header);
static lv_res_t _img_decoder_acts_raw_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);
static lv_res_t _img_decoder_acts_raw_read_line(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc,
											    lv_coord_t x, lv_coord_t y, lv_coord_t len, uint8_t * buf);
static void _img_decoder_acts_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_img_decoder_acts_raw_init(void)
{
	lv_img_decoder_t * decoder = lv_img_decoder_create();
	LV_ASSERT_MALLOC(decoder);
	if(decoder == NULL) {
		LV_LOG_WARN("decoder_acts_raw_init: out of memory");
		return;
	}

	lv_img_decoder_set_info_cb(decoder, _img_decoder_acts_raw_info);
	lv_img_decoder_set_open_cb(decoder, _img_decoder_acts_raw_open);
	lv_img_decoder_set_read_line_cb(decoder, _img_decoder_acts_raw_read_line);
	lv_img_decoder_set_close_cb(decoder, _img_decoder_acts_close);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_res_t _img_decoder_acts_raw_info(lv_img_decoder_t * decoder, const void * src,
									       lv_img_header_t * header)
{
#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0
	LV_UNUSED(decoder);

	lv_img_src_t src_type = lv_img_src_get_type(src);

	if (src_type == LV_IMG_SRC_VARIABLE) {
		const lv_img_dsc_t * img_dsc = src;
		if (img_dsc->header.cf != LV_IMG_CF_RAW)
			return LV_RES_INV;

		uint32_t pic_w = 0, pic_h = 0;
		if (hal_jpeg_get_picture_info((void *)img_dsc->data, img_dsc->data_size, &pic_w, &pic_h) == 0) {
			header->always_zero = 0;
			header->cf = LV_IMG_CF_RAW;
			header->w  = pic_w;
			header->h  = pic_h;
			return LV_RES_OK;
		}
	}
#endif /* LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0 */

	return LV_RES_INV;
}

static lv_res_t _img_decoder_acts_raw_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
	LV_UNUSED(decoder);
	LV_UNUSED(dsc);

	return LV_RES_OK;
}

static void _img_decoder_acts_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
	LV_UNUSED(decoder);
	LV_UNUSED(dsc);
}

static lv_res_t _img_decoder_acts_raw_read_line(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc,
											    lv_coord_t x, lv_coord_t y, lv_coord_t len, uint8_t * buf)
{
#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0
	const lv_img_dsc_t * img_dsc = dsc->src;

	int dec_len = jpg_decode((void *)img_dsc->data, (int)img_dsc->data_size, buf,
				(LV_COLOR_DEPTH == 16) ? 1 : 0, len, x, y, len, 1);

	return (dec_len > 0) ? LV_RES_OK : LV_RES_INV;
#else
	return LV_RES_INV;
#endif
}

#endif /* LV_USE_GPU_ACTS_JPG && !defined(CONFIG_LVGL_USE_IMG_DECODER_ACTS) */
