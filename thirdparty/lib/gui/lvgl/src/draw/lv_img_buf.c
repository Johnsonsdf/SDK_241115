/**
 * @file lv_img_buf.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <stddef.h>
#include <string.h>
#include "lv_img_buf.h"
#include "lv_draw_img.h"
#include "../misc/lv_math.h"
#include "../misc/lv_log.h"
#include "../misc/lv_mem.h"

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

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_color_t lv_img_buf_get_px_color(const lv_img_dsc_t * dsc, lv_coord_t x, lv_coord_t y, lv_color_t color)
{
    lv_color_t p_color = lv_color_black();
    uint8_t * buf_u8 = (uint8_t *)dsc->data;

    if(dsc->header.cf == LV_IMG_CF_TRUE_COLOR || dsc->header.cf == LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED ||
       dsc->header.cf == LV_IMG_CF_TRUE_COLOR_ALPHA || dsc->header.cf == LV_IMG_CF_RGB565A8) {
        uint8_t px_size = lv_img_cf_get_px_size(dsc->header.cf) >> 3;
        uint32_t px     = dsc->header.w * y * px_size + x * px_size;
        lv_memcpy_small(&p_color, &buf_u8[px], sizeof(lv_color_t));
#if LV_COLOR_SIZE == 32
        p_color.ch.alpha = 0xFF; /*Only the color should be get so use a default alpha value*/
#endif
    }
#if LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0
    else if(dsc->header.cf == LV_IMG_CF_RGB_565) {
        uint32_t px     = dsc->header.w * y * 2 + x * 2;
        uint8_t  blue   = (buf_u8[px] << 3) | (buf_u8[px] & 0x7);
        uint8_t  green  = (buf_u8[px + 1] << 5) | ((buf_u8[px] >> 3) & 0x1c) | ((buf_u8[px] >> 5) & 0x3);
        uint8_t  red    = (buf_u8[px + 1] & 0xf8) | ((buf_u8[px + 1] >> 3) & 0x7);
        p_color         = lv_color_make(red, green, blue);
    }
    else if(dsc->header.cf == LV_IMG_CF_ARGB_8565) {
        uint32_t px     = dsc->header.w * y * 3 + x * 3;
        uint8_t  blue   = (buf_u8[px] << 3) | (buf_u8[px] & 0x7);
        uint8_t  green  = (buf_u8[px + 1] << 5) | ((buf_u8[px] >> 3) & 0x1c) | ((buf_u8[px] >> 5) & 0x3);
        uint8_t  red    = (buf_u8[px + 1] & 0xf8) | ((buf_u8[px + 1] >> 3) & 0x7);
        p_color         = lv_color_make(red, green, blue);
    }
#endif
#if LV_COLOR_DEPTH != 32
    else if(dsc->header.cf == LV_IMG_CF_ARGB_8888 || dsc->header.cf == LV_IMG_CF_XRGB_8888) {
        uint32_t px     = dsc->header.w * y * 4 + x * 4;
        p_color         = lv_color_make(buf_u8[px + 2], buf_u8[px + 1], buf_u8[px]);
    }
