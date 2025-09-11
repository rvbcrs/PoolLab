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

#ifndef LV_ATTRIBUTE_IMG_WATER_PUMP_24DP_E3E3E3_FILL0_WGHT400_GRAD0_OPSZ24
#define LV_ATTRIBUTE_IMG_WATER_PUMP_24DP_E3E3E3_FILL0_WGHT400_GRAD0_OPSZ24
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_WATER_PUMP_24DP_E3E3E3_FILL0_WGHT400_GRAD0_OPSZ24 uint8_t water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24_map[] = {
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x06, 
  0x00, 0x7f, 0xfe, 
  0x00, 0xff, 0xfe, 
  0x01, 0xc3, 0x86, 
  0x03, 0x00, 0xc6, 
  0x07, 0x00, 0xfe, 
  0x06, 0x18, 0x7e, 
  0x06, 0x3c, 0x66, 
  0x66, 0x3c, 0x60, 
  0x7e, 0x3c, 0x60, 
  0x7f, 0x18, 0xe0, 
  0x63, 0x00, 0xc0, 
  0x61, 0xc3, 0x80, 
  0x7f, 0xff, 0x00, 
  0x7f, 0xfe, 0x00, 
  0x60, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 
};

const lv_img_dsc_t water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24 = {
  .header.cf = LV_IMG_CF_ALPHA_1BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 24,
  .header.h = 24,
  .data_size = 72,
  .data = water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24_map,
};
