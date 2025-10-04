// Build this implementation only when ADS1115 is enabled and not on S3 BSP path
#if defined(USE_ADS1115) && (USE_ADS1115) && !defined(BOARD_ESP32S3_35)
#include "AdsPhOrpSensor.h"
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

} // namespace io
#endif

