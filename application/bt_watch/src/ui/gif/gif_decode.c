#include "gif_decode.h"
#include <lvgl/lvgl.h>
#include <fs/fs.h>
#include <memory/mem_cache.h>
#include <display/ui_memsetcpy.h>
#ifdef CONFIG_UI_MANAGER
#include <ui_mem.h>
#endif
#if GIF_USE_DMA2D_RENDER
#  include <dma2d_hal.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef CONFIG_UI_MANAGER
#  define GIF_MALLOC(size)        ui_mem_alloc(MEM_RES, size, __func__)
#  define GIF_FREE(ptr)           ui_mem_free(MEM_RES, ptr)
#  define GIF_REALLOC(ptr,size)   ui_mem_realloc(MEM_RES,ptr,size,__func__)
#else
#  define GIF_MALLOC(size)        lv_mem_alloc(size)
#  define GIF_FREE(ptr)           lv_mem_free(ptr)
#  define GIF_REALLOC(ptr,size)   lv_mem_realloc(ptr,size)
#endif

typedef struct Entry {
    uint16_t length;
    uint16_t prefix;
    uint8_t  suffix;
} Entry;

typedef struct Table {
    int bulk;
    int nentries;
    Entry *entries;
} Table;

static decode_GIF *  gif_open(decode_GIF * gif);
static bool f_gif_open(decode_GIF * gif, const void * path, bool is_file);
static void f_gif_read(decode_GIF * gif, void * buf, size_t len);
static int f_gif_seek(decode_GIF * gif, size_t pos, int k);
static void f_gif_close(decode_GIF * gif);

#if GIF_USE_DMA2D_RENDER
static bool dma2d_inited = false;
static hal_dma2d_handle_t g_hdma2d;

static hal_dma2d_handle_t * get_dma2d_handle(void)
{
    if (dma2d_inited)
        return &g_hdma2d;

    if (hal_dma2d_init(&g_hdma2d, HAL_DMA2D_FULL_MODES) == 0) {
        /* initialize dma2d constant fields */
        g_hdma2d.output_cfg.mode = HAL_DMA2D_M2M;
        g_hdma2d.output_cfg.color_format = HAL_PIXEL_FORMAT_ARGB_8888;
        g_hdma2d.layer_cfg[1].alpha_mode = HAL_DMA2D_NO_MODIF_ALPHA;
        g_hdma2d.layer_cfg[1].color_format = HAL_PIXEL_FORMAT_I8;
        g_hdma2d.layer_cfg[1].input_alpha = 0xffffffff;
        g_hdma2d.layer_cfg[0].alpha_mode = HAL_DMA2D_NO_MODIF_ALPHA;
        g_hdma2d.layer_cfg[0].color_format = HAL_PIXEL_FORMAT_ARGB_8888;
        g_hdma2d.layer_cfg[0].input_alpha = 0xffffffff;

        dma2d_inited = true;
        return &g_hdma2d;
    } else {
        LV_LOG_ERROR("dma2d init failed");
    }

    return NULL;
}

static void cvt_palette32(decode_Palette *palette)
{
    int size = palette->size;
    uint8_t *color_p = palette->colors + (palette->size - 1) * 3;
    uint8_t *color32_p = palette->colors + (palette->size - 1) * 4;

    for (; size > 0; size--) {
        uint8_t color[3] = { color_p[2], color_p[1], color_p[0] };
        color32_p[0] = color[0];
        color32_p[1] = color[1];
        color32_p[2] = color[2];
        color32_p[3] = 0xff;

        color_p -= 3;
        color32_p -= 4;
    }

    palette->tindex = -1;
}

