/**
 * @file transition_anim.h
 *
 */

#ifndef _TRANSIMITION_ANIM_H
#define _TRANSIMITION_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <ui_manager.h>
#include <lvgl/lvgl_res_loader.h>

#define TRANSITION_ANIMATION_DURATION 500

typedef enum {
	TRANSITION_ANIM_NONE,
	TRANSITION_ANIM_OVER_LEFT,
	TRANSITION_ANIM_OVER_RIGHT,
	TRANSITION_ANIM_OVER_TOP,
	TRANSITION_ANIM_OVER_BOTTOM,
	TRANSITION_ANIM_MOVE_LEFT,
	TRANSITION_ANIM_MOVE_RIGHT,
	TRANSITION_ANIM_MOVE_TOP,
	TRANSITION_ANIM_MOVE_BOTTOM,
	TRANSITION_ANIM_FADE_IN,
	TRANSITION_ANIM_FADE_ON = TRANSITION_ANIM_FADE_IN, /*For backward compatibility*/
	TRANSITION_ANIM_FADE_OUT,
	TRANSITION_ANIM_OUT_LEFT,
	TRANSITION_ANIM_OUT_RIGHT,
	TRANSITION_ANIM_OUT_TOP,
	TRANSITION_ANIM_OUT_BOTTOM,
} transition_anim_type_t;

/** Describes an animation*/
typedef struct _transition_anim_t {
	// lv obj of exit view image
	lv_obj_t *exit_view_image;
	// img dsc for exit view screen shot
	lv_img_dsc_t exit_view_image_dsc;
	// data offset to keep image data aligned
	uint8_t exit_view_image_data_ofs;
	// flag for animation prepared
	uint8_t transition_anim_prepared:1;

} transition_anim_t;

/**
 * @brief prepare for transition animation
 *
 * @param exit_view_data view data of exit view
 *
 * @return N/A
 */

void transition_anim_prepare(view_data_t *exit_view_data);

/**
 * @brief cleanup the prepared transition animation
 *
 * @return N/A
 */

void transition_anim_unprepare(void);

/**
 * @brief prepare for transition animation
 *
 * @param view_data view data of enter view
 * @param exit_anim_type animation type of exit view
 * @param enter_anim_type animation type of enter view
 *
 * @return N/A
 */
void transition_anim_start(view_data_t *view_data, transition_anim_type_t exit_anim_type, transition_anim_type_t enter_anim_type);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*transition_ANIM_H*/
