#include "User_Setup_JC2432W328C.h"
#include <LovyanGFX.hpp>
#include "CST820.h"
#include <lvgl.h>
#include "my_font_28.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <TinyGPSPlus.h>
#include <stdlib.h>
#include <string.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#endif

// ===== Pin Configuration (edit here first) =====
// Display SPI pins (ST7789 over hardware SPI)
// SCK/MOSI must be output-capable GPIOs; common choices: 14/13 or 18/23.
constexpr int PIN_TFT_SCLK = 14;
constexpr int PIN_TFT_MOSI = 13;
// CS/DC can be any output-capable GPIO.
constexpr int PIN_TFT_CS   = 15;
constexpr int PIN_TFT_DC   = 2;

// Touch controller pins (keep constructor order matching CST820 library).
// If your touch is I2C-based in your wiring, SDA/SCL can be remapped on ESP32.
constexpr int PIN_TOUCH_1 = 33;
constexpr int PIN_TOUCH_2 = 32;
constexpr int PIN_TOUCH_3 = 25;
constexpr int PIN_TOUCH_4 = 21;

// Backlight PWM pin (must support PWM/LEDC output).
// Most output GPIOs work; avoid input-only 34-39.
constexpr int PIN_BACKLIGHT = 27;

// External SD on dedicated HSPI bus to avoid display SPI conflicts.
// SCK/MISO/MOSI: SPI-capable GPIOs; common stable mapping is 18/19/23.
// CS: any output-capable GPIO.
constexpr int PIN_SD_SCK  = 18;
constexpr int PIN_SD_MISO = 19;
constexpr int PIN_SD_MOSI = 23;
constexpr int PIN_SD_CS   = 5;

SPIClass sd_spi(HSPI);

bool log_request = false;

String pending_species;
String pending_size;
String pending_weight;

// ===== GPS =====
TinyGPSPlus gps;

bool gps_stream_detected = false;
bool gps_fix_announced = false;

unsigned long last_gps_print = 0;
const unsigned long GPS_PRINT_INTERVAL = 30000; // 30 seconds

// ===== Global Time Overlay Label =====
lv_obj_t* global_time_label = nullptr;

// ===== User Settings =====
int brightness_level = 255;          // 0-255 PWM level
bool timezone_auto_enabled = true;   // Auto-update timezone from GPS longitude
int timezone_offset_hours = -4;      // UTC offset; updated by GPS when auto is enabled
// Longitude gives ~standard-time zones; many regions add +1h in summer (DST). GPS has no IANA TZ DB.
bool auto_dst_enabled = true;        // When true, add ~1h in a US-style summer window (see get_dst_extra_hours)

// ===== Settings Screen Widgets =====
lv_obj_t* settings_brightness_value_label = nullptr;
lv_obj_t* settings_timezone_value_label = nullptr;
lv_obj_t* settings_timezone_mode_label = nullptr;
lv_obj_t* settings_tz_minus_btn = nullptr;
lv_obj_t* settings_tz_plus_btn = nullptr;

// ===== Display Setup =====
class LGFX_JustDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI _bus;

public:
  LGFX_JustDisplay(void) {
    { auto cfg = _bus.config();
      cfg.spi_host   = VSPI_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = 1;
      cfg.pin_sclk   = PIN_TFT_SCLK;
      cfg.pin_mosi   = PIN_TFT_MOSI;
      cfg.pin_miso   = -1;
      cfg.pin_dc     = PIN_TFT_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    { auto cfg = _panel.config();
      cfg.pin_cs           = PIN_TFT_CS; // Display CS
      cfg.pin_rst          = -1;
      cfg.pin_busy         = -1;
      cfg.panel_width      = 240;
      cfg.panel_height     = 320;
      cfg.offset_rotation  = 0;
      cfg.readable         = false;
      cfg.invert           = false;
      cfg.bus_shared       = true;
      _panel.config(cfg);
    }

    setPanel(&_panel);
  }
  
};

LGFX_JustDisplay tft;
CST820 touch(PIN_TOUCH_1, PIN_TOUCH_2, PIN_TOUCH_3, PIN_TOUCH_4);

// ===== LVGL display handle =====
lv_display_t* disp;

#define LVGL_TICK_PERIOD 5

// ===== Forward Declarations =====
void create_main_menu();
void create_condition_screen();
void create_log_screen();
void create_settings_screen();
void create_about_screen();
void create_map_screen();
void create_view_entries_screen();
void create_gps_screen();
void create_time_overlay();
void replace_root_screen(lv_obj_t* new_screen, bool opening_main_menu = false);
void refresh_settings_labels();
void apply_brightness(int value);
int compute_timezone_offset_from_longitude(double longitude);
int get_dst_extra_hours();
int normalized_local_hour(int utc_hour);

