#pragma once

// ESP32-P4 4.3" 480x800 MIPI-DSI Display Pin Configuration
// Using LVGL software rotation for landscape
// Back to PORTRAIT for stability fix

#define LCD_H_RES 480
#define LCD_V_RES 800

#define LCD_RST -1
#define LCD_LED -1

// GT911 Touch Controller I2C pins
#define TP_I2C_SDA 7
#define TP_I2C_SCL 8
#define TP_RST -1
#define TP_INT -1

// ESP32-C6 Communication (for WiFi/Zigbee)
#define C6_UART_RX 16
#define C6_UART_TX 17
#define C6_IO2     6
#define C6_BOOT    35

// I2C for ESP32-C6 communication (alternative to UART)
#define C6_I2C_SDA 9
#define C6_I2C_SCL 10