static int dma2d_render_frame_rect(decode_GIF *gif, uint8_t *buffer)
{
    if (gif->fw * gif->fh < GIF_DMA2D_RENDER_SIZE_LIMIT)
        return -EMSGSIZE;

    hal_dma2d_handle_t * hdma2d = get_dma2d_handle();
    if (hdma2d == NULL)
        return -ENODEV;

    if (hal_dma2d_clut_load_start(hdma2d, 1, gif->palette->size, (uint32_t *)gif->palette->colors) < 0)
        return -ENOTSUP;

    hdma2d->output_cfg.output_pitch = gif->width * 4;
    hal_dma2d_config_output(hdma2d);

    hdma2d->layer_cfg[1].input_pitch = gif->width;
    hdma2d->layer_cfg[1].input_width = gif->fw;
    hdma2d->layer_cfg[1].input_height = gif->fh;
    hal_dma2d_config_layer(hdma2d, 1);

    hdma2d->layer_cfg[0].input_pitch = gif->width * 4;
    hdma2d->layer_cfg[0].input_width = gif->fw;
    hdma2d->layer_cfg[0].input_height = gif->fh;
    hal_dma2d_config_layer(hdma2d, 0);

    uint32_t ofs = gif->fy * gif->width + gif->fx;

    mem_dcache_clean(&gif->frame[ofs], gif->width * gif->fh);
    mem_dcache_flush(&buffer[ofs * 4], gif->width * gif->fh * 4);

    if (gif->palette->tindex < 0) {
        if (hal_dma2d_start(hdma2d, (uint32_t)&gif->frame[ofs],
                            (uint32_t)&buffer[ofs * 4], gif->fw, gif->fh) < 0)
            return -ENOTSUP;
    } else {
        if (hal_dma2d_blending_start(hdma2d, (uint32_t)&gif->frame[ofs], (uint32_t)&buffer[ofs * 4],
		                             (uint32_t)&buffer[ofs * 4], gif->fw, gif->fh) < 0)
            return -ENOTSUP;
    }

    hal_dma2d_poll_transfer(hdma2d, -1);
    return 0;
}

#endif /* GIF_USE_DMA2D_RENDER */

static uint16_t read_num(decode_GIF * gif)
{
    uint8_t bytes[2];

    f_gif_read(gif, bytes, 2);
    return bytes[0] + (((uint16_t) bytes[1]) << 8);
}

decode_GIF *decode_open_gif_file(const char *fname)
{
    decode_GIF gif_base;
    memset(&gif_base, 0, sizeof(gif_base));

    bool res = f_gif_open(&gif_base, fname, true);
    if(!res) return NULL;

    return gif_open(&gif_base);
}

