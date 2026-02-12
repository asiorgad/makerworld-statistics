# MakerStats - MakerWorld Statistics Display

A compact ESP32-based display that shows your MakerWorld profile statistics in real-time on a round TFT screen.

## Quick Setup

1. **Upload** the sketch to your ESP32
2. **Connect** to "MakerStats-Setup" WiFi from your phone/computer
3. **⚠️ IMPORTANT: Go to "Setup" tab FIRST** and configure your Gist URL
   - The URL must end with `/raw/bambu.txt`
   - Example: `https://gist.githubusercontent.com/username/gistid/raw/bambu.txt`
4. **Then go to "WiFi"** tab and select your network
5. **Save** - device will connect and start displaying stats

## Features

- **Real-time Statistics Display**: Shows your MakerWorld profile stats including:
  - Profile Name
  - Username
  - Followers count
  - Following count
  - Boosts received
  - Likes received
  - Downloads count
  - Prints count
  - Device Uptime

- **WiFi Manager**: Easy WiFi setup via captive portal - no hardcoded credentials!
  - On first boot, creates "MakerStats-Setup" access point
  - Connect to configure WiFi and data URL through web interface
  - Settings are saved and persist across reboots

- **7-Day Change Tracking**: Displays the increase in each stat over the last 7 days
  - Shows "+X since [date]" for any stat that has increased
  - Snapshot is automatically updated every 7 days
  - Date/time displayed in UTC format

- **Data Wipe**: Hold BOOT button for 2 seconds to clear all data
  - Clears snapshot data
  - Clears WiFi credentials
  - Clears custom URL
  - Device restarts and enters setup mode

- **Auto-Refresh**: Fetches new data every 5 minutes

- **Visual Feedback**: Shows "Fetching..." indicator when updating data

- **Rotating Display**: Cycles through all 9 screens every 4 seconds

## Hardware Requirements

- ESP32 development board
- GC9A01 round TFT display (240x240 pixels)
- WiFi connection

## Libraries Required

- `WiFi.h` - ESP32 WiFi connectivity
- `HTTPClient.h` - HTTP requests
- `SPI.h` - SPI communication
- `TFT_eSPI.h` - TFT display driver
- `Preferences.h` - Persistent storage
- `WiFiManager.h` - WiFi configuration portal

## Installation

1. Install the required libraries in Arduino IDE
2. Configure TFT_eSPI for your display (see Pin Configuration below)
3. Upload the sketch to your ESP32
4. On first boot, connect to "MakerStats-Setup" WiFi network
5. Configure your WiFi credentials and data URL in the captive portal

## First-Time Setup

1. Power on the device
2. The startup screen shows for 3 seconds with instructions
3. Connect to the "MakerStats-Setup" WiFi network from your phone/computer
4. A captive portal will open automatically (or navigate to 192.168.4.1)
5. Select your WiFi network and enter the password
6. Enter your data URL (Gist URL)
7. Click Save - the device will connect and start displaying stats

## Data Wipe / Factory Reset

To clear all settings and start fresh:

**During startup:**
- Hold the BOOT button while powering on

**During operation:**
- Hold the BOOT button for 2 seconds

This will:
- Clear all snapshot data
- Clear WiFi credentials
- Clear the custom data URL
- Restart the device in setup mode

## Data Source

The device fetches statistics from a text file hosted online (e.g., GitHub Gist). The expected format is space-separated values:

```
Name Username Followers Following Boosts Likes Downloads Prints
```

Example:
```
JohnDoe @johndoe 1.2k 50 234 567 890 123
```

Supports `k` (thousands) and `m` (millions) suffixes for large numbers.

## Display Screens

| Screen | Description |
|--------|-------------|
| Name | Profile display name |
| User | Username/handle |
| Followers | Number of followers |
| Following | Number of accounts following |
| Boosts | Total boosts received |
| Likes | Total likes received |
| Downloads | Total model downloads |
| Prints | Total prints of your models |
| Uptime | How long the device has been running |

## How It Works

1. **Boot**: Device shows startup screen with last snapshot date
2. **Connect**: Uses WiFiManager to connect (or setup if not configured)
3. **Sync Time**: Synchronizes with NTP server (UTC time)
4. **Fetch**: Gets initial statistics from configured URL
5. **Snapshot**: Saves baseline values (first run or every 7 days)
6. **Display**: Rotates through all stats every 4 seconds
7. **Refresh**: Every 5 minutes, fetches new data from the server
8. **Compare**: Shows the difference between current and snapshot values

## Display Format

### Normal stat (no change):
```
Downloads
890
```

### Stat with increase:
```
Downloads
892
+2
since 05/02 14:30 UTC
```

### Large numbers with suffix:
```
Followers
1.2k
```

## Timing Configuration

```cpp
const unsigned long fetchInterval = 300000UL;        // Data refresh: 5 minutes
const unsigned long STAT_SWITCH_INTERVAL = 4000;     // Screen rotation: 4 seconds
const int SCROLL_SPEED = 30;                         // Text scroll speed: 30ms
const int SNAPSHOT_INTERVAL_DAYS = 7;                // Snapshot interval: 7 days
const unsigned long BUTTON_HOLD_MS = 2000;           // Button hold time: 2 seconds
```

## Pin Configuration

Configure your TFT_eSPI library's `User_Setup.h` for the GC9A01 display and your specific ESP32 pin connections.

The BOOT button is on GPIO 0 (standard on most ESP32 boards).

## License

Open source - feel free to modify and share!

## Author

Created for displaying MakerWorld statistics on a desktop display.
