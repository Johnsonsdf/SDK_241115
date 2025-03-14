/**
 * @file lv_barcode
 *
 */

#ifndef LV_BARCODE_H
#define LV_BARCODE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../../lvgl.h"
#if LV_USE_BARCODE

#include "barcodegen.h"

/*********************
 *      DEFINES
 *********************/

extern const lv_obj_class_t lv_barcode_class;

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create an empty BAR code (an `lv_canvas`) object.
 * @param parent point to an object where to create the BAR code
 * @param width width of the BAR code
 * @param height height of the BAR code
 * @param dark_color dark color of the BAR code
 * @param light_color light color of the BAR code
 * @return pointer to the created BAR code object
 */
lv_obj_t * lv_barcode_create(lv_obj_t * parent, lv_coord_t width, lv_coord_t height,
									lv_color_t dark_color, lv_color_t light_color);

/**
 * Set the data of a BAR code object
 * @param barcode pointer to a BAR code object
 * @param data data to display
 * @param data_len length of data in bytes
 * @param type type of BAR code (enum barcodegen_Type)
 * @return LV_RES_OK: if no error; LV_RES_INV: on error
 */
lv_res_t lv_barcode_update(lv_obj_t * barcode, const void * data, uint32_t data_len, int type);

/**
 * Delete a BAR code object
 * @param qrcode pointer to a BAR code object
 */
void lv_barcode_delete(lv_obj_t * barcode);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_BARCODE*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_BARCODE_H*/
