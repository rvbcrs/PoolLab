#include <Arduino.h>
#include <lvgl.h>
#include <JC3248W535.h>

void setup() {
    Serial.begin(115200);
    Serial.println("JC3248W535 Simple Example");
    
    // Initialize the display with one line!
    if (!JC3248W535::begin(0, 80)) {  // 0° rotation, 80% backlight
        Serial.println("Failed to initialize display!");
        while(1) delay(1000);
    }
    
    Serial.println("Display initialized successfully!");
    
    // Create a simple label
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello JC3248W535!");
    lv_obj_center(label);
    
    // Create a button
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50);
    
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Click me!");
    lv_obj_center(btn_label);
    
    // Add button event
    lv_obj_add_event_cb(btn, [](lv_event_t *e) {
        static int count = 0;
        count++;
        Serial.printf("Button clicked %d times!\n", count);
        
        // Update the main label
        lv_obj_t *label = (lv_obj_t*)lv_event_get_user_data(e);
        lv_label_set_text_fmt(label, "Clicked %d times!", count);
    }, LV_EVENT_CLICKED, label);
}

void loop() {
    // LVGL task handler
    lv_timer_handler();
    delay(5);
}


