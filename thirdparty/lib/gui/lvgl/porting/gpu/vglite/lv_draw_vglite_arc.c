/**
 * @file lv_draw_vglite_arc.c
 *
 */

/**
 * MIT License
 *
 * Copyright 2021-2023 NXP
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next paragraph)
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_vglite_arc.h"

#if LV_USE_GPU_ACTS_VG_LITE
#include "lv_vglite_buf.h"
#include <math.h>

/*********************
 *      DEFINES
 *********************/

#define T_FRACTION 16384.0f

#define DICHOTO_ITER 5

static const uint16_t TperDegree[90] = {
    0, 174, 348, 522, 697, 873, 1049, 1226, 1403, 1581,
    1759, 1938, 2117, 2297, 2477, 2658, 2839, 3020, 3202, 3384,
    3567, 3749, 3933, 4116, 4300, 4484, 4668, 4852, 5037, 5222,
    5407, 5592, 5777, 5962, 6148, 6334, 6519, 6705, 6891, 7077,
    7264, 7450, 7636, 7822, 8008, 8193, 8378, 8564, 8750, 8936,
    9122, 9309, 9495, 9681, 9867, 10052, 10238, 10424, 10609, 10794,
    10979, 11164, 11349, 11534, 11718, 11902, 12086, 12270, 12453, 12637,
    12819, 13002, 13184, 13366, 13547, 13728, 13909, 14089, 14269, 14448,
    14627, 14805, 14983, 15160, 15337, 15513, 15689, 15864, 16038, 16212
};

/**********************
 *      TYPEDEFS
 **********************/

/* intermediate arc params */
typedef struct _vg_arc {
    int32_t angle;   /* angle <90deg */
    int32_t quarter; /* 0-3 counter-clockwise */
    float rad;     /* radius */
    float p0x;       /* point P0 */
    float p0y;
    float p1x;      /* point P1 */
    float p1y;
    float p2x;      /* point P2 */
    float p2y;
    float p3x;      /* point P3 */
    float p3y;
} vg_arc;

