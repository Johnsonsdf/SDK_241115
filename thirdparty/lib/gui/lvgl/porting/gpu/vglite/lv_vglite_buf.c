/**
 * @file lv_vglite_buf.c
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

/*********************
 *      INCLUDES
 *********************/

#include "../lv_gpu_acts.h"
#include "lv_vglite_buf.h"
#include "lv_vglite_utils.h"

#if LV_USE_GPU_ACTS_VG_LITE

/*********************
 *      DEFINES
 *********************/

#ifdef _WIN32
#  define LV_GPU_VG_LITE_MEM_ALIGN_SIZE 1
#else
#  define LV_GPU_VG_LITE_MEM_ALIGN_SIZE 64
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static inline lv_res_t lv_vglite_set_dest_buf(const lv_color_t * buf, lv_img_cf_t cf, const lv_area_t * area, lv_coord_t stride);
static inline lv_res_t lv_vglite_set_buf_ptr(vg_lite_buffer_t * vgbuf, const lv_color_t * buf);

/**********************
 *  STATIC VARIABLES
 **********************/

static vg_lite_buffer_t dest_vgbuf;
static vg_lite_buffer_t src_vgbuf;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_res_t lv_gpu_vglite_init_buf(const lv_color_t * buf, bool transp, const lv_area_t * area, lv_coord_t stride)
{
    lv_res_t res = lv_vglite_set_dest_buf(buf,
            transp ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR, area, stride);
    if (res != LV_RES_OK) {
        LV_LOG_TRACE("vglite init buf error");
        return res;
    }

    lv_vglite_disable_scissor();
    return LV_RES_OK;
}

vg_lite_buffer_t * lv_vglite_get_dest_buf(void)
{
    return (dest_vgbuf.handle != NULL) ? &dest_vgbuf : NULL;
}

vg_lite_buffer_t * lv_vglite_get_src_buf(void)
{
    return &src_vgbuf;
}

lv_res_t lv_vglite_set_dest_buf_ptr(const lv_color_t * buf)
{
    return lv_vglite_set_buf_ptr(&dest_vgbuf, buf);
}

lv_res_t lv_vglite_set_src_buf_ptr(const lv_color_t * buf)
{
    return lv_vglite_set_buf_ptr(&src_vgbuf, buf);
}

lv_res_t lv_vglite_set_src_buf(const lv_color_t * buf, lv_img_cf_t cf, const lv_area_t * area, lv_coord_t stride)
{
    lv_res_t res = LV_RES_OK;

    if(1/*src_vgbuf.memory != (void *)buf*/) {
        res = lv_vglite_set_buf(&src_vgbuf, buf, cf, area, stride);
        if (res == LV_RES_INV)
            return res;

        if (!lvgl_img_buf_is_clean(buf, 0)) {
            mem_dcache_clean(buf, src_vgbuf.stride * src_vgbuf.height);
            mem_dcache_sync();
        }
    }

    return res;
}

lv_res_t lv_vglite_set_buf(vg_lite_buffer_t * vgbuf, const lv_color_t * buf,
                           lv_img_cf_t cf, const lv_area_t * area, lv_coord_t stride)
{
    vg_lite_uint32_t * palette = (void *)buf; 
    uint8_t * memory = (void *)buf;
    uint8_t bpp = 0;
    int format;

    if(vgbuf->handle != NULL) {
        vg_lite_unmap(vgbuf);
        vgbuf->handle = NULL;
    }

    format = lvgl_img_cf_to_vglite(cf, &bpp);
    if(format < 0)
        return LV_RES_INV;

    if (format == VG_LITE_INDEX_8) {
        if (cf == LV_IMG_CF_INDEXED_8BIT_ACTS) {
            memory += *palette * 4 + 4;
            palette++;
        } else {
            memory += 256 * 4;
        }
    }

    vgbuf->memory = (void *)memory;
    if ((((uintptr_t)vgbuf->memory) & (LV_GPU_VG_LITE_MEM_ALIGN_SIZE - 1)) != 0)
        return LV_RES_INV;

    vgbuf->format = format;
    vgbuf->image_mode = VG_LITE_NORMAL_IMAGE_MODE;
    vgbuf->transparency_mode = VG_LITE_IMAGE_OPAQUE;
    vgbuf->width = (int32_t)lv_area_get_width(area);
    vgbuf->height = (int32_t)lv_area_get_height(area);
    vgbuf->stride = (int32_t)(stride) * bpp / 8;

    if (format == VG_LITE_RGBA8888_ETC2_EAC) {
        vgbuf->stride = (vgbuf->stride + 0x3) & ~0x3;
        vgbuf->tiled =  VG_LITE_TILED;
    } else {
        vgbuf->tiled = VG_LITE_LINEAR;
    }

    if(vg_lite_map(vgbuf, VG_LITE_MAP_USER_MEMORY, -1) != VG_LITE_SUCCESS)
        return LV_RES_INV;

    if (format == VG_LITE_INDEX_8)
        vg_lite_set_CLUT(256, palette);

    return LV_RES_OK;
}

int lv_vglite_buf_has_alpha(const vg_lite_buffer_t *buf)
{
    switch(buf->format) {
    case VG_LITE_BGRA5658:
    case VG_LITE_BGRA8888:
    case VG_LITE_BGRA5551:
    case VG_LITE_A8:
    case VG_LITE_INDEX_8:
    case VG_LITE_RGBA8888_ETC2_EAC:
        return 1;
    default:
        return 0;
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static inline lv_res_t lv_vglite_set_dest_buf(const lv_color_t * buf, lv_img_cf_t cf, const lv_area_t * area, lv_coord_t stride)
{
    return lv_vglite_set_buf(&dest_vgbuf, buf, cf, area, stride);
}

static inline lv_res_t lv_vglite_set_buf_ptr(vg_lite_buffer_t * vgbuf, const lv_color_t * buf)
{
    if(vgbuf->handle != NULL)
        vg_lite_unmap(vgbuf);

    vgbuf->memory = (void *)buf;
    return (vg_lite_map(vgbuf, VG_LITE_MAP_USER_MEMORY, -1) == VG_LITE_SUCCESS ? LV_RES_OK : LV_RES_INV);
}

#endif /*LV_USE_GPU_ACTS_VG_LITE*/