// ===== LVGL flush =====
void lv_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* color_p) {
  tft.pushImage(area->x1, area->y1,
                area->x2 - area->x1 + 1,
                area->y2 - area->y1 + 1,
                (lgfx::rgb565_t*)color_p);
  lv_display_flush_ready(disp);
}

// ===== Touch =====
void touchpad_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
  uint16_t rawX, rawY;
  if (touch.getTouch(&rawX, &rawY)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = rawY;
    data->point.y = 240 - rawX;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ===== Back Button Callback =====
void back_to_menu(lv_event_t* e) {
  create_main_menu();
}

// ===== Helper: Create Back Button =====
lv_obj_t* add_back_button(lv_obj_t* parent) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, 100, 35);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);

  lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(btn, lv_color_hex(0x00AA00), LV_PART_MAIN);

  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, "Back");
  lv_obj_center(label);
  lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);

  lv_obj_add_event_cb(btn, back_to_menu, LV_EVENT_CLICKED, NULL);
  return btn;
}

// ===== SD Mount Flag =====
bool sd_mounted = false;

// ===== SD Logging (safe SPI switching) =====
void log_catch_safe(String species, String size, String weight) {
  if (!sd_mounted) { 
    Serial.println("❌ SD not mounted"); 
    return; 
  }

  // Open file for write (append)
  File file = SD.open("/catch_log.csv", FILE_WRITE);
  if (!file) {
    Serial.println("❌ Cannot open file");
    return;
  }

  // Write entry
  String entry = species + "," + size + "," + weight + "," + String(millis()) + "\n";
  file.print(entry);
  file.close();

  Serial.println("✅ Catch logged");
}

// ===== Delete Entry (with SD reinit) =====
void delete_entry(const String& line) {
  if (!sd_mounted) return;

  File file = SD.open("/catch_log.csv");
  if (!file) {
    Serial.println("❌ Cannot open catch log for delete");
    return;
  }

  File temp = SD.open("/temp.csv", FILE_WRITE);
  if (!temp) {
    Serial.println("❌ Cannot open temp file for delete");
    file.close();
    return;
  }
  String l;
  unsigned n = 0;
  while (file.available()) {
    l = file.readStringUntil('\n');
    if (l != line) temp.println(l);
    if ((++n & 0x1Fu) == 0) {
#if defined(ARDUINO_ARCH_ESP32)
      yield();
#endif
    }
  }

  file.close();
  temp.close();

  SD.remove("/catch_log.csv");
  SD.rename("/temp.csv", "/catch_log.csv");
}

// ===== Log Catch Screen =====
lv_obj_t *species_input, *size_input, *weight_input;
lv_obj_t* log_cont;

struct kb_userdata_t {
    lv_obj_t* kb;
    lv_obj_t* submit_btn;
    lv_obj_t* view_btn;
    lv_obj_t* back_btn;
};

static kb_userdata_t s_log_kb_userdata;

void textarea_focused_cb(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    kb_userdata_t* ud = (kb_userdata_t*)lv_event_get_user_data(e);

    lv_keyboard_set_textarea(ud->kb, ta);
    lv_obj_clear_flag(ud->kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ud->kb);

    if(ud->submit_btn) lv_obj_add_flag(ud->submit_btn, LV_OBJ_FLAG_HIDDEN);
    if(ud->view_btn) lv_obj_add_flag(ud->view_btn, LV_OBJ_FLAG_HIDDEN);
    if(ud->back_btn) lv_obj_add_flag(ud->back_btn, LV_OBJ_FLAG_HIDDEN);

    lv_coord_t kb_h = lv_obj_get_height(ud->kb);
    lv_coord_t ta_y = lv_obj_get_y(ta);
    lv_coord_t cont_y = lv_obj_get_y(log_cont);

    if(ta_y + kb_h > 240) {
        lv_obj_set_y(log_cont, cont_y - (ta_y + kb_h - 240 + 10));
    }

    lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON);
}

