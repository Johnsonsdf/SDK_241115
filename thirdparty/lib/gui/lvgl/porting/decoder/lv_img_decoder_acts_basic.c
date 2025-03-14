/**
 * @file lv_decoder_acts_basic.c
 *
 */

/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_img_decoder_acts.h"

/*********************
 *      DEFINES
 *********************/
#define CF_ACTS_BASIC_FIRST   LV_IMG_CF_ARGB_6666

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_fs_file_t f;

    struct {
        uint32_t size; /* number of colors in palette */
        lv_color32_t * colors;
    } palette;
} _decoder_basic_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_res_t _img_decoder_acts_basic_info(lv_img_decoder_t * decoder, const void * src,
                                             lv_img_header_t * header);
static lv_res_t _img_decoder_acts_basic_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);
static lv_res_t _img_decoder_acts_basic_read_line(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc,
                                                  lv_coord_t x, lv_coord_t y, lv_coord_t len, uint8_t * buf);
static void _img_decoder_acts_basic_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);

static lv_res_t _img_decoder_basic_line_alpha(lv_img_decoder_dsc_t * dsc, lv_coord_t x, lv_coord_t y,
                                              lv_coord_t len, uint8_t * buf);
static lv_res_t _img_decoder_basic_line_index(lv_img_decoder_dsc_t * dsc, lv_coord_t x, lv_coord_t y,
                                              lv_coord_t len, uint8_t * buf);
static lv_res_t _img_decoder_basic_line_rgb_color(lv_img_decoder_dsc_t * dsc, lv_coord_t x, lv_coord_t y,
                                                  lv_coord_t len, uint8_t * buf);

