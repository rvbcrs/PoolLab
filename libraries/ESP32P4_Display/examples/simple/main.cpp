/**
 * @file main.cpp
 * @brief ESP32P4_Display wrapper example
 * 
 * This example demonstrates the basic usage of the ESP32P4_Display wrapper
 * for initializing the ST7701 LCD and GT911 touch controller on ESP32-P4.
 */

#include <Arduino.h>
#include "ESP32P4_Display.h"
#include "lvgl.h"

// Create the display wrapper instance
ESP32P4_Display display;

void setup() {
    Serial.begin(115200);
    Serial.println("=== ESP32P4_Display Wrapper Example ===");
    
    // Initialize LVGL first
    lv_init();
    
    // Initialize the display hardware (LCD + Touch)
    Serial.println("Initializing display hardware...");
    if (!display.begin()) {
        Serial.println("Failed to initialize display!");
        while(1) delay(1000);
    }
    Serial.println("Display hardware initialized successfully!");
    
    // Initialize LVGL display and input drivers
    Serial.println("Initializing LVGL...");
    lv_disp_t* disp = display.initLVGL(LV_DISP_ROT_270);
    if (!disp) {
        Serial.println("Failed to initialize LVGL!");
        while(1) delay(1000);
    }
    Serial.println("LVGL initialized successfully!");
    
    // Set backlight to 80%
    display.setBacklight(80);
    Serial.println("Backlight set to 80%");
    
    // Create a simple test screen
    Serial.println("Creating test screen...");
    
    // Create a label
    lv_obj_t* label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "ESP32P4_Display\nWrapper Works!");
    lv_obj_center(label);
    
    // Create a button
    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 200, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50);
    
    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Touch Me!");
    lv_obj_center(btn_label);
    
    // Add button click event
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        static int counter = 0;
        counter++;
        Serial.printf("Button clicked %d times!\n", counter);
        
        // Update the main label
        lv_obj_t* label = lv_obj_get_child(lv_scr_act(), 0);
        if (label) {
            char text[50];
            snprintf(text, sizeof(text), "ESP32P4_Display\nWrapper Works!\nClicks: %d", counter);
            lv_label_set_text(label, text);
        }
    }, LV_EVENT_CLICKED, NULL);
    
    Serial.println("Test screen created!");
    Serial.println("Touch the button to test touch functionality");
}

void loop() {
    // Handle LVGL tasks
    lv_timer_handler();
    delay(5);
}