void kb_event_cb(lv_event_t* e) {
    lv_obj_t* kb = (lv_obj_t*)lv_event_get_target(e);
    kb_userdata_t* ud = (kb_userdata_t*)lv_event_get_user_data(e);

    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    if(ud->submit_btn) lv_obj_clear_flag(ud->submit_btn, LV_OBJ_FLAG_HIDDEN);
    if(ud->view_btn) lv_obj_clear_flag(ud->view_btn, LV_OBJ_FLAG_HIDDEN);
    if(ud->back_btn) lv_obj_clear_flag(ud->back_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_y(log_cont, 30);
}

lv_obj_t* gps_info_label;
// Opening GPS Info while a fix is already valid: TinyGPS may not report isUpdated() again until the next sentence.
static bool s_force_gps_label_paint = false;

// Old screens were never deleted — each navigation leaked the whole LVGL tree until heap exhaustion / random freeze.
// Clear globals that pointed at widgets on the outgoing screen so loop() never touches freed objects.
// (Must live after all of these globals are declared — Arduino compiles top-to-bottom.)
static void clear_ui_refs_on_screen_change() {
  gps_info_label = nullptr;
  settings_brightness_value_label = nullptr;
  settings_timezone_value_label = nullptr;
  settings_timezone_mode_label = nullptr;
  settings_tz_minus_btn = nullptr;
  settings_tz_plus_btn = nullptr;
  species_input = nullptr;
  size_input = nullptr;
  weight_input = nullptr;
  log_cont = nullptr;
}

static constexpr uint32_t UI_SCREEN_ANIM_MS = 260;

void replace_root_screen(lv_obj_t* new_screen, bool opening_main_menu) {
  lv_obj_t* old_scr = lv_screen_active();
  // Do NOT clear_ui_refs here — create_*() already set pointers to widgets on new_screen;
  // clearing after that left gps_info_label (and others) nullptr forever. Clear at the *start*
  // of each full-screen builder instead.

  if (old_scr == nullptr || old_scr == new_screen) {
    lv_screen_load(new_screen);
    return;
  }

  // Animated transition; auto_del deletes old_scr when the animation finishes (no double-delete).
  lv_screen_load_anim_t anim = opening_main_menu ? LV_SCREEN_LOAD_ANIM_MOVE_RIGHT
                                                 : LV_SCREEN_LOAD_ANIM_MOVE_LEFT;
  lv_screen_load_anim(new_screen, anim, UI_SCREEN_ANIM_MS, 0, true);
}

static void menu_btn_fade_anim_cb(void* var, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t*)var, (lv_opa_t)v, LV_PART_MAIN);
}

void apply_brightness(int value) {
  brightness_level = constrain(value, 0, 255);
  analogWrite(PIN_BACKLIGHT, brightness_level);
}

int compute_timezone_offset_from_longitude(double longitude) {
  int tz = (int)round(longitude / 15.0);
  return constrain(tz, -12, 14);
}

// Approximate +1h daylight saving for mid-latitudes (US-style calendar, northern hemisphere only).
// Not exact on transition Sundays; turn off in Settings if you are in AZ/HI or outside US-style DST.
int get_dst_extra_hours() {
  if (!auto_dst_enabled) return 0;
  if (!gps.date.isValid()) return 0;
  if (gps.location.isValid() && gps.location.lat() < 0) return 0;

  int m = gps.date.month();
  int d = gps.date.day();
  if (m >= 4 && m <= 10) return 1;   // Apr–Oct (month 10 = October)
  if (m == 3 && d >= 8) return 1;    // ~on or after US spring forward (2nd Sun Mar)
  return 0;
}

int effective_timezone_offset_hours() {
  return constrain(timezone_offset_hours + get_dst_extra_hours(), -12, 14);
}

int normalized_local_hour(int utc_hour) {
  int local_hour = utc_hour + effective_timezone_offset_hours();
  while (local_hour < 0) local_hour += 24;
  while (local_hour >= 24) local_hour -= 24;
  return local_hour;
}

