/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for display composer
 */

#ifndef ZEPHYR_FRAMEWORK_INCLUDE_DISPLAY_DISPLAY_COMPOSER_H_
#define ZEPHYR_FRAMEWORK_INCLUDE_DISPLAY_DISPLAY_COMPOSER_H_

/**
 * @brief Display Composer Interface
 * @defgroup display_composer_interface Display Composer Interface
 * @ingroup display_libraries
 * @{
 */

/*********************
 *      INCLUDES
 *********************/

#include <stdint.h>
#include <drivers/display.h>
#include <drivers/display/display_graphics.h>
#include <display/graphic_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

#ifdef CONFIG_DISPLAY_ENGINE_LARK
#  define NUM_POST_LAYERS   (2)
#else
#  define NUM_POST_LAYERS   (3)
#endif

/**********************
 *      TYPEDEFS
 **********************/

/*
 * @brief Display composer flags
 *
 */
enum display_composer_flags {
	/* first post in one frame */
	FIRST_POST_IN_FRAME = BIT(0),
	/* last post in one frame */
	LAST_POST_IN_FRAME  = BIT(1),
	/* post using DE path.
	 * posting by DE has much higher efficiency than DMA, but may be affected
	 * by drawing, since DMA2D HAL is accelerated by DE.
	 */
	POST_PATH_BY_DE = BIT(7),

	POST_FULL_FRAME = FIRST_POST_IN_FRAME | LAST_POST_IN_FRAME,
};

/**
 * @typedef display_composer_post_cleanup_t
 * @brief Callback API executed when post cleanup
 *
 */
typedef void (*display_composer_post_cleanup_t)(void *user_data);

/**
 * @struct ui_layer
 * @brief Structure holding ui layer for composition
 *
 */
typedef struct ui_layer {
	/* pointer to graphic buffer */
	graphic_buffer_t *buffer;

	/* area of the source to consider, the origin is the top-left corner of the buffer. */
	ui_region_t crop;

	/* where to composite the crop onto the display, the origin is the top-left corner
	 * of the screen. So far, the size of frame must equal to the crop's, since scaling
	 * not supported by hardware.
	 */
	ui_region_t frame;

	/* blending to apply during composition */
	uint8_t blending;

	/* panel alpha */
	uint8_t plane_alpha;

	/* user post cleanup cb */
	display_composer_post_cleanup_t cleanup_cb;
	void *cleanup_data;
} ui_layer_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief Initialize the display composer
 *
 * @return 0 on success else negative errno code.
 */
int display_composer_init(void);

/**
 * @brief Destroy the display composer
 *
 * @return N/A.
 */
void display_composer_destroy(void);

/**
 * @brief Register the display composer callback
 *
 * The callback will be called when one frame complete.
 *
 * @param callback callback function
 *
 * @return N/A.
 */
void display_composer_register_callback(const struct display_callback *callback);

/**
 * @brief Set post period
 *
 * @multiple how many vsync periods to post one frame
 *
 * @return N/A
 */
void display_composer_set_post_period(uint8_t multiple);

/**
 * @brief Get actual display refresh rate
 *
 * @return refresh rate in Hz
 */
uint8_t display_composer_get_refresh_rate(void);

/**
 * @brief Get geometry of the display
 *
 * @param width address to store the x resolution
 * @param height address to store the y resolution
 * @param pixel_format address to store the pixel format
 * @param round_screen address to store screen shape
 *
 * @return 0 on success else negative code.
 */
int display_composer_get_geometry(uint16_t *width, uint16_t *height,
			uint32_t *pixel_format, uint8_t *round_screen);

/**
 * @brief Get display orientation
 *
 * @return rotation (CW) in degrees
 */
uint16_t display_composer_get_orientation(void);

/**
 * @brief Get number of layers supported
 *
 * @return number of layers supported.
 */
uint8_t display_composer_get_num_layers(void);

/**
 * @brief Set the brightness of the display
 *
 * Set the brightness of the display in steps of 1/256, where 255 is full
 * brightness and 0 is minimal.
 *
 * @param brightness Brightness in steps of 1/256
 *
 * @retval 0 on success else negative errno code.
 */
int display_composer_set_brightness(uint8_t brightness);

/**
 * @brief Extend the invalidated region to match with the display requirements
 *
 * @param region the invalidated/dirty region of the display
 *
 * @retval N/A
 */
void display_composer_round(ui_region_t *region);

/**
 * @brief Post the layers to display
 *
 * This routine may be blocked in thread context.
 *
 * @param layers layer array to post
 * @param num_layers number of layers to post
 * @param post_flags post flags, see enum display_composer_flags
 *
 * @return 0 on success else negative errno code.
 */
int display_composer_post(const ui_layer_t *layers, int num_layers, uint32_t post_flags);

/**
 * @brief Flush to display
 *
 * This routine can be used in AOD mode without vsync/TE enabled.
 *
 * @param timeout timeout in milliseconds
 *
 * @retval number of frames flushed on success else negative errno code.
 */
int display_composer_flush(unsigned int timeout);

#ifdef __cplusplus
}
#endif
/**
 * @}
 */

#endif /* ZEPHYR_FRAMEWORK_INCLUDE_DISPLAY_DISPLAY_COMPOSER_H_ */