static void _decode_rgb_565(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len);
static void _decode_argb_8565(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len);
static void _decode_argb_8888(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len);
static void _decode_xrgb_8888(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len);
static void _decode_rgb_888(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len);
static void _decode_argb_6666(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len);
static void _decode_argb_1555(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_img_decoder_acts_basic_init(void)
{
    lv_img_decoder_t * decoder = lv_img_decoder_create();
    LV_ASSERT_MALLOC(decoder);
    if(decoder == NULL) {
        LV_LOG_WARN("decoder_acts_basic_init: out of memory");
        return;
    }

    lv_img_decoder_set_info_cb(decoder, _img_decoder_acts_basic_info);
    lv_img_decoder_set_open_cb(decoder, _img_decoder_acts_basic_open);
    lv_img_decoder_set_read_line_cb(decoder, _img_decoder_acts_basic_read_line);
    lv_img_decoder_set_close_cb(decoder, _img_decoder_acts_basic_close);
}

lv_res_t lv_img_decode_acts_rgb_color(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len, lv_img_cf_t cf)
{
    switch (cf) {
        case LV_IMG_CF_RGB_565:
            _decode_rgb_565(dst_buf, src_buf, len);
            break;
        case LV_IMG_CF_ARGB_8565:
            _decode_argb_8565(dst_buf, src_buf, len);
            break;
        case LV_IMG_CF_ARGB_8888:
            _decode_argb_8888(dst_buf, src_buf, len);
            break;
        case LV_IMG_CF_XRGB_8888:
            _decode_xrgb_8888(dst_buf, src_buf, len);
            break;
        case LV_IMG_CF_RGB_888:
            _decode_rgb_888(dst_buf, src_buf, len);
            break;
        case LV_IMG_CF_ARGB_6666:
            _decode_argb_6666(dst_buf, src_buf, len);
            break;
        case LV_IMG_CF_ARGB_1555:
            _decode_argb_1555(dst_buf, src_buf, len);
            break;
        default:
            return LV_RES_INV;
    }

    return LV_RES_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_res_t _img_decoder_acts_basic_info(lv_img_decoder_t * decoder, const void * src,
                                             lv_img_header_t * header)
{
    LV_UNUSED(decoder); /*Unused*/

    lv_img_src_t src_type = lv_img_src_get_type(src);

    if (src_type == LV_IMG_SRC_VARIABLE) {
        lv_img_cf_t cf = ((lv_img_dsc_t *)src)->header.cf;
        if (cf < CF_ACTS_BASIC_FIRST) return LV_RES_INV;

        header->w  = ((lv_img_dsc_t *)src)->header.w;
        header->h  = ((lv_img_dsc_t *)src)->header.h;
        header->cf = ((lv_img_dsc_t *)src)->header.cf;
    }
    else if (src_type == LV_IMG_SRC_FILE) {
        /*Support only "*.bin" files*/
        if (strcmp(lv_fs_get_ext(src), "bin")) return LV_RES_INV;

        lv_fs_file_t f;
        lv_fs_res_t res = lv_fs_open(&f, src, LV_FS_MODE_RD);
        if (res == LV_FS_RES_OK) {
            uint32_t rn;
            res = lv_fs_read(&f, header, sizeof(lv_img_header_t), &rn);
            lv_fs_close(&f);
            if (res != LV_FS_RES_OK || rn != sizeof(lv_img_header_t)) {
                LV_LOG_WARN("Image get info get read file header");
                return LV_RES_INV;
            }
        }

        if (header->cf < CF_ACTS_BASIC_FIRST) return LV_RES_INV;
    }
    else {
        return LV_RES_INV;
    }

    return LV_RES_OK;
}

static lv_res_t _img_decoder_acts_basic_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    /*Open the file if it's a file*/
    if (dsc->src_type == LV_IMG_SRC_FILE) {
        /*Support only "*.bin" files*/
        if (strcmp(lv_fs_get_ext(dsc->src), "bin")) return LV_RES_INV;

        lv_fs_file_t f;
        lv_fs_res_t res = lv_fs_open(&f, dsc->src, LV_FS_MODE_RD);
        if (res != LV_FS_RES_OK) {
            LV_LOG_WARN("Built-in image decoder can't open the file");
            return LV_RES_INV;
        }

        /*If the file was open successfully save the file descriptor*/
        if (dsc->user_data == NULL) {
            dsc->user_data = lv_mem_alloc(sizeof(_decoder_basic_data_t));
            LV_ASSERT_MALLOC(dsc->user_data);
            if (dsc->user_data == NULL) {
                LV_LOG_ERROR("_img_decoder_acts_basic_open: out of memory");
                lv_fs_close(&f);
                return LV_RES_INV;
            }

            lv_memset_00(dsc->user_data, sizeof(_decoder_basic_data_t));
        }

        _decoder_basic_data_t * user_data = dsc->user_data;
        lv_memcpy_small(&user_data->f, &f, sizeof(f));
    }
    else if (dsc->src_type == LV_IMG_SRC_VARIABLE) {
        /*The variables should have valid data*/
        if (((lv_img_dsc_t *)dsc->src)->data == NULL) {
            return LV_RES_INV;
        }
    }

    if (dsc->header.cf == LV_IMG_CF_INDEXED_8BIT_ACTS) {
        if (dsc->src_type == LV_IMG_SRC_FILE) {
            _decoder_basic_data_t * user_data = dsc->user_data;

            lv_fs_seek(&user_data->f, 0, LV_FS_SEEK_SET);
            lv_fs_read(&user_data->f, &user_data->palette.size, 4, NULL);

            if (user_data->palette.size == 0 || user_data->palette.size > 256) {
                LV_LOG_ERROR("_img_decoder_acts_basic_open: invalid palette %u", user_data->palette.size);
                _img_decoder_acts_basic_close(decoder, dsc);
                return LV_RES_INV;
            }

            user_data->palette.colors = lv_mem_alloc(user_data->palette.size * sizeof(lv_color32_t));
            LV_ASSERT_MALLOC(user_data->palette.colors);
            if (user_data->palette.colors == NULL) {
                LV_LOG_ERROR("_img_decoder_acts_basic_open: out of memory");
                _img_decoder_acts_basic_close(decoder, dsc);
                return LV_RES_INV;
            }

            lv_fs_read(&user_data->f, user_data->palette.colors, user_data->palette.size * sizeof(lv_color32_t), NULL);
        } else {
             uint32_t palette_size = *(uint32_t *)(((lv_img_dsc_t *)dsc->src)->data);
             if (palette_size == 0 || palette_size > 256) {
                LV_LOG_ERROR("_img_decoder_acts_basic_open: invalid palette %u", palette_size);
                return LV_RES_INV;
             }
        }
    }

    return LV_RES_OK; /*Nothing to process*/
}

static lv_res_t _img_decoder_acts_basic_read_line(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc,
                                                  lv_coord_t x, lv_coord_t y, lv_coord_t len, uint8_t * buf)
{
    if (dsc->header.cf == LV_IMG_CF_ALPHA_1BIT_LE || dsc->header.cf == LV_IMG_CF_ALPHA_4BIT_LE) {
        return _img_decoder_basic_line_alpha(dsc, x, y, len, buf);
    } else if (dsc->header.cf == LV_IMG_CF_INDEXED_8BIT_ACTS) {
        return _img_decoder_basic_line_index(dsc, x, y, len, buf);
    } else {
        return _img_decoder_basic_line_rgb_color(dsc, x, y, len, buf);
    }
}

static void _img_decoder_acts_basic_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder); /*Unused*/

    _decoder_basic_data_t * user_data = dsc->user_data;
    if (user_data) {
        if (dsc->src_type == LV_IMG_SRC_FILE) {
            lv_fs_close(&user_data->f);
        }

        if (user_data->palette.colors) {
            lv_mem_free(user_data->palette.colors);
        }

        lv_mem_free(user_data);
        dsc->user_data = NULL;
    }
}

