/****************************************************************************
*
*    Copyright 2023 Vivante Corporation, Santa Clara, California.
*    All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

#define _GNU_SOURCE /* make strtok_r() visible */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr.h>

#ifdef CONFIG_UI_MEMORY_MANAGER
#  include <ui_mem.h>
#  define NANOSVG_MALLOC(size)        ui_mem_alloc(MEM_RES, size, __func__)
#  define NANOSVG_FREE(ptr)           ui_mem_free(MEM_RES, ptr)
#  define NANOSVG_REALLOC(ptr, size)  ui_mem_realloc(MEM_RES, ptr, size, __func__)
#else
#  define NANOSVG_MALLOC(size)        malloc(size)
#  define NANOSVG_FREE(ptr)           free(ptr)
#  define NANOSVG_REALLOC(ptr, size)  realloc(ptr, size)
#endif

#define NANOSVG_PRINT(str, ...)     printf(str, ##__VA_ARGS__)
#define NANOSVG_TRACE(str, ...)     printf(str, ##__VA_ARGS__)
#define NANOSVG_GETTIME             k_uptime_get_32

#define NANOSVG_ALL_COLOR_KEYWORDS                // Include full list of color keywords.
#define NANOSVG_IMPLEMENTATION                    // Expands NanoSVG implementation
#define VG_STABLE_MODE                        1   // Enhance stability when drawing complex paths.

#define DUMP_API                              0   // Dump VGLite API.
#define USE_BOUNDARY_JUDGMENT                 0   // When USE_BOUNDARY_JUDGMENT is enabled, determine if the rendering boundary of path is within buffer.


#if VG_STABLE_MODE
#define MAX_COMMITTED_PATHS                   4   // set 4 for hero.svg
static int commit_count = 0;
#endif

#include "nanosvg.h"
#include "vg_lite.h"

extern int spng_load_memory(vg_lite_buffer_t * buffer, const void * png_bytes, size_t png_len);
extern int spng_free_buffer(vg_lite_buffer_t * buffer);

#ifdef CONFIG_FREETYPE_FONT
#  include <ft2build.h>
#  include FT_BBOX_H
#  include FT_OUTLINE_H
#endif

#define VGL_ERROR_CHECK(func) \
    if ((error = func) != VG_LITE_SUCCESS) \
    goto VGLError

#define swap(a,b) {int temp; temp=a; a=b; b=temp;}

#define  PATH_DATA_BUFFER_SIZE  8192
#define  PATTERN_BUFFER_SIZE    1024

#define  VG_LITE_CMDBUF_SIZE    (128 << 10)

static vg_lite_float_t path_data_buf[PATH_DATA_BUFFER_SIZE];
static vg_lite_float_t pattern_data_buf[PATTERN_BUFFER_SIZE];

static vg_lite_color_ramp_t grad_ramps[VLC_MAX_COLOR_RAMP_STOPS];

static uint32_t grad_colors[VLC_MAX_GRADIENT_STOPS];
static uint32_t grad_stops[VLC_MAX_GRADIENT_STOPS];

static const vg_lite_gradient_spreadmode_t vglspread[] = { VG_LITE_GRADIENT_SPREAD_PAD, VG_LITE_GRADIENT_SPREAD_REFLECT, VG_LITE_GRADIENT_SPREAD_REPEAT };

static vg_lite_error_t error = VG_LITE_SUCCESS;
static vg_lite_matrix_t matrix;
static vg_lite_path_t vglpath;
#ifdef CONFIG_FREETYPE_FONT
static vg_lite_path_t fontpath;
static unsigned int unicode;
#endif
static vg_lite_fill_t fill_rule;
static vg_lite_float_t shape_bound[4]; // [minx,miny,maxx,maxy]
static const vg_lite_float_t scale = 1.0f;

#ifdef CONFIG_FREETYPE_FONT
#define TEXT_SIZE 1024
static int32_t text_pdata_start[TEXT_SIZE];
static int32_t * text_pdata_cur = text_pdata_start;
const static int32_t * text_pdata_end = text_pdata_start + TEXT_SIZE;

static int move_to(const FT_Vector* to, void* user)
{
    if (text_pdata_cur + 3 < text_pdata_end) {
        *text_pdata_cur++ = VLC_OP_MOVE;
        *text_pdata_cur++ = to->x;
        *text_pdata_cur++ = -to->y;
    } else {
        NANOSVG_PRINT("increase TEXT_SIZE to complex glyph\n");
    }

    return 0;
}

static int line_to(const FT_Vector* to, void* user)
{
    if (text_pdata_cur + 3 < text_pdata_end) {
        *text_pdata_cur++ = VLC_OP_LINE;
        *text_pdata_cur++ = to->x;
        *text_pdata_cur++ = -to->y;
    } else {
        NANOSVG_PRINT("increase TEXT_SIZE to complex glyph\n");
    }

    return 0;
}

static int conic_to(const FT_Vector* control, const FT_Vector* to, void* user)
{
    if (text_pdata_cur + 5 < text_pdata_end) {
        *text_pdata_cur++ = VLC_OP_QUAD;
        *text_pdata_cur++ = control->x;
        *text_pdata_cur++ = -control->y;
        *text_pdata_cur++ = to->x;
        *text_pdata_cur++ = -to->y;
    } else {
        NANOSVG_PRINT("increase TEXT_SIZE to complex glyph\n");
    }

    return 0;
}

static int cubic_to(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user)
{
    if (text_pdata_cur + 7 < text_pdata_end) {
        *text_pdata_cur++ = VLC_OP_QUAD;
        *text_pdata_cur++ = control1->x;
        *text_pdata_cur++ = -control1->y;
        *text_pdata_cur++ = control2->x;
        *text_pdata_cur++ = -control2->y;
        *text_pdata_cur++ = to->x;
        *text_pdata_cur++ = -to->y;
    } else {
        NANOSVG_PRINT("increase TEXT_SIZE to complex glyph\n");
    }

    return 0;
}

