#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <os_common_api.h>
#include <font_mempool.h>
#include <lvgl/lvgl_freetype_font.h>

#ifdef CONFIG_FREETYPE_FONT_ENABLE_SVG_PATH
#include <vg_lite/vg_lite.h>
#endif

static int force_bitmap_output = 0;
/*
static uint32_t tstart, tend;
static uint32_t ttotal, taverage;
static uint32_t tcount;
*/
bool freetype_font_get_glyph_dsc_cb(const lv_font_t * lv_font, lv_font_glyph_dsc_t * dsc_out, uint32_t unicode, uint32_t unicode_next)
{	
	bbox_metrics_t* metric;
	lv_font_fmt_freetype_dsc_t * dsc = (lv_font_fmt_freetype_dsc_t *)(lv_font->user_data);
#if CONFIG_FREETYPE_FONT_ENABLE_SVG_PATH
	float scale;
#endif
	
	SYS_LOG_DBG("lv font %p, font %p, unicode 0x%x, func %p\n", lv_font, dsc->font, unicode, __func__);
	if(unicode < 0x20) 
	{
		dsc_out->adv_w = 0;
		dsc_out->box_h = 0;
		dsc_out->box_w = 0;
		dsc_out->ofs_x = 0;
		dsc_out->ofs_y = 0;
		dsc_out->bpp = dsc->font->bpp;
		return true;
	}

	if(unicode == 0x20 || unicode == 0xa0) 
	{
		dsc_out->adv_w = dsc->font->font_size/2;
		dsc_out->box_h = 0;
		dsc_out->box_w = 0;
		dsc_out->ofs_x = 0;
		dsc_out->ofs_y = 0;
		dsc_out->bpp = dsc->font->bpp;
		return true;
	}


#ifdef CONFIG_BITMAP_FONT
	if(unicode >= 0x1F300)
	{
		if(bitmap_font_get_max_emoji_num() == 0)
		{
			dsc_out->adv_w = 0;
			dsc_out->box_h = 0;
			dsc_out->box_w = 0;
			dsc_out->ofs_x = 0;
			dsc_out->ofs_y = 0;
			dsc_out->bpp = dsc->font->bpp;
			return true;			
		}
		else
		{
			metric = (bbox_metrics_t*)bitmap_font_get_emoji_glyph_dsc(dsc->emoji_font, unicode, true);
		}
	}
	else
#endif
	{
		//tstart = os_cycle_get_32();
#if CONFIG_FREETYPE_FONT_ENABLE_SVG_PATH
		if(force_bitmap_output)
		{
			metric = freetype_font_get_glyph_dsc(dsc->font, dsc->cache, unicode);
		}
		else
		{
			metric = freetype_font_get_glyph_dsc(dsc->font, dsc->font->shape_cache, unicode);
		}
#else
		metric = freetype_font_get_glyph_dsc(dsc->font, dsc->cache, unicode);
#endif
		//tend = os_cycle_get_32();

/*
		taverage = k_cyc_to_us_near32(tend-tstart);
		if(taverage > 50)
		{
			SYS_LOG_INF("0x%x get dsc %d us\n", unicode, taverage);
		}
*/
/*
		taverage = 	k_cyc_to_us_near32(tend-tstart);
		if(taverage > 200 && unicode != 0x20)
		{
			ttotal = ttotal + taverage;
			tcount++;
			if(tcount % 30 == 0)
			{
				taverage = ttotal/tcount;
				SYS_LOG_INF("30 average is %d us\n", taverage);
				ttotal = 0;
				taverage = 0;
				tcount = 0;
			}
		}
*/	
	}

	SYS_LOG_DBG("font size %d, metric size %d\n", dsc->font->font_size, metric->metric_size);


	if(metric == NULL)
	{
		dsc_out->adv_w = 0;
		dsc_out->box_h = 0;
		dsc_out->box_w = 0;
		dsc_out->ofs_x = 0;
		dsc_out->ofs_y = 0;
		dsc_out->bpp = dsc->font->bpp;
		return true;		
	}

#if CONFIG_FREETYPE_FONT_ENABLE_SVG_PATH
	if(force_bitmap_output || unicode >= 0x1F300)
	{
		dsc_out->adv_w = (uint16_t)(metric->advance);
		dsc_out->box_h = (uint16_t)(metric->bbh); 		/*Height of the bitmap in [px]*/
		dsc_out->box_w = (uint16_t)(metric->bbw);		 /*Width of the bitmap in [px]*/
	}	
	else
	{
		scale = (float)(dsc->font->font_size)/metric->metric_size;
		dsc_out->adv_w = (uint16_t)(metric->advance*scale);
		dsc_out->box_h = (uint16_t)(metric->bbh*scale); 		/*Height of the bitmap in [px]*/
		dsc_out->box_w = (uint16_t)(metric->bbw*scale);		 /*Width of the bitmap in [px]*/
	}
#else
	dsc_out->adv_w = (uint16_t)(metric->advance);
	dsc_out->box_h = (uint16_t)(metric->bbh); 		/*Height of the bitmap in [px]*/
	dsc_out->box_w = (uint16_t)(metric->bbw);		 /*Width of the bitmap in [px]*/
#endif
	dsc_out->ofs_x = metric->bbx; 		/*X offset of the bitmap in [pf]*/
	dsc_out->ofs_y = metric->bby;		  /*Y offset of the bitmap measured from the as line*/

	SYS_LOG_DBG("0x%x, metrics %d %d %d %d, %d\n", unicode, dsc_out->box_h, dsc_out->box_w, dsc_out->ofs_x, dsc_out->ofs_y, dsc_out->adv_w);

#if CONFIG_FREETYPE_FONT_ENABLE_SVG_PATH
	if(force_bitmap_output)
	{
		dsc_out->bpp = dsc->font->bpp; 		/*Bit per pixel: 1/2/4/8*/
	}
	else
	{
		dsc_out->bpp = LV_SVGFONT_BPP;
	}
#else
	dsc_out->bpp = dsc->font->bpp; 		/*Bit per pixel: 1/2/4/8*/
#endif

	return true;
}