#endif
    else if(dsc->header.cf == LV_IMG_CF_RGB_888) {
        uint32_t px     = dsc->header.w * y * 3 + x * 3;
        p_color         = lv_color_make(buf_u8[px + 2], buf_u8[px + 1], buf_u8[px]);
    }
    else if(dsc->header.cf == LV_IMG_CF_ARGB_6666) {
        uint32_t px     = dsc->header.w * y * 3 + x * 3;
        uint8_t  blue   = (buf_u8[px] << 2) | (buf_u8[px] & 0x3);
        uint8_t  green  = (buf_u8[px + 1] << 4) | ((buf_u8[px] >> 4) & 0x0c) | (buf_u8[px] >> 6);
        uint8_t  red    = (buf_u8[px + 2] << 6) | ((buf_u8[px + 1] >> 2) & 0x3c) | ((buf_u8[px + 1] >> 4) & 0x03);
        p_color         = lv_color_make(red, green, blue);
    }
    else if(dsc->header.cf == LV_IMG_CF_ARGB_1555) {
        uint32_t px     = dsc->header.w * y * 2 + x * 2;
        uint8_t  blue   = (buf_u8[px] << 3) | (buf_u8[px] & 0x7);
        uint8_t  green  = (buf_u8[px + 1] << 6) | ((buf_u8[px] >> 2) & 0x38) | (buf_u8[px] >> 5);
        uint8_t  red    = ((buf_u8[px + 1] << 1) & 0xf8) | ((buf_u8[px + 1] >> 2) & 0x7);
        p_color         = lv_color_make(red, green, blue);
    }
    else if(dsc->header.cf == LV_IMG_CF_INDEXED_1BIT) {
        buf_u8 += 4 * 2;
        uint8_t bit = x & 0x7;
        x           = x >> 3;

        /*Get the current pixel.
         *dsc->header.w + 7 means rounding up to 8 because the lines are byte aligned
         *so the possible real width are 8, 16, 24 ...*/
        uint32_t px  = ((dsc->header.w + 7) >> 3) * y + x;
        p_color.full = (buf_u8[px] & (1 << (7 - bit))) >> (7 - bit);
    }
    else if(dsc->header.cf == LV_IMG_CF_INDEXED_2BIT) {
        buf_u8 += 4 * 4;
        uint8_t bit = (x & 0x3) * 2;
        x           = x >> 2;

        /*Get the current pixel.
         *dsc->header.w + 3 means rounding up to 4 because the lines are byte aligned
         *so the possible real width are 4, 8, 12 ...*/
        uint32_t px  = ((dsc->header.w + 3) >> 2) * y + x;
        p_color.full = (buf_u8[px] & (3 << (6 - bit))) >> (6 - bit);
    }
    else if(dsc->header.cf == LV_IMG_CF_INDEXED_4BIT) {
        buf_u8 += 4 * 16;
        uint8_t bit = (x & 0x1) * 4;
        x           = x >> 1;

        /*Get the current pixel.
         *dsc->header.w + 1 means rounding up to 2 because the lines are byte aligned
         *so the possible real width are 2, 4, 6 ...*/
        uint32_t px  = ((dsc->header.w + 1) >> 1) * y + x;
        p_color.full = (buf_u8[px] & (0xF << (4 - bit))) >> (4 - bit);
    }
    else if(dsc->header.cf == LV_IMG_CF_INDEXED_8BIT) {
        buf_u8 += 4 * 256;
        uint32_t px  = dsc->header.w * y + x;
        p_color.full = buf_u8[px];
    }
    else if(dsc->header.cf == LV_IMG_CF_INDEXED_8BIT_ACTS) {
        uint32_t cnt = *(uint32_t *)dsc->data;
        buf_u8 += 4 * cnt + 4;
        uint32_t px  = dsc->header.w * y + x;
        p_color.full = buf_u8[px];
    }
    else if(dsc->header.cf == LV_IMG_CF_ALPHA_1BIT || dsc->header.cf == LV_IMG_CF_ALPHA_2BIT ||
            dsc->header.cf == LV_IMG_CF_ALPHA_4BIT || dsc->header.cf == LV_IMG_CF_ALPHA_8BIT ||
            dsc->header.cf == LV_IMG_CF_ALPHA_4BIT_LE || dsc->header.cf == LV_IMG_CF_ALPHA_1BIT_LE) {
        p_color = color;
    }
    return p_color;
}