static lv_res_t _img_decoder_basic_line_alpha(lv_img_decoder_dsc_t * dsc, lv_coord_t x, lv_coord_t y,
                                              lv_coord_t len, uint8_t * buf)
{
    const lv_opa_t alpha1_opa_table[2]  = {0, 255};          /*Opacity mapping with bpp = 1 (Just for compatibility)*/
    const lv_opa_t alpha4_opa_table[16] = {0,  17, 34,  51,  /*Opacity mapping with bpp = 4*/
                                           68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255
                                          };

    /*Simply fill the buffer with the color. Later only the alpha value will be modified.*/
    lv_color_t bg_color = dsc->color;
    lv_coord_t i;
    for(i = 0; i < len; i++) {
#if LV_COLOR_DEPTH == 8 || LV_COLOR_DEPTH == 1
        buf[i * LV_IMG_PX_SIZE_ALPHA_BYTE] = bg_color.full;
#elif LV_COLOR_DEPTH == 16
        /*Because of Alpha byte 16 bit color can start on odd address which can cause crash*/
        buf[i * LV_IMG_PX_SIZE_ALPHA_BYTE] = bg_color.full & 0xFF;
        buf[i * LV_IMG_PX_SIZE_ALPHA_BYTE + 1] = (bg_color.full >> 8) & 0xFF;
#elif LV_COLOR_DEPTH == 32
        *((uint32_t *)&buf[i * LV_IMG_PX_SIZE_ALPHA_BYTE]) = bg_color.full;
#else
#error "Invalid LV_COLOR_DEPTH. Check it in lv_conf.h"
#endif
    }

    const lv_opa_t * opa_table = NULL;
    uint8_t px_size            = lv_img_cf_get_px_size(dsc->header.cf);
    uint16_t mask              = (1 << px_size) - 1; /*E.g. px_size = 2; mask = 0x03*/

    lv_coord_t w = 0;
    uint32_t ofs = 0;
    int8_t pos   = 0;
    switch(dsc->header.cf) {
        case LV_IMG_CF_ALPHA_4BIT_LE:
            w = (dsc->header.w + 1) >> 1; /*E.g. w = 13 -> w = 6 + 1 (bytes)*/
            ofs += w * y + (x >> 1); /*First pixel*/
            pos  = ((x & 0x1) * 4);
            opa_table = alpha4_opa_table;
            break;
        case LV_IMG_CF_ALPHA_1BIT_LE:
        default:
            w = (dsc->header.w + 7) >> 3; /*E.g. w = 20 -> w = 2 + 1*/
            if(dsc->header.w & 0x7) w++;
            ofs += w * y + (x >> 3); /*First pixel*/
            pos  = (x & 0x7);
            opa_table = alpha1_opa_table;
            break;
    }

    const uint8_t * data_tmp = NULL;
    uint8_t * fs_buf = NULL;

    if (dsc->src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t * img_dsc = dsc->src;
        data_tmp = img_dsc->data + ofs;
    }
    else {
        _decoder_basic_data_t * user_data = dsc->user_data;
        lv_fs_file_t * filp = &user_data->f;

        fs_buf = lv_mem_buf_get(w);
        if (fs_buf == NULL) return LV_RES_INV;

        lv_fs_seek(filp, ofs + sizeof(lv_img_dsc_t), LV_FS_SEEK_SET);
        lv_fs_read(filp, fs_buf, w, NULL);
        data_tmp = fs_buf;
    }

    for (i = 0; i < len; i++) {
        uint8_t val_act = (*data_tmp >> pos) & mask;

        buf[i * LV_IMG_PX_SIZE_ALPHA_BYTE + LV_IMG_PX_SIZE_ALPHA_BYTE - 1] =
            (dsc->header.cf == LV_IMG_CF_ALPHA_8BIT) ? val_act : opa_table[val_act];

        pos += px_size;
        if(pos >= 8) {
            pos = pos - 8;
            data_tmp++;
        }
    }

    if (fs_buf != NULL) {
        lv_mem_buf_release(fs_buf);
    }

    return LV_RES_OK;
}

