/**
 * @file lv_gpu_acts.h
 *
 */

/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PORTING_GPU_LV_GPU_ACTS_H
#define PORTING_GPU_LV_GPU_ACTS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @cond INTERNAL_HIDDEN
 */

/*********************
 *      INCLUDES
 *********************/
#include "../../src/lv_conf_internal.h"
#include "../../src/draw/sw/lv_draw_sw.h"

#if LV_USE_GPU_ACTS
#include <assert.h>
#include <lvgl/lvgl_memory.h>
#include <memory/mem_cache.h>
#include <display/sw_math.h>

/*********************
 *      DEFINES
 *********************/

/* Default screen resolution */
#define DEF_SCREEN_W 480
#define DEF_SCREEN_H 480

#define ACCL_BIT(type) (1u << type)

/**********************
 *      TYPEDEFS
 **********************/

enum ACCELERATOR_TYPE {
    ACCL_CPU = 0,

#if LV_USE_GPU_ACTS_DMA2D
    ACCL_DMA2D,
#endif
#if LV_USE_GPU_ACTS_VG_LITE
    ACCL_VGLITE,
#endif
#if LV_USE_GPU_ACTS_JPG
    ACCL_JPG,
#endif

    ACCL_NUM_HW_TYPES,

    ACCL_NONE,
};

enum ACCELERATOR_BITFIELD {
    ACCL_CPU_BIT    = ACCL_BIT(ACCL_CPU),

#if LV_USE_GPU_ACTS_DMA2D
    ACCL_DMA2D_BIT  = ACCL_BIT(ACCL_DMA2D),
#endif
#if LV_USE_GPU_ACTS_VG_LITE
    ACCL_VGLITE_BIT = ACCL_BIT(ACCL_VGLITE),
#endif
#if LV_USE_GPU_ACTS_JPG
    ACCL_JPG_BIT    = ACCL_BIT(ACCL_JPG),
#endif

    ACCL_NONE_BIT   = 0,
};

typedef lv_draw_sw_ctx_t lv_draw_gpu_acts_ctx_t;

typedef struct {
	lv_draw_sw_layer_ctx_t sw_ctx;
	uint8_t buf_ofs; /*layer buffer offset for alignment*/
} lv_draw_gpu_acts_layer_ctx_t;

typedef void (*lv_gpu_acts_wait_finish_t)(lv_draw_ctx_t * draw_ctx);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * During rendering, LVGL might initializes new draw_ctxs and start drawing into
 * a separate buffer (called layer). If the content to be rendered has "holes",
 * e.g. rounded corner, LVGL temporarily sets the disp_drv.screen_transp flag.
 * It means the renderers should draw into an ARGB buffer.
 * With 32 bit color depth it's not a big problem but with 16 bit color depth
 * the target pixel format is ARGB8565 which is not supported by the GPU.
 * In this case, the VG-Lite callbacks should fallback to SW rendering.
 */
static inline bool need_argb8565_support(lv_draw_ctx_t * draw_ctx)
{
#if LV_COLOR_DEPTH != 32 && LV_COLOR_SCREEN_TRANSP
    lv_disp_t * disp = _lv_refr_get_disp_refreshing();

    if (disp->driver->screen_transp == 1)
        return true;
#endif

    return false;
}

/*
 * @brief submit (record) a new draw step.
 *
 * @retval N/A
 */
void lv_gpu_acts_timeline_submit(void);

/*
 * @brief complete one draw step.
 *
 * @retval N/A
 */
LV_ATTRIBUTE_VERY_FAST_MEM void lv_gpu_acts_timeline_inc(void);

/*
 * @brief initialize GPU draw context
 *
 * @param drv pointer to structure lv_disp_drv_t
 * @param draw_ctx pointer to structure draw_ctx
 *
 * @retval N/A
 */
void lv_draw_gpu_acts_ctx_init(lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx);

/*
 * @brief deinitialize GPU draw context
 *
 * @param drv pointer to structure lv_disp_drv_t
 * @param draw_ctx pointer to structure draw_ctx
 *
 * @retval N/A
 */
void lv_draw_gpu_acts_ctx_deinit(lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx);

/*
 * @brief set GPU accelerator type
 *
 * @param draw_ctx pointer to structure lv_draw_ctx_t
 * @param type new accelerator type
 * @param area the draw area that will be written to (absolute coords in the display)
 *
 * @retval N/A
 */
void lv_gpu_acts_set_accl_type(lv_draw_ctx_t * draw_ctx, uint8_t type, const lv_area_t * area);

