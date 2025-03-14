/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FRAMEWORK_DISPLAY_UI_SERVICE_EFFECTS_INNER_H_
#define FRAMEWORK_DISPLAY_UI_SERVICE_EFFECTS_INNER_H_

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <ui_manager.h>

#ifdef CONFIG_DMA2D_HAL
#  include <dma2d_hal.h>
#endif
#ifdef CONFIG_VG_LITE
#  include <vg_lite/vglite_util.h>
#endif
#ifdef CONFIG_UI_SCROLL_EFFECT
#  include <ui_effects/scroll_effect.h>
#endif
#ifdef CONFIG_UI_SWITCH_EFFECT
#  include <ui_effects/switch_effect.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/
#define FRAME_DIGS 10
#define FRAME_FIXP(x) ((x) << FRAME_DIGS)
#define FRAME_STEP    FRAME_FIXP(1)

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
	uint8_t type; /* effect type */

#ifdef CONFIG_VG_LITE
	bool param_inited;
	/* set true on setting effect type if one buffer's foreground content
	 * overlapped with another buffer's corner background garbage.
	 */
	bool opt_round_screen_overlapped;

	float camera_distance;

	/* SCROLL_EFFECT_CUBE */
	struct {
		int16_t look_down;			/*look down value*/
		int16_t look_down_coord;	/*look down center compensation coord*/
		uint16_t dynamic_angle;		/*dynamic execute angle scope:0-(angle/2)*/
		uint8_t alpha;
		float cube_zoom;			/*zoom value >1.0 magnify , <1.0 lessen*/
		float angle;
		float radius;
		uint8_t out :	1;
	} cube;

	struct {
		float angle; /* range (0, 90] */
		float radius;
		float pivot_x;
		float pivot_y;
	} fan;

	struct {
		float min_ratio;
	} scale;
#endif
} scroll_effect_ctx_t;

typedef struct {
	uint8_t type; /* effect type */

#ifdef CONFIG_VG_LITE
	/* internal flags */
	uint8_t blend : 1;
	uint8_t dither : 1;

	struct {
		uint8_t global_alpha;
		bool fade;
	} alpha;
#endif
} vscroll_effect_ctx_t;

typedef struct {
	uint8_t type; /* effect type */
	uint8_t new_type; /* new effect type to applied later */
	uint8_t trans_end; /* 1: end */
	uint8_t tp_pressed; /* previous frame has touch pressed or not */
	int32_t frame; /* current frame count in fixed-point for interpolation convenience */
	int32_t max_frames; /* maximum frames in fixed-point */

#ifdef CONFIG_VG_LITE
	/* set true on setting effect type if one buffer's foreground content
	 * overlapped with another buffer's corner background garbage.
	 */
	bool opt_round_screen_overlapped;

	float camera_distance;

	struct {
		float angle; /* range (0, 90.0f] */
		float radius;
		float pivot_x;
		float pivot_y;
	} fan;
#endif

#ifdef CONFIG_DMA2D_HAL
	hal_dma2d_handle_t dma2d;
	bool dma2d_inited;
#endif
} switch_effect_ctx_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
int32_t switch_effects_path_cos(int32_t start, int32_t end);

#ifdef CONFIG_VG_LITE
void vglite_scroll_proc_cube_effect(scroll_effect_ctx_t *ctx, const ui_transform_param_t *param);
void vglite_scroll_proc_fan_effect(scroll_effect_ctx_t *ctx, const ui_transform_param_t *param);
void vglite_scroll_proc_page_effect(scroll_effect_ctx_t *ctx, const ui_transform_param_t *param);
void vglite_scroll_proc_scale_effect(scroll_effect_ctx_t *ctx, const ui_transform_param_t *param);

void vglite_vscroll_proc_alpha_effect(vscroll_effect_ctx_t *ctx, const ui_transform_param_t *param);

int vglite_switch_proc_fan_effect(switch_effect_ctx_t *ctx, const ui_transform_param_t *param);
int vglite_switch_proc_scale_effect(switch_effect_ctx_t *ctx, const ui_transform_param_t *param);
int vglite_switch_proc_page_effect(switch_effect_ctx_t *ctx, const ui_transform_param_t *param);
int vglite_switch_proc_zoom_alpha(switch_effect_ctx_t *ctx, const ui_transform_param_t *param);
#endif /* CONFIG_VG_LITE */

#ifdef __cplusplus
}
#endif
#endif /*FRAMEWORK_DISPLAY_UI_SERVICE_EFFECTS_INNER_H_*/
