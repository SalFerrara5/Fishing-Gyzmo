#include <LovyanGFX.hpp>
#include "CST820.h"
#include <lvgl.h>
#include "my_font_28.h"

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
      cfg.pin_cs           = 15;
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

#define LVGL_TICK_PERIOD 5

// ===== Forward Declarations =====
void create_main_menu();
void create_condition_screen();
void create_log_screen();
void create_settings_screen();
void create_about_screen();
void create_map_screen();

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
void add_back_button(lv_obj_t* parent) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_size(btn, 120, 40);

  // Green button style
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(btn, lv_color_hex(0x00AA00), LV_PART_MAIN);

  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, "Back");
  lv_obj_center(label);
  lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);

  lv_obj_add_event_cb(btn, back_to_menu, LV_EVENT_CLICKED, NULL);
}

// ===== Screens =====
void create_condition_screen() {
  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* label = lv_label_create(screen);
  lv_label_set_text(label, "Condition Test Screen");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

  add_back_button(screen);
  lv_screen_load(screen);
}

void create_log_screen() {
  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* label = lv_label_create(screen);
  lv_label_set_text(label, "Log Catch Screen");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

  add_back_button(screen);
  lv_screen_load(screen);
}

void create_settings_screen() {
  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* label = lv_label_create(screen);
  lv_label_set_text(label, "Settings Screen");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

  add_back_button(screen);
  lv_screen_load(screen);
}

void create_about_screen() {
  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* label = lv_label_create(screen);
  lv_label_set_text(label, "About Screen");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

  add_back_button(screen);
  lv_screen_load(screen);
}

// ===== Map Screen =====
void create_map_screen() {
  lv_obj_t* screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), LV_PART_MAIN);

  lv_obj_t* label = lv_label_create(screen);
  lv_label_set_text(label, "Map Screen");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

  add_back_button(screen);
  lv_screen_load(screen);
}

// ===== Menu Button Callback =====
void on_menu_button(lv_event_t* e) {
  lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
  lv_obj_t* label_obj = lv_obj_get_child(target, 0);
  const char* btn_text = lv_label_get_text(label_obj);

  if (strcmp(btn_text, "Condition Test") == 0)
    create_condition_screen();
  else if (strcmp(btn_text, "Log Catch") == 0)
    create_log_screen();
  else if (strcmp(btn_text, "Settings") == 0)
    create_settings_screen();
  else if (strcmp(btn_text, "About") == 0)
    create_about_screen();
  else if (strcmp(btn_text, "Map") == 0)
    create_map_screen();
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

  const char* menu_buttons[] = {
    "Condition Test",
    "Log Catch",
    "Map",
    "Settings",
    "About"
       
  };

  lv_obj_t* prev_btn = nullptr;
  int start_y = 60;   // vertical offset from top
  int spacing = 20;   // space between buttons

  for (int i = 0; i < 5; i++) {
      lv_obj_t* btn = lv_button_create(screen);
      lv_obj_set_size(btn, 200, 40);  // uniform size

      if (!prev_btn)
          lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, start_y); // first button
      else
          lv_obj_align_to(btn, prev_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, spacing); // below previous, centered

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

  pinMode(27, OUTPUT);
  analogWrite(27, 255);

  tft.init();
  tft.setRotation(1);

  touch.begin();
  lv_init();

  static lv_color_t buf1[240 * 60];
  static lv_display_t* disp = lv_display_create(320, 240);
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
  delay(1);
}
