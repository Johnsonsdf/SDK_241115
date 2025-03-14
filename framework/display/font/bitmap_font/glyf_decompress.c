/*********************
 *      INCLUDES
 *********************/
#include <os_common_api.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef enum {
    RLE_STATE_SINGLE = 0,
    RLE_STATE_REPEATE,
    RLE_STATE_COUNTER,
} rle_state_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
void decompress_glyf_bitmap(const uint8_t * in, uint8_t * out, int16_t w, int16_t h, uint8_t bpp, bool prefilter, uint8_t* linebuf1, uint8_t* linebuf2);
static inline void decompress_line(uint8_t * out, int16_t w);
static inline uint8_t get_bits(const uint8_t * in, uint32_t bit_pos, uint8_t len);
static inline void bits_write(uint8_t * out, uint32_t bit_pos, uint8_t val, uint8_t len);
static inline void rle_init(const uint8_t * in,  uint8_t bpp);
static inline uint8_t rle_next(void);



/**********************
 *  STATIC VARIABLES
 **********************/
static uint32_t rle_rdp;
static const uint8_t * rle_in;
static uint8_t rle_bpp;
static uint8_t rle_prev_v;
static uint8_t rle_cnt;
static rle_state_t rle_state;


/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * The compress a glyph's bitmap
 * @param in the compressed bitmap
 * @param out buffer to store the result
 * @param px_num number of pixels in the glyph (width * height)
 * @param bpp bit per pixel (bpp = 3 will be converted to bpp = 4)
 * @param prefilter true: the lines are XORed
 */
void decompress_glyf_bitmap(const uint8_t * in, uint8_t * out, int16_t w, int16_t h, uint8_t bpp, bool prefilter, uint8_t* linebuf1, uint8_t* linebuf2)
{
    uint32_t wrp = 0;
    uint8_t wr_size = bpp;
    if(bpp == 3) wr_size = 4;

    rle_init(in, bpp);

    //uint8_t * line_buf1 = bitmap_font_cache_malloc(w);
    uint8_t* line_buf1 = linebuf1;

    uint8_t * line_buf2 = NULL;

    if(prefilter) {		
        //line_buf2 = bitmap_font_cache_malloc(w);
        line_buf2 = linebuf2;
    }

    decompress_line(line_buf1, w);

    int16_t y;
    int16_t x;

    for(x = 0; x < w; x++) {
        bits_write(out, wrp, line_buf1[x], bpp);
        wrp += wr_size;
    }

    for(y = 1; y < h; y++) {
        if(prefilter) {
            decompress_line(line_buf2, w);

            for(x = 0; x < w; x++) {
                line_buf1[x] = line_buf2[x] ^ line_buf1[x];
                bits_write(out, wrp, line_buf1[x], bpp);
                wrp += wr_size;
            }
        }
        else {
            decompress_line(line_buf1, w);

            for(x = 0; x < w; x++) {
                bits_write(out, wrp, line_buf1[x], bpp);
                wrp += wr_size;
            }
        }
    }

    //bitmap_font_cache_free(line_buf1);
    //bitmap_font_cache_free(line_buf2);
}

/**
 * Decompress one line. Store one pixel per byte
 * @param out output buffer
 * @param w width of the line in pixel count
 */
static inline void decompress_line(uint8_t * out, int16_t w)
{
    int16_t i;
    for(i = 0; i < w; i++) {
        out[i] = rle_next();
    }
}

/**
 * Read bits from an input buffer. The read can cross byte boundary.
 * @param in the input buffer to read from.
 * @param bit_pos index of the first bit to read.
 * @param len number of bits to read (must be <= 8).
 * @return the read bits
 */