static lv_res_t _img_decoder_basic_line_index(lv_img_decoder_dsc_t * dsc, lv_coord_t x, lv_coord_t y,
                                              lv_coord_t len, uint8_t * buf)
{
    _decoder_basic_data_t * user_data = dsc->user_data;
    lv_coord_t w = dsc->header.w;
    lv_color32_t *palette;
    uint16_t palette_size;
    uint32_t ofs = 0;

    if (dsc->src_type == LV_IMG_SRC_FILE) {
        palette = user_data->palette.colors;
        palette_size = user_data->palette.size;
    }
    else {
        const lv_img_dsc_t * img_dsc = dsc->src;
        palette_size = *(uint32_t *)img_dsc->data;
        palette = (lv_color32_t *)(img_dsc->data + 4);
    }

    ofs = 4 + palette_size * sizeof(lv_color32_t) + w * y + x;

    const uint8_t * data_tmp = NULL;
    uint8_t * fs_buf = NULL;

    if (dsc->src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t * img_dsc = dsc->src;
        data_tmp                     = img_dsc->data + ofs;
    }
    else {
        fs_buf = lv_mem_buf_get(len);
        if (fs_buf == NULL) return LV_RES_INV;

        lv_fs_seek(&user_data->f, ofs + sizeof(lv_img_dsc_t), LV_FS_SEEK_SET);
        lv_fs_read(&user_data->f, fs_buf, w, NULL);
        data_tmp = fs_buf;
    }

    lv_coord_t i;
    for(i = 0; i < len; i++) {
        uint8_t val_act = *data_tmp++;
        lv_color_t color = lv_color_hex(palette[val_act].full);

#if LV_COLOR_DEPTH == 8 || LV_COLOR_DEPTH == 1
        buf[i * LV_IMG_PX_SIZE_ALPHA_BYTE] = color.full;
#elif LV_COLOR_DEPTH == 16
        /*Because of Alpha byte 16 bit color can start on odd address which can cause crash*/
        buf[i * LV_IMG_PX_SIZE_ALPHA_BYTE] = color.full & 0xFF;
        buf[i * LV_IMG_PX_SIZE_ALPHA_BYTE + 1] = (color.full >> 8) & 0xFF;
#elif LV_COLOR_DEPTH == 32
        *((uint32_t *)&buf[i * LV_IMG_PX_SIZE_ALPHA_BYTE]) = color.full;
#else
#error "Invalid LV_COLOR_DEPTH. Check it in lv_conf.h"
#endif

        buf[i * LV_IMG_PX_SIZE_ALPHA_BYTE + LV_IMG_PX_SIZE_ALPHA_BYTE - 1] = palette[val_act].ch.alpha;
    }

    if (fs_buf != NULL) {
        lv_mem_buf_release(fs_buf);
    }

    return LV_RES_OK;
}

