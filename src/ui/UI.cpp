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
static lv_obj_t *lv_ta_mqtt_host = nullptr;
static lv_obj_t *lv_ta_mqtt_port = nullptr;
static lv_obj_t *lv_ta_mqtt_user = nullptr;
static lv_obj_t *lv_ta_mqtt_pw = nullptr;
static lv_obj_t *lv_ta_ssid = nullptr;
static lv_obj_t *lv_ta_pass = nullptr;
static lv_obj_t *lv_lbl_ssid = nullptr;
static bool onSettings = false;
static uint8_t initial_m1=60, initial_m2=60;
static Handlers handlers;
static bool initial_mode_zigbee = true;
static lv_obj_t *lv_modal_active = nullptr; // generic modal holder

void init(lv_disp_t* disp){ (void)disp; }
static lv_timer_t *g_update_timer = nullptr;

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

    // Ensure periodic value updates
    if (!g_update_timer) {
      g_update_timer = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::updateValues(); }, 500, NULL);
    }
    //ESP_LOGI("UI","Baseline labels + sliders created");
    return;
  }
  // Minimal main labels in center (keeps build small for first split)
  // --- Web-style UI: three cards + IP + Settings ---
  // Slightly lighter background (38,38,38)
  lv_obj_set_style_bg_color(scr, lv_color_make(46,46,46), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  int scr_w = (int)lv_disp_get_hor_res(NULL);
  int scr_h = (int)lv_disp_get_ver_res(NULL);
  int pad = 12;
  int col_gap = 12;

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
  lv_obj_set_style_pad_column(row, col_gap, 0);
  lv_obj_set_style_pad_bottom(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  auto make_card = [&](lv_color_t c1)->lv_obj_t*{
    lv_obj_t *card = lv_obj_create(row);
    lv_obj_remove_style_all(card);
    lv_obj_set_style_bg_color(card, c1, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    // Compute inner width of root: (scr_w - pad*2) is root size, minus its own padding again
    lv_coord_t inner_w = (scr_w - (pad*2)) - (pad*2);
    lv_coord_t cw = (inner_w - (col_gap * 2)) / 3; // three equal tiles within width
    lv_obj_set_size(card, cw, 160);
    return card;
  };

  // Slightly darker, less saturated tile colors
  lv_obj_t *card_ph  = make_card(lv_palette_darken(LV_PALETTE_BLUE, 3));
  lv_obj_t *card_orp = make_card(lv_palette_darken(LV_PALETTE_AMBER, 3));
  lv_obj_t *card_tmp = make_card(lv_palette_darken(LV_PALETTE_TEAL, 3));

  // Icons + labels
  lv_obj_t *icon_ph = lv_img_create(card_ph); lv_img_set_src(icon_ph, &water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40); lv_obj_align(icon_ph, LV_ALIGN_TOP_LEFT, 0, 0); lv_obj_set_style_img_recolor_opa(icon_ph, LV_OPA_COVER, 0); lv_obj_set_style_img_recolor(icon_ph, lv_color_white(), 0);
  lv_lbl_ph  = lv_label_create(card_ph);  lv_obj_set_style_text_color(lv_lbl_ph, lv_color_white(), 0);  lv_label_set_text(lv_lbl_ph, "--.--");  lv_obj_set_style_text_font(lv_lbl_ph, &lv_font_montserrat_28, 0); lv_obj_align(lv_lbl_ph, LV_ALIGN_TOP_LEFT, 0, 44);

  lv_obj_t *icon_orp = lv_img_create(card_orp); lv_img_set_src(icon_orp, &water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40); lv_obj_align(icon_orp, LV_ALIGN_TOP_LEFT, 0, 0); lv_obj_set_style_img_recolor_opa(icon_orp, LV_OPA_COVER, 0); lv_obj_set_style_img_recolor(icon_orp, lv_color_white(), 0);
  lv_lbl_orp = lv_label_create(card_orp); lv_obj_set_style_text_color(lv_lbl_orp, lv_color_white(), 0); lv_label_set_text(lv_lbl_orp, "----");  lv_obj_set_style_text_font(lv_lbl_orp, &lv_font_montserrat_28, 0); lv_obj_align(lv_lbl_orp, LV_ALIGN_TOP_LEFT, 0, 44);
  lv_lbl_orp_unit = lv_label_create(card_orp); lv_obj_set_style_text_color(lv_lbl_orp_unit, lv_color_white(), 0); lv_label_set_text(lv_lbl_orp_unit, " mV"); lv_obj_align_to(lv_lbl_orp_unit, lv_lbl_orp, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

  // Temp icon + value label
  lv_obj_t *icon_tmp = lv_img_create(card_tmp); lv_img_set_src(icon_tmp, &device_thermostat_32dp_999999_FILL0_wght400_GRAD0_opsz40); lv_obj_align(icon_tmp, LV_ALIGN_TOP_LEFT, 0, 0); lv_obj_set_style_img_recolor_opa(icon_tmp, LV_OPA_COVER, 0); lv_obj_set_style_img_recolor(icon_tmp, lv_color_white(), 0);
  lv_lbl_temp = lv_label_create(card_tmp); lv_obj_set_style_text_color(lv_lbl_temp, lv_color_white(), 0); lv_label_set_text(lv_lbl_temp, "--.- C"); lv_obj_set_style_text_font(lv_lbl_temp, &lv_font_montserrat_28, 0); lv_obj_align(lv_lbl_temp, LV_ALIGN_TOP_LEFT, 0, 44);

  // Subtext: two-line layout (label, then values)
  lv_obj_t *ph_sub_lbl = lv_label_create(card_ph);  lv_obj_set_style_text_color(ph_sub_lbl, lv_palette_lighten(LV_PALETTE_GREY,3), 0); lv_label_set_text(ph_sub_lbl, "Target"); lv_obj_align(ph_sub_lbl, LV_ALIGN_TOP_LEFT, 0, 78);
  lv_obj_t *ph_sub_vals = lv_label_create(card_ph); lv_obj_set_style_text_color(ph_sub_vals, lv_palette_lighten(LV_PALETTE_GREY,2), 0); lv_label_set_text(ph_sub_vals, "6.80 - 7.60"); lv_obj_align(ph_sub_vals, LV_ALIGN_TOP_LEFT, 0, 96);
  lv_obj_t *or_sub_lbl = lv_label_create(card_orp); lv_obj_set_style_text_color(or_sub_lbl, lv_palette_lighten(LV_PALETTE_GREY,3), 0); lv_label_set_text(or_sub_lbl, "Min/Max"); lv_obj_align(or_sub_lbl, LV_ALIGN_TOP_LEFT, 0, 78);
  lv_obj_t *or_sub_vals = lv_label_create(card_orp); lv_obj_set_style_text_color(or_sub_vals, lv_palette_lighten(LV_PALETTE_GREY,2), 0); lv_label_set_text(or_sub_vals, "250 / 850"); lv_obj_align(or_sub_vals, LV_ALIGN_TOP_LEFT, 0, 96);

  // Click handlers to open range editor modals
  lv_obj_add_flag(card_ph, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card_ph, [](lv_event_t *e){
    if (lv_event_get_code(e)==LV_EVENT_CLICKED) {
      // Defer modal creation to next tick to avoid re-entrancy during event processing
      lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showRangeEditor(true); }, 0, NULL);
      lv_timer_set_repeat_count(t, 1);
    }
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(card_orp, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card_orp, [](lv_event_t *e){
    if (lv_event_get_code(e)==LV_EVENT_CLICKED) {
      lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showRangeEditor(false); }, 0, NULL);
      lv_timer_set_repeat_count(t, 1);
    }
  }, LV_EVENT_CLICKED, NULL);

  // Settings button
  lv_obj_t *btn = lv_btn_create(root); lv_obj_set_width(btn, LV_PCT(100)); lv_obj_set_height(btn, 44); lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -8); lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0); lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *lblb = lv_label_create(btn); lv_label_set_text(lblb, "Settings"); lv_obj_center(lblb);
  lv_obj_add_event_cb(btn, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) { if (handlers.onSettings) handlers.onSettings(); } }, LV_EVENT_CLICKED, NULL);

  // IP label (content width), placed just above the Settings button on the left (slightly higher)
  lv_lbl_ip = lv_label_create(root); lv_obj_set_style_text_color(lv_lbl_ip, lv_palette_lighten(LV_PALETTE_GREY, 3), 0); lv_obj_set_style_text_font(lv_lbl_ip, &lv_font_montserrat_14, 0); lv_label_set_long_mode(lv_lbl_ip, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lv_lbl_ip, LV_SIZE_CONTENT); lv_obj_set_style_text_align(lv_lbl_ip, LV_TEXT_ALIGN_LEFT, 0); lv_obj_align_to(lv_lbl_ip, btn, LV_ALIGN_OUT_TOP_LEFT, 0, -10); lv_label_set_text(lv_lbl_ip, "IP: --");

  // SSID label above IP (stacked), left aligned
  lv_lbl_ssid = lv_label_create(root);
  lv_obj_set_style_text_color(lv_lbl_ssid, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
  lv_obj_set_style_text_font(lv_lbl_ssid, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(lv_lbl_ssid, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lv_lbl_ssid, LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(lv_lbl_ssid, LV_TEXT_ALIGN_LEFT, 0);
  // place directly above the IP label on the left (slightly higher)
  lv_obj_align_to(lv_lbl_ssid, lv_lbl_ip, LV_ALIGN_OUT_TOP_LEFT, 0, -6);
  lv_label_set_text(lv_lbl_ssid, "SSID: --");

  // Ensure periodic value updates
  if (!g_update_timer) {
    g_update_timer = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::updateValues(); }, 500, NULL);
  }
  // Immediate first refresh
  ui::updateValues();
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
  // scroll enabled by default; ensure vertical scroll and faster throw
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *title = lv_label_create(content);
  lv_label_set_text(title, "Settings");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  // pH slider
  lv_obj_t *lblm1 = lv_label_create(content); lv_label_set_text(lblm1, "pH Motor Speed"); lv_obj_align(lblm1, LV_ALIGN_TOP_LEFT, 0, 28);
  lv_lbl_val_m1 = lv_label_create(content); { char b[8]; snprintf(b, sizeof(b), "%u%%", (unsigned)initial_m1); lv_label_set_text(lv_lbl_val_m1, b);} lv_obj_align(lv_lbl_val_m1, LV_ALIGN_TOP_RIGHT, 0, 28);
  lv_slider_m1 = lv_slider_create(content); lv_obj_set_width(lv_slider_m1, lv_pct(100)); lv_obj_align(lv_slider_m1, LV_ALIGN_TOP_LEFT, 0, 48);
  // Let vertical swipe gestures bubble to the scrollable parent instead of only moving the slider
  lv_obj_add_flag(lv_slider_m1, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_flag(lv_slider_m1, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_slider_set_range(lv_slider_m1, 0, 100);
  lv_slider_set_value(lv_slider_m1, initial_m1, LV_ANIM_OFF);

  // ORP slider
  lv_obj_t *lblm2 = lv_label_create(content); lv_label_set_text(lblm2, "ORP Motor Speed"); lv_obj_align(lblm2, LV_ALIGN_TOP_LEFT, 0, 88);
  lv_lbl_val_m2 = lv_label_create(content); { char b[8]; snprintf(b, sizeof(b), "%u%%", (unsigned)initial_m2); lv_label_set_text(lv_lbl_val_m2, b);} lv_obj_align(lv_lbl_val_m2, LV_ALIGN_TOP_RIGHT, 0, 88);
  lv_slider_m2 = lv_slider_create(content); lv_obj_set_width(lv_slider_m2, lv_pct(100)); lv_obj_align(lv_slider_m2, LV_ALIGN_TOP_LEFT, 0, 108);
  // Bubble gestures/events so vertical swipes scroll the page
  lv_obj_add_flag(lv_slider_m2, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_flag(lv_slider_m2, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_slider_set_range(lv_slider_m2, 0, 100);
  lv_slider_set_value(lv_slider_m2, initial_m2, LV_ANIM_OFF);

  lv_obj_add_event_cb(lv_slider_m1, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value(lv_event_get_target(e)); if (lv_lbl_val_m1){ char b[8]; snprintf(b,sizeof(b),"%d%%",v); lv_label_set_text(lv_lbl_val_m1,b);} if (handlers.onSpeedChange) handlers.onSpeedChange(1,v);} }, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(lv_slider_m2, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value(lv_event_get_target(e)); if (lv_lbl_val_m2){ char b[8]; snprintf(b,sizeof(b),"%d%%",v); lv_label_set_text(lv_lbl_val_m2,b);} if (handlers.onSpeedChange) handlers.onSpeedChange(2,v);} }, LV_EVENT_VALUE_CHANGED, NULL);

  // Intercept vertical swipes on sliders to scroll the content instead of moving the slider
  struct ScrollInterceptCtx { lv_obj_t *scroll; int initial; int ax; int ay; bool adjust; };
  auto attach_interceptor = [&](lv_obj_t *slider){
    ScrollInterceptCtx *ctx = (ScrollInterceptCtx*)lv_mem_alloc(sizeof(ScrollInterceptCtx));
    ctx->scroll = content; ctx->initial = (int)lv_slider_get_value(slider); ctx->ax=0; ctx->ay=0; ctx->adjust=false;
    lv_obj_add_event_cb(slider, [](lv_event_t *e){
      auto code = lv_event_get_code(e);
      ScrollInterceptCtx *c = (ScrollInterceptCtx*)lv_event_get_user_data(e);
      lv_obj_t *sl = (lv_obj_t*)lv_event_get_target(e);
      if (code == LV_EVENT_PRESSED){
        c->initial = (int)lv_slider_get_value(sl); c->ax=0; c->ay=0; c->adjust=false;
      } else if (code == LV_EVENT_PRESSING){
        lv_indev_t *indev = lv_indev_get_act(); if (!indev) return; lv_point_t v; lv_indev_get_vect(indev, &v);
        c->ax += v.x>=0? v.x : -v.x; c->ay += v.y>=0? v.y : -v.y; // absolute values
        if (!c->adjust){
          if (c->ay > c->ax + 3){
            // Scroll vertically; keep slider fixed to initial
            if (c->scroll) lv_obj_scroll_by_bounded(c->scroll, 0, v.y, LV_ANIM_OFF);
            lv_slider_set_value(sl, c->initial, LV_ANIM_OFF);
            // prevent slider change side-effects
            lv_event_stop_bubbling(e); lv_event_stop_processing(e);
            return;
          } else if (c->ax > c->ay + 3){
            c->adjust = true; // enter real slider adjust mode (horizontal intent)
          } else {
            // undecided: keep slider fixed to prevent accidental value change
            lv_slider_set_value(sl, c->initial, LV_ANIM_OFF);
            lv_event_stop_bubbling(e); lv_event_stop_processing(e);
            return;
          }
        }
      } else if (code == LV_EVENT_VALUE_CHANGED){
        if (!c->adjust){
          // block value changes during vertical scroll
          lv_slider_set_value(sl, c->initial, LV_ANIM_OFF);
          lv_event_stop_bubbling(e); lv_event_stop_processing(e);
          return;
        }
      } else if (code == LV_EVENT_RELEASED){
        if (!c->adjust){ lv_slider_set_value(sl, c->initial, LV_ANIM_OFF); }
      } else if (code == LV_EVENT_DELETE){
        lv_mem_free(c);
      }
    }, LV_EVENT_ALL, ctx);
  };
  attach_interceptor(lv_slider_m1);
  attach_interceptor(lv_slider_m2);

  // Editable WiFi fields (prefilled later via setSavedWifi)
  lv_obj_t *lblEdSsid = lv_label_create(content); lv_label_set_text(lblEdSsid, "WiFi SSID"); lv_obj_align(lblEdSsid, LV_ALIGN_TOP_LEFT, 0, 148);
  lv_ta_ssid = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_ssid, true); lv_obj_set_width(lv_ta_ssid, lv_pct(100)); lv_obj_align(lv_ta_ssid, LV_ALIGN_TOP_LEFT, 0, 172);
  lv_obj_t *lblEdPass = lv_label_create(content); lv_label_set_text(lblEdPass, "WiFi Password"); lv_obj_align(lblEdPass, LV_ALIGN_TOP_LEFT, 0, 220);
  lv_ta_pass = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_pass, true); lv_textarea_set_password_mode(lv_ta_pass, true); lv_obj_set_width(lv_ta_pass, lv_pct(100)); lv_obj_align(lv_ta_pass, LV_ALIGN_TOP_LEFT, 0, 244);

  // MQTT broker settings
  lv_obj_t *lblHost = lv_label_create(content); lv_label_set_text(lblHost, "MQTT Host"); lv_obj_align(lblHost, LV_ALIGN_TOP_LEFT, 0, 284);
  lv_ta_mqtt_host = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_mqtt_host, true); lv_obj_set_width(lv_ta_mqtt_host, lv_pct(100)); lv_obj_align(lv_ta_mqtt_host, LV_ALIGN_TOP_LEFT, 0, 306);
  lv_obj_t *lblPort = lv_label_create(content); lv_label_set_text(lblPort, "MQTT Port"); lv_obj_align(lblPort, LV_ALIGN_TOP_LEFT, 0, 346);
  lv_ta_mqtt_port = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_mqtt_port, true); lv_obj_set_width(lv_ta_mqtt_port, lv_pct(100)); lv_obj_align(lv_ta_mqtt_port, LV_ALIGN_TOP_LEFT, 0, 368);
  lv_obj_t *lblUser = lv_label_create(content); lv_label_set_text(lblUser, "MQTT User"); lv_obj_align(lblUser, LV_ALIGN_TOP_LEFT, 0, 408);
  lv_ta_mqtt_user = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_mqtt_user, true); lv_obj_set_width(lv_ta_mqtt_user, lv_pct(100)); lv_obj_align(lv_ta_mqtt_user, LV_ALIGN_TOP_LEFT, 0, 430);
  lv_obj_t *lblPw = lv_label_create(content); lv_label_set_text(lblPw, "MQTT Password"); lv_obj_align(lblPw, LV_ALIGN_TOP_LEFT, 0, 470);
  lv_ta_mqtt_pw = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_mqtt_pw, true); lv_textarea_set_password_mode(lv_ta_mqtt_pw, true); lv_obj_set_width(lv_ta_mqtt_pw, lv_pct(100)); lv_obj_align(lv_ta_mqtt_pw, LV_ALIGN_TOP_LEFT, 0, 492);

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

  lv_obj_t *btnWifi = lv_btn_create(footer); lv_obj_set_height(btnWifi, footer_h-16); lv_obj_set_flex_grow(btnWifi, 1); lv_obj_t *lblw = lv_label_create(btnWifi); lv_label_set_text(lblw, "Reset WiFi"); lv_obj_center(lblw);
  lv_obj_add_event_cb(btnWifi, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) { if (handlers.onWifiReset) handlers.onWifiReset(); } }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btnSave = lv_btn_create(footer); lv_obj_set_height(btnSave, footer_h-16); lv_obj_set_flex_grow(btnSave, 1); lv_obj_t *lbls = lv_label_create(btnSave); lv_label_set_text(lbls, "Save"); lv_obj_center(lbls);
  // Save WiFi + MQTT settings via handlers (main will persist)
  lv_obj_add_event_cb(btnSave, [](lv_event_t *e){
    if (lv_event_get_code(e)==LV_EVENT_CLICKED) {
      if (handlers.onWifiSave) {
        const char *s = lv_textarea_get_text(lv_ta_ssid);
        const char *p = lv_textarea_get_text(lv_ta_pass);
        handlers.onWifiSave(s, p);
      }
      if (handlers.onMqttSave) {
        const char *host = lv_textarea_get_text(lv_ta_mqtt_host);
        const char *port = lv_textarea_get_text(lv_ta_mqtt_port);
        const char *user = lv_textarea_get_text(lv_ta_mqtt_user);
        const char *pw   = lv_textarea_get_text(lv_ta_mqtt_pw);
        uint16_t prt = (uint16_t)atoi(port && *port ? port : "1883");
        handlers.onMqttSave(host, prt, user, pw);
      }
    }
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btnBack = lv_btn_create(footer); lv_obj_set_height(btnBack, footer_h-16); lv_obj_set_flex_grow(btnBack, 1); lv_obj_t *lblb = lv_label_create(btnBack); lv_label_set_text(lblb, "Back"); lv_obj_center(lblb);
  lv_obj_add_event_cb(btnBack, [](lv_event_t *e){
    if (lv_event_get_code(e)==LV_EVENT_CLICKED) {
      // Defer navigation to avoid deleting objects during event dispatch
      lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showMain(); }, 0, NULL);
      lv_timer_set_repeat_count(t, 1);
    }
  }, LV_EVENT_CLICKED, NULL);
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

