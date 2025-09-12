#pragma once

#include <lvgl.h>

namespace ui {

void init(lv_disp_t* disp);
void build(bool safeBaseline);
void updateValues();

struct Handlers {
  void (*onSpeedChange)(int idx, int value) = nullptr; // idx: 1=pH motor, 2=ORP motor
  void (*onModeToggle)(bool zigbee) = nullptr;         // true -> Zigbee mode, false -> WiFi/MQTT
};

void configureHandlers(const Handlers &h);
void setInitialSpeeds(uint8_t m1, uint8_t m2);
void setInitialMode(bool zigbee);

} // namespace ui