lv_opa_t lv_img_buf_get_px_alpha(const lv_img_dsc_t * dsc, lv_coord_t x, lv_coord_t y)
{
    uint8_t * buf_u8 = (uint8_t *)dsc->data;

    if(dsc->header.cf == LV_IMG_CF_TRUE_COLOR_ALPHA) {
        uint32_t px = dsc->header.w * y * LV_IMG_PX_SIZE_ALPHA_BYTE + x * LV_IMG_PX_SIZE_ALPHA_BYTE;
        return buf_u8[px + LV_IMG_PX_SIZE_ALPHA_BYTE - 1];
    }
#if LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0
    else if(dsc->header.cf == LV_IMG_CF_ARGB_8565) {
        uint32_t px = dsc->header.w * y * 3 + x * 3;
        return buf_u8[px + 2];
    }
#endif
#if LV_COLOR_DEPTH != 32
    else if(dsc->header.cf == LV_IMG_CF_ARGB_8888) {
        uint32_t px = dsc->header.w * y * 4 + x * 4;
        return buf_u8[px + 4 - 1];
    }
#endif
    else if(dsc->header.cf == LV_IMG_CF_ARGB_6666) {
        uint32_t px = dsc->header.w * y * 3 + x * 3;
        return (buf_u8[px + 2] & 0xfc) | ((buf_u8[px + 2] & 0x0c) >> 2);
    }
    else if(dsc->header.cf == LV_IMG_CF_ARGB_1555) {
        uint32_t px = dsc->header.w * y * 2 + x * 2;
        return (buf_u8[px + 1] & 0x80) ? 255 : 0;
    }
    else if(dsc->header.cf == LV_IMG_CF_ALPHA_1BIT || dsc->header.cf == LV_IMG_CF_ALPHA_1BIT_LE) {
        uint8_t bit = x & 0x7;
        x           = x >> 3;

        if(dsc->header.cf == LV_IMG_CF_ALPHA_1BIT_LE)
            bit = 7 - bit;

        /*Get the current pixel.
         *dsc->header.w + 7 means rounding up to 8 because the lines are byte aligned
         *so the possible real width are 8 ,16, 24 ...*/
        uint32_t px    = ((dsc->header.w + 7) >> 3) * y + x;
        uint8_t px_opa = (buf_u8[px] & (1 << (7 - bit))) >> (7 - bit);
        return px_opa ? LV_OPA_TRANSP : LV_OPA_COVER;
    }
    else if(dsc->header.cf == LV_IMG_CF_ALPHA_2BIT) {
        const uint8_t opa_table[4] = {0, 85, 170, 255}; /*Opacity mapping with bpp = 2*/

        uint8_t bit = (x & 0x3) * 2;
        x           = x >> 2;

        /*Get the current pixel.
         *dsc->header.w + 4 means rounding up to 8 because the lines are byte aligned
         *so the possible real width are 4 ,8, 12 ...*/
        uint32_t px    = ((dsc->header.w + 3) >> 2) * y + x;
        uint8_t px_opa = (buf_u8[px] & (3 << (6 - bit))) >> (6 - bit);
        return opa_table[px_opa];
    }
    else if(dsc->header.cf == LV_IMG_CF_ALPHA_4BIT || dsc->header.cf == LV_IMG_CF_ALPHA_4BIT_LE) {
        const uint8_t opa_table[16] = {0,  17, 34,  51, /*Opacity mapping with bpp = 4*/
                                       68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255
                                      };

        uint8_t bit = (x & 0x1) * 4;
        x           = x >> 1;

        if(dsc->header.cf == LV_IMG_CF_ALPHA_4BIT_LE)
            bit = 4 - bit;

        /*Get the current pixel.
         *dsc->header.w + 1 means rounding up to 8 because the lines are byte aligned
         *so the possible real width are 2 ,4, 6 ...*/
        uint32_t px    = ((dsc->header.w + 1) >> 1) * y + x;
        uint8_t px_opa = (buf_u8[px] & (0xF << (4 - bit))) >> (4 - bit);
        return opa_table[px_opa];
    }
    else if(dsc->header.cf == LV_IMG_CF_ALPHA_8BIT) {
        uint32_t px = dsc->header.w * y + x;
        return buf_u8[px];
    }

    return LV_OPA_COVER;
}

