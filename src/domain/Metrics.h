#pragma once

namespace domain {

struct Metrics {
  bool havePh = false;
  bool haveOrp = false;
  bool haveTemp = false;
  float phVal = 0.0f;
  float orpMv = 0.0f;
  float tempC = 0.0f;
  bool preferPhPrimary = false;

  static Metrics& instance();
};

} // namespace domain