/*
 * @brief invalidate the buffer cache
 *
 * @param draw_ctx pointer to structure lv_draw_ctx_t
 * @param area pointer to the invalid area (absolute coords in the display)
 *
 * @retval N/A
 */
void lv_gpu_acts_flush_dcache(lv_draw_ctx_t * draw_ctx, const lv_area_t * area);

/*
 * @brief get the final src mapped area and draw area considering transformation
 *
 * @param draw_ctx pointer to structure lv_draw_ctx_t
 * @param draw_dsc pointer to structure lv_draw_img_dsc_t
 * @param draw_area the draw area covered by src
 * @param src_area the src mapping area relative the top-left corner of the src
 * @param coords the src coords
 *
 * @retval LV_RES_OK on success else LV_RES_INV
 */
lv_res_t lv_gpu_acts_get_img_transformed_area(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        lv_area_t * draw_area, lv_area_t * src_area, const lv_area_t * coords);

/**********************
 * SW accelerator
 **********************/

/*
 * @brief SW clear API implement
 */
void lv_gpu_acts_sw_clear(uint8_t * buf, uint32_t size);

/*
 * @brief SW buffer_copy API implement
 */
void lv_gpu_acts_sw_copy(
        lv_color_t * dest, int16_t dest_stride, const lv_color_t * src,
        int16_t src_stride, int16_t copy_w, int16_t copy_h);

/*
 * @brief SW draw_img API implement
 *
 * @retval LV_RES_OK on success else LV_RES_INV
 */
lv_res_t lv_gpu_acts_sw_draw_img_dsc(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src);

/**********************
 * DMA2D accelerator
 **********************/

/*
 * @brief DMA2D draw context init
 *
 * @retval 0 on success else negative errno code
 */
int lv_gpu_acts_dma2d_init(void);

/*
 * @brief DMA2D wait_for_finish API implement
 */
void lv_gpu_acts_dma2d_wait_for_finish(lv_draw_ctx_t * draw_ctx);

/*
 * @brief DMA2D init_buf API implement
 *
 * @retval N/A
 */
void lv_gpu_acts_dma2d_init_buf(lv_draw_ctx_t * draw_ctx, bool screen_transp);

/*
 * @brief DMA2D buffer_copy API implement
 *
 * @retval LV_RES_OK on success else LV_RES_INV
 */
lv_res_t lv_gpu_acts_dma2d_copy(
        lv_color_t * dest, int16_t dest_stride, const lv_color_t * src_buf,
        int16_t src_stride, int16_t copy_w, int16_t copy_h);

/*
 * @brief DMA2D draw_img API implement
 *
 * @retval LV_RES_OK on success else LV_RES_INV
 */
lv_res_t lv_gpu_acts_dma2d_draw_img_dsc(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src);

/*
 * @brief DMA2D draw_letter API implement
 *
 * @retval LV_RES_OK on success else LV_RES_INV
 */
lv_res_t lv_gpu_acts_dma2d_draw_letter(
        lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,
        const lv_area_t * area_p, lv_font_glyph_dsc_t * g, const uint8_t * map_p);

/*
 * @brief DMA2D blend API implement
 *
 * @retval LV_RES_OK on success else LV_RES_INV
 */
lv_res_t lv_gpu_acts_dma2d_blend(
        lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc);

/**********************
 * JPEG accelerator
 **********************/

/*
 * @brief JPEG draw context init
 *
 * @retval 0 on success else negative errno code
 */
int lv_gpu_acts_jpg_init(void);

/*
 * @brief JPEG wait_for_finish API implement
 */
void lv_gpu_acts_jpg_wait_for_finish(lv_draw_ctx_t * draw_ctx);

/*
 * @brief JPEG draw_img API implement
 *
 * @retval LV_RES_OK on success else LV_RES_INV
 */
lv_res_t lv_gpu_acts_jpg_draw_img_dsc(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src);


/*
 * @brief decompress draw_img API implement
 *
 * @retval LV_RES_OK on success else LV_RES_INV
 */
lv_res_t lv_gpu_acts_decompress_draw_img_dsc(
        lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
        const lv_area_t * coords, const lv_img_dsc_t * src);

/**********************
 *      MACROS
 **********************/
#endif /*LV_USE_GPU_ACTS*/

/**
 * INTERNAL_HIDDEN @endcond
 */

#ifdef __cplusplus
}
#endif

#endif /* PORTING_GPU_LV_GPU_ACTS_H */