void lv_img_buf_set_px_alpha(const lv_img_dsc_t * dsc, lv_coord_t x, lv_coord_t y, lv_opa_t opa)
{
    uint8_t * buf_u8 = (uint8_t *)dsc->data;

    if(dsc->header.cf == LV_IMG_CF_TRUE_COLOR_ALPHA) {
        uint8_t px_size          = lv_img_cf_get_px_size(dsc->header.cf) >> 3;
        uint32_t px              = dsc->header.w * y * px_size + x * px_size;
        buf_u8[px + px_size - 1] = opa;
    }
#if LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0
    else if(dsc->header.cf == LV_IMG_CF_ARGB_8565) {
        uint32_t px    = dsc->header.w * y * 3 + x * 3;
        buf_u8[px + 2] = opa;
    }
#endif
#if LV_COLOR_DEPTH != 32
    else if(dsc->header.cf == LV_IMG_CF_ARGB_8888) {
        uint32_t px    = dsc->header.w * y * 4 + x * 4;
        buf_u8[px + 3] = opa;
    }
#endif
    else if(dsc->header.cf == LV_IMG_CF_ARGB_6666) {
        uint32_t px = dsc->header.w * y * 3 + x * 3;
        buf_u8[px + 2] = (buf_u8[px + 2] & ~0xfc) | (opa & 0xfc);
    }
    else if(dsc->header.cf == LV_IMG_CF_ARGB_1555) {
        uint32_t px = dsc->header.w * y * 2 + x * 2;
        buf_u8[px + 1] = (buf_u8[px + 1] & ~0x80) | (opa & 0x80);
    }
    else if(dsc->header.cf == LV_IMG_CF_ALPHA_1BIT || dsc->header.cf == LV_IMG_CF_ALPHA_1BIT_LE) {
        opa         = opa >> 7; /*opa -> [0,1]*/
        uint8_t bit = x & 0x7;
        x           = x >> 3;

        if(dsc->header.cf == LV_IMG_CF_ALPHA_1BIT_LE)
            bit = 7 - bit;

        /*Get the current pixel.
         *dsc->header.w + 7 means rounding up to 8 because the lines are byte aligned
         *so the possible real width are 8 ,16, 24 ...*/
        uint32_t px = ((dsc->header.w + 7) >> 3) * y + x;
        buf_u8[px]  = buf_u8[px] & ~(1 << (7 - bit));
        buf_u8[px]  = buf_u8[px] | ((opa & 0x1) << (7 - bit));
    }
    else if(dsc->header.cf == LV_IMG_CF_ALPHA_2BIT) {
        opa         = opa >> 6; /*opa -> [0,3]*/
        uint8_t bit = (x & 0x3) * 2;
        x           = x >> 2;

        /*Get the current pixel.
         *dsc->header.w + 4 means rounding up to 8 because the lines are byte aligned
         *so the possible real width are 4 ,8, 12 ...*/
        uint32_t px = ((dsc->header.w + 3) >> 2) * y + x;
        buf_u8[px]  = buf_u8[px] & ~(3 << (6 - bit));
        buf_u8[px]  = buf_u8[px] | ((opa & 0x3) << (6 - bit));
    }
    else if(dsc->header.cf == LV_IMG_CF_ALPHA_4BIT || dsc->header.cf == LV_IMG_CF_ALPHA_4BIT_LE) {
        opa         = opa >> 4; /*opa -> [0,15]*/
        uint8_t bit = (x & 0x1) * 4;
        x           = x >> 1;

        if(dsc->header.cf == LV_IMG_CF_ALPHA_4BIT_LE)
            bit = 4 - bit;

        /*Get the current pixel.
         *dsc->header.w + 1 means rounding up to 8 because the lines are byte aligned
         *so the possible real width are 2 ,4, 6 ...*/
        uint32_t px = ((dsc->header.w + 1) >> 1) * y + x;
        buf_u8[px]  = buf_u8[px] & ~(0xF << (4 - bit));
        buf_u8[px]  = buf_u8[px] | ((opa & 0xF) << (4 - bit));
    }
    else if(dsc->header.cf == LV_IMG_CF_ALPHA_8BIT) {
        uint32_t px = dsc->header.w * y + x;
        buf_u8[px]  = opa;
    }
}

