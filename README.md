# Fishing-Gyzmo

## Overview

Fishing-Gyzmo is a standalone, ESP32-based touchscreen fishing assistant designed to log catches and provide real-time GPS data without relying on a phone or external apps.

It combines a graphical interface, SD card storage, and GPS tracking into a compact system intended for practical, real-world use while fishing.

The system is built using LVGL for the UI, TinyGPS++ for GPS parsing, and LovyanGFX for display control.

---

## Core Features

### 1. Catch Logging System

* Input fields:

  * Species
  * Size (cm)
  * Weight (kg)
* Touchscreen keyboard for data entry
* One-button submission workflow
* Logs stored in CSV format on SD card

Each entry includes:

* Species
* Size
* Weight
* Timestamp (based on system uptime via `millis()`)

---

### 2. Catch Log Viewer

* Displays all saved entries from `catch_log.csv`
* Scrollable list interface
* Each entry includes:

  * Raw CSV line
* Delete button per entry:

  * Removes entry by rewriting file
  * Uses temporary file swap method (`temp.csv`)

---

### 3. GPS System

#### Live Data Display

* Latitude / Longitude
* Altitude (converted to feet)
* Speed (mph)
* Heading (degrees)
* Satellite count
* HDOP (signal accuracy)
* Date and time

#### Status Detection

* Detects:

  * No serial data
  * GPS data stream active
  * No satellite fix
  * Valid position fix

#### Behavior

* Continuously reads UART buffer
* Uses non-blocking parsing
* Periodically yields to avoid watchdog resets

---

### 4. Time System

#### GPS-Based Time

* Uses UTC time from GPS module

#### Timezone Handling

* Automatic mode:

  * Calculates timezone using longitude (`longitude / 15`)
* Manual mode:

  * User-adjustable offset (-12 to +14)

#### DST (Daylight Saving Time)

* Optional auto DST toggle
* Approximation:

  * Enabled for:

    * March (after ~8th)
    * April through October
* Disabled automatically in southern hemisphere

#### Display

* Global time overlay (top-right corner)
* Updates every second
* Also shown in GPS screen

---

### 5. Settings System

#### Brightness Control

* Slider (10–255)
* Uses PWM output via `analogWrite`
* Real-time adjustment

#### Time Settings

* Auto timezone toggle (GPS-based)
* Manual timezone +/- buttons
* DST toggle

#### Dynamic UI Behavior

* Manual controls hidden when auto mode is enabled
* Labels update live

---

### 6. User Interface (LVGL)

#### Structure

* Full-screen UI with screen replacement system
* Each screen is dynamically created and destroyed

#### Screens

* Main Menu
* Log Catch
* View Entries
* GPS Info
* Settings
* About
* Condition Test (placeholder)
* Map (placeholder)

#### Navigation

* Button-based navigation
* Back button on all screens
* Smooth animated transitions

#### Keyboard Handling

* On-screen keyboard appears when text fields are focused
* UI shifts to prevent overlap
* Buttons hidden while typing

---

### 7. SD Card System

#### Initialization

* Uses HSPI bus (separate from display SPI)
* Prevents bus conflicts

#### File Operations

* Append logging
* Safe delete via:

  1. Read original file
  2. Write filtered data to temp file
  3. Replace original file

#### Error Handling

* Detects:

  * Mount failure
  * File open failure

---

## Hardware Configuration

### Microcontroller

* ESP32

---

### Display (ST7789 SPI)

| Signal | GPIO |
| ------ | ---- |
| SCLK   | 14   |
| MOSI   | 13   |
| CS     | 15   |
| DC     | 2    |

---

### Touch Controller (CST820)

| Signal     | GPIO           |
| ---------- | -------------- |
| Touch Pins | 33, 32, 25, 21 |

---

### Backlight

| Signal | GPIO |
| ------ | ---- |
| PWM    | 27   |

---

### SD Card (HSPI Bus)

| Signal | GPIO |
| ------ | ---- |
| SCK    | 18   |
| MISO   | 19   |
| MOSI   | 23   |
| CS     | 5    |

---

### GPS Module (NEO-6M)

* Connected to hardware Serial (UART0)
* Baud rate: 9600
* TX (GPS) → RX (ESP32)

---

## Software Dependencies

* LVGL
* LovyanGFX
* TinyGPS++
* Arduino SD library
* SPI library

---

## System Architecture

### Main Loop Responsibilities

* LVGL tick + rendering
* UART GPS data parsing
* Logging requests handling
* Time overlay updates
* UI updates based on GPS state

---

### Memory Management

* Screens are dynamically created and replaced
* Old screens are deleted using LVGL animation system
* Global UI pointers are cleared on screen change to prevent:

  * Dangling pointers
  * Memory leaks
  * Random crashes

---

### Event-Driven Design

* UI interactions handled via LVGL callbacks
* Logging triggered via flag (`log_request`)
* GPS updates handled asynchronously

---

## Data Format

### catch_log.csv

```id="fishinglog"
species,size,weight,timestamp
```

Example:

```id="examplelog"
Bass,45,2.3,12345678
```

---

## Known Limitations

* No persistent real-time clock (time resets without GPS)
* DST is approximate, not region-specific
* Map feature not implemented yet
* CSV file can grow indefinitely
* GPS shares UART with USB (debugging conflicts possible)

---

## Future Improvements

* Real map integration (tile-based or GPS plotting)
* Bluetooth or WiFi data export
* Better file management (size limits, pagination)
* RTC backup for timekeeping
* Weather integration
* Improved GPS fix visualization

---

## Usage Notes

* Always ensure SD card is inserted before boot
* GPS needs open sky for proper fix
* First GPS fix may take several minutes
* Avoid using input-only pins (GPIO 34–39) for outputs

---

## Summary

Fishing-Gyzmo is built like a proper embedded system should be:

* Simple
* Reliable
* Self-contained

No dependencies on phones or cloud services. Just power it on and it does its job.
