/*******************************************************************************
 * Size: 18 px
 * Bpp: 1
 * Opts: --bpp 1 --size 18 --no-compress --stride 1 --align 1 --font SourceCodePro-VariableFont_wght.ttf --range 32 --format lvgl -o LV_FONT_SOURCE_CODE_PRO_18.c
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



#ifndef LV_FONT_SOURCE_CODE_PRO_18
#define LV_FONT_SOURCE_CODE_PRO_18 1
#endif

#if LV_FONT_SOURCE_CODE_PRO_18
/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xaa, 0xaa, 0xf,

    /* U+0022 "\"" */
    0x8c, 0x63, 0x18, 0xc4,

    /* U+0023 "#" */
    0x24, 0x49, 0x17, 0xf4, 0x49, 0x12, 0x7f, 0x48,
    0x91, 0x24, 0x40,

    /* U+0024 "$" */
    0x10, 0x20, 0xe6, 0x28, 0x10, 0x10, 0x1c, 0x4,
    0x4, 0xc, 0x27, 0x82, 0x4, 0x0,

    /* U+0025 "%" */
    0x70, 0x22, 0x28, 0x92, 0x28, 0x90, 0xc, 0x0,
    0x38, 0x52, 0x24, 0x51, 0x18, 0x44, 0xe,

    /* U+0026 "&" */
    0x18, 0x12, 0x9, 0x5, 0x3, 0x1, 0x81, 0x43,
    0x12, 0x8d, 0x43, 0x20, 0x8f, 0xb0,

    /* U+0027 "'" */
    0xfc,

    /* U+0028 "(" */
    0x11, 0x10, 0x88, 0x42, 0x10, 0x84, 0x21, 0x4,
    0x10, 0x82,

    /* U+0029 ")" */
    0x41, 0x8, 0x21, 0x4, 0x21, 0x8, 0x42, 0x21,
    0x11, 0x10,

    /* U+002A "*" */
    0x10, 0x20, 0x47, 0xf1, 0x5, 0x11, 0x0,

    /* U+002B "+" */
    0x10, 0x20, 0x40, 0x8f, 0xe2, 0x4, 0x8,

    /* U+002C "," */
    0x6c, 0x95, 0x0,

    /* U+002D "-" */
    0xfe,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x4, 0x8, 0x10, 0x40, 0x81, 0x4, 0x8, 0x10,
    0x40, 0x82, 0x4, 0x8, 0x20, 0x40,

    /* U+0030 "0" */
    0x3c, 0x42, 0x82, 0x81, 0x81, 0x99, 0x99, 0x81,
    0x81, 0x82, 0x42, 0x3c,

    /* U+0031 "1" */
    0x30, 0xa0, 0x40, 0x81, 0x2, 0x4, 0x8, 0x10,
    0x20, 0x47, 0xf0,

    /* U+0032 "2" */
    0x79, 0x8, 0x8, 0x10, 0x20, 0x82, 0x4, 0x10,
    0x41, 0x7, 0xe0,

    /* U+0033 "3" */
    0x3c, 0x3, 0x1, 0x1, 0x2, 0x1c, 0x6, 0x1,
    0x1, 0x1, 0x82, 0x7c,

    /* U+0034 "4" */
    0x2, 0x3, 0x2, 0x81, 0x41, 0x21, 0x11, 0x8,
    0x84, 0xff, 0x81, 0x0, 0x80, 0x40,

    /* U+0035 "5" */
    0x7e, 0x40, 0x80, 0x80, 0xbc, 0x42, 0x1, 0x1,
    0x1, 0x1, 0x82, 0x7c,

    /* U+0036 "6" */
    0x1e, 0x60, 0x40, 0x80, 0x98, 0xe2, 0x81, 0x81,
    0x81, 0x81, 0x42, 0x3c,

    /* U+0037 "7" */
    0xff, 0x2, 0x2, 0x4, 0x8, 0x8, 0x8, 0x10,
    0x10, 0x10, 0x10, 0x10,

    /* U+0038 "8" */
    0x1c, 0x31, 0x10, 0x48, 0x24, 0x21, 0x91, 0xb9,
    0x2, 0x80, 0xc0, 0x50, 0x47, 0xc0,

    /* U+0039 "9" */
    0x3c, 0xc2, 0x82, 0x81, 0x81, 0x43, 0x1d, 0x1,
    0x2, 0x2, 0x4, 0x78,

    /* U+003A ":" */
    0xf0, 0x3, 0xc0,

    /* U+003B ";" */
    0x6c, 0x0, 0x3, 0x64, 0xa8,

    /* U+003C "<" */
    0x2, 0x18, 0x43, 0x8, 0x10, 0x18, 0x8, 0xc,
    0x4,

    /* U+003D "=" */
    0xfe, 0x0, 0x0, 0xf, 0xe0,

    /* U+003E ">" */
    0x80, 0x80, 0xc0, 0x40, 0x40, 0x82, 0x18, 0x41,
    0x0,

    /* U+003F "?" */
    0x7a, 0x10, 0x41, 0x8, 0x42, 0x8, 0x0, 0x3,
    0xc,

    /* U+0040 "@" */
    0xe, 0x18, 0x90, 0x48, 0x18, 0xc, 0x1e, 0x33,
    0x21, 0x90, 0xc8, 0xe3, 0x28, 0x4, 0x1, 0x0,
    0x7c,

    /* U+0041 "A" */
    0x8, 0xc, 0x5, 0x4, 0x82, 0x41, 0x11, 0x8,
    0xfc, 0x41, 0x40, 0xa0, 0x50, 0x10,

    /* U+0042 "B" */
    0xf8, 0x84, 0x82, 0x82, 0x84, 0xf8, 0x86, 0x81,
    0x81, 0x81, 0x82, 0xfc,

    /* U+0043 "C" */
    0x1e, 0x10, 0x90, 0x10, 0x8, 0x4, 0x2, 0x1,
    0x0, 0x80, 0x20, 0x8, 0x43, 0xc0,

    /* U+0044 "D" */
    0xf8, 0x84, 0x82, 0x82, 0x81, 0x81, 0x81, 0x81,
    0x82, 0x82, 0x84, 0xf8,

    /* U+0045 "E" */
    0xff, 0x2, 0x4, 0x8, 0x1f, 0xa0, 0x40, 0x81,
    0x2, 0x7, 0xf0,

    /* U+0046 "F" */
    0xff, 0x2, 0x4, 0x8, 0x1f, 0xa0, 0x40, 0x81,
    0x2, 0x4, 0x0,

    /* U+0047 "G" */
    0x1f, 0x10, 0x50, 0x10, 0x8, 0x4, 0x2, 0x1f,
    0x1, 0x80, 0xa0, 0x48, 0x23, 0xe0,

    /* U+0048 "H" */
    0x81, 0x81, 0x81, 0x81, 0x81, 0xff, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81,

    /* U+0049 "I" */
    0xfe, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8, 0x10,
    0x20, 0x47, 0xf0,

    /* U+004A "J" */
    0x7e, 0x4, 0x8, 0x10, 0x20, 0x40, 0x81, 0x2,
    0x6, 0x13, 0xc0,

    /* U+004B "K" */
    0x82, 0x84, 0x88, 0x88, 0x90, 0xb0, 0xc8, 0x84,
    0x84, 0x82, 0x82, 0x81,

    /* U+004C "L" */
    0x81, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x81,
    0x2, 0x7, 0xf0,

    /* U+004D "M" */
    0x81, 0xc3, 0xc3, 0xc5, 0xa5, 0xa5, 0xa9, 0x99,
    0x81, 0x81, 0x81, 0x81,

    /* U+004E "N" */
    0x81, 0xc1, 0xc1, 0xa1, 0xa1, 0x91, 0x89, 0x89,
    0x85, 0x85, 0x83, 0x81,

    /* U+004F "O" */
    0x1c, 0x31, 0x10, 0x50, 0x28, 0xc, 0x6, 0x3,
    0x1, 0x81, 0x20, 0x90, 0x87, 0x80,

    /* U+0050 "P" */
    0xfc, 0x82, 0x81, 0x81, 0x81, 0x82, 0xfc, 0x80,
    0x80, 0x80, 0x80, 0x80,

    /* U+0051 "Q" */
    0x1c, 0x31, 0x10, 0x50, 0x28, 0xc, 0x6, 0x3,
    0x1, 0x81, 0x20, 0x90, 0x87, 0x80, 0x80, 0x20,
    0xe,

    /* U+0052 "R" */
    0xfc, 0x82, 0x81, 0x81, 0x81, 0x82, 0xfc, 0x88,
    0x84, 0x84, 0x82, 0x81,

    /* U+0053 "S" */
    0x3c, 0xc2, 0x80, 0x80, 0x40, 0x30, 0xc, 0x2,
    0x1, 0x1, 0x82, 0x7c,

    /* U+0054 "T" */
    0xff, 0x84, 0x2, 0x1, 0x0, 0x80, 0x40, 0x20,
    0x10, 0x8, 0x4, 0x2, 0x1, 0x0,

    /* U+0055 "U" */
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x42, 0x3c,

    /* U+0056 "V" */
    0x81, 0x40, 0x90, 0x48, 0x24, 0x21, 0x10, 0x88,
    0x48, 0x34, 0xa, 0x6, 0x1, 0x0,

    /* U+0057 "W" */
    0x80, 0x50, 0xa, 0x1, 0x42, 0x24, 0xc4, 0x98,
    0x92, 0xa2, 0x94, 0x52, 0x8a, 0x31, 0x86, 0x10,
    0x40,

    /* U+0058 "X" */
    0x41, 0x42, 0x22, 0x24, 0x18, 0x8, 0x18, 0x14,
    0x24, 0x22, 0x42, 0x81,

    /* U+0059 "Y" */
    0x80, 0xe0, 0x90, 0x44, 0x42, 0x40, 0xa0, 0x60,
    0x10, 0x8, 0x4, 0x2, 0x1, 0x0,

    /* U+005A "Z" */
    0x7f, 0x2, 0x2, 0x4, 0x8, 0x8, 0x10, 0x20,
    0x20, 0x40, 0x80, 0xff,

    /* U+005B "[" */
    0xf4, 0x21, 0x8, 0x42, 0x10, 0x84, 0x21, 0x8,
    0x42, 0x1f,

    /* U+005C "\\" */
    0x81, 0x1, 0x2, 0x4, 0x4, 0x8, 0x8, 0x10,
    0x20, 0x20, 0x40, 0x80, 0x81, 0x2,

    /* U+005D "]" */
    0x78, 0x42, 0x10, 0x84, 0x21, 0x8, 0x42, 0x10,
    0x84, 0x3f,

    /* U+005E "^" */
    0x10, 0xc2, 0x92, 0x49, 0x18, 0x40,

    /* U+005F "_" */
    0xff, 0x80,

    /* U+0060 "`" */
    0x24,

    /* U+0061 "a" */
    0x3c, 0x42, 0x1, 0x1, 0x3f, 0xc1, 0x81, 0x83,
    0x7d,

    /* U+0062 "b" */
    0x80, 0x80, 0x80, 0x80, 0xbc, 0xc2, 0x81, 0x81,
    0x81, 0x81, 0x82, 0x82, 0xfc,

    /* U+0063 "c" */
    0x1e, 0x61, 0x80, 0x80, 0x80, 0x80, 0x80, 0x41,
    0x3e,

    /* U+0064 "d" */
    0x1, 0x1, 0x1, 0x1, 0x3d, 0x43, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x43, 0x3d,

    /* U+0065 "e" */
    0x1e, 0x30, 0xa0, 0x30, 0x1f, 0xfc, 0x2, 0x0,
    0x80, 0x3f, 0x0,

    /* U+0066 "f" */
    0xe, 0x30, 0x20, 0x20, 0x20, 0x7c, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20,

    /* U+0067 "g" */
    0x3f, 0xa2, 0x10, 0x88, 0x44, 0x43, 0xc1, 0x0,
    0x80, 0x3c, 0x60, 0xe0, 0x30, 0x27, 0xe0,

    /* U+0068 "h" */
    0x80, 0x80, 0x80, 0x80, 0x9e, 0xe2, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81,

    /* U+0069 "i" */
    0x8, 0x41, 0xf0, 0x84, 0x21, 0x8, 0x42, 0x10,

    /* U+006A "j" */
    0x4, 0x10, 0x1f, 0x4, 0x10, 0x41, 0x4, 0x10,
    0x41, 0x4, 0x10, 0x9e,

    /* U+006B "k" */
    0x80, 0x80, 0x80, 0x80, 0x82, 0x84, 0x88, 0x90,
    0xa8, 0xc8, 0x84, 0x82, 0x81,

    /* U+006C "l" */
    0xf0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0xf,

    /* U+006D "m" */
    0xb3, 0x66, 0x62, 0x31, 0x18, 0x8c, 0x46, 0x23,
    0x11, 0x88, 0x80,

    /* U+006E "n" */
    0x9e, 0xe2, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81,

    /* U+006F "o" */
    0x3e, 0x20, 0xa0, 0x50, 0x18, 0xc, 0x6, 0x4,
    0x82, 0x3e, 0x0,

    /* U+0070 "p" */
    0xbc, 0xc2, 0x81, 0x81, 0x81, 0x81, 0x82, 0x82,
    0xfc, 0x80, 0x80, 0x80, 0x80,

    /* U+0071 "q" */
    0x3d, 0x43, 0x81, 0x81, 0x81, 0x81, 0x81, 0x43,
    0x3d, 0x1, 0x1, 0x1, 0x1,

    /* U+0072 "r" */
    0x9f, 0xc2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,

    /* U+0073 "s" */
    0x7c, 0x82, 0x80, 0x40, 0x2c, 0x2, 0x1, 0x1,
    0xfe,

    /* U+0074 "t" */
    0x20, 0x20, 0x20, 0x7e, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x1f,

    /* U+0075 "u" */
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x83,
    0x7d,

    /* U+0076 "v" */
    0x81, 0x40, 0x90, 0x48, 0x42, 0x21, 0x20, 0x50,
    0x30, 0x8, 0x0,

    /* U+0077 "w" */
    0x84, 0x51, 0x8a, 0x31, 0x25, 0x25, 0x28, 0xa5,
    0x14, 0xa1, 0x8c, 0x21, 0x0,

    /* U+0078 "x" */
    0x82, 0x89, 0x21, 0x81, 0x5, 0x12, 0x42, 0x82,

    /* U+0079 "y" */
    0x81, 0x40, 0x90, 0x48, 0x42, 0x21, 0x20, 0x50,
    0x28, 0x8, 0x4, 0x4, 0x2, 0xe, 0x0,

    /* U+007A "z" */
    0x7f, 0x2, 0x4, 0x8, 0x10, 0x20, 0x20, 0x40,
    0xff,

    /* U+007B "{" */
    0x18, 0x82, 0x8, 0x20, 0x82, 0x30, 0x60, 0x82,
    0x8, 0x20, 0x82, 0x7,

    /* U+007C "|" */
    0xff, 0xff, 0xe0,

    /* U+007D "}" */
    0x60, 0x41, 0x4, 0x10, 0x41, 0x3, 0x18, 0x41,
    0x4, 0x10, 0x41, 0x38,

    /* U+007E "~" */
    0x61, 0x24, 0x30
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 173, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 173, .box_w = 2, .box_h = 12, .ofs_x = 4, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 173, .box_w = 5, .box_h = 6, .ofs_x = 3, .ofs_y = 7},
    {.bitmap_index = 8, .adv_w = 173, .box_w = 7, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 19, .adv_w = 173, .box_w = 7, .box_h = 15, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 33, .adv_w = 173, .box_w = 10, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 173, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 62, .adv_w = 173, .box_w = 1, .box_h = 6, .ofs_x = 5, .ofs_y = 7},
    {.bitmap_index = 63, .adv_w = 173, .box_w = 5, .box_h = 16, .ofs_x = 4, .ofs_y = -3},
    {.bitmap_index = 73, .adv_w = 173, .box_w = 5, .box_h = 16, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 83, .adv_w = 173, .box_w = 7, .box_h = 8, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 90, .adv_w = 173, .box_w = 7, .box_h = 8, .ofs_x = 2, .ofs_y = 3},
    {.bitmap_index = 97, .adv_w = 173, .box_w = 3, .box_h = 6, .ofs_x = 4, .ofs_y = -4},
    {.bitmap_index = 100, .adv_w = 173, .box_w = 7, .box_h = 1, .ofs_x = 2, .ofs_y = 6},
    {.bitmap_index = 101, .adv_w = 173, .box_w = 2, .box_h = 2, .ofs_x = 4, .ofs_y = 0},
    {.bitmap_index = 102, .adv_w = 173, .box_w = 7, .box_h = 16, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 116, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 128, .adv_w = 173, .box_w = 7, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 173, .box_w = 7, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 150, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 162, .adv_w = 173, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 212, .adv_w = 173, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 226, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 238, .adv_w = 173, .box_w = 2, .box_h = 9, .ofs_x = 4, .ofs_y = 0},
    {.bitmap_index = 241, .adv_w = 173, .box_w = 3, .box_h = 13, .ofs_x = 3, .ofs_y = -4},
    {.bitmap_index = 246, .adv_w = 173, .box_w = 7, .box_h = 10, .ofs_x = 2, .ofs_y = 1},
    {.bitmap_index = 255, .adv_w = 173, .box_w = 7, .box_h = 5, .ofs_x = 2, .ofs_y = 4},
    {.bitmap_index = 260, .adv_w = 173, .box_w = 7, .box_h = 10, .ofs_x = 2, .ofs_y = 1},
    {.bitmap_index = 269, .adv_w = 173, .box_w = 6, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 278, .adv_w = 173, .box_w = 9, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 295, .adv_w = 173, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 309, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 321, .adv_w = 173, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 335, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 347, .adv_w = 173, .box_w = 7, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 358, .adv_w = 173, .box_w = 7, .box_h = 12, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 369, .adv_w = 173, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 383, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 395, .adv_w = 173, .box_w = 7, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 406, .adv_w = 173, .box_w = 7, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 417, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 429, .adv_w = 173, .box_w = 7, .box_h = 12, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 440, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 452, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 464, .adv_w = 173, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 478, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 490, .adv_w = 173, .box_w = 9, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 507, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 519, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 531, .adv_w = 173, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 545, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 557, .adv_w = 173, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 571, .adv_w = 173, .box_w = 11, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 588, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 600, .adv_w = 173, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 614, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 626, .adv_w = 173, .box_w = 5, .box_h = 16, .ofs_x = 4, .ofs_y = -3},
    {.bitmap_index = 636, .adv_w = 173, .box_w = 7, .box_h = 16, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 650, .adv_w = 173, .box_w = 5, .box_h = 16, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 660, .adv_w = 173, .box_w = 6, .box_h = 7, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 666, .adv_w = 173, .box_w = 9, .box_h = 1, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 668, .adv_w = 173, .box_w = 2, .box_h = 3, .ofs_x = 4, .ofs_y = 11},
    {.bitmap_index = 669, .adv_w = 173, .box_w = 8, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 678, .adv_w = 173, .box_w = 8, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 691, .adv_w = 173, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 700, .adv_w = 173, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 713, .adv_w = 173, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 724, .adv_w = 173, .box_w = 8, .box_h = 14, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 738, .adv_w = 173, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 753, .adv_w = 173, .box_w = 8, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 766, .adv_w = 173, .box_w = 5, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 774, .adv_w = 173, .box_w = 6, .box_h = 16, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 786, .adv_w = 173, .box_w = 8, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 799, .adv_w = 173, .box_w = 8, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 812, .adv_w = 173, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 823, .adv_w = 173, .box_w = 8, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 832, .adv_w = 173, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 843, .adv_w = 173, .box_w = 8, .box_h = 13, .ofs_x = 2, .ofs_y = -4},
    {.bitmap_index = 856, .adv_w = 173, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 869, .adv_w = 173, .box_w = 7, .box_h = 9, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 877, .adv_w = 173, .box_w = 8, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 886, .adv_w = 173, .box_w = 8, .box_h = 12, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 898, .adv_w = 173, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 907, .adv_w = 173, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 918, .adv_w = 173, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 931, .adv_w = 173, .box_w = 7, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 939, .adv_w = 173, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 954, .adv_w = 173, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 963, .adv_w = 173, .box_w = 6, .box_h = 16, .ofs_x = 3, .ofs_y = -3},
    {.bitmap_index = 975, .adv_w = 173, .box_w = 1, .box_h = 19, .ofs_x = 5, .ofs_y = -5},
    {.bitmap_index = 978, .adv_w = 173, .box_w = 6, .box_h = 16, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 990, .adv_w = 173, .box_w = 7, .box_h = 3, .ofs_x = 2, .ofs_y = 5}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
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
const lv_font_t lv_font_source_code_pro_18 = {
#else
lv_font_t LV_FONT_SOURCE_CODE_PRO_18 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 19,          /*The maximum line height required by the font*/
    .base_line = 5,             /*Baseline measured from the bottom of the line*/
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

#endif /*#if LV_FONT_SOURCE_CODE_PRO_18*/