void setSavedMqtt(const char *host, uint16_t port, const char *user, const char *pass){
  if (lv_ta_mqtt_host) lv_textarea_set_text(lv_ta_mqtt_host, (host&&host[0])?host:"");
  if (lv_ta_mqtt_port) { char b[8]; snprintf(b,sizeof(b),"%u", (unsigned)port); lv_textarea_set_text(lv_ta_mqtt_port, b); }
  if (lv_ta_mqtt_user) lv_textarea_set_text(lv_ta_mqtt_user, (user&&user[0])?user:"");
  if (lv_ta_mqtt_pw)   lv_textarea_set_text(lv_ta_mqtt_pw,   (pass&&pass[0])?pass:"");
}

void configureHandlers(const Handlers &h){ handlers = h; }
void setInitialSpeeds(uint8_t m1, uint8_t m2){ initial_m1=m1; initial_m2=m2; }
void setInitialMode(bool zigbee){ initial_mode_zigbee = zigbee; }

// -------- Extracted helpers --------
void showRangeEditor(bool isPh){
  if (lv_modal_active) { lv_obj_del(lv_modal_active); lv_modal_active=nullptr; }
  lv_obj_t *modal = lv_obj_create(lv_layer_top()); lv_modal_active = modal;
  lv_obj_set_size(modal, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_style_bg_opa(modal, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
  lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(modal, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(modal, LV_OPA_TRANSP, LV_PART_SCROLLBAR);

  lv_obj_t *dlg = lv_obj_create(modal); lv_obj_set_size(dlg, lv_disp_get_hor_res(NULL)-40, lv_disp_get_ver_res(NULL)-40); lv_obj_center(dlg); lv_obj_set_style_radius(dlg, 10, 0); lv_obj_set_style_pad_all(dlg, 12, 0);
  lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);

  const char *title = isPh ? "pH range" : "ORP range";
  lv_obj_t *titleLbl = lv_label_create(dlg); lv_label_set_text(titleLbl, title); lv_obj_align(titleLbl, LV_ALIGN_TOP_MID, 0, 0);

  // Build two rows for Min/Max: [label][slider grows][value]
  lv_coord_t dlg_w = lv_obj_get_width(dlg);
  auto make_row = [&](const char *leftText, bool ph)->std::tuple<lv_obj_t*, lv_obj_t*, lv_obj_t*>{
    lv_obj_t *row = lv_obj_create(dlg);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(row, 6, 0);
    lv_obj_set_style_pad_column(row, 10, 0);

    lv_obj_t *lbl = lv_label_create(row); lv_label_set_text(lbl, leftText);
    lv_obj_t *sl  = lv_slider_create(row);
    lv_obj_set_flex_grow(sl, 1);
    lv_obj_set_height(sl, 18);
    lv_obj_set_style_min_width(sl, 120, 0);
    lv_obj_set_style_height(sl, 18, LV_PART_MAIN);
    lv_obj_set_style_radius(sl, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, lv_color_make(70,70,70), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_width(sl, 20, LV_PART_KNOB);
    lv_obj_set_style_height(sl, 20, LV_PART_KNOB);
    lv_obj_t *val = lv_label_create(row);
    if (ph) lv_slider_set_range(sl, 0, 1400); else lv_slider_set_range(sl, 0, 3000);
    return {sl, val, row};
  };
  auto [slMin, valMin, row1] = make_row(isPh?"Min":"Min (mV):", isPh);
  auto [slMax, valMax, row2] = make_row(isPh?"Max":"Max (mV):", isPh);
  // Place rows vertically at fixed y positions to avoid overlap
  lv_obj_align(row1, LV_ALIGN_TOP_MID, 0, 36);
  lv_obj_align(row2, LV_ALIGN_TOP_MID, 0, 36 + 48);
  // Ensure rows layout so flex can size children
  lv_obj_update_layout(dlg);
  // Remove any accidental minimum widths; rely on flex_grow + row width
  lv_obj_set_width(slMin, LV_SIZE_CONTENT);
  lv_obj_set_width(slMax, LV_SIZE_CONTENT);
  // Strengthen contrast to ensure track/indicator are visible on dark bg
  lv_obj_set_style_bg_color(slMin, lv_color_make(60,60,60), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slMax, lv_color_make(60,60,60), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slMin, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slMax, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
  // Set defaults
  if (isPh){ lv_slider_set_value(slMin, 680, LV_ANIM_OFF); lv_slider_set_value(slMax, 760, LV_ANIM_OFF);} else { lv_slider_set_value(slMin, 250, LV_ANIM_OFF); lv_slider_set_value(slMax, 850, LV_ANIM_OFF);}  
  auto set_val_text = [&](lv_obj_t *lbl, bool ph, int v){ char b[16]; if (ph){ int vi=v/100, vf=v%100; snprintf(b,sizeof(b),"%d.%02d",vi,vf);} else { snprintf(b,sizeof(b),"%d",v);} lv_label_set_text(lbl,b); };
  set_val_text(valMin, isPh, (int)lv_slider_get_value(slMin));
  set_val_text(valMax, isPh, (int)lv_slider_get_value(slMax));

  lv_obj_t *btnCancel = lv_btn_create(dlg); lv_obj_set_size(btnCancel, 120, 44); lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_LEFT, 0, 0); { lv_obj_t *t=lv_label_create(btnCancel); lv_label_set_text(t, "Cancel"); lv_obj_center(t);} 
  lv_obj_t *btnSave   = lv_btn_create(dlg); lv_obj_set_size(btnSave, 120, 44); lv_obj_align(btnSave, LV_ALIGN_BOTTOM_RIGHT, 0, 0); { lv_obj_t *t=lv_label_create(btnSave); lv_label_set_text(t, "Save"); lv_obj_center(t);} 

  struct Loc { bool isPh; lv_obj_t *slMin; lv_obj_t *slMax; lv_obj_t *modal; };
  Loc *ctx = (Loc*)lv_mem_alloc(sizeof(Loc)); ctx->isPh=isPh; ctx->slMin=slMin; ctx->slMax=slMax; ctx->modal=modal;

  // Update live value labels on slider change
  struct VCtx { bool isPh; lv_obj_t *lbl; };
  VCtx *v1=(VCtx*)lv_mem_alloc(sizeof(VCtx)); v1->isPh=isPh; v1->lbl=valMin;
  VCtx *v2=(VCtx*)lv_mem_alloc(sizeof(VCtx)); v2->isPh=isPh; v2->lbl=valMax;
  lv_obj_add_event_cb(slMin, [](lv_event_t *e){ VCtx *c=(VCtx*)lv_event_get_user_data(e); if (lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)); char b[16]; if(c->isPh){ snprintf(b,sizeof(b),"%d.%02d", v/100, v%100);} else { snprintf(b,sizeof(b),"%d", v);} lv_label_set_text(c->lbl,b);} }, LV_EVENT_ALL, v1);
  lv_obj_add_event_cb(slMax, [](lv_event_t *e){ VCtx *c=(VCtx*)lv_event_get_user_data(e); if (lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)); char b[16]; if(c->isPh){ snprintf(b,sizeof(b),"%d.%02d", v/100, v%100);} else { snprintf(b,sizeof(b),"%d", v);} lv_label_set_text(c->lbl,b);} }, LV_EVENT_ALL, v2);

  lv_obj_add_event_cb(btnCancel, [](lv_event_t *e){
    if (lv_event_get_code(e)==LV_EVENT_CLICKED) {
      Loc *c=(Loc*)lv_event_get_user_data(e);
      // defer delete to avoid destroying hierarchy during event handler
      lv_obj_t *to_del = c->modal;
      lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ lv_obj_t *obj=(lv_obj_t*)tm->user_data; if (obj) lv_obj_del(obj); }, 0, to_del);
      lv_timer_set_repeat_count(t, 1);
      lv_mem_free(c); lv_modal_active=nullptr;
    }
  }, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(btnSave, [](lv_event_t *e){
    if (lv_event_get_code(e)==LV_EVENT_CLICKED) {
      Loc *c=(Loc*)lv_event_get_user_data(e);
      lv_obj_t *to_del = c->modal;
      lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ lv_obj_t *obj=(lv_obj_t*)tm->user_data; if (obj) lv_obj_del(obj); }, 0, to_del);
      lv_timer_set_repeat_count(t, 1);
      lv_mem_free(c); lv_modal_active=nullptr;
    }
  }, LV_EVENT_CLICKED, ctx);
}

