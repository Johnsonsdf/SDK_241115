/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WIDGETS_DRAW_UTIL_H_
#define WIDGETS_DRAW_UTIL_H_

/*********************
 *      INCLUDES
 *********************/
#include <sys/util.h>
#include <display/sw_draw.h>
#include <display/sw_math.h>
#include <lvgl/lvgl.h>

/*********************
 *      DEFINES
 *********************/
/* emoji unicode range start */
#define EMOJI_UNICODE_START	(0x1F300)

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
	int32_t line_count; /* Number of lines that really drawn */
	const char * remain_txt; /* The start of remain text not drawn (clip by bottom y2) */
} lvgl_draw_label_result_t;

typedef struct {
	uint32_t code;
	lv_point_t pos;
	lv_img_dsc_t img;
} lvgl_emoji_dsc_t;

typedef struct {
	lvgl_emoji_dsc_t * emoji;
	const lv_font_t * font;

	uint16_t count;
	uint16_t max_count;
} lvgl_draw_emoji_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/*
 * split a decimal integer into digits
 *
 * @param value decimal integer value
 * @param buf buffer to store the digits
 * @param len buffer len
 * @param min_w minimum split digits (may pad 0 if integer too small)
 *
 * @retval number of split digits
 */
int split_decimal(uint32_t value, uint8_t *buf, int len, int min_w);

/*
 * draw label which support emoji
 *
 * @param draw_ctx draw context
 * @param dsc pointer to draw descriptor
 * @param coords the coordinates of the text
 * @param txt `\0` terminated text to write
 * @param hint pointer to a `lv_draw_label_hint_t` variable.
 * @param feedback pointer to a `lvgl_draw_label_result_t` variable to store draw feedback
 * @param emoji_dsc pointer to a `lvgl_draw_emoji_dsc_t` variable to store emoji info
 *
 * @retval N/A
 */
void lvgl_draw_label_with_emoji(lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc,
		const lv_area_t * coords, const char * txt, lv_draw_label_hint_t * hint,
		lvgl_draw_label_result_t *feedback, lvgl_draw_emoji_dsc_t * emoji_dsc);

/**
 * Get size of a text
 * @param size_res pointer to a 'point_t' variable to store the result
 * @param text pointer to a text
 * @param font pointer to font of the text
 * @param letter_space letter space of the text
 * @param line_space line space of the text
 * @param flags settings for the text from ::lv_text_flag_t
 * @param max_width max width of the text (break the lines to fit this size). Set COORD_MAX to avoid
 * @param max_height max height of the text (break the lines to fit this size). Set COORD_MAX to avoid
 * line breaks
 *
 * @retval true if exceed max_height, otherwise false
 */
bool lvgl_txt_get_size(lv_point_t * size_res, const char * text, const lv_font_t * font, lv_coord_t letter_space,
                     lv_coord_t line_space, lv_coord_t max_width, lv_coord_t max_height, lv_text_flag_t flag);

/**
 * Test if has enough encoded length
 * @param text pointer to a text
 * @param len encoded legthe to compared
 *
 * @retval true if has enough encoded length, otherwise false
 */
bool lvgl_txt_has_encoded_length(const char * text, uint32_t len);

#endif /* WIDGETS_DRAW_UTIL_H_ */
