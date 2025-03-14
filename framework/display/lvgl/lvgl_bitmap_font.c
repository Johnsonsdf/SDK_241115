#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <os_common_api.h>
#include <lvgl/lvgl_bitmap_font.h>
#include "font_mempool.h"

bool bitmap_font_get_glyph_dsc_cb(const lv_font_t * lv_font, lv_font_glyph_dsc_t * dsc_out, uint32_t unicode, uint32_t unicode_next)
{
	lv_font_fmt_bitmap_dsc_t* font_dsc;
	bitmap_font_t* font;
	bitmap_cache_t* cache;
	glyph_metrics_t* metric;

	if (unicode == '\n' || unicode == '\r')
		return false;

	if((lv_font == NULL) || (dsc_out == NULL))
	{
		SYS_LOG_ERR("null font info, %p, %p\n", lv_font, dsc_out);
		return false;
	}
	font_dsc = (lv_font_fmt_bitmap_dsc_t*)lv_font->user_data;
	if(font_dsc == NULL)
	{
		SYS_LOG_ERR("null bitmap font for font %p\n", lv_font);
		return false;
	}
	font = font_dsc->font;
	cache = font_dsc->cache;

	if(unicode >= 0x1F300)
	{
		if(bitmap_font_get_max_emoji_num() == 0)
		{
			metric = bitmap_font_get_glyph_dsc(font, cache, 0x20);
		}
		else
		{
			metric = bitmap_font_get_emoji_glyph_dsc(font_dsc->emoji_font, unicode, true);
		}
	}
	else
	{
		metric = bitmap_font_get_glyph_dsc(font, cache, unicode);
	}
	
	if(metric == NULL)
	{
		SYS_LOG_ERR("null metric for glyph 0x%x\n", unicode);
		return false;
	}
	
	dsc_out->adv_w = metric->advance;
	dsc_out->ofs_x = metric->bbx;
	dsc_out->ofs_y = metric->bby - font->descent;
	dsc_out->box_w = metric->bbw;
	dsc_out->box_h = metric->bbh;
	dsc_out->bpp = font->bpp;

//	SYS_LOG_INF("dsc out %d x %d y %d w %d h %d\n\n", dsc_out->adv_w, dsc_out->ofs_x, dsc_out->ofs_y, dsc_out->box_w, dsc_out->box_h);
	return true;

}

const uint8_t * bitmap_font_get_bitmap_cb(const lv_font_t * lv_font, uint32_t unicode)
{
	uint8_t* data;
	lv_font_fmt_bitmap_dsc_t* font_dsc;
	bitmap_font_t* font;
	bitmap_cache_t* cache;

	if(lv_font == NULL)
	{
		SYS_LOG_ERR("null font info, %p\n", lv_font);
		return NULL;
	}
	font_dsc = (lv_font_fmt_bitmap_dsc_t*)lv_font->user_data;
	if(font_dsc == NULL)
	{
		SYS_LOG_ERR("null bitmap font for font %p\n", lv_font);
		return false;
	}	
	font = font_dsc->font;
	cache = font_dsc->cache;

	if(unicode >= 0x1F300)
	{
		if(bitmap_font_get_max_emoji_num() == 0)
		{
			data = bitmap_font_get_bitmap(font, cache, 0x20);
		}
		else
		{
			data = bitmap_font_get_emoji_bitmap(font_dsc->emoji_font, unicode);
		}
	}
	else
	{
		data = bitmap_font_get_bitmap(font, cache, unicode);
	}
	return data;
}

int lvgl_bitmap_font_init(const char *def_font_path)
{
	bitmap_font_init();


	if(bitmap_font_get_high_freq_enabled()==1)
	{
		if (def_font_path) 
		{
			bitmap_font_load_high_freq_chars(def_font_path);
		}
	}
	return 0;
}

int lvgl_bitmap_font_deinit(void)
{
	bitmap_font_deinit();
	return 0;
}

