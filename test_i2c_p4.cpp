// Minimal I2C test for ESP32-P4 - exact copy of working example
#include <Arduino.h>
#include "driver/i2c_master.h"

#define TP_I2C_SDA 7
#define TP_I2C_SCL 8

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== ESP32-P4 I2C Test ===");
    
    // Exactly like the working example
    i2c_master_bus_handle_t i2c_handle = NULL;
    
    i2c_master_bus_config_t i2c_bus_conf = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = (gpio_num_t)TP_I2C_SDA,
        .scl_io_num = (gpio_num_t)TP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    
    Serial.println("Creating I2C bus on port 1...");
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_conf, &i2c_handle);
    if (ret != ESP_OK) {
        Serial.printf("FAILED to create I2C bus! Error: 0x%x\n", ret);
        return;
    }
    Serial.printf("SUCCESS! I2C handle: %p\n", i2c_handle);
    
    // Try to retrieve the handle
    Serial.println("\nTrying i2c_master_get_bus_handle(1)...");
    i2c_master_bus_handle_t retrieved_handle = NULL;
    ret = i2c_master_get_bus_handle(1, &retrieved_handle);
    if (ret != ESP_OK) {
        Serial.printf("FAILED to get bus handle! Error: 0x%x\n", ret);
    } else {
        Serial.printf("SUCCESS! Retrieved handle: %p (matches: %s)\n", 
                     retrieved_handle, 
                     (retrieved_handle == i2c_handle) ? "YES" : "NO");
    }
    
    // Try to scan I2C bus
    Serial.println("\nScanning I2C bus for devices...");
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };
        i2c_master_dev_handle_t dev = NULL;
        if (i2c_master_bus_add_device(i2c_handle, &dev_cfg, &dev) == ESP_OK) {
            uint8_t test_data;
            if (i2c_master_transmit_receive(dev, NULL, 0, &test_data, 1, 100) == ESP_OK) {
                Serial.printf("  Device found at 0x%02X\n", addr);
                found++;
            }
            i2c_master_bus_rm_device(dev);
        }
    }
    Serial.printf("\nScan complete. Found %d device(s).\n", found);
    
    Serial.println("\n=== Test Complete ===");
}

void loop() {
    delay(5000);
}

