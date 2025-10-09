#pragma once

// Select board via build flag: -D BOARD_ESP32C6_TOUCH_1_47 or -D BOARD_ESP32S3_35 or -D BOARD_ESP32P4_43

// Capability flags default (can be overridden by build flags)
#ifndef HAS_ZIGBEE
#if CONFIG_IDF_TARGET_ESP32C6
#define HAS_ZIGBEE 1
#elif defined(BOARD_ESP32P4_43)
// ESP32-P4 board has ESP32-C6 for WiFi/Zigbee via UART
#define HAS_ZIGBEE 1
#else
#define HAS_ZIGBEE 0
#endif
#endif

// Display configuration structure
struct DisplayConfig {
  int width;
  int height;
  int rotation;
  int colOffset1;
  int rowOffset1;
  int colOffset2;
  int rowOffset2;
  int rstPin;
};

#if defined(BOARD_ESP32C6_TOUCH_1_47)
  static const DisplayConfig DISPLAY_CFG{172,320,0,34,0,34,0,22};
#elif defined(BOARD_ESP32S3_35)
  // Example 480x320 (ILI9488/ST7796 style), adjust to your wiring
  static const DisplayConfig DISPLAY_CFG{480,320,1,0,0,0,0,33};
#elif defined(BOARD_ESP32P4_43)
  // ESP32-P4 4.3" MIPI-DSI touch display (480x800, ST7701 + GT911)
  static const DisplayConfig DISPLAY_CFG{480,800,0,0,0,0,0,-1};
#else
  static const DisplayConfig DISPLAY_CFG{172,320,0,34,0,34,0,22};
#endif


