#include "PowerManager.h"
#include "I2CScanner.h"
#include "WebUI.h"

extern io::WebUI webui;

#define IP5306_ADDR 0x75
#define REG_READ0 0x70
#define REG_READ3 0x78

PowerManager Power;

void PowerManager::begin(int sda, int scl) {
    // 1. Try probing on I2C0 (Shared bus, managed by BSP: Pins 4, 8)
    _port = I2C_NUM_0;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IP5306_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(_port, cmd, 100 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (err == ESP_OK) {
        _hasIP5306 = true;
        _useADC = false;
        Serial.println("PowerManager: IP5306 found at 0x75 on I2C0 (Shared Bus)!");
    } else {
        Serial.printf("PowerManager: IP5306 NOT found (err %d). Switching to ADC mode on Pin %d.\n", err, _adcPin);
        _hasIP5306 = false;
        _useADC = true;
        
        // Setup ADC with proper configuration
        pinMode(_adcPin, INPUT);
        analogReadResolution(12);
        analogSetAttenuation(ADC_11db);  // 0-3.3V range
        
        // Take a few readings to stabilize
        for (int i = 0; i < 5; i++) {
            analogRead(_adcPin);
            delay(10);
        }
        
        // Show scanner just to be sure
        performI2CScan();
    }
}

void PowerManager::update() {
    if (millis() - _lastUpdate < 5000) return; // Update every 5s
    _lastUpdate = millis();

    int level = getBatteryLevel();
    bool charging = isCharging();
    
    if (_hasIP5306) {
        Serial.printf("PowerManager (IP5306): Level=%d%%, Charging=%d\n", level, charging);
    } else if (_useADC) {
        float volts = readBatteryVoltage();
        Serial.printf("PowerManager (ADC Pin %d): %.2fV -> %d%% (Charging=%d)\n", _adcPin, volts, level, charging);
    } else {
        // No power monitoring
    }
}

uint8_t PowerManager::readRegister(uint8_t reg) {
    if (!_hasIP5306) return 0;
    
    uint8_t data = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IP5306_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    
    i2c_master_start(cmd); // Repeated start
    i2c_master_write_byte(cmd, (IP5306_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t err = i2c_master_cmd_begin(_port, cmd, 100 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (err != ESP_OK) return 0;
    return data;
}

int PowerManager::getBatteryLevel() {
    if (_hasIP5306) {
        uint8_t level = readRegister(REG_READ3);
        switch(level & 0xF0) {
            case 0xF0: return 100;
            case 0xE0: return 75;
            case 0xC0: return 50;
            case 0x80: return 25; 
            default: return 0;
        }
    } else if (_useADC) {
        float volts = readBatteryVoltage();
        return voltageToPercent(volts);
    }
    return -1;
}

bool PowerManager::isCharging() {
    if (_hasIP5306) {
        uint8_t status = readRegister(REG_READ0);
        return (status & 0x08) ? true : false;
    } 
    // ADC mode: detect USB connection as "charging" indicator
    // When USB is connected, Serial (USB CDC) is active
    #if ARDUINO_USB_CDC_ON_BOOT
    return Serial;  // Returns true if USB CDC is connected to host
    #else
    return false;
    #endif
}

bool PowerManager::isConnected() {
    return _hasIP5306 || _useADC;
}

float PowerManager::readBatteryVoltage() {
    // 12-bit ADC: 0-4095
    // Reference 3.3V
    // Divider per schematic: R26=33K (High), R27=100K (Low)
    // V_out = V_in * 100 / (33 + 100) = V_in * 0.7518
    // V_in = V_out * 1.33
    
    // Calibrated based on user feedback (raw ~2890 at full charge -> 4.2V)
    // Means V_in = V_out * 1.80
    
    // Average 10 samples for stability
    long sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(_adcPin);
        delayMicroseconds(100);
    }
    int raw = sum / 10;
    
    // Debug raw value to check calibration
    float v = (raw / 4095.0f) * 3.3f * 1.80f;
    String msg = "ADC raw: " + String(raw) + " -> Volts: " + String(v, 2);
    Serial.println(msg);
    webui.log(msg);
    
    return v;
}

int PowerManager::voltageToPercent(float volts) {
    // LiPo voltage range under small load: 3.2V (empty) to 4.1V (full)
    // 4.2V is charging max, 4.1V is realistic full rest.
    if (volts >= 4.10) return 100;
    if (volts <= 3.20) return 0;
    return (int)((volts - 3.20) / (4.10 - 3.20) * 100.0);
}

