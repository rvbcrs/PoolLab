#include "Storage.h"

namespace core {

Storage::Storage(const char* ns): _ns(ns) {}

bool Storage::begin(bool readOnly) {
  return _prefs.begin(_ns.c_str(), readOnly);
}

float Storage::getPhMin(float def) const { return _prefs.getFloat("ph_min", def); }
float Storage::getPhMax(float def) const { return _prefs.getFloat("ph_max", def); }
int   Storage::getOrpMin(int def) const { return _prefs.getInt("orp_min", def); }
int   Storage::getOrpMax(int def) const { return _prefs.getInt("orp_max", def); }

void  Storage::setPhMin(float v) { _prefs.putFloat("ph_min", v); }
void  Storage::setPhMax(float v) { _prefs.putFloat("ph_max", v); }
void  Storage::setOrpMin(int v) { _prefs.putInt("orp_min", v); }
void  Storage::setOrpMax(int v) { _prefs.putInt("orp_max", v); }

int Storage::getM1Speed(int def) const { return _prefs.getInt("m1_speed", def); }
int Storage::getM2Speed(int def) const { return _prefs.getInt("m2_speed", def); }
void Storage::setM1Speed(int v) { _prefs.putInt("m1_speed", v); }
void Storage::setM2Speed(int v) { _prefs.putInt("m2_speed", v); }

} // namespace core


