#pragma once

#include <driver/i2c_master.h>

namespace core {

// Initialize and return a singleton handle to the new IDF I2C master bus.
// Calling this multiple times with the same pins is safe; the first call wins.
i2c_master_bus_handle_t i2c_bus_init(int sda, int scl);

// Initialize and return a singleton handle for a specific I2C port (e.g. I2C_NUM_0 or I2C_NUM_1)
i2c_master_bus_handle_t i2c_bus_init_port(int sda, int scl, i2c_port_t port);

} // namespace core