static lv_res_t _img_decoder_basic_line_rgb_color(lv_img_decoder_dsc_t * dsc, lv_coord_t x, lv_coord_t y,
                                                  lv_coord_t len, uint8_t * buf)
{
    uint8_t px_bytes = lv_img_cf_get_px_size(dsc->header.cf) >> 3;
    uint32_t ofs = (y * dsc->header.w + x) * px_bytes;

    const uint8_t * data_tmp = NULL;
    uint8_t * fs_buf = NULL;

    if (dsc->src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t * img_dsc = dsc->src;
        data_tmp = img_dsc->data + ofs;
    }
    else {
        _decoder_basic_data_t * user_data = dsc->user_data;
        lv_fs_file_t * filp = &user_data->f;
        uint32_t btr = len * px_bytes;
        uint32_t br  = 0;

        fs_buf = lv_mem_buf_get(btr);
        if (fs_buf == NULL) return LV_RES_INV;

        lv_fs_res_t fs_res;
        fs_res = lv_fs_seek(filp, ofs + sizeof(lv_img_dsc_t), LV_FS_SEEK_SET);
        if (fs_res != LV_FS_RES_OK) {
            LV_LOG_WARN("Acts-basic image decoder seek failed");
            lv_mem_buf_release(fs_buf);
            return LV_RES_INV;
        }

        fs_res = lv_fs_read(filp, fs_buf, btr, &br);
        if (fs_res != LV_FS_RES_OK || btr != br) {
            LV_LOG_WARN("Acts-basic image decoder read failed");
            lv_mem_buf_release(fs_buf);
            return LV_RES_INV;
        }

        data_tmp = fs_buf;
    }

    lv_res_t res = lv_img_decode_acts_rgb_color(buf, data_tmp, len, dsc->header.cf);

    if (fs_buf != NULL) {
        lv_mem_buf_release(fs_buf);
    }

    return res;
}

static void _decode_rgb_565(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len)
{
#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0
    memcpy(dst_buf, src_buf, len * 2);
#else

    for (; len > 0; len--) {
#if LV_COLOR_DEPTH == 1
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = (src_buf[0] & 0x10) >> 4;
        color->ch.green = (src_buf[1] & 0x04) >> 2;
        color->ch.red = src_buf[1] >> 7;
#elif LV_COLOR_DEPTH == 8
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = (src_buf[0] & 0x18) >> 3;
        color->ch.green = (src_buf[1] & 0x07);
        color->ch.red = (src_buf[1] & 0xe0) >> 5;
#elif LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 1
        dst_buf[0] = src_buf[1];
        dst_buf[1] = src_buf[0];
#elif LV_COLOR_DEPTH == 32
        dst_buf[0] = (src_buf[0] << 3) | (src_buf[0] & 0x7);
        dst_buf[1] = (src_buf[1] << 5) | ((src_buf[0] >> 3) & 0x1c) | ((src_buf[0] >> 5) & 0x3);
        dst_buf[2] = (src_buf[1] & 0xf8) | ((src_buf[1] >> 3) & 0x7);
#else
#error "Invalid LV_COLOR_DEPTH. Check it in lv_conf.h"
#endif

        dst_buf += LV_COLOR_SIZE / 8;
        src_buf += 2;
    }
#endif /* LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0 */
}

static void _decode_argb_8565(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len)
{
#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0
    memcpy(dst_buf, src_buf, len * 3);
#else

    for (; len > 0; len--) {
#if LV_COLOR_DEPTH == 1
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = (src_buf[0] & 0x10) >> 4;
        color->ch.green = (src_buf[1] & 0x04) >> 2;
        color->ch.red = src_buf[1] >> 7;
#elif LV_COLOR_DEPTH == 8
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = (src_buf[0] & 0x18) >> 3;
        color->ch.green = (src_buf[1] & 0x07);
        color->ch.red = (src_buf[1] & 0xe0) >> 5;
#elif LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 1
        dst_buf[0] = src_buf[1];
        dst_buf[1] = src_buf[0];
#elif LV_COLOR_DEPTH == 32
        dst_buf[0] = (src_buf[0] << 3) | (src_buf[0] & 0x7);
        dst_buf[1] = (src_buf[1] << 5) | ((src_buf[0] >> 3) & 0x1c) | ((src_buf[0] >> 5) & 0x3);
        dst_buf[2] = (src_buf[1] & 0xf8) | ((src_buf[1] >> 3) & 0x7);
#else
#error "Invalid LV_COLOR_DEPTH. Check it in lv_conf.h"
#endif

        dst_buf[LV_IMG_PX_SIZE_ALPHA_BYTE - 1] = src_buf[2];
        dst_buf += LV_IMG_PX_SIZE_ALPHA_BYTE;
        src_buf += 3;
    }
#endif /* LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0 */
}

