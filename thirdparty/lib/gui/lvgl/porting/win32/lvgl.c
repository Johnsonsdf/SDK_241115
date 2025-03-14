/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include "../lvgl_porting.h"
#include "../decoder/lv_img_decoder_acts.h"

#ifdef CONFIG_LVGL_USE_IMG_DECODER_ACTS
#  include <lvgl/lvgl_img_decoder.h>
#endif

#ifdef CONFIG_LVGL_USE_IMG_DECODER_ACTS_RES
#  include <lvgl/lvgl_img_res_decoder.h>
#endif

#if LV_USE_THORVG
#  if LV_USE_THORVG_EXTERNAL
#    include <thorvg_capi.h>
#  else
#    include "../../src/extra/libs/thorvg/thorvg_capi.h"
#  endif
#endif

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
lv_res_t lv_port_init(void)
{
    lv_init();

    lv_img_decoder_acts_basic_init();

#ifdef CONFIG_LVGL_USE_IMG_DECODER_ACTS
    lvgl_img_decoder_acts_init();
#elif LV_USE_GPU_ACTS_JPG
	lv_img_decoder_acts_raw_init();
#endif

#if defined(CONFIG_LVGL_USE_IMG_DECODER_ACTS_RES)
	lvgl_img_decoder_acts_res_init();
#endif

#if LV_USE_THORVG
    tvg_engine_init(TVG_ENGINE_SW, 0);
#endif

    return LV_RES_OK;
}