static void setOpacity(vg_lite_color_t* pcolor, float opacity)
{
    if (opacity != 1.0f)
    {
        vg_lite_color_t color = *pcolor;
        uint8_t a = (uint8_t)(((color & 0xFF000000) >> 24) * opacity);
        uint8_t b = (uint8_t)(((color & 0x00FF0000) >> 16) * opacity);
        uint8_t g = (uint8_t)(((color & 0x0000FF00) >> 8) * opacity);
        uint8_t r = (uint8_t)(((color & 0x000000FF) >> 0) * opacity);
        *pcolor = ((r << 0) | (g << 8) | (b << 16) | (a << 24));
    }
}

static vg_lite_error_t drawCharacter(vg_lite_buffer_t * fb, NSVGshape* shape, float size, float x, float y, unsigned int color,
                                     float scale, int box_xMin, int box_yMin, int box_xMax, int box_yMax)
{
    vg_lite_matrix_t font_matrix;

    *text_pdata_cur++ = VLC_OP_END;

    VGL_ERROR_CHECK(vg_lite_init_path(&fontpath, VG_LITE_S32, VG_LITE_MEDIUM,
            (text_pdata_cur - text_pdata_start) * 4 - 3, text_pdata_start, box_xMin, box_yMin, box_xMax, box_yMax));

    if (shape->fill.text->hasStroke == 1)
    {
        vg_lite_cap_style_t linecap = VG_LITE_CAP_BUTT + shape->strokeLineCap;
        vg_lite_join_style_t joinstyle = VG_LITE_JOIN_MITER + shape->strokeLineJoin;
        vg_lite_float_t strokewidth = shape->strokeWidth ;

        vg_lite_color_t stroke_color = shape->stroke.color;

        /* Render the stroke path only */
        fontpath.path_type = VG_LITE_DRAW_STROKE_PATH;
        /* Disable anti-aliasing line rendering for thin stroke line */
        fontpath.quality = VG_LITE_MEDIUM;
        setOpacity(&stroke_color, shape->opacity);

        /* Setup stroke parameters properly */
        VGL_ERROR_CHECK(vg_lite_set_stroke(&fontpath, linecap, joinstyle, strokewidth, shape->miterLimit,
            shape->strokeDashArray, shape->strokeDashCount, shape->strokeDashOffset, 0xFFFFFFFF));
        VGL_ERROR_CHECK(vg_lite_update_stroke(&fontpath));

        /* Draw stroke path with stroke color */
        fontpath.stroke_color = stroke_color;
    }

    memcpy(&font_matrix, &matrix, sizeof(matrix));
    vg_lite_translate(x, y, &font_matrix);
    vg_lite_scale(1 / scale, 1 / scale, &font_matrix);

    VGL_ERROR_CHECK(vg_lite_draw(fb, &fontpath, VG_LITE_FILL_EVEN_ODD, &font_matrix, VG_LITE_BLEND_SRC_OVER, color));
    VGL_ERROR_CHECK(vg_lite_clear_path(&fontpath));
    return 0;

VGLError:
    return 1;
}

static vg_lite_error_t renderText(vg_lite_buffer_t * fb, NSVGshape * shape, unsigned int color, FT_Face face)
{
    FT_GlyphSlot slot;
    FT_Outline outline;
    FT_Outline_Funcs callbacks;
    FT_Error error;
#if 0
    FT_Library library;
    FT_Face face;
#endif
    FT_BBox box = { 0 };

    float font_scale = 64, font_scale_ch = 32, defaul_size = 16;
    float font_size, pos_x, pos_y, advance;
    char* text_content;
    int text_length = 0, English_Flag = 0;
    unsigned int index;

    pos_x = shape->fill.text->x * scale;
    pos_y = shape->fill.text->y * scale;

    if (svg_scale_image_width > 0)
        font_size = shape->fill.text->fontSize * scale * (svg_scale_image_width / svg_image_width);
    else
        font_size = shape->fill.text->fontSize * scale;

    text_content = shape->fill.text->content;
    text_length = strlen(text_content);
    font_scale = font_size != 0 ? font_scale / font_size : font_scale / defaul_size;

#if 0
    error = FT_Init_FreeType(&library);
    if (error)
    {
        NANOSVG_PRINT("error in init freetype.\n");
        goto VGLError;
    }

    error = FT_New_Face(library, font_path, 0, &face);
    if (error == FT_Err_Unknown_File_Format)
    {
        NANOSVG_PRINT("error in file format.\n");
        goto VGLError;
    }
    else if (error)
    {
        NANOSVG_PRINT("error in creat face.\n");
        goto VGLError;
    }

    error = FT_Set_Pixel_Sizes(face, 1, 0);
    if (error)
    {
        NANOSVG_PRINT("error in set pixel size.\n");
        goto VGLError;
    }
#endif

    NSVGparser* p;
    p = nsvg__createParser();

    unsigned char character;
    int i = 0;
    int fontnums = 0;

    if (p == NULL)
    {
        NANOSVG_PRINT("nsvg__createParser failed.\n");
        return 0;
    }

    while (text_content[fontnums] != '\0')
    {
        fontnums++;
    }
    while(text_content[i] != '\0')
    {
        character = text_content[i];

        if((character & 0xE0) == 0xC0)
        {
            /* Two bytes UTF - 8 encoding. */
            unicode = ((character & 0x1F) << 6) | (text_content[i + 1] & 0x3F);
            i += 2;
        }
        else if ((character & 0xF0) == 0xE0)
        {
            /*Three bytes UTF-8 encoding. */
            unicode = ((character & 0x0F) << 12) | ((text_content[i+1] & 0x3F) << 6) | (text_content[i+2] & 0x3F);
            i += 3;
        }
        else if ((character & 0xF8) == 0xF0)
        {
            /* Four bytes UTF-8 encoding.*/
            unicode = ((character & 0x07) << 18) | ((text_content[i+1] & 0x3F) << 12) | ((text_content[i+2] & 0x3F) << 6) | (text_content[i+3] & 0x3F);
            i += 4;
        }
        else
        {   /* ASCII encoding.*/
            unicode = character;
            i++;
            English_Flag = 1;
        }

        index = FT_Get_Char_Index(face, unicode);
        error = FT_Load_Glyph(face, index, FT_LOAD_DEFAULT);
        if (error)
        {
            NANOSVG_PRINT("error in load glyph 0x%x.\n", unicode);
            //goto VGLError;
        }

        slot = face->glyph;

        if (English_Flag)
        {
            advance = slot->metrics.horiAdvance / font_scale / 2;
            if (shape->fill.text->textLength > 0)
            {
                advance = shape->fill.text->textLength * scale / (fontnums - 1);
            }
        }
        else
        {
            if (font_size <= 0)
            {
                font_scale = font_scale_ch / defaul_size; //adjust the font size of default chinese characters
                advance = slot->metrics.horiAdvance / font_scale / 1.3;
            }
            else
                advance = slot->metrics.horiAdvance / font_scale;
        }

        if (error == 0)
        {
            outline = slot->outline;
            callbacks.move_to = move_to;
            callbacks.line_to = line_to;
            callbacks.conic_to = conic_to;
            callbacks.cubic_to = cubic_to;
            callbacks.shift = 0;
            callbacks.delta = 0;

            error = FT_Outline_Decompose(&outline, &callbacks, 0);
            error = FT_Outline_Get_BBox(&outline, &box);

            if (text_pdata_cur == text_pdata_start)
            {
                pos_x = pos_x + advance;
                continue;
            }

            drawCharacter(fb, shape, font_size, pos_x, pos_y, color, font_scale, box.xMin, box.yMin, box.xMax, box.yMax);

            text_pdata_cur = text_pdata_start;
        }

        pos_x = pos_x + advance;

        nsvg__deletePaths(p->plist);
        p->plist = NULL; // For safe free.
    }

    nsvg__deleteParser(p);
    shape->paths = NULL; // For safe free.
#if 0
    FT_Done_Face(face);
    FT_Done_FreeType(library);
#endif
    return 0;
}
#endif /* CONFIG_FREETYPE_FONT */

