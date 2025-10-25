#pragma once

// ESP32-P4 Pin Configuration - EXACTLY like lvgl_demo_v8
// This is the WORKING demo project configuration

#define LCD_H_RES 480
#define LCD_V_RES 800

#define LCD_RST 5
#define LCD_LED 23

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