void showCommissioning(uint32_t seconds){
  if (lv_modal_active) { lv_obj_del(lv_modal_active); lv_modal_active=nullptr; }
  lv_obj_t *modal = lv_obj_create(lv_layer_top()); lv_modal_active = modal;
  lv_obj_set_size(modal, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_style_bg_opa(modal, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
  lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(modal, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *dlg = lv_obj_create(modal); lv_obj_set_size(dlg, lv_disp_get_hor_res(NULL)-40, lv_disp_get_ver_res(NULL)-40); lv_obj_center(dlg);
  lv_obj_set_style_radius(dlg, 10, 0); lv_obj_set_style_pad_all(dlg, 12, 0);
  lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *title = lv_label_create(dlg); lv_label_set_text(title, "Zigbee commissioning"); lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *spinner = lv_spinner_create(dlg, 1000, 60); lv_obj_set_size(spinner, 28, 28); lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -6);
  lv_obj_t *msg = lv_label_create(dlg); char b[48]; snprintf(b, sizeof(b), "Pairing... %us", (unsigned)seconds); lv_label_set_text(msg, b); lv_obj_align(msg, LV_ALIGN_CENTER, 0, 22);
}

void showHoldToPair(){
  if (lv_modal_active) { lv_obj_del(lv_modal_active); lv_modal_active=nullptr; }
  lv_obj_t *modal = lv_obj_create(lv_layer_top()); lv_modal_active=modal;
  lv_obj_set_size(modal, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_style_bg_opa(modal, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
  lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(modal, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *dlg = lv_obj_create(modal); lv_obj_set_size(dlg, lv_disp_get_hor_res(NULL)-40, lv_disp_get_ver_res(NULL)-40); lv_obj_center(dlg);
  lv_obj_set_style_radius(dlg, 10, 0); lv_obj_set_style_pad_all(dlg, 12, 0);
  lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *title = lv_label_create(dlg); lv_label_set_text(title, "Hold BOOT 3s to start Zigbee pairing"); lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
}

} // namespace ui