static vg_lite_error_t renderImage(vg_lite_buffer_t* fb, NSVGshape* shape)
{
    if (!shape->fill.image->drawFlag)
    {
        NANOSVG_PRINT("Bypass an image because of its oversize.\n");
        return 0;
    }

    NSVGdefsImage* image = shape->fill.image;
    vg_lite_buffer_t src;
    vg_lite_matrix_t image_matrix;
    vg_lite_blend_t blend = OPENVG_BLEND_SRC_OVER;
    const char* s = image->imgdata;
    unsigned char* output;
    size_t len;
    int pos_x, pos_y;

    /* Decode image data. */
    if (nsvgBase64Decode(s, &output, &len) < 0) {
        NANOSVG_PRINT("nsvgBase64Decode failed\n");
        return 0;
    }

    /* Load decoded png data to vglite buffer. */
    //vg_lite_load_png_data(&src, output, len);
    spng_load_memory(&src, output, len);

    pos_x = image->x;
    pos_y = image->y;

    src.compress_mode = VG_LITE_DEC_DISABLE;
    src.image_mode = VG_LITE_ZERO;
    src.tiled = VG_LITE_LINEAR;

    memcpy(&image_matrix, &matrix, sizeof(matrix));

    VGL_ERROR_CHECK(vg_lite_translate(pos_x * scale, pos_y * scale, &image_matrix));
    VGL_ERROR_CHECK(vg_lite_scale(image->width * scale/src.width, image->height * scale/src.height, &image_matrix));
    VGL_ERROR_CHECK(vg_lite_blit(fb, &src, &image_matrix, blend, 0, 0));
    VGL_ERROR_CHECK(vg_lite_finish());

    //VGL_ERROR_CHECK(vg_lite_free(&src));
    spng_free_buffer(&src);
    NANOSVG_FREE(output);
    //NANOSVG_FREE(image);
    return 0;

VGLError:
    NANOSVG_FREE(output);
    //NANOSVG_FREE(image);
    return 1;
}

static vg_lite_error_t generateVGLitePath(NSVGshape* shape, vg_lite_path_t* vglpath, vg_lite_path_type_t path_type)
{
    float* d = &((float*)vglpath->path)[0];
    float* bufend = d + PATH_DATA_BUFFER_SIZE;

    for (NSVGpath* path = shape->paths; path != NULL; path = path->next)
    {
        float* s = &path->pts[0];

        /* Exit if remaining path_data_buf[] is not sufficient to hold the path data */
        if ((bufend - d) < (7 * path->npts + 10) / 3)
        {
            NANOSVG_PRINT("Error: Need to increase PATH_DATA_BUFFER_SIZE for path_data_buf[].\n");
            goto VGLError;
        }

        /* Create VGLite cubic bezier path data from path->pts[] */
        *((char*)d) = VLC_OP_MOVE; d++;
        *d++ = (*s++) * scale;
        *d++ = (*s++) * scale;
        for (int i = 0; i < path->npts - 1; i += 3)
        {
            *((char*)d) = VLC_OP_CUBIC; d++;
            *d++ = (*s++) * scale;
            *d++ = (*s++) * scale;
            *d++ = (*s++) * scale;
            *d++ = (*s++) * scale;
            *d++ = (*s++) * scale;
            *d++ = (*s++) * scale;
        }

        if (path->closed)
        {
            *((char*)d) = VLC_OP_CLOSE; d++;
        }
    }
    if (path_type != VG_LITE_DRAW_STROKE_PATH)
    {
        *((char*)d) = VLC_OP_END; d++;
    }

    /* Compute the accurate VGLite path data length */
    vglpath->path_length = ((char*)d - (char*)vglpath->path);

    return 0;

VGLError:
    return 1;
}