typedef struct _cubic_cont_pt {
    float p0;
    float p1;
    float p2;
    float p3;
} cubic_cont_pt;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void rotate_point(int32_t angle, float * x, float * y);
static void add_arc_path(float * arc_path, int * pidx, float radius,
                         int32_t start_angle, int32_t end_angle,
                         float cx, float cy, bool cw);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_res_t lv_gpu_vglite_draw_arc(const lv_point_t * center, int32_t radius, int32_t start_angle, int32_t end_angle,
                                    const lv_area_t * clip_area, const lv_draw_arc_dsc_t * dsc)
{
    vg_lite_error_t err = VG_LITE_SUCCESS;
    lv_color32_t col32 = {.full = lv_color_to32(dsc->color)}; /*Convert color to RGBA8888*/
    vg_lite_path_t path;
    vg_lite_color_t vgcol; /* vglite takes ABGR */
    bool donut = ((end_angle - start_angle) % 360 == 0) ? true : false;
    vg_lite_buffer_t * vgbuf = lv_vglite_get_dest_buf();
    vg_lite_buffer_t * srcbuf = NULL;

    /* Set src_vgbuf structure. */
    if (dsc->img_src) {
        if (lv_img_src_get_type(dsc->img_src) != LV_IMG_SRC_VARIABLE)
            return LV_RES_INV;

        const lv_img_dsc_t * img_dsc = dsc->img_src;
        lv_area_t src_area = { 0, 0, img_dsc->header.w - 1, img_dsc->header.h - 1 };
        if (LV_RES_OK != lv_vglite_set_src_buf((void *)img_dsc->data, img_dsc->header.cf, &src_area, img_dsc->header.w))
            return LV_RES_INV;

        srcbuf = lv_vglite_get_src_buf();
    }

    /* path: max size = 16 cubic bezier (7 words each) */
    float arc_path[16 * 7];
    lv_memset_00(arc_path, sizeof(arc_path));

    /*** Init path ***/
    lv_coord_t width = dsc->width;  /* inner arc radius = outer arc radius - width */
    if(width > (lv_coord_t)radius)
        width = radius;

    int pidx = 0;
    float cp_x, cp_y; /* control point coords */

    /* first control point of curve */
    cp_x = radius;
    cp_y = 0;
    rotate_point(start_angle, &cp_x, &cp_y);
    ((int32_t *)arc_path)[pidx++] = VLC_OP_MOVE;
    arc_path[pidx++] = center->x + cp_x;
    arc_path[pidx++] = center->y + cp_y;

    /* draw 1-5 outer quarters */
    add_arc_path(arc_path, &pidx, radius, start_angle, end_angle, center->x, center->y, true);

    if(donut) {
        /* close outer circle */
        cp_x = radius;
        cp_y = 0;
        rotate_point(start_angle, &cp_x, &cp_y);
        ((int32_t *)arc_path)[pidx++] = VLC_OP_LINE;
        arc_path[pidx++] = center->x + cp_x;
        arc_path[pidx++] = center->y + cp_y;
        /* start inner circle */
        cp_x = radius - width;
        cp_y = 0;
        rotate_point(start_angle, &cp_x, &cp_y);
        ((int32_t *)arc_path)[pidx++] = VLC_OP_MOVE;
        arc_path[pidx++] = center->x + cp_x;
        arc_path[pidx++] = center->y + cp_y;
    }
    else if(dsc->rounded != 0U) {    /* 1st rounded arc ending */
        cp_x = radius - width / 2.0f;
        cp_y = 0;
        rotate_point(end_angle, &cp_x, &cp_y);
        add_arc_path(arc_path, &pidx, width / 2.0f, end_angle, (end_angle + 180),
                     center->x + cp_x, center->y + cp_y, true);
    }
    else {   /* 1st flat ending */
        cp_x = radius - width;
        cp_y = 0;
        rotate_point(end_angle, &cp_x, &cp_y);
        ((int32_t *)arc_path)[pidx++] = VLC_OP_LINE;
        arc_path[pidx++] = center->x + cp_x;
        arc_path[pidx++] = center->y + cp_y;
    }

    /* draw 1-5 inner quarters */
    add_arc_path(arc_path, &pidx, radius - width, start_angle, end_angle, center->x, center->y, false);

    /* last control point of curve */
    if(donut) {     /* close the loop */
        cp_x = radius - width;
        cp_y = 0;
        rotate_point(start_angle, &cp_x, &cp_y);
        ((int32_t *)arc_path)[pidx++] = VLC_OP_LINE;
        arc_path[pidx++] = center->x + cp_x;
        arc_path[pidx++] = center->y + cp_y;
    }
    else if(dsc->rounded != 0U) {    /* 2nd rounded arc ending */
        cp_x = radius - width / 2.0f;
        cp_y = 0;
        rotate_point(start_angle, &cp_x, &cp_y);
        add_arc_path(arc_path, &pidx, width / 2.0f, (start_angle + 180), (start_angle + 360),
                     center->x + cp_x, center->y + cp_y, true);
    }
    else {   /* 2nd flat ending */
        cp_x = radius;
        cp_y = 0;
        rotate_point(start_angle, &cp_x, &cp_y);
        ((int32_t *)arc_path)[pidx++] = VLC_OP_LINE;
        arc_path[pidx++] = center->x + cp_x;
        arc_path[pidx++] = center->y + cp_y;
    }

    ((int32_t *)arc_path)[pidx++] = VLC_OP_END;

    err = vg_lite_init_path(&path, VG_LITE_FP32, VG_LITE_HIGH, (uint32_t)pidx * sizeof(int32_t), arc_path,
                            (vg_lite_float_t)clip_area->x1, (vg_lite_float_t)clip_area->y1,
                            ((vg_lite_float_t)clip_area->x2) + 1.0f, ((vg_lite_float_t)clip_area->y2) + 1.0f);
    VG_LITE_ERR_RETURN_INV(err, "Init path failed.");

    vg_lite_buffer_format_t color_format = VG_LITE_RGBA8888;
    if(lv_vglite_premult_and_swizzle(&vgcol, col32, dsc->opa, color_format) != LV_RES_OK)
        VG_LITE_RETURN_INV("Premultiplication and swizzle failed.");

    vg_lite_matrix_t matrix;
    vg_lite_identity(&matrix);

    /* Choose vglite blend mode based on given lvgl blend mode */
    vg_lite_blend_t vglite_blend_mode = VG_LITE_BLEND_NONE;
    if(dsc->opa < (lv_opa_t)LV_OPA_MAX || dsc->blend_mode != LV_BLEND_MODE_NORMAL)
        vglite_blend_mode = lv_vglite_get_blend_mode(dsc->blend_mode);

    lv_vglite_set_scissor(clip_area);

    /*** Draw arc ***/
    if (srcbuf == NULL) {
        err = vg_lite_draw(vgbuf, &path, VG_LITE_FILL_NON_ZERO, &matrix, vglite_blend_mode, vgcol);
    }
    else {
        vg_lite_matrix_t pat_matrix;
        vg_lite_identity(&pat_matrix);
        vg_lite_translate(center->x - radius, center->y - radius, &pat_matrix);
        err = vg_lite_draw_pattern(vgbuf, &path, VG_LITE_FILL_NON_ZERO, &matrix, srcbuf,
                &pat_matrix, vglite_blend_mode, VG_LITE_PATTERN_COLOR, vgcol, 0, VG_LITE_FILTER_BI_LINEAR);
    }

    VG_LITE_ERR_RETURN_INV(err, "Draw arc failed.");

    if(lv_vglite_run(clip_area) != LV_RES_OK)
        VG_LITE_RETURN_INV("Run failed.");

    lv_vglite_disable_scissor();

    err = vg_lite_clear_path(&path);
    VG_LITE_ERR_RETURN_INV(err, "Clear path failed.");

    return LV_RES_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void copy_arc(vg_arc * dst, vg_arc * src)
{
    dst->quarter = src->quarter;
    dst->rad = src->rad;
    dst->angle = src->angle;
    dst->p0x = src->p0x;
    dst->p1x = src->p1x;
    dst->p2x = src->p2x;
    dst->p3x = src->p3x;
    dst->p0y = src->p0y;
    dst->p1y = src->p1y;
    dst->p2y = src->p2y;
    dst->p3y = src->p3y;
}

/**
 * Rotate the point according given rotation angle rotation center is 0,0
 */
static void rotate_point(int32_t angle, float * x, float * y)
{
    float ori_x = *x;
    float ori_y = *y;
    int16_t alpha = (int16_t)angle;
    *x = (lv_trigo_cos(alpha) * ori_x - lv_trigo_sin(alpha) * ori_y) / LV_TRIGO_SIN_MAX;
    *y = (lv_trigo_sin(alpha) * ori_x + lv_trigo_cos(alpha) * ori_y) / LV_TRIGO_SIN_MAX;
}

/**
 * Set full arc control points depending on quarter.
 * Control points match the best approximation of a circle.
 * Arc Quarter position is:
 * Q2 | Q3
 * ---+---
 * Q1 | Q0
 */
static void set_full_arc(vg_arc * fullarc)
{
    /* the tangent lenght for the bezier circle approx */
    float tang = fullarc->rad * BEZIER_OPTIM_CIRCLE;
    switch(fullarc->quarter) {
        case 0:
            /* first quarter */
            fullarc->p0x = fullarc->rad;
            fullarc->p0y = 0;
            fullarc->p1x = fullarc->rad;
            fullarc->p1y = tang;
            fullarc->p2x = tang;
            fullarc->p2y = fullarc->rad;
            fullarc->p3x = 0;
            fullarc->p3y = fullarc->rad;
            break;
        case 1:
            /* second quarter */
            fullarc->p0x = 0;
            fullarc->p0y = fullarc->rad;
            fullarc->p1x = 0 - tang;
            fullarc->p1y = fullarc->rad;
            fullarc->p2x = 0 - fullarc->rad;
            fullarc->p2y = tang;
            fullarc->p3x = 0 - fullarc->rad;
            fullarc->p3y = 0;
            break;
        case 2:
            /* third quarter */
            fullarc->p0x = 0 - fullarc->rad;
            fullarc->p0y = 0;
            fullarc->p1x = 0 - fullarc->rad;
            fullarc->p1y = 0 - tang;
            fullarc->p2x = 0 - tang;
            fullarc->p2y = 0 - fullarc->rad;
            fullarc->p3x = 0;
            fullarc->p3y = 0 - fullarc->rad;
            break;
        case 3:
            /* fourth quarter */
            fullarc->p0x = 0;
            fullarc->p0y = 0 - fullarc->rad;
            fullarc->p1x = tang;
            fullarc->p1y = 0 - fullarc->rad;
            fullarc->p2x = fullarc->rad;
            fullarc->p2y = 0 - tang;
            fullarc->p3x = fullarc->rad;
            fullarc->p3y = 0;
            break;
        default:
            LV_LOG_ERROR("Invalid arc quarter value.");
            break;
    }
}

/**
 * Linear interpolation between two points 'a' and 'b'
 * 't' parameter is the proportion ratio expressed in range [0 ; T_FRACTION ]
 */
static inline float lerp(float coord_a, float coord_b, float tf)
{
    return ((T_FRACTION - tf) * coord_a + tf * coord_b) / T_FRACTION;
}

/**
 * Computes a point of bezier curve given 't' param
 */
static inline float comp_bezier_point(float t, cubic_cont_pt cp)
{
    float t_sq = t * t;
    float inv_t_sq = (1.0f - t) * (1.0f - t);
    float apt = (1.0f - t) * inv_t_sq * cp.p0 + 3.0f * inv_t_sq * t * cp.p1 + 3.0f * (1.0f - t) * t_sq * cp.p2 + t * t_sq *
                cp.p3;
    return apt;
}

/**
 * Find parameter 't' in curve at point 'pt'
 * proceed by dichotomy on only 1 dimension,
 * works only if the curve is monotonic
 * bezier curve is defined by control points [p0 p1 p2 p3]
 * 'dec' tells if curve is decreasing (true) or increasing (false)
 */
static float get_bez_t_from_pos(float pt, cubic_cont_pt cp, bool dec)
{
    /* initialize dichotomy with boundary 't' values */
    float t_low = 0.0f;
    float t_mid = 0.5f;
    float t_hig = 1.0f;
    float a_pt;
    /* dichotomy loop */
    for(int i = 0; i < DICHOTO_ITER; i++) {
        a_pt = comp_bezier_point(t_mid, cp);
        /* check mid-point position on bezier curve versus targeted point */
        if((a_pt > pt) != dec) {
            t_hig = t_mid;
        }
        else {
            t_low = t_mid;
        }
        /* define new 't' param for mid-point */
        t_mid = (t_low + t_hig) / 2.0f;
    }
    /* return parameter 't' in integer range [0 ; T_FRACTION] */
    return t_mid * T_FRACTION;
}

/**
 * Gives relative coords of the control points
 * for the sub-arc starting at angle with given angle span
 */
static void get_subarc_control_points(vg_arc * arc, int32_t span)
{
    vg_arc fullarc;
    fullarc.angle = arc->angle;
    fullarc.quarter = arc->quarter;
    fullarc.rad = arc->rad;
    set_full_arc(&fullarc);

    /* special case of full arc */
    if(arc->angle == 90) {
        copy_arc(arc, &fullarc);
        return;
    }

    /* compute 1st arc using the geometric construction of curve */
    uint16_t t2 = TperDegree[arc->angle + span];

    /* lerp for A */
    float a2x = lerp(fullarc.p0x, fullarc.p1x, t2);
    float a2y = lerp(fullarc.p0y, fullarc.p1y, t2);
    /* lerp for B */
    float b2x = lerp(fullarc.p1x, fullarc.p2x, t2);
    float b2y = lerp(fullarc.p1y, fullarc.p2y, t2);
    /* lerp for C */
    float c2x = lerp(fullarc.p2x, fullarc.p3x, t2);
    float c2y = lerp(fullarc.p2y, fullarc.p3y, t2);

    /* lerp for D */
    float d2x = lerp(a2x, b2x, t2);
    float d2y = lerp(a2y, b2y, t2);
    /* lerp for E */
    float e2x = lerp(b2x, c2x, t2);
    float e2y = lerp(b2y, c2y, t2);

    float pt2x = lerp(d2x, e2x, t2);
    float pt2y = lerp(d2y, e2y, t2);

    /* compute sub-arc using the geometric construction of curve */
    uint16_t t1 = TperDegree[arc->angle];

    /* lerp for A */
    float a1x = lerp(fullarc.p0x, fullarc.p1x, t1);
    float a1y = lerp(fullarc.p0y, fullarc.p1y, t1);
    /* lerp for B */
    float b1x = lerp(fullarc.p1x, fullarc.p2x, t1);
    float b1y = lerp(fullarc.p1y, fullarc.p2y, t1);
    /* lerp for C */
    float c1x = lerp(fullarc.p2x, fullarc.p3x, t1);
    float c1y = lerp(fullarc.p2y, fullarc.p3y, t1);

    /* lerp for D */
    float d1x = lerp(a1x, b1x, t1);
    float d1y = lerp(a1y, b1y, t1);
    /* lerp for E */
    float e1x = lerp(b1x, c1x, t1);
    float e1y = lerp(b1y, c1y, t1);

    float pt1x = lerp(d1x, e1x, t1);
    float pt1y = lerp(d1y, e1y, t1);

    /* find the 't3' parameter for point P(t1) on the sub-arc [P0 A2 D2 P(t2)] using dichotomy
     * use position of x axis only */
    float t3;
    t3 = get_bez_t_from_pos(pt1x,
    (cubic_cont_pt) {
        .p0 = (fullarc.p0x), .p1 = a2x, .p2 = d2x, .p3 = pt2x
    },
    (bool)(pt2x < fullarc.p0x));

    /* lerp for B */
    float b3x = lerp(a2x, d2x, t3);
    float b3y = lerp(a2y, d2y, t3);
    /* lerp for C */
    float c3x = lerp(d2x, pt2x, t3);
    float c3y = lerp(d2y, pt2y, t3);

    /* lerp for E */
    float e3x = lerp(b3x, c3x, t3);
    float e3y = lerp(b3y, c3y, t3);

    arc->p0x = pt1x;
    arc->p0y = pt1y;
    arc->p1x = e3x;
    arc->p1y = e3y;
    arc->p2x = c3x;
    arc->p2y = c3y;
    arc->p3x = pt2x;
    arc->p3y = pt2y;
}

/**
 * Gives relative coords of the control points
 */
static void get_arc_control_points(vg_arc * arc, bool start)
{
    vg_arc fullarc;
    fullarc.angle = arc->angle;
    fullarc.quarter = arc->quarter;
    fullarc.rad = arc->rad;
    set_full_arc(&fullarc);

    /* special case of full arc */
    if(arc->angle == 90) {
        copy_arc(arc, &fullarc);
        return;
    }

    /* compute sub-arc using the geometric construction of curve */
    uint16_t t = TperDegree[arc->angle];
    /* lerp for A */
    float ax = lerp(fullarc.p0x, fullarc.p1x, t);
    float ay = lerp(fullarc.p0y, fullarc.p1y, t);
    /* lerp for B */
    float bx = lerp(fullarc.p1x, fullarc.p2x, t);
    float by = lerp(fullarc.p1y, fullarc.p2y, t);
    /* lerp for C */
    float cx = lerp(fullarc.p2x, fullarc.p3x, t);
    float cy = lerp(fullarc.p2y, fullarc.p3y, t);

    /* lerp for D */
    float dx = lerp(ax, bx, t);
    float dy = lerp(ay, by, t);
    /* lerp for E */
    float ex = lerp(bx, cx, t);
    float ey = lerp(by, cy, t);

    /* sub-arc's control points are tangents of DeCasteljau's algorithm */
    if(start) {
        arc->p0x = lerp(dx, ex, t);
        arc->p0y = lerp(dy, ey, t);
        arc->p1x = ex;
        arc->p1y = ey;
        arc->p2x = cx;
        arc->p2y = cy;
        arc->p3x = fullarc.p3x;
        arc->p3y = fullarc.p3y;
    }
    else {
        arc->p0x = fullarc.p0x;
        arc->p0y = fullarc.p0y;
        arc->p1x = ax;
        arc->p1y = ay;
        arc->p2x = dx;
        arc->p2y = dy;
        arc->p3x = lerp(dx, ex, t);
        arc->p3y = lerp(dy, ey, t);
    }
}

/**
 * Add the arc control points into the path data for vglite,
 * taking into account the real center of the arc (translation).
 * arc_path: (in/out) the path data array for vglite
 * pidx: (in/out) index of last element added in arc_path
 * q_arc: (in) the arc data containing control points
 * center: (in) the center of the circle in draw coordinates
 * cw: (in) true if arc is clockwise
 */
static void add_split_arc_path(float * arc_path, int * pidx, vg_arc * q_arc, float cx, float cy, bool cw)
{
    /* assumes first control point already in array arc_path[] */
    int idx = *pidx;
    if(cw) {
#if BEZIER_DBG_CONTROL_POINTS
        ((int32_t *)arc_path)[idx++] = VLC_OP_LINE;
        arc_path[idx++] = q_arc->p1x + cx;
        arc_path[idx++] = q_arc->p1y + cy;
        *(int32_t *)&(arc_path[idx++]) = VLC_OP_LINE;
        arc_path[idx++] = q_arc->p2x + cx;
        arc_path[idx++] = q_arc->p2y + cy;
        *(int32_t *)&(arc_path[idx++]) = VLC_OP_LINE;
        arc_path[idx++] = q_arc->p3x + cx;
        arc_path[idx++] = q_arc->p3y + cy;
#else
        ((int32_t *)arc_path)[idx++] = VLC_OP_CUBIC;
        arc_path[idx++] = q_arc->p1x + cx;
        arc_path[idx++] = q_arc->p1y + cy;
        arc_path[idx++] = q_arc->p2x + cx;
        arc_path[idx++] = q_arc->p2y + cy;
        arc_path[idx++] = q_arc->p3x + cx;
        arc_path[idx++] = q_arc->p3y + cy;
#endif
    }
    else {      /* reverse points order when counter-clockwise */
#if BEZIER_DBG_CONTROL_POINTS
        ((int32_t *)arc_path)[idx++] = VLC_OP_LINE;
        arc_path[idx++] = q_arc->p2x + cx;
        arc_path[idx++] = q_arc->p2y + cy;
        ((int32_t *)arc_path)[idx++] = VLC_OP_LINE;
        arc_path[idx++] = q_arc->p1x + cx;
        arc_path[idx++] = q_arc->p1y + cy;
        ((int32_t *)arc_path)[idx++] = VLC_OP_LINE;
        arc_path[idx++] = q_arc->p0x + cx;
        arc_path[idx++] = q_arc->p0y + cy;
#else
        ((int32_t *)arc_path)[idx++] = VLC_OP_CUBIC;
        arc_path[idx++] = q_arc->p2x + cx;
        arc_path[idx++] = q_arc->p2y + cy;
        arc_path[idx++] = q_arc->p1x + cx;
        arc_path[idx++] = q_arc->p1y + cy;
        arc_path[idx++] = q_arc->p0x + cx;
        arc_path[idx++] = q_arc->p0y + cy;
#endif
    }
    /* update index i n path array*/
    *pidx = idx;
}

static void add_arc_path(float * arc_path, int * pidx, float radius,
                         int32_t start_angle, int32_t end_angle,
                         float cx, float cy, bool cw)
{
    /* set number of arcs to draw */
    vg_arc q_arc;
    int32_t start_arc_angle = start_angle % 90;
    int32_t end_arc_angle = end_angle % 90;
    int32_t inv_start_arc_angle = (start_arc_angle > 0) ? (90 - start_arc_angle) : 0;
    int32_t nbarc = (end_angle - start_angle - inv_start_arc_angle - end_arc_angle) / 90;
    q_arc.rad = radius;

    /* handle special case of start & end point in the same quarter */
    if(((start_angle / 90) == (end_angle / 90)) && (nbarc <= 0)) {
        q_arc.quarter = (start_angle / 90) % 4;
        q_arc.angle = start_arc_angle;
        get_subarc_control_points(&q_arc, end_arc_angle - start_arc_angle);
        add_split_arc_path(arc_path, pidx, &q_arc, cx, cy, cw);
        return;
    }

    if(cw) {
        /* partial starting arc */
        if(start_arc_angle > 0) {
            q_arc.quarter = (start_angle / 90) % 4;
            q_arc.angle = start_arc_angle;
            /* get cubic points relative to center */
            get_arc_control_points(&q_arc, true);
            /* put cubic points in arc_path */
            add_split_arc_path(arc_path, pidx, &q_arc, cx, cy, cw);
        }
        /* full arcs */
        for(int32_t q = 0; q < nbarc ; q++) {
            q_arc.quarter = (q + ((start_angle + 89) / 90)) % 4;
            q_arc.angle = 90;
            /* get cubic points relative to center */
            get_arc_control_points(&q_arc, true);   /* 2nd parameter 'start' ignored */
            /* put cubic points in arc_path */
            add_split_arc_path(arc_path, pidx, &q_arc, cx, cy, cw);
        }
        /* partial ending arc */
        if(end_arc_angle > 0) {
            q_arc.quarter = (end_angle / 90) % 4;
            q_arc.angle = end_arc_angle;
            /* get cubic points relative to center */
            get_arc_control_points(&q_arc, false);
            /* put cubic points in arc_path */
            add_split_arc_path(arc_path, pidx, &q_arc, cx, cy, cw);
        }

    }
    else {   /* counter clockwise */

        /* partial ending arc */
        if(end_arc_angle > 0) {
            q_arc.quarter = (end_angle / 90) % 4;
            q_arc.angle = end_arc_angle;
            /* get cubic points relative to center */
            get_arc_control_points(&q_arc, false);
            /* put cubic points in arc_path */
            add_split_arc_path(arc_path, pidx, &q_arc, cx, cy, cw);
        }
        /* full arcs */
        for(int32_t q = nbarc - 1; q >= 0; q--) {
            q_arc.quarter = (q + ((start_angle + 89) / 90)) % 4;
            q_arc.angle = 90;
            /* get cubic points relative to center */
            get_arc_control_points(&q_arc, true);   /* 2nd parameter 'start' ignored */
            /* put cubic points in arc_path */
            add_split_arc_path(arc_path, pidx, &q_arc, cx, cy, cw);
        }
        /* partial starting arc */
        if(start_arc_angle > 0) {
            q_arc.quarter = (start_angle / 90) % 4;
            q_arc.angle = start_arc_angle;
            /* get cubic points relative to center */
            get_arc_control_points(&q_arc, true);
            /* put cubic points in arc_path */
            add_split_arc_path(arc_path, pidx, &q_arc, cx, cy, cw);
        }
    }
}

#endif /*LV_USE_GPU_ACTS_VG_LITE*/
