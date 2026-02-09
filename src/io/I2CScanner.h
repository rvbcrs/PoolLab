#pragma once
#include <Arduino.h>
#include <driver/i2c.h>

void performI2CScan() {
    Serial.println("\n--- I2C Scanner (I2C0 / Shared Bus) ---");
    i2c_port_t port = I2C_NUM_0; 
    
    // We assume bus is already initialized by BSP.
    // If not, this check will fail or return errors.
    
    int nDevices = 0;
    for (int address = 1; address < 127; address++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        // Timeout 50ms
        esp_err_t ret = i2c_master_cmd_begin(port, cmd, 50 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            Serial.printf("I2C device found at address 0x%02X", address);
            if (address == 0x75) Serial.print(" (IP5306?)");
            if (address == 0x5D || address == 0x14 || address == 0x38) Serial.print(" (Touch?)");
            Serial.println("  !");
            nDevices++;
        } else if (ret == ESP_ERR_TIMEOUT) {
            // Serial.printf("0x%02X: Timeout\n", address);
        } else if (ret != ESP_FAIL) { // ESP_FAIL usually NACK
             // Serial.printf("0x%02X: Error %d\n", address, ret);
        }
    }
    
    if (nDevices == 0)
        Serial.println("No I2C devices found on I2C0.\n");
    else
        Serial.println("Scan done.\n");
}
