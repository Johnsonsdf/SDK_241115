/**
 * @file lv_lottie.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../../../lv_conf_internal.h"
#if LV_USE_LOTTIE

#if LV_USE_THORVG_EXTERNAL
    #include <thorvg_capi.h>
#else
    #include "../../libs/thorvg/thorvg_capi.h"
#endif

#include "../../../misc/lv_timer.h"
#include "lv_lottie.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS (&lv_lottie_class)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_lottie_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_lottie_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void anim_exec_cb(void * var, int32_t v);
static void lottie_update(lv_lottie_t * lottie, int32_t v);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_lottie_class = {
    .constructor_cb = lv_lottie_constructor,
    .destructor_cb = lv_lottie_destructor,
    .width_def = LV_DPI_DEF,
    .height_def = LV_DPI_DEF,
    .instance_size = sizeof(lv_lottie_t),
    .base_class = &lv_canvas_class,
};

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_lottie_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lv_lottie_set_buffer(lv_obj_t * obj, int32_t w, int32_t h, void * buf)
{
    lv_lottie_t * lottie = (lv_lottie_t *)obj;

    tvg_swcanvas_set_target(lottie->tvg_canvas, buf, w, w, h, TVG_COLORSPACE_ARGB8888S);
    tvg_canvas_push(lottie->tvg_canvas, lottie->tvg_paint);
    lv_canvas_set_buffer(obj, buf, w, h, LV_IMG_CF_ARGB_8888);
    tvg_picture_set_size(lottie->tvg_paint, w, h);

    /*Force updating when the buffer changes*/
    float f_current;
    tvg_animation_get_frame(lottie->tvg_anim, &f_current);
    anim_exec_cb(obj, (int32_t) f_current);
}

void lv_lottie_set_src_data(lv_obj_t * obj, const void * src, size_t src_size, bool copy)
{
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    tvg_picture_load_data(lottie->tvg_paint, src, src_size, "lottie", copy);
    lv_img_dsc_t * img_dsc = lv_canvas_get_img(obj);
    if(img_dsc) {
        tvg_picture_set_size(lottie->tvg_paint, img_dsc->header.w, img_dsc->header.h);
    }

    float f_total;
    tvg_animation_get_total_frame(lottie->tvg_anim, &f_total);
    lv_anim_set_time(lottie->anim, (int32_t)f_total * 1000 / 60); /*60 FPS*/
    lottie->anim->act_time = 0;
    lottie->anim->end_value = (int32_t)f_total;
    lottie->anim->playback_now = false;
    lottie_update(lottie, 0);   /*Render immediately*/
}

void lv_lottie_set_src_file(lv_obj_t * obj, const char * src)
{
#if 1
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    if(lottie->buffer) {
        lv_mem_free(lottie->buffer);
        lottie->buffer = NULL;
    }

    lv_fs_file_t file;
    if(LV_FS_RES_OK != lv_fs_open(&file, src, LV_FS_MODE_RD)) return;

    uint32_t size = 0;
    lv_fs_seek(&file, 0, LV_FS_SEEK_END);
    lv_fs_tell(&file, &size);
    lv_fs_seek(&file, 0, LV_FS_SEEK_SET);

    if(size == 0) {
        lv_fs_close(&file);
        return;
    }

    lottie->buffer = lv_mem_alloc(size + 1);
    if(lottie->buffer) {
        lv_fs_read(&file, lottie->buffer, size, NULL);
        lottie->buffer[size] = 0;

        lv_lottie_set_src_data(obj, lottie->buffer, size + 1, false);
    }

    lv_fs_close(&file);

#else
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    tvg_picture_load(lottie->tvg_paint, src);
    lv_img_dsc_t * img_dsc = lv_canvas_get_img(obj);
    if(img_dsc) {
        tvg_picture_set_size(lottie->tvg_paint, img_dsc->header.w, img_dsc->header.h);
    }

    float f_total;
    tvg_animation_get_total_frame(lottie->tvg_anim, &f_total);
    lv_anim_set_time(lottie->anim, (int32_t)f_total * 1000 / 60); /*60 FPS*/
    lottie->anim->act_time = 0;
    lottie->anim->end_value = (int32_t)f_total;
    lottie->anim->playback_now = false;
    lottie_update(lottie, 0);   /*Render immediately*/
#endif
}


lv_anim_t * lv_lottie_get_anim(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    return lottie->anim;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_lottie_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    lv_lottie_t * lottie = (lv_lottie_t *)obj;
    lottie->tvg_anim = tvg_animation_new();

    lottie->tvg_paint = tvg_animation_get_picture(lottie->tvg_anim);

    lottie->tvg_canvas = tvg_swcanvas_create();

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, anim_exec_cb);
    lv_anim_set_var(&a, obj);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lottie->anim = lv_anim_start(&a);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_lottie_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_lottie_t * lottie = (lv_lottie_t *)obj;

    tvg_animation_del(lottie->tvg_anim);
    tvg_canvas_destroy(lottie->tvg_canvas);

    if(lottie->buffer) {
        lv_mem_free(lottie->buffer);
    }
}

static void anim_exec_cb(void * var, int32_t v)
{
    lv_lottie_t * lottie = var;

    /*Do not render not visible animations.*/
    if(lv_obj_is_visible(var)) {
        lottie_update(lottie, v);
        if(lottie->anim) {
            lottie->last_rendered_time = lottie->anim->act_time;
        }
    }
    else {
        /*Artificially keep the animation on the last rendered frame's time
         *To avoid a jump when the widget becomes visible*/
        if(lottie->anim) {
            lottie->anim->act_time = lottie->last_rendered_time;
        }
    }
}

static void lottie_update(lv_lottie_t * lottie, int32_t v)
{
    lv_obj_t * obj = (lv_obj_t *) lottie;

#if 0
    lv_draw_buf_t * draw_buf = lv_canvas_get_draw_buf(obj);
    if(draw_buf) {
#if LV_USE_DRAW_VG_LITE && LV_USE_VG_LITE_THORVG
        /**
         * Since the buffer clearing operation in the SwRenderer::preRender
         * function is removed when the VG-Lite simulator is enabled, the canvas
         * buffer must be manually cleared here.
         */
        lv_draw_buf_clear(draw_buf, NULL);
#endif
        /*Drop old cached image*/
        lv_image_cache_drop(lv_image_get_src(obj));
    }
#endif

    tvg_animation_set_frame(lottie->tvg_anim, v);
    tvg_canvas_update(lottie->tvg_canvas);
    tvg_canvas_draw(lottie->tvg_canvas);
    tvg_canvas_sync(lottie->tvg_canvas);

    lv_obj_invalidate(obj);
}

#endif /*LV_USE_LOTTIE*/
