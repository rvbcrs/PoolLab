#include <Arduino.h>
#include <driver/i2c.h>

class PowerManager {
public:
    void begin(int sda = 18, int scl = 19);
    void update();
    int getBatteryLevel(); // 0, 25, 50, 75, 100
    bool isCharging();
    bool isConnected();

private:
    i2c_port_t _port = I2C_NUM_1;
    bool _hasIP5306 = false;
    int _lastLevel = -1;
    bool _lastCharging = false;
    unsigned long _lastUpdate = 0;
    
    uint8_t readRegister(uint8_t reg);
    
    // ADC mode
    bool _useADC = false;
    int _adcPin = 5;
    float readBatteryVoltage();
    int voltageToPercent(float volts);
};

extern PowerManager Power;
