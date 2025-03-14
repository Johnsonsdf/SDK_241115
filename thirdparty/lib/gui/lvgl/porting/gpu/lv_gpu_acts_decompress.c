/**
 * @file lv_gpu_acts_decompress.c
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

#if LV_USE_GPU_ACTS_SW_DECODER

#include <compress_api.h>

/*********************
 *      DEFINES
 *********************/

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

/**********************
 *      MACROS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static bool _is_decompress(const uint8_t * raw_data, size_t len);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_res_t lv_gpu_acts_decompress_draw_img_dsc(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src)
{
    int res;

    if (LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0 || need_argb8565_support(draw_ctx))
        return LV_RES_INV;

    if (draw_dsc->angle != 0 || draw_dsc->zoom != LV_IMG_ZOOM_NONE || draw_dsc->opa < LV_OPA_MAX)
        return LV_RES_INV;

    if (src->header.cf != LV_IMG_CF_RAW || _is_decompress(src->data, src->data_size) == false)
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

    lv_gpu_acts_set_accl_type(draw_ctx, ACCL_CPU, &draw_area);

    res = pic_decompress((void *)src->data,
            (void *)dest, (int)src->data_size, 0, dest_stride * 2,
            draw_area.x1 - coords->x1, draw_area.y1 - coords->y1,
            draw_w, draw_h);
    if (res <= 0) {
        LV_LOG_WARN("decompress decode failed %d", res);
        return LV_RES_INV;
    }

    lv_gpu_acts_flush_dcache(draw_ctx, &draw_area);

    return LV_RES_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static bool _is_decompress(const uint8_t * raw_data, size_t len)
{
    compress_pic_head_t *pic_head = (compress_pic_head_t *)raw_data;
    if ((pic_head->magic != LZ4_PIC_MAGIC &&
        pic_head->magic != RLE_PIC_MAGIC &&
        pic_head->magic != RAW_PIC_MAGIC)
		|| pic_head->bytes_per_pixel != 2) {
        return false;
    }

    return true;
}

#endif /* LV_USE_GPU_ACTS_SW_DECODER */