static int renderGradientPath(vg_lite_buffer_t* fb, NSVGpaint *paint, char *gradid, unsigned int color, float opacity,
                              float fill_opacity, float stroke_opacity, int is_fill, int is_stroke, char* patternid,
                              int shape_blend)
{
    vg_lite_blend_t blend_mode = VG_LITE_BLEND_NONE;
    if (is_fill)
    {
        if (opacity < 1 || fill_opacity < 1)
            blend_mode = VG_LITE_BLEND_SRC_OVER;
    }
    else if (is_stroke)
    {
        if (stroke_opacity < 1)
            blend_mode = VG_LITE_BLEND_SRC_OVER;
    }
    if (patternid[0] != '\0' && is_fill)
        blend_mode = VG_LITE_BLEND_SRC_OVER;

    if (gradid[0] != '\0')
    {
        int grad_blend = VG_LITE_BLEND_SRC_OVER;
        if (shape_blend == VG_MIXBLENDMODE_MULTIPLY)
        {
            grad_blend = VG_LITE_BLEND_MULTIPLY;
        }
        else if (shape_blend == VG_MIXBLENDMODE_SCREEN)
        {
            grad_blend = VG_LITE_BLEND_SCREEN;
        }

        if (paint->type == NSVG_PAINT_LINEAR_GRADIENT)
        {
            if (vg_lite_query_feature(gcFEATURE_BIT_VG_LINEAR_GRADIENT_EXT))
            {
                vg_lite_ext_linear_gradient_t grad;
                vg_lite_linear_gradient_parameter_t grad_param;
                vg_lite_matrix_t* grad_matrix;
                float dx, dy, X0, X1, Y0, Y1;

                memset(&grad, 0, sizeof(grad));

                for (int i = 0; i < paint->gradient->nstops; i++) {
                    setOpacity(&(paint->gradient->stops[i].color), opacity);
                    grad_ramps[i].stop = paint->gradient->stops[i].offset;
                    grad_ramps[i].alpha = ((paint->gradient->stops[i].color & 0xFF000000) >> 24) / 255.0f;
                    grad_ramps[i].blue = ((paint->gradient->stops[i].color & 0x00FF0000) >> 16) / 255.0f;
                    grad_ramps[i].green = ((paint->gradient->stops[i].color & 0x0000FF00) >> 8) / 255.0f;;
                    grad_ramps[i].red = (paint->gradient->stops[i].color & 0x000000FF) / 255.0f;;
                }

                X0 = paint->gradient->param[0];
                Y0 = paint->gradient->param[1];
                X1 = paint->gradient->param[2];
                Y1 = paint->gradient->param[3];
                dx = shape_bound[2] - shape_bound[0];
                dy = shape_bound[3] - shape_bound[1];
                if (paint->gradient->units == NSVG_OBJECT_SPACE)
                {
                    grad_param.X0 = shape_bound[0] + dx * X0;
                    grad_param.X1 = shape_bound[0] + dx * X1;
                    grad_param.Y0 = shape_bound[1] + dy * Y0;
                    grad_param.Y1 = shape_bound[1] + dy * Y1;
                }
                else
                {
                    grad_param.X0 = X0 * scale;
                    grad_param.X1 = X1 * scale;
                    grad_param.Y0 = Y0 * scale;
                    grad_param.Y1 = Y1 * scale;
                }

                VGL_ERROR_CHECK(vg_lite_set_linear_grad(&grad, paint->gradient->nstops, grad_ramps, grad_param, vglspread[(int)paint->gradient->spread], 0));

                grad_matrix = vg_lite_get_linear_grad_matrix(&grad);
                VGL_ERROR_CHECK(vg_lite_identity(grad_matrix));
                if (1 /*paint->gradient->xform != NULL*/)
                {
                    int j = 0;
                    for (int i = 0; i < 3; i++)
                    {
                        grad_matrix->m[j % 2][i] = paint->gradient->xform[j];
                        j++;
                        grad_matrix->m[j % 2][i] = paint->gradient->xform[j];
                        j++;
                    }
                }

                VGL_ERROR_CHECK(vg_lite_update_linear_grad(&grad));

                VGL_ERROR_CHECK(vg_lite_draw_linear_grad(fb, &vglpath, VG_LITE_FILL_EVEN_ODD, &matrix, &grad, color, grad_blend, VG_LITE_FILTER_POINT));

                VGL_ERROR_CHECK(vg_lite_finish());
                VGL_ERROR_CHECK(vg_lite_clear_linear_grad(&grad));
            }
            else
            {
                vg_lite_linear_gradient_t grad;
                vg_lite_matrix_t* grad_matrix;
                unsigned int alpha, blue, green, red;
                int x1, x2;
                float stops, X0, X1, Y0, Y1, angle;

                memset(&grad, 0, sizeof(grad));

                X0 = paint->gradient->param[0];
                Y0 = paint->gradient->param[1];
                X1 = paint->gradient->param[2];
                Y1 = paint->gradient->param[3];

                if ((X0 == 0 && X1 == 0) && ((Y0 != 0) || (Y1 != 0))) {
                    angle = 90;
                    X0 = Y0;
                    X1 = Y1;
                }
                else {
                    angle = 0;
                }

                if (paint->gradient->units == NSVG_OBJECT_SPACE)
                {
                    x1 = shape_bound[0] + (shape_bound[2] - shape_bound[0]) * X0;
                    x2 = shape_bound[0] + (shape_bound[2] - shape_bound[0]) * X1;
                }
                else
                {
                    x1 = shape_bound[0] * scale;
                    x2 = shape_bound[2] * scale;
                }

                if (x1 > x2)
                    swap(x1, x2);

                for (int i = 0; i < paint->gradient->nstops; i++) {
                    setOpacity(&(paint->gradient->stops[i].color), opacity);

                    alpha = (paint->gradient->stops[i].color & 0xFF000000) >> 24;
                    blue = (paint->gradient->stops[i].color & 0x00FF0000) >> 16;
                    green = (paint->gradient->stops[i].color & 0x0000FF00) >> 8;
                    red = (paint->gradient->stops[i].color & 0x000000FF) >> 0;
                    grad_colors[i] = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);

                    stops = x1 + paint->gradient->stops[i].offset * (x2 - x1);
                    grad_stops[i] = stops > 0 ? stops : 0;
                }

                VGL_ERROR_CHECK(vg_lite_init_grad(&grad));
                VGL_ERROR_CHECK(vg_lite_set_grad(&grad, paint->gradient->nstops, grad_colors, grad_stops));
                VGL_ERROR_CHECK(vg_lite_update_grad(&grad));

                grad_matrix = vg_lite_get_grad_matrix(&grad);
                VGL_ERROR_CHECK(vg_lite_identity(grad_matrix));
                if (paint->gradient->units == NSVG_OBJECT_SPACE)
                    VGL_ERROR_CHECK(vg_lite_rotate(angle, grad_matrix));

                VGL_ERROR_CHECK(vg_lite_draw_grad(fb, &vglpath, VG_LITE_FILL_EVEN_ODD, &matrix, &grad, grad_blend));

                VGL_ERROR_CHECK(vg_lite_finish());
                VGL_ERROR_CHECK(vg_lite_clear_grad(&grad));
            }
        }
        else if (paint->type == NSVG_PAINT_RADIAL_GRADIENT)
        {
            if (vg_lite_query_feature(gcFEATURE_BIT_VG_RADIAL_GRADIENT)) {
                vg_lite_radial_gradient_t grad;
                vg_lite_radial_gradient_parameter_t grad_param;
                vg_lite_matrix_t* grad_matrix;
                float cx, cy, r, fx, fy;
                float dx, dy;

                memset(&grad, 0, sizeof(grad));

                for (int i = 0; i < paint->gradient->nstops; i++) {
                    setOpacity(&(paint->gradient->stops[i].color), opacity);
                    grad_ramps[i].stop = paint->gradient->stops[i].offset;
                    grad_ramps[i].alpha = ((paint->gradient->stops[i].color & 0xFF000000) >> 24) / 255.0f;
                    grad_ramps[i].blue = ((paint->gradient->stops[i].color & 0x00FF0000) >> 16) / 255.0f;
                    grad_ramps[i].green = ((paint->gradient->stops[i].color & 0x0000FF00) >> 8) / 255.0f;;
                    grad_ramps[i].red = (paint->gradient->stops[i].color & 0x000000FF) / 255.0f;;
                }

                cx = paint->gradient->param[0];
                cy = paint->gradient->param[1];
                r = paint->gradient->param[2];
                fx = paint->gradient->param[3];
                fy = paint->gradient->param[4];
                dx = shape_bound[2] - shape_bound[0];
                dy = shape_bound[3] - shape_bound[1];
                if (paint->gradient->units == NSVG_OBJECT_SPACE)
                {
                    grad_param.cx = shape_bound[0] + cx * dx;
                    grad_param.cy = shape_bound[1] + cy * dy;
                    grad_param.r = r * (dx > dy ? dx : dy);
                    grad_param.fx = (fx == 0.0f) ? grad_param.cx : (shape_bound[0] + fx * dx);
                    grad_param.fy = (fy == 0.0f) ? grad_param.cy : (shape_bound[1] + fy * dy);
                }
                else
                {
                    grad_param.cx = cx * scale;
                    grad_param.cy = cy * scale;
                    grad_param.r = r * scale;
                    grad_param.fx = (fx == 0.0f) ? grad_param.cx : (fx * scale);
                    grad_param.fy = (fy == 0.0f) ? grad_param.cy : (fy * scale);
                }

                VGL_ERROR_CHECK(vg_lite_set_radial_grad(&grad, paint->gradient->nstops, grad_ramps, grad_param, vglspread[(int)paint->gradient->spread], 0));
                VGL_ERROR_CHECK(vg_lite_update_radial_grad(&grad));

                grad_matrix = vg_lite_get_radial_grad_matrix(&grad);
                VGL_ERROR_CHECK(vg_lite_identity(grad_matrix));
                if (1 /*paint->gradient->xform != NULL*/)
                {
                    int j = 0;
                    for (int i = 0; i < 3; i++)
                    {
                        grad_matrix->m[j % 2][i] = paint->gradient->xform[j];
                        j++;
                        grad_matrix->m[j % 2][i] = paint->gradient->xform[j];
                        j++;
                    }
                }
                VGL_ERROR_CHECK(vg_lite_draw_radial_grad(fb, &vglpath, VG_LITE_FILL_EVEN_ODD, &matrix, &grad, color, grad_blend, VG_LITE_FILTER_POINT));

                VGL_ERROR_CHECK(vg_lite_finish());
                VGL_ERROR_CHECK(vg_lite_clear_radial_grad(&grad));
            }
            else
            {
                NANOSVG_PRINT("Radial gradient is not supported!\n");
                vg_lite_finish();
                return -1;
            }
        }
    }
    else
    {
        /* Render the shape with VGLite cubic bezier path data */
        VGL_ERROR_CHECK(vg_lite_draw(fb, &vglpath, fill_rule, &matrix, blend_mode, color));
    }

    return 0;

