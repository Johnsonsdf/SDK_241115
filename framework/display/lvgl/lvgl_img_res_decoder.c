/**
 * @file lvgl_img_res_decoder.c
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
#include <os_common_api.h>
#include <lvgl/lvgl_img_decoder.h>
#include <lvgl/src/draw/lv_img_decoder.h>
#include <lvgl/src/draw/lv_draw_img.h>
#include <lvgl/porting/decoder/lv_img_decoder_acts.h>
#include <res_manager_api.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/


/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_res_t _img_decoder_acts_res_info(lv_img_decoder_t * decoder, const void * src,
									   lv_img_header_t * header);
static lv_res_t _img_decoder_acts_res_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);
static lv_res_t _img_decoder_acts_res_read_line(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc,
											lv_coord_t x, lv_coord_t y, lv_coord_t len, uint8_t * buf);
static void _img_decoder_acts_res_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);


/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lvgl_img_decoder_acts_res_init(void)
{
	lv_img_decoder_t *res_decoder = lv_img_decoder_create();
	LV_ASSERT_MALLOC(res_decoder);
	if(res_decoder == NULL) {
		LV_LOG_WARN("lvgl_img_decoder_acts_init: out of memory");
		return;
	}

	lv_img_decoder_set_info_cb(res_decoder, _img_decoder_acts_res_info);
	lv_img_decoder_set_open_cb(res_decoder, _img_decoder_acts_res_open);
	lv_img_decoder_set_read_line_cb(res_decoder, _img_decoder_acts_res_read_line);
	lv_img_decoder_set_close_cb(res_decoder, _img_decoder_acts_res_close);

}

static lv_img_cf_t _get_res_img_cf(const style_bitmap_t *sty)
{
	lv_img_cf_t cf; 

	switch (sty->format) {
	case RESOURCE_BITMAP_FORMAT_ARGB8888:
		cf = LV_IMG_CF_ARGB_8888;
		break;
	case RESOURCE_BITMAP_FORMAT_RGB565:
	case RESOURCE_BITMAP_FORMAT_JPEG:	
		cf = LV_IMG_CF_RGB_565;
		break;
	case RESOURCE_BITMAP_FORMAT_ARGB8565:
		cf = LV_IMG_CF_ARGB_8565;
		break;

	case RESOURCE_BITMAP_FORMAT_A8:
		cf = LV_IMG_CF_ALPHA_8BIT;
		break;
	case RESOURCE_BITMAP_FORMAT_ARGB6666:
		cf = LV_IMG_CF_ARGB_6666;
		break;
	case RESOURCE_BITMAP_FORMAT_ARGB1555:
		cf = LV_IMG_CF_ARGB_1555;
		break;
	case RESOURCE_BITMAP_FORMAT_RGB888:
	case RESOURCE_BITMAP_FORMAT_RAW:
	default:
		cf = LV_IMG_CF_UNKNOWN;
	}

	return cf;
}

static lv_res_t _img_decoder_acts_res_info(lv_img_decoder_t * decoder, const void * src, lv_img_header_t * header)
{
	LV_UNUSED(decoder);
	lv_img_src_t src_type = lv_img_src_get_type(src);

	SYS_LOG_INF("in _img_decoder_acts_res_info %d\n", src_type);
	if (src_type == LV_IMG_SRC_VARIABLE) 
	{
		const lv_img_dsc_t * img_dsc = src;
		if (img_dsc->header.cf != LV_IMG_CF_RAW && img_dsc->header.cf != LV_IMG_CF_RAW_ALPHA)
		{
			SYS_LOG_INF("not raw\n");
			return LV_RES_INV;
		}
			
		if (img_dsc->data_size != sizeof(style_bitmap_t))
		{
			SYS_LOG_INF("not style bitmap %d\n", sizeof(style_bitmap_t));
			return LV_RES_INV;
		}

		const style_bitmap_t * sty = (style_bitmap_t *)img_dsc->data;
		header->always_zero = 0;
		header->w  = sty->width;
		header->h  = sty->height;
		header->cf = _get_res_img_cf(sty);

		return LV_RES_OK;
	}

	return LV_RES_INV;
}


static lv_res_t _img_decoder_acts_res_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
	LV_UNUSED(decoder);
	int ret;
	lv_img_dsc_t* src = (lv_img_dsc_t*)dsc->src;

	if(src->data_size != sizeof(style_bitmap_t))
	{
		return LV_RES_INV;
	}

	style_bitmap_t* sty = (style_bitmap_t*)src->data;
	ret = res_manager_load_bitmap_for_decoder(sty);
	if(ret < 0)
	{
		return LV_RES_INV;
	}
	dsc->img_data = sty->buffer;

	return LV_RES_OK;
}

static lv_res_t _img_decoder_acts_res_read_line(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc,
											lv_coord_t x, lv_coord_t y, lv_coord_t len, uint8_t * buf)
{
	return LV_RES_OK;
}

static void _img_decoder_acts_res_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
	LV_UNUSED(decoder);
	lv_img_dsc_t* src = (lv_img_dsc_t*)dsc->src;

	if(src->data_size != sizeof(style_bitmap_t))
	{
		return;
	}

	res_manager_free_bitmap_for_decoder((void*)dsc->img_data);
	dsc->error_msg = NULL;
	dsc->img_data  = NULL;
	dsc->user_data = NULL;
	dsc->time_to_open = 0;	

	return;
}