#if CONFIG_FREETYPE_FONT_ENABLE_SVG_PATH
static int ttf_get_svg_path_size(freetype_vertex_t * vertices, int num_verts)
{
    /*At least, VLC_OP_END, bounding box and path_length*/
    int len = 6 * sizeof(float);

    for (; num_verts > 0; num_verts--, vertices++) {
        switch (vertices[0].type) {
        case FT_vmove:
            len += (2 + 1) * sizeof(float);
            break;
        case FT_vline:
            len += (2 + 1) * sizeof(float);
            break;
        case FT_vcurve:
            len += (4 + 1) * sizeof(float);
            break;
        case FT_vcubic:
            len += (6 + 1) * sizeof(float);
            break;
        default:
            break;
        }
    }

    return len;
}

static void ttf_convert_svg_path(float * path_data, int path_size,
        int bbox_x1, int bbox_y1, int bbox_x2, int bbox_y2,
        freetype_vertex_t * vertices, int num_verts)
{
    /*bounding box*/
    *path_data++ = bbox_x1;
    *path_data++ = bbox_y1;
    *path_data++ = bbox_x2;
    *path_data++ = bbox_y2;

    /*path length*/
    *(int *)path_data = path_size - 5 * sizeof(float);
    path_data++;

    /*
     * All the vertices coordinate are relative to "Current Point" metioned in stb_truetype.
     * Also invert y to follow the display screen Y direction.
     */
    for (; num_verts > 0; num_verts--, vertices++) {
        switch (vertices[0].type) {
        case FT_vmove:
            *(uint8_t *)path_data = VLC_OP_MOVE;
            path_data++;
            *path_data++ = vertices[0].x;
            *path_data++ = -vertices[0].y;
            break;
        case FT_vline:
            *(uint8_t *)path_data = VLC_OP_LINE;
            path_data++;
            *path_data++ = vertices[0].x;
            *path_data++ = -vertices[0].y;
            break;
        case FT_vcurve:
            *(uint8_t *)path_data = VLC_OP_QUAD;
            path_data++;
            *path_data++ = vertices[0].cx;
            *path_data++ = -vertices[0].cy;
            *path_data++ = vertices[0].x;
            *path_data++ = -vertices[0].y;
            break;
        case FT_vcubic:
            *(uint8_t *)path_data = VLC_OP_CUBIC;
            path_data++;
            *path_data++ = vertices[0].cx;
            *path_data++ = -vertices[0].cy;
            *path_data++ = vertices[0].cx1;
            *path_data++ = -vertices[0].cy1;
            *path_data++ = vertices[0].x;
            *path_data++ = -vertices[0].y;
            break;
        default:
            break;
        }
    }

    *(uint8_t *)path_data = VLC_OP_END;
}

