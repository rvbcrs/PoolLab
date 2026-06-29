#if defined(USE_ADS1115) && (USE_ADS1115)
#include "AdsPhOrpSensor.h"

#if defined(BOARD_ESP32S3_35)
  // BSP and PowerManager use the legacy IDF i2c driver — ADS must too, else
  // driver_ng vs old-driver conflict aborts at boot. Run on I2C_NUM_1.
  #include <driver/i2c.h>
  namespace io {
    static bool s_ads_port_ready = false;
    void AdsPhOrpSensor::beginBus(){
      Serial.printf("[ADS1115] beginBus: SDA=%d SCL=%d port=I2C_NUM_1\n", _sda, _scl);
      if (!s_ads_port_ready) {
        i2c_config_t cfg = {};
        cfg.mode = I2C_MODE_MASTER;
        cfg.sda_io_num = _sda;
        cfg.scl_io_num = _scl;
        cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
        cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
        cfg.master.clk_speed = 400000;
        esp_err_t e1 = i2c_param_config(I2C_NUM_1, &cfg);
        esp_err_t e2 = i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0);
        Serial.printf("[ADS1115] i2c_param_config=%d install=%d\n", (int)e1, (int)e2);
        s_ads_port_ready = (e1 == ESP_OK && e2 == ESP_OK);
      }
    }
    void AdsPhOrpSensor::writeBytes(uint8_t reg, const uint8_t *data, size_t len){
      uint8_t tmp[3]; tmp[0]=reg; if (len>0 && len<=2) memcpy(&tmp[1], data, len);
      i2c_cmd_handle_t cmd = i2c_cmd_link_create();
      i2c_master_start(cmd);
      i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
      i2c_master_write(cmd, tmp, len+1, true);
      i2c_master_stop(cmd);
      i2c_master_cmd_begin(I2C_NUM_1, cmd, 100 / portTICK_PERIOD_MS);
      i2c_cmd_link_delete(cmd);
    }
    void AdsPhOrpSensor::readBytes(uint8_t reg, uint8_t *data, size_t len){
      i2c_cmd_handle_t cmd = i2c_cmd_link_create();
      i2c_master_start(cmd);
      i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
      i2c_master_write_byte(cmd, reg, true);
      i2c_master_start(cmd);
      i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_READ, true);
      if (len > 1) i2c_master_read(cmd, data, len-1, I2C_MASTER_ACK);
      i2c_master_read_byte(cmd, &data[len-1], I2C_MASTER_NACK);
      i2c_master_stop(cmd);
      i2c_master_cmd_begin(I2C_NUM_1, cmd, 100 / portTICK_PERIOD_MS);
      i2c_cmd_link_delete(cmd);
    }
  }
#else
  #include <driver/i2c_master.h>
  #include "core/I2CBus.h"
  namespace io {
    void AdsPhOrpSensor::beginBus(){
      auto bus = core::i2c_bus_init(_sda, _scl);
      _bus = bus;
      if (!_dev){
        i2c_device_config_t dev_cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = _addr, .scl_speed_hz = 400000, .scl_wait_us = 0, .flags = { 0 } };
        i2c_master_dev_handle_t dev=nullptr;
        ESP_ERROR_CHECK(i2c_master_bus_add_device((i2c_master_bus_handle_t)_bus, &dev_cfg, &dev));
        _dev = dev;
      }
    }
    void AdsPhOrpSensor::writeBytes(uint8_t reg, const uint8_t *data, size_t len){
      uint8_t tmp[3]; tmp[0]=reg; if (len>0 && len<=2) memcpy(&tmp[1], data, len);
      ESP_ERROR_CHECK(i2c_master_transmit((i2c_master_dev_handle_t)_dev, tmp, len+1, -1));
    }
    void AdsPhOrpSensor::readBytes(uint8_t reg, uint8_t *data, size_t len){
      ESP_ERROR_CHECK(i2c_master_transmit_receive((i2c_master_dev_handle_t)_dev, &reg, 1, data, len, -1));
    }
  }
#endif

#endif