static decode_GIF * gif_open(decode_GIF * gif_base)
{
    uint8_t sigver[3];
    uint16_t width, height, depth;
    uint8_t fdsz, bgidx, aspect;
#if LV_COLOR_DEPTH < 16
    int i;
#endif
    uint8_t *bgcolor;
    int gct_sz;
    decode_GIF *gif = NULL;

    /* Header */
    f_gif_read(gif_base, sigver, 3);
    if (memcmp(sigver, "GIF", 3) != 0) {
        LV_LOG_WARN("invalid signature\n");
        goto fail;
    }
    /* Version */
    f_gif_read(gif_base, sigver, 3);
    if (memcmp(sigver, "89a", 3) != 0) {
        LV_LOG_WARN("invalid version\n");
        goto fail;
    }
    /* Width x Height */
    width  = read_num(gif_base);
    height = read_num(gif_base);
    /* FDSZ */
    f_gif_read(gif_base, &fdsz, 1);
    /* Presence of GCT */
    if (!(fdsz & 0x80)) {
        LV_LOG_WARN("no global color table\n");
        goto fail;
    }
    /* Color Space's Depth */
    depth = ((fdsz >> 4) & 7) + 1;
    /* Ignore Sort Flag. */
    /* GCT Size */
    gct_sz = 1 << ((fdsz & 0x07) + 1);
    /* Background Color Index */
    f_gif_read(gif_base, &bgidx, 1);
    /* Aspect Ratio */
    f_gif_read(gif_base, &aspect, 1);
    /* Create decode_GIF Structure. */
#if LV_COLOR_DEPTH >= 16
    gif = GIF_MALLOC(sizeof(decode_GIF) + 5 * width * height);
#elif LV_COLOR_DEPTH == 8 || LV_COLOR_DEPTH == 1
    gif = GIF_MALLOC(sizeof(decode_GIF) + 3 * width * height);
#endif

    if (!gif) goto fail;
    memcpy(gif, gif_base, sizeof(decode_GIF));
    gif->width  = width;
    gif->height = height;
    gif->depth  = depth;
    /* Read GCT */
    gif->gct.size = gct_sz;
    f_gif_read(gif, gif->gct.colors, 3 * gif->gct.size);
#if GIF_USE_DMA2D_RENDER
    cvt_palette32(&gif->gct);
#endif
    gif->palette = &gif->gct;
    gif->bgindex = bgidx;
    gif->canvas = (uint8_t *) &gif[1];
#if LV_COLOR_DEPTH >= 16
    gif->frame = &gif->canvas[4 * width * height];
#elif LV_COLOR_DEPTH == 8 || LV_COLOR_DEPTH == 1
    gif->frame = &gif->canvas[2 * width * height];
#endif
    if (gif->bgindex) {
       // memset(gif->frame, gif->bgindex, gif->width * gif->height);
       ui_memset(gif->frame, gif->bgindex, gif->width * gif->height);
       mem_dcache_flush(gif->frame, gif->width * gif->height);
       ui_memsetcpy_wait_finish(-1);
    }
#if GIF_USE_DMA2D_RENDER
    bgcolor = &gif->palette->colors[gif->bgindex*4];
#else
    bgcolor = &gif->palette->colors[gif->bgindex*3];
#endif

#if LV_COLOR_DEPTH >= 16
#if GIF_USE_DMA2D_RENDER
    uint32_t color32 = *(uint32_t *)bgcolor;
#else
    uint32_t color32 = bgcolor[2] | ((uint32_t)bgcolor[1] << 8) | ((uint32_t)bgcolor[0] << 16) | 0xff000000;
#endif
    ui_memset32(gif->canvas, color32, gif->width * gif->height);
    mem_dcache_flush(gif->canvas, gif->width * gif->height * 4);
    ui_memsetcpy_wait_finish(-1);
#else

    for (i = 0; i < gif->width * gif->height; i++) {
#if LV_COLOR_DEPTH >= 16
        gif->canvas[i*4 + 0] = *(bgcolor + 2);
        gif->canvas[i*4 + 1] = *(bgcolor + 1);
        gif->canvas[i*4 + 2] = *(bgcolor + 0);
        gif->canvas[i*4 + 3] = 0xff;
#elif LV_COLOR_DEPTH == 8
        lv_color_t c = lv_color_make(*(bgcolor + 0), *(bgcolor + 1), *(bgcolor + 2));
        gif->canvas[i*2 + 0] = c.full;
        gif->canvas[i*2 + 1] = 0xff;
#elif LV_COLOR_DEPTH == 1
        lv_color_t c = lv_color_make(*(bgcolor + 0), *(bgcolor + 1), *(bgcolor + 2));
        gif->canvas[i*2 + 0] = c.ch.red > 128 ? 1 : 0;
        gif->canvas[i*2 + 1] = 0xff;
#endif
    }
#endif /* LV_COLOR_DEPTH >= 16 */

    gif->anim_start = f_gif_seek(gif, 0, FS_SEEK_CUR);
    gif->loop_count = -1;
    goto ok;
fail:
    f_gif_close(gif_base);
ok:
    return gif;
}

static void discard_sub_blocks(decode_GIF *gif)
{
    uint8_t size;

    do {
        f_gif_read(gif, &size, 1);
        f_gif_seek(gif, size, FS_SEEK_CUR);
    } while (size);
}

static void read_plain_text_ext(decode_GIF *gif)
{
    if (gif->plain_text) {
        uint16_t tx, ty, tw, th;
        uint8_t cw, ch, fg, bg;
        size_t sub_block;
        f_gif_seek(gif, 1, FS_SEEK_CUR); /* block size = 12 */
        tx = read_num(gif);
        ty = read_num(gif);
        tw = read_num(gif);
        th = read_num(gif);
        f_gif_read(gif, &cw, 1);
        f_gif_read(gif, &ch, 1);
        f_gif_read(gif, &fg, 1);
        f_gif_read(gif, &bg, 1);
        sub_block = f_gif_seek(gif, 0, FS_SEEK_CUR);
        gif->plain_text(gif, tx, ty, tw, th, cw, ch, fg, bg);
        f_gif_seek(gif, sub_block, FS_SEEK_SET);
    } else {
        /* Discard plain text metadata. */
        f_gif_seek(gif, 13, FS_SEEK_CUR);
    }
    /* Discard plain text sub-blocks. */
    discard_sub_blocks(gif);
}

