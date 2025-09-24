#pragma once

#include "domain/Telemetry.h"

namespace domain {

class SensorSource {
public:
  virtual ~SensorSource() {}
  virtual void begin() = 0;
  virtual bool read(Telemetry &out) = 0; // returns true if any value updated
};

} // namespace domain


