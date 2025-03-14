/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <math.h>
#include <os_common_api.h>
#include "effects_inner.h"

/*********************
 *      DEFINES
 *********************/
/**
 * Camera distance from screen
 *
 * set 0.0f for orthogonal projection)
 */
#define CAMERA_DISTANCE  512.0f

/**
 * SCROLL_EFFECT_SCALE.
 */
/* can see the cube outside or inside during rotation */
#define CUBE_OUT_ROTATE  1
/* angles between the 2 normal of buffer planes, range (0, 90] */
#if CUBE_OUT_ROTATE
#  define CUBE_ANGLE  60
#  define CUBE_CAMERA_VALUE 100
#  define CUBE_CAMERA_COORD 100
#  define CUBE_ZOOM (0.8)
#  define CUBE_DYNAMIC_ANGLE 16
#else
#  define CUBE_ANGLE  90
#endif


#define CUBE_ALPHA 0xFF

/**
 * SCROLL_EFFECT_FAN.
 */
/* angles between the 2 normal of buffers, range (0, 90] */
#define FAN_ANGLE  30

/**
 * SCROLL_EFFECT_SCALE.
 */
/* minimum scale value, range (0.0, 1.0) */
#define SCALE_MIN  0.5f

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#ifdef CONFIG_VG_LITE
static void _vglite_scroll_effect_handle(const ui_transform_param_t *param, int *trans_end);
static void _vglite_vscroll_effect_handle(const ui_transform_param_t *param, int *trans_end);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
__unused static scroll_effect_ctx_t scroll_ctx = {
	.type = UI_SCROLL_EFFECT_NONE,

#ifdef CONFIG_VG_LITE
	.opt_round_screen_overlapped = false,
	.camera_distance = CAMERA_DISTANCE,

	.cube = {
		.out = CUBE_OUT_ROTATE,
		.angle = CUBE_ANGLE,
		.alpha = CUBE_ALPHA,
#ifdef CUBE_ZOOM
		.cube_zoom = CUBE_ZOOM,
		.look_down = CUBE_CAMERA_VALUE,
		.look_down_coord = CUBE_CAMERA_COORD,
#if (CUBE_ANGLE/2) > CUBE_DYNAMIC_ANGLE
		.dynamic_angle = CUBE_DYNAMIC_ANGLE,
#else
		.dynamic_angle = 0,
#endif
#endif
	},

	.fan = {
		.angle = FAN_ANGLE,
	},

	.scale = {
		.min_ratio = SCALE_MIN,
	},
#endif
};

__unused static vscroll_effect_ctx_t vscroll_ctx = {
	.type = UI_VSCROLL_EFFECT_NONE,

#ifdef CONFIG_VG_LITE
	.alpha = {
		.global_alpha = CONFIG_UI_VIEW_OVERLAY_OPA,
		.fade = false,
	},
#endif
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int ui_scroll_effect_set_type(uint8_t type)
{
	if (type >= NUM_UI_SCROLL_EFFECTS)
		return -EINVAL;

#ifdef CONFIG_VG_LITE
	scroll_ctx.type = type;

	if (type != UI_SCROLL_EFFECT_NONE) {
		ui_manager_set_transform_scroll_callback(_vglite_scroll_effect_handle);
	} else {
		ui_manager_set_transform_scroll_callback(NULL);
	}

	return 0;
#else
	return -ENOSYS;
#endif /* CONFIG_VG_LITE */
}

int ui_vscroll_effect_set_type(uint8_t type)
{
	if (type >= NUM_UI_VSCROLL_EFFECTS)
		return -EINVAL;

#ifdef CONFIG_VG_LITE
	vscroll_ctx.type = type;

	if (type != UI_VSCROLL_EFFECT_NONE) {
		ui_manager_set_transform_vscroll_callback(_vglite_vscroll_effect_handle);
	} else {
		ui_manager_set_transform_vscroll_callback(NULL);
	}

	return 0;
#else
	return -ENOSYS;
#endif /* CONFIG_VG_LITE */
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
#ifdef CONFIG_VG_LITE
static void _vglite_init_effect_param(const ui_transform_param_t *param)
{
	if (!scroll_ctx.param_inited) {
		scroll_ctx.param_inited = true;

		if (param->rotation == 0 || param->rotation == 180) {
			scroll_ctx.fan.radius = (param->dst->height / 2.0f) +
					((param->dst->width / 2.0f) / tan(RAD(scroll_ctx.fan.angle/2.0f)));
		} else {
			scroll_ctx.fan.radius = (param->dst->width / 2.0f) +
					((param->dst->height / 2.0f) / tan(RAD(scroll_ctx.fan.angle/2.0f)));
		}

		scroll_ctx.fan.pivot_x = param->dst->width / 2.0f;
		scroll_ctx.fan.pivot_y = param->dst->height / 2.0f;
		if (param->rotation == 0) {
			scroll_ctx.fan.pivot_y += scroll_ctx.fan.radius;
		} else if (param->rotation == 90) {
			scroll_ctx.fan.pivot_x -= scroll_ctx.fan.radius;
		} else if (param->rotation == 180) {
			scroll_ctx.fan.pivot_y -= scroll_ctx.fan.radius;
		} else /* (param->rotation == 270) */ {
			scroll_ctx.fan.pivot_x += scroll_ctx.fan.radius;
		}

		if (param->rotation == 0 || param->rotation == 180) {
			scroll_ctx.cube.radius = (param->dst->width / 2.0f) / tan(RAD(scroll_ctx.cube.angle/2.0f));
		} else {
			scroll_ctx.cube.radius = (param->dst->height / 2.0f) / tan(RAD(scroll_ctx.cube.angle/2.0f));
		}
	}
}

static void _vglite_scroll_effect_handle(const ui_transform_param_t *param, int *trans_end)
{
	os_strace_u32x2(SYS_TRACE_ID_VIEW_SCROLL_EFFECT, scroll_ctx.type, ui_region_get_width(&param->region_old));

	_vglite_init_effect_param(param);

	switch (scroll_ctx.type) {
	case UI_SCROLL_EFFECT_CUBE:
		vglite_scroll_proc_cube_effect(&scroll_ctx, param);
		break;
	case UI_SCROLL_EFFECT_FAN:
		vglite_scroll_proc_fan_effect(&scroll_ctx, param);
		break;
	case UI_SCROLL_EFFECT_PAGE:
		vglite_scroll_proc_page_effect(&scroll_ctx, param);
		break;
	case UI_SCROLL_EFFECT_SCALE:
		vglite_scroll_proc_scale_effect(&scroll_ctx, param);
		break;
	default:
		break;
	}

	os_strace_end_call(SYS_TRACE_ID_VIEW_SCROLL_EFFECT);
}

static void _vglite_vscroll_effect_handle(const ui_transform_param_t *param, int *trans_end)
{
	os_strace_u32x2(SYS_TRACE_ID_VIEW_SCROLL_EFFECT, vscroll_ctx.vtype, ui_region_get_height(&param->region_new));

	_vglite_init_effect_param(param);

	switch (vscroll_ctx.type) {
	case UI_VSCROLL_EFFECT_ALPHA:
		vglite_vscroll_proc_alpha_effect(&vscroll_ctx, param);
		break;
	default:
		break;
	}

	os_strace_end_call(SYS_TRACE_ID_VIEW_SCROLL_EFFECT);
}

#endif /* CONFIG_VG_LITE */
