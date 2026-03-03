# 🎣 Fishing Gyzmo

Fishing Gyzmo is a touchscreen-based fishing companion built on an ESP32 using LVGL and a 2" ST7789 display. It provides a clean, responsive interface for logging catches, testing conditions, and managing fishing data in a standalone device.

The goal is simple: a dedicated tool that boots instantly and feels like real hardware, not a phone app.

---

## 📦 Features

* Dark themed LVGL v9 interface
* Custom title font support
* Touchscreen button menu
* Adjustable display backlight
* Modular screen system
* Serial debugging output
* Designed for future expansion (WiFi, logging, sensors)

Planned modules:

* Condition Test
* Log Catch
* Settings
* Catch History
* Weather integration

---

## 🛠 Hardware

* ESP32 (VSPI used)
* 240x320 ST7789 display
* CST820 capacitive touch controller
* PWM-controlled backlight

### Pin Configuration (Current Build)

**Display**

* SCLK → GPIO 14
* MOSI → GPIO 13
* DC → GPIO 2
* CS → GPIO 15
* Backlight → GPIO 27

**Touch**

* SDA → GPIO 33
* SCL → GPIO 32
* RST → GPIO 25
* INT → GPIO 21

If your wiring differs, update the SPI configuration and CST820 constructor in the sketch.

---

## 🧰 Software Stack

* LVGL v9
* LovyanGFX
* CST820 driver
* Arduino framework

Important: Custom fonts must be generated for LVGL v9. Fonts generated for earlier versions will not compile without modification.

---

## 🖥 UI Structure

The interface is built using LVGL objects:

* Root screen object
* Title label
* Vertically stacked buttons
* Event callbacks for interaction

To add a new button:

```cpp
lv_obj_t* btn = lv_button_create(lv_screen_active());
lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 10, 80);
lv_obj_add_event_cb(btn, my_callback, LV_EVENT_CLICKED, NULL);

lv_obj_t* label = lv_label_create(btn);
lv_label_set_text(label, "New Button");
lv_obj_center(label);
```

---

## 🎨 Customization

### Change Background Color

```cpp
lv_obj_set_style_bg_color(lv_screen_active(),
                          lv_color_hex(0x222222),
                          LV_PART_MAIN);
```

---

### Change Button Color

```cpp
lv_obj_set_style_bg_color(btn,
                          lv_color_hex(0x00AA00),
                          LV_PART_MAIN);
```

You can also style pressed state:

```cpp
lv_obj_set_style_bg_color(btn,
                          lv_color_hex(0x007700),
                          LV_PART_MAIN | LV_STATE_PRESSED);
```

---

### Using Custom Fonts

1. Generate a font using the LVGL font converter
2. Ensure a **space character is included** if limiting symbols
3. Place the generated `.h` file in the sketch folder
4. Include it:

```cpp
#include "my_font_28.h"
```

5. Apply it to a label:

```cpp
lv_obj_set_style_text_font(title, &my_font_28, LV_PART_MAIN);
```

If compilation fails, remove `.static_bitmap` from the font struct and ensure the struct matches LVGL v9 format.

---

## ⚡ Performance Tips

If the UI feels slow:

* Increase SPI write frequency in LovyanGFX config
* Increase LVGL buffer size:

```cpp
static lv_color_t buf1[240 * 20];
```

* Reduce unnecessary Serial printing
* Avoid heavy logic inside event callbacks

---

## 🔧 Screen Navigation Structure

Each feature should live in its own function:

```cpp
void create_main_menu();
void create_log_screen();
void create_settings_screen();
```

To switch screens:

```cpp
lv_obj_clean(lv_screen_active());
create_log_screen();
```

This keeps the code modular and easier to expand.

---

## 🚀 Future Expansion Ideas

* EEPROM / Preferences storage
* SD card logging
* GPS integration
* Barometric pressure sensor
* Moon phase calculations
* WiFi time sync
* USB data export

---

## 📜 Project Philosophy

Fishing Gyzmo is meant to be:

* Fast
* Simple
* Readable outdoors
* Purpose-built

No clutter. No unnecessary animations. Just a clean interface that works every time.

---
