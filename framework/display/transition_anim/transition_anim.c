/**
 * @file transimition_anim.c
 *
 */
/*********************
 *      INCLUDES
 *********************/
#include <lvgl/lvgl_memory.h>
#include <memory/mem_cache.h>
#include "transition_anim.h"

/* cache line 32 bytes */
#define IMG_DATA_HW_ALIGN 32U

static transition_anim_t transition_anim;

static void _transition_anim_free_img(transition_anim_t *a)
{
	if (a->exit_view_image_dsc.data) {
		lv_mem_free((uint8_t *)a->exit_view_image_dsc.data - a->exit_view_image_data_ofs);
		a->exit_view_image_dsc.data = NULL;
	}
}

static void _transition_anim_status_cb(lv_event_t * e)
{
	lv_event_code_t event = lv_event_get_code(e);
	transition_anim_t *a = (transition_anim_t *)lv_event_get_user_data(e);

	if (event == LV_EVENT_SCREEN_UNLOADED) {
		SYS_LOG_INF("transition animation finished\n");
#if CONFIG_LV_USE_SNAPSHOT
		_transition_anim_free_img(a);
#endif
	}
}

void transition_anim_prepare(view_data_t *exit_view_data)
{
	transition_anim_t *a = &transition_anim;

#if CONFIG_LV_USE_SNAPSHOT
	if (a->transition_anim_prepared) {
		_transition_anim_free_img(a);
	}
#endif

	memset(a, 0, sizeof(transition_anim_t));

#if CONFIG_LV_USE_SNAPSHOT
	// prepare exit view image
	lv_obj_t *scr = lv_disp_get_scr_act(exit_view_data->display);
	uint32_t buf_size = lv_snapshot_buf_size_needed(scr, LV_IMG_CF_TRUE_COLOR);
	uint8_t * buf = lv_mem_alloc(UI_ROUND_UP(buf_size, IMG_DATA_HW_ALIGN) + IMG_DATA_HW_ALIGN);

	if (buf != NULL) {
		uint8_t * buf_aligned = (void *)UI_ROUND_UP((uintptr_t)buf, IMG_DATA_HW_ALIGN);

		mem_dcache_clean(buf_aligned, buf_size);
		mem_dcache_sync();

		lv_res_t res = lv_snapshot_take_to_buf(scr, LV_IMG_CF_TRUE_COLOR,
							&a->exit_view_image_dsc, buf_aligned, buf_size);
		if (res == LV_RES_INV) {
			SYS_LOG_WRN("snapshot take failed");
			lv_mem_free(buf);
		} else {
			a->transition_anim_prepared = 1;
			a->exit_view_image_data_ofs = buf_aligned - buf;
			mem_dcache_clean(buf_aligned, buf_size);
			mem_dcache_sync();
		}
	} else {
		SYS_LOG_WRN("snapshot buf alloc failed");
	}
#endif
}

void transition_anim_unprepare(void)
{
	transition_anim_t *a = &transition_anim;

	if (a->transition_anim_prepared) {
		_transition_anim_free_img(a);
		a->transition_anim_prepared = 0;
	}
}

void transition_anim_start(view_data_t *view_data, transition_anim_type_t exit_anim_type, transition_anim_type_t enter_anim_type)
{
	transition_anim_t *a = &transition_anim;
	if (!a->transition_anim_prepared)
		return;

	lv_disp_set_default(view_data->display);
	lv_obj_t * scr_act = lv_scr_act();

	a->exit_view_image = lv_img_create(NULL);
	lv_img_set_src(a->exit_view_image, &a->exit_view_image_dsc);

	lv_obj_add_event_cb(a->exit_view_image, _transition_anim_status_cb, LV_EVENT_SCREEN_UNLOADED, a);

	lv_disp_load_scr(a->exit_view_image);
	lv_scr_load_anim(scr_act, enter_anim_type, TRANSITION_ANIMATION_DURATION, 0 , true);

	a->transition_anim_prepared = 0;
}