static inline uint8_t get_bits(const uint8_t * in, uint32_t bit_pos, uint8_t len)
{
    uint8_t bit_mask;
    switch(len) {
        case 1:
            bit_mask = 0x1;
            break;
        case 2:
            bit_mask = 0x3;
            break;
        case 3:
            bit_mask = 0x7;
            break;
        case 4:
            bit_mask = 0xF;
            break;
        case 8:
            bit_mask = 0xFF;
            break;
        default:
            bit_mask = (uint16_t)((uint16_t) 1 << len) - 1;
    }

    uint32_t byte_pos = bit_pos >> 3;
    bit_pos = bit_pos & 0x7;

    if(bit_pos + len >= 8) {
        uint16_t in16 = (in[byte_pos] << 8) + in[byte_pos + 1];
        return (in16 >> (16 - bit_pos - len)) & bit_mask;
    }
    else {
        return (in[byte_pos] >> (8 - bit_pos - len)) & bit_mask;
    }
}

/**
 * Write `val` data to `bit_pos` position of `out`. The write can NOT cross byte boundary.
 * @param out buffer where to write
 * @param bit_pos bit index to write
 * @param val value to write
 * @param len length of bits to write from `val`. (Counted from the LSB).
 * @note `len == 3` will be converted to `len = 4` and `val` will be upscaled too
 */
static inline void bits_write(uint8_t * out, uint32_t bit_pos, uint8_t val, uint8_t len)
{
    if(len == 3) {
        len = 4;
        switch(val) {
            case 0:
                val = 0;
                break;
            case 1:
                val = 2;
                break;
            case 2:
                val = 4;
                break;
            case 3:
                val = 6;
                break;
            case 4:
                val = 9;
                break;
            case 5:
                val = 11;
                break;
            case 6:
                val = 13;
                break;
            case 7:
                val = 15;
                break;
        }
    }

    uint16_t byte_pos = bit_pos >> 3;
    bit_pos = bit_pos & 0x7;
    bit_pos = 8 - bit_pos - len;

    uint8_t bit_mask = (uint16_t)((uint16_t) 1 << len) - 1;
    out[byte_pos] &= ((~bit_mask) << bit_pos);
    out[byte_pos] |= (val << bit_pos);
}

static inline void rle_init(const uint8_t * in,  uint8_t bpp)
{
    rle_in = in;
    rle_bpp = bpp;
    rle_state = RLE_STATE_SINGLE;
    rle_rdp = 0;
    rle_prev_v = 0;
    rle_cnt = 0;
}

static inline uint8_t rle_next(void)
{
    uint8_t v = 0;
    uint8_t ret = 0;

    if(rle_state == RLE_STATE_SINGLE) {
        ret = get_bits(rle_in, rle_rdp, rle_bpp);
        if(rle_rdp != 0 && rle_prev_v == ret) {
            rle_cnt = 0;
            rle_state = RLE_STATE_REPEATE;
        }

        rle_prev_v = ret;
        rle_rdp += rle_bpp;
    }
    else if(rle_state == RLE_STATE_REPEATE) {
        v = get_bits(rle_in, rle_rdp, 1);
        rle_cnt++;
        rle_rdp += 1;
        if(v == 1) {
            ret = rle_prev_v;
            if(rle_cnt == 11) {
                rle_cnt = get_bits(rle_in, rle_rdp, 6);
                rle_rdp += 6;
                if(rle_cnt != 0) {
                    rle_state = RLE_STATE_COUNTER;
                }
                else {
                    ret = get_bits(rle_in, rle_rdp, rle_bpp);
                    rle_prev_v = ret;
                    rle_rdp += rle_bpp;
                    rle_state = RLE_STATE_SINGLE;
                }
            }
        }
        else {
            ret = get_bits(rle_in, rle_rdp, rle_bpp);
            rle_prev_v = ret;
            rle_rdp += rle_bpp;
            rle_state = RLE_STATE_SINGLE;
        }

    }
    else if(rle_state == RLE_STATE_COUNTER) {
        ret = rle_prev_v;
        rle_cnt--;
        if(rle_cnt == 0) {
            ret = get_bits(rle_in, rle_rdp, rle_bpp);
            rle_prev_v = ret;
            rle_rdp += rle_bpp;
            rle_state = RLE_STATE_SINGLE;
        }
    }

    return ret;
}

