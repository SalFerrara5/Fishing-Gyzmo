#include <LovyanGFX.hpp>
#include "CST820.h"
#include <lvgl.h>
#include "my_font_28.h" //custom font file

// ===== Display Setup =====
class LGFX_JustDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI _bus;

public:
  LGFX_JustDisplay(void) {
    { auto cfg = _bus.config();
      cfg.spi_host   = VSPI_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 27000000;
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
unsigned long lastLvTick = 0;

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

// ===== Menu callback =====
void on_menu_button(lv_event_t* e) {
  lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
  lv_obj_t* label_obj = lv_obj_get_child(target, 0);
  const char* btn_text = lv_label_get_text(label_obj);
  Serial.printf("✅ Menu Button '%s' clicked!\n", btn_text);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("🧪 LVGL Menu Demo");

  pinMode(27, OUTPUT);
  analogWrite(27, 255);

  tft.init();
  tft.setRotation(1);

  touch.begin();

  lv_init();

  // ===== Bigger LVGL buffer for higher FPS =====
  static lv_color_t buf1[240 * 40];  // 40 lines instead of 10
  static lv_display_t* disp = lv_display_create(320, 240);
  lv_display_set_flush_cb(disp, lv_flush_cb);
  lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Touch input
  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchpad_read_cb);

  // Background
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x222222), LV_PART_MAIN);

  // ===== Title =====
  lv_obj_t* title = lv_label_create(lv_screen_active());
  lv_label_set_text(title, "Fishing-Gyzmo");
  lv_obj_set_style_text_color(title, lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &my_font_28, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  // ===== Menu Buttons =====
  const char* menu_buttons[] = { "Condition Test", "Log Catch", "Settings", "About" };
  lv_obj_t* prev_btn = nullptr;
  for (int i = 0; i < 4; i++) {
    lv_obj_t* btn = lv_button_create(lv_screen_active());
    if (!prev_btn) lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 10, 60);
    else lv_obj_align_to(btn, prev_btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);

    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x00AA00), LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, menu_buttons[i]);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(btn, on_menu_button, LV_EVENT_CLICKED, NULL);
    prev_btn = btn;
  }
}

// ===== Loop =====
void loop() {
  lv_tick_inc(LVGL_TICK_PERIOD);
  lv_timer_handler();
  delay(1); // slightly shorter delay to help responsiveness
}