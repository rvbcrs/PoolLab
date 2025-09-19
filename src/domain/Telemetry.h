#pragma once

namespace domain {

struct Telemetry {
  float phVal = 0.0f;
  float orpMv = 0.0f;
  float tempC = 0.0f;
  bool havePh = false;
  bool haveOrp = false;
  bool haveTemp = false;
};

} // namespace domain