VGLError:
    return 1;
}

static int renderPatternPath(vg_lite_buffer_t *fb, NSVGpaint* paint, char* patternid)
{
    if (patternid[0] != '\0')
    {
        /* Render path to source buffer */
        vg_lite_path_t pattern_path;
        vg_lite_float_t pattern_shape_bound[4];
        NSVGshape* pattern_shape = paint->pattern->shape;
        vg_lite_path_type_t path_type;
        vg_lite_matrix_t matrix;
        vg_lite_matrix_t vglmatrix;
        vg_lite_matrix_t draw_pattern_matrix;
        vg_lite_buffer_t pattern_buf;

        memset(&pattern_buf, 0, sizeof(vg_lite_buffer_t));

        VGL_ERROR_CHECK(vg_lite_identity(&matrix));
        VGL_ERROR_CHECK(vg_lite_identity(&draw_pattern_matrix));
        VGL_ERROR_CHECK(vg_lite_identity(&vglmatrix));

        if (paint->pattern->units == NSVG_OBJECT_SPACE)
        {
            pattern_buf.width = (shape_bound[2] - shape_bound[0]) * paint->pattern->width * scale;
            pattern_buf.height = (shape_bound[3] - shape_bound[1]) * paint->pattern->height * scale;
        }
        else
        {
            pattern_buf.width = paint->pattern->width * scale;
            pattern_buf.height = paint->pattern->height * scale;
        }

        pattern_buf.format = VG_LITE_RGBA8888;
        VGL_ERROR_CHECK(vg_lite_allocate(&pattern_buf));
        VGL_ERROR_CHECK(vg_lite_clear(&pattern_buf, NULL, 0xFFFFFFFF));

        fill_rule = VG_LITE_FILL_NON_ZERO - pattern_shape->fillRule;

        for (; pattern_shape; pattern_shape = pattern_shape->next)
        {
            memset(&pattern_path, 0, sizeof(vg_lite_path_t));
            pattern_shape_bound[0] = pattern_shape->bounds[0] * scale;
            pattern_shape_bound[1] = pattern_shape->bounds[1] * scale;
            pattern_shape_bound[2] = pattern_shape->bounds[2] * scale;
            pattern_shape_bound[3] = pattern_shape->bounds[3] * scale;

            VGL_ERROR_CHECK(vg_lite_init_path(&pattern_path, VG_LITE_FP32, VG_LITE_MEDIUM, 0, &pattern_data_buf[0],
                pattern_shape_bound[0], pattern_shape_bound[1], pattern_shape_bound[2], pattern_shape_bound[3]));

            path_type = 0;
            if (pattern_shape->fill.type != NSVG_PAINT_NONE)
                path_type |= VG_LITE_DRAW_FILL_PATH;
            if (pattern_shape->stroke.type != NSVG_PAINT_NONE)
                path_type |= VG_LITE_DRAW_STROKE_PATH;


            VGL_ERROR_CHECK(generateVGLitePath(pattern_shape, &pattern_path, path_type));

            if (pattern_shape->stroke.type != NSVG_PAINT_NONE)
            {
                vg_lite_cap_style_t linecap = VG_LITE_CAP_BUTT + pattern_shape->strokeLineCap;
                vg_lite_join_style_t joinstyle = VG_LITE_JOIN_MITER + pattern_shape->strokeLineJoin;
                vg_lite_float_t strokewidth = pattern_shape->strokeWidth * scale;
                vg_lite_color_t stroke_color = pattern_shape->stroke.color;

                pattern_path.path_type = VG_LITE_DRAW_STROKE_PATH;
                if (pattern_shape->opacity < 1 || pattern_shape->fill_opacity == 0)
                {
                    vg_lite_color_t color = stroke_color;
                    uint8_t a = (uint8_t)(((color & 0xFF000000) >> 24) * pattern_shape->opacity);
                    stroke_color = (color & 0x00ffffff) | ((a << 24) & 0xff000000);
                }
                else
                {
                    setOpacity(&stroke_color, pattern_shape->opacity);
                }

                VGL_ERROR_CHECK(vg_lite_set_stroke(&pattern_path, linecap, joinstyle, strokewidth, pattern_shape->miterLimit,
                    pattern_shape->strokeDashArray, pattern_shape->strokeDashCount, pattern_shape->strokeDashOffset, 0xFFFFFFFF));
                VGL_ERROR_CHECK(vg_lite_update_stroke(&pattern_path));

                pattern_path.stroke_color = stroke_color;
            }

            VGL_ERROR_CHECK(vg_lite_draw(&pattern_buf, &pattern_path, fill_rule, &draw_pattern_matrix, VG_LITE_BLEND_NONE, pattern_shape->fill.color));

        }

        if (vg_lite_query_feature(gcFEATURE_BIT_VG_IM_REPEAT_REFLECT))
        {
            /* Render source buffer to the specified path with repaet/reflect support */
            if (paint->pattern->units == NSVG_OBJECT_SPACE)
            {
                VGL_ERROR_CHECK(vg_lite_translate((shape_bound[2] - shape_bound[0]) * paint->pattern->x + shape_bound[0], (shape_bound[3] - shape_bound[1]) * paint->pattern->y + shape_bound[1], &matrix));
            }
            else
            {
                VGL_ERROR_CHECK(vg_lite_translate(paint->pattern->x, paint->pattern->y, &matrix));
            }

            VGL_ERROR_CHECK(vg_lite_draw_pattern(fb, &vglpath, VG_LITE_FILL_EVEN_ODD, &vglmatrix, &pattern_buf, &matrix, VG_LITE_BLEND_NONE, VG_LITE_PATTERN_REPEAT, 0, 0, VG_LITE_FILTER_POINT));
            VGL_ERROR_CHECK(vg_lite_finish());
        }
        else
        {
            /* Render source buffer to the specified path with workaround */
            vg_lite_buffer_t temp_buf;
            vg_lite_matrix_t temp_mat;

            memset(&temp_buf, 0, sizeof(vg_lite_buffer_t));
            temp_buf.width = shape_bound[2] - shape_bound[0];
            temp_buf.height = shape_bound[3] - shape_bound[1];
            temp_buf.format = VG_LITE_RGBA8888;

            VGL_ERROR_CHECK(vg_lite_allocate(&temp_buf));
            VGL_ERROR_CHECK(vg_lite_clear(&temp_buf, NULL, 0x00000000));

            VGL_ERROR_CHECK(vg_lite_identity(&temp_mat));
            VGL_ERROR_CHECK(vg_lite_translate(-shape_bound[0], -shape_bound[1], &temp_mat));
            VGL_ERROR_CHECK(vg_lite_draw(&temp_buf, &vglpath, VG_LITE_FILL_EVEN_ODD, &temp_mat, VG_LITE_BLEND_SRC_OVER, 0xFFFFFFFF));

            int start_x = 0;
            int start_y = 0;
            int offset_x = 0;
            int offset_y = 0;

            if (paint->pattern->units == NSVG_OBJECT_SPACE) {
                start_x = paint->pattern->x > 1 ? 0 : (shape_bound[2] - shape_bound[0]) * paint->pattern->x * scale;
                start_y = paint->pattern->y > 1 ? 0 : (shape_bound[3] - shape_bound[1]) * paint->pattern->y * scale;
                while (start_x > 0)
                    start_x -= temp_buf.width;
                while (start_y > 0)
                    start_y -= temp_buf.height;
            }
            else
            {
                start_x = paint->pattern->x;
                start_y = paint->pattern->y;
                while (start_x > shape_bound[0])
                    start_x -= temp_buf.width;
                while (start_y > shape_bound[1])
                    start_y -= temp_buf.height;
                offset_x = shape_bound[0];
                offset_y = shape_bound[1];
            }

            for (; start_x < offset_x + temp_buf.width; start_x += pattern_buf.width)
            {
                int tem_y = start_y;
                for (; start_y < offset_y + temp_buf.height; start_y += pattern_buf.height)
                {
                    VGL_ERROR_CHECK(vg_lite_identity(&temp_mat));
                    VGL_ERROR_CHECK(vg_lite_translate(-offset_x, -offset_y, &temp_mat));
                    VGL_ERROR_CHECK(vg_lite_translate(start_x, start_y, &temp_mat));
                    VGL_ERROR_CHECK(vg_lite_blit(&temp_buf, &pattern_buf, &temp_mat, VG_LITE_BLEND_SRC_IN, 0, 0));
                }
                start_y = tem_y;
            }

            VGL_ERROR_CHECK(vg_lite_identity(&temp_mat));
            VGL_ERROR_CHECK(vg_lite_translate(shape_bound[0], shape_bound[1], &temp_mat));
            VGL_ERROR_CHECK(vg_lite_blit(fb, &temp_buf, &temp_mat, VG_LITE_BLEND_SRC_OVER, 0, 0));
            VGL_ERROR_CHECK(vg_lite_finish());

            VGL_ERROR_CHECK(vg_lite_free(&temp_buf));
        }

        VGL_ERROR_CHECK(vg_lite_free(&pattern_buf));
        return 0;

    VGLError:
        vg_lite_free(&pattern_buf);
        return 1;
    }

    return 0;
}

