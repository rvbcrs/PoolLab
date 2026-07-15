#include "History.h"
#include "Metrics.h"

namespace domain {

History& History::instance() {
  static History h;
  return h;
}

History::History() {
  for (int i = 0; i < SLOTS; i++) { _ph100[i] = EMPTY; _orp[i] = EMPTY; _temp10[i] = EMPTY; }
}

void History::finalizeBucket() {
  _head = (_head + 1) % SLOTS;
  if (_count < SLOTS) _count++;
  _ph100[_head]  = _phN   ? (int16_t)lrintf(_phSum   / _phN   * 100.0f) : EMPTY;
  _orp[_head]    = _orpN  ? (int16_t)lrintf(_orpSum  / _orpN)           : EMPTY;
  _temp10[_head] = _tempN ? (int16_t)lrintf(_tempSum / _tempN * 10.0f)  : EMPTY;
  _phSum = _orpSum = _tempSum = 0;
  _phN = _orpN = _tempN = 0;
}

void History::sample() {
  uint32_t id = millis() / BUCKET_MS;
  if (_bucketId == 0) _bucketId = id;
  if (id != _bucketId) {           // ponytail: any gap (incl. millis wrap) advances one slot
    finalizeBucket();
    _bucketId = id;
  }
  Metrics &m = Metrics::instance();
  if (m.havePh)   { _phSum   += m.phVal;  _phN++; }
  if (m.haveOrp)  { _orpSum  += m.orpMv;  _orpN++; }
  if (m.haveTemp) { _tempSum += m.tempC;  _tempN++; }
}

void History::toJson(String &out) {
  out.reserve(out.length() + 5200);
  const int16_t *arr[3] = { _ph100, _orp, _temp10 };
  const char *name[3] = { "ph", "orp", "temp" };
  const float div[3] = { 100.0f, 1.0f, 10.0f };
  const int dec[3] = { 2, 0, 1 };
  for (int a = 0; a < 3; a++) {
    out += '"'; out += name[a]; out += "\":[";
    for (int i = 0; i < _count; i++) {
      int idx = (_head - _count + 1 + i + SLOTS * 2) % SLOTS;
      if (i) out += ',';
      int16_t v = arr[a][idx];
      if (v == EMPTY) { out += "null"; }
      else { char b[16]; dtostrf(v / div[a], 0, dec[a], b); out += b; }
    }
    out += "],";
  }
  out += "\"step\":300";
}

} // namespace domain