void lv_img_buf_set_px_color(const lv_img_dsc_t * dsc, lv_coord_t x, lv_coord_t y, lv_color_t c)
{
    uint8_t * buf_u8 = (uint8_t *)dsc->data;

    if(dsc->header.cf == LV_IMG_CF_TRUE_COLOR || dsc->header.cf == LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED) {
        uint8_t px_size = lv_img_cf_get_px_size(dsc->header.cf) >> 3;
        uint32_t px     = dsc->header.w * y * px_size + x * px_size;
        lv_memcpy_small(&buf_u8[px], &c, px_size);
    }
#if LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0
    else if(dsc->header.cf == LV_IMG_CF_RGB_565) {
        uint32_t px  = dsc->header.w * y * 2 + x * 2;
        uint16_t c16 = lv_color_to16(c);
        buf_u8[px] = c16 & 0xff;
        buf_u8[px + 1] = c16 >> 8;
    }
    else if(dsc->header.cf == LV_IMG_CF_ARGB_8565) {
        uint32_t px  = dsc->header.w * y * 3 + x * 3;
        uint16_t c16 = lv_color_to16(c);
        buf_u8[px] = c16 & 0xff;
        buf_u8[px + 1] = c16 >> 8;
    }
#endif
#if LV_COLOR_DEPTH != 32
    else if(dsc->header.cf == LV_IMG_CF_ARGB_8888 || dsc->header.cf == LV_IMG_CF_XRGB_8888) {
        uint32_t px      = dsc->header.w * y * 4 + x * 4;
        lv_color32_t c32 = { .full = lv_color_to32(c), };
        buf_u8[px] = c32.ch.blue;
        buf_u8[px + 1] = c32.ch.green;
        buf_u8[px + 2] = c32.ch.red;
    }
#endif
    else if(dsc->header.cf == LV_IMG_CF_RGB_888) {
        uint32_t px      = dsc->header.w * y * 3 + x * 3;
        lv_color32_t c32 = { .full = lv_color_to32(c), };
        buf_u8[px] = c32.ch.blue;
        buf_u8[px + 1] = c32.ch.green;
        buf_u8[px + 2] = c32.ch.red;
    }
    else if(dsc->header.cf == LV_IMG_CF_ARGB_6666) {
        uint32_t px      = dsc->header.w * y * 3 + x * 3;
        lv_color32_t c32 = { .full = lv_color_to32(c), };
        buf_u8[px] = (c32.ch.blue >> 2) | ((c32.ch.green >> 2) << 6);
        buf_u8[px + 1] = (c32.ch.green >> 4) | ((c32.ch.red >> 2) << 4);
        buf_u8[px + 2] = (buf_u8[px + 2] & ~0x3) | (c32.ch.red >> 6);
    }
    else if(dsc->header.cf == LV_IMG_CF_ARGB_1555) {
        uint32_t px  = dsc->header.w * y * 2 + x * 2;
        uint16_t c16 = lv_color_to16(c);
        c16 = ((c16 & 0xffc0) >> 1) | (c16 & 0x1f);
        buf_u8[px] = c16 & 0xff;
        buf_u8[px + 1] = (c16 >> 8) | (buf_u8[px + 1] & 0x80);
    }
    else if(dsc->header.cf == LV_IMG_CF_TRUE_COLOR_ALPHA) {
        uint8_t px_size = lv_img_cf_get_px_size(dsc->header.cf) >> 3;
        uint32_t px     = dsc->header.w * y * px_size + x * px_size;
        lv_memcpy_small(&buf_u8[px], &c, px_size - 1); /*-1 to not overwrite the alpha value*/
    }
    else if(dsc->header.cf == LV_IMG_CF_INDEXED_1BIT) {
        buf_u8 += sizeof(lv_color32_t) * 2; /*Skip the palette*/

        uint8_t bit = x & 0x7;
        x           = x >> 3;

        /*Get the current pixel.
         *dsc->header.w + 7 means rounding up to 8 because the lines are byte aligned
         *so the possible real width are 8 ,16, 24 ...*/
        uint32_t px = ((dsc->header.w + 7) >> 3) * y + x;
        buf_u8[px]  = buf_u8[px] & ~(1 << (7 - bit));
        buf_u8[px]  = buf_u8[px] | ((c.full & 0x1) << (7 - bit));
    }
    else if(dsc->header.cf == LV_IMG_CF_INDEXED_2BIT) {
        buf_u8 += sizeof(lv_color32_t) * 4; /*Skip the palette*/
        uint8_t bit = (x & 0x3) * 2;
        x           = x >> 2;

        /*Get the current pixel.
         *dsc->header.w + 3 means rounding up to 4 because the lines are byte aligned
         *so the possible real width are 4, 8 ,12 ...*/
        uint32_t px = ((dsc->header.w + 3) >> 2) * y + x;

        buf_u8[px] = buf_u8[px] & ~(3 << (6 - bit));
        buf_u8[px] = buf_u8[px] | ((c.full & 0x3) << (6 - bit));
    }
    else if(dsc->header.cf == LV_IMG_CF_INDEXED_4BIT) {
        buf_u8 += sizeof(lv_color32_t) * 16; /*Skip the palette*/
        uint8_t bit = (x & 0x1) * 4;
        x           = x >> 1;

        /*Get the current pixel.
         *dsc->header.w + 1 means rounding up to 2 because the lines are byte aligned
         *so the possible real width are 2 ,4, 6 ...*/
        uint32_t px = ((dsc->header.w + 1) >> 1) * y + x;
        buf_u8[px]  = buf_u8[px] & ~(0xF << (4 - bit));
        buf_u8[px]  = buf_u8[px] | ((c.full & 0xF) << (4 - bit));
    }
    else if(dsc->header.cf == LV_IMG_CF_INDEXED_8BIT) {
        buf_u8 += sizeof(lv_color32_t) * 256; /*Skip the palette*/
        uint32_t px = dsc->header.w * y + x;
        buf_u8[px]  = c.full;
    }
    else if(dsc->header.cf == LV_IMG_CF_INDEXED_8BIT_ACTS) {
        uint32_t cnt = *(uint32_t *)dsc->data;
        buf_u8 += sizeof(lv_color32_t) * cnt + 4; /*Skip the palette*/
        uint32_t px = dsc->header.w * y + x;
        buf_u8[px]  = c.full;
    }
}

