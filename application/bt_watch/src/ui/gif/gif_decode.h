/**
 * @file gif_decode.h
 *
 */

#ifndef GIF_DECODE_H
#define GIF_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <lvgl/lvgl.h>
#include <fs/fs.h>

/*********************
 *      DEFINES
 *********************/

#if defined(CONFIG_DMA2D_HAL) && LV_COLOR_DEPTH >= 16
#  define GIF_USE_DMA2D_RENDER 1
#  define GIF_DMA2D_RENDER_SIZE_LIMIT 32 /* render using DMA2D only above this area size */
#else
#  define GIF_USE_DMA2D_RENDER 0
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef struct _decode_Palette {
#if GIF_USE_DMA2D_RENDER
    union {
        struct {
            int16_t size;
            int16_t tindex;
        };
        int __reserverd; /* make sure 4-byte alignment */
    };
    uint8_t colors[0x100 * 4];
#else
    int size;
    uint8_t colors[0x100 * 3];
#endif
} decode_Palette;

typedef struct _decode_GCE {
    uint16_t delay;
    uint8_t tindex;
    uint8_t disposal;
    int input;
    int transparency;
} decode_GCE;

typedef struct decode_GIF {
    struct fs_file_t fd;
    const char * data;
    //uint32_t f_rw_p;
    int32_t anim_start;
    uint16_t width, height;
    uint16_t depth;
    int32_t loop_count;
    decode_GCE gce;
    decode_Palette *palette;
    decode_Palette lct, gct;
    void (*plain_text)(
        struct decode_GIF *gif, uint16_t tx, uint16_t ty,
        uint16_t tw, uint16_t th, uint8_t cw, uint8_t ch,
        uint8_t fg, uint8_t bg
    );
    void (*comment)(struct decode_GIF *gif);
    void (*application)(struct decode_GIF *gif, char id[8], char auth[3]);
    uint16_t fx, fy, fw, fh;
    uint8_t bgindex;
    uint8_t *canvas, *frame;
} decode_GIF;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
decode_GIF *decode_open_gif_file(const char *fname);

void decode_close_gif(decode_GIF *gif);

int decode_get_frame(decode_GIF *gif);

void decode_render_frame(decode_GIF *gif, uint8_t *buffer);

void decode_rewind(decode_GIF *gif);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
