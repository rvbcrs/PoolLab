// 24h ring buffer of 5-minute averages for pH/ORP/temp.
// RAM-only (~2 KB): refills after reboot, which is fine for a sparkline.
#pragma once

#include <Arduino.h>

namespace domain {

class History {
public:
  static constexpr int SLOTS = 288;                 // 24 h / 5 min
  static constexpr uint32_t BUCKET_MS = 300000UL;   // 5 min
  static History& instance();

  void sample();                 // call every ~5 s; averages Metrics into the current bucket
  void toJson(String &out);      // appends "ph":[...],"orp":[...],"temp":[...],"step":300

private:
  static constexpr int16_t EMPTY = INT16_MIN;
  int16_t _ph100[SLOTS];         // pH * 100
  int16_t _orp[SLOTS];           // mV
  int16_t _temp10[SLOTS];        // °C * 10
  int _head = -1;                // slot of the newest finalized bucket
  int _count = 0;                // finalized buckets stored
  uint32_t _bucketId = 0;
  float _phSum = 0, _orpSum = 0, _tempSum = 0;
  uint16_t _phN = 0, _orpN = 0, _tempN = 0;

  History();
  void finalizeBucket();
};

} // namespace domain