void lv_img_buf_set_palette(const lv_img_dsc_t * dsc, uint8_t id, lv_color_t c)
{
    if((dsc->header.cf == LV_IMG_CF_ALPHA_1BIT && id > 1) || (dsc->header.cf == LV_IMG_CF_ALPHA_2BIT && id > 3) ||
       (dsc->header.cf == LV_IMG_CF_ALPHA_4BIT && id > 15) || (dsc->header.cf == LV_IMG_CF_ALPHA_8BIT) ||
       (dsc->header.cf == LV_IMG_CF_ALPHA_1BIT_LE && id > 1) || (dsc->header.cf == LV_IMG_CF_ALPHA_4BIT_LE && id > 15)) {
        LV_LOG_WARN("lv_img_buf_set_px_alpha: invalid 'id'");
        return;
    }

    lv_color32_t c32;
    c32.full      = lv_color_to32(c);
    uint8_t * buf = (uint8_t *)dsc->data;
    lv_memcpy_small(&buf[id * sizeof(c32)], &c32, sizeof(c32));
}

lv_img_dsc_t * lv_img_buf_alloc(lv_coord_t w, lv_coord_t h, lv_img_cf_t cf)
{
    /*Allocate image descriptor*/
    lv_img_dsc_t * dsc = lv_mem_alloc(sizeof(lv_img_dsc_t));
    if(dsc == NULL)
        return NULL;

    lv_memset_00(dsc, sizeof(lv_img_dsc_t));

    /*Get image data size*/
    dsc->data_size = lv_img_buf_get_img_size(w, h, cf);
    if(dsc->data_size == 0) {
        lv_mem_free(dsc);
        return NULL;
    }

    /*Allocate raw buffer*/
    dsc->data = lv_mem_alloc(dsc->data_size);
    if(dsc->data == NULL) {
        lv_mem_free(dsc);
        return NULL;
    }
    lv_memset_00((uint8_t *)dsc->data, dsc->data_size);

    /*Fill in header*/
    dsc->header.always_zero = 0;
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = cf;
    return dsc;
}

void lv_img_buf_free(lv_img_dsc_t * dsc)
{
    if(dsc != NULL) {
        if(dsc->data != NULL)
            lv_mem_free((void *)dsc->data);

        lv_mem_free(dsc);
    }
}

