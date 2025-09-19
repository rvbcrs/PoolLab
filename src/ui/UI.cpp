#include "UI.h"
#include <Arduino.h>
#include <esp_log.h>
#include <WiFi.h>
#include <math.h>
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
static lv_obj_t *lv_lbl_val_m1 = nullptr;
static lv_obj_t *lv_lbl_val_m2 = nullptr;
static lv_obj_t *lv_lbl_saved_ssid = nullptr;
static lv_obj_t *lv_lbl_saved_pass = nullptr;
static lv_obj_t *lv_ta_ssid = nullptr;
static lv_obj_t *lv_ta_pass = nullptr;
static lv_obj_t *lv_lbl_ssid = nullptr;
static bool onSettings = false;
static uint8_t initial_m1=60, initial_m2=60;
static Handlers handlers;
static bool initial_mode_zigbee = true;

void init(lv_disp_t* disp){ (void)disp; }

void build(bool safeBaseline){
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
  if (safeBaseline){
    // Static captions for clarity
    lv_obj_t *cap_ph = lv_label_create(scr);
    lv_label_set_text(cap_ph, "pH");
    lv_obj_align(cap_ph, LV_ALIGN_LEFT_MID, 10, -40);
    lv_obj_t *cap_orp = lv_label_create(scr);
    lv_label_set_text(cap_orp, "ORP");
    lv_obj_align(cap_orp, LV_ALIGN_RIGHT_MID, -60, -40);
    lv_obj_t *cap_orp_unit = lv_label_create(scr);
    lv_label_set_text(cap_orp_unit, "mV");
    lv_obj_align(cap_orp_unit, LV_ALIGN_RIGHT_MID, -10, -40);

    // Dynamic values
    lv_obj_t *lbl1 = lv_label_create(scr);
    lv_label_set_text(lbl1, "--.--");
    lv_obj_align(lbl1, LV_ALIGN_LEFT_MID, 10, -10);
    lv_obj_set_style_text_color(lbl1, lv_color_white(), 0);
    lv_lbl_ph = lbl1;
    lv_obj_t *lbl2 = lv_label_create(scr);
    lv_label_set_text(lbl2, "----");
    lv_obj_align(lbl2, LV_ALIGN_RIGHT_MID, -60, -10);
    lv_obj_set_style_text_color(lbl2, lv_color_white(), 0);
    lv_lbl_orp = lbl2;
    // keep unit separate if needed later
    lv_lbl_orp_unit = nullptr;

    // Temperature label under pH
    lv_lbl_temp = lv_label_create(scr);
    lv_label_set_text(lv_lbl_temp, "--.- C");
    lv_obj_align(lv_lbl_temp, LV_ALIGN_LEFT_MID, 10, 24);
    lv_obj_set_style_text_color(lv_lbl_temp, lv_color_white(), 0);

    // IP bottom-right
    lv_lbl_ip = lv_label_create(scr);
    lv_label_set_text(lv_lbl_ip, "IP: --");
    lv_obj_align(lv_lbl_ip, LV_ALIGN_BOTTOM_RIGHT, -14, -1);

    // Settings button full width
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, lv_obj_get_width(scr)-20, 44);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -34);
    lv_obj_t *lblb = lv_label_create(btn); lv_label_set_text(lblb, "Settings"); lv_obj_center(lblb);
    if (handlers.onSettings) {
      lv_obj_add_event_cb(btn, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) handlers.onSettings(); }, LV_EVENT_ALL, NULL);
    }

    // Compact slider panel at top
    lv_obj_t *panel = lv_obj_create(scr);
    lv_obj_set_size(panel, lv_obj_get_width(scr)-20, 90);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_opa(panel, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(panel, 6, 0);

    lv_obj_t *lblm1 = lv_label_create(panel); lv_label_set_text(lblm1, "pH Motor %"); lv_obj_align(lblm1, LV_ALIGN_LEFT_MID, 0, 14);
    lv_slider_m1 = lv_slider_create(panel); lv_obj_set_size(lv_slider_m1, lv_obj_get_width(panel)-110, 12); lv_obj_align(lv_slider_m1, LV_ALIGN_LEFT_MID, 100, 14); lv_slider_set_range(lv_slider_m1, 0, 100); lv_slider_set_value(lv_slider_m1, initial_m1, LV_ANIM_OFF);

    lv_obj_t *lblm2 = lv_label_create(panel); lv_label_set_text(lblm2, "ORP Motor %"); lv_obj_align(lblm2, LV_ALIGN_LEFT_MID, 0, 44);
    lv_slider_m2 = lv_slider_create(panel); lv_obj_set_size(lv_slider_m2, lv_obj_get_width(panel)-110, 12); lv_obj_align(lv_slider_m2, LV_ALIGN_LEFT_MID, 100, 44); lv_slider_set_range(lv_slider_m2, 0, 100); lv_slider_set_value(lv_slider_m2, initial_m2, LV_ANIM_OFF);

    if (handlers.onSpeedChange) {
      lv_obj_add_event_cb(lv_slider_m1, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value(lv_event_get_target(e)); handlers.onSpeedChange(1,v);} }, LV_EVENT_ALL, NULL);
      lv_obj_add_event_cb(lv_slider_m2, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value(lv_event_get_target(e)); handlers.onSpeedChange(2,v);} }, LV_EVENT_ALL, NULL);
    }

    ESP_LOGI("UI","Baseline labels + sliders created");
    return;
  }
  // Minimal main labels in center (keeps build small for first split)
  // --- Web-style UI: three cards + wide Settings button ---
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  int scr_w = (int)lv_disp_get_hor_res(NULL);
  int scr_h = (int)lv_disp_get_ver_res(NULL);
  int pad = 12;

  // Container with padding
  lv_obj_t *root = lv_obj_create(scr);
  lv_obj_remove_style_all(root);
  lv_obj_set_size(root, scr_w - pad*2, scr_h - pad*2);
  lv_obj_align(root, LV_ALIGN_TOP_MID, 0, pad);
  lv_obj_set_style_pad_all(root, pad, 0);
  lv_obj_set_style_bg_color(root, lv_color_make(24,24,24), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(root, 14, 0);
  lv_obj_set_style_shadow_width(root, 16, 0);
  lv_obj_set_style_shadow_opa(root, LV_OPA_30, 0);

  // Horizontal row for three cards
  lv_obj_t *row = lv_obj_create(root);
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_set_style_pad_bottom(row, 8, 0);
  // Place row near top inside root
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);
  // Disable scrolling to ensure clicks are recognized
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  // Use screen width to compute thirds to avoid 0 width before layout
  auto make_card = [&](lv_color_t c1, const char *title)->lv_obj_t*{
    lv_obj_t *card = lv_obj_create(row);
    lv_obj_remove_style_all(card);
    lv_obj_set_style_bg_color(card, c1, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_coord_t gap = 6;
    lv_coord_t cw = (scr_w - (pad*2) - gap*2) / 3; // 3 cards with small gaps
    lv_obj_set_size(card, cw, 160);
    // No text title; icons will serve as titles
    return card;
  };

  lv_obj_t *card_ph  = make_card(lv_color_make(18,32,60), "pH");
  lv_obj_t *card_orp = make_card(lv_color_make(50,34,12), "ORP");
  lv_obj_t *card_tmp = make_card(lv_color_make(10,45,42), "Temp");

  // Large value labels + icons
  {
    lv_obj_t *icon_ph = lv_img_create(card_ph);
    lv_img_set_src(icon_ph, &water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40);
    lv_obj_align(icon_ph, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_img_recolor_opa(icon_ph, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(icon_ph, lv_color_white(), 0);
  }
  lv_lbl_ph  = lv_label_create(card_ph);  lv_obj_set_style_text_color(lv_lbl_ph, lv_color_white(), 0);  lv_label_set_text(lv_lbl_ph, "--.--");  lv_obj_set_style_text_font(lv_lbl_ph, &lv_font_montserrat_28, 0); lv_obj_align(lv_lbl_ph, LV_ALIGN_TOP_LEFT, 0, 44);
  {
    lv_obj_t *icon_orp = lv_img_create(card_orp);
    lv_img_set_src(icon_orp, &water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40);
    lv_obj_align(icon_orp, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_img_recolor_opa(icon_orp, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(icon_orp, lv_color_white(), 0);
  }
  lv_lbl_orp = lv_label_create(card_orp); lv_obj_set_style_text_color(lv_lbl_orp, lv_color_white(), 0); lv_label_set_text(lv_lbl_orp, "----");  lv_obj_set_style_text_font(lv_lbl_orp, &lv_font_montserrat_28, 0); lv_obj_align(lv_lbl_orp, LV_ALIGN_TOP_LEFT, 0, 44);
  lv_lbl_orp_unit = lv_label_create(card_orp); lv_obj_set_style_text_color(lv_lbl_orp_unit, lv_color_white(), 0); lv_label_set_text(lv_lbl_orp_unit, " mV"); lv_obj_align_to(lv_lbl_orp_unit, lv_lbl_orp, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
  lv_lbl_temp = lv_label_create(card_tmp); lv_obj_set_style_text_color(lv_lbl_temp, lv_color_white(), 0); lv_label_set_text(lv_lbl_temp, "--.- C"); lv_obj_set_style_text_font(lv_lbl_temp, &lv_font_montserrat_28, 0); lv_obj_align(lv_lbl_temp, LV_ALIGN_TOP_LEFT, 0, 44);
  // If needed, we can reduce widths further on small screens to avoid clipping

  // Subtext lines (Target / MinMax)
  lv_obj_t *ph_sub = lv_label_create(card_ph);  lv_obj_set_style_text_color(ph_sub, lv_palette_lighten(LV_PALETTE_GREY,3), 0); lv_label_set_text(ph_sub, "Target: 6.80 - 7.60"); lv_obj_align(ph_sub, LV_ALIGN_TOP_LEFT, 0, 76);
  lv_obj_t *or_sub = lv_label_create(card_orp); lv_obj_set_style_text_color(or_sub, lv_palette_lighten(LV_PALETTE_GREY,3), 0); lv_label_set_text(or_sub, "Min/Max: 250 / 850"); lv_obj_align(or_sub, LV_ALIGN_TOP_LEFT, 0, 76);

  // Settings button (full width) stick to bottom
  lv_obj_t *btn = lv_btn_create(root);
  lv_obj_set_width(btn, LV_PCT(100));
  lv_obj_set_height(btn, 44);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *lblb = lv_label_create(btn); lv_label_set_text(lblb, "Settings"); lv_obj_center(lblb);
  lv_obj_add_event_cb(btn, [](lv_event_t *e){
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
      ESP_LOGI("UI", "Settings clicked");
      if (handlers.onSettings) handlers.onSettings();
    }
  }, LV_EVENT_ALL, NULL);

  // IP label above the Settings button, aligned to left
  lv_lbl_ip = lv_label_create(root);
  lv_obj_set_style_text_color(lv_lbl_ip, lv_palette_lighten(LV_PALETTE_GREY,3), 0);
  lv_label_set_text(lv_lbl_ip, "IP: --");
  lv_obj_align_to(lv_lbl_ip, btn, LV_ALIGN_OUT_TOP_LEFT, 0, -6);
  // SSID label above IP
  lv_lbl_ssid = lv_label_create(root);
  lv_obj_set_style_text_color(lv_lbl_ssid, lv_palette_lighten(LV_PALETTE_GREY,3), 0);
  lv_label_set_text(lv_lbl_ssid, "SSID: --");
  lv_obj_align_to(lv_lbl_ssid, lv_lbl_ip, LV_ALIGN_OUT_TOP_LEFT, 0, -6);
  // Mode switch omitted here to keep layout matching web UI

  if (handlers.onSpeedChange) {
    // No sliders on this screen; settings button opens portal
  }

  // Periodic UI update timer to keep values fresh without cross-thread locks
  // Single periodic UI update timer (500ms). Ensure only one instance exists.
  static lv_timer_t *val_timer = NULL;
  if (!val_timer) {
    val_timer = lv_timer_create([](lv_timer_t *t){ (void)t; ui::updateValues(); }, 500, NULL);
    // Run once immediately for initial paint
    ui::updateValues();
  }
}

void updateValues(){
  // Defensive: ensure labels exist before touching
  if (onSettings) return;
  auto &M = domain::Metrics::instance();
  // pH as integer + 2 decimals (avoid float printf)
  if (lv_lbl_ph) {
    if (M.havePh) {
      int ph100 = (int)((M.phVal * 100.0f) + (M.phVal >= 0 ? 0.5f : -0.5f));
      int ph_i = ph100 / 100; int ph_f = abs(ph100 % 100);
      char b[16]; snprintf(b, sizeof(b), "%d.%02d", ph_i, ph_f);
      lv_label_set_text(lv_lbl_ph, b);
    } else {
      lv_label_set_text(lv_lbl_ph, "--.--");
    }
  }
  // ORP as integer
  if (lv_lbl_orp) {
    if (M.haveOrp) {
      char b[16]; snprintf(b, sizeof(b), "%d", (int)(M.orpMv >= 0 ? (M.orpMv + 0.5f) : (M.orpMv - 0.5f)));
      lv_label_set_text(lv_lbl_orp, b);
      if (lv_lbl_orp_unit) lv_obj_clear_flag(lv_lbl_orp_unit, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(lv_lbl_orp, "----");
    }
  }
  // Temp with 1 decimal and unit
  if (lv_lbl_temp) {
    if (M.haveTemp) {
      int t10 = (int)((M.tempC * 10.0f) + (M.tempC >= 0 ? 0.5f : -0.5f));
      int t_i = t10 / 10; int t_f = abs(t10 % 10);
      char b[20]; snprintf(b, sizeof(b), "%d.%d C", t_i, t_f);
      lv_label_set_text(lv_lbl_temp, b);
    } else {
      lv_label_set_text(lv_lbl_temp, "--.- C");
    }
  }
  // Refresh IP/SSID on each tick to restore after returning from settings
  if (lv_lbl_ip) {
    String ip = (WiFi.status()==WL_CONNECTED)? WiFi.localIP().toString() : String("--");
    char bi[64]; snprintf(bi, sizeof(bi), "IP: %s", ip.c_str());
    lv_label_set_text(lv_lbl_ip, bi);
  }
  if (lv_lbl_ssid) {
    String s = (WiFi.status()==WL_CONNECTED)? WiFi.SSID() : String("--");
    char bs[96]; snprintf(bs, sizeof(bs), "SSID: %s", s.c_str());
    lv_label_set_text(lv_lbl_ssid, bs);
  }
}

void setIp(const char *ipText){
  if (!lv_lbl_ip) return;
  if (!ipText) ipText = "--";
  char b[64]; snprintf(b, sizeof(b), "IP: %s", ipText);
  lv_label_set_text(lv_lbl_ip, b);
}

void setSsid(const char *ssid){
  if (!lv_lbl_ssid) return;
  if (!ssid || !ssid[0]) ssid = "--";
  char b[96]; snprintf(b, sizeof(b), "SSID: %s", ssid);
  lv_label_set_text(lv_lbl_ssid, b);
}

void showSettings(){
  // In-app settings with scrollable content and sticky footer
  onSettings = true;
  // Invalidate main labels so async timers do not touch freed objects
  lv_lbl_ph = lv_lbl_orp = lv_lbl_orp_unit = lv_lbl_temp = lv_lbl_ip = lv_lbl_ssid = nullptr;
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);

  const lv_coord_t scr_w = lv_disp_get_hor_res(NULL);
  const lv_coord_t scr_h = lv_disp_get_ver_res(NULL);
  const lv_coord_t pad = 12;
  const lv_coord_t footer_h = 48;

  // Scrollable content area
  lv_obj_t *content = lv_obj_create(scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_size(content, scr_w - pad*2, scr_h - pad*2 - footer_h);
  lv_obj_align(content, LV_ALIGN_TOP_MID, 0, pad);
  lv_obj_set_style_bg_color(content, lv_color_make(24,24,24), 0);
  lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(content, 12, 0);
  lv_obj_set_style_pad_all(content, pad, 0);
  // scroll enabled by default; ensure vertical scroll
  lv_obj_set_scroll_dir(content, LV_DIR_VER);

  lv_obj_t *title = lv_label_create(content);
  lv_label_set_text(title, "Settings");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  // pH slider
  lv_obj_t *lblm1 = lv_label_create(content); lv_label_set_text(lblm1, "pH Motor Speed"); lv_obj_align(lblm1, LV_ALIGN_TOP_LEFT, 0, 28);
  lv_lbl_val_m1 = lv_label_create(content); lv_label_set_text(lv_lbl_val_m1, "60%"); lv_obj_align(lv_lbl_val_m1, LV_ALIGN_TOP_RIGHT, 0, 28);
  lv_slider_m1 = lv_slider_create(content); lv_obj_set_width(lv_slider_m1, lv_pct(100)); lv_obj_align(lv_slider_m1, LV_ALIGN_TOP_LEFT, 0, 48);
  lv_slider_set_range(lv_slider_m1, 0, 100);
  lv_slider_set_value(lv_slider_m1, initial_m1, LV_ANIM_OFF);

  // ORP slider
  lv_obj_t *lblm2 = lv_label_create(content); lv_label_set_text(lblm2, "ORP Motor Speed"); lv_obj_align(lblm2, LV_ALIGN_TOP_LEFT, 0, 88);
  lv_lbl_val_m2 = lv_label_create(content); lv_label_set_text(lv_lbl_val_m2, "60%"); lv_obj_align(lv_lbl_val_m2, LV_ALIGN_TOP_RIGHT, 0, 88);
  lv_slider_m2 = lv_slider_create(content); lv_obj_set_width(lv_slider_m2, lv_pct(100)); lv_obj_align(lv_slider_m2, LV_ALIGN_TOP_LEFT, 0, 108);
  lv_slider_set_range(lv_slider_m2, 0, 100);
  lv_slider_set_value(lv_slider_m2, initial_m2, LV_ANIM_OFF);

  lv_obj_add_event_cb(lv_slider_m1, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value(lv_event_get_target(e)); if (lv_lbl_val_m1){ char b[8]; snprintf(b,sizeof(b),"%d%%",v); lv_label_set_text(lv_lbl_val_m1,b);} if (handlers.onSpeedChange) handlers.onSpeedChange(1,v);} }, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(lv_slider_m2, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value(lv_event_get_target(e)); if (lv_lbl_val_m2){ char b[8]; snprintf(b,sizeof(b),"%d%%",v); lv_label_set_text(lv_lbl_val_m2,b);} if (handlers.onSpeedChange) handlers.onSpeedChange(2,v);} }, LV_EVENT_VALUE_CHANGED, NULL);

  // Editable WiFi fields (prefilled later via setSavedWifi)
  lv_obj_t *lblEdSsid = lv_label_create(content); lv_label_set_text(lblEdSsid, "WiFi SSID"); lv_obj_align(lblEdSsid, LV_ALIGN_TOP_LEFT, 0, 148);
  lv_ta_ssid = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_ssid, true); lv_obj_set_width(lv_ta_ssid, lv_pct(100)); lv_obj_align(lv_ta_ssid, LV_ALIGN_TOP_LEFT, 0, 168);
  lv_obj_t *lblEdPass = lv_label_create(content); lv_label_set_text(lblEdPass, "WiFi Password"); lv_obj_align(lblEdPass, LV_ALIGN_TOP_LEFT, 0, 202);
  lv_ta_pass = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_pass, true); lv_textarea_set_password_mode(lv_ta_pass, true); lv_obj_set_width(lv_ta_pass, lv_pct(100)); lv_obj_align(lv_ta_pass, LV_ALIGN_TOP_LEFT, 0, 222);

  // Sticky footer
  lv_obj_t *footer = lv_obj_create(scr);
  lv_obj_remove_style_all(footer);
  lv_obj_set_size(footer, scr_w - pad*2, footer_h);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -pad);
  lv_obj_set_style_bg_color(footer, lv_color_make(32,32,32), 0);
  lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(footer, 12, 0);
  lv_obj_set_style_pad_all(footer, 8, 0);
  lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(footer, 10, 0);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(footer, LV_DIR_NONE);

  lv_obj_t *btnWifi = lv_btn_create(footer); lv_obj_set_size(btnWifi, (scr_w - pad*2 - 20)/3, footer_h-16); lv_obj_t *lblw = lv_label_create(btnWifi); lv_label_set_text(lblw, "Reset WiFi"); lv_obj_center(lblw);
  lv_obj_add_event_cb(btnWifi, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) { if (handlers.onWifiReset) handlers.onWifiReset(); } }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btnSave = lv_btn_create(footer); lv_obj_set_size(btnSave, (scr_w - pad*2 - 20)/3, footer_h-16); lv_obj_t *lbls = lv_label_create(btnSave); lv_label_set_text(lbls, "Save WiFi"); lv_obj_center(lbls);
  lv_obj_add_event_cb(btnSave, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) { if (handlers.onWifiSave) { const char *s = lv_textarea_get_text(lv_ta_ssid); const char *p = lv_textarea_get_text(lv_ta_pass); handlers.onWifiSave(s, p); } } }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btnBack = lv_btn_create(footer); lv_obj_set_size(btnBack, (scr_w - pad*2 - 20)/3, footer_h-16); lv_obj_t *lblb = lv_label_create(btnBack); lv_label_set_text(lblb, "Back"); lv_obj_center(lblb);
  lv_obj_add_event_cb(btnBack, [](lv_event_t *e){ auto code=lv_event_get_code(e); if (code==LV_EVENT_CLICKED || code==LV_EVENT_SHORT_CLICKED || code==LV_EVENT_RELEASED){ ui::showMain(); } }, LV_EVENT_ALL, NULL);
}

void showMain(){
  onSettings = false;
  lv_obj_clean(lv_scr_act());
  ui::build(false);
  // immediate refresh so values/SSID/IP terug zijn zonder te wachten op timer
  ui::updateValues();
}

void setSavedWifi(const char *ssid, const char *pass){
  if (lv_ta_ssid) lv_textarea_set_text(lv_ta_ssid, (ssid&&ssid[0])?ssid:"");
  if (lv_ta_pass) lv_textarea_set_text(lv_ta_pass, (pass&&pass[0])?pass:"");
}

void configureHandlers(const Handlers &h){ handlers = h; }
void setInitialSpeeds(uint8_t m1, uint8_t m2){ initial_m1=m1; initial_m2=m2; }
void setInitialMode(bool zigbee){ initial_mode_zigbee = zigbee; }


} // namespace ui


