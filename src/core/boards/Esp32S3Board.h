#pragma once

#include "core/Board.h"

namespace core {

class Esp32S3Board : public Board {
public:
  const char* name() const override { return "ESP32-S3"; }
  void earlyInit() override;
  void initPeripherals() override;
};

} // namespace core


