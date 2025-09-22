#pragma once

#include <Arduino.h>

namespace io {

struct ButtonPins {
  int primaryPin;
  int secondaryPin;
};

class Buttons {
public:
  void begin(const ButtonPins &pins) {
    _pins = pins;
    if (_pins.primaryPin >= 0) pinMode(_pins.primaryPin, INPUT_PULLUP);
    if (_pins.secondaryPin >= 0) pinMode(_pins.secondaryPin, INPUT_PULLUP);
    bool raw = readRaw();
    _rawPrev = raw; _stable = raw; _prev = raw; _pressMs = 0; _lastChangeMs = millis();
  }

  // Call frequently from loop(); returns true once when a long press is detected
  bool pollLongPress(uint32_t minHoldMs = 800) {
    uint32_t now = millis();
    bool raw = readRaw();
    if (raw != _rawPrev) { _rawPrev = raw; _lastChangeMs = now; }
    if ((now - _lastChangeMs) >= 15) { _stable = raw; }

    bool current = _stable;
    bool fired = false;
    if (current && !_prev) {
      _pressMs = now;
    } else if (!current && _prev) {
      uint32_t held = _pressMs ? (now - _pressMs) : 0;
      if (held >= minHoldMs) fired = true;
      _pressMs = 0;
    }
    _prev = current;
    return fired;
  }

private:
  bool readRaw() const {
    bool v1 = (_pins.primaryPin >= 0) ? (digitalRead(_pins.primaryPin) == LOW) : false;
    bool v2 = (_pins.secondaryPin >= 0) ? (digitalRead(_pins.secondaryPin) == LOW) : false;
    return v1 || v2;
  }

  ButtonPins _pins{ -1, -1 };
  bool _prev = false;
  bool _rawPrev = false;
  bool _stable = false;
  uint32_t _pressMs = 0;
  uint32_t _lastChangeMs = 0;
};

} // namespace io