static void _decode_argb_8888(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len)
{
#if LV_COLOR_DEPTH == 32
    memcpy(dst_buf, src_buf, len * 4);
#else

    for (; len > 0; len--) {
#if LV_COLOR_DEPTH == 1
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = src_buf[0] >> 7;
        color->ch.green = src_buf[1] >> 7;
        color->ch.red = src_buf[2]>> 7;
#elif LV_COLOR_DEPTH == 8
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = src_buf[0] >> 6;
        color->ch.green = src_buf[1] >> 5;
        color->ch.red = src_buf[2]>> 5;
#elif LV_COLOR_DEPTH == 16
#if LV_COLOR_16_SWAP == 0
        /*Because of Alpha byte 16 bit color can start on odd address which can cause crash*/
        dst_buf[0] = ((src_buf[1] & 0x1c) << 3) | (src_buf[0]>> 3);
        dst_buf[1] = (src_buf[2] & 0xF8) | (src_buf[1]>> 5);
#else
        dst_buf[0] = (src_buf[2] & 0xF8) | (src_buf[1] >> 5);
        dst_buf[1] = ((src_buf[1] & 0x1c) << 3) | (src_buf[0]>> 3);
#endif
#else
#error "Invalid LV_COLOR_DEPTH. Check it in lv_conf.h"
#endif

        dst_buf[LV_IMG_PX_SIZE_ALPHA_BYTE - 1] = src_buf[3];
        dst_buf += LV_IMG_PX_SIZE_ALPHA_BYTE;
        src_buf += 4;
    }
#endif /* LV_COLOR_DEPTH == 32 */
}

static void _decode_xrgb_8888(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len)
{
#if LV_COLOR_DEPTH == 32
    memcpy(dst_buf, src_buf, len * 4);
#else

    for (; len > 0; len--) {
#if LV_COLOR_DEPTH == 1
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = src_buf[0] >> 7;
        color->ch.green = src_buf[1] >> 7;
        color->ch.red = src_buf[2]>> 7;
#elif LV_COLOR_DEPTH == 8
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = src_buf[0] >> 6;
        color->ch.green = src_buf[1] >> 5;
        color->ch.red = src_buf[2]>> 5;
#elif LV_COLOR_DEPTH == 16
#if LV_COLOR_16_SWAP == 0
        /*Because of Alpha byte 16 bit color can start on odd address which can cause crash*/
        dst_buf[0] = ((src_buf[1] & 0x1c) << 3) | (src_buf[0]>> 3);
        dst_buf[1] = (src_buf[2] & 0xF8) | (src_buf[1]>> 5);
#else
        dst_buf[0] = (src_buf[2] & 0xF8) | (src_buf[1] >> 5);
        dst_buf[1] = ((src_buf[1] & 0x1c) << 3) | (src_buf[0]>> 3);
#endif
#else
#error "Invalid LV_COLOR_DEPTH. Check it in lv_conf.h"
#endif

        dst_buf += LV_COLOR_SIZE / 8;
        src_buf += 4;
    }
#endif /* LV_COLOR_DEPTH == 32 */
}

static void _decode_rgb_888(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len)
{
    for (; len > 0; len--) {
#if LV_COLOR_DEPTH == 1
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = src_buf[0] >> 7;
        color->ch.green = src_buf[1] >> 7;
        color->ch.red = src_buf[2]>> 7;
#elif LV_COLOR_DEPTH == 8
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = src_buf[0] >> 6;
        color->ch.green = src_buf[1] >> 5;
        color->ch.red = src_buf[2]>> 5;
#elif LV_COLOR_DEPTH == 16
#if LV_COLOR_16_SWAP == 0
        /*Because of Alpha byte 16 bit color can start on odd address which can cause crash*/
        dst_buf[0] = ((src_buf[1] & 0x1c) << 3) | (src_buf[0]>> 3);
        dst_buf[1] = (src_buf[2] & 0xF8) | (src_buf[1]>> 5);
#else
        dst_buf[0] = (src_buf[2] & 0xF8) | (src_buf[1] >> 5);
        dst_buf[1] = ((src_buf[1] & 0x1c) << 3) | (src_buf[0]>> 3);
#endif
#elif LV_COLOR_DEPTH == 32
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = src_buf[0];
        color->ch.green = src_buf[1];
        color->ch.red = src_buf[2];
#else
#error "Invalid LV_COLOR_DEPTH. Check it in lv_conf.h"
#endif

        dst_buf += LV_COLOR_SIZE / 8;
        src_buf += 3;
    }
}

