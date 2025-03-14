/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PORTING_LVGL_PORTING_H
#define PORTING_LVGL_PORTING_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../lvgl.h"

/**********************
 *      TYPEDEFS
 **********************/
enum {
	LV_PORT_PM_STATE_ACTIVE = 0,
	LV_PORT_PM_STATE_IDLE,
	LV_PORT_PM_STATE_OFF,
};

/* GPU Event callback */
typedef void (*lv_port_gpu_event_cb_t)(void * user_data);

/* Display Event callback */
typedef struct lv_port_disp_callback {
	/* Vsync/TE callback: timestamp: measured in sys clock cycles */
	void (*vsync)(const struct lv_port_disp_callback *callback, uint32_t timestamp);
	/* (*pm_notify) is called when pm state changed */
	void (*pm_notify)(const struct lv_port_disp_callback *callback, uint32_t pm_state);
} lv_port_disp_callback_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*
 * @brief Initialize LVGL
 *
 * Only required called once.
 *
 * @retval LV_RES_OK on success else LV_RES_INV. (always succeed)
 */
lv_res_t lv_port_init(void);

/*
 * @brief Register LVGL Display callback
 *
 * Only required for physical and virtual displays which are registerd in lv_port_init().
 *
 * @param callback pointer to structure lv_port_disp_callback_t
 *
 * @retval LV_RES_OK on success else LV_RES_INV. (always succeed)
 */
lv_res_t lv_port_disp_register_callback(const lv_port_disp_callback_t *callback);

/*
 * @brief Wait flush finish
 *
 * @retval LV_RES_OK on success else LV_RES_INV.
 */
lv_res_t lv_port_disp_flush_wait(void);

/*
 * @brief Wait flush finish
 *
 * @param buf the buffer to store image data.
 * @param buf_size provided buffer size in bytes.
 *
 * @retval LV_RES_OK on success else LV_RES_INV.
 */
lv_res_t lv_port_disp_read_to_buf(void *buf, uint32_t buf_size);

/*
 * @brief Prepare overlay flushing
 *
 * @retval fb address on success else NULL.
 */
void * lv_port_disp_ovelay_prepare(void);

/*
 * @brief Unprepare overlay flushing
 *
 * @retval LV_RES_OK on success else LV_RES_INV.
 */
lv_res_t lv_port_disp_ovelay_unprepare(void);

/*
 * @brief Flush using overlay
 *
 * @param bg_col background color map
 * @param bg_area background area
 * @param fg_col foreground color map
 * @param fg_area foreground area
 * @param fg_opa foreground opacity
 *
 * @retval LV_RES_OK on success else LV_RES_INV.
 */
lv_res_t lv_port_disp_overlay_flush(const lv_color_t *bg_col, const lv_area_t *bg_area,
                                    const lv_color_t *fg_col, const lv_area_t *fg_area,
                                    lv_opa_t fg_opa);

/*
 * @brief Reflush full-screen
 *
 * @retval LV_RES_OK on success else LV_RES_INV.
 */
lv_res_t lv_port_disp_flush_screen(void);

/*
 * @brief Initialize LVGL GPU interface
 *
 * Only required for virtual displays which are not registerd in lv_port_init().
 *
 * @param drv pointer to structure lv_disp_drv_t
 *
 * @retval LV_RES_OK on success else LV_RES_INV. (always succeed)
 */
lv_res_t lv_port_gpu_init(lv_disp_drv_t * drv);

/*
 * @brief Insert LVGL GPU event.
 *
 * Only required for virtual display.
 *
 * @param event_cb event callback
 * @param user_data user specific data
 *
 * @retval event id on success else negative value.
 */
int lv_port_gpu_insert_event(lv_port_gpu_event_cb_t event_cb, void * user_data);

 /*
 * @brief Wit a GPU event
 *
 * Only required for virtual display.
 *
 * @param event_id event id. set negative to wait all the events.
 *
 * @retval LV_RES_OK on success else LV_RES_INV. (always succeed)
 */
lv_res_t lv_port_gpu_wait_event(int event_id);

/*
 * @brief set refresh display manually, not by timer
 *
 * @param disp pointer to display which refresh manually. Set NULL to indicate
 *             all the displays.
 *
 * @retval N/A
 */
void lv_port_disp_set_manual(lv_disp_t * disp);

/*
 * @brief set refresh display/indev manually, not by timer
 *
 * @retval N/A
 */
void lv_port_refr_set_manual(void);

/*
 * @brief refresh display/indev immediately
 *
 * @retval N/A
 */
void lv_port_refr_now(void);

/*
 * @brief Make display/indev refresh timer ready, will not wait their periods
 *
 * @retval N/A
 */
void lv_port_refr_ready(void);

/*
 * @brief make display/indev refresh timer pause, will not wait their periods
 *
 * @retval N/A
 */
void lv_port_refr_pause(void);

/*
 * @brief make display/indev refresh timer resume, will not wait their periods
 *
 * @retval N/A
 */
void lv_port_refr_resume(void);

/*
 * @brief Update display/indev refresh period
 *
 * @param period new period in milliseconds.
 *
 * @retval N/A
 */
void lv_port_update_refr_period(uint32_t period);

#ifdef __cplusplus
}
#endif

#endif /* PORTING_LVGL_PORTING_H */
