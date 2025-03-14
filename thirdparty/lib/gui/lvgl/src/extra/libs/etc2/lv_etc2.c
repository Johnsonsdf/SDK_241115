/**
 * @file lv_etc2.c
 *
 */
/*********************
 *      INCLUDES
 *********************/
#include "../../../lvgl.h"

#if LV_USE_ETC2

/*********************
 *      DEFINES
 *********************/
#define ETC2_RGB_NO_MIPMAPS             1
#define ETC2_RGBA_NO_MIPMAPS            3
#define ETC2_PKM_HEADER_SIZE            16
#define ETC2_PKM_FORMAT_OFFSET          6
#define ETC2_PKM_ENCODED_WIDTH_OFFSET   8
#define ETC2_PKM_ENCODED_HEIGHT_OFFSET  10
#define ETC2_PKM_WIDTH_OFFSET           12
#define ETC2_PKM_HEIGHT_OFFSET          14

#define ETC2_EAC_FORMAT_CODE            3

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#if !LV_USE_GPU_ACTS_VG_LITE
extern void setupAlphaTable(void);
extern void decompressBlockAlphaC(uint8_t * data, uint8_t * img, int width, int height, int ix, int iy, int channels);
extern void decompressBlockETC2c(uint32_t block_part1, uint32_t block_part2, uint8_t * img, int width, int height, int startx, int starty, int channels);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/

