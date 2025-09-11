/*******************************************************************************
 * Size: 12 px
 * Bpp: 1
 * Opts: --bpp 1 --size 12 --no-compress --stride 1 --align 1 --font SourceCodePro-VariableFont_wght.ttf --range 200-700 --format lvgl -o LV_FONT_SOURCE_CODE_PRO_12.c
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



#ifndef LV_FONT_SOURCE_CODE_PRO_12
#define LV_FONT_SOURCE_CODE_PRO_12 1
#endif

#if LV_FONT_SOURCE_CODE_PRO_12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+00C8 "È" */
    0x20, 0x3d, 0x8, 0x7a, 0x10, 0x87, 0x80,

    /* U+00C9 "É" */
    0x10, 0x3d, 0x8, 0x7a, 0x10, 0x87, 0x80,

    /* U+00CA "Ê" */
    0x1, 0x81, 0xe8, 0x43, 0xd0, 0x84, 0x3c,

    /* U+00CB "Ë" */
    0x20, 0x3d, 0x8, 0x7a, 0x10, 0x87, 0x80,

    /* U+00CC "Ì" */
    0x1, 0x1c, 0x42, 0x10, 0x84, 0x23, 0x80,

    /* U+00CD "Í" */
    0x0, 0x1c, 0x42, 0x10, 0x84, 0x23, 0x80,

    /* U+00CE "Î" */
    0x22, 0x80, 0xe2, 0x10, 0x84, 0x21, 0x1c,

    /* U+00CF "Ï" */
    0x50, 0x1c, 0x42, 0x10, 0x84, 0x23, 0x80,

    /* U+00D0 "Ð" */
    0x78, 0x89, 0xf, 0x94, 0x28, 0x51, 0x3c,

    /* U+00D1 "Ñ" */
    0x20, 0x60, 0x21, 0xc7, 0x1a, 0x65, 0x96, 0x38,
    0x40,

    /* U+00D2 "Ò" */
    0x20, 0x3, 0x32, 0x86, 0x18, 0x61, 0x89, 0xc0,

    /* U+00D3 "Ó" */
    0x10, 0x3, 0x32, 0x86, 0x18, 0x61, 0x89, 0xc0,

    /* U+00D4 "Ô" */
    0x0, 0xc0, 0xc, 0xca, 0x18, 0x61, 0x86, 0x27,
    0x0,

    /* U+00D5 "Õ" */
    0x29, 0x40, 0xc, 0xca, 0x18, 0x61, 0x86, 0x27,
    0x0,

    /* U+00D6 "Ö" */
    0x28, 0x3, 0x32, 0x86, 0x18, 0x61, 0x89, 0xc0,

    /* U+00D7 "×" */
    0x13, 0x18, 0x20,

    /* U+00D8 "Ø" */
    0x33, 0x29, 0x69, 0xa7, 0x18, 0xbc,

    /* U+00D9 "Ù" */
    0x20, 0x8, 0x61, 0x86, 0x18, 0x61, 0x89, 0xe0,

    /* U+00DA "Ú" */
    0x10, 0x8, 0x61, 0x86, 0x18, 0x61, 0x89, 0xe0,

    /* U+00DB "Û" */
    0x0, 0xc0, 0x21, 0x86, 0x18, 0x61, 0x86, 0x27,
    0x80,

    /* U+00DC "Ü" */
    0x50, 0x8, 0x61, 0x86, 0x18, 0x61, 0x89, 0xe0,

    /* U+00DD "Ý" */
    0x0, 0x1, 0x19, 0x29, 0x84, 0x21, 0x0,

    /* U+00DE "Þ" */
    0x87, 0x27, 0x18, 0xfa, 0x10,

    /* U+00DF "ß" */
    0x33, 0x29, 0x28, 0xa2, 0x48, 0xa1, 0xb8,

    /* U+00E0 "à" */
    0x1, 0x0, 0xe0, 0x8f, 0xb1, 0xf8,

    /* U+00E1 "á" */
    0x11, 0x0, 0xe0, 0x8f, 0xb1, 0xf8,

    /* U+00E2 "â" */
    0x20, 0x80, 0xe0, 0x8f, 0xb1, 0xf8,

    /* U+00E3 "ã" */
    0x60, 0x80, 0xe0, 0x8f, 0xb1, 0xf8,

    /* U+00E4 "ä" */
    0x50, 0x1c, 0x11, 0xf6, 0x3f,

    /* U+00E5 "å" */
    0x1, 0x80, 0x7, 0x4, 0x7d, 0x8f, 0xc0,

    /* U+00E6 "æ" */
    0x2e, 0x11, 0x3f, 0xd0, 0x90, 0xef,

    /* U+00E7 "ç" */
    0x3b, 0x8, 0x20, 0x81, 0xa1, 0x4, 0x0,

    /* U+00E8 "è" */
    0x20, 0x40, 0xe, 0xc7, 0xf8, 0x20, 0x78,

    /* U+00E9 "é" */
    0x10, 0x0, 0xe, 0xc7, 0xf8, 0x20, 0x78,

    /* U+00EA "ê" */
    0x10, 0xa0, 0xe, 0xc7, 0xf8, 0x20, 0x78,

    /* U+00EB "ë" */
    0x50, 0x3, 0xb1, 0xfe, 0x8, 0x1e,

    /* U+00EC "ì" */
    0x1, 0x7, 0x11, 0x11, 0x10,

    /* U+00ED "í" */
    0x0, 0x80, 0xe1, 0x8, 0x42, 0x10,

    /* U+00EE "î" */
    0x30, 0x40, 0xe1, 0x8, 0x42, 0x10,

    /* U+00EF "ï" */
    0x28, 0x1c, 0x21, 0x8, 0x42,

    /* U+00F0 "ð" */
    0x20, 0xc0, 0x8e, 0xc6, 0x18, 0x62, 0x78,

    /* U+00F1 "ñ" */
    0x60, 0x81, 0xe8, 0xc6, 0x31, 0x88,

    /* U+00F2 "ò" */
    0x20, 0x0, 0x1e, 0x8a, 0x18, 0x62, 0x78,

    /* U+00F3 "ó" */
    0x10, 0x0, 0x1e, 0x8a, 0x18, 0x62, 0x78,

    /* U+00F4 "ô" */
    0x30, 0x0, 0x1e, 0x8a, 0x18, 0x62, 0x78,

    /* U+00F5 "õ" */
    0x60, 0x40, 0x1e, 0x8a, 0x18, 0x62, 0x78,

    /* U+00F6 "ö" */
    0x50, 0x7, 0xa2, 0x86, 0x18, 0x9e,

    /* U+00F7 "÷" */
    0x20, 0x3e, 0x2, 0x0,

    /* U+00F8 "ø" */
    0x7a, 0x29, 0x69, 0x49, 0xe0,

    /* U+00F9 "ù" */
    0x1, 0x1, 0x18, 0xc6, 0x31, 0xf8,

    /* U+00FA "ú" */
    0x0, 0x1, 0x18, 0xc6, 0x31, 0xf8,

    /* U+00FB "û" */
    0x62, 0x81, 0x18, 0xc6, 0x31, 0xf8,

    /* U+00FC "ü" */
    0x50, 0x23, 0x18, 0xc6, 0x3f,

    /* U+00FD "ý" */
    0x0, 0x0, 0x8, 0xc9, 0x4a, 0x21, 0x10, 0x80,

    /* U+00FE "þ" */
    0x82, 0x8, 0x3e, 0x8a, 0x18, 0x62, 0xd2, 0x88,
    0x20,

    /* U+00FF "ÿ" */
    0x50, 0x1, 0x19, 0x29, 0x44, 0x22, 0x10,

    /* U+0100 "Ā" */
    0x70, 0x2, 0x18, 0x51, 0x47, 0x22, 0x88, 0x0,

    /* U+0101 "ā" */
    0x20, 0x1c, 0x11, 0xf6, 0x3f,

    /* U+0102 "Ă" */
    0x0, 0x82, 0x18, 0x51, 0x47, 0x22, 0x88, 0x0,

    /* U+0103 "ă" */
    0x41, 0x0, 0xe0, 0x8f, 0xb1, 0xf8,

    /* U+0104 "Ą" */
    0x21, 0x85, 0x14, 0x89, 0xe8, 0x81, 0x8, 0x20,
    0x80,

    /* U+0105 "ą" */
    0x70, 0x47, 0xd8, 0xcd, 0xc2, 0x10,

    /* U+0106 "Ć" */
    0x10, 0x3, 0xb0, 0x82, 0x8, 0x20, 0x81, 0xe0,

    /* U+0107 "ć" */
    0x10, 0x0, 0xe, 0xc2, 0x8, 0x20, 0x78,

    /* U+0108 "Ĉ" */
    0x10, 0x80, 0x8e, 0xc2, 0x8, 0x20, 0x82, 0x7,
    0x80,

    /* U+0109 "ĉ" */
    0x30, 0x20, 0xe, 0xc2, 0x8, 0x20, 0x78,

    /* U+010A "Ċ" */
    0x0, 0x3, 0xb0, 0x82, 0x8, 0x20, 0x81, 0xe0,

    /* U+010B "ċ" */
    0x10, 0x3, 0xb0, 0x82, 0x8, 0x1e,

    /* U+010C "Č" */
    0x0, 0xc0, 0xe, 0xc2, 0x8, 0x20, 0x82, 0x7,
    0x80,

    /* U+010D "č" */
    0x8, 0xc0, 0xe, 0xc2, 0x8, 0x20, 0x78,

    /* U+010E "Ď" */
    0x10, 0x8f, 0x22, 0x86, 0x18, 0x61, 0x8b, 0xc0,

    /* U+010F "ď" */
    0x2, 0xc, 0x18, 0x23, 0xd8, 0xa1, 0x42, 0x84,
    0xf8,

    /* U+0110 "Đ" */
    0x78, 0x89, 0xf, 0x94, 0x28, 0x51, 0x3c,

    /* U+0111 "đ" */
    0x4, 0x3c, 0x11, 0xec, 0x50, 0xa1, 0x42, 0x7c,

    /* U+0112 "Ē" */
    0x70, 0x3d, 0x8, 0x7a, 0x10, 0x87, 0x80,

    /* U+0113 "ē" */
    0x10, 0x3, 0xb1, 0xfe, 0x8, 0x1e,

    /* U+0114 "Ĕ" */
    0x41, 0x35, 0x8, 0x7a, 0x10, 0x87, 0x80,

    /* U+0115 "ĕ" */
    0x8, 0x40, 0xe, 0xc7, 0xf8, 0x20, 0x78,

    /* U+0116 "Ė" */
    0x0, 0x3d, 0x8, 0x7a, 0x10, 0x87, 0x80,

    /* U+0117 "ė" */
    0x20, 0x3, 0xb1, 0xfe, 0x8, 0x1e,

    /* U+0118 "Ę" */
    0xf4, 0x21, 0xe8, 0x42, 0x10, 0x78, 0x84,

    /* U+0119 "ę" */
    0x3b, 0x28, 0x7f, 0x81, 0xa1, 0x4, 0x10,

    /* U+011A "Ě" */
    0x1, 0x3d, 0x8, 0x7a, 0x10, 0x87, 0x80,

    /* U+011B "ě" */
    0x8, 0xc0, 0xe, 0xc7, 0xf8, 0x20, 0x78,

    /* U+011C "Ĝ" */
    0x10, 0x80, 0xe, 0xc2, 0x8, 0x27, 0x86, 0x17,
    0x80,

    /* U+011D "ĝ" */
    0x21, 0x40, 0x1e, 0x8a, 0x29, 0x38, 0x83, 0xf8,
    0x5e,

    /* U+011E "Ğ" */
    0x0, 0xe0, 0xe, 0xc2, 0x8, 0x27, 0x86, 0x17,
    0x80,

    /* U+011F "ğ" */
    0x1, 0xc0, 0x1e, 0x8a, 0x29, 0x38, 0x83, 0xf8,
    0x5e,

    /* U+0120 "Ġ" */
    0x0, 0x3, 0xb0, 0x82, 0x9, 0xe1, 0x85, 0xe0,

    /* U+0121 "ġ" */
    0x20, 0x7, 0xa2, 0x8a, 0x4e, 0x20, 0xfe, 0x17,
    0x80,

    /* U+0122 "Ģ" */
    0x3b, 0x8, 0x20, 0x9e, 0x18, 0x5a, 0x10, 0x0,
    0x0,

    /* U+0123 "ģ" */
    0x10, 0x80, 0x1e, 0x8a, 0x29, 0x38, 0x83, 0xf8,
    0x5e,

    /* U+0124 "Ĥ" */
    0x0, 0xc0, 0x21, 0x86, 0x1f, 0xe1, 0x86, 0x18,
    0x40,

    /* U+0125 "ĥ" */
    0xc0, 0x4, 0x10, 0x41, 0xe4, 0x51, 0x45, 0x14,
    0x40,

    /* U+0126 "Ħ" */
    0x42, 0xff, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42,

    /* U+0127 "ħ" */
    0x43, 0xc4, 0x1e, 0x45, 0x14, 0x51, 0x44,

    /* U+0128 "Ĩ" */
    0x41, 0x80, 0xe2, 0x10, 0x84, 0x21, 0x1c,

    /* U+0129 "ĩ" */
    0x62, 0xc0, 0xe1, 0x8, 0x42, 0x10,

    /* U+012A "Ī" */
    0x70, 0x1c, 0x42, 0x10, 0x84, 0x23, 0x80,

    /* U+012B "ī" */
    0x10, 0x1c, 0x21, 0x8, 0x42,

    /* U+012C "Ĭ" */
    0x3, 0x9c, 0x42, 0x10, 0x84, 0x23, 0x80,

    /* U+012D "ĭ" */
    0x0, 0xc0, 0xe1, 0x8, 0x42, 0x10,

    /* U+012E "Į" */
    0x71, 0x8, 0x42, 0x10, 0x84, 0x52, 0x1c,

    /* U+012F "į" */
    0x10, 0x71, 0x11, 0x11, 0x22, 0x20,

    /* U+0130 "İ" */
    0x20, 0x1c, 0x42, 0x10, 0x84, 0x23, 0x80,

    /* U+0131 "ı" */
    0x71, 0x11, 0x11,

    /* U+0132 "Ĳ" */
    0x99, 0x99, 0x99, 0x99, 0x12,

    /* U+0133 "ĳ" */
    0x88, 0x23, 0x18, 0xc6, 0x31, 0x8, 0x40,

    /* U+0134 "Ĵ" */
    0x1, 0x80, 0x70, 0x84, 0x21, 0xc, 0x5c,

    /* U+0135 "ĵ" */
    0x30, 0x40, 0xe1, 0x8, 0x42, 0x10, 0x85, 0xc0,

    /* U+0136 "Ķ" */
    0x8a, 0x4a, 0x30, 0x92, 0x48, 0xa0, 0x0, 0x42,
    0x0,

    /* U+0137 "ķ" */
    0x82, 0x8, 0x22, 0x92, 0x8d, 0x22, 0x80, 0x1,
    0x8,

    /* U+0138 "ĸ" */
    0x8a, 0x4a, 0x34, 0x92, 0x20,

    /* U+0139 "Ĺ" */
    0x0, 0x21, 0x8, 0x42, 0x10, 0x87, 0x80,

    /* U+013A "ĺ" */
    0x0, 0x18, 0x42, 0x10, 0x84, 0x21, 0x4,

    /* U+013B "Ļ" */
    0x84, 0x21, 0x8, 0x42, 0x10, 0x70, 0x0,

    /* U+013C "ļ" */
    0x61, 0x8, 0x42, 0x10, 0x84, 0x20, 0x80, 0x0,

    /* U+013D "Ľ" */
    0x14, 0xa5, 0x8, 0x42, 0x10, 0xf0,

    /* U+013E "ľ" */
    0x13, 0x8c, 0x42, 0x10, 0x84, 0x21, 0xc0,

    /* U+013F "Ŀ" */
    0x84, 0x21, 0x28, 0x42, 0x1e,

    /* U+0140 "ŀ" */
    0x60, 0x82, 0x8, 0x20, 0x82, 0x8, 0x10,

    /* U+0141 "Ł" */
    0x41, 0x4, 0x14, 0x61, 0x4, 0x1e,

    /* U+0142 "ł" */
    0x61, 0x8, 0x66, 0x10, 0x84, 0x10,

    /* U+0143 "Ń" */
    0x10, 0x8, 0x71, 0xc6, 0x99, 0x65, 0x8e, 0x10,

    /* U+0144 "ń" */
    0x1, 0x1, 0xe8, 0xc6, 0x31, 0x88,

    /* U+0145 "Ņ" */
    0x87, 0x1c, 0x69, 0x96, 0x58, 0xe1, 0x0, 0x42,
    0x0,

    /* U+0146 "ņ" */
    0xf4, 0x63, 0x18, 0xc4, 0x0, 0x0,

    /* U+0147 "Ň" */
    0x0, 0xc8, 0x71, 0xc6, 0x99, 0x65, 0x8e, 0x10,

    /* U+0148 "ň" */
    0x51, 0x1, 0xe8, 0xc6, 0x31, 0x88,

    /* U+0149 "ŉ" */
    0xc0, 0x81, 0x5, 0xe2, 0x24, 0x48, 0x91, 0x22,

    /* U+014A "Ŋ" */
    0xf2, 0x28, 0x61, 0x86, 0x18, 0xa2,

    /* U+014B "ŋ" */
    0xf4, 0x63, 0x18, 0xc4, 0x21, 0x10,

    /* U+014C "Ō" */
    0x30, 0x3, 0x32, 0x86, 0x18, 0x61, 0x89, 0xc0,

    /* U+014D "ō" */
    0x0, 0x7, 0xa2, 0x86, 0x18, 0x9e,

    /* U+014E "Ŏ" */
    0x0, 0x80, 0xc, 0xca, 0x18, 0x61, 0x86, 0x27,
    0x0,

    /* U+014F "ŏ" */
    0x48, 0xc0, 0x1e, 0x8a, 0x18, 0x62, 0x78,

    /* U+0150 "Ő" */
    0x20, 0x3, 0x32, 0x86, 0x18, 0x61, 0x89, 0xc0,

    /* U+0151 "ő" */
    0x8, 0x80, 0x1e, 0x8a, 0x18, 0x62, 0x78,

    /* U+0152 "Œ" */
    0x3b, 0x49, 0x27, 0x92, 0x49, 0x1e,

    /* U+0153 "œ" */
    0x76, 0x89, 0x8f, 0x88, 0x98, 0x77,

    /* U+0154 "Ŕ" */
    0x0, 0x3d, 0x18, 0xfa, 0x12, 0x94, 0x40,

    /* U+0155 "ŕ" */
    0x1, 0x1, 0x7c, 0x42, 0x10, 0x80,

    /* U+0156 "Ŗ" */
    0xf4, 0x63, 0xe8, 0x4a, 0x51, 0x0, 0x0,

    /* U+0157 "ŗ" */
    0xb6, 0x21, 0x8, 0x40, 0x0, 0x0,

    /* U+0158 "Ř" */
    0x41, 0x35, 0x18, 0xfa, 0x12, 0x94, 0x40,

    /* U+0159 "ř" */
    0x13, 0x1, 0x7c, 0x42, 0x10, 0x80,

    /* U+015A "Ś" */
    0x10, 0x7, 0xa0, 0x81, 0x81, 0x81, 0x5, 0xe0,

    /* U+015B "ś" */
    0x1, 0x1, 0xe8, 0x20, 0xc1, 0x70,

    /* U+015C "Ŝ" */
    0x0, 0xc0, 0x1e, 0x82, 0x6, 0x6, 0x4, 0x17,
    0x80,

    /* U+015D "ŝ" */
    0x22, 0x81, 0xe8, 0x20, 0xc1, 0x70,

    /* U+015E "Ş" */
    0x7a, 0x8, 0x18, 0x18, 0x10, 0x56, 0x20, 0x40,
    0x0,

    /* U+015F "ş" */
    0xf4, 0x10, 0x60, 0xa8, 0x84, 0x0,

    /* U+0160 "Š" */
    0x8, 0xc0, 0x1e, 0x82, 0x6, 0x6, 0x4, 0x17,
    0x80,

    /* U+0161 "š" */
    0x13, 0x1, 0xe8, 0x20, 0xc1, 0x70,

    /* U+0162 "Ţ" */
    0x7c, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8, 0x10,
    0x20, 0x40,

    /* U+0163 "ţ" */
    0x42, 0x1c, 0x84, 0x21, 0x4, 0x10, 0x80,

    /* U+0164 "Ť" */
    0x28, 0x21, 0xf0, 0x81, 0x2, 0x4, 0x8, 0x10,
    0x20,

    /* U+0165 "ť" */
    0x10, 0x94, 0x87, 0x21, 0x8, 0x41, 0xc0,

    /* U+0166 "Ŧ" */
    0x7c, 0x20, 0x41, 0xc1, 0x2, 0x4, 0x8,

    /* U+0167 "ŧ" */
    0x42, 0x1c, 0x8f, 0xa1, 0x7,

    /* U+0168 "Ũ" */
    0x20, 0x40, 0x21, 0x86, 0x18, 0x61, 0x86, 0x27,
    0x80,

    /* U+0169 "ũ" */
    0x41, 0x81, 0x18, 0xc6, 0x31, 0xf8,

    /* U+016A "Ū" */
    0x78, 0x8, 0x61, 0x86, 0x18, 0x61, 0x89, 0xe0,

    /* U+016B "ū" */
    0x20, 0x23, 0x18, 0xc6, 0x3f,

    /* U+016C "Ŭ" */
    0x48, 0xc0, 0x21, 0x86, 0x18, 0x61, 0x86, 0x27,
    0x80,

    /* U+016D "ŭ" */
    0x3, 0x81, 0x18, 0xc6, 0x31, 0xf8,

    /* U+016E "Ů" */
    0x21, 0x42, 0x21, 0x86, 0x18, 0x61, 0x86, 0x27,
    0x80,

    /* U+016F "ů" */
    0x22, 0x88, 0x8, 0xc6, 0x31, 0x8f, 0xc0,

    /* U+0170 "Ű" */
    0x0, 0xa0, 0x21, 0x86, 0x18, 0x61, 0x86, 0x27,
    0x80,

    /* U+0171 "ű" */
    0x20, 0x1, 0x18, 0xc6, 0x31, 0xf8,

    /* U+0172 "Ų" */
    0x86, 0x18, 0x61, 0x86, 0x18, 0x52, 0x30, 0x82,
    0x0,

    /* U+0173 "ų" */
    0x8c, 0x63, 0x18, 0xdd, 0x42, 0x10,

    /* U+0174 "Ŵ" */
    0x10, 0x10, 0x4, 0x8, 0x32, 0x6d, 0x56, 0xac,
    0x99, 0x10,

    /* U+0175 "ŵ" */
    0x30, 0x40, 0x4, 0x8b, 0x36, 0xab, 0x36, 0x44,

    /* U+0176 "Ŷ" */
    0x22, 0x80, 0x8, 0xc9, 0x4c, 0x21, 0x8,

    /* U+0177 "ŷ" */
    0x22, 0x80, 0x8, 0xc9, 0x4a, 0x21, 0x10, 0x80,

    /* U+0178 "Ÿ" */
    0x50, 0x1, 0x19, 0x29, 0x84, 0x21, 0x0,

    /* U+0179 "Ź" */
    0x10, 0x7, 0x82, 0x10, 0x82, 0x10, 0x83, 0xe0,

    /* U+017A "ź" */
    0x10, 0x0, 0xe, 0x18, 0x82, 0x10, 0xf8,

    /* U+017B "Ż" */
    0x20, 0x7, 0x82, 0x10, 0x82, 0x10, 0x83, 0xe0,

    /* U+017C "ż" */
    0x20, 0x3, 0x86, 0x20, 0x84, 0x3e,

    /* U+017D "Ž" */
    0x0, 0xc7, 0x82, 0x10, 0x82, 0x10, 0x83, 0xe0,

    /* U+017E "ž" */
    0x0, 0xc0, 0xe, 0x18, 0x82, 0x10, 0xf8,

    /* U+017F "ſ" */
    0x32, 0x10, 0x84, 0x21, 0x8, 0x40,

    /* U+0180 "ƀ" */
    0x41, 0xf1, 0x3, 0xe4, 0x48, 0x50, 0xa2, 0x78,

    /* U+018A "Ɗ" */
    0x79, 0x4a, 0x8d, 0x16, 0x24, 0x49, 0x1c,

    /* U+018F "Ə" */
    0x70, 0x20, 0x7f, 0x86, 0x18, 0x9c,

    /* U+0192 "ƒ" */
    0x11, 0x8, 0xf2, 0x11, 0x8, 0x44, 0x0,

    /* U+0193 "Ɠ" */
    0x0, 0x13, 0xb0, 0x82, 0x8, 0xe1, 0x85, 0xe0,

    /* U+01A0 "Ơ" */
    0x0, 0x13, 0xb2, 0x86, 0x18, 0x61, 0x89, 0xc0,

    /* U+01A1 "ơ" */
    0x4, 0x17, 0xa2, 0x86, 0x18, 0x9e,

    /* U+01AF "Ư" */
    0x0, 0x8, 0x61, 0x86, 0x18, 0x61, 0x89, 0xe0,

    /* U+01B0 "ư" */
    0x4, 0x18, 0xa2, 0x8a, 0x28, 0xbe,

    /* U+01C2 "ǂ" */
    0x21, 0x8, 0x42, 0x7f, 0xe4, 0x21, 0x8, 0x40,

    /* U+01CD "Ǎ" */
    0x50, 0x82, 0x18, 0x51, 0x47, 0x22, 0x88, 0x0,

    /* U+01CE "ǎ" */
    0x1, 0x80, 0xe0, 0x8f, 0xb1, 0xf8,

    /* U+01CF "Ǐ" */
    0x51, 0x1c, 0x42, 0x10, 0x84, 0x23, 0x80,

    /* U+01D0 "ǐ" */
    0x9, 0x80, 0xe1, 0x8, 0x42, 0x10,

    /* U+01D1 "Ǒ" */
    0x0, 0xc0, 0xc, 0xca, 0x18, 0x61, 0x86, 0x27,
    0x0,

    /* U+01D2 "ǒ" */
    0x0, 0xc0, 0x1e, 0x8a, 0x18, 0x62, 0x78,

    /* U+01D3 "Ǔ" */
    0x48, 0xc0, 0x21, 0x86, 0x18, 0x61, 0x86, 0x27,
    0x80,

    /* U+01D4 "ǔ" */
    0x53, 0x1, 0x18, 0xc6, 0x31, 0xf8,

    /* U+01D5 "Ǖ" */
    0x79, 0x40, 0x21, 0x86, 0x18, 0x61, 0x86, 0x27,
    0x80,

    /* U+01D6 "ǖ" */
    0x70, 0x14, 0x8, 0xc6, 0x31, 0x8f, 0xc0,

    /* U+01D7 "Ǘ" */
    0x0, 0x5, 0x0, 0x86, 0x18, 0x61, 0x86, 0x18,
    0x9e,

    /* U+01D8 "ǘ" */
    0x0, 0x14, 0x8, 0xc6, 0x31, 0x8f, 0xc0,

    /* U+01D9 "Ǚ" */
    0x0, 0xc5, 0x0, 0x86, 0x18, 0x61, 0x86, 0x18,
    0x9e,

    /* U+01DA "ǚ" */
    0x1, 0x14, 0x8, 0xc6, 0x31, 0x8f, 0xc0,

    /* U+01DB "Ǜ" */
    0x0, 0x5, 0x0, 0x86, 0x18, 0x61, 0x86, 0x18,
    0x9e,

    /* U+01DC "ǜ" */
    0x0, 0x14, 0x8, 0xc6, 0x31, 0x8f, 0xc0,

    /* U+01E2 "Ǣ" */
    0xe, 0x0, 0xc, 0x18, 0x28, 0x2e, 0x48, 0x38,
    0x48, 0xe,

    /* U+01E3 "ǣ" */
    0x10, 0x0, 0x2e, 0x11, 0x3f, 0xd0, 0x90, 0xef,

    /* U+01E6 "Ǧ" */
    0x0, 0xc0, 0xe, 0xc2, 0x8, 0x27, 0x86, 0x17,
    0x80,

    /* U+01E7 "ǧ" */
    0x40, 0xc0, 0x1e, 0x8a, 0x29, 0x38, 0x83, 0xf8,
    0x5e,

    /* U+01EA "Ǫ" */
    0x33, 0x28, 0x61, 0x86, 0x18, 0x9e, 0x10, 0x82,
    0x0,

    /* U+01EB "ǫ" */
    0x7a, 0x28, 0x61, 0x89, 0x23, 0x8, 0x20,

    /* U+01F4 "Ǵ" */
    0x0, 0x3, 0xb0, 0x82, 0x9, 0xe1, 0x85, 0xe0,

    /* U+01F5 "ǵ" */
    0x10, 0x80, 0x1e, 0x8a, 0x29, 0x38, 0x83, 0xf8,
    0x5e,

    /* U+01F8 "Ǹ" */
    0x20, 0x8, 0x71, 0xc6, 0x99, 0x65, 0x8e, 0x10,

    /* U+01F9 "ǹ" */
    0x1, 0x1, 0xe8, 0xc6, 0x31, 0x88,

    /* U+01FA "Ǻ" */
    0x10, 0x50, 0x41, 0x83, 0x5, 0x12, 0x1c, 0x45,
    0x8, 0x0,

    /* U+01FB "ǻ" */
    0x1, 0x80, 0x7, 0x4, 0x7d, 0x8f, 0xc0,

    /* U+01FC "Ǽ" */
    0x2, 0x0, 0xc, 0x18, 0x28, 0x2e, 0x48, 0x38,
    0x48, 0xe,

    /* U+01FD "ǽ" */
    0x8, 0x10, 0x0, 0x2e, 0x11, 0x3f, 0xd0, 0x90,
    0xef,

    /* U+01FE "Ǿ" */
    0x10, 0x3, 0x32, 0x96, 0x9a, 0x71, 0x8b, 0xc0,

    /* U+01FF "ǿ" */
    0x10, 0x0, 0x1e, 0x8a, 0x5a, 0x52, 0x78,

    /* U+0218 "Ș" */
    0x7a, 0x8, 0x18, 0x18, 0x10, 0x56, 0x20, 0x40,
    0x0,

    /* U+0219 "ș" */
    0xf4, 0x10, 0x60, 0xa8, 0x80, 0x0,

    /* U+021A "Ț" */
    0x7c, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8, 0x0,
    0x0, 0x40,

    /* U+021B "ț" */
    0x42, 0x1c, 0x84, 0x21, 0x5, 0x10, 0x84,

    /* U+0237 "ȷ" */
    0x71, 0x11, 0x11, 0x11, 0xe0,

    /* U+0243 "Ƀ" */
    0x78, 0x89, 0x13, 0xc4, 0xde, 0x50, 0xbe,

    /* U+0250 "ɐ" */
    0xf4, 0x65, 0xc8, 0x38,

    /* U+0251 "ɑ" */
    0x3f, 0x18, 0x61, 0x85, 0xf0,

    /* U+0252 "ɒ" */
    0xfa, 0x28, 0x61, 0x8b, 0xc0,

    /* U+0253 "ɓ" */
    0x62, 0x8, 0x3e, 0x8a, 0x18, 0x62, 0xf0,

    /* U+0254 "ɔ" */
    0x70, 0x20, 0x41, 0x9, 0xc0,

    /* U+0255 "ɕ" */
    0x3b, 0x8, 0x20, 0x89, 0xe5, 0x0,

    /* U+0256 "ɖ" */
    0x4, 0x8, 0x11, 0xec, 0x50, 0xa1, 0x42, 0x4c,
    0x68, 0x8,

    /* U+0257 "ɗ" */
    0x2, 0x8, 0x11, 0xec, 0x50, 0xa1, 0x42, 0x7c,

    /* U+0258 "ɘ" */
    0x7a, 0x2f, 0xc1, 0x9, 0xc0,

    /* U+0259 "ə" */
    0x78, 0x2f, 0xe1, 0x89, 0xe0,

    /* U+025A "ɚ" */
    0x70, 0x28, 0xae, 0x49, 0xe, 0x0,

    /* U+025B "ɛ" */
    0x74, 0x19, 0x8, 0x38,

    /* U+025C "ɜ" */
    0x70, 0x4c, 0x10, 0xf8,

    /* U+025E "ɞ" */
    0x3b, 0x19, 0xa1, 0x85, 0xe0,

    /* U+025F "ɟ" */
    0x70, 0x41, 0x1f, 0x10, 0x41, 0x4, 0xe0,

    /* U+0260 "ɠ" */
    0x2, 0x8, 0xf6, 0x28, 0x50, 0xa1, 0x26, 0x34,
    0x9, 0xe0,

    /* U+0261 "ɡ" */
    0x3f, 0x18, 0x61, 0x85, 0x33, 0x41, 0x78,

    /* U+0262 "ɢ" */
    0x3b, 0x8, 0x27, 0x85, 0xe0,

    /* U+0263 "ɣ" */
    0x0, 0x89, 0x22, 0x43, 0x6, 0xc, 0x14, 0x38,

    /* U+0264 "ɤ" */
    0x51, 0x35, 0x18, 0xb8,

    /* U+0265 "ɥ" */
    0x8c, 0x63, 0x18, 0xdd, 0x21, 0x8,

    /* U+0266 "ɦ" */
    0x64, 0x21, 0xe8, 0xc6, 0x31, 0x88,

    /* U+0267 "ɧ" */
    0x64, 0x21, 0xe8, 0xc6, 0x31, 0x88, 0x42, 0x20,

    /* U+0268 "ɨ" */
    0x10, 0x7, 0x4, 0x7c, 0x41, 0x4,

    /* U+026A "ɪ" */
    0x71, 0x8, 0x42, 0x38,

    /* U+026B "ɫ" */
    0x61, 0x9, 0x43, 0x10, 0x84, 0x10,

    /* U+026C "ɬ" */
    0x61, 0x9, 0xc7, 0x10, 0x84, 0x10,

    /* U+026D "ɭ" */
    0x61, 0x8, 0x42, 0x10, 0x84, 0x21, 0x6,

    /* U+026E "ɮ" */
    0x82, 0x8, 0x2e, 0x8a, 0x4a, 0x26, 0x84, 0x10,
    0x9e,

    /* U+026F "ɯ" */
    0x93, 0x26, 0x4c, 0x99, 0x3f, 0xc0,

    /* U+0270 "ɰ" */
    0x93, 0x26, 0x4c, 0x99, 0x36, 0xd2, 0x81, 0x2,

    /* U+0271 "ɱ" */
    0xed, 0x26, 0x4c, 0x99, 0x32, 0x40, 0x81, 0x4,

    /* U+0272 "ɲ" */
    0x79, 0x14, 0x51, 0x45, 0x14, 0x10, 0x80,

    /* U+0273 "ɳ" */
    0xf2, 0x28, 0xa2, 0x8a, 0x20, 0x82, 0x4,

    /* U+0274 "ɴ" */
    0x8e, 0x73, 0x59, 0xc4,

    /* U+0275 "ɵ" */
    0x7a, 0x2f, 0xe1, 0x89, 0xe0,

    /* U+0276 "ɶ" */
    0x7a, 0x49, 0xe4, 0x91, 0xe0,

    /* U+0278 "ɸ" */
    0x10, 0x20, 0x41, 0xcd, 0x52, 0x64, 0xca, 0x78,
    0x20, 0x40, 0x80,

    /* U+0279 "ɹ" */
    0x8, 0x42, 0x10, 0xfc,

    /* U+027A "ɺ" */
    0x8, 0x42, 0x10, 0x84, 0x21, 0xf8,

    /* U+027B "ɻ" */
    0x21, 0x8, 0x42, 0x30, 0x82,

    /* U+027D "ɽ" */
    0xbe, 0x21, 0x8, 0x42, 0x10, 0x20,

    /* U+027E "ɾ" */
    0x7c, 0x21, 0x8, 0x40,

    /* U+0280 "ʀ" */
    0xf4, 0x63, 0xe9, 0x40,

    /* U+0281 "ʁ" */
    0x84, 0xbd, 0x18, 0xf8,

    /* U+0282 "ʂ" */
    0x7a, 0x4, 0xe, 0x7, 0x2b, 0x10,

    /* U+0283 "ʃ" */
    0x19, 0x8, 0x42, 0x10, 0x84, 0x21, 0x8, 0x80,

    /* U+0284 "ʄ" */
    0x19, 0x8, 0x42, 0x13, 0xe4, 0x21, 0x8, 0x80,

    /* U+0287 "ʇ" */
    0xe0, 0x84, 0x21, 0x9, 0xc2,

    /* U+0288 "ʈ" */
    0x42, 0x1c, 0x84, 0x21, 0x8, 0x40, 0x80,

    /* U+0289 "ʉ" */
    0x44, 0x8b, 0xfa, 0x24, 0x47, 0x80,

    /* U+028A "ʊ" */
    0x4a, 0x28, 0x61, 0x89, 0xe0,

    /* U+028B "ʋ" */
    0x8a, 0x18, 0x61, 0x89, 0xc0,

    /* U+028C "ʌ" */
    0x23, 0x15, 0x28, 0x80,

    /* U+028D "ʍ" */
    0x44, 0xda, 0xb5, 0xab, 0x32, 0x0,

    /* U+028E "ʎ" */
    0x9, 0x88, 0x46, 0x2a, 0x51, 0x0,

    /* U+028F "ʏ" */
    0x8c, 0x94, 0x42, 0x10,

    /* U+0290 "ʐ" */
    0x38, 0x10, 0x41, 0x4, 0x10, 0x1f, 0x2, 0x2,

    /* U+0291 "ʑ" */
    0x78, 0x42, 0x12, 0x96, 0xa5, 0x0,

    /* U+0292 "ʒ" */
    0x38, 0x41, 0x8, 0x70, 0x20, 0x41, 0x78,

    /* U+0294 "ʔ" */
    0x64, 0x82, 0x11, 0x10, 0x84, 0x20,

    /* U+0295 "ʕ" */
    0x36, 0x61, 0x4, 0x10, 0x84, 0x20,

    /* U+0298 "ʘ" */
    0x33, 0x28, 0xa1, 0xa6, 0x18, 0x62, 0x78,

    /* U+0299 "ʙ" */
    0xf2, 0x2f, 0x23, 0x87, 0xe0,

    /* U+029C "ʜ" */
    0x8c, 0x7f, 0x18, 0xc4,

    /* U+029D "ʝ" */
    0x10, 0xc, 0x21, 0x8, 0x42, 0x13, 0xbc,

    /* U+029E "ʞ" */
    0x45, 0x12, 0xc5, 0x25, 0x10, 0x41, 0x4,

    /* U+029F "ʟ" */
    0x84, 0x21, 0x8, 0x78,

    /* U+02A1 "ʡ" */
    0x64, 0x82, 0x11, 0x7c, 0x84, 0x20,

    /* U+02A2 "ʢ" */
    0x26, 0x61, 0x4, 0x7c, 0x84, 0x20,

    /* U+02A4 "ʤ" */
    0x10, 0x10, 0x10, 0x76, 0x92, 0x94, 0x94, 0x94,
    0x52, 0x21, 0x1, 0xe,

    /* U+02A6 "ʦ" */
    0x40, 0x81, 0xf2, 0x84, 0x88, 0x90, 0x9e,

    /* U+02A7 "ʧ" */
    0x5, 0x24, 0x9a, 0x49, 0x24, 0x92, 0x48, 0xa0,
    0x80,

    /* U+02B0 "ʰ" */
    0x88, 0xe9, 0x99,

    /* U+02B2 "ʲ" */
    0x1, 0x92, 0x4e,

    /* U+02B3 "ʳ" */
    0xd2, 0x40,

    /* U+02B7 "ʷ" */
    0x2f, 0xbc, 0xe0,

    /* U+02B8 "ʸ" */
    0x16, 0xa4, 0x0,

    /* U+02B9 "ʹ" */
    0x60,

    /* U+02BB "ʻ" */
    0xac,

    /* U+02BC "ʼ" */
    0xd8
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 7, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 14, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 21, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 28, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 35, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 42, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 49, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 56, .adv_w = 115, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 72, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 80, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 88, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 106, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 114, .adv_w = 115, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 117, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 123, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 148, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 163, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 168, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 175, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 181, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 187, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 193, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 204, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 211, .adv_w = 115, .box_w = 8, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 217, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 224, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 231, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 238, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 245, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 251, .adv_w = 115, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 256, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 262, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 268, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 273, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 280, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 286, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 293, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 307, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 314, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 320, .adv_w = 115, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 324, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 329, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 335, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 341, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 347, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 352, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 360, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 369, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 376, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 389, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 397, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 403, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 412, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 418, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 426, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 433, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 442, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 449, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 457, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 463, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 472, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 479, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 487, .adv_w = 115, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 496, .adv_w = 115, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 503, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 511, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 518, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 524, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 531, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 538, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 545, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 551, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 558, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 565, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 572, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 579, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 588, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 597, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 606, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 615, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 623, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 632, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 641, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 650, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 659, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 668, .adv_w = 115, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 676, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 683, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 690, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 696, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 703, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 708, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 715, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 721, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 728, .adv_w = 115, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 734, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 741, .adv_w = 115, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 744, .adv_w = 115, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 749, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 756, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 763, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 771, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 780, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 789, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 794, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 801, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 808, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 815, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 823, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 829, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 836, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 841, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 848, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 854, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 860, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 868, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 874, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 883, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 889, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 897, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 903, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 911, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 917, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 923, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 931, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 937, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 946, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 953, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 961, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 968, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 974, .adv_w = 115, .box_w = 8, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 980, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 987, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 993, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1000, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 1006, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1013, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 1019, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1027, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1033, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1042, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1048, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1057, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1063, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1072, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1078, .adv_w = 115, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1088, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1095, .adv_w = 115, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1104, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1111, .adv_w = 115, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1118, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1123, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1132, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1138, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1146, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1151, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1160, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1166, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1175, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1182, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1191, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1197, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1206, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1212, .adv_w = 115, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1222, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1230, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1237, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1245, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1252, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1260, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1267, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1275, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1281, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1289, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1296, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 1302, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1310, .adv_w = 115, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1317, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1323, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 1330, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1338, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1346, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1352, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1360, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1366, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1374, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1382, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1388, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1395, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1401, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1410, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1417, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1426, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1432, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1441, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1448, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1457, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1464, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1473, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1480, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1489, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1496, .adv_w = 115, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1506, .adv_w = 115, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1514, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1523, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1532, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1541, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1548, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1556, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1565, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1573, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1579, .adv_w = 115, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1589, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1596, .adv_w = 115, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1606, .adv_w = 115, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1615, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1623, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1630, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1639, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1645, .adv_w = 115, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1655, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1662, .adv_w = 115, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1667, .adv_w = 115, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1674, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1678, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1683, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1688, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1695, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1700, .adv_w = 115, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1706, .adv_w = 115, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 1716, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1724, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1729, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1734, .adv_w = 115, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1740, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1744, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1748, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1753, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1760, .adv_w = 115, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1770, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1777, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1782, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1790, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1794, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1800, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1806, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1814, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1820, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1824, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1830, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1836, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 1843, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1852, .adv_w = 115, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1858, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1866, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1874, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1881, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1888, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1892, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1897, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1902, .adv_w = 115, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1913, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1917, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1923, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 1928, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 1934, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 1938, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1942, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1946, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 1952, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1960, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1968, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 1973, .adv_w = 115, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 1980, .adv_w = 115, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1986, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1991, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1996, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2000, .adv_w = 115, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2006, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2012, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2016, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 2024, .adv_w = 115, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2030, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 2037, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2043, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2049, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2056, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2061, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2065, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 2072, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 2079, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 2083, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2089, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2095, .adv_w = 115, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 2107, .adv_w = 115, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 2114, .adv_w = 115, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 2123, .adv_w = 115, .box_w = 4, .box_h = 6, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 2126, .adv_w = 115, .box_w = 3, .box_h = 8, .ofs_x = 2, .ofs_y = 3},
    {.bitmap_index = 2129, .adv_w = 115, .box_w = 3, .box_h = 4, .ofs_x = 3, .ofs_y = 5},
    {.bitmap_index = 2131, .adv_w = 115, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 2134, .adv_w = 115, .box_w = 3, .box_h = 6, .ofs_x = 2, .ofs_y = 3},
    {.bitmap_index = 2137, .adv_w = 115, .box_w = 1, .box_h = 3, .ofs_x = 3, .ofs_y = 6},
    {.bitmap_index = 2138, .adv_w = 115, .box_w = 2, .box_h = 3, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 2139, .adv_w = 115, .box_w = 2, .box_h = 4, .ofs_x = 3, .ofs_y = 4}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0x5, 0x8, 0x9, 0x16, 0x17, 0x25, 0x26,
    0x38, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51,
    0x52, 0x58, 0x59, 0x5c, 0x5d, 0x60, 0x61, 0x6a,
    0x6b, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74,
    0x75, 0x8e, 0x8f, 0x90, 0x91, 0xad, 0xb9
};

