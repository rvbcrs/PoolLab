#include <Arduino.h>
#include "ESP32P4_Display.h"

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32P4 Display Test");
    
    // Super simple initialization - just 2 lines!
    Display.begin();
    Display.initLVGL();
    
    // Create a simple test screen
    lv_obj_t *scr = lv_scr_act();
    
    // Create a red rectangle
    lv_obj_t *rect = lv_obj_create(scr);
    lv_obj_set_size(rect, 200, 100);
    lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(rect, lv_color_hex(0xFF0000), LV_PART_MAIN);
    
    // Create a label
    lv_obj_t *label = lv_label_create(rect);
    lv_label_set_text(label, "ESP32P4 Works!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    
    Serial.println("Display initialized successfully!");
}

void loop() {
    // Just call this in your loop
    Display.lvglLoop();
    delay(5);
}