static void read_graphic_control_ext(decode_GIF *gif)
{
    uint8_t rdit;

    /* Discard block size (always 0x04). */
    f_gif_seek(gif, 1, FS_SEEK_CUR);
    f_gif_read(gif, &rdit, 1);
    gif->gce.disposal = (rdit >> 2) & 3;
    gif->gce.input = rdit & 2;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif);
    f_gif_read(gif, &gif->gce.tindex, 1);
    /* Skip block terminator. */
    f_gif_seek(gif, 1, FS_SEEK_CUR);
}

static void read_comment_ext(decode_GIF *gif)
{
    if (gif->comment) {
        size_t sub_block = f_gif_seek(gif, 0, FS_SEEK_CUR);
        gif->comment(gif);
        f_gif_seek(gif, sub_block, FS_SEEK_SET);
    }
    /* Discard comment sub-blocks. */
    discard_sub_blocks(gif);
}

static void read_application_ext(decode_GIF *gif)
{
    char app_id[8];
    char app_auth_code[3];
    uint16_t loop_count;

    /* Discard block size (always 0x0B). */
    f_gif_seek(gif, 1, FS_SEEK_CUR);
    /* Application Identifier. */
    f_gif_read(gif, app_id, 8);
    /* Application Authentication Code. */
    f_gif_read(gif, app_auth_code, 3);
    if (!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
        /* Discard block size (0x03) and constant byte (0x01). */
        f_gif_seek(gif, 2, FS_SEEK_CUR);
        loop_count = read_num(gif);
        if(gif->loop_count < 0) {
            if(loop_count == 0) {
                gif->loop_count = 0;
            }
            else{
                gif->loop_count = loop_count + 1;
            }
        }
        /* Skip block terminator. */
        f_gif_seek(gif, 1, FS_SEEK_CUR);
    } else if (gif->application) {
        size_t sub_block = f_gif_seek(gif, 0, FS_SEEK_CUR);
        gif->application(gif, app_id, app_auth_code);
        f_gif_seek(gif, sub_block, FS_SEEK_SET);
        discard_sub_blocks(gif);
    } else {
        discard_sub_blocks(gif);
    }
}

static void read_ext(decode_GIF *gif)
{
    uint8_t label;

    f_gif_read(gif, &label, 1);
    switch (label) {
    case 0x01:
        read_plain_text_ext(gif);
        break;
    case 0xF9:
        read_graphic_control_ext(gif);
        break;
    case 0xFE:
        read_comment_ext(gif);
        break;
    case 0xFF:
        read_application_ext(gif);
        break;
    default:
        LV_LOG_WARN("unknown extension: %02X\n", label);
    }
}

static Table *new_table(int key_size)
{
    int key;
    int init_bulk = MAX(1 << (key_size + 1), 0x100);
    Table *table = GIF_MALLOC(sizeof(*table) + sizeof(Entry) * init_bulk);
    if (table) {
        table->bulk = init_bulk;
        table->nentries = (1 << key_size) + 2;
        table->entries = (Entry *) &table[1];
        for (key = 0; key < (1 << key_size); key++)
            table->entries[key] = (Entry) {1, 0xFFF, key};
    }
    return table;
}

/* Add table entry. Return value:
 *  0 on success
 *  +1 if key size must be incremented after this addition
 *  -1 if could not realloc table */
static int add_entry(Table **tablep, uint16_t length, uint16_t prefix, uint8_t suffix)
{
    Table *table = *tablep;
    if (table->nentries == table->bulk) {
        table->bulk *= 2;
        table = GIF_REALLOC(table, sizeof(*table) + sizeof(Entry) * table->bulk);
        if (!table) return -1;
        table->entries = (Entry *) &table[1];
        *tablep = table;
    }
    table->entries[table->nentries] = (Entry) {length, prefix, suffix};
    table->nentries++;
    if ((table->nentries & (table->nentries - 1)) == 0)
        return 1;
    return 0;
}

