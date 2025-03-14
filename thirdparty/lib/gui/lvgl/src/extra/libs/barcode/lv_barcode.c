/**
 * @file lv_barcode.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_barcode.h"
#if LV_USE_BARCODE

#include "barcodegen.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

const lv_obj_class_t lv_barcode_class = {
    .base_class = &lv_canvas_class
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Create an empty BAR code (an `lv_canvas`) object.
 * @param parent point to an object where to create the BAR code
 * @param size width and height of the BAR code
 * @param dark_color dark color of the BAR code
 * @param light_color light color of the BAR code
 * @return pointer to the created BAR code object
 */
lv_obj_t * lv_barcode_create(lv_obj_t * parent, lv_coord_t width, lv_coord_t height,
                                lv_color_t dark_color, lv_color_t light_color)
{
   uint32_t buf_size = LV_CANVAS_BUF_SIZE_INDEXED_1BIT(width, height);
   uint8_t * buf = lv_mem_alloc(buf_size);
   LV_ASSERT_MALLOC(buf);
   if(buf == NULL) return NULL;

   lv_obj_t * canvas = lv_canvas_create(parent);
   if(canvas == NULL) return NULL;

   lv_canvas_set_buffer(canvas, buf, width, height, LV_IMG_CF_INDEXED_1BIT);
   lv_canvas_set_palette(canvas, 0, light_color);
   lv_canvas_set_palette(canvas, 1, dark_color);

   return canvas;

}

/**
 * Set the data of a BAR code object
 * @param barcode pointer to a BAR code object
 * @param data data to display
 * @param data_len length of data in bytes
 * @param type type of BAR code (enum barcodegen_Type)
 * @return LV_RES_OK: if no error; LV_RES_INV: on error
 */
lv_res_t lv_barcode_update(lv_obj_t * barcode, const void * data, uint32_t data_len, int type)
{
    lv_color_t c;
    c.full = 0;
    lv_canvas_fill_bg(barcode, c, LV_OPA_COVER);

    if(data_len > barcodegen_BUFFER_LEN_MAX) return LV_RES_INV;

    uint16_t * bar0 = lv_mem_alloc(barcodegen_BUFFER_LEN_MAX);
    LV_ASSERT_MALLOC(bar0);
    uint8_t * data_tmp = lv_mem_alloc(barcodegen_DATA_LEN_MAX);
    LV_ASSERT_MALLOC(data_tmp);
    memcpy(data_tmp, data, data_len);

    uint32_t bitsLen;
    bool ok = barcodegen_encodeBinary(data_tmp, data_len, (uint8_t*)bar0, type, &bitsLen);

    if (!ok) {
        lv_mem_free(bar0);
        lv_mem_free(data_tmp);
        return LV_RES_INV;
    }

    lv_img_dsc_t * imgdsc = lv_canvas_get_img(barcode);
    uint8_t * buf_u8 = (uint8_t *)imgdsc->data + 8;    /*+8 skip the palette*/
    uint32_t row_byte_cnt = (imgdsc->header.w + 7) >> 3;
    int scale = imgdsc->header.w / bitsLen;
    int scaled = bitsLen * scale;
    int margin = (imgdsc->header.w - scaled) / 2;
    int x, y, i, j;

    /*Align and scale the bar to first row*/
    for (i = 0, x = margin; i < bitsLen; i ++) {
        bool a = barcodegen_getModule((uint8_t*)bar0, i);
        c.full = a ? 1 : 0;
        for(j = 0; j < scale; j ++) {
            lv_canvas_set_px_color(barcode, x++, 0, c);
        }
    }

    /*The Bar is scaled so simply to the repeated rows*/
    for(y = 1; y < imgdsc->header.h; y++) {
        memcpy(buf_u8 + row_byte_cnt * y, buf_u8, row_byte_cnt);
    }

    lv_obj_invalidate(barcode);

    lv_mem_free(bar0);
    lv_mem_free(data_tmp);
    return LV_RES_OK;
}

/**
 * Delete a BAR code object
 * @param qrcode pointer to a BAR code object
 */
void lv_barcode_delete(lv_obj_t * barcode)
{
    lv_img_dsc_t * img = lv_canvas_get_img(barcode);
    lv_img_cache_invalidate_src(img);
    lv_mem_free((void*)img->data);
    lv_obj_del(barcode);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /*LV_USE_BARCODE*/
