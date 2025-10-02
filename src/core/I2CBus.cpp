#include "I2CBus.h"

namespace core {

#if !defined(BOARD_ESP32S3_35)

static i2c_master_bus_handle_t s_bus = nullptr;
static i2c_master_bus_handle_t s_bus1 = nullptr;

i2c_master_bus_handle_t i2c_bus_init(int sda, int scl){
  if (s_bus) return s_bus;
  i2c_master_bus_config_t cfg = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = (gpio_num_t)sda,
    .scl_io_num = (gpio_num_t)scl,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .intr_priority = 0,
    .flags = { .enable_internal_pullup = true }
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &s_bus));
  return s_bus;
}

i2c_master_bus_handle_t i2c_bus_init_port(int sda, int scl, i2c_port_t port){
  if (port == I2C_NUM_0) return i2c_bus_init(sda, scl);
  if (port == I2C_NUM_1 && s_bus1) return s_bus1;
  i2c_master_bus_config_t cfg = {
    .i2c_port = port,
    .sda_io_num = (gpio_num_t)sda,
    .scl_io_num = (gpio_num_t)scl,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .intr_priority = 0,
    .flags = { .enable_internal_pullup = true }
  };
  if (port == I2C_NUM_1) {
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &s_bus1));
    return s_bus1;
  }
  i2c_master_bus_handle_t tmp = nullptr;
  ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &tmp));
  return tmp;
}

#else

// S3: provide stubs that do not reference new driver symbols
i2c_master_bus_handle_t i2c_bus_init(int, int){ return nullptr; }
i2c_master_bus_handle_t i2c_bus_init_port(int, int, i2c_port_t){ return nullptr; }

#endif

} // namespace core