static float ttf_svg_cache[2048];
static lv_svgfont_bitmap_dsc_t ft_svgmap;

static void * ttf_generate_svg_path(freetype_font_t* font, uint32_t unicode, float* scale)
{
	float sca;
    freetype_glyphshape_t * shape = NULL;
	SYS_LOG_DBG("ttf_generate_svg_path entry 0x%x\n", unicode);
    freetype_font_load_glyph_shape(font, font->shape_cache, unicode, &shape);
    if (shape == NULL)
	{
		SYS_LOG_INF("shape not found 0x%x\n", unicode);
        return NULL;
	}
	SYS_LOG_DBG("font size %d, shape size %d\n", font->font_size, shape->current_size);
	sca = (float)(font->font_size)/shape->current_size;
	sca = sca/64.0;
	*scale = sca;

    int path_size = ttf_get_svg_path_size(shape->vertices, shape->num_vertices);
	if (path_size > sizeof(ttf_svg_cache))
		return NULL;

	SYS_LOG_DBG("path size %d\n", path_size);
    float * path_data = ttf_svg_cache;

    /* CARE: the vertex can not have subpixel shift which applied after scaling */
	int16_t bbx1 = shape->bbox_x1;//*sca*64;
	int16_t bbx2 = shape->bbox_x2;//*sca*64;
	int16_t bby1 = shape->bbox_y1;//*sca*64;
	int16_t bby2 = shape->bbox_y2;//*sca*64;
    ttf_convert_svg_path(path_data, path_size, bbx1, -bby2,
			bbx2, -bby1, shape->vertices, shape->num_vertices);

    freetype_font_free_glyph_shape(shape);


    return path_data;
}
#endif /* CONFIG_FREETYPE_FONT_ENABLE_SVG_PATH */



/* Get the bitmap of `unicode_letter` from `font`. */
static const uint8_t * freetype_font_get_glyph_bitmap_cb(const lv_font_t * font, uint32_t unicode)
{
	uint8_t* data;
	lv_font_fmt_freetype_dsc_t* font_dsc;

	if(font == NULL)
	{
		SYS_LOG_ERR("null font info, %p\n", font);
		return NULL;
	}
	font_dsc = (lv_font_fmt_freetype_dsc_t*)font->user_data;

#ifdef CONFIG_BITMAP_FONT
	if(unicode >= 0x1F300)
	{
		if(bitmap_font_get_max_emoji_num() == 0)
		{
			return NULL;
		}
		else
		{
			data = bitmap_font_get_emoji_bitmap(font_dsc->emoji_font, unicode);
		}
	}
	else
#endif		
	{
#if CONFIG_FREETYPE_FONT_ENABLE_SVG_PATH
		if(force_bitmap_output)
		{
			data = freetype_font_get_bitmap(font_dsc->font, font_dsc->cache, unicode);
		}
		else
		{
			float scale = 1.0/64.0;
			data = ttf_generate_svg_path(font_dsc->font, unicode, &scale);
			if (data) {
				SYS_LOG_DBG("scale %f\n", scale);
				ft_svgmap.scale = scale; /* TODO: set the proper scale factor */
				ft_svgmap.shift_x = 0.15;
				ft_svgmap.shift_y = 0.15;
				memcpy(ft_svgmap.path_box, data, sizeof(ft_svgmap.path_box));
				data += 4 * sizeof(float);
				ft_svgmap.path_len = *(int *)data;
				ft_svgmap.path_data = (float *)data + 1;
				return (uint8_t * )&ft_svgmap;
			}
		}
#else
		data = freetype_font_get_bitmap(font_dsc->font, font_dsc->cache, unicode);
#endif
	}
	
	//SYS_LOG_INF("0x%x get bitmap %d us\n", unicode, k_cyc_to_us_near32(tend-tstart));

	return data;

}


