/**
 * @file lv_gif.h
 *
 */

#ifndef GIF_IMG_H
#define GIF_IMG_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <lvgl/lvgl.h>
#include "gif_decode.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_img_t img;
    decode_GIF * gif;
    lv_timer_t * timer;
    lv_img_dsc_t imgdsc;
    uint32_t last_call;
    uint32_t pause_offset;
} gif_img_t;

extern const lv_obj_class_t lv_gif_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

lv_obj_t * gif_img_create(lv_obj_t * parent);
void gif_img_set_src(lv_obj_t * obj, const void * src);
void gif_img_restart(lv_obj_t * gif);
void gif_img_set_pause(lv_obj_t * obj);
void gif_img_set_resume(lv_obj_t * obj);
bool gif_img_get_paused(lv_obj_t * obj);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_GIF_H*/
