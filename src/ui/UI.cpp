#include "UI.h"
#include <Arduino.h>
#include <esp_log.h>
#include <WiFi.h>
#include <math.h>
#include "domain/Metrics.h"
#include "core/Storage.h"
#if USE_ANALOG_SENSORS
#include "io/AnalogPhOrpSensor.h"
#endif

extern ::core::Storage g_storage;

namespace ui {

static lv_obj_t *lv_tile_main = nullptr;
static lv_obj_t *lv_tile_settings = nullptr;
static lv_obj_t *lv_lbl_ph = nullptr;
static lv_obj_t *lv_lbl_orp = nullptr;
static lv_obj_t *lv_lbl_orp_unit = nullptr;
static lv_obj_t *lv_lbl_temp = nullptr;
static lv_obj_t *lv_card_temp = nullptr;
static lv_obj_t *lv_lbl_ip = nullptr;
static lv_obj_t *lv_lbl_mqtt = nullptr;
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
static float g_phMin = 6.80f, g_phMax = 7.60f; static int g_orpMin = 250, g_orpMax = 850;
static bool g_pumpPh = false, g_pumpOrp = false;
static float g_phSession = 0, g_phFlow = 0, g_orpSession = 0, g_orpFlow = 0;
static lv_obj_t *lv_pump_ph = nullptr, *lv_pump_orp = nullptr;
static lv_obj_t *lv_pump_ph_stats = nullptr, *lv_pump_orp_stats = nullptr;
// Debounce for on-screen keyboard (avoid double insert)
static uint32_t g_kb_last_ms = 0; static int16_t g_kb_last_id = -1;
static lv_obj_t *lv_ta_ssid = nullptr;
static lv_obj_t *lv_ta_pass = nullptr;
static lv_obj_t *lv_lbl_ssid = nullptr;
static bool onSettings = false;
static uint8_t initial_m1=60, initial_m2=60;
static Handlers handlers;
static bool initial_mode_zigbee = true;
static lv_obj_t *lv_modal_active = nullptr; // generic modal holder
// Pump calibration state
static lv_timer_t *g_cal_timer = nullptr;
static int g_cal_motor = 0;  // 0=none, 1=M1, 2=M2
static int g_cal_countdown = 0;
static lv_obj_t *lv_lbl_cal_m1_status = nullptr;
static lv_obj_t *lv_lbl_cal_m2_status = nullptr;

void init(lv_disp_t* disp){ (void)disp; }
static lv_timer_t *g_update_timer = nullptr;

// Pump calibration timer callback (60-second countdown)
static void pump_cal_timer_cb(lv_timer_t *timer) {
  (void)timer;
  g_cal_countdown--;
  
  // Update status label
  char buf[64];
  if (g_cal_motor == 1 && lv_lbl_cal_m1_status) {
    if (g_cal_countdown > 0) {
      snprintf(buf, sizeof(buf), "Running... %ds", g_cal_countdown);
      lv_label_set_text(lv_lbl_cal_m1_status, buf);
    } else {
      lv_label_set_text(lv_lbl_cal_m1_status, "Completed! Measure volume.");
    }
  } else if (g_cal_motor == 2 && lv_lbl_cal_m2_status) {
    if (g_cal_countdown > 0) {
      snprintf(buf, sizeof(buf), "Running... %ds", g_cal_countdown);
      lv_label_set_text(lv_lbl_cal_m2_status, buf);
    } else {
      lv_label_set_text(lv_lbl_cal_m2_status, "Completed! Measure volume.");
    }
  }
  
  // Stop when countdown reaches 0
  if (g_cal_countdown <= 0) {
    if (handlers.onPumpCalStop) handlers.onPumpCalStop(g_cal_motor);
    g_cal_motor = 0;
    lv_timer_del(g_cal_timer);
    g_cal_timer = nullptr;
  }
}

// --- Speed dial (floating action) ---
static void anim_set_pos_y(void *obj, int32_t v){ lv_obj_set_y((lv_obj_t*)obj, v); }
static void anim_set_pos_x(void *obj, int32_t v){ lv_obj_set_x((lv_obj_t*)obj, v); }

static void ui_create_speed_dial(lv_obj_t *parent){
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, 160, 160);
  lv_obj_align(box, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

  // Main FAB
  lv_obj_t *fab = lv_btn_create(box);
  lv_obj_set_size(fab, 60, 60);
  lv_obj_set_style_radius(fab, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(fab, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_bg_opa(fab, LV_OPA_COVER, 0);
  lv_obj_align(fab, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_t *lblFab = lv_label_create(fab);
  lv_label_set_text(lblFab, LV_SYMBOL_PLUS);
  lv_obj_set_style_text_color(lblFab, lv_color_white(), 0);
  lv_obj_set_style_text_font(lblFab, &lv_font_montserrat_28, 0);
  lv_obj_center(lblFab);

  auto make_option_sym = [&](const char *sym, const char *text, const lv_font_t *font)->lv_obj_t*{
    lv_obj_t *btn = lv_btn_create(box);
    lv_obj_set_size(btn, 52, 52);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 3), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_white(), 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_40, 0);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, 4, 4);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *li = lv_label_create(btn); lv_label_set_text(li, sym);
    if (font) lv_obj_set_style_text_font(li, font, 0);
    lv_obj_set_style_text_color(li, lv_color_white(), 0);
    lv_obj_center(li);
    lv_obj_t *cap = lv_label_create(box); lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0); lv_obj_set_style_text_color(cap, lv_palette_lighten(LV_PALETTE_GREY, 2), 0); lv_label_set_text(cap, text);
    lv_obj_align_to(cap, btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
    lv_obj_add_flag(cap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_user_data(btn, cap);
    return btn;
  };

  auto make_option_img = [&](const lv_img_dsc_t *img, const char *text)->lv_obj_t*{
    lv_obj_t *btn = lv_btn_create(box);
    lv_obj_set_size(btn, 52, 52);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 3), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_white(), 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_40, 0);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, 4, 4);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *im = lv_img_create(btn); lv_img_set_src(im, img);
    lv_obj_set_style_img_recolor_opa(im, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(im, lv_color_white(), 0);
    lv_obj_center(im);
    lv_obj_t *cap = lv_label_create(box); lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0); lv_obj_set_style_text_color(cap, lv_palette_lighten(LV_PALETTE_GREY, 2), 0); lv_label_set_text(cap, text);
    lv_obj_align_to(cap, btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
    lv_obj_add_flag(cap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_user_data(btn, cap);
    return btn;
  };

  lv_obj_t *btnSettings = make_option_sym(LV_SYMBOL_SETTINGS, "Settings", &lv_font_montserrat_28);
  lv_obj_t *btnPhCal   = make_option_img(&water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40, "pH Cal");
  lv_obj_t *btnOrpCal  = make_option_img(&water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40, "ORP Cal");

  struct DialCtx { lv_obj_t *b1; lv_obj_t *b2; lv_obj_t *b3; };
  DialCtx *ctx = (DialCtx*)lv_mem_alloc(sizeof(DialCtx));
  ctx->b1 = btnSettings; ctx->b2 = btnPhCal; ctx->b3 = btnOrpCal;

  // Wire actions
  lv_obj_add_event_cb(btnSettings, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED){ lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; if (handlers.onSettings) handlers.onSettings(); }, 0, NULL); lv_timer_set_repeat_count(t, 1); } }, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(btnPhCal, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED){ lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showPhCalibration(); }, 0, NULL); lv_timer_set_repeat_count(t, 1); } }, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(btnOrpCal, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED){ lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ (void)tm; ui::showOrpCalibration(); }, 0, NULL); lv_timer_set_repeat_count(t, 1); } }, LV_EVENT_ALL, NULL);

  // Toggle expansion
  lv_obj_add_event_cb(fab, [](lv_event_t *e){
    static bool expanded = false;
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    expanded = !expanded;
    DialCtx *d = (DialCtx*)lv_event_get_user_data(e);
    if (!d) return;
    auto set_btn = [&](lv_obj_t *btn, int x_off, int y_off){
      lv_obj_t *cap = (lv_obj_t*)lv_obj_get_user_data(btn);
      if (expanded) {
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);
        if (cap) { lv_obj_clear_flag(cap, LV_OBJ_FLAG_HIDDEN); lv_obj_align_to(cap, btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 4); }
        // position relative to bottom-right
        lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, x_off, y_off);
      } else {
        // collapse back onto FAB and hide
        lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, 4, 4);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
        if (cap) lv_obj_add_flag(cap, LV_OBJ_FLAG_HIDDEN);
      }
    };
    // quarter circle with spacing tuned for 52px buttons
    set_btn(d->b1,   0, -74);
    set_btn(d->b2, -54, -54);
    set_btn(d->b3, -74,   0);
  }, LV_EVENT_ALL, ctx);
}

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

  auto make_card = [&](lv_obj_t *parent, lv_color_t c1, lv_coord_t cw)->lv_obj_t*{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_style_bg_color(card, c1, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_set_size(card, cw, 195);  // Increased from 160 to 195 for pump stats and spacing
    return card;
  };

  bool small_layout = (scr_w <= 320) || (scr_h <= 180);
  lv_obj_t *card_ph = nullptr, *card_orp = nullptr, *card_tmp = nullptr;
  lv_coord_t inner_w = (scr_w - (pad*2)) - (pad*2);
  if (small_layout) {
    // 2-tile layout for C6
    lv_coord_t cw = (inner_w - col_gap) / 2;
    card_ph  = make_card(row, lv_palette_darken(LV_PALETTE_BLUE, 3), cw);
    card_orp = make_card(row, lv_palette_darken(LV_PALETTE_TEAL, 3), cw);
  } else {
    // 3-card layout for S3
    lv_coord_t cw = (inner_w - (col_gap * 2)) / 3;
    card_ph  = make_card(row, lv_palette_darken(LV_PALETTE_BLUE, 3), cw);
    card_orp = make_card(row, lv_palette_darken(LV_PALETTE_TEAL, 3), cw);
    card_tmp = make_card(row, lv_palette_darken(LV_PALETTE_AMBER, 4), cw);
  }

  // Icons + labels
  lv_obj_t *icon_ph = lv_img_create(card_ph); lv_img_set_src(icon_ph, &water_ph_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40); lv_obj_align(icon_ph, LV_ALIGN_TOP_LEFT, 0, 0); lv_obj_set_style_img_recolor_opa(icon_ph, LV_OPA_COVER, 0); lv_obj_set_style_img_recolor(icon_ph, lv_color_white(), 0);
  lv_lbl_ph  = lv_label_create(card_ph);  lv_obj_set_style_text_color(lv_lbl_ph, lv_color_white(), 0);  lv_label_set_text(lv_lbl_ph, "--.--");  lv_obj_set_style_text_font(lv_lbl_ph, &lv_font_montserrat_28, 0); lv_obj_align(lv_lbl_ph, LV_ALIGN_TOP_LEFT, 0, 44);
  lv_obj_t *icon_orp = lv_img_create(card_orp); lv_img_set_src(icon_orp, &water_orp_32dp_E3E3E3_FILL0_wght400_GRAD0_opsz40); lv_obj_align(icon_orp, LV_ALIGN_TOP_LEFT, 0, 0); lv_obj_set_style_img_recolor_opa(icon_orp, LV_OPA_COVER, 0); lv_obj_set_style_img_recolor(icon_orp, lv_color_white(), 0);
  lv_lbl_orp = lv_label_create(card_orp); lv_obj_set_style_text_color(lv_lbl_orp, lv_color_white(), 0); lv_label_set_text(lv_lbl_orp, "----");  lv_obj_set_style_text_font(lv_lbl_orp, &lv_font_montserrat_28, 0); lv_obj_align(lv_lbl_orp, LV_ALIGN_TOP_LEFT, 0, 44);
  lv_lbl_orp_unit = lv_label_create(card_orp); lv_obj_set_style_text_color(lv_lbl_orp_unit, lv_color_white(), 0); lv_label_set_text(lv_lbl_orp_unit, " mV"); lv_obj_align_to(lv_lbl_orp_unit, lv_lbl_orp, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
  // Pump stats labels (above pump icon, with descriptive text)
  lv_pump_ph_stats = lv_label_create(card_ph); lv_obj_set_style_text_color(lv_pump_ph_stats, lv_palette_lighten(LV_PALETTE_GREY,2), 0); lv_label_set_text(lv_pump_ph_stats, ""); lv_obj_set_style_text_font(lv_pump_ph_stats, &lv_font_montserrat_12, 0); lv_obj_align(lv_pump_ph_stats, LV_ALIGN_TOP_LEFT, 0, 125); lv_obj_add_flag(lv_pump_ph_stats, LV_OBJ_FLAG_HIDDEN);
  lv_pump_orp_stats = lv_label_create(card_orp); lv_obj_set_style_text_color(lv_pump_orp_stats, lv_palette_lighten(LV_PALETTE_GREY,2), 0); lv_label_set_text(lv_pump_orp_stats, ""); lv_obj_set_style_text_font(lv_pump_orp_stats, &lv_font_montserrat_12, 0); lv_obj_align(lv_pump_orp_stats, LV_ALIGN_TOP_LEFT, 0, 125); lv_obj_add_flag(lv_pump_orp_stats, LV_OBJ_FLAG_HIDDEN);
  // Pump icons (module UI) — placed below stats labels with more spacing from bottom
  lv_pump_ph = lv_img_create(card_ph); lv_img_set_src(lv_pump_ph, &water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24); lv_obj_set_style_img_recolor_opa(lv_pump_ph, LV_OPA_COVER, 0); lv_obj_set_style_img_recolor(lv_pump_ph, lv_color_white(), 0); lv_obj_align(lv_pump_ph, LV_ALIGN_TOP_LEFT, 0, 155); lv_obj_add_flag(lv_pump_ph, LV_OBJ_FLAG_HIDDEN);
  lv_pump_orp = lv_img_create(card_orp); lv_img_set_src(lv_pump_orp, &water_pump_24dp_E3E3E3_FILL0_wght400_GRAD0_opsz24); lv_obj_set_style_img_recolor_opa(lv_pump_orp, LV_OPA_COVER, 0); lv_obj_set_style_img_recolor(lv_pump_orp, lv_color_white(), 0); lv_obj_align(lv_pump_orp, LV_ALIGN_TOP_LEFT, 0, 155); lv_obj_add_flag(lv_pump_orp, LV_OBJ_FLAG_HIDDEN);

  // Temp label: only in 3-card layout; in small layout we skip
  if (!small_layout && card_tmp) {
    lv_obj_t *icon_tmp = lv_img_create(card_tmp); lv_img_set_src(icon_tmp, &device_thermostat_32dp_999999_FILL0_wght400_GRAD0_opsz40); lv_obj_align(icon_tmp, LV_ALIGN_TOP_LEFT, 0, 0); lv_obj_set_style_img_recolor_opa(icon_tmp, LV_OPA_COVER, 0); lv_obj_set_style_img_recolor(icon_tmp, lv_color_white(), 0);
    lv_lbl_temp = lv_label_create(card_tmp); lv_obj_set_style_text_color(lv_lbl_temp, lv_color_white(), 0); lv_label_set_text(lv_lbl_temp, "--.- C"); lv_obj_set_style_text_font(lv_lbl_temp, &lv_font_montserrat_28, 0); lv_obj_align(lv_lbl_temp, LV_ALIGN_TOP_LEFT, 0, 44);
    // Store temp card for dynamic color updates
    lv_card_temp = card_tmp;
  }

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

  // Replace Settings button with floating speed-dial
  ui_create_speed_dial(root);
  // Adjust IP label anchor: align to root bottom area instead of removed button
  if (lv_lbl_ip) {
    lv_obj_align(lv_lbl_ip, LV_ALIGN_BOTTOM_LEFT, 0, -24);
  }

  // IP label (content width), placed near bottom-left above the speed dial with more spacing from cards
  lv_lbl_ip = lv_label_create(root); lv_obj_set_style_text_color(lv_lbl_ip, lv_palette_lighten(LV_PALETTE_GREY, 3), 0); lv_obj_set_style_text_font(lv_lbl_ip, &lv_font_montserrat_14, 0); lv_label_set_long_mode(lv_lbl_ip, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lv_lbl_ip, LV_SIZE_CONTENT); lv_obj_set_style_text_align(lv_lbl_ip, LV_TEXT_ALIGN_LEFT, 0); lv_obj_align(lv_lbl_ip, LV_ALIGN_BOTTOM_LEFT, 0, -24); lv_label_set_text(lv_lbl_ip, "IP: --");

  // MQTT host label onder IP
  lv_lbl_mqtt = lv_label_create(root);
  lv_obj_set_style_text_color(lv_lbl_mqtt, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
  lv_obj_set_style_text_font(lv_lbl_mqtt, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(lv_lbl_mqtt, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lv_lbl_mqtt, LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(lv_lbl_mqtt, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align_to(lv_lbl_mqtt, lv_lbl_ip, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
  lv_label_set_text(lv_lbl_mqtt, "MQTT: --");

  // SSID label above IP (stacked), left aligned with proper spacing
  lv_lbl_ssid = lv_label_create(root);
  lv_obj_set_style_text_color(lv_lbl_ssid, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
  lv_obj_set_style_text_font(lv_lbl_ssid, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(lv_lbl_ssid, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lv_lbl_ssid, LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(lv_lbl_ssid, LV_TEXT_ALIGN_LEFT, 0);
  // place directly above the IP label on the left with more spacing
  lv_obj_align_to(lv_lbl_ssid, lv_lbl_ip, LV_ALIGN_OUT_TOP_LEFT, 0, -8);
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
  // pH with 2 decimals - thread-safe with static buffer
  static char buf_ph[16] = "--.--";
  if (lv_lbl_ph) {
    if (M.havePh) {
      int ph100 = (int)((M.phVal * 100.0f) + (M.phVal >= 0 ? 0.5f : -0.5f));
      int ph_i = ph100 / 100; int ph_f = abs(ph100 % 100);
      snprintf(buf_ph, sizeof(buf_ph), "%d.%02d", ph_i, ph_f);
      lv_label_set_text_static(lv_lbl_ph, buf_ph);
      // Colorize by thresholds
      lv_color_t c = lv_color_white();
      bool below = M.phVal < g_phMin; bool above = M.phVal > g_phMax;
      bool warn = (!below && !above) && (M.phVal <= g_phMin + 0.05f || M.phVal >= g_phMax - 0.05f);
      if (below || above) c = lv_palette_main(LV_PALETTE_RED); else if (warn) c = lv_palette_main(LV_PALETTE_ORANGE);
      lv_obj_set_style_text_color(lv_lbl_ph, c, 0);
    } else {
      strncpy(buf_ph, "--.--", sizeof(buf_ph) - 1);
      lv_label_set_text_static(lv_lbl_ph, buf_ph);
    }
  }
  // ORP as integer - thread-safe with static buffer
  static char buf_orp[16] = "----";
  if (lv_lbl_orp) {
    if (M.haveOrp) {
      snprintf(buf_orp, sizeof(buf_orp), "%d", (int)(M.orpMv >= 0 ? (M.orpMv + 0.5f) : (M.orpMv - 0.5f)));
      lv_label_set_text_static(lv_lbl_orp, buf_orp);
      if (lv_lbl_orp_unit) lv_obj_clear_flag(lv_lbl_orp_unit, LV_OBJ_FLAG_HIDDEN);
      // Colorize by thresholds
      int v = (int)(M.orpMv >= 0 ? (M.orpMv + 0.5f) : (M.orpMv - 0.5f));
      lv_color_t c = lv_color_white();
      bool low = v < g_orpMin; bool high = v > g_orpMax;
      bool warn = (!low && !high) && (v <= g_orpMin + 20 || v >= g_orpMax - 20);
      if (low || high) c = lv_palette_main(LV_PALETTE_RED); else if (warn) c = lv_palette_main(LV_PALETTE_ORANGE);
      lv_obj_set_style_text_color(lv_lbl_orp, c, 0);
    } else {
      strncpy(buf_orp, "----", sizeof(buf_orp) - 1);
      lv_label_set_text_static(lv_lbl_orp, buf_orp);
    }
  }
  // Temp with 1 decimal and unit - thread-safe with static buffer
  static char buf_temp[20] = "--.- C";
  if (lv_lbl_temp) {
    if (M.haveTemp) {
      int t10 = (int)((M.tempC * 10.0f) + (M.tempC >= 0 ? 0.5f : -0.5f));
      int t_i = t10 / 10; int t_f = abs(t10 % 10);
      snprintf(buf_temp, sizeof(buf_temp), "%d.%d C", t_i, t_f);
      lv_label_set_text_static(lv_lbl_temp, buf_temp);
      if (lv_card_temp) {
        // Color thresholds: <15C blue, 15..25C amber, >25C red
        lv_color_t c = lv_palette_darken(LV_PALETTE_AMBER, 4);
        if (M.tempC < 15.0f) c = lv_palette_darken(LV_PALETTE_BLUE, 3);
        else if (M.tempC > 25.0f) c = lv_palette_darken(LV_PALETTE_RED, 1);
        lv_obj_set_style_bg_color(lv_card_temp, c, 0);
      }
    } else {
      strncpy(buf_temp, "--.- C", sizeof(buf_temp) - 1);
      lv_label_set_text_static(lv_lbl_temp, buf_temp);
    }
  }
  // Update pump icons and stats (runs in LVGL thread, no async needed!)
  static bool last_pump_ph = false, last_pump_orp = false;
  static char last_ph_text[32] = {0}, last_orp_text[32] = {0};
  static char ph_buf[32] = {0}, orp_buf[32] = {0};
  
  if (lv_pump_ph && lv_pump_orp && lv_pump_ph_stats && lv_pump_orp_stats) {
    // Check if pH pump state or stats changed
    snprintf(ph_buf, sizeof(ph_buf), "%.0fml\n@%.0fml/min", g_phSession, g_phFlow);
    if (g_pumpPh != last_pump_ph || strcmp(ph_buf, last_ph_text) != 0) {
      last_pump_ph = g_pumpPh;
      strncpy(last_ph_text, ph_buf, sizeof(last_ph_text) - 1);
      if (g_pumpPh) {
        lv_obj_clear_flag(lv_pump_ph, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_static(lv_pump_ph_stats, ph_buf);
        lv_obj_clear_flag(lv_pump_ph_stats, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(lv_pump_ph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lv_pump_ph_stats, LV_OBJ_FLAG_HIDDEN);
      }
    }
    // Check if ORP pump state or stats changed
    snprintf(orp_buf, sizeof(orp_buf), "%.0fml\n@%.0fml/min", g_orpSession, g_orpFlow);
    if (g_pumpOrp != last_pump_orp || strcmp(orp_buf, last_orp_text) != 0) {
      last_pump_orp = g_pumpOrp;
      strncpy(last_orp_text, orp_buf, sizeof(last_orp_text) - 1);
      if (g_pumpOrp) {
        lv_obj_clear_flag(lv_pump_orp, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_static(lv_pump_orp_stats, orp_buf);
        lv_obj_clear_flag(lv_pump_orp_stats, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(lv_pump_orp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lv_pump_orp_stats, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
  
  // Refresh IP/SSID on each tick - thread-safe with static buffers
  static char buf_ip[64] = "IP: --";
  static char buf_mqtt[96] = "MQTT: --";
  static char buf_ssid[96] = "SSID: --";
  static char temp_str[80] = {0};
  
  if (lv_lbl_ip) {
    if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip = WiFi.localIP();
      snprintf(temp_str, sizeof(temp_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      snprintf(buf_ip, sizeof(buf_ip), "IP: %s", temp_str);
    } else {
      strncpy(buf_ip, "IP: --", sizeof(buf_ip) - 1);
    }
    lv_label_set_text_static(lv_lbl_ip, buf_ip);
  }
  if (lv_lbl_mqtt) {
    String host = ::g_storage.getMqttHost("");
    if (host.length() > 0) {
      strncpy(temp_str, host.c_str(), sizeof(temp_str) - 1);
      temp_str[sizeof(temp_str) - 1] = '\0';
      snprintf(buf_mqtt, sizeof(buf_mqtt), "MQTT: %s", temp_str);
    } else {
      strncpy(buf_mqtt, "MQTT: --", sizeof(buf_mqtt) - 1);
    }
    lv_label_set_text_static(lv_lbl_mqtt, buf_mqtt);
  }
  if (lv_lbl_ssid) {
    if (WiFi.status() == WL_CONNECTED) {
      strncpy(temp_str, WiFi.SSID().c_str(), sizeof(temp_str) - 1);
      temp_str[sizeof(temp_str) - 1] = '\0';
      snprintf(buf_ssid, sizeof(buf_ssid), "SSID: %s", temp_str);
    } else {
      strncpy(buf_ssid, "SSID: --", sizeof(buf_ssid) - 1);
    }
    lv_label_set_text_static(lv_lbl_ssid, buf_ssid);
  }
}

void setThresholds(float phMin, float phMax, int orpMin, int orpMax){
  g_phMin = phMin; g_phMax = phMax; g_orpMin = orpMin; g_orpMax = orpMax;
}

void setPumpActive(bool phActive, bool orpActive){
  // Just store values - they'll be applied by updateValues() which runs in LVGL thread
  if (onSettings) return;
  g_pumpPh = phActive; 
  g_pumpOrp = orpActive;
}

void setPumpStats(bool phActive, float phSession, float phFlow, bool orpActive, float orpSession, float orpFlow){
  if (onSettings) return;
  
  // Just store values - they'll be applied by updateValues() which runs in LVGL thread
  // This is MUCH safer than async calls which can cause memory corruption
  g_pumpPh = phActive;
  g_pumpOrp = orpActive;
  g_phSession = phSession;
  g_phFlow = phFlow;
  g_orpSession = orpSession;
  g_orpFlow = orpFlow;
}

void setIp(const char *ipText){
  static char s_ip[64] = {0};
  if (!ipText) ipText = "--";
  snprintf(s_ip, sizeof(s_ip), "IP: %s", ipText);
  // Defer to LVGL task to avoid cross-thread writes
  lv_async_call([](void *p){ (void)p; if (lv_lbl_ip) lv_label_set_text(lv_lbl_ip, s_ip); }, NULL);
}

void setSsid(const char *ssid){
  static char s_ssid[96] = {0};
  if (!ssid || !ssid[0]) ssid = "--";
  snprintf(s_ssid, sizeof(s_ssid), "SSID: %s", ssid);
  lv_async_call([](void *p){ (void)p; if (lv_lbl_ssid) lv_label_set_text(lv_lbl_ssid, s_ssid); }, NULL);
}

void showSettings(){
  onSettings = true;
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
  lv_obj_add_flag(lv_slider_m1, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_flag(lv_slider_m1, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_slider_set_range(lv_slider_m1, 0, 100);
  lv_slider_set_value(lv_slider_m1, initial_m1, LV_ANIM_OFF);

  // ORP slider
  lv_obj_t *lblm2 = lv_label_create(content); lv_label_set_text(lblm2, "ORP Motor Speed"); lv_obj_align(lblm2, LV_ALIGN_TOP_LEFT, 0, 88);
  lv_lbl_val_m2 = lv_label_create(content); { char b[8]; snprintf(b, sizeof(b), "%u%%", (unsigned)initial_m2); lv_label_set_text(lv_lbl_val_m2, b);} lv_obj_align(lv_lbl_val_m2, LV_ALIGN_TOP_RIGHT, 0, 88);
  lv_slider_m2 = lv_slider_create(content); lv_obj_set_width(lv_slider_m2, lv_pct(100)); lv_obj_align(lv_slider_m2, LV_ALIGN_TOP_LEFT, 0, 108);
  lv_obj_add_flag(lv_slider_m2, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_flag(lv_slider_m2, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_slider_set_range(lv_slider_m2, 0, 100);
  lv_slider_set_value(lv_slider_m2, initial_m2, LV_ANIM_OFF);

  lv_obj_add_event_cb(lv_slider_m1, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value(lv_event_get_target(e)); if (lv_lbl_val_m1){ char b[8]; snprintf(b,sizeof(b),"%d%%",v); lv_label_set_text(lv_lbl_val_m1,b);} if (handlers.onSpeedChange) handlers.onSpeedChange(1,v);} }, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(lv_slider_m2, [](lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED){ int v=(int)lv_slider_get_value(lv_event_get_target(e)); if (lv_lbl_val_m2){ char b[8]; snprintf(b,sizeof(b),"%d%%",v); lv_label_set_text(lv_lbl_val_m2,b);} if (handlers.onSpeedChange) handlers.onSpeedChange(2,v);} }, LV_EVENT_VALUE_CHANGED, NULL);

  // Intercept vertical swipe on sliders to scroll the content instead of adjusting the slider
  struct ScrollInterceptCtx { lv_obj_t *scroll; int initial; int ax; int ay; bool adjust; };
  auto attach_interceptor = [&](lv_obj_t *slider){
    ScrollInterceptCtx *ctx = (ScrollInterceptCtx*)lv_mem_alloc(sizeof(ScrollInterceptCtx));
    ctx->scroll = content; ctx->initial = (int)lv_slider_get_value(slider); ctx->ax = 0; ctx->ay = 0; ctx->adjust = false;
    lv_obj_add_event_cb(slider, [](lv_event_t *e){
      auto code = lv_event_get_code(e);
      ScrollInterceptCtx *c = (ScrollInterceptCtx*)lv_event_get_user_data(e);
      lv_obj_t *sl = (lv_obj_t*)lv_event_get_target(e);
      if (code == LV_EVENT_PRESSED) {
        c->initial = (int)lv_slider_get_value(sl); c->ax = 0; c->ay = 0; c->adjust = false;
      } else if (code == LV_EVENT_PRESSING) {
        lv_indev_t *indev = lv_indev_get_act(); if (!indev) return; lv_point_t v; lv_indev_get_vect(indev, &v);
        c->ax += v.x >= 0 ? v.x : -v.x; c->ay += v.y >= 0 ? v.y : -v.y;
        if (!c->adjust) {
          if (c->ay > c->ax + 3) {
            if (c->scroll) lv_obj_scroll_by_bounded(c->scroll, 0, v.y, LV_ANIM_OFF);
            lv_slider_set_value(sl, c->initial, LV_ANIM_OFF);
            lv_event_stop_bubbling(e); lv_event_stop_processing(e); return;
          } else if (c->ax > c->ay + 3) {
            c->adjust = true;
          } else {
            lv_slider_set_value(sl, c->initial, LV_ANIM_OFF);
            lv_event_stop_bubbling(e); lv_event_stop_processing(e); return;
          }
        }
      } else if (code == LV_EVENT_VALUE_CHANGED) {
        if (!c->adjust) {
          lv_slider_set_value(sl, c->initial, LV_ANIM_OFF);
          lv_event_stop_bubbling(e); lv_event_stop_processing(e); return;
        }
      } else if (code == LV_EVENT_RELEASED) {
        if (!c->adjust) { lv_slider_set_value(sl, c->initial, LV_ANIM_OFF); }
      } else if (code == LV_EVENT_DELETE) {
        lv_mem_free(c);
      }
    }, LV_EVENT_ALL, ctx);
  };
  attach_interceptor(lv_slider_m1);
  attach_interceptor(lv_slider_m2);

  // Editable WiFi fields
  lv_obj_t *lblEdSsid = lv_label_create(content); lv_label_set_text(lblEdSsid, "WiFi SSID"); lv_obj_align(lblEdSsid, LV_ALIGN_TOP_LEFT, 0, 148);
  lv_ta_ssid = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_ssid, true); lv_obj_set_width(lv_ta_ssid, lv_pct(100)); lv_obj_align(lv_ta_ssid, LV_ALIGN_TOP_LEFT, 0, 172);
  lv_obj_t *lblEdPass = lv_label_create(content); lv_label_set_text(lblEdPass, "WiFi Password"); lv_obj_align(lblEdPass, LV_ALIGN_TOP_LEFT, 0, 220);
  lv_ta_pass = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_pass, true); lv_textarea_set_password_mode(lv_ta_pass, true); lv_obj_set_width(lv_ta_pass, lv_pct(100)); lv_obj_align(lv_ta_pass, LV_ALIGN_TOP_LEFT, 0, 244);

  // MQTT fields
  lv_obj_t *lblMqttHost = lv_label_create(content); lv_label_set_text(lblMqttHost, "MQTT Host"); lv_obj_align(lblMqttHost, LV_ALIGN_TOP_LEFT, 0, 292);
  lv_ta_mqtt_host = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_mqtt_host, true); lv_obj_set_width(lv_ta_mqtt_host, lv_pct(100)); lv_obj_align(lv_ta_mqtt_host, LV_ALIGN_TOP_LEFT, 0, 316);
  lv_obj_t *lblMqttPort = lv_label_create(content); lv_label_set_text(lblMqttPort, "MQTT Port"); lv_obj_align(lblMqttPort, LV_ALIGN_TOP_LEFT, 0, 364);
  lv_ta_mqtt_port = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_mqtt_port, true); lv_obj_set_width(lv_ta_mqtt_port, lv_pct(100)); lv_obj_align(lv_ta_mqtt_port, LV_ALIGN_TOP_LEFT, 0, 388);
  lv_obj_t *lblMqttUser = lv_label_create(content); lv_label_set_text(lblMqttUser, "MQTT User"); lv_obj_align(lblMqttUser, LV_ALIGN_TOP_LEFT, 0, 436);
  lv_ta_mqtt_user = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_mqtt_user, true); lv_obj_set_width(lv_ta_mqtt_user, lv_pct(100)); lv_obj_align(lv_ta_mqtt_user, LV_ALIGN_TOP_LEFT, 0, 460);
  lv_obj_t *lblMqttPass = lv_label_create(content); lv_label_set_text(lblMqttPass, "MQTT Password"); lv_obj_align(lblMqttPass, LV_ALIGN_TOP_LEFT, 0, 508);
  lv_ta_mqtt_pw = lv_textarea_create(content); lv_textarea_set_one_line(lv_ta_mqtt_pw, true); lv_textarea_set_password_mode(lv_ta_mqtt_pw, true); lv_obj_set_width(lv_ta_mqtt_pw, lv_pct(100)); lv_obj_align(lv_ta_mqtt_pw, LV_ALIGN_TOP_LEFT, 0, 532);

  // Pump Flow Rate Calibration section
  lv_obj_t *sepPump = lv_obj_create(content); lv_obj_remove_style_all(sepPump); lv_obj_set_size(sepPump, lv_pct(100), 2);
  lv_obj_set_style_bg_color(sepPump, lv_color_make(60,60,60), 0); lv_obj_set_style_bg_opa(sepPump, LV_OPA_COVER, 0);
  lv_obj_align_to(sepPump, lv_ta_mqtt_pw, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 24);
  
  lv_obj_t *lblPumpCal = lv_label_create(content); lv_label_set_text(lblPumpCal, "Pump Flow Rate Calibration");
  lv_obj_align_to(lblPumpCal, sepPump, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
  
  // M1 (pH pump) calibration
  lv_obj_t *lblM1Cal = lv_label_create(content); lv_label_set_text(lblM1Cal, "pH Pump (M1)");
  lv_obj_align_to(lblM1Cal, lblPumpCal, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);
  
  lv_lbl_cal_m1_status = lv_label_create(content); 
  lv_label_set_text(lv_lbl_cal_m1_status, "Ready");
  lv_obj_set_style_text_color(lv_lbl_cal_m1_status, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_align_to(lv_lbl_cal_m1_status, lblM1Cal, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
  
  lv_obj_t *btnM1Cal = lv_btn_create(content); 
  lv_obj_set_size(btnM1Cal, lv_pct(100), 40);
  lv_obj_align_to(btnM1Cal, lv_lbl_cal_m1_status, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
  lv_obj_t *lblBtnM1 = lv_label_create(btnM1Cal); 
  lv_label_set_text(lblBtnM1, "Start Calibratie (60s @ 100%)"); 
  lv_obj_center(lblBtnM1);
  lv_obj_add_event_cb(btnM1Cal, [](lv_event_t *e){
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_cal_motor != 0) return; // Already running
    g_cal_motor = 1;
    g_cal_countdown = 60;
    if (lv_lbl_cal_m1_status) lv_label_set_text(lv_lbl_cal_m1_status, "Running... 60s");
    if (handlers.onPumpCalStart) handlers.onPumpCalStart(1);
    if (!g_cal_timer) g_cal_timer = lv_timer_create(pump_cal_timer_cb, 1000, NULL);
  }, LV_EVENT_CLICKED, NULL);
  
  // M2 (ORP pump) calibration
  lv_obj_t *lblM2Cal = lv_label_create(content); lv_label_set_text(lblM2Cal, "ORP Pump (M2)");
  lv_obj_align_to(lblM2Cal, btnM1Cal, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 16);
  
  lv_lbl_cal_m2_status = lv_label_create(content); 
  lv_label_set_text(lv_lbl_cal_m2_status, "Ready");
  lv_obj_set_style_text_color(lv_lbl_cal_m2_status, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_align_to(lv_lbl_cal_m2_status, lblM2Cal, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
  
  lv_obj_t *btnM2Cal = lv_btn_create(content); 
  lv_obj_set_size(btnM2Cal, lv_pct(100), 40);
  lv_obj_align_to(btnM2Cal, lv_lbl_cal_m2_status, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
  lv_obj_t *lblBtnM2 = lv_label_create(btnM2Cal); 
  lv_label_set_text(lblBtnM2, "Start Calibratie (60s @ 100%)"); 
  lv_obj_center(lblBtnM2);
  lv_obj_add_event_cb(btnM2Cal, [](lv_event_t *e){
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_cal_motor != 0) return; // Already running
    g_cal_motor = 2;
    g_cal_countdown = 60;
    if (lv_lbl_cal_m2_status) lv_label_set_text(lv_lbl_cal_m2_status, "Running... 60s");
    if (handlers.onPumpCalStart) handlers.onPumpCalStart(2);
    if (!g_cal_timer) g_cal_timer = lv_timer_create(pump_cal_timer_cb, 1000, NULL);
  }, LV_EVENT_CLICKED, NULL);

#if USE_ANALOG_SENSORS
  // Calibration section (pH and ORP sensors), positioned under pump calibration
  lv_obj_t *sep = lv_obj_create(content); lv_obj_remove_style_all(sep); lv_obj_set_size(sep, lv_pct(100), 2);
  lv_obj_set_style_bg_color(sep, lv_color_make(60,60,60), 0); lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
  lv_obj_align_to(sep, btnM2Cal, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 24);

  lv_obj_t *lblCal = lv_label_create(content); lv_label_set_text(lblCal, "Calibration");
  lv_obj_align_to(lblCal, sep, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

  // Two buttons: pH Cal and ORP Cal in a row
  lv_obj_t *rowCal = lv_obj_create(content); lv_obj_remove_style_all(rowCal);
  lv_obj_set_width(rowCal, LV_PCT(100)); lv_obj_set_height(rowCal, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(rowCal, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(rowCal, 10, 0);
  lv_obj_align_to(rowCal, lblCal, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
  lv_obj_t *btnPhCal = lv_btn_create(rowCal); lv_obj_set_size(btnPhCal, 120, 36); { lv_obj_t *t=lv_label_create(btnPhCal); lv_label_set_text(t, "pH Cal"); lv_obj_center(t);} lv_obj_add_event_cb(btnPhCal, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) ui::showPhCalibration(); }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *btnOrpCal = lv_btn_create(rowCal); lv_obj_set_size(btnOrpCal, 120, 36); { lv_obj_t *t=lv_label_create(btnOrpCal); lv_label_set_text(t, "ORP Cal"); lv_obj_center(t);} lv_obj_add_event_cb(btnOrpCal, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) ui::showOrpCalibration(); }, LV_EVENT_CLICKED, NULL);
#endif

  // Footer
  lv_obj_t *footer = lv_obj_create(scr);
  lv_obj_remove_style_all(footer);
  lv_obj_set_size(footer, scr_w, footer_h);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -pad);
  lv_obj_set_style_bg_color(footer, lv_color_make(30,30,30), 0);
  lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(footer, 12, 0);
  lv_obj_set_style_pad_all(footer, pad, 0);
  lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(footer, 10, 0);

  // Reset WiFi button
  lv_obj_t *btnReset = lv_btn_create(footer); lv_obj_set_height(btnReset, footer_h-16); lv_obj_set_flex_grow(btnReset, 1); lv_obj_set_style_bg_color(btnReset, lv_palette_main(LV_PALETTE_GREY), 0); lv_obj_add_flag(btnReset, LV_OBJ_FLAG_CLICKABLE);
  { lv_obj_t *lbl = lv_label_create(btnReset); lv_label_set_text(lbl, "Reset WiFi"); lv_obj_center(lbl); lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE); }
  lv_obj_add_event_cb(btnReset, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) { if (handlers.onWifiReset) handlers.onWifiReset(); } }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btnSave = lv_btn_create(footer); lv_obj_set_height(btnSave, footer_h-16); lv_obj_set_flex_grow(btnSave, 1); lv_obj_set_style_bg_color(btnSave, lv_palette_main(LV_PALETTE_BLUE), 0); lv_obj_add_flag(btnSave, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *lblSave = lv_label_create(btnSave); lv_label_set_text(lblSave, "Save"); lv_obj_center(lblSave);
  lv_obj_clear_flag(lblSave, LV_OBJ_FLAG_CLICKABLE);  // Let touches pass through to button
  lv_obj_add_event_cb(btnSave, [](lv_event_t *e){
    if (lv_event_get_code(e)==LV_EVENT_CLICKED) {
      const char *s = lv_textarea_get_text(lv_ta_ssid);
      const char *p = lv_textarea_get_text(lv_ta_pass);
      if (handlers.onWifiSave) handlers.onWifiSave(s ? s : "", p ? p : "");
      const char *host = lv_textarea_get_text(lv_ta_mqtt_host);
      const char *port = lv_textarea_get_text(lv_ta_mqtt_port);
      const char *user = lv_textarea_get_text(lv_ta_mqtt_user);
      const char *pw   = lv_textarea_get_text(lv_ta_mqtt_pw);
      uint16_t prt = (uint16_t)((port && *port) ? atoi(port) : 0); // 0 = unspecified
      if (handlers.onMqttSave) handlers.onMqttSave(host ? host : "", prt, user ? user : "", pw ? pw : "");
      if (handlers.onSaveSettings) handlers.onSaveSettings();
    }
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btnCancel = lv_btn_create(footer); lv_obj_set_height(btnCancel, footer_h-16); lv_obj_set_flex_grow(btnCancel, 1); lv_obj_set_style_bg_color(btnCancel, lv_palette_main(LV_PALETTE_RED), 0); lv_obj_add_flag(btnCancel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *lblCancel = lv_label_create(btnCancel); lv_label_set_text(lblCancel, "Back"); lv_obj_center(lblCancel);
  lv_obj_clear_flag(lblCancel, LV_OBJ_FLAG_CLICKABLE);  // Let touches pass through to button
  lv_obj_add_event_cb(btnCancel, [](lv_event_t *e){
    if (lv_event_get_code(e)==LV_EVENT_CLICKED) {
      if (handlers.onCancelSettings) handlers.onCancelSettings();
    }
  }, LV_EVENT_CLICKED, NULL);

  // Keyboard binding
  lv_obj_t *kb = lv_keyboard_create(scr);
  lv_obj_set_width(kb, scr_w - pad*2);
  lv_obj_set_style_max_height(kb, scr_h/2, 0);
  // Align keyboard above footer so it doesn't overlap it
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -(pad + footer_h));
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  struct KbCtx { lv_obj_t *kb; lv_obj_t *content; lv_coord_t base_pad; lv_coord_t footer_h; lv_coord_t pad; };
  static KbCtx kbctx_store; KbCtx *kbctx = &kbctx_store;
  kbctx->kb = kb; kbctx->content = content; kbctx->base_pad = pad; kbctx->footer_h = footer_h; kbctx->pad = pad;
  auto bind_kb = [&](lv_obj_t *ta){
    lv_obj_add_event_cb(ta, [](lv_event_t *e){
      auto code = lv_event_get_code(e);
      lv_obj_t *ta = (lv_obj_t*)lv_event_get_target(e);
      KbCtx *c = (KbCtx*)lv_event_get_user_data(e);
      if (!c) return;
      lv_obj_t *kb = c->kb;
      if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        // Ensure TA is visible above keyboard: increase bottom padding and scroll into view
        if (c->content) {
          lv_coord_t kh = lv_obj_get_height(kb);
          lv_obj_set_style_pad_bottom(c->content, kh + c->pad, 0);
          lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON);
        }
      } else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(kb, NULL);
        if (c->content) lv_obj_set_style_pad_bottom(c->content, c->base_pad, 0);
      }
    }, LV_EVENT_ALL, kbctx);
  };
  bind_kb(lv_ta_ssid);
  bind_kb(lv_ta_pass);
  bind_kb(lv_ta_mqtt_host);
  bind_kb(lv_ta_mqtt_port);
  bind_kb(lv_ta_mqtt_user);
  bind_kb(lv_ta_mqtt_pw);
  // Hide keyboard on ready/cancel
  lv_obj_add_event_cb(kb, [](lv_event_t *e){
    auto code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
      lv_obj_t *kb = (lv_obj_t*)lv_event_get_target(e);
      lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
  }, LV_EVENT_ALL, NULL);
  // Fields will be populated by caller after opening settings
}
// --- Calibration Dialogs ---
static void modal_close_async(lv_obj_t *modal){ lv_timer_t *t = lv_timer_create([](lv_timer_t *tm){ lv_obj_t *obj=(lv_obj_t*)tm->user_data; if (obj) lv_obj_del(obj); }, 0, modal); lv_timer_set_repeat_count(t, 1); }

void showPhCalibration(){
  if (lv_modal_active) { lv_obj_del(lv_modal_active); lv_modal_active=nullptr; }
  lv_obj_t *modal = lv_obj_create(lv_layer_top()); lv_modal_active = modal;
  lv_obj_set_size(modal, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_style_bg_opa(modal, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
  lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(modal, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *dlg = lv_obj_create(modal); lv_obj_set_size(dlg, lv_disp_get_hor_res(NULL)-40, lv_disp_get_ver_res(NULL)-40); lv_obj_center(dlg); lv_obj_set_style_radius(dlg, 10, 0); lv_obj_set_style_pad_all(dlg, 12, 0);
  lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *title = lv_label_create(dlg); lv_label_set_text(title, "pH Calibration"); lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *lbl1 = lv_label_create(dlg); lv_label_set_text(lbl1, "Place in pH 4.00 buffer and press SAMPLE"); lv_obj_align(lbl1, LV_ALIGN_TOP_LEFT, 0, 32);
  lv_obj_t *val4 = lv_label_create(dlg); lv_label_set_text(val4, "V4: ---.-- V"); lv_obj_align(val4, LV_ALIGN_TOP_LEFT, 0, 56);
  lv_obj_t *btn4 = lv_btn_create(dlg); lv_obj_set_size(btn4, 110, 36); lv_obj_align(btn4, LV_ALIGN_TOP_RIGHT, -8, 48); { lv_obj_t *t=lv_label_create(btn4); lv_label_set_text(t, "Sample"); lv_obj_center(t);} 
  lv_obj_t *lbl2 = lv_label_create(dlg); lv_label_set_text(lbl2, "Place in pH 10.00 buffer and press SAMPLE"); lv_obj_align(lbl2, LV_ALIGN_TOP_LEFT, 0, 92);
  lv_obj_t *val10 = lv_label_create(dlg); lv_label_set_text(val10, "V10: ---.-- V"); lv_obj_align(val10, LV_ALIGN_TOP_LEFT, 0, 116);
  lv_obj_t *btn10 = lv_btn_create(dlg); lv_obj_set_size(btn10, 110, 36); lv_obj_align(btn10, LV_ALIGN_TOP_RIGHT, -8, 108); { lv_obj_t *t=lv_label_create(btn10); lv_label_set_text(t, "Sample"); lv_obj_center(t);} 

  lv_obj_t *btnCancel = lv_btn_create(dlg); lv_obj_set_size(btnCancel, 120, 44); lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_LEFT, 0, 0); { lv_obj_t *t=lv_label_create(btnCancel); lv_label_set_text(t, "Cancel"); lv_obj_center(t);} 
  lv_obj_t *btnSave   = lv_btn_create(dlg); lv_obj_set_size(btnSave, 120, 44); lv_obj_align(btnSave, LV_ALIGN_BOTTOM_RIGHT, 0, 0); { lv_obj_t *t=lv_label_create(btnSave); lv_label_set_text(t, "Save"); lv_obj_center(t);} 

  struct Ctx { lv_obj_t *modal; lv_obj_t *v4; lv_obj_t *v10; float s4=0, s10=0; };
  Ctx *ctx = (Ctx*)lv_mem_alloc(sizeof(Ctx)); ctx->modal=modal; ctx->v4=val4; ctx->v10=val10; ctx->s4=0; ctx->s10=0;
  lv_obj_add_event_cb(btn4, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) { Ctx *c=(Ctx*)lv_event_get_user_data(e);
#if USE_ANALOG_SENSORS
    extern io::AnalogPhOrpSensor g_analog; float v = g_analog.sampleVoltsPh();
#else
    float v = 0.0f;
#endif
    c->s4 = v; char b[24]; snprintf(b,sizeof(b),"V4: %.3f V", v); lv_label_set_text(c->v4,b);} }, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(btn10, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) { Ctx *c=(Ctx*)lv_event_get_user_data(e);
#if USE_ANALOG_SENSORS
    extern io::AnalogPhOrpSensor g_analog; float v = g_analog.sampleVoltsPh();
#else
    float v = 0.0f;
#endif
    c->s10 = v; char b[24]; snprintf(b,sizeof(b),"V10: %.3f V", v); lv_label_set_text(c->v10,b);} }, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(btnCancel, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED){ Ctx *c=(Ctx*)lv_event_get_user_data(e); modal_close_async(c->modal); lv_mem_free(c); } }, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(btnSave, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED){ Ctx *c=(Ctx*)lv_event_get_user_data(e); ::g_storage.setPhVAt4(c->s4 > 0 ? c->s4 : ::g_storage.getPhVAt4(3.00f)); ::g_storage.setPhVAt10(c->s10 > 0 ? c->s10 : ::g_storage.getPhVAt10(2.00f));
#if USE_ANALOG_SENSORS
    extern io::AnalogPhOrpSensor g_analog; io::AnalogPhOrpSensor::PhCal cal = g_analog.getPhCalibration(); cal.voltsAtPh4 = ::g_storage.getPhVAt4(cal.voltsAtPh4); cal.voltsAtPh10 = ::g_storage.getPhVAt10(cal.voltsAtPh10); g_analog.setPhCalibration(cal);
#endif
    modal_close_async(c->modal); lv_mem_free(c);} }, LV_EVENT_CLICKED, ctx);
}

void showOrpCalibration(){
  if (lv_modal_active) { lv_obj_del(lv_modal_active); lv_modal_active=nullptr; }
  lv_obj_t *modal = lv_obj_create(lv_layer_top()); lv_modal_active = modal;
  lv_obj_set_size(modal, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_style_bg_opa(modal, LV_OPA_50, 0);
  lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
  lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(modal, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *dlg = lv_obj_create(modal); lv_obj_set_size(dlg, lv_disp_get_hor_res(NULL)-40, lv_disp_get_ver_res(NULL)-40); lv_obj_center(dlg); lv_obj_set_style_radius(dlg, 10, 0); lv_obj_set_style_pad_all(dlg, 12, 0);
  lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *title = lv_label_create(dlg); lv_label_set_text(title, "ORP Calibration"); lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *lbl1 = lv_label_create(dlg); lv_label_set_text(lbl1, "Place in 0 mV solution and press SAMPLE"); lv_obj_align(lbl1, LV_ALIGN_TOP_LEFT, 0, 32);
  lv_obj_t *val0 = lv_label_create(dlg); lv_label_set_text(val0, "V0: ---.-- V"); lv_obj_align(val0, LV_ALIGN_TOP_LEFT, 0, 56);
  lv_obj_t *btn0 = lv_btn_create(dlg); lv_obj_set_size(btn0, 110, 36); lv_obj_align(btn0, LV_ALIGN_TOP_RIGHT, -8, 48); { lv_obj_t *t=lv_label_create(btn0); lv_label_set_text(t, "Sample"); lv_obj_center(t);} 
  lv_obj_t *lbl2 = lv_label_create(dlg); lv_label_set_text(lbl2, "Adjust mV/V scale if needed"); lv_obj_align(lbl2, LV_ALIGN_TOP_LEFT, 0, 92);
  lv_obj_t *taScale = lv_textarea_create(dlg); lv_textarea_set_one_line(taScale, true); lv_obj_set_width(taScale, 120); lv_obj_align(taScale, LV_ALIGN_TOP_LEFT, 0, 116); lv_textarea_set_text(taScale, "1000");

  lv_obj_t *btnCancel = lv_btn_create(dlg); lv_obj_set_size(btnCancel, 120, 44); lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_LEFT, 0, 0); { lv_obj_t *t=lv_label_create(btnCancel); lv_label_set_text(t, "Cancel"); lv_obj_center(t);} 
  lv_obj_t *btnSave   = lv_btn_create(dlg); lv_obj_set_size(btnSave, 120, 44); lv_obj_align(btnSave, LV_ALIGN_BOTTOM_RIGHT, 0, 0); { lv_obj_t *t=lv_label_create(btnSave); lv_label_set_text(t, "Save"); lv_obj_center(t);} 

  struct Ctx { lv_obj_t *modal; lv_obj_t *v0; lv_obj_t *ta; float s0=0; };
  Ctx *ctx = (Ctx*)lv_mem_alloc(sizeof(Ctx)); ctx->modal=modal; ctx->v0=val0; ctx->ta=taScale; ctx->s0=0;
  lv_obj_add_event_cb(btn0, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED) { Ctx *c=(Ctx*)lv_event_get_user_data(e);
#if USE_ANALOG_SENSORS
    extern io::AnalogPhOrpSensor g_analog; float v = g_analog.sampleVoltsOrp();
#else
    float v = 0.0f;
#endif
    c->s0 = v; char b[24]; snprintf(b,sizeof(b),"V0: %.3f V", v); lv_label_set_text(c->v0,b);} }, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(btnCancel, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED){ Ctx *c=(Ctx*)lv_event_get_user_data(e); modal_close_async(c->modal); lv_mem_free(c); } }, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(btnSave, [](lv_event_t *e){ if (lv_event_get_code(e)==LV_EVENT_CLICKED){ Ctx *c=(Ctx*)lv_event_get_user_data(e); float mvperv = 1000.0f; const char *t=lv_textarea_get_text(c->ta); if (t && *t) { mvperv = (float)atof(t); if (mvperv < 100.0f) mvperv = 100.0f; if (mvperv > 5000.0f) mvperv = 5000.0f; } ::g_storage.setOrpVAt0(c->s0 > 0 ? c->s0 : ::g_storage.getOrpVAt0(2.50f)); ::g_storage.setOrpMvPerV(mvperv);
#if USE_ANALOG_SENSORS
    extern io::AnalogPhOrpSensor g_analog; io::AnalogPhOrpSensor::OrpCal cal = g_analog.getOrpCalibration(); cal.voltsAt0mV = ::g_storage.getOrpVAt0(cal.voltsAt0mV); cal.mVPerVolt = ::g_storage.getOrpMvPerV(cal.mVPerVolt); g_analog.setOrpCalibration(cal);
#endif
    modal_close_async(c->modal); lv_mem_free(c);} }, LV_EVENT_CLICKED, ctx);
}

void setSavedPhCalibration(float v_at4, float v_at10){ (void)v_at4; (void)v_at10; }
void setSavedOrpCalibration(float v_at0, float mv_per_v){ (void)v_at0; (void)mv_per_v; }

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
  if (lv_ta_mqtt_port) {
    if (port == 0) { lv_textarea_set_text(lv_ta_mqtt_port, ""); }
    else { char b[8]; snprintf(b,sizeof(b),"%u", (unsigned)port); lv_textarea_set_text(lv_ta_mqtt_port, b); }
  }
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