static uint16_t get_key(decode_GIF *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
{
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key;

    key = 0;
    for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0) {
            /* Update byte. */
            if (*sub_len == 0) {
                f_gif_read(gif, sub_len, 1); /* Must be nonzero! */
                if (*sub_len == 0) return 0x1000;
            }
            f_gif_read(gif, byte, 1);
            (*sub_len)--;
        }
        frag_size = MIN(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t) ((*byte) >> rpad)) << bits_read;
    }
    /* Clear extra bits to the left. */
    key &= (1 << key_size) - 1;
    *shift = (*shift + key_size) % 8;
    return key;
}

/* Compute output index of y-th input line, in frame of height h. */
static int interlaced_line_index(int h, int y)
{
    int p; /* number of lines in current pass */

    p = (h - 1) / 8 + 1;
    if (y < p) /* pass 1 */
        return y * 8;
    y -= p;
    p = (h - 5) / 8 + 1;
    if (y < p) /* pass 2 */
        return y * 8 + 4;
    y -= p;
    p = (h - 3) / 4 + 1;
    if (y < p) /* pass 3 */
        return y * 4 + 2;
    y -= p;
    /* pass 4 */
    return y * 2 + 1;
}

/* Decompress image pixels.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int read_image_data(decode_GIF *gif, int interlace)
{
    uint8_t sub_len, shift, byte;
    int init_key_size, key_size, table_is_full=0;
    int frm_off, frm_size, str_len=0, i, p, x, y;
    uint16_t key, clear, stop;
    int ret;
    Table *table;
    Entry entry = {0};
    size_t start, end;

    f_gif_read(gif, &byte, 1);
    key_size = (int) byte;
    start = f_gif_seek(gif, 0, FS_SEEK_CUR);
    discard_sub_blocks(gif);
    end = f_gif_seek(gif, 0, FS_SEEK_CUR);
    f_gif_seek(gif, start, FS_SEEK_SET);
    clear = 1 << key_size;
    stop = clear + 1;
    table = new_table(key_size);
    key_size++;
    init_key_size = key_size;
    sub_len = shift = 0;
    key = get_key(gif, key_size, &sub_len, &shift, &byte); /* clear code */
    frm_off = 0;
    ret = 0;
    frm_size = gif->fw*gif->fh;
    while (frm_off < frm_size) {
        if (key == clear) {
            key_size = init_key_size;
            table->nentries = (1 << (key_size - 1)) + 2;
            table_is_full = 0;
        } else if (!table_is_full) {
            ret = add_entry(&table, str_len + 1, key, entry.suffix);
            if (ret == -1) {
                GIF_FREE(table);
                return -1;
            }
            if (table->nentries == 0x1000) {
                ret = 0;
                table_is_full = 1;
            }
        }
        key = get_key(gif, key_size, &sub_len, &shift, &byte);
        if (key == clear) continue;
        if (key == stop || key == 0x1000) break;
        if (ret == 1) key_size++;
        entry = table->entries[key];
        str_len = entry.length;
        for (i = 0; i < str_len; i++) {
            p = frm_off + entry.length - 1;
            x = p % gif->fw;
            y = p / gif->fw;
            if (interlace)
                y = interlaced_line_index((int) gif->fh, y);
            uint32_t len = (gif->fy + y) * gif->width + gif->fx + x;
            if(len < gif->width * gif->height - 1)
                gif->frame[(gif->fy + y) * gif->width + gif->fx + x] = entry.suffix;
            if (entry.prefix == 0xFFF)
                break;
            else
                entry = table->entries[entry.prefix];
        }
        frm_off += str_len;
        if (key < table->nentries - 1 && !table_is_full)
            table->entries[table->nentries - 1].suffix = entry.suffix;
    }

    GIF_FREE(table);
    if (key == stop) f_gif_read(gif, &sub_len, 1); /* Must be zero! */
    f_gif_seek(gif, end, FS_SEEK_SET);
    return 0;
}

/* Read image.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int read_image(decode_GIF *gif)
{
    uint8_t fisrz;
    int interlace;

    /* Image Descriptor. */
    gif->fx = read_num(gif);
    gif->fy = read_num(gif);
    gif->fw = read_num(gif);
    gif->fh = read_num(gif);
    f_gif_read(gif, &fisrz, 1);
    interlace = fisrz & 0x40;
    /* Ignore Sort Flag. */
    /* Local Color Table? */
    if (fisrz & 0x80) {
        /* Read LCT */
        gif->lct.size = 1 << ((fisrz & 0x07) + 1);
        f_gif_read(gif, gif->lct.colors, 3 * gif->lct.size);
#if GIF_USE_DMA2D_RENDER
        cvt_palette32(&gif->lct);
#endif
        gif->palette = &gif->lct;
    } else
        gif->palette = &gif->gct;
    /* Image Data. */
    return read_image_data(gif, interlace);
}

