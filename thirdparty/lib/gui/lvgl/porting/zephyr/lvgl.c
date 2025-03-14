/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#define LOG_MODULE_CUSTOMER

#include <os_common_api.h>
#include <zephyr.h>
#include "lvgl_inner.h"
#include "../decoder/lv_img_decoder_acts.h"
#ifdef CONFIG_UI_MEMORY_MANAGER
#  include LV_MEM_CUSTOM_INCLUDE
#endif
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

#define LOG_LEVEL CONFIG_LV_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(lvgl);


/**********************
 *  STATIC FUNCTIONS
 **********************/
/*
 * In LVGLv8 the signature of the logging callback has changes and it no longer
 * takes the log level as an integer argument. Instead, the log level is now
 *
 * already part of the buffer passed to the logging callback. It's not optimal
 * but we need to live with it and parse the buffer manually to determine the
 * level and then truncate the string we actually pass to the logging framework.
 */
static void lvgl_log(const char *buf)
{
#if 1
	/*
	 * This is ugly and should be done in a loop or something but as it
	 * turned out, Z_LOG()'s first argument (that specifies the log level)
	 * cannot be an l-value...
	 *
	 * We also assume lvgl is sane and always supplies the level string.
	 */
	switch (buf[1]) {
	case 'E':
		LOG_ERR("%s", buf + strlen("[Error] "));
		break;
	case 'W':
		LOG_WRN("%s", buf + strlen("Warn] "));
		break;
	case 'I':
		LOG_INF("%s", buf + strlen("[Info] "));
		break;
	case 'T':
		LOG_DBG("%s", buf + strlen("[Trace] "));
		break;
	case 'U':
		LOG_WRN("%s", buf);
		break;
	default:
		LOG_WRN("%s", buf);
		break;
	}
#else
	os_printk("%s", buf);
#endif
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
lv_res_t lv_port_init(void)
{
#if !defined(CONFIG_UI_SERVICE) && defined(CONFIG_UI_MEMORY_MANAGER)
	ui_mem_init();
#endif

#if defined(CONFIG_LV_Z_MEM_POOL_SYS_HEAP) && !defined(CONFIG_UI_MEMORY_MANAGER)
	lvgl_heap_init();
#endif

	lv_log_register_print_cb(lvgl_log);

	lv_init();
	lv_img_decoder_acts_basic_init();

#if defined(CONFIG_LVGL_USE_IMG_DECODER_ACTS)
	lvgl_img_decoder_acts_init();
#elif LV_USE_GPU_ACTS_JPG
	lv_img_decoder_acts_raw_init();
#endif

#if defined(CONFIG_LVGL_USE_IMG_DECODER_ACTS_RES)
	lvgl_img_decoder_acts_res_init();
#endif

#if defined(CONFIG_LV_Z_USE_FILESYSTEM)
	lvgl_port_z_fs_init();
#endif

#if LV_USE_THORVG
    tvg_engine_init(TVG_ENGINE_SW, 0);
#endif

#if !defined(CONFIG_UI_SERVICE)
	if (lvgl_port_disp_init()) {
		return LV_RES_INV;
	}

	lvgl_port_indev_pointer_init();
#endif

	return LV_RES_OK;
}