int lvgl_bitmap_font_set_emoji_font(lv_font_t* lv_font, const char* emoji_font_path)
{
	lv_font_fmt_bitmap_dsc_t * dsc = (lv_font_fmt_bitmap_dsc_t*)lv_font->user_data;

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

int lvgl_bitmap_font_get_emoji_dsc(const lv_font_t* lv_font, uint32_t unicode, lv_img_dsc_t* dsc, lv_point_t* pos, bool force_retrieve)
{
	lv_font_fmt_bitmap_dsc_t* font_dsc;
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

	font_dsc = (lv_font_fmt_bitmap_dsc_t*)lv_font->user_data;
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

int lvgl_bitmap_font_set_default_code(lv_font_t* font, uint32_t word_code, uint32_t emoji_code)
{
	lv_font_fmt_bitmap_dsc_t* font_dsc;

	if(font == NULL)
	{
		SYS_LOG_ERR("null font");
		return -1;
	}
	
	font_dsc = (lv_font_fmt_bitmap_dsc_t*)font->user_data;
	if(font_dsc == NULL)
	{
		SYS_LOG_ERR("cant set default code to an unopend font %p", font);
		return -1;
	}

	bitmap_font_set_default_code(font_dsc->font, word_code);

	if(bitmap_font_get_max_emoji_num() > 0)
	{
		bitmap_font_set_default_emoji_code(font_dsc->emoji_font, emoji_code);
	}

	return 0;
}

int lvgl_bitmap_font_set_default_bitmap(lv_font_t* font, uint8_t* bitmap, uint32_t width, uint32_t height, uint32_t gap, uint32_t bpp)
{
	lv_font_fmt_bitmap_dsc_t* font_dsc;

	if(font == NULL)
	{
		SYS_LOG_ERR("null font");
		return -1;
	}
	
	font_dsc = (lv_font_fmt_bitmap_dsc_t*)font->user_data;
	if(font_dsc == NULL)
	{
		SYS_LOG_ERR("cant set default code to an unopend font %p", font);
		return -1;
	}

	bitmap_font_set_default_bitmap(font_dsc->font, bitmap, width, height, gap, bpp);

	return 0;

}

int lvgl_bitmap_font_cache_preset(bitmap_font_cache_preset_t* preset, int count)
{
    return bitmap_font_cache_preset(preset, count);
}

int lvgl_bitmap_font_open(lv_font_t* font, const char * font_path)
{
	lv_font_fmt_bitmap_dsc_t * dsc;

	if(font == NULL)
	{
		SYS_LOG_ERR("null font pointer");
		return -1;
	}

	if(font_path == NULL)
	{
		SYS_LOG_ERR("null path\n");
		return -1;
	}

	dsc = mem_malloc(sizeof(lv_font_fmt_bitmap_dsc_t));
	if(dsc == NULL)
	{
		SYS_LOG_ERR("malloc font dsc data failed\n");
		return -1;
	}
	
	memset(dsc, 0, sizeof(lv_font_fmt_bitmap_dsc_t));

	dsc->font = bitmap_font_open(font_path);
	if(dsc->font == NULL)
	{
		SYS_LOG_ERR("open bitmap font failed\n");
		goto ERR_EXIT;
	}
	dsc->cache = bitmap_font_get_cache(dsc->font);

	font->get_glyph_dsc = bitmap_font_get_glyph_dsc_cb;        /*Set a callback to get info about gylphs*/
	font->get_glyph_bitmap = bitmap_font_get_bitmap_cb;		/*Set a callback to get bitmap of gylphs*/

	font->user_data = dsc;
	
	if(dsc->font->ascent <= dsc->font->font_size)
	{
		font->line_height = dsc->font->ascent - dsc->font->descent;
		font->base_line = 0; //(dsc->font->ascent);  //Base line measured from the top of line_height
	}
	else
	{
		font->line_height = dsc->font->ascent;
		font->base_line = dsc->font->descent/2; //(dsc->font->ascent);  /*Base line measured from the top of line_height*/
	}
	font->subpx = LV_FONT_SUBPX_NONE;
	return 0;

ERR_EXIT:
	if(dsc->font != NULL)
	{
		bitmap_font_close(dsc->font);
	}

	mem_free(dsc);
	return -1;
}

void lvgl_bitmap_font_close(lv_font_t* font)
{
	lv_font_fmt_bitmap_dsc_t * dsc;

	if(font == NULL)
	{
		SYS_LOG_ERR("null font pointer\n");
		return;
	}

	dsc = (lv_font_fmt_bitmap_dsc_t*)font->user_data;
	if(dsc == NULL)
	{
		SYS_LOG_ERR("null font dsc pointer for font %p\n", font);
		return;
	}
	bitmap_font_close(dsc->font);
	if(dsc->emoji_font != NULL)
	{
		bitmap_emoji_font_close(dsc->emoji_font);
	}
	mem_free(dsc);
	font->user_data = NULL;
}


