#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif


#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_LINK_16DP_999999_FILL0_WGHT400_GRAD0_OPSZ20
#define LV_ATTRIBUTE_IMG_LINK_16DP_999999_FILL0_WGHT400_GRAD0_OPSZ20
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_LINK_16DP_999999_FILL0_WGHT400_GRAD0_OPSZ20 uint8_t link_16dp_999999_FILL0_wght400_GRAD0_opsz20_map[] = {
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x0f, 0x9f, 0x00, 
  0x1c, 0x03, 0x80, 
  0x30, 0x00, 0xc0, 
  0x31, 0xf8, 0xc0, 
  0x31, 0xf8, 0xc0, 
  0x30, 0x00, 0xc0, 
  0x1c, 0x03, 0x80, 
  0x0f, 0x9f, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
};

const lv_img_dsc_t link_16dp_999999_FILL0_wght400_GRAD0_opsz20 = {
  .header.cf = LV_IMG_CF_ALPHA_1BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 20,
  .header.h = 20,
  .data_size = 60,
  .data = link_16dp_999999_FILL0_wght400_GRAD0_opsz20_map,
};
