/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief lvgl memory
 */

#ifndef FRAMEWORK_DISPLAY_INCLUDE_LVGL_MEMORY_H_
#define FRAMEWORK_DISPLAY_INCLUDE_LVGL_MEMORY_H_

/**
 * @defgroup lvgl_memory_apis LVGL Memory APIs
 * @ingroup lvgl_apis
 * @{
 */

/*********************
 *      INCLUDES
 *********************/
#include <lvgl/lvgl.h>
#ifdef CONFIG_VG_LITE
#  include <vg_lite/vg_lite.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief Clean cache of lvgl img buf.
 *
 * @param dsc pointer to an lvgl image descriptor.
 *
 * @retval N/A.
 */
void lvgl_img_buf_clean_cache(const lv_img_dsc_t *dsc);

/**
 * @brief Invalidate cache of lvgl img buf.
 *
 * @param dsc pointer to an lvgl image descriptor.
 *
 * @retval N/A.
 */
void lvgl_img_buf_invalidate_cache(const lv_img_dsc_t *dsc);

/**
 * @brief Query if the img buf is cache clean
 *
 * @param buf address of img buffer.
 * @param size size of img buffer
 *
 * @retval true if cache clean or not cachable else false
 */
bool lvgl_img_buf_is_clean(const void *buf, unsigned int size);

/**
 * @brief Convert lvgl image cf to hal pixel format
 *
 * @param cf lvgl image cf.
 * @param bits_per_pixel optional address to store the bits_per_pixel of format.
 *
 * @retval hal pixel format
 */
uint32_t lvgl_img_cf_to_display(lv_img_cf_t cf, uint8_t * bits_per_pixel);

/**
 * @brief Convert lvgl color to ABGR-8888
 *
 * @param color lvgl color.
 *
 * @retval result color.
 */
static inline uint32_t lvgl_color_to_abgr32(lv_color_t color)
{
	uint32_t col32 = lv_color_to32(color);
	return (col32 & 0xff00ff00) | ((col32 & 0xff) << 16) | ((col32 & 0xff0000) >> 16);
}

#ifdef CONFIG_VG_LITE

/**
 * @brief Convert lvgl image cf to vglite buffer format
 *
 * @param cf lvgl image cf.
 * @param bits_per_pixel optional address to store the bits_per_pixel of format.
 *
 * @retval vglite buffer format
 */
int lvgl_img_cf_to_vglite(lv_img_cf_t cf, uint8_t * bits_per_pixel);

/**
 * @brief Map a lvgl color buf to vglite buffer
 *
 * @param vgbuf pointer to structure vg_lite_buffer_t.
 * @param color_p pointer to lvgl color buffer.
 * @param w width of lvgl buffer in pixels.
 * @param h height of lvgl buffer in pixels.
 * @param stride stride of lvgl buffer in pixels.
 * @param cf color format of lvgl buffer.
 *
 * @retval query result
 */
int lvgl_vglite_map(vg_lite_buffer_t *vgbuf, const void *color_p,
		uint16_t w, uint16_t h, uint16_t stride, lv_img_cf_t cf);

/**
 * @brief Map a lvgl image to vglite buffer
 *
 * @param vgbuf pointer to structure vg_lite_buffer_t.
 * @param img_dsc pointer to structure lv_img_dsc_t.
 *
 * @retval 0 on success else negative code.
 */
int lvgl_vglite_map_img(vg_lite_buffer_t *vgbuf, const lv_img_dsc_t *img_dsc);

/**
 * @brief Unmap a vglite buffer
 *
 * @param vgbuf pointer to structure vg_lite_buffer_t.
 *
 * @retval 0 on success else negative code.
 */
int lvgl_vglite_unmap(vg_lite_buffer_t *vgbuf);

/**
 * @brief Set clut for lvgl indexed image
 *
 * @param img_dsc pointer to structure lv_img_dsc_t.
 *
 * @retval 0 on success else negative code.
 */
int lvgl_vglite_set_img_clut(const lv_img_dsc_t *img_dsc);

#endif /* CONFIG_VG_LITE */

#ifdef __cplusplus
}
#endif
/**
 * @}
 */

#endif /* FRAMEWORK_DISPLAY_INCLUDE_LVGL_MEMORY_H_ */
