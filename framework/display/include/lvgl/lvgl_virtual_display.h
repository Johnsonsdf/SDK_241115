/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FRAMEWORK_DISPLAY_LIBDISPLAY_LVGL_LVGL_VIRTUAL_DISPLAY_H
#define FRAMEWORK_DISPLAY_LIBDISPLAY_LVGL_LVGL_VIRTUAL_DISPLAY_H

#include <ui_surface.h>
#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a lvgl virtual display
 *
 * The requested pixel format must have the same color depth as LV_COLOR_DEPTH,
 * so no color transform is required between lvgl and the display surface.
 *
 * @param surface pointer to the display surface structure
 *
 * @return pointer to the created display; NULL if failed.
 */
lv_disp_t *lvgl_virtual_display_create(surface_t *surface);

/**
 * @brief Destroy a lvgl virtual display
 *
 * @param disp pointer to a lvgl display
 *
 * @return N/A
 */
void lvgl_virtual_display_destroy(lv_disp_t *disp);

/**
 * @brief Set a lvgl virtual display as default display
 *
 * Set display as default does not mean it is focuesd and has input events.
 * It is just for convenience of LVGL user, like calling lv_scr_act().
 *
 * @param disp pointer to a lvgl display
 *
 * @retval 0 on success else negative errno code.
 */
int lvgl_virtual_display_set_default(lv_disp_t *disp);

/**
 * @brief Focus on a lvgl virtual display
 *
 * @param disp pointer to a lvgl display
 * @param reset_indev should reset the indev or not
 *
 * @retval 0 on success else negative errno code.
 */
int lvgl_virtual_display_set_focus(lv_disp_t *disp, bool reset_indev);

/**
 * @brief Get display workq for delayed flush
 *
 * @retval pointer to workq.
 */
os_work_q * lvgl_display_get_flush_workq(void);

#ifdef __cplusplus
}
#endif

#endif /* FRAMEWORK_DISPLAY_LIBDISPLAY_LVGL_LVGL_VIRTUAL_DISPLAY_H */
