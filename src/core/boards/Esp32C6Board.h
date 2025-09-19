#pragma once

#include "core/Board.h"

namespace core {

class Esp32C6Board : public Board {
public:
  const char* name() const override { return "ESP32-C6"; }
  void earlyInit() override;
  void initPeripherals() override;
};

} // namespace core


