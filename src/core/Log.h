#pragma once

#include <Arduino.h>
#include <esp_log.h>

namespace core {

class Log {
public:
  static void init(bool disableWhenNoCDC = true) {
    Serial.setDebugOutput(false);
    if (disableWhenNoCDC) {
      esp_log_set_vprintf(&Log::vprintfRedirect);
    }
    esp_log_level_set("*", ESP_LOG_INFO);
  }

private:
  static int vprintfRedirect(const char* fmt, va_list args) {
    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    #if defined(ARDUINO_USB_CDC_ON_BOOT)
    if (!Serial) return len;
    if (Serial.availableForWrite() <= 0) return len;
    #endif
    size_t w = (size_t)((len < (int)sizeof(buf)) ? len : (int)sizeof(buf));
    Serial.write((const uint8_t*)buf, w);
    return len;
  }
};

} // namespace core


