#include "UI.h"
#include <Arduino.h>
#include "domain/Metrics.h"

namespace ui {

static lv_obj_t *lv_tile_main = nullptr;
static lv_obj_t *lv_tile_settings = nullptr;
static lv_obj_t *lv_lbl_ph = nullptr;
static lv_obj_t *lv_lbl_orp = nullptr;
static lv_obj_t *lv_lbl_orp_unit = nullptr;
static lv_obj_t *lv_lbl_temp = nullptr;
static lv_obj_t *lv_lbl_ip = nullptr;
static lv_obj_t *lv_slider_m1 = nullptr;
static lv_obj_t *lv_slider_m2 = nullptr;
static uint8_t initial_m1=60, initial_m2=60;
static Handlers handlers;
static bool initial_mode_zigbee = true;

void init(lv_disp_t* disp){ (void)disp; }

void build(bool safeBaseline){
  lv_obj_t *scr = lv_scr_act();
  if (safeBaseline){
    lv_obj_t *lbl1 = lv_label_create(scr);
    lv_label_set_text(lbl1, "pH --.--");
    lv_obj_align(lbl1, LV_ALIGN_LEFT_MID, 10, -10);
    lv_lbl_ph = lbl1;
    lv_obj_t *lbl2 = lv_label_create(scr);
    lv_label_set_text(lbl2, "ORP ---- mV");
    lv_obj_align(lbl2, LV_ALIGN_RIGHT_MID, -10, -10);
    lv_lbl_orp = lbl2;
    lv_lbl_ip = lv_label_create(scr);
    lv_label_set_text(lv_lbl_ip, "IP: --");
    lv_obj_align(lv_lbl_ip, LV_ALIGN_BOTTOM_RIGHT, -14, -1);
    return;
  }
  // Minimal main labels in center (keeps build small for first split)
  lv_lbl_ph = lv_label_create(scr); lv_obj_align(lv_lbl_ph, LV_ALIGN_LEFT_MID, 16, -8); lv_label_set_text(lv_lbl_ph, "--.--");
  lv_lbl_orp = lv_label_create(scr); lv_obj_align(lv_lbl_orp, LV_ALIGN_RIGHT_MID, -40, -8); lv_label_set_text(lv_lbl_orp, "----");
  lv_lbl_orp_unit = lv_label_create(scr); lv_label_set_text(lv_lbl_orp_unit, " mV"); lv_obj_align_to(lv_lbl_orp_unit, lv_lbl_orp, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
  lv_lbl_temp = lv_label_create(scr); lv_obj_align(lv_lbl_temp, LV_ALIGN_LEFT_MID, 16, 28); lv_label_set_text(lv_lbl_temp, "--.- C");
  lv_lbl_ip = lv_label_create(scr); lv_obj_align(lv_lbl_ip, LV_ALIGN_BOTTOM_RIGHT, -14, -1); lv_label_set_text(lv_lbl_ip, "IP: --");

  // Settings section with sliders (compact, bovenaan)
  lv_obj_t *panel = lv_obj_create(scr); lv_obj_set_size(panel, lv_obj_get_width(scr)-20, 106); lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_bg_opa(panel, LV_OPA_20, 0);
  lv_obj_set_style_pad_all(panel, 6, 0);

  // Mode toggle (Zigbee vs WiFi/MQTT)
  lv_obj_t *lblMode = lv_label_create(panel); lv_label_set_text(lblMode, "Mode:"); lv_obj_align(lblMode, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *swMode = lv_switch_create(panel); lv_obj_align(swMode, LV_ALIGN_TOP_RIGHT, -10, 0);
  if (initial_mode_zigbee) lv_obj_add_state(swMode, LV_STATE_CHECKED); else lv_obj_clear_state(swMode, LV_STATE_CHECKED);
  if (handlers.onModeToggle) {
    lv_obj_add_event_cb(swMode, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ bool zig = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED); handlers.onModeToggle(zig);} }, LV_EVENT_ALL, NULL);
  }

  lv_obj_t *lblm1 = lv_label_create(panel); lv_label_set_text(lblm1, "pH Motor %"); lv_obj_align(lblm1, LV_ALIGN_LEFT_MID, 0, 14);
  lv_slider_m1 = lv_slider_create(panel); lv_obj_set_size(lv_slider_m1, lv_obj_get_width(panel)-110, 12); lv_obj_align(lv_slider_m1, LV_ALIGN_LEFT_MID, 100, 14); lv_slider_set_range(lv_slider_m1, 0, 100); lv_slider_set_value(lv_slider_m1, initial_m1, LV_ANIM_OFF);
  lv_obj_t *lblm2 = lv_label_create(panel); lv_label_set_text(lblm2, "ORP Motor %"); lv_obj_align(lblm2, LV_ALIGN_LEFT_MID, 0, 44);
  lv_slider_m2 = lv_slider_create(panel); lv_obj_set_size(lv_slider_m2, lv_obj_get_width(panel)-110, 12); lv_obj_align(lv_slider_m2, LV_ALIGN_LEFT_MID, 100, 44); lv_slider_set_range(lv_slider_m2, 0, 100); lv_slider_set_value(lv_slider_m2, initial_m2, LV_ANIM_OFF);
  if (handlers.onSpeedChange) {
    lv_obj_add_event_cb(lv_slider_m1, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value(lv_event_get_target(e)); handlers.onSpeedChange(1,v);} }, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(lv_slider_m2, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value(lv_event_get_target(e)); handlers.onSpeedChange(2,v);} }, LV_EVENT_ALL, NULL);
  }
}

void updateValues(){
  auto &M = domain::Metrics::instance();
  if (lv_lbl_ph)   { lv_label_set_text_fmt(lv_lbl_ph,   M.havePh? "%.2f" : "--.--", (double)M.phVal); }
  if (lv_lbl_orp)  { if (M.haveOrp) { lv_label_set_text_fmt(lv_lbl_orp, "%d", (int)lrintf(M.orpMv)); if (lv_lbl_orp_unit) lv_obj_clear_flag(lv_lbl_orp_unit, LV_OBJ_FLAG_HIDDEN);} else { lv_label_set_text(lv_lbl_orp, "----"); } }
  if (lv_lbl_temp) { lv_label_set_text_fmt(lv_lbl_temp, M.haveTemp? "%.1f C" : "--.- C", (double)M.tempC); }
}

void configureHandlers(const Handlers &h){ handlers = h; }
void setInitialSpeeds(uint8_t m1, uint8_t m2){ initial_m1=m1; initial_m2=m2; }
void setInitialMode(bool zigbee){ initial_mode_zigbee = zigbee; }


} // namespace ui