static void _decode_argb_6666(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len)
{
    for (; len > 0; len--) {
#if LV_COLOR_DEPTH == 1
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = (src_buf[0] & 0x20) >> 5;
        color->ch.green = (src_buf[1] & 0x08) >> 3;
        color->ch.red = (src_buf[2] & 0x02) >> 1;
#elif LV_COLOR_DEPTH == 8
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = (src_buf[0] & 0x30) >> 4;
        color->ch.green = (src_buf[1] & 0x0e) >> 1;
        color->ch.red = ((src_buf[1] & 0x80) >> 7) | ((src_buf[2] & 0x03) << 1);
#elif LV_COLOR_DEPTH == 16
#if LV_COLOR_16_SWAP == 0
        /*Because of Alpha byte 16 bit color can start on odd address which can cause crash*/
        dst_buf[0] = (src_buf[1] << 7) | (src_buf[0] >> 1);
        dst_buf[1] = (src_buf[2] << 6) | ((src_buf[1] >> 2) & 0x38) | ((src_buf[1] >> 1) & 0x7);
#else
        dst_buf[0] = (src_buf[2] << 6) | ((src_buf[1] >> 2) & 0x38) | ((src_buf[1] >> 1) & 0x7);
        dst_buf[1] = (src_buf[1] << 7) | (src_buf[0] >> 1);
#endif
#elif LV_COLOR_DEPTH == 32
        dst_buf[0] = (src_buf[0] << 2) | (src_buf[0] & 0x3);
        dst_buf[1] = (src_buf[1] << 4) | ((src_buf[0] >> 4) & 0x0c) | (src_buf[0] >> 6);
        dst_buf[2] = (src_buf[2] << 6) | ((src_buf[1] >> 2) & 0x3c) | ((src_buf[1] >> 4) & 0x03);
#else
#error "Invalid LV_COLOR_DEPTH. Check it in lv_conf.h"
#endif

        dst_buf[LV_IMG_PX_SIZE_ALPHA_BYTE - 1] = (src_buf[2] & 0xfc) | ((src_buf[2] >> 2) & 0x03);
        dst_buf += LV_IMG_PX_SIZE_ALPHA_BYTE;
        src_buf += 3;
    }
}

static void _decode_argb_1555(uint8_t * dst_buf, const uint8_t * src_buf, lv_coord_t len)
{
    for (; len > 0; len--) {
#if LV_COLOR_DEPTH == 8 || LV_COLOR_DEPTH == 1
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = (src_buf[0] & 0x10) >> 4;
        color->ch.green = (src_buf[1] & 0x02) >> 1;
        color->ch.red = (src_buf[1] & 0x40) >> 6;
#elif LV_COLOR_DEPTH == 8
        lv_color_t *color = (lv_color_t *)dst_buf;
        color->ch.blue = (src_buf[0] & 0x18) >> 3;
        color->ch.green = ((src_buf[0] & 0x80) >> 7) | ((src_buf[1] & 0x03) << 1);
        color->ch.red = (src_buf[1] & 0x70) >> 4;
#elif LV_COLOR_DEPTH == 16
#if LV_COLOR_16_SWAP == 0
        /*Because of Alpha byte 16 bit color can start on odd address which can cause crash*/
        dst_buf[0] = (src_buf[0] & 0x3f) | ((src_buf[0] & 0x60) << 1);
        dst_buf[1] = (src_buf[1] << 1) | ((src_buf[0] & 0x80) >> 7);
#else
        dst_buf[0] = (src_buf[1] << 1) | ((src_buf[0] & 0x80) >> 7);
        dst_buf[1] = (src_buf[0] & 0x3f) | ((src_buf[0] & 0x60) << 1);
#endif
#elif LV_COLOR_DEPTH == 32
        dst_buf[0] = (src_buf[0] << 3) | (src_buf[0] & 0x7);
        dst_buf[1] = (src_buf[1] << 6) | ((src_buf[0] >> 2) & 0x38) | (src_buf[0] >> 5);
        dst_buf[2] = ((src_buf[1] & 0x7c) << 1) | ((src_buf[1] >> 2) & 0x7);
#else
#error "Invalid LV_COLOR_DEPTH. Check it in lv_conf.h"
#endif

        dst_buf[LV_IMG_PX_SIZE_ALPHA_BYTE - 1] = (src_buf[1] & 0x80) ? 255 : 0;
        dst_buf += LV_IMG_PX_SIZE_ALPHA_BYTE;
        src_buf += 2;
    }
}