int lvgl_freetype_font_init(void)
{
	void* ret;
	ret = freetype_font_init(1);
	if(ret == NULL)
	{
		SYS_LOG_ERR("freetype library init failed\n");
		return -1;
	}
	return 0;
}

int lvgl_freetype_font_deinit(void)
{
	freetype_font_deinit(1);
	return 0;
}

int lvgl_freetype_font_open(lv_font_t* font, const char * font_path, uint32_t font_size)
{
	lv_font_fmt_freetype_dsc_t * dsc = bitmap_font_cache_malloc(sizeof(lv_font_fmt_freetype_dsc_t));
	if(dsc == NULL) return -1;

	SYS_LOG_INF("open ftxxx %s %d\n", font_path, font_size);

	dsc->font = freetype_font_open(font_path, font_size);
	if(!dsc->font)
	{
		SYS_LOG_ERR("freetype opne font %s faild\n", font_path);
		bitmap_font_cache_free(dsc);
		return -1;
	}
	
	dsc->cache = freetype_font_get_cache(dsc->font);
	SYS_LOG_INF("dsc->font %p\n", dsc->font);
	//dsc->font_size = font_size;


	font->get_glyph_dsc = freetype_font_get_glyph_dsc_cb; 	   /*Set a callback to get info about gylphs*/
	font->get_glyph_bitmap = freetype_font_get_glyph_bitmap_cb;  /*Set a callback to get bitmap of a glyp*/

	if(font->user_data != NULL)
	{
		SYS_LOG_INF("ftxxx non null font user data %p\n", font->user_data);
	}

	font->user_data = dsc;
	font->line_height = dsc->font->line_height;
	font->base_line = dsc->font->base_line;  /*Base line measured from the top of line_height*/
	if(freetype_font_enable_subpixel())
	{
		font->subpx = LV_FONT_SUBPX_HOR;
	}
	else
	{
		font->subpx = LV_FONT_SUBPX_NONE;
	}

	SYS_LOG_INF("line height %d, base line %d, dsc %p\n", font->line_height, font->base_line, dsc);
	return 0;
		
}

void lvgl_freetype_font_close(lv_font_t* font)
{
    lv_font_fmt_freetype_dsc_t * dsc;

	if(font == NULL)
	{
		SYS_LOG_ERR("null font pointer\n");
		return;
	}	

	dsc = (lv_font_fmt_freetype_dsc_t*)font->user_data;
	
	if(dsc == NULL)
	{
		SYS_LOG_ERR("invalid font dsc \n");
		return;
	}

	SYS_LOG_INF("close ftxxx %p, %d\n", dsc, dsc->font->font_size);
	freetype_font_close(dsc->font);	
/*	
	if(dsc->emoji_font != NULL)
	{
		bitmap_emoji_font_close(dsc->emoji_font);
	}	
*/
	bitmap_font_cache_free(dsc);
	font->user_data = NULL;
}

#ifdef CONFIG_BITMAP_FONT
int lvgl_freetype_font_set_emoji_font(lv_font_t* lv_font, const char* emoji_font_path)
{
	lv_font_fmt_freetype_dsc_t * dsc = (lv_font_fmt_freetype_dsc_t*)lv_font->user_data;

	if(dsc == NULL)
	{
		SYS_LOG_ERR("null bitmap font for font %p\n", lv_font);
		return -1;
	}

	if(bitmap_font_get_max_emoji_num() == 0)
	{
		SYS_LOG_ERR("emoji not supported\n");
		return -1;	
	}
	
	dsc->emoji_font = bitmap_emoji_font_open(emoji_font_path);
	if(dsc->emoji_font == NULL)
	{
		SYS_LOG_ERR("open bitmap emoji font failed\n");
		return -1;
	}
	return 0;
}

