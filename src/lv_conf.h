#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#if defined(BOARD_ESP32S3_35)
#define LV_COLOR_16_SWAP 1
#else
#define LV_COLOR_16_SWAP 0
#endif
#if defined(BOARD_ESP32S3_35)
// S3 uses BSP lvgl_port tick (calls lv_tick_inc), so use default LVGL tick
#define LV_TICK_CUSTOM 0
#else
// C6 and others: use Arduino millis() as LVGL tick source
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

#define LV_USE_LOG 0

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1

#define LV_USE_GPU 0

/* Input device settings */
#define LV_INDEV_DEF_READ_PERIOD 10  /* Read input every 10ms for snappier response */
#define LV_INDEV_DEF_DRAG_LIMIT 4    /* Start drag after 4 pixels */
#define LV_INDEV_DEF_DRAG_THROW 8    /* Drag throw slow-down */
#define LV_INDEV_DEF_LONG_PRESS_TIME 400  /* Long press time */
#define LV_INDEV_DEF_LONG_PRESS_REP_TIME 100  /* Repeated trigger period */
/* Note: leave gesture defaults to LVGL to avoid redefinition warnings; set per-driver at runtime */

#endif // LV_CONF_H


