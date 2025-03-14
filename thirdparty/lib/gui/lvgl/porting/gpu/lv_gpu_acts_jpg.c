/**
 * @file lv_gpu_acts_sw.c
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
#include "lv_gpu_acts.h"

#if LV_USE_GPU_ACTS_JPG

#include <jpeg_hal.h>
#include <compress_api.h>

/*********************
 *      DEFINES
 *********************/
#if LV_COLOR_DEPTH == 16
#  define JPEG_OUT_FORMAT 1 /* RGB565 */
#else
#  define JPEG_OUT_FORMAT 0 /* RGB888 */
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL PROTOTYPES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static hal_jpeg_handle_t jpg_decoder;
static bool jpg_opened = false;

/**********************
 *      MACROS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static bool _is_jpg(const uint8_t * raw_data, size_t len);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int lv_gpu_acts_jpg_init(void)
{
    if (LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0)
        return -ENOTSUP;

    return hal_jpeg_init(&jpg_decoder, 0);
}

void lv_gpu_acts_jpg_wait_for_finish(lv_draw_ctx_t * draw_ctx)
{
    if (jpg_opened) {
        hal_jpeg_decode_wait_finised(&jpg_decoder, 1000);
        hal_jpeg_decode_close(&jpg_decoder);
        jpg_opened = false;
    }
}

lv_res_t lv_gpu_acts_jpg_draw_img_dsc(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src)
{
    int res;

    if (LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0 || need_argb8565_support(draw_ctx))
        return -EINVAL;

    if (draw_dsc->angle != 0 || draw_dsc->zoom != LV_IMG_ZOOM_NONE || draw_dsc->opa < LV_OPA_MAX)
        return LV_RES_INV;

    if (src->header.cf != LV_IMG_CF_RAW || _is_jpg(src->data, src->data_size) == false)
        return LV_RES_INV;

    lv_area_t draw_area;
    if (_lv_area_intersect(&draw_area, coords, draw_ctx->clip_area) == false)
        return LV_RES_OK;

    if (lv_draw_mask_is_any(&draw_area))
        return LV_RES_INV;

    int32_t dest_stride = lv_area_get_width(draw_ctx->buf_area);
    lv_color_t *dest = (lv_color_t *)draw_ctx->buf +
            dest_stride * (draw_area.y1 - draw_ctx->buf_area->y1) +
                (draw_area.x1 - draw_ctx->buf_area->x1);
    lv_coord_t draw_w = lv_area_get_width(&draw_area);
    lv_coord_t draw_h = lv_area_get_height(&draw_area);

    lv_gpu_acts_jpg_wait_for_finish(draw_ctx); /* close previous jpg coder */

    res = hal_jpeg_decode_open(&jpg_decoder);
    if (res) {
        LV_LOG_WARN("jpeg open failed %d", res);
        return LV_RES_INV;
    }

    lv_gpu_acts_set_accl_type(draw_ctx, ACCL_JPG, &draw_area);

    res = hal_jpeg_decode(&jpg_decoder, (void *)src->data, (int)src->data_size,
            (void *)dest, JPEG_OUT_FORMAT, dest_stride,
            draw_area.x1 - coords->x1, draw_area.y1 - coords->y1,
            draw_w, draw_h);
    if (res) {
        LV_LOG_WARN("jpeg decode failed %d", res);
        hal_jpeg_decode_close(&jpg_decoder);
        return LV_RES_INV;
    }

    lv_gpu_acts_flush_dcache(draw_ctx, &draw_area);
    jpg_opened = true;

    return LV_RES_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static bool _is_jpg(const uint8_t * raw_data, size_t len)
{
#ifdef CONFIG_LVGL_USE_IMG_DECODER_ACTS
    compress_pic_head_t *pic_head = (compress_pic_head_t *)raw_data;
    if (pic_head->magic != LZ4_PIC_MAGIC &&
        pic_head->magic != RLE_PIC_MAGIC &&
        pic_head->magic != RAW_PIC_MAGIC) {
        return true;
    }

    return false;
#else
    struct jpeg_head_info *head_info = (struct jpeg_head_info *)raw_data;
    if (head_info->flag == JPEG_FLAG)
        return true;

    const uint8_t jpg_signature[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46};
    if(len < sizeof(jpg_signature)) return false;
    return (memcmp(jpg_signature, raw_data, sizeof(jpg_signature)) == 0) ? true : false;
#endif
}

#endif /*LV_USE_GPU_ACTS_JPG*/