int renderSVGImage(NSVGimage *image, vg_lite_buffer_t *fb, vg_lite_matrix_t *extra_matrix, void *ft_face)
{
    NSVGshape *shape = NULL;
    vg_lite_path_type_t path_type;

    memset(&vglpath, 0, sizeof(vg_lite_path_t));

    /* Clear the frame buffer to white color */
    //VGL_ERROR_CHECK(vg_lite_clear(fb, NULL, 0xFFFFFFFF));

    if (extra_matrix) {
        memcpy(&matrix, extra_matrix, sizeof(matrix));
    }
    else {
        VGL_ERROR_CHECK(vg_lite_identity(&matrix));
    }

    /* Loop through all shape structures in NSVGimage to render all primitves */
    for (shape = image->shapes; shape != NULL; shape = shape->next)
    {
        if (!(shape->flags & NSVG_FLAGS_VISIBLE))
            continue;

        if (shape->mediaFlag)
        {
            if ((shape->maxWidth > 0 && shape->maxWidth < matrix.m[0][0] * fb->width) || (shape->maxHeight > 0 && shape->maxHeight < matrix.m[1][1] * fb->height))
                continue;
            if (shape->minWidth > matrix.m[0][0] * fb->width || shape->minHeight > matrix.m[1][1] * fb->height)
                continue;
        }

#if USE_BOUNDARY_JUDGMENT
        if (shape->fill.type == NSVG_PAINT_TEXT)
        {
            float font_x = matrix.m[0][0] * shape->fill.text->x * scale + matrix.m[0][1] * shape->fill.text->y * scale + matrix.m[0][2];
            float font_y = matrix.m[1][0] * shape->fill.text->x * scale + matrix.m[1][1] * shape->fill.text->y * scale + matrix.m[1][2];

            if (font_x <= 0 || font_y <= 0 || font_x >= fb->width || font_y >= fb->height)
                continue;
        }
        else if (shape->fill.type == NSVG_PAINT_IMAGE)
        {
            float image_x = matrix.m[0][0] * shape->fill.image->x * scale + matrix.m[0][1] * shape->fill.image->y * scale + matrix.m[0][2];
            float image_y = matrix.m[1][0] * shape->fill.image->x * scale + matrix.m[1][1] * shape->fill.image->y * scale + matrix.m[1][2];

            if (image_x <= 0 || image_y <= 0 || image_x >= fb->width || image_y >= fb->height)
                continue;
        }
        else if (shape->fill.type != NSVG_PAINT_NONE || shape->stroke.type != NSVG_PAINT_NONE)
        {
            float path_minx = matrix.m[0][0] * shape->paths->bounds[0] + matrix.m[0][1] * shape->paths->bounds[1] + matrix.m[0][2];
            float path_miny = matrix.m[1][0] * shape->paths->bounds[0] + matrix.m[1][1] * shape->paths->bounds[1] + matrix.m[1][2];
            float path_maxx = matrix.m[0][0] * shape->paths->bounds[2] + matrix.m[0][1] * shape->paths->bounds[3] + matrix.m[0][2];
            float path_maxy = matrix.m[1][0] * shape->paths->bounds[2] + matrix.m[1][1] * shape->paths->bounds[2] + matrix.m[1][2];

            if (path_maxx <= 0 || path_maxy <= 0 || path_minx >= fb->width || path_miny >= fb->height)
                continue;
        }
#endif

        shape_bound[0] = shape->bounds[0] * scale;
        shape_bound[1] = shape->bounds[1] * scale;
        shape_bound[2] = shape->bounds[2] * scale;
        shape_bound[3] = shape->bounds[3] * scale;

        VGL_ERROR_CHECK(vg_lite_init_path(&vglpath, VG_LITE_FP32, VG_LITE_MEDIUM, 0, &path_data_buf[0],
            shape_bound[0], shape_bound[1], shape_bound[2], shape_bound[3]));

        path_type = 0;
        if (shape->fill.type != NSVG_PAINT_NONE) {
            path_type |= VG_LITE_DRAW_FILL_PATH;
        }
        if (shape->stroke.type != NSVG_PAINT_NONE) {
            path_type |= VG_LITE_DRAW_STROKE_PATH;
        }

        /* Set VGLite fill rule */
        fill_rule = VG_LITE_FILL_NON_ZERO - shape->fillRule;

        /* Generate VGLite cubic bezier path data from current shape */
        VGL_ERROR_CHECK(generateVGLitePath(shape, &vglpath, path_type));

        /* Render VGLite fill path with linear/radial gradient support */
        if (shape->fill.type != NSVG_PAINT_NONE)
        {
            int is_fill = 1;
            int is_stroke = 0;
            /* Render the fill path only */
            vglpath.path_type = VG_LITE_DRAW_FILL_PATH;

            vg_lite_color_t fill_color = shape->fill.color;
            setOpacity(&fill_color, shape->opacity);

            if (shape->fill.type == NSVG_PAINT_TEXT) {
#ifdef CONFIG_FREETYPE_FONT
                if (ft_face) {
                    VGL_ERROR_CHECK(renderText(fb, shape, fill_color, ft_face));
                } else {
                    NANOSVG_PRINT("FT face is null\n");
                }
#else
                NANOSVG_PRINT("Text is not supported!\n");
#endif
            }
            else if (shape->fill.type == NSVG_PAINT_IMAGE) {
                VGL_ERROR_CHECK(renderImage(fb, shape));
            }
            else {
                VGL_ERROR_CHECK(renderPatternPath(fb, &shape->fill, shape->fillPattern));

                VGL_ERROR_CHECK(renderGradientPath(fb, &shape->fill, shape->fillGradient, fill_color, shape->opacity,
                                shape->fill_opacity, shape->stroke_opacity, is_fill, is_stroke, shape->fillPattern,
                                shape->blendMode));
            }
        }

        /* Render VGLite stroke path with linear/radial gradient support */
        if (shape->stroke.type != NSVG_PAINT_NONE)
        {
            int is_fill = 0;
            int is_stroke = 1;
            vg_lite_cap_style_t linecap = VG_LITE_CAP_BUTT + shape->strokeLineCap;
            vg_lite_join_style_t joinstyle = VG_LITE_JOIN_MITER + shape->strokeLineJoin;
            vg_lite_float_t strokewidth = shape->strokeWidth * scale;
            vg_lite_color_t stroke_color = shape->stroke.color;

            /* Render the stroke path only */
            vglpath.path_type = VG_LITE_DRAW_STROKE_PATH;
            if (shape->opacity < 1 || shape->fill_opacity == 0)
            {
                vg_lite_color_t color = stroke_color;
                uint8_t a = (uint8_t)(((color & 0xFF000000) >> 24) * shape->opacity);
                stroke_color = (color&0x00ffffff) | ((a << 24) & 0xff000000);
            }
            else {
                setOpacity(&stroke_color, shape->opacity);
            }

            /* Setup stroke parameters properly */
            VGL_ERROR_CHECK(vg_lite_set_stroke(&vglpath, linecap, joinstyle, strokewidth, shape->miterLimit,
                shape->strokeDashArray, shape->strokeDashCount, shape->strokeDashOffset, 0xFFFFFFFF));
            VGL_ERROR_CHECK(vg_lite_update_stroke(&vglpath));

            /* Draw stroke path with stroke color */
            vglpath.stroke_color = stroke_color;
            VGL_ERROR_CHECK(renderGradientPath(fb, &shape->stroke, shape->strokeGradient, stroke_color, shape->opacity,
                            shape->fill_opacity, shape->stroke_opacity, is_fill, is_stroke, shape->fillPattern,
                            shape->blendMode));
        }

        /* Clear vglpath. */
        VGL_ERROR_CHECK(vg_lite_clear_path(&vglpath));

#if VG_STABLE_MODE
        if (++commit_count >= MAX_COMMITTED_PATHS) {
            VGL_ERROR_CHECK(vg_lite_flush());
            commit_count = 0;
        }
#endif
    }

#if VG_STABLE_MODE
    commit_count = 0;
#endif

    VGL_ERROR_CHECK(vg_lite_frame_delimiter(VG_LITE_FRAME_END_FLAG));

    return 0;

VGLError:
    NANOSVG_PRINT("render SVG error!\n");
    vg_lite_clear_path(&vglpath);

    return 1;
}

NSVGimage* parseSVGImage(char *input, int *pwidth, int *pheight)
{
    NSVGimage *image = nsvgParse(input, "px", 96.0f);

    if (image) {
        *pwidth = (int)(image->width * scale);
        *pheight = (int)(image->height * scale);
    }

    return image;
}

NSVGimage* parseSVGImageFromFile(const char *filename, int *pwidth, int *pheight)
{
#if 0
    NSVGimage *image = nsvgParseFromFile(filename, "px", 96.0f);

    if (image) {
        *pwidth = (int)(image->width * scale);
        *pheight = (int)(image->height * scale);
    }

    return image;
#else
    return NULL;
#endif
}

void deleteSVGImage(NSVGimage *image)
{
    nsvgDelete(image);
}