int lvgl_freetype_font_get_emoji_dsc(const lv_font_t* lv_font, uint32_t unicode, lv_img_dsc_t* dsc, lv_point_t* pos, bool force_retrieve)
{
	lv_font_fmt_freetype_dsc_t* font_dsc;
	glyph_metrics_t* metric;

	if(bitmap_font_get_max_emoji_num() == 0)
	{
		SYS_LOG_ERR("emoji not supported\n");
		return -1;	
	}

	if(dsc == NULL)
	{
		SYS_LOG_ERR("invalid img dsc for emoji 0x%x\n", unicode);
		return -1;
	}

	if(unicode < 0x1F300)
	{
		SYS_LOG_ERR("invalid emoji code 0x%x\n", unicode);
		return -1;
	}

	font_dsc = (lv_font_fmt_freetype_dsc_t*)lv_font->user_data;
	if(font_dsc == NULL)
	{
		SYS_LOG_ERR("null bitmap font for font %p\n", lv_font);
		return -1;
	}

	metric = bitmap_font_get_emoji_glyph_dsc(font_dsc->emoji_font, unicode, force_retrieve);
	if(!metric)
	{
		return -1;
	}
	
	dsc->header.cf = LV_IMG_CF_ARGB_6666;
	dsc->header.w = metric->bbw;
	dsc->header.h = metric->bbh;
	dsc->header.always_zero = 0;
	dsc->data_size = metric->bbw*metric->bbh*3;
//	SYS_LOG_INF("emoji metric %d %d, dsc %d %d, size %d\n", metric->bbw, metric->bbh, dsc->header.w, dsc->header.h, dsc->data_size);
	dsc->data = bitmap_font_get_emoji_bitmap(font_dsc->emoji_font, unicode);

	if(pos)
	{
		pos->y += metric->bby;
	}
	return 0;
}
#else
int lvgl_freetype_font_set_emoji_font(lv_font_t* lv_font, const char* emoji_font_path)
{
	return 0;
}

int lvgl_freetype_font_get_emoji_dsc(const lv_font_t* lv_font, uint32_t unicode, lv_img_dsc_t* dsc, lv_point_t* pos, bool force_retrieve)
{
	return 0;
}

#endif

int lvgl_freetype_font_set_default_code(lv_font_t* font, uint32_t word_code, uint32_t emoji_code)
{
	lv_font_fmt_freetype_dsc_t* font_dsc;

	if(font == NULL)
	{
		SYS_LOG_ERR("null font");
		return -1;
	}
	
	font_dsc = (lv_font_fmt_freetype_dsc_t*)font->user_data;
	if(font_dsc == NULL)
	{
		SYS_LOG_ERR("cant set default code to an unopend font %p", font);
		return -1;
	}

	freetype_font_set_default_code(font_dsc->font, word_code);

#ifdef CONFIG_BITMAP_FONT
	if(bitmap_font_get_max_emoji_num() > 0)
	{
		bitmap_font_set_default_emoji_code(font_dsc->emoji_font, emoji_code);
	}
#endif
	return 0;
}

int lvgl_freetype_font_set_default_bitmap(lv_font_t* font, uint8_t* bitmap, uint32_t width, uint32_t height, uint32_t gap, uint32_t bpp)
{
	lv_font_fmt_freetype_dsc_t* font_dsc;

	if(font == NULL)
	{
		SYS_LOG_ERR("null font");
		return -1;
	}
	
	font_dsc = (lv_font_fmt_freetype_dsc_t*)font->user_data;
	if(font_dsc == NULL)
	{
		SYS_LOG_ERR("cant set default code to an unopend font %p", font);
		return -1;
	}

	freetype_font_set_default_bitmap(font_dsc->font, bitmap, width, height, gap, bpp);

	return 0;

}

int lvgl_freetype_font_cache_preset(freetype_font_cache_preset_t* preset, int count)
{
    return freetype_font_cache_preset(preset, count);
}

void lvgl_freetype_force_bitmap(lv_font_t* font, int enable)
{
	force_bitmap_output = enable;
}
