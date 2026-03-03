# Fishing-Gyzmo
Fishing Gyzmo is a touchscreen-based fishing companion device built on an ESP32 using LVGL and a 2 inch ST7789 display. It provides an interactive menu system designed for logging catches, running condition checks, and managing fishing data in a compact, standalone unit.

The goal is to create a dedicated, purpose-built tool instead of relying on a phone app. It boots straight into a clean interface and is designed to feel like a finished product, not a dev board.

Hardware

ESP32 (tested on ESP32 with VSPI)

240x320 ST7789 display

CST820 capacitive touch controller

Backlight connected to GPIO 27 (PWM controlled)

Pin Configuration (current build)

Display:

SCLK: GPIO 14

MOSI: GPIO 13

DC: GPIO 2

CS: GPIO 15

Backlight: GPIO 27

Touch:

SDA: GPIO 33

SCL: GPIO 32

RST: GPIO 25

INT: GPIO 21

If you use different pins, edit the SPI and touch configuration in the display class and CST820 constructor.

Software Stack

LVGL v9

LovyanGFX

CST820 touch driver

Arduino framework

Important: Custom fonts must be generated for LVGL v9 or manually adjusted for v9 compatibility.

Current Features

Dark themed UI

Custom title font

Centered title: “Fishing Gyzmo”

Touchscreen button menu

Adjustable backlight slider

Clean left-aligned layout

Serial debugging output

Planned feature modules:

Condition Test

Log Catch

Settings

Catch History

Possibly weather integration (WiFi based)

UI Structure

The interface is built using LVGL objects:

Screen root object

Title label

Button objects stacked vertically

Event callbacks for touch interaction

To add a new menu button:

Create a button using lv_button_create()

Align it below the previous object

Add an event callback with lv_obj_add_event_cb()

Attach a label with lv_label_create()

Example:

lv_obj_t* btn = lv_button_create(lv_screen_active());
lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 10, 80);
lv_obj_add_event_cb(btn, my_callback, LV_EVENT_CLICKED, NULL);
Customizing the UI
Change Background Color
lv_obj_set_style_bg_color(lv_screen_active(),
                          lv_color_hex(0x222222),
                          LV_PART_MAIN);

Replace hex value as needed.

Change Button Color
lv_obj_set_style_bg_color(btn,
                          lv_color_hex(0x00AA00),
                          LV_PART_MAIN);

You can also modify pressed state styling using LV_PART_MAIN | LV_STATE_PRESSED.

Change Fonts

Generate a font using the LVGL font converter.

Include the .h file in the project.

Assign the font:

lv_obj_set_style_text_font(title, &my_font_28, LV_PART_MAIN);

Important:

Make sure space character is included if limiting symbols.

If compilation fails, remove .static_bitmap from the font struct for LVGL v9.

Improve Performance

If UI feels sluggish:

Increase SPI write frequency in LovyanGFX config.

Increase LVGL draw buffer size:

static lv_color_t buf1[240 * 20];

Reduce unnecessary Serial printing.

Avoid heavy operations inside event callbacks.

Project Philosophy

This is a dedicated tool, not a phone replacement. It should:

Boot fast

Respond instantly

Be readable in sunlight

Feel like purpose-built hardware

The UI is intentionally simple and structured. Clean layout, large tap targets, no clutter.

Future Expansion Ideas

EEPROM or Preferences-based data storage

SD card logging

GPS module integration

Barometric pressure tracking

Moon phase calculations

WiFi time sync

Data export over USB

How to Extend the Code

Each feature should live in its own function:

create_main_menu()

create_log_screen()

create_settings_screen()

Switch screens by cleaning the active screen:

lv_obj_clean(lv_screen_active());
create_log_screen();

This keeps things modular and easier to scale.