uint32_t lv_img_buf_get_img_size(lv_coord_t w, lv_coord_t h, lv_img_cf_t cf)
{
    switch(cf) {
        case LV_IMG_CF_TRUE_COLOR:
            return LV_IMG_BUF_SIZE_TRUE_COLOR(w, h);
        case LV_IMG_CF_TRUE_COLOR_ALPHA:
        case LV_IMG_CF_RGB565A8:
            return LV_IMG_BUF_SIZE_TRUE_COLOR_ALPHA(w, h);
        case LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED:
            return LV_IMG_BUF_SIZE_TRUE_COLOR_CHROMA_KEYED(w, h);
#if LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0
        case LV_IMG_CF_RGB_565:
            return w * h * 2;
		case LV_IMG_CF_ARGB_8565:
            return w * h * 3;
#endif
#if LV_COLOR_DEPTH != 32
        case LV_IMG_CF_ARGB_8888:
        case LV_IMG_CF_XRGB_8888:
            return w * h * 4;
#endif
        case LV_IMG_CF_RGB_888:
        case LV_IMG_CF_ARGB_6666:
            return w * h * 3;
        case LV_IMG_CF_ARGB_1555:
            return w * h * 2;
        case LV_IMG_CF_ALPHA_1BIT:
        case LV_IMG_CF_ALPHA_1BIT_LE:
            return LV_IMG_BUF_SIZE_ALPHA_1BIT(w, h);
        case LV_IMG_CF_ALPHA_2BIT:
            return LV_IMG_BUF_SIZE_ALPHA_2BIT(w, h);
        case LV_IMG_CF_ALPHA_4BIT:
        case LV_IMG_CF_ALPHA_4BIT_LE:
            return LV_IMG_BUF_SIZE_ALPHA_4BIT(w, h);
        case LV_IMG_CF_ALPHA_8BIT:
            return LV_IMG_BUF_SIZE_ALPHA_8BIT(w, h);
        case LV_IMG_CF_INDEXED_1BIT:
            return LV_IMG_BUF_SIZE_INDEXED_1BIT(w, h);
        case LV_IMG_CF_INDEXED_2BIT:
            return LV_IMG_BUF_SIZE_INDEXED_2BIT(w, h);
        case LV_IMG_CF_INDEXED_4BIT:
            return LV_IMG_BUF_SIZE_INDEXED_4BIT(w, h);
        case LV_IMG_CF_INDEXED_8BIT:
            return LV_IMG_BUF_SIZE_INDEXED_8BIT(w, h);
        case LV_IMG_CF_INDEXED_8BIT_ACTS:
            return LV_IMG_BUF_SIZE_INDEXED_8BIT(w, h) + 4;
        case LV_IMG_CF_ETC2_EAC:
            return ((w + 0x3) & ~0x3) * ((h + 0x3) & ~0x3);
        default:
            return 0;
    }
}

void _lv_img_buf_get_transformed_area(lv_area_t * res, lv_coord_t w, lv_coord_t h, int16_t angle, uint16_t zoom,
                                      const lv_point_t * pivot)
{
#if LV_DRAW_COMPLEX
    if(angle == 0 && zoom == LV_IMG_ZOOM_NONE) {
        res->x1 = 0;
        res->y1 = 0;
        res->x2 = w - 1;
        res->y2 = h - 1;
        return;
    }

    lv_point_t p[4] = {
        {0, 0},
        {w, 0},
        {0, h},
        {w, h},
    };
    lv_point_transform(&p[0], angle, zoom, pivot);
    lv_point_transform(&p[1], angle, zoom, pivot);
    lv_point_transform(&p[2], angle, zoom, pivot);
    lv_point_transform(&p[3], angle, zoom, pivot);
    res->x1 = LV_MIN4(p[0].x, p[1].x, p[2].x, p[3].x) - 2;
    res->x2 = LV_MAX4(p[0].x, p[1].x, p[2].x, p[3].x) + 2;
    res->y1 = LV_MIN4(p[0].y, p[1].y, p[2].y, p[3].y) - 2;
    res->y2 = LV_MAX4(p[0].y, p[1].y, p[2].y, p[3].y) + 2;

#else
    LV_UNUSED(angle);
    LV_UNUSED(zoom);
    LV_UNUSED(pivot);
    res->x1 = 0;
    res->y1 = 0;
    res->x2 = w - 1;
    res->y2 = h - 1;
#endif
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
