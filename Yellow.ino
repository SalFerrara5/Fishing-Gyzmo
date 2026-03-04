#include "User_Setup_JC2432W328C.h"
#include <LovyanGFX.hpp>
#include "CST820.h"
#include <lvgl.h>
#include "my_font_28.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

bool log_request = false;

String pending_species;
String pending_size;
String pending_weight;

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
      cfg.pin_sclk   = 14;
      cfg.pin_mosi   = 13;
      cfg.pin_miso   = -1;
      cfg.pin_dc     = 2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    { auto cfg = _panel.config();
      cfg.pin_cs           = 15; // Display CS
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
CST820 touch(33, 32, 25, 21);

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

  // Reinit SPI for SD (14MHz)
  SPI.begin(18, 19, 23, 5);
  if (!SD.begin(5, SPI, 14000000)) {
    Serial.println("❌ SD.begin failed");
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

  // Restore screen SPI
  tft.init();
  tft.setRotation(1);
}

// ===== Delete Entry (with SD reinit) =====
void delete_entry(const String& line) {
  if (!sd_mounted) return;

  SPI.begin(18, 19, 23, 5);
  if (!SD.begin(5, SPI, 14000000)) {
    Serial.println("❌ SD.begin failed for delete");
    return;
  }

  File file = SD.open("/catch_log.csv");

  File temp = SD.open("/temp.csv", FILE_WRITE);
  String l;
  while (file.available()) {
    l = file.readStringUntil('\n');
    if (l != line) temp.println(l);
  }

  file.close();
  temp.close();

  SD.remove("/catch_log.csv");
  SD.rename("/temp.csv", "/catch_log.csv");

  // Restore screen
  tft.init();
  tft.setRotation(1);
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

void create_log_screen() {
  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* title_label = lv_label_create(screen);
  lv_label_set_text(title_label, "Log Catch");
  lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

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

  kb_userdata_t* kb_data = new kb_userdata_t{ kb, submit_btn, nullptr, back_btn };
  lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY, kb_data);
  lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, kb_data);

  lv_obj_add_event_cb(species_input, textarea_focused_cb, LV_EVENT_FOCUSED, kb_data);
  lv_obj_add_event_cb(size_input, textarea_focused_cb, LV_EVENT_FOCUSED, kb_data);
  lv_obj_add_event_cb(weight_input, textarea_focused_cb, LV_EVENT_FOCUSED, kb_data);

  lv_screen_load(screen);
}

// ===== View Entries Screen =====
void create_view_entries_screen() {
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
    lv_screen_load(screen);
    return;
  }

  File file = SD.open("/catch_log.csv");
  if (!file) {
    lv_label_set_text(lv_label_create(screen), "No entries");
    lv_screen_load(screen);
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

    String* line_copy = new String(line);
    lv_obj_add_event_cb(del_btn, [](lv_event_t* e){
      String* txt = (String*)lv_event_get_user_data(e);
      delete_entry(*txt);
      delete txt;
      create_view_entries_screen();
    }, LV_EVENT_CLICKED, line_copy);

    y += 35;
  }
  file.close();

  add_back_button(screen);
  lv_screen_load(screen);
}

// ===== Other Screens =====
void create_condition_screen() { lv_obj_t* screen = lv_obj_create(NULL); lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN); lv_obj_t* label = lv_label_create(screen); lv_label_set_text(label, "Condition Test Screen"); lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20); add_back_button(screen); lv_screen_load(screen); }
void create_settings_screen() { lv_obj_t* screen = lv_obj_create(NULL); lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN); lv_obj_t* label = lv_label_create(screen); lv_label_set_text(label, "Settings Screen"); lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20); add_back_button(screen); lv_screen_load(screen); }
void create_about_screen() { lv_obj_t* screen = lv_obj_create(NULL); lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN); lv_obj_t* label = lv_label_create(screen); lv_label_set_text(label, "About Screen"); lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20); add_back_button(screen); lv_screen_load(screen); }
void create_map_screen() { lv_obj_t* screen = lv_obj_create(NULL); lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN); lv_obj_t* label = lv_label_create(screen); lv_label_set_text(label, "Map Screen"); lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20); add_back_button(screen); lv_screen_load(screen); }

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
}

// ===== Main Menu =====
void create_main_menu() {
  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x222222), LV_PART_MAIN);

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "Fishing-Gyzmo");
  lv_obj_set_style_text_color(title, lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &my_font_28, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  const char* menu_buttons[] = { "Condition Test", "Log Catch", "Map", "Settings", "About", "View Entries" };
  lv_obj_t* prev_btn = nullptr;
  int start_y = 60;
  int spacing = 20;

  for (int i = 0; i < 6; i++) {
    lv_obj_t* btn = lv_button_create(screen);
    lv_obj_set_size(btn, 200, 40);

    if (!prev_btn)
      lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, start_y);
    else
      lv_obj_align_to(btn, prev_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, spacing);

    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x00AA00), LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, menu_buttons[i]);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);

    lv_obj_add_event_cb(btn, on_menu_button, LV_EVENT_CLICKED, NULL);
    prev_btn = btn;
  }

  lv_screen_load(screen);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(1500);

  // Mount SD card with explicit CS pin
  SPI.begin(18, 19, 23, 5);
  if (!SD.begin(5, SPI, 14000000)) {
      Serial.println("❌ SD mount failed");
  } else {
      Serial.println("✅ SD mounted successfully");
      sd_mounted = true;
  }

  pinMode(27, OUTPUT);
  analogWrite(27, 255);

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

  delay(1);
}
