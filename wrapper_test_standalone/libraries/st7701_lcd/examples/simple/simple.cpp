#include <Arduino.h>
#include <lvgl.h>
#include <ST7701_GT911.h>

void setup() {
    Serial.begin(115200);
    Serial.println("ST7701 + GT911 Simple Example");
    
    // Initialize display and touch with ONE LINE!
    if (!ST7701_GT911::begin(0, 80)) {  // 0° rotation, 80% backlight
        Serial.println("Failed to initialize display!");
        while(1) delay(1000);
    }
    
    Serial.println("Display initialized successfully!");
    
    // Initialize LVGL
    if (!ST7701_GT911::initLVGL()) {
        Serial.println("Failed to initialize LVGL!");
        while(1) delay(1000);
    }
    
    Serial.println("LVGL initialized successfully!");
    
    // Create a title label
    lv_obj_t *title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "ST7701 + GT911 Test");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // Create a status label
    lv_obj_t *status = lv_label_create(lv_scr_act());
    lv_label_set_text(status, "Display: ✅ Ready");
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 60);
    
    // Create a button
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 150, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Click Me!");
    lv_obj_center(btn_label);
    
    // Create a counter label
    lv_obj_t *counter = lv_label_create(lv_scr_act());
    lv_label_set_text(counter, "Clicks: 0");
    lv_obj_align(counter, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Add button event
    lv_obj_add_event_cb(btn, [](lv_event_t *e) {
        static int count = 0;
        count++;
        Serial.printf("Button clicked %d times!\n", count);
        
        // Update the counter label
        lv_obj_t *counter = (lv_obj_t*)lv_event_get_user_data(e);
        lv_label_set_text_fmt(counter, "Clicks: %d", count);
        
        // Update status
        lv_obj_t *status = lv_obj_get_child(lv_scr_act(), 1); // Second child
        lv_label_set_text_fmt(status, "Display: ✅ %d clicks", count);
    }, LV_EVENT_CLICKED, counter);
    
    Serial.println("UI created successfully!");
    Serial.println("Try touching the button on the display");
}

void loop() {
    // LVGL task handler - this is essential!
    ST7701_GT911::lvglTask();
    delay(5);
}


