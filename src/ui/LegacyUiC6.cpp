#include "LegacyUiC6.h"

#if !defined(USE_JC3248W535)
#include <Arduino_GFX_Library.h>

namespace ui {

static inline void drawButton(Arduino_GFX *gfx, int x,int y,int w,int h,const char* label,uint16_t fg,uint16_t bg,int textSize=2){
  gfx->fillRoundRect(x, y, w, h, 4, bg);
  gfx->drawRoundRect(x, y, w, h, 4, fg);
  gfx->setTextSize(textSize);
  gfx->setTextColor(fg);
  int16_t tw = (int16_t)(strlen(label) * 6 * textSize);
  int16_t th = (int16_t)(8 * textSize);
  int16_t cx = x + (w - tw) / 2;
  int16_t cy = y + (h - th) / 2;
  gfx->setCursor(cx, cy);
  gfx->print(label);
}

void LegacyUiC6::showRangeOverlay(bool forPh, float phMin, float phMax, int orpMin, int orpMax){
  if (!gfx) return;
  ovlActive = true;
  ovl = forPh ? OVL_PH : OVL_ORP;
  const int OVL_X=12, OVL_Y=6, OVL_W=300, OVL_H=148;
  const int BTN_W=48, BTN_H=32;
  const int ROW1_Y = OVL_Y + 26;
  const int ROW2_Y = ROW1_Y + 48;
  const int VAL_W=110;
  const int COL_LEFT = OVL_X + 16;
  const int COL_VAL  = OVL_X + 70;
  const int COL_PLUS = OVL_X + 220;
  const int COL_MINUS= OVL_X + 170;
  char b[16];
  gfx->fillRoundRect(OVL_X, OVL_Y, OVL_W, OVL_H, 10, DARKGREY);
  gfx->drawRoundRect(OVL_X, OVL_Y, OVL_W, OVL_H, 10, WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(COL_LEFT, ROW1_Y-10); gfx->print("Min");
  gfx->setCursor(COL_LEFT, ROW2_Y-10); gfx->print("Max");
  if (ovl==OVL_PH) { snprintf(b,sizeof(b),"%.2f", phMin); }
  else { snprintf(b,sizeof(b),"%d", orpMin); }
  drawButton(gfx, COL_VAL, ROW1_Y-8, VAL_W, BTN_H, b, WHITE, BLACK, 2);
  if (ovl==OVL_PH) { snprintf(b,sizeof(b),"%.2f", phMax); }
  else { snprintf(b,sizeof(b),"%d", orpMax); }
  drawButton(gfx, COL_VAL, ROW2_Y-8, VAL_W, BTN_H, b, WHITE, BLACK, 2);
  drawButton(gfx, COL_MINUS, ROW1_Y-8, BTN_W, BTN_H, "-", WHITE, DARKGREY, 3);
  drawButton(gfx, COL_PLUS,  ROW1_Y-8, BTN_W, BTN_H, "+", WHITE, DARKGREY, 3);
  drawButton(gfx, COL_MINUS, ROW2_Y-8, BTN_W, BTN_H, "-", WHITE, DARKGREY, 3);
  drawButton(gfx, COL_PLUS,  ROW2_Y-8, BTN_W, BTN_H, "+", WHITE, DARKGREY, 3);
  gfx->fillRect(OVL_X+8, OVL_Y+OVL_H-44, OVL_W-16, 40, DARKGREY);
  drawButton(gfx, OVL_X+34,  OVL_Y+OVL_H-38, 100, BTN_H, "Save", WHITE, GREEN, 2);
  drawButton(gfx, OVL_X+OVL_W-144, OVL_Y+OVL_H-38, 100, BTN_H, "Cancel", WHITE, RED, 2);
}

void LegacyUiC6::drawStaticUI(){
  if (!gfx) return;
  gfx->fillScreen(BLACK);
  // outer frame
  gfx->drawRoundRect(2, 2, 316, 168, 10, CYAN);
  gfx->drawRoundRect(4, 4, 312, 164, 10, DARKGREEN);
  // labels for tiles
  gfx->setFont(nullptr);
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(PH_BOX_X - 18, PH_BOX_Y + 12);
  gfx->print("pH");
  gfx->setCursor(ORP_BOX_X - 24, ORP_BOX_Y + 12);
  gfx->print("ORP");
  gfx->setCursor(TEMP_BOX_X - 26, TEMP_BOX_Y + 12);
  gfx->print("Temp");
  gfx->setCursor(IP_BOX_X - 12, IP_BOX_Y + 12);
  gfx->print(""); // IP label comes from canvas
  // frames around value boxes
  // Fill tile backgrounds to match earlier look
  gfx->fillRoundRect(PH_BOX_X - 2, PH_BOX_Y - 2, PH_BOX_W + 4, PH_BOX_H + 4, 6, BLUE);
  gfx->fillRoundRect(ORP_BOX_X - 2, ORP_BOX_Y - 2, ORP_BOX_W + 4, ORP_BOX_H + 4, 6, GREEN);
  gfx->drawRoundRect(PH_BOX_X - 2, PH_BOX_Y - 2, PH_BOX_W + 4, PH_BOX_H + 4, 6, WHITE);
  gfx->drawRoundRect(ORP_BOX_X - 2, ORP_BOX_Y - 2, ORP_BOX_W + 4, ORP_BOX_H + 4, 6, WHITE);
  gfx->drawRoundRect(TEMP_BOX_X - 2, TEMP_BOX_Y - 2, TEMP_BOX_W + 4, TEMP_BOX_H + 4, 4, DARKGREY);
  gfx->drawRoundRect(IP_BOX_X - 2, IP_BOX_Y - 2, IP_BOX_W + 4, IP_BOX_H + 4, 4, DARKGREY);
  // settings button (page switch)
  drawButton(gfx, 210, 145, 100, 25, "Settings", WHITE, DARKGREY, 1);
}

void LegacyUiC6::drawSettingsPage(uint8_t m1Speed, uint8_t m2Speed){
  if (!gfx) return;
  gfx->drawRoundRect(2, 2, 316, 168, 10, CYAN);
  gfx->drawRoundRect(4, 4, 312, 164, 10, DARKGREEN);
  gfx->setFont(nullptr);
  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);
  gfx->setCursor(10, 20);
  gfx->print("Motor Settings");
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(20, 50); gfx->print("pH Motor Speed:");
  drawButton(gfx, 150, 45, 40, 20, "-", WHITE, DARKGREY, 1);
  char b1[8]; snprintf(b1,sizeof(b1),"%u", (unsigned)m1Speed);
  drawButton(gfx, 195, 45, 50, 20, b1, WHITE, BLACK, 1);
  drawButton(gfx, 250, 45, 40, 20, "+", WHITE, DARKGREY, 1);
  gfx->setCursor(295, 50); gfx->print("%");
  gfx->setCursor(20, 80); gfx->print("ORP Motor Speed:");
  drawButton(gfx, 150, 75, 40, 20, "-", WHITE, DARKGREY, 1);
  char b2[8]; snprintf(b2,sizeof(b2),"%u", (unsigned)m2Speed);
  drawButton(gfx, 195, 75, 50, 20, b2, WHITE, BLACK, 1);
  drawButton(gfx, 250, 75, 40, 20, "+", WHITE, DARKGREY, 1);
  gfx->setCursor(295, 80); gfx->print("%");
  drawButton(gfx, 110, 120, 100, 30, "Save", WHITE, GREEN, 2);
  // back button to main
  drawButton(gfx, 10, 145, 60, 25, "Back", WHITE, DARKGREY, 1);
}

void LegacyUiC6::drawPagination(){
  if (!gfx) return;
  gfx->setTextColor(WHITE);
  gfx->setCursor(8, 148); gfx->print(st.currentPage==0?"<":" ");
  gfx->setCursor(280, 148); gfx->print(st.currentPage==1?">":" ");
}

void LegacyUiC6::updateValues(bool havePh, float phVal,
                    bool haveOrp, float orpMv,
                    bool haveTemp, float tempC,
                    const String &ipText){
  if (!gfx) return;
  #if !defined(USE_JC3248W535)
  if (!phCanvas) phCanvas = new GFXcanvas16(PH_BOX_W, PH_BOX_H);
  if (!orpCanvas) orpCanvas = new GFXcanvas16(ORP_BOX_W, ORP_BOX_H);
  if (!tempCanvas) tempCanvas = new GFXcanvas16(TEMP_BOX_W, TEMP_BOX_H);
  if (!ipCanvas) ipCanvas = new GFXcanvas16(IP_BOX_W, IP_BOX_H);
  #endif

  int phScaled = havePh ? (int)lrintf(phVal * 100.0f) : INT32_MIN;
  if (phScaled != lastPhScaled) {
    lastPhScaled = phScaled;
    #if !defined(USE_JC3248W535)
    if (phCanvas) {
      phCanvas->fillScreen(BLACK);
      phCanvas->setFont(&FreeSansBold24pt7b);
      uint16_t c = WHITE;
      phCanvas->setTextColor(c);
      phCanvas->setCursor(0, 34);
      if (havePh) { char b[16]; snprintf(b,sizeof(b),"%.2f", phVal); phCanvas->print(b); } else { phCanvas->print("--.--"); }
      gfx->draw16bitRGBBitmap(PH_BOX_X, PH_BOX_Y, phCanvas->getBuffer(), PH_BOX_W, PH_BOX_H);
      phCanvas->setFont(nullptr);
    }
    #endif
  }

  int orpInt = haveOrp ? (int)lrintf(orpMv) : INT32_MIN;
  if (orpInt != lastOrpInt) {
    lastOrpInt = orpInt;
    #if !defined(USE_JC3248W535)
    if (orpCanvas) {
      orpCanvas->fillScreen(BLACK);
      orpCanvas->setFont(&FreeSansBold24pt7b);
      uint16_t c = WHITE;
      orpCanvas->setTextColor(c);
      orpCanvas->setCursor(0, 34);
      char vb[16];
      if (haveOrp) { snprintf(vb,sizeof(vb),"%d", orpInt); orpCanvas->print(vb); } else { strcpy(vb, "----"); orpCanvas->print(vb); }
      gfx->draw16bitRGBBitmap(ORP_BOX_X, ORP_BOX_Y, orpCanvas->getBuffer(), ORP_BOX_W, ORP_BOX_H);
      orpCanvas->setFont(nullptr);
      int16_t bx1, by1; uint16_t bw, bh;
      orpCanvas->setFont(&FreeSansBold24pt7b);
      orpCanvas->getTextBounds(vb, 0, 34, &bx1, &by1, &bw, &bh);
      int mvx = ORP_BOX_X + (int)min((int)bw + 8, ORP_BOX_W - 16);
      int mvy = ORP_BOX_Y + 28;
      gfx->setFont(nullptr);
      gfx->setTextSize(1);
      gfx->setTextColor(WHITE);
      gfx->setCursor(mvx, mvy);
      gfx->print("mV");
    }
    #endif
  }

  int tempScaled = haveTemp ? (int)lrintf(tempC * 10.0f) : INT32_MIN;
  if (tempScaled != lastTempScaled) {
    lastTempScaled = tempScaled;
    #if !defined(USE_JC3248W535)
    if (tempCanvas) {
      tempCanvas->fillScreen(BLACK);
      tempCanvas->setFont(&FreeSans12pt7b);
      tempCanvas->setTextColor(WHITE);
      tempCanvas->setCursor(0, 16);
      if (haveTemp) { char b[20]; snprintf(b,sizeof(b),"%.1f °C", tempC); tempCanvas->print(b); } else { tempCanvas->print("--.- °C"); }
      gfx->draw16bitRGBBitmap(TEMP_BOX_X, TEMP_BOX_Y, tempCanvas->getBuffer(), TEMP_BOX_W, TEMP_BOX_H);
      tempCanvas->setFont(nullptr);
    }
    #endif
  }

  if (ipText != shownIp) {
    shownIp = ipText;
    #if !defined(USE_JC3248W535)
    if (ipCanvas) {
      ipCanvas->fillScreen(BLACK);
      ipCanvas->setFont(nullptr);
      ipCanvas->setTextSize(1);
      ipCanvas->setTextColor(WHITE);
      ipCanvas->setCursor(0, 12);
      ipCanvas->print("IP: "); ipCanvas->print(shownIp);
      gfx->draw16bitRGBBitmap(IP_BOX_X, IP_BOX_Y, ipCanvas->getBuffer(), IP_BOX_W, IP_BOX_H);
    }
    #endif
  }
}

void LegacyUiC6::handleTouch(bool (*readTouch)(int16_t&,int16_t&,bool&),
                   uint8_t &m1Speed, uint8_t &m2Speed,
                   float &phMin, float &phMax, int &orpMin, int &orpMax,
                   const std::function<void()> &onSave,
                   const std::function<void()> &onRenderMain,
                   const std::function<void()> &onRenderSettings,
                   const std::function<void()> &onOverlaySave,
                   const std::function<void()> &onOverlayCancel){
  if (!gfx || !readTouch) return;
  int16_t x=0,y=0; bool down=false;
  static bool touching=false; static int16_t lx=0,ly=0;
  if (readTouch(x,y,down) && down){ touching=true; lx=x; ly=y; return; }
  if (!down && touching){
    touching=false;
    if (st.currentPage==0){
      if (lx>=210 && lx<310 && ly>=145 && ly<170){ st.currentPage=1; gfx->fillScreen(BLACK); if (onRenderSettings) onRenderSettings(); else drawSettingsPage(m1Speed,m2Speed); return; }
    } else {
      if (lx>=10 && lx<70 && ly>=145 && ly<170){ st.currentPage=0; gfx->fillScreen(BLACK); if (onRenderMain) onRenderMain(); else drawStaticUI(); return; }
      if (lx>=150 && lx<190 && ly>=45 && ly<65){ if (m1Speed>=5) m1Speed-=5; drawSettingsPage(m1Speed,m2Speed); return; }
      if (lx>=250 && lx<290 && ly>=45 && ly<65){ if (m1Speed<=95) m1Speed+=5; drawSettingsPage(m1Speed,m2Speed); return; }
      if (lx>=150 && lx<190 && ly>=75 && ly<95){ if (m2Speed>=5) m2Speed-=5; drawSettingsPage(m1Speed,m2Speed); return; }
      if (lx>=250 && lx<290 && ly>=75 && ly<95){ if (m2Speed<=95) m2Speed+=5; drawSettingsPage(m1Speed,m2Speed); return; }
      if (lx>=110 && lx<210 && ly>=120 && ly<150){ if (onSave) onSave(); st.currentPage=0; gfx->fillScreen(BLACK); if (onRenderMain) onRenderMain(); else drawStaticUI(); return; }
    }
    // Overlay interactions
    if (ovlActive) {
      const int OVL_X=12, OVL_Y=6, OVL_W=300, OVL_H=148;
      const int BTN_W=48, BTN_H=32;
      const int ROW1_Y = OVL_Y + 26;
      const int ROW2_Y = ROW1_Y + 48;
      const int COL_PLUS = OVL_X + 220;
      const int COL_MINUS= OVL_X + 170;
      // Save
      if (lx>=OVL_X+34 && lx<OVL_X+34+100 && ly>=OVL_Y+OVL_H-38 && ly<OVL_Y+OVL_H-38+BTN_H){
        ovlActive=false; ovl=OVL_NONE; if (onOverlaySave) onOverlaySave(); return; }
      // Cancel
      if (lx>=OVL_X+OVL_W-144 && lx<OVL_X+OVL_W-144+100 && ly>=OVL_Y+OVL_H-38 && ly<OVL_Y+OVL_H-38+BTN_H){
        ovlActive=false; ovl=OVL_NONE; if (onOverlayCancel) onOverlayCancel(); return; }
      // +/- buttons adjust
      const float phStep = 0.05f; const int orpStep = 10;
      if (lx>=COL_MINUS && lx<COL_MINUS+BTN_W && ly>=ROW1_Y-8 && ly<ROW1_Y-8+BTN_H){
        if (ovl==OVL_PH) { phMin-=phStep; } else { orpMin-=orpStep; }
        showRangeOverlay(ovl==OVL_PH, phMin, phMax, orpMin, orpMax); return;
      }
      if (lx>=COL_PLUS && lx<COL_PLUS+BTN_W && ly>=ROW1_Y-8 && ly<ROW1_Y-8+BTN_H){
        if (ovl==OVL_PH) { phMin+=phStep; } else { orpMin+=orpStep; }
        showRangeOverlay(ovl==OVL_PH, phMin, phMax, orpMin, orpMax); return;
      }
      if (lx>=COL_MINUS && lx<COL_MINUS+BTN_W && ly>=ROW2_Y-8 && ly<ROW2_Y-8+BTN_H){
        if (ovl==OVL_PH) { phMax-=phStep; } else { orpMax-=orpStep; }
        showRangeOverlay(ovl==OVL_PH, phMin, phMax, orpMin, orpMax); return;
      }
      if (lx>=COL_PLUS && lx<COL_PLUS+BTN_W && ly>=ROW2_Y-8 && ly<ROW2_Y-8+BTN_H){
        if (ovl==OVL_PH) { phMax+=phStep; } else { orpMax+=orpStep; }
        showRangeOverlay(ovl==OVL_PH, phMin, phMax, orpMin, orpMax); return;
      }
    }
  }
}

} // namespace ui

#else
// S3 build: no implementation required
namespace ui {}
#endif


