/**
 * @file lvgl_img_decoder_acts.c
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
#include <compress_api.h>
#include <lvgl/lvgl_img_decoder.h>
#include <lvgl/src/draw/lv_img_decoder.h>
#include <lvgl/src/draw/lv_draw_img.h>
#include <lvgl/porting/decoder/lv_img_decoder_acts.h>

#ifdef CONFIG_JPEG_HAL
#  include <jpeg_hal.h>
#endif

/*********************
 *      DEFINES
 *********************/
/* Meet cache line (32 bytes) alignment and VG-Lite requirement */
#define DECODE_CACHE_BUF_SIZE CONFIG_LVGL_DECODE_CACHE_BUF_SIZE

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
	const uint8_t *src_raw;
	lv_img_dsc_t src_dsc;
	lv_area_t src_area;
	uint8_t src_px_bytes;

	uint32_t max_bytes;
} decoder_cache_acts_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_res_t _img_decoder_acts_info(lv_img_decoder_t * decoder, const void * src,
									   lv_img_header_t * header);
static lv_res_t _img_decoder_acts_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);
static lv_res_t _img_decoder_acts_read_line(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc,
											lv_coord_t x, lv_coord_t y, lv_coord_t len, uint8_t * buf);
static void _img_decoder_acts_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);

static bool _is_jpeg(const void *data);
static lv_img_cf_t _get_decoded_img_cf(const lv_img_dsc_t *src);
static int _img_decoder_decode(void * buf, uint32_t buf_size,
                               const lv_img_dsc_t *src, const lv_area_t *area);

/**********************
 *  STATIC VARIABLES
 **********************/
#ifndef _WIN32
__attribute__((__aligned__(64))) __in_section_unique(decompress.bss.cache)
#endif
static uint8_t s_decoder_cache_buf[DECODE_CACHE_BUF_SIZE];

static decoder_cache_acts_t s_decoder_cache;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lvgl_img_decoder_acts_init(void)
{
	lv_img_decoder_t * decoder = lv_img_decoder_create();
	LV_ASSERT_MALLOC(decoder);
	if(decoder == NULL) {
		LV_LOG_WARN("lvgl_img_decoder_acts_init: out of memory");
		return;
	}

	lv_img_decoder_set_info_cb(decoder, _img_decoder_acts_info);
	lv_img_decoder_set_open_cb(decoder, _img_decoder_acts_open);
	lv_img_decoder_set_read_line_cb(decoder, _img_decoder_acts_read_line);
	lv_img_decoder_set_close_cb(decoder, _img_decoder_acts_close);
}

const void * lvgl_img_decoder_acts_read_area(const lv_img_dsc_t * src, const lv_area_t * area,
                                             lv_coord_t * stride, lv_img_cf_t * cf)
{
	decoder_cache_acts_t *cache = &s_decoder_cache;

	if (src->data != cache->src_raw || _lv_area_is_in(area, &cache->src_area, 0) == false) {
		compress_pic_head_t *pic_head = (compress_pic_head_t *)src->data;
		int len;

		cache->src_dsc.header.cf = _get_decoded_img_cf(src);
		if (cache->src_dsc.header.cf == LV_IMG_CF_UNKNOWN)
			return NULL;

		len = _img_decoder_decode(s_decoder_cache_buf, sizeof(s_decoder_cache_buf), src, area);
		if (len < 0)
			return NULL;

		if (len > cache->max_bytes) {
			SYS_LOG_INF("img dec max %u bytes", len);
			cache->max_bytes = len;
		}

		cache->src_dsc.data = s_decoder_cache_buf;
		cache->src_dsc.data_size = len;
		cache->src_dsc.header.w = lv_area_get_width(area);
		cache->src_dsc.header.h = lv_area_get_height(area);

		lv_area_copy(&cache->src_area, area);

		cache->src_raw = src->data;
		cache->src_px_bytes = pic_head->bytes_per_pixel;
	}

	*stride = cache->src_dsc.header.w;
	*cf = cache->src_dsc.header.cf;

	return cache->src_dsc.data + ((area->y1 - cache->src_area.y1) * (*stride) +
				(area->x1 - cache->src_area.x1)) * cache->src_px_bytes;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_res_t _img_decoder_acts_info(lv_img_decoder_t * decoder, const void * src,
									   lv_img_header_t * header)
{
	LV_UNUSED(decoder);

	lv_img_src_t src_type = lv_img_src_get_type(src);

	if (src_type == LV_IMG_SRC_VARIABLE) {
		const lv_img_dsc_t * img_dsc = src;
		if (img_dsc->header.cf != LV_IMG_CF_RAW && img_dsc->header.cf != LV_IMG_CF_RAW_ALPHA)
			return LV_RES_INV;
		if (img_dsc->data_size < sizeof(compress_pic_head_t))
			return LV_RES_INV;

		const compress_pic_head_t * head = (compress_pic_head_t *)img_dsc->data;
		if (!_is_jpeg(head)) {
			header->always_zero = 0;
			header->w  = head->width;
			header->h  = head->height;
			header->cf = (head->format == COMPRESSED_PIC_CF_RGB_565) ?
					LV_IMG_CF_RAW : LV_IMG_CF_RAW_ALPHA;
			return LV_RES_OK;
		} else {
#ifdef CONFIG_JPEG_HAL
			uint32_t pic_w = 0, pic_h = 0;
			if (hal_jpeg_get_picture_info((void *)img_dsc->data, img_dsc->data_size, &pic_w, &pic_h) == 0) {
				header->always_zero = 0;
				header->cf = LV_IMG_CF_RAW;
				header->w  = pic_w;
				header->h  = pic_h;
				return LV_RES_OK;
			}
#endif /* CONFIG_JPEG_HAL */
		}
	}

	return LV_RES_INV;
}

static lv_res_t _img_decoder_acts_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
	LV_UNUSED(decoder);
	LV_UNUSED(dsc);

	return LV_RES_OK;
}