static void render_frame_rect(decode_GIF *gif, uint8_t *buffer)
{
#if GIF_USE_DMA2D_RENDER
    if (!gif->gce.transparency) {
        if (gif->palette->tindex >= 0) {
            gif->palette->colors[gif->palette->tindex * 4 + 3] = 0xff;
            gif->palette->tindex = -1;
        }
    } else if (gif->palette->tindex != gif->gce.tindex) {
        if (gif->palette->tindex >= 0)
            gif->palette->colors[gif->palette->tindex * 4 + 3] = 0xff;

        gif->palette->tindex = gif->gce.tindex;
        gif->palette->colors[gif->palette->tindex * 4 + 3] = 0x00;
    }

    if (dma2d_render_frame_rect(gif, buffer) >= 0)
        return;
#endif /* GIF_USE_DMA2D_RENDER */

    int i, j, k;
    uint8_t index, *color;
    i = gif->fy * gif->width + gif->fx;
    for (j = 0; j < gif->fh; j++) {
        for (k = 0; k < gif->fw; k++) {
            index = gif->frame[(gif->fy + j) * gif->width + gif->fx + k];
#if GIF_USE_DMA2D_RENDER
            color = &gif->palette->colors[index*4];
#else
            color = &gif->palette->colors[index*3];
#endif
            if (!gif->gce.transparency || index != gif->gce.tindex) {
#if LV_COLOR_DEPTH >= 16
#if GIF_USE_DMA2D_RENDER
                buffer[(i+k)*4 + 0] = *(color + 0);
                buffer[(i+k)*4 + 1] = *(color + 1);
                buffer[(i+k)*4 + 2] = *(color + 2);
                buffer[(i+k)*4 + 3] = 0xFF;
#else
                buffer[(i+k)*4 + 0] = *(color + 2);
                buffer[(i+k)*4 + 1] = *(color + 1);
                buffer[(i+k)*4 + 2] = *(color + 0);
                buffer[(i+k)*4 + 3] = 0xFF;
#endif
#elif LV_COLOR_DEPTH == 8
                lv_color_t c = lv_color_make(*(color + 0), *(color + 1), *(color + 2));
                buffer[(i+k)*2 + 0] = c.full;
                buffer[(i+k)*2 + 1] = 0xff;
#elif LV_COLOR_DEPTH == 1
                uint8_t b = (*(color + 0)) | (*(color + 1)) | (*(color + 2));
                buffer[(i+k)*2 + 0] = b > 128 ? 1 : 0;
                buffer[(i+k)*2 + 1] = 0xff;
#endif
            }
        }
        i += gif->width;
    }

#if LV_COLOR_DEPTH >= 16
    mem_dcache_clean(&buffer[(gif->fy * gif->width + gif->fx) * 4], 4 * gif->width * gif->fh);
#else
    mem_dcache_clean(&buffer[(gif->fy * gif->width + gif->fx) * 2], 2 * gif->width * gif->fh);
#endif
}

