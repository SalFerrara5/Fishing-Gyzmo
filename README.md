# Fishing Gyzmo 🎣

## Overview

Fishing Gyzmo is a handheld embedded system built on an ESP32 platform designed to assist anglers in the field. It combines a touchscreen interface, GPS tracking, and onboard data logging to create a simple, reliable tool for recording catches and viewing real-time location data.

This project focuses on keeping things practical: quick inputs, clear data, and hardware that works outdoors without needing a phone or internet connection.

---

## Features

### 📍 GPS Tracking

* Real-time latitude and longitude display
* Altitude (converted to feet)
* Speed (mph) and heading (degrees)
* Satellite count and signal accuracy (HDOP)
* Date and local time (EST offset applied)

### 📝 Catch Logging

* Log fish species, size, and weight
* Entries stored locally on SD card (`/catch_log.csv`)
* Timestamped using system uptime
* Designed for quick entry in the field

### 📂 Log Management

* View all logged catches directly on the device
* Delete individual entries
* Scrollable interface for large logs

### 🖥️ Touchscreen UI

* Built with LVGL for responsive graphics
* On-screen keyboard for data entry
* Clean menu navigation system
* Multiple screens:

  * Main Menu
  * Log Catch
  * View Entries
  * GPS Info
  * Map (placeholder)
  * Settings (placeholder)
  * About (placeholder)
  * Condition Test

---

## Hardware

* ESP32-based board (JC2432W328C configuration)
* 320x240 ST7789 display (SPI)
* CST820 capacitive touchscreen
* NEO-6M GPS module (UART)
* SD card module (SPI)
* Backlight control via PWM (GPIO 27)

---

## Software & Libraries

* LVGL (GUI framework)
* LovyanGFX (display driver)
* TinyGPS++ (GPS parsing)
* SD / SPI / FS (file system handling)

---

## How It Works

### System Flow

1. Device boots and initializes:

   * Display
   * Touch input
   * SD card
   * GPS serial stream

2. Main menu is displayed

3. User selects an action:

   * Log a catch → data stored to SD card
   * View entries → reads from CSV file
   * GPS info → live data from satellite feed

4. Background processes:

   * GPS continuously parsed from serial input
   * UI updated in real time
   * Logging handled safely with SPI reinitialization

---

## Data Format

Catch logs are stored as CSV:

```
species,size,weight,timestamp
```

Example:

```
Bass,45,2.1,12345678
```

---

## Notable Implementation Details

* **SPI Bus Sharing**

  * Display and SD card share SPI
  * System safely reinitializes SPI before SD operations
  * Prevents common ESP32 display corruption issues

* **Touch Input Mapping**

  * Coordinates rotated and flipped to match screen orientation

* **Keyboard Handling**

  * UI dynamically shifts when keyboard appears
  * Prevents input fields from being hidden

* **GPS Handling**

  * Detects incoming NMEA stream automatically
  * Announces when satellite fix is acquired
  * Converts UTC → EST manually

---

## Current Limitations

* Map screen is a placeholder (no rendering yet)
* Settings and About screens not implemented
* No persistent timestamp (uses millis instead of RTC/GPS time)
* Timezone is hardcoded (EST only, no DST handling)
* No filtering or sorting of log entries

---

## Future Improvements

* Add offline map tiles and GPS plotting
* Replace millis timestamp with GPS-based time
* Implement waypoint saving
* Add weather/environment data integration
* Improve UI styling and responsiveness
* Export logs via USB or wireless
* Add species presets to speed up logging

---

## Getting Started

1. Flash code to ESP32
2. Insert formatted SD card
3. Connect GPS module to UART (default baud: 9600)
4. Power on device
5. Wait for GPS signal lock
6. Start logging catches

---

## Philosophy

This project sticks to a simple idea: tools in the field should just work. No subscriptions, no signal required, no nonsense. Turn it on, catch fish, log it, move on.

---

## Author Notes

Built as part of an embedded systems project with a focus on real-world usability. The goal isn’t to overcomplicate things, it’s to make something you’d actually bring fishing without thinking twice.
