# Fishing Gyzmo 🎣

## Overview

Fishing Gyzmo is a purpose-built handheld fishing assistant running on an ESP32. It combines GPS tracking, touchscreen input, and local data logging into a self-contained device that works anywhere, no signal required.

The design leans toward reliability over flash. Everything runs locally, boots fast, and does exactly what you need on the water without distractions.

---

## Core Capabilities

### 📍 GPS System

The device uses a NEO-6M GPS module with continuous NMEA parsing via UART.

**Live Data Displayed:**

* Latitude / Longitude (6 decimal precision)
* Altitude (converted from meters → feet)
* Speed (mph)
* Heading (degrees)
* Satellite count
* HDOP (accuracy indicator)
* Date (GPS-derived)
* Time (converted from UTC → EST manually)

**Behavior Notes:**

* Automatically detects incoming GPS stream
* Announces when a valid satellite fix is acquired
* Continuously updates UI when new data arrives
* Handles time conversion locally (no RTC dependency)

---

### 📝 Catch Logging System

The logging system is built around simplicity and durability.

**Inputs:**

* Species (text)
* Size (cm)
* Weight (kg)

**Storage:**

* File: `/catch_log.csv`
* Format: comma-separated values
* Each entry appended safely

**Write Flow:**

1. User enters data via touchscreen keyboard
2. Data stored temporarily in memory
3. `log_request` flag triggers write in main loop
4. SPI bus switches from display → SD card
5. Data appended to file
6. SPI restored to display

This avoids crashes and display corruption, which is a classic ESP32 problem when sharing SPI.

---

### 📂 Log Viewer & Management

Users can browse all saved entries directly on the device.

**Features:**

* Scrollable list of entries
* Each entry displayed as raw CSV line
* Individual delete button per entry

**Delete Process:**

* Reads original file
* Writes all non-matching entries to temp file
* Replaces original file
* Reinitializes display afterward

Not fancy, but solid and predictable.

---

### 🖥️ User Interface (LVGL)

The UI is built using LVGL with a custom layout for a 320x240 display.

**Design Goals:**

* High contrast (dark background, bright green accents)
* Large touch targets
* Minimal layers or clutter
* Fast transitions between screens

**Main Menu Options:**

* Condition Test
* Log Catch
* Map (placeholder)
* GPS Info
* Settings (placeholder)
* About (placeholder)
* View Entries

---

### ⌨️ On-Screen Keyboard System

The keyboard is dynamically shown when a text field is selected.

**Key Behaviors:**

* Automatically attaches to active textarea
* Hides other buttons while active
* Shifts container upward to prevent overlap
* Restores layout when dismissed

This avoids the classic “keyboard covers input field” issue.

---

## Hardware Architecture

### Microcontroller

* ESP32 (dual-core)
* Handles UI, GPS parsing, SD logging, and touch input

### Display

* ST7789 (320x240)
* Driven via SPI using LovyanGFX
* Optimized for high refresh rates (80 MHz write)

### Touch Controller

* CST820 capacitive touch
* Custom coordinate remapping:

  * X/Y flipped
  * Adjusted for rotation

### GPS Module

* NEO-6M
* Connected via hardware serial (9600 baud default)

### Storage

* MicroSD card via SPI
* Dedicated chip select (GPIO 5)

### Power + Backlight

* Backlight controlled via PWM (GPIO 27)
* Set to full brightness on boot

---

## Pin Configuration (Key Lines)

| Function      | GPIO     |
| ------------- | -------- |
| TFT SCLK      | 14       |
| TFT MOSI      | 13       |
| TFT DC        | 2        |
| TFT CS        | 15       |
| SD CS         | 5        |
| SD SPI        | 18/19/23 |
| Touch INT/RST | 33/32    |
| Backlight     | 27       |

---

## Software Architecture

### Main Loop Responsibilities

The loop is intentionally simple and predictable:

1. Update LVGL tick and UI handler
2. Process pending log requests
3. Read GPS serial data continuously
4. Update GPS display if new data available
5. Announce GPS fix once
6. Small delay for stability

---

### Display Pipeline

* LVGL renders into buffer
* `lv_flush_cb` pushes pixels via LovyanGFX
* Partial rendering used to reduce memory usage

---

### Touch Input Handling

* Polls CST820 controller
* Converts raw coordinates to screen space
* Feeds into LVGL input system

---

### SD Card Handling Strategy

SPI is shared between:

* Display
* SD card

To prevent conflicts:

* SPI is reinitialized before every SD operation
* Display is reinitialized after

This is not elegant, but it’s reliable. On ESP32, that matters more.

---

## File Structure

```
/catch_log.csv
/temp.csv (used during delete operations)
```

---

## Data Format

```csv
species,size,weight,timestamp
```

Example:

```csv
Trout,38,1.4,52344123
Bass,45,2.1,52344987
```

**Note:**

* Timestamp = `millis()` (time since boot)
* Not absolute time (yet)

---

## Known Limitations

* No real map rendering yet
* No persistent clock (relies on uptime)
* Timezone hardcoded (EST only)
* No daylight savings adjustment
* CSV not parsed into structured UI (raw display only)
* No error recovery if SD removed mid-operation
* Memory not optimized for very large log files

---

## Future Roadmap

### High Priority

* Replace millis timestamp with GPS time
* Implement waypoint saving
* Add basic map tile rendering (offline)

### Medium Priority

* Species quick-select buttons
* Log filtering (by species/date)
* UI polish (fonts, spacing, icons)

### Long Term

* Bluetooth or WiFi export
* Weather data integration
* Fish pattern tracking
* Depth/temp sensor integration

---

## Design Philosophy

This project sticks to a simple rule:
If it doesn’t help you catch fish or remember what worked, it doesn’t belong.

No cloud. No accounts. No nonsense.

Just:

* Turn it on
* Log your catch
* Check your location
* Keep moving

---

## Build & Setup

1. Flash firmware to ESP32
2. Insert FAT32-formatted SD card
3. Wire GPS module to UART (9600 baud)
4. Power device
5. Wait for GPS fix
6. Use touchscreen to navigate

---

## Debug Output (Serial)

Helpful messages include:

* SD mount success/failure
* GPS stream detection
* GPS fix acquisition
* Logging success/errors

---

## Final Notes

This is the kind of tool you toss in your tackle box and forget about until you need it. It’s not trying to compete with your phone. It’s trying to replace the stuff your phone does poorly when you’re actually out fishing.

And honestly, that’s where it shines.
