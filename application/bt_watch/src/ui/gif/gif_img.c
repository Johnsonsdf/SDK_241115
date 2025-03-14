/**
 * @file gif_img.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "gif_img.h"
#include "gif_decode.h"
#include <fs/fs.h>

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS    &gif_img_class

#define GIF_PROFILE 0

/**********************
 *      TYPEDEFS
 **********************/


/**********************
 *   STATIC FUNCTIONS
 **********************/
static void next_frame_task_cb(lv_timer_t * t)
{
    lv_obj_t * obj = t->user_data;
    gif_img_t * gifobj = (gif_img_t *) obj;
    uint32_t elaps = lv_tick_elaps(gifobj->last_call);
    if(elaps < gifobj->gif->gce.delay * 10) return;

    gifobj->last_call = lv_tick_get();

#if GIF_PROFILE
    uint32_t cycles = k_cycle_get_32();
#endif

    int has_next = decode_get_frame(gifobj->gif);

#if GIF_PROFILE
    cycles = k_cycle_get_32() - cycles;
    LV_LOG_WARN("gif get_frame %u us", k_cyc_to_us_floor32(cycles));
#endif

    if(has_next == 0) {
        /*It was the last repeat*/
        lv_res_t res = lv_event_send(obj, LV_EVENT_READY, NULL);
        lv_timer_pause(t);
        if(res != LV_FS_RES_OK) return;
    }

#if GIF_PROFILE
    cycles = k_cycle_get_32();
#endif

    decode_render_frame(gifobj->gif, (uint8_t *)gifobj->imgdsc.data);

#if GIF_PROFILE
    cycles = k_cycle_get_32() - cycles;
    LV_LOG_WARN("gif render_frame %u us", k_cyc_to_us_floor32(cycles));
#endif

    lv_img_cache_invalidate_src(lv_img_get_src(obj));
    lv_obj_invalidate(obj);
}

static void gif_img_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    gif_img_t * gifobj = (gif_img_t *) obj;

    gifobj->gif = NULL;
    gifobj->timer = lv_timer_create(next_frame_task_cb, 10, obj);
    lv_timer_pause(gifobj->timer);
}

static void gif_img_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    gif_img_t * gifobj = (gif_img_t *) obj;
    lv_img_cache_invalidate_src(&gifobj->imgdsc);
    if(gifobj->gif)
        decode_close_gif(gifobj->gif);
    lv_timer_del(gifobj->timer);
}

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t gif_img_class = {
    .constructor_cb = gif_img_constructor,
    .destructor_cb = gif_img_destructor,
    .instance_size = sizeof(gif_img_t),
    .base_class = &lv_img_class
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * gif_img_create(lv_obj_t * parent)
{

    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void gif_img_set_src(lv_obj_t * obj, const void * src)
{
    gif_img_t * gifobj = (gif_img_t *) obj;

    /*Close previous gif if any*/
    if(gifobj->gif) {
        lv_img_cache_invalidate_src(&gifobj->imgdsc);
        decode_close_gif(gifobj->gif);
        gifobj->gif = NULL;
        gifobj->imgdsc.data = NULL;
    }

    if(src != NULL) {
#if GIF_PROFILE
        uint32_t cycles = k_cycle_get_32();
#endif

        gifobj->gif = decode_open_gif_file(src);

#if GIF_PROFILE
        cycles = k_cycle_get_32() - cycles;
        LV_LOG_WARN("gif open %dx%d %u us\n", gifobj->gif->width, gifobj->gif->height, k_cyc_to_us_floor32(cycles));
#endif
    }

    if(gifobj->gif == NULL) {
        LV_LOG_WARN("Could't load the source");
        return;
    }

    gifobj->imgdsc.data = gifobj->gif->canvas;
    gifobj->imgdsc.header.always_zero = 0;
#if LV_COLOR_DEPTH >= 16
    gifobj->imgdsc.header.cf = LV_IMG_CF_ARGB_8888;
#else
    gifobj->imgdsc.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
#endif
    gifobj->imgdsc.header.h = gifobj->gif->height;
    gifobj->imgdsc.header.w = gifobj->gif->width;
    gifobj->last_call = lv_tick_get();

    lv_img_set_src(obj, &gifobj->imgdsc);

    lv_timer_resume(gifobj->timer);
    lv_timer_reset(gifobj->timer);

    next_frame_task_cb(gifobj->timer);

}

void gif_img_restart(lv_obj_t * obj)
{
    gif_img_t * gifobj = (gif_img_t *) obj;
    decode_rewind(gifobj->gif);
    lv_timer_resume(gifobj->timer);
    lv_timer_reset(gifobj->timer);
}

void gif_img_set_pause(lv_obj_t * obj)
{
    gif_img_t * gifobj = (gif_img_t *) obj;
    gifobj->pause_offset = lv_tick_elaps(gifobj->last_call);
    lv_timer_pause(gifobj->timer);
}

bool gif_img_get_paused(lv_obj_t * obj)
{
    gif_img_t * gifobj = (gif_img_t *) obj;
    return gifobj->timer->paused;
}

void gif_img_set_resume(lv_obj_t * obj)
{
    gif_img_t * gifobj = (gif_img_t *) obj;
    gifobj->last_call = lv_tick_get();
    if(gifobj->pause_offset > gifobj->last_call)
        gifobj->last_call = UINT32_MAX - (gifobj->pause_offset - gifobj->last_call);
    else
        gifobj->last_call = gifobj->last_call - gifobj->pause_offset;
    lv_timer_resume(gifobj->timer);
}

