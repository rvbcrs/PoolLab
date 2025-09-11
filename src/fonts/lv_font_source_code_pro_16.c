/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --bpp 1 --size 16 --no-compress --stride 1 --align 1 --font SourceCodePro-VariableFont_wght.ttf --range 500 --format lvgl -o Source Code Pro.c
 ******************************************************************************/

 #ifdef __has_include
 #if __has_include("lvgl.h")
     #ifndef LV_LVGL_H_INCLUDE_SIMPLE
         #define LV_LVGL_H_INCLUDE_SIMPLE
     #endif
 #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
 #include "lvgl.h"
#else
 #include "lvgl/lvgl.h"
#endif



#ifndef LV_FONT_SOURCE_CODE_PRO_16
#define LV_FONT_SOURCE_CODE_PRO_16 1
#endif

#if LV_FONT_SOURCE_CODE_PRO_16

/*-----------------
*    BITMAPS
*----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
 /* U+01F4 "Ǵ" */
 0x0, 0xc, 0x0, 0x1e, 0x60, 0x80, 0x80, 0x80,
 0x87, 0x81, 0x81, 0x81, 0x41, 0x3e
};


/*---------------------
*  GLYPH DESCRIPTION
*--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
 {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
 {.bitmap_index = 0, .adv_w = 154, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0}
};

/*---------------------
*  CHARACTER MAPPING
*--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
 {
     .range_start = 500, .range_length = 1, .glyph_id_start = 1,
     .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
 }
};



/*--------------------
*  ALL CUSTOM DATA
*--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
 .glyph_bitmap = glyph_bitmap,
 .glyph_dsc = glyph_dsc,
 .cmaps = cmaps,
 .kern_dsc = NULL,
 .kern_scale = 0,
 .cmap_num = 1,
 .bpp = 1,
 .kern_classes = 0,
 .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
 .cache = &cache
#endif

};



/*-----------------
*  PUBLIC FONT
*----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lv_font_source_code_pro_16 = {
#else
lv_font_t lv_font_source_code_pro_16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 14,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /* LV_FONT_SOURCE_CODE_PRO_16 */
