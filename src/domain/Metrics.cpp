#include "Metrics.h"

namespace domain {

Metrics& Metrics::instance() {
  static Metrics m;
  return m;
}

} // namespace domain


