# MakerStats - MakerWorld Statistics Display

A compact ESP32-based display that shows your MakerWorld profile statistics in real-time on a round TFT screen.

![MakerStats Display](https://img.shields.io/badge/ESP32-Ready-green) ![License](https://img.shields.io/badge/License-Open%20Source-blue)

## 🚀 Installation

### Option 1: Web Flasher (Recommended)

The easiest way to install - no software required!

1. **Open the Web Flasher**: [https://asiorgad.github.io/makerworld-statistics/docs/](https://asiorgad.github.io/makerworld-statistics/docs/)
2. **Connect** your ESP32 via USB
3. **Click** "Install MakerStats"
4. **Wait** for flashing to complete

> **Requirements**: Chrome, Edge, or Opera browser on desktop

### Option 2: Arduino IDE (Manual)

1. **Install Libraries** in Arduino IDE:
   - `WiFiManager` by tzapu
   - `TFT_eSPI` by Bodmer

2. **Configure TFT_eSPI** - Edit `User_Setup.h` in the TFT_eSPI library folder:
   ```cpp
   #define GC9A01_DRIVER
   #define TFT_WIDTH  240
   #define TFT_HEIGHT 240
   #define TFT_MISO -1
   #define TFT_MOSI 23
   #define TFT_SCLK 18
   #define TFT_CS   15
   #define TFT_DC    2
   #define TFT_RST   4
   ```

3. **Upload** `mwstats.ino` to your ESP32

## ⚙️ First-Time Setup

After flashing, configure your device:

1. **Connect** to "MakerStats-Setup" WiFi from your phone/computer
2. **⚠️ Go to "Setup" tab FIRST** and enter your Gist URL:
   ```
   https://gist.githubusercontent.com/USERNAME/GIST_ID/raw/bambu.txt
   ```
3. **Then go to "WiFi" tab** and select your network
4. **Save** - device will connect and start displaying stats

## 📊 Features

- **Real-time Statistics**: Displays followers, likes, downloads, prints, and more
- **7-Day Change Tracking**: Shows "+X since [date]" for increasing stats
- **WiFi Manager**: Easy setup via captive portal - no hardcoded credentials
- **Auto-Refresh**: Updates every 5 minutes
- **Rotating Display**: Cycles through 9 screens every 4 seconds
- **Factory Reset**: Hold BOOT button for 2 seconds to wipe all data

## 🔧 Hardware Requirements

| Component | Specification |
|-----------|---------------|
| Board | ESP32, ESP32-C3, or ESP32-S3 |
| Display | GC9A01 round TFT (240x240 pixels) |
| Connection | WiFi 2.4GHz |

## 📝 Data Source

Create a GitHub Gist with your MakerWorld stats in this format:

```
Name Username Followers Following Boosts Likes Downloads Prints
```

**Example:**
```
JohnDoe @johndoe 1.2k 50 234 567 890 123
```

> Supports `k` (thousands) and `m` (millions) suffixes for large numbers.

## 📺 Display Screens

| Screen | Description |
|--------|-------------|
| Name | Profile display name |
| User | Username/handle |
| Followers | Number of followers (+change) |
| Following | Number following |
| Boosts | Total boosts received (+change) |
| Likes | Total likes received (+change) |
| Downloads | Total downloads (+change) |
| Prints | Total prints (+change) |
| Uptime | Device running time |

## 🔄 Reset Snapshot Data

To clear the 7-day snapshot and start fresh tracking:

- **Hold BOOT button** for 2 seconds (during operation)
- Or **hold BOOT button** while powering on

This clears:
- ✓ Snapshot data (7-day tracking resets)
- ✓ Custom data URL

**WiFi settings are preserved** - no need to reconfigure your network.

> **Tip**: To fully reset WiFi settings, re-flash the device using the web flasher.

## ⏱️ Timing Configuration

| Setting | Default | Description |
|---------|---------|-------------|
| Data refresh | 5 min | How often stats are fetched |
| Screen rotation | 4 sec | Time per stat screen |
| Snapshot interval | 7 days | Change tracking period |
| Button hold | 2 sec | Time to trigger reset |

## 📌 Pin Configuration

The BOOT button uses GPIO 0 (standard on most ESP32 boards).

For custom pin configurations, edit the TFT_eSPI `User_Setup.h` file.

## 📄 License

Open source - feel free to modify and share!

## 👤 Author

Created for displaying MakerWorld statistics on a desktop display.
