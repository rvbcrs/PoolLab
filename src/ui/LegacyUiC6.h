#pragma once

#include <Arduino.h>
#if !defined(USE_JC3248W535)
#include <Arduino_GFX_Library.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#endif

namespace ui {

struct LegacyUiState {
  int currentPage = 0;
};

class LegacyUiC6 {
public:
  LegacyUiC6(
  #if !defined(USE_JC3248W535)
    Arduino_GFX *gfxRef
  #else
    void *gfxRef
  #endif
  ) :
  #if !defined(USE_JC3248W535)
    gfx(gfxRef)
  #else
    gfx(nullptr)
  #endif
  {}
  void drawStaticUI();
  void drawSettingsPage(uint8_t m1Speed, uint8_t m2Speed);
  void drawPagination();
  void updateValues(bool havePh, float phVal,
                    bool haveOrp, float orpMv,
                    bool haveTemp, float tempC,
                    const String &ipText);
  void handleTouch(bool (*readTouch)(int16_t&,int16_t&,bool&),
                   uint8_t &m1Speed, uint8_t &m2Speed,
                   float &phMin, float &phMax, int &orpMin, int &orpMax,
                   const std::function<void()> &onSave,
                   const std::function<void()> &onRenderMain,
                   const std::function<void()> &onRenderSettings,
                   const std::function<void()> &onOverlaySave,
                   const std::function<void()> &onOverlayCancel);
  void showRangeOverlay(bool forPh, float phMin, float phMax, int orpMin, int orpMax);
  bool overlayActive() const { return ovlActive; }
  int page() const { return st.currentPage; }
  void setPage(int p){ st.currentPage = p; }
private:
  #if !defined(USE_JC3248W535)
  Arduino_GFX *gfx;
  #else
  void *gfx;
  #endif
  LegacyUiState st;
  // Cached canvases and last values
  #if !defined(USE_JC3248W535)
  GFXcanvas16 *phCanvas = nullptr;
  GFXcanvas16 *orpCanvas = nullptr;
  GFXcanvas16 *tempCanvas = nullptr;
  GFXcanvas16 *ipCanvas = nullptr;
  #endif
  int32_t lastPhScaled = INT32_MAX;
  int32_t lastOrpInt   = INT32_MAX;
  int32_t lastTempScaled = INT32_MAX;
  String shownIp;
  // Layout
  static constexpr int PH_BOX_X=24, PH_BOX_Y=40, PH_BOX_W=110, PH_BOX_H=40;
  static constexpr int ORP_BOX_X=182, ORP_BOX_Y=40, ORP_BOX_W=110, ORP_BOX_H=40;
  static constexpr int TEMP_BOX_X=24, TEMP_BOX_Y=118, TEMP_BOX_W=120, TEMP_BOX_H=16;
  static constexpr int IP_BOX_X=182, IP_BOX_Y=118, IP_BOX_W=120, IP_BOX_H=16;
  bool ovlActive = false;
  enum { OVL_NONE=0, OVL_PH, OVL_ORP } ovl = OVL_NONE;
};

} // namespace ui


