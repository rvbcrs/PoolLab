#include <Arduino.h>
#include "ESP32P4_Display.h"

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32P4 Display - Simple Init");

    if (!Display.begin()) {
        Serial.println("Display init failed");
        return;
    }
    if (!Display.initLVGL()) {
        Serial.println("LVGL init failed");
        return;
    }

    // Simple demo UI
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *rect = lv_obj_create(scr);
    lv_obj_set_size(rect, 200, 100);
    lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(rect, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_t *label = lv_label_create(rect);
    lv_label_set_text(label, "ESP32P4 Works!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
}

void loop() {
    Display.lvglLoop();
    delay(5);
}