void refresh_settings_labels() {
  if (settings_brightness_value_label != nullptr) {
    char bbuf[16];
    sprintf(bbuf, "%d%%", (brightness_level * 100) / 255);
    lv_label_set_text(settings_brightness_value_label, bbuf);
  }

  if (settings_timezone_value_label != nullptr) {
    char tzbuf[40];
    int eff = effective_timezone_offset_hours();
    int dst = get_dst_extra_hours();
    if (dst != 0) {
      sprintf(tzbuf, "Local UTC%+d (base %+d +%dh DST)", eff, timezone_offset_hours, dst);
    } else {
      sprintf(tzbuf, "Local UTC%+d", eff);
    }
    lv_label_set_text(settings_timezone_value_label, tzbuf);
  }

  if (settings_timezone_mode_label != nullptr) {
    lv_label_set_text(settings_timezone_mode_label, timezone_auto_enabled ? "Mode: Auto (GPS)" : "Mode: Manual");
  }

  if (settings_tz_minus_btn != nullptr && settings_tz_plus_btn != nullptr) {
    if (timezone_auto_enabled) {
      lv_obj_add_flag(settings_tz_minus_btn, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(settings_tz_plus_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(settings_tz_minus_btn, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(settings_tz_plus_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void create_gps_screen() {
  clear_ui_refs_on_screen_change();

  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "GPS Info");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

  gps_info_label = lv_label_create(screen);
  lv_label_set_text(gps_info_label, "Waiting for GPS data...");
  lv_obj_align(gps_info_label, LV_ALIGN_TOP_LEFT, 10, 40);
  lv_obj_set_style_text_color(gps_info_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

  add_back_button(screen);

  s_force_gps_label_paint = true;
  replace_root_screen(screen);
}

static void paint_gps_summary_label() {
  if (gps_info_label == nullptr || !gps.location.isValid()) return;

  static char s_gps_info_buf[768];
  int local_hour = normalized_local_hour(gps.time.hour());
  int hour12 = local_hour % 12;
  if (hour12 == 0) hour12 = 12;
  const char* ampm = (local_hour >= 12) ? "PM" : "AM";

  char timebuf[20];
  snprintf(timebuf, sizeof(timebuf), "%d:%02d:%02d %s",
           hour12, gps.time.minute(), gps.time.second(), ampm);

  int eff_off = effective_timezone_offset_hours();

  snprintf(
    s_gps_info_buf, sizeof(s_gps_info_buf),
    "Lat: %.6f\n"
    "Lon: %.6f\n"
    "Alt (ft): %.1f\n"
    "Speed (mph): %.1f\n"
    "Heading: %.1f\n"
    "Satellites: %u\n"
    "HDOP: %.2f\n"
    "Date: %u/%u/%u\n"
    "Local (UTC%+d): %s",
    gps.location.lat(),
    gps.location.lng(),
    (double)(gps.altitude.meters() * 3.28084),
    (double)gps.speed.mph(),
    (double)gps.course.deg(),
    (unsigned)gps.satellites.value(),
    (double)gps.hdop.hdop(),
    (unsigned)gps.date.month(),
    (unsigned)gps.date.day(),
    (unsigned)gps.date.year(),
    eff_off,
    timebuf);

  lv_label_set_text(gps_info_label, s_gps_info_buf);
}

// Time/date often decode before a 3D fix; show live status instead of a frozen "Waiting..." line.
static void paint_gps_no_fix_label() {
  if (gps_info_label == nullptr) return;

  static char buf[576];
  size_t n = 0;

  n += snprintf(buf + n, sizeof(buf) - n, "No position fix yet.\n\n");

  if (!gps_stream_detected) {
    snprintf(buf + n, sizeof(buf) - n,
             "No data on Serial.\n"
             "Check: GPS TX -> ESP RX, 9600 baud,\n"
             "shared UART0 vs USB debug.");
    lv_label_set_text(gps_info_label, buf);
    return;
  }

  n += snprintf(buf + n, sizeof(buf) - n, "NMEA OK — need sky view / more sats.\n\n");

  if (gps.time.isValid()) {
    int lh = normalized_local_hour(gps.time.hour());
    int h12 = lh % 12;
    if (h12 == 0) h12 = 12;
    const char* ap = (lh >= 12) ? "PM" : "AM";
    n += snprintf(buf + n, sizeof(buf) - n,
                  "Local: %d:%02d:%02d %s\n",
                  h12, gps.time.minute(), gps.time.second(), ap);
    n += snprintf(buf + n, sizeof(buf) - n,
                  "UTC:   %02d:%02d:%02d\n",
                  gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    n += snprintf(buf + n, sizeof(buf) - n, "Time: not decoded yet\n");
  }

  if (gps.date.isValid()) {
    n += snprintf(buf + n, sizeof(buf) - n,
                  "Date: %u/%u/%u\n",
                  (unsigned)gps.date.month(),
                  (unsigned)gps.date.day(),
                  (unsigned)gps.date.year());
  }

  if (gps.satellites.isValid()) {
    n += snprintf(buf + n, sizeof(buf) - n,
                  "Satellites: %u\n", (unsigned)gps.satellites.value());
  }
  if (gps.hdop.isValid()) {
    n += snprintf(buf + n, sizeof(buf) - n, "HDOP: %.2f\n", (double)gps.hdop.hdop());
  }

  lv_label_set_text(gps_info_label, buf);
}

void create_log_screen() {
  clear_ui_refs_on_screen_change();

  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* title_label = lv_label_create(screen);
  lv_label_set_text(title_label, "Log Catch");
  lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

  log_cont = lv_obj_create(screen);
  lv_obj_set_size(log_cont, 320, 240 - 50);
  lv_obj_align(log_cont, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_set_scroll_dir(log_cont, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(log_cont, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_bg_color(log_cont, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_border_width(log_cont, 0, LV_PART_MAIN);

  species_input = lv_textarea_create(log_cont);
  lv_textarea_set_placeholder_text(species_input, "Species");
  lv_obj_set_size(species_input, 200, 40);
  lv_obj_align(species_input, LV_ALIGN_TOP_LEFT, 10, 10);

  lv_obj_t* species_label = lv_label_create(log_cont);
  lv_label_set_text(species_label, "Species:");
  lv_obj_align_to(species_label, species_input, LV_ALIGN_OUT_LEFT_MID, -70, 0);

  size_input = lv_textarea_create(log_cont);
  lv_textarea_set_placeholder_text(size_input, "cm");
  lv_obj_set_size(size_input, 200, 40);
  lv_obj_align(size_input, LV_ALIGN_TOP_LEFT, 10, 60);

  lv_obj_t* size_label = lv_label_create(log_cont);
  lv_label_set_text(size_label, "Size:");
  lv_obj_align_to(size_label, size_input, LV_ALIGN_OUT_LEFT_MID, -70, 0);

  weight_input = lv_textarea_create(log_cont);
  lv_textarea_set_placeholder_text(weight_input, "kg");
  lv_obj_set_size(weight_input, 200, 40);
  lv_obj_align(weight_input, LV_ALIGN_TOP_LEFT, 10, 110);

  lv_obj_t* weight_label = lv_label_create(log_cont);
  lv_label_set_text(weight_label, "Weight:");
  lv_obj_align_to(weight_label, weight_input, LV_ALIGN_OUT_LEFT_MID, -70, 0);

  lv_obj_t* kb = lv_keyboard_create(screen);
  lv_obj_set_size(kb, 320, 120);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* submit_btn = lv_button_create(screen);
  lv_obj_set_size(submit_btn, 100, 35);
  lv_obj_align(submit_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_set_style_bg_color(submit_btn, lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(submit_btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(submit_btn, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(submit_btn, lv_color_hex(0x00AA00), LV_PART_MAIN);
  lv_obj_t* submit_label = lv_label_create(submit_btn);
  lv_label_set_text(submit_label, "Submit");
  lv_obj_center(submit_label);

  lv_obj_add_event_cb(submit_btn, [](lv_event_t* e){
    pending_species = String(lv_textarea_get_text(species_input));
    pending_size    = String(lv_textarea_get_text(size_input));
    pending_weight  = String(lv_textarea_get_text(weight_input));

    log_request = true;
    lv_textarea_set_text(species_input, "");
    lv_textarea_set_text(size_input, "");
    lv_textarea_set_text(weight_input, "");
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t* back_btn = add_back_button(screen);

  s_log_kb_userdata = { kb, submit_btn, nullptr, back_btn };
  kb_userdata_t* kb_data = &s_log_kb_userdata;
  lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY, kb_data);
  lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, kb_data);

  lv_obj_add_event_cb(species_input, textarea_focused_cb, LV_EVENT_FOCUSED, kb_data);
  lv_obj_add_event_cb(size_input, textarea_focused_cb, LV_EVENT_FOCUSED, kb_data);
  lv_obj_add_event_cb(weight_input, textarea_focused_cb, LV_EVENT_FOCUSED, kb_data);

  replace_root_screen(screen);
}

struct CatchLineUd {
  char line[192];
};

static void view_entry_del_btn_cb(lv_event_t* e) {
  CatchLineUd* ud = (CatchLineUd*)lv_event_get_user_data(e);
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    if (!ud) return;
    delete_entry(String(ud->line));
    create_view_entries_screen();
    return;
  }
  if (code == LV_EVENT_DELETE) {
    free(ud);
  }
}

// ===== View Entries Screen =====
void create_view_entries_screen() {
  clear_ui_refs_on_screen_change();

  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* title_label = lv_label_create(screen);
  lv_label_set_text(title_label, "Catch Log");
  lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t* cont = lv_obj_create(screen);
  lv_obj_set_size(cont, 320, 240 - 50);
  lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_set_scroll_dir(cont, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_bg_color(cont, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);

  if (!sd_mounted) {
    lv_label_set_text(lv_label_create(screen), "SD card error");
    replace_root_screen(screen);
    return;
  }

  File file = SD.open("/catch_log.csv");
  if (!file) {
    lv_label_set_text(lv_label_create(screen), "No entries");
    replace_root_screen(screen);
    return;
  }

  int y = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');

    lv_obj_t* entry_label = lv_label_create(cont);
    lv_label_set_text(entry_label, line.c_str());
    lv_obj_align(entry_label, LV_ALIGN_TOP_LEFT, 10, y);

    lv_obj_t* del_btn = lv_button_create(cont);
    lv_obj_set_size(del_btn, 60, 30);
    lv_obj_align(del_btn, LV_ALIGN_TOP_RIGHT, -10, y);
    lv_obj_set_style_bg_color(del_btn, lv_color_hex(0xFF5555), LV_PART_MAIN);

    lv_obj_t* del_label = lv_label_create(del_btn);
    lv_label_set_text(del_label, "Del");
    lv_obj_center(del_label);

    CatchLineUd* ud = (CatchLineUd*)malloc(sizeof(CatchLineUd));
    if (ud) {
      strncpy(ud->line, line.c_str(), sizeof(ud->line) - 1);
      ud->line[sizeof(ud->line) - 1] = '\0';
      lv_obj_add_event_cb(del_btn, view_entry_del_btn_cb, LV_EVENT_ALL, ud);
    }

    y += 35;
  }
  file.close();

  add_back_button(screen);
  replace_root_screen(screen);
}

// ===== Other Screens =====
void create_condition_screen() {
  clear_ui_refs_on_screen_change();
  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_t* label = lv_label_create(screen);
  lv_label_set_text(label, "Condition Test Screen");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);
  add_back_button(screen);
  replace_root_screen(screen);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
}
void create_settings_screen() {
  clear_ui_refs_on_screen_change();

  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "Settings");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_text_color(title, lv_color_hex(0x00FF00), LV_PART_MAIN);

  lv_obj_t* cont = lv_obj_create(screen);
  lv_obj_set_size(cont, 300, 178);
  lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 35);
  lv_obj_set_style_bg_color(cont, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
  lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(cont, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_scroll_dir(cont, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_t* bright_title = lv_label_create(cont);
  lv_label_set_text(bright_title, "Screen Brightness");
  lv_obj_align(bright_title, LV_ALIGN_TOP_LEFT, 10, 10);
  lv_obj_set_style_text_color(bright_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

  lv_obj_t* brightness_slider = lv_slider_create(cont);
  lv_obj_set_size(brightness_slider, 210, 16);
  lv_obj_align(brightness_slider, LV_ALIGN_TOP_LEFT, 10, 35);
  lv_slider_set_range(brightness_slider, 10, 255);
  lv_slider_set_value(brightness_slider, brightness_level, LV_ANIM_OFF);

  settings_brightness_value_label = lv_label_create(cont);
  lv_obj_align(settings_brightness_value_label, LV_ALIGN_TOP_RIGHT, -10, 30);
  lv_obj_set_style_text_color(settings_brightness_value_label, lv_color_hex(0x00FF00), LV_PART_MAIN);

  lv_obj_add_event_cb(brightness_slider, [](lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    apply_brightness(value);
    refresh_settings_labels();
  }, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t* tz_title = lv_label_create(cont);
  lv_label_set_text(tz_title, "Timezone");
  lv_obj_align(tz_title, LV_ALIGN_TOP_LEFT, 10, 65);
  lv_obj_set_style_text_color(tz_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

  lv_obj_t* auto_switch = lv_switch_create(cont);
  lv_obj_align(auto_switch, LV_ALIGN_TOP_RIGHT, -10, 62);
  if (timezone_auto_enabled) {
    lv_obj_add_state(auto_switch, LV_STATE_CHECKED);
  }

  settings_timezone_mode_label = lv_label_create(cont);
  lv_obj_align(settings_timezone_mode_label, LV_ALIGN_TOP_LEFT, 10, 90);
  lv_obj_set_style_text_color(settings_timezone_mode_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

  settings_timezone_value_label = lv_label_create(cont);
  lv_label_set_long_mode(settings_timezone_value_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(settings_timezone_value_label, 200);
  lv_obj_align(settings_timezone_value_label, LV_ALIGN_TOP_LEFT, 10, 108);
  lv_obj_set_style_text_color(settings_timezone_value_label, lv_color_hex(0x00FF00), LV_PART_MAIN);

  settings_tz_minus_btn = lv_button_create(cont);
  lv_obj_set_size(settings_tz_minus_btn, 35, 28);
  lv_obj_align(settings_tz_minus_btn, LV_ALIGN_TOP_RIGHT, -60, 105);
  lv_obj_t* minus_label = lv_label_create(settings_tz_minus_btn);
  lv_label_set_text(minus_label, "-");
  lv_obj_center(minus_label);
  lv_obj_add_event_cb(settings_tz_minus_btn, [](lv_event_t* e) {
    if (!timezone_auto_enabled) {
      timezone_offset_hours = constrain(timezone_offset_hours - 1, -12, 14);
      refresh_settings_labels();
    }
  }, LV_EVENT_CLICKED, NULL);

  settings_tz_plus_btn = lv_button_create(cont);
  lv_obj_set_size(settings_tz_plus_btn, 35, 28);
  lv_obj_align(settings_tz_plus_btn, LV_ALIGN_TOP_RIGHT, -10, 105);
  lv_obj_t* plus_label = lv_label_create(settings_tz_plus_btn);
  lv_label_set_text(plus_label, "+");
  lv_obj_center(plus_label);
  lv_obj_add_event_cb(settings_tz_plus_btn, [](lv_event_t* e) {
    if (!timezone_auto_enabled) {
      timezone_offset_hours = constrain(timezone_offset_hours + 1, -12, 14);
      refresh_settings_labels();
    }
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t* dst_label = lv_label_create(cont);
  lv_label_set_text(dst_label, "Auto DST (+1h)");
  lv_obj_align(dst_label, LV_ALIGN_TOP_LEFT, 10, 148);
  lv_obj_set_style_text_color(dst_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

  lv_obj_t* dst_switch = lv_switch_create(cont);
  lv_obj_align(dst_switch, LV_ALIGN_TOP_RIGHT, -10, 145);
  if (auto_dst_enabled) {
    lv_obj_add_state(dst_switch, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(dst_switch, [](lv_event_t* e) {
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    auto_dst_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    refresh_settings_labels();
  }, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_add_event_cb(auto_switch, [](lv_event_t* e) {
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    timezone_auto_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    refresh_settings_labels();
  }, LV_EVENT_VALUE_CHANGED, NULL);

  refresh_settings_labels();
  add_back_button(screen);
  replace_root_screen(screen);
}
void create_about_screen() {
  clear_ui_refs_on_screen_change();

  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "Project Info");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_text_color(title, lv_color_hex(0x00FF00), LV_PART_MAIN);

  lv_obj_t* info_cont = lv_obj_create(screen);
  lv_obj_set_size(info_cont, 300, 160);
  lv_obj_align(info_cont, LV_ALIGN_TOP_MID, 0, 35);
  lv_obj_set_style_bg_color(info_cont, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
  lv_obj_set_style_border_color(info_cont, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_border_width(info_cont, 1, LV_PART_MAIN);
  lv_obj_set_scroll_dir(info_cont, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(info_cont, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_t* info_label = lv_label_create(info_cont);
  lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(info_label, 280);
  lv_obj_set_style_text_color(info_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_label_set_text(
    info_label,
    "Fishing-Gyzmo Project\n"
    "\n"
    "Purpose:\n"
    "- Touchscreen fishing assistant for logs and GPS.\n"
    "\n"
    "Core Features:\n"
    "- Log species, size, and weight.\n"
    "- Save catches to SD card (catch_log.csv).\n"
    "- View and delete saved catch entries.\n"
    "- Live GPS location, speed, altitude, and heading.\n"
    "- GPS-based local time (longitude + optional auto DST).\n"
    "\n"
    "Hardware:\n"
    "- ESP32 + 320x240 ST7789 display.\n"
    "- CST820 touch controller.\n"
    "- NEO-6M GPS module.\n"
    "- SD card storage.\n"
    "\n"
    "Notes:\n"
    "- Built with LVGL + LovyanGFX.\n"
    "- Use the Back button to return to menu."
  );
  lv_obj_align(info_label, LV_ALIGN_TOP_LEFT, 0, 0);

  add_back_button(screen);
  replace_root_screen(screen);
}
void create_map_screen() {
  clear_ui_refs_on_screen_change();
  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_t* label = lv_label_create(screen);
  lv_label_set_text(label, "Map Screen");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);
  add_back_button(screen);
  replace_root_screen(screen);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
}

// ===== Menu Button Callback =====
void on_menu_button(lv_event_t* e) {
  lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
  lv_obj_t* label_obj = lv_obj_get_child(target, 0);
  const char* btn_text = lv_label_get_text(label_obj);

  if (strcmp(btn_text, "Condition Test") == 0) create_condition_screen();
  else if (strcmp(btn_text, "Log Catch") == 0) create_log_screen();
  else if (strcmp(btn_text, "Settings") == 0) create_settings_screen();
  else if (strcmp(btn_text, "About") == 0) create_about_screen();
  else if (strcmp(btn_text, "Map") == 0) create_map_screen();
  else if (strcmp(btn_text, "View Entries") == 0) create_view_entries_screen();
  else if (strcmp(btn_text, "GPS Info") == 0) create_gps_screen();
}

// ===== Main Menu =====
void create_main_menu() {
  clear_ui_refs_on_screen_change();

  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x222222), LV_PART_MAIN);

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "Fishing-Gyzmo");
  lv_obj_set_style_text_color(title, lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &my_font_28, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_opa(title, LV_OPA_TRANSP, LV_PART_MAIN);
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, title);
    lv_anim_set_exec_cb(&a, menu_btn_fade_anim_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 220);
    lv_anim_set_delay(&a, 40);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
  }

  lv_obj_t* menu_scroll = lv_obj_create(screen);
  lv_obj_set_size(menu_scroll, 300, 186);
  lv_obj_align(menu_scroll, LV_ALIGN_TOP_MID, 0, 46);
  lv_obj_set_style_bg_opa(menu_scroll, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(menu_scroll, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_left(menu_scroll, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_right(menu_scroll, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_top(menu_scroll, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(menu_scroll, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_row(menu_scroll, 14, LV_PART_MAIN);
  lv_obj_set_scroll_dir(menu_scroll, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(menu_scroll, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_add_flag(menu_scroll, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_set_scroll_snap_y(menu_scroll, LV_SCROLL_SNAP_NONE);
  lv_obj_set_layout(menu_scroll, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(menu_scroll, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(menu_scroll, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  const char* menu_buttons[] = { "Condition Test", "Log Catch", "Map", "GPS Info", "Settings", "About", "View Entries"};

  for (int i = 0; i < 7; i++) {
    lv_obj_t* btn = lv_button_create(menu_scroll);
    lv_obj_set_size(btn, 220, 42);

    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x00AA00), LV_PART_MAIN);
    lv_obj_set_style_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, menu_buttons[i]);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);

    lv_obj_add_event_cb(btn, on_menu_button, LV_EVENT_CLICKED, NULL);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, btn);
    lv_anim_set_exec_cb(&a, menu_btn_fade_anim_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 200);
    lv_anim_set_delay(&a, (uint32_t)(80 + i * 55));
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
  }

  replace_root_screen(screen, true);
}

// ===== Time Overlay (always on top) =====
void create_time_overlay() {
  lv_obj_t* overlay = lv_layer_top();
  global_time_label = lv_label_create(overlay);
  lv_label_set_text(global_time_label, "--:-- --");
  lv_obj_align(global_time_label, LV_ALIGN_TOP_RIGHT, -5, 5);
  lv_obj_set_style_text_color(global_time_label, lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_bg_color(global_time_label, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(global_time_label, LV_OPA_50, LV_PART_MAIN);
  lv_obj_set_style_pad_all(global_time_label, 2, LV_PART_MAIN);
}

// ===== Setup =====
void setup() {
#if defined(ARDUINO_ARCH_ESP32)
  Serial.setRxBufferSize(2048);
#endif
  Serial.begin(9600);   // NEO-6M GPS default baud
  Serial.println("Fishing Gyzmo Starting...");
  Serial.println("Initializing GPS on RX0/TX0...");

  delay(1500);

  // Mount external SD on dedicated HSPI bus
  sd_spi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS, sd_spi, 14000000)) {
      Serial.println("❌ SD mount failed");
  } else {
      Serial.println("✅ SD mounted successfully");
      sd_mounted = true;
  }

  pinMode(PIN_BACKLIGHT, OUTPUT);
  apply_brightness(brightness_level);

  tft.init();
  tft.setRotation(1);

  touch.begin();
  lv_init();

  static lv_color_t buf1[240 * 60];
  disp = lv_display_create(320, 240);
  lv_display_set_flush_cb(disp, lv_flush_cb);
  lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchpad_read_cb);

  create_time_overlay(); // Create the persistent time label on lv_layer_top()
  create_main_menu();
}

// ===== Loop =====
void loop() {
  lv_tick_inc(LVGL_TICK_PERIOD);
  lv_timer_handler();

  if (log_request) {
    log_request = false;
    log_catch_safe(pending_species, pending_size, pending_weight);
  }

  // Drain all pending UART bytes each loop (NMEA must not be dropped). yield() periodically for WDT.
  int serial_reads = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (!gps_stream_detected) {
      gps_stream_detected = true;
      Serial.println("GPS module detected. Receiving NMEA data...");
    }

    gps.encode(c);
    if ((++serial_reads & 0x3F) == 0) {
#if defined(ARDUINO_ARCH_ESP32)
      yield();
#endif
    }
  }

  // ===== Update Top-Right Time Overlay =====
  if (gps.time.isValid() && global_time_label != nullptr) {
    static uint8_t last_second = 255;
    if (gps.time.second() != last_second) {
      last_second = gps.time.second();

      int local_hour = normalized_local_hour(gps.time.hour());
      int hour12 = local_hour % 12;
      if (hour12 == 0) hour12 = 12;
      const char* ampm = (local_hour >= 12) ? "PM" : "AM";

      char timebuf[12];
      sprintf(timebuf, "%d:%02d %s", hour12, gps.time.minute(), ampm);
      lv_label_set_text(global_time_label, timebuf);
    }
  }

  const bool gps_loc_new = gps.location.isUpdated();
  if (gps_loc_new) {
    if (timezone_auto_enabled) {
      int computed_tz = compute_timezone_offset_from_longitude(gps.location.lng());
      if (computed_tz != timezone_offset_hours) {
        timezone_offset_hours = computed_tz;
        Serial.print("🌍 Auto timezone updated: UTC");
        if (timezone_offset_hours >= 0) Serial.print("+");
        Serial.println(timezone_offset_hours);
        refresh_settings_labels();
      }
    }
  }

  if (gps_info_label != nullptr) {
    if (gps.location.isValid()) {
      if (gps_loc_new || s_force_gps_label_paint) {
        if (s_force_gps_label_paint) s_force_gps_label_paint = false;
        paint_gps_summary_label();
      }
    } else {
      // No lat/lon fix yet — still refresh page (time/sats/HDOP) so it matches the corner clock.
      static uint32_t last_no_fix_paint_ms = 0;
      uint32_t now = millis();
      if (s_force_gps_label_paint || now - last_no_fix_paint_ms >= 250u) {
        last_no_fix_paint_ms = now;
        if (s_force_gps_label_paint) s_force_gps_label_paint = false;
        paint_gps_no_fix_label();
      }
    }
  }

  if (gps.location.isValid() && !gps_fix_announced) {
    gps_fix_announced = true;
    Serial.println("GPS satellite fix acquired.");
  }

#if defined(ARDUINO_ARCH_ESP32)
  yield();
#endif
  delay(1);
}