static lv_res_t _img_decoder_acts_read_line(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc,
											lv_coord_t x, lv_coord_t y, lv_coord_t len, uint8_t * buf)
{
	LV_UNUSED(decoder);

	lv_area_t area = { x, y, x + len - 1, y };
	lv_coord_t stride = 0;
	lv_img_cf_t cf = LV_IMG_CF_UNKNOWN;

	const uint8_t * data = lvgl_img_decoder_acts_read_area(dsc->src, &area, &stride, &cf);
	if (data == NULL)
		return LV_RES_INV;

	return lv_img_decode_acts_rgb_color(buf, data, len, cf);
}

static void _img_decoder_acts_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
	LV_UNUSED(decoder);
	LV_UNUSED(dsc);
}

static bool _is_jpeg(const void *data)
{
	const compress_pic_head_t *pic_head = (compress_pic_head_t *)data;

	if (pic_head->magic != LZ4_PIC_MAGIC &&
		pic_head->magic != RLE_PIC_MAGIC &&
		pic_head->magic != RAW_PIC_MAGIC) {
		return true;
	}

	return false;
}

static lv_img_cf_t _get_decoded_img_cf(const lv_img_dsc_t *src)
{
	compress_pic_head_t *pic_head = (compress_pic_head_t *)src->data;

	if (_is_jpeg(pic_head))
		return LV_IMG_CF_RGB_565; /* should be JPEG */

	switch (pic_head->format) {
	case COMPRESSED_PIC_CF_RGB_565:
		return LV_IMG_CF_RGB_565;
	case COMPRESSED_PIC_CF_ARGB_8565:
		return LV_IMG_CF_ARGB_8565;
	case COMPRESSED_PIC_CF_ARGB_6666:
		return LV_IMG_CF_ARGB_6666;
	case COMPRESSED_PIC_CF_ARGB_8888:
		return LV_IMG_CF_ARGB_8888;
	default:
		LV_LOG_ERROR("pic %p compressed format %d", pic_head, pic_head->format);
		return LV_IMG_CF_UNKNOWN;
	}
}

static int _img_decoder_decode(void * buf, uint32_t buf_size,
                               const lv_img_dsc_t *src, const lv_area_t *area)
{
	compress_pic_head_t *pic_head = (compress_pic_head_t *)src->data;
	bool is_jpg = _is_jpeg(pic_head);
	int dec_len = (is_jpg ? sizeof(lv_color_t) : pic_head->bytes_per_pixel) * lv_area_get_size(area);
	int len;

	if (dec_len > buf_size) {
		LV_LOG_WARN("pic %p dec buf too small (%u < %d), area (%d %d %d %d)",
				pic_head, buf_size, dec_len, area->x1, area->y1, area->x2, area->y2);
		return -ENOMEM;
	}

	os_strace_u32x4(SYS_TRACE_ID_IMG_DECODE, pic_head->magic, (uint32_t)src,
	                lv_area_get_width(area), lv_area_get_height(area));

	if (is_jpg == false) {
		len = pic_decompress((void *)src->data, buf,
				src->data_size, buf_size, 0, area->x1, area->y1,
				lv_area_get_width(area), lv_area_get_height(area));
	} else { /* JPEG */
#ifdef CONFIG_JPEG_HAL
		len = jpg_decode((void *)src->data, (int)src->data_size, buf,
				(LV_COLOR_DEPTH == 16) ? 1 : 0, lv_area_get_width(area),
				area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area));
#else
		LV_LOG_ERROR("pic %p magic 0x%08x invalid", pic_head, pic_head->magic);
		return -EINVAL;
#endif
	}

	os_strace_end_call(SYS_TRACE_ID_IMG_DECODE);

	if (len != dec_len) {
		LV_LOG_ERROR("pic %p decompress error %d %d", pic_head, len, dec_len);
		return -EINVAL;
	}

	return len;
}
