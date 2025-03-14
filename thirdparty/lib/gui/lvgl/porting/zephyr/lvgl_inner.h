/**
 * @file lvgl_inner.h
 *
 */

#ifndef PORTING_ZEPHYR_LVGL_INNER_H
#define PORTING_ZEPHYR_LVGL_INNER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../lvgl.h"
#include "../lvgl_porting.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief Initialize LVGL display porting
 *
 * @retval 0 on success else negative code.
 */
int lvgl_port_disp_init(void);

/**
 * @brief Initialize LVGL indev pointer porting
 *
 * @retval 0 on success else negative code.
 */
int lvgl_port_indev_pointer_init(void);

/**
 * @brief Period callback to put pointer to indev pointer buffer queue
 *
 * @retval N/A.
 */
void lvgl_port_indev_pointer_scan(void);

/**
 * @brief Initialize LVGL zephyr file system
 *
 * @retval N/A.
 */
void lvgl_port_z_fs_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*PORTING_ZEPHYR_LVGL_INNER_H*/