static uint16_t read_big_endian_uint16(const uint8_t * buf);
static lv_res_t decoder_info(lv_img_decoder_t * decoder, const void * src, lv_img_header_t * header);
static lv_res_t decoder_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);
static void decoder_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_etc2_init(void)
{
    lv_img_decoder_t * dec = lv_img_decoder_create();
    LV_ASSERT_MALLOC(dec);
    if(dec == NULL) {
        LV_LOG_WARN("lv_etc2_init: out of memory");
        return;
    }

    lv_img_decoder_set_info_cb(dec, decoder_info);
    lv_img_decoder_set_open_cb(dec, decoder_open);
    lv_img_decoder_set_close_cb(dec, decoder_close);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static uint16_t read_big_endian_uint16(const uint8_t * buf)
{
    return (buf[0] << 8) | buf[1];
}

#if !LV_USE_GPU_ACTS_VG_LITE
static uint32_t read_big_endian_uint32(const uint8_t * buf)
{
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static void copy_block(uint8_t * dstbuf, uint32_t * pixel32, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t dst_pitch = w * LV_IMG_PX_SIZE_ALPHA_BYTE;
    uint16_t copy_w = LV_MIN(w - x, 4);
    uint16_t copy_h = LV_MIN(h - y, 4);

    dstbuf += dst_pitch * y + x * LV_IMG_PX_SIZE_ALPHA_BYTE;

    for(; copy_h > 0; copy_h--, pixel32 += 4, dstbuf += dst_pitch) {
        uint8_t *dst8 = dstbuf;
        uint8_t *pixel8 = (uint8_t *)pixel32;

        for(x = copy_w; x > 0; x--, pixel8 += 4, dst8 += LV_IMG_PX_SIZE_ALPHA_BYTE) {
#if LV_COLOR_DEPTH == 32
            dst8[0] = pixel8[2];
            dst8[1] = pixel8[1];
            dst8[2] = pixel8[0];
#elif LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0
            dst8[0] = ((pixel8[1] & 0x1c) << 3) | (pixel8[2] >> 3);
            dst8[1] = (pixel8[0] & 0xf8) | (pixel8[1] >> 5);
#elif LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 1
            dst8[0] = (pixel8[0] & 0xf8) | (pixel8[1] >> 5);
            dst8[1] = ((pixel8[1] & 0x1c) << 3) | (pixel8[2] >> 3);
#else
# error "Invalid LV_COLOR_DEPTH. Check it in lv_conf.h"
#endif
            dst8[LV_IMG_PX_SIZE_ALPHA_BYTE - 1] = pixel8[3];
        }
    }
}

static lv_res_t decode_etc2(uint8_t * dstbuf, uint16_t w, uint16_t h, uint8_t * data, lv_fs_file_t * fp)
{
    uint8_t data8[16];
    uint32_t pixel32[16];

    setupAlphaTable();

    if(fp) data = data8;

    for(uint16_t y = 0; y < h; y += 4) {
        for(uint16_t x = 0; x < w; x += 4) {
            if(fp) {
                uint32_t rn;
                lv_res_t res = lv_fs_read(fp, data, 16, &rn);
                if(res != LV_FS_RES_OK || rn != 16) {
                    LV_LOG_WARN("read ETC2 block failed");
                    return LV_RES_INV;
                }
            }

            uint8_t *pixel8 = (uint8_t *)pixel32;
            decompressBlockAlphaC(data, pixel8 + 3, 4, 4, 0, 0, 4);

            uint32_t block_part1 = read_big_endian_uint32(data + 8);
            uint32_t block_part2 = read_big_endian_uint32(data + 12);
            decompressBlockETC2c(block_part1, block_part2, pixel8, 4, 4, 0, 0, 4);

            copy_block(dstbuf, pixel32, x, y, w, h);

            if(fp == NULL) data += 16;
        }
    }

    return LV_RES_OK;
}
#endif /* !LV_USE_GPU_ACTS_VG_LITE */

static lv_res_t decoder_info(lv_img_decoder_t * decoder, const void * src, lv_img_header_t * header)
{
    LV_UNUSED(decoder); /*Unused*/

    lv_img_src_t src_type = lv_img_src_get_type(src);

    if(src_type == LV_IMG_SRC_VARIABLE) {
        lv_img_cf_t cf = ((lv_img_dsc_t *)src)->header.cf;
        if (cf !=LV_IMG_CF_ETC2_EAC) return LV_RES_INV;

        header->cf = cf;
        header->w  = ((lv_img_dsc_t *)src)->header.w;
        header->h  = ((lv_img_dsc_t *)src)->header.h;
        return LV_RES_OK;
    }
    else if(src_type == LV_IMG_SRC_FILE) {
        const char * ext = lv_fs_get_ext(src);
        if(!(strcmp(ext, "pkm") == 0)) {
            return LV_RES_INV;
        }

        lv_fs_file_t f;
        lv_fs_res_t res = lv_fs_open(&f, src, LV_FS_MODE_RD);
        if(res != LV_FS_RES_OK) {
            LV_LOG_WARN("open %s failed", (const char *)src);
            return LV_RES_INV;
        }

        uint32_t rn;
        uint8_t pkm_header[ETC2_PKM_HEADER_SIZE];
        static const char pkm_magic[] = { 'P', 'K', 'M', ' ', '2', '0' };

        res = lv_fs_read(&f, pkm_header, ETC2_PKM_HEADER_SIZE, &rn);
        lv_fs_close(&f);

        if(res != LV_FS_RES_OK || rn != ETC2_PKM_HEADER_SIZE) {
            LV_LOG_WARN("Image get info read file magic number failed");
            return LV_RES_INV;
        }

        if(memcmp(pkm_header, pkm_magic, sizeof(pkm_magic)) != 0) {
            LV_LOG_WARN("Image get info magic number invalid");
            return LV_RES_INV;
        }

        uint16_t pkm_format = read_big_endian_uint16(pkm_header + ETC2_PKM_FORMAT_OFFSET);
        if(pkm_format != ETC2_EAC_FORMAT_CODE) {
            LV_LOG_WARN("Image header format invalid : %d", pkm_format);
            return LV_RES_INV;
        }

        header->cf = LV_IMG_CF_ETC2_EAC;
        header->w = read_big_endian_uint16(pkm_header + ETC2_PKM_WIDTH_OFFSET);
        header->h = read_big_endian_uint16(pkm_header + ETC2_PKM_HEIGHT_OFFSET);
        /* header->stride = read_big_endian_uint16(pkm_header + ETC2_PKM_ENCODED_WIDTH_OFFSET); */

        return LV_RES_OK;
    }

    return LV_RES_INV;
}

static lv_res_t decoder_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
#if !LV_USE_GPU_ACTS_VG_LITE
    lv_img_header_t * header = &dsc->header;
    lv_fs_res_t res = LV_RES_OK;

    dsc->user_data = lv_mem_alloc(header->w * header->h * LV_IMG_PX_SIZE_ALPHA_BYTE);
    LV_ASSERT_MALLOC(dsc->user_data);
    if(dsc->user_data == NULL) return LV_RES_INV;

    if(dsc->src_type == LV_IMG_SRC_VARIABLE) {
        res = decode_etc2(dsc->user_data, header->w, header->h, (uint8_t *)((lv_img_dsc_t *)dsc->src)->data, NULL);
    }
    else if(dsc->src_type == LV_IMG_SRC_FILE) {
        lv_fs_file_t f;
        res = lv_fs_open(&f, dsc->src, LV_FS_MODE_RD);
        if(res != LV_FS_RES_OK) {
            LV_LOG_WARN("etc2 decoder open %s failed", (const char *)dsc->src);
            lv_mem_free(dsc->user_data);
            return LV_RES_INV;
        }

        res = lv_fs_seek(&f, ETC2_PKM_HEADER_SIZE, LV_FS_SEEK_SET);
        if(res != LV_FS_RES_OK) {
            LV_LOG_WARN("etc2 file seek %s failed", (const char *)dsc->src);
            lv_fs_close(&f);
            lv_mem_free(dsc->user_data);
            return LV_RES_INV;
        }

        res = decode_etc2(dsc->user_data, header->w, header->h, NULL, &f);

        lv_fs_close(&f);
    }

    if(res == LV_RES_OK) {
        dsc->img_data = dsc->user_data;
    }
    else {
        lv_mem_free(dsc->user_data);
        dsc->user_data = NULL;
    }

    return res;

#else
    return LV_RES_OK;
#endif
}

static void decoder_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder); /*Unused*/

    if(dsc->user_data) {
        lv_mem_free(dsc->user_data);
        dsc->user_data = NULL;
    }
}

#endif /*LV_USE_ETC2*/