static const uint8_t glyph_id_ofs_list_2[] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 0, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 0, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 0,
    37, 38, 39, 40, 0, 41, 42, 0,
    43, 44, 45, 46, 47, 0, 0, 48,
    49, 50, 51, 52, 53, 54, 55, 56,
    57, 58, 59, 0, 60, 61, 0, 0,
    62, 63, 0, 0, 64, 65, 66, 67,
    0, 68, 69, 0, 70, 0, 71, 72,
    0, 0, 0, 0, 0, 0, 0, 0,
    73, 0, 74, 75, 0, 0, 0, 76,
    77, 78, 0, 79, 80
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 200, .range_length = 185, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 394, .range_length = 186, .glyph_id_start = 186,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 47, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    },
    {
        .range_start = 592, .range_length = 109, .glyph_id_start = 233,
        .unicode_list = NULL, .glyph_id_ofs_list = glyph_id_ofs_list_2, .list_length = 109, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL
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
    .cmap_num = 3,
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
const lv_font_t lv_font_source_code_pro_12 = {
#else
lv_font_t LV_FONT_SOURCE_CODE_PRO_12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 15,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
/* Fields below are LVGL-version dependent; drop non-existent ones for v8 */
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



#endif /*#if LV_FONT_SOURCE_CODE_PRO_12*/