static void dispose(decode_GIF *gif)
{
#if LV_COLOR_DEPTH >= 16
    int i;
#else
    int i, j, k;
#endif
    uint8_t *bgcolor;
    switch (gif->gce.disposal) {
    case 2: /* Restore to background color. */
#if GIF_USE_DMA2D_RENDER
        bgcolor = &gif->palette->colors[gif->bgindex*4];
#else
        bgcolor = &gif->palette->colors[gif->bgindex*3];
#endif

        uint8_t opa = 0xff;
        if(gif->gce.transparency) opa = 0x00;

        i = gif->fy * gif->width + gif->fx;

#if LV_COLOR_DEPTH >= 16
#if GIF_USE_DMA2D_RENDER
        uint32_t color32 = (*(uint32_t *)bgcolor & 0xffffff) | ((uint32_t)opa << 24);
#else
        uint32_t color32 = bgcolor[2] | ((uint32_t)bgcolor[1] << 8) | ((uint32_t)bgcolor[0] << 16) | ((uint32_t)opa << 24);
#endif
        ui_memset2d_32(&gif->canvas[i*4], gif->width * 4, color32, gif->fw, gif->fh);
        mem_dcache_flush(&gif->canvas[i*4], gif->width * gif->fh * 4);
        ui_memsetcpy_wait_finish(-1);
#else

        for (j = 0; j < gif->fh; j++) {
            for (k = 0; k < gif->fw; k++) {
#if LV_COLOR_DEPTH >= 16
                gif->canvas[(i+k)*4 + 0] = *(bgcolor + 2);
                gif->canvas[(i+k)*4 + 1] = *(bgcolor + 1);
                gif->canvas[(i+k)*4 + 2] = *(bgcolor + 0);
                gif->canvas[(i+k)*4 + 3] = opa;
#elif LV_COLOR_DEPTH == 8
                lv_color_t c = lv_color_make(*(bgcolor + 0), *(bgcolor + 1), *(bgcolor + 2));
                gif->canvas[(i+k)*2 + 0] = c.full;
                gif->canvas[(i+k)*2 + 1] = opa;
#elif LV_COLOR_DEPTH == 1
                uint8_t b = (*(bgcolor + 0)) | (*(bgcolor + 1)) | (*(bgcolor + 2));
                gif->canvas[(i+k)*2 + 0] = b > 128 ? 1 : 0;
                gif->canvas[(i+k)*2 + 1] = opa;
#endif
            }
            i += gif->width;
        }
#endif /* LV_COLOR_DEPTH >= 16 */

        break;
    case 3: /* Restore to previous, i.e., don't update canvas.*/
        break;
    default:
        /* Add frame non-transparent pixels to canvas. */
        render_frame_rect(gif, gif->canvas);
    }
}

/* Return 1 if got a frame; 0 if got GIF trailer; -1 if error. */
int decode_get_frame(decode_GIF *gif)
{
    char sep;

    dispose(gif);
    f_gif_read(gif, &sep, 1);
    while (sep != ',') {
        if (sep == ';') {
            f_gif_seek(gif, gif->anim_start, FS_SEEK_SET);
            if(gif->loop_count == 1 || gif->loop_count < 0) {
                return 0;
            }
            else if(gif->loop_count > 1) {
                gif->loop_count--;
            }
        }
        else if (sep == '!')
            read_ext(gif);
        else return -1;
        f_gif_read(gif, &sep, 1);
    }
    if (read_image(gif) == -1)
        return -1;
    return 1;
}

void decode_render_frame(decode_GIF *gif, uint8_t *buffer)
{
//    uint32_t i;
//    uint32_t j;
//    for(i = 0, j = 0; i < gif->width * gif->height * 3; i+= 3, j+=4) {
//        buffer[j + 0] = gif->canvas[i + 2];
//        buffer[j + 1] = gif->canvas[i + 1];
//        buffer[j + 2] = gif->canvas[i + 0];
//        buffer[j + 3] = 0xFF;
//    }
//    memcpy(buffer, gif->canvas, gif->width * gif->height * 3);
    render_frame_rect(gif, buffer);
}

void decode_rewind(decode_GIF *gif)
{
    gif->loop_count = -1;
    f_gif_seek(gif, gif->anim_start, FS_SEEK_SET);
}

void decode_close_gif(decode_GIF *gif)
{
    f_gif_close(gif);
    GIF_FREE(gif);
}

static bool f_gif_open(decode_GIF * gif, const void * path, bool is_file)
{
    gif->data = NULL;
    if (fs_open(&gif->fd, path , FS_O_READ)) {
        LV_LOG_ERROR("%s not found", (char *)path);
        return LV_RES_INV;
    }
    return LV_RES_OK;
}

static void f_gif_read(decode_GIF * gif, void * buf, size_t len)
{
    fs_read(&gif->fd, buf, len);
}

static int f_gif_seek(decode_GIF * gif, size_t pos, int k)
{
    fs_seek(&gif->fd, pos, k);
    uint32_t x = fs_tell(&gif->fd);
    return x;
}

static void f_gif_close(decode_GIF * gif)
{
    fs_close(&gif->fd);
}

