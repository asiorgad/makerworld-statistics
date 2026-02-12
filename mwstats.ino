#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <time.h>

// --- Configuration ---
// Default URL (can be overridden via WiFiManager portal)
char dataUrl[256] = "https://gist.githubusercontent.com/USERNAME/GIST_ID/raw/bambu.txt";

// WiFiManager portal name
const char* AP_NAME = "MakerStats-Setup";

// NTP Server for time sync
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;  // UTC time
const int daylightOffset_sec = 0;  // No daylight saving

// --- Timing Variables ---
unsigned long lastFetchTime = 0;
const unsigned long fetchInterval = 300000UL; // 5 minutes
unsigned long lastStatSwitch = 0;
const unsigned long STAT_SWITCH_INTERVAL = 4000; // 4 seconds per screen
unsigned long lastScroll = 0;
const int SCROLL_SPEED = 30; // ms between movement

// --- Snapshot interval (7 days) ---
const unsigned long SNAPSHOT_INTERVAL_MS = 604800000UL; // 7 days in milliseconds
const int SNAPSHOT_INTERVAL_DAYS = 7;

// --- Boot button for clearing data ---
const int BOOT_BUTTON_PIN = 0; // GPIO 0 is the BOOT button on most ESP32 boards
const unsigned long BUTTON_DEBOUNCE_MS = 50; // Debounce time
const unsigned long BUTTON_HOLD_MS = 2000; // Hold time to trigger clear (2 seconds)
unsigned long buttonPressStart = 0;
bool buttonWasPressed = false;

// --- Persistent Storage ---
Preferences preferences;

// --- WiFiManager ---
WiFiManager wifiManager;
WiFiManagerParameter customUrl("url", "Data URL (Gist)", dataUrl, 255);
bool shouldSaveConfig = false;

// Callback for saving config
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// --- Display Objects ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite scrollSprite = TFT_eSprite(&tft);

// --- Data Variables ---
String displayLabels[9] = {"Name", "User", "Followers", "Following", "Boosts", "Likes", "Downloads", "Prints", "Uptime"};
String displayValues[9] = {"", "", "", "", "", "", "", "", ""};
long snapshotValues[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0}; // values from last snapshot
unsigned long snapshotTimestamp = 0; // Unix timestamp when snapshot was saved
bool snapshotLoaded = false;
int currentStatIndex = 0;
bool dataLoaded = false;

// --- Scrolling Variables ---
int scrollPos = 0;
bool scrollLeft = true;

// --- Time formatting ---
String formatUptime(unsigned long ms) {
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  
  String result = "";
  if (days > 0) {
    result += String(days) + "d ";
  }
  if (hours > 0 || days > 0) {
    result += String(hours) + "h ";
  }
  if (minutes > 0 || hours > 0 || days > 0) {
    result += String(minutes) + "m";
  } else {
    result += String(seconds) + "s";
  }
  
  return result;
}

String formatDate(unsigned long timestamp) {
  if (timestamp == 0) return "N/A";
  
  time_t t = (time_t)timestamp;
  struct tm* timeinfo = gmtime(&t);  // Use UTC time
  
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%d/%m %H:%M UTC", timeinfo);
  return String(buffer);
}

unsigned long getCurrentTimestamp() {
  time_t now;
  time(&now);
  return (unsigned long)now;
}

// --- Boot Button Check (runtime) ---
void checkBootButton() {
  bool buttonPressed = (digitalRead(BOOT_BUTTON_PIN) == LOW);
  
  if (buttonPressed && !buttonWasPressed) {
    // Button just pressed
    buttonPressStart = millis();
    buttonWasPressed = true;
  } else if (buttonPressed && buttonWasPressed) {
    // Button being held
    unsigned long holdTime = millis() - buttonPressStart;
    
    if (holdTime >= BUTTON_HOLD_MS) {
      // Held long enough - clear snapshot data only (keep WiFi settings)
      Serial.println("BOOT button held - clearing snapshot data!");
      
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawCentreString("Clearing", 120, 90, 4);
      tft.drawCentreString("snapshot data...", 120, 120, 4);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.drawCentreString("(WiFi kept)", 120, 160, 2);
      
      clearSnapshot();
      delay(1500);
      
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawCentreString("Data cleared!", 120, 100, 4);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawCentreString("Restarting...", 120, 140, 2);
      delay(1500);
      
      ESP.restart();
    }
  } else if (!buttonPressed && buttonWasPressed) {
    // Button released
    buttonWasPressed = false;
  }
}

// --- Snapshot Functions ---
void clearSnapshot() {
  // Clear snapshot data
  preferences.remove("snapTime");
  preferences.remove("followers");
  preferences.remove("following");
  preferences.remove("boosts");
  preferences.remove("likes");
  preferences.remove("downloads");
  preferences.remove("prints");
  
  // Clear custom URL
  preferences.remove("dataUrl");
  
  snapshotTimestamp = 0;
  for (int i = 0; i < 9; i++) {
    snapshotValues[i] = 0;
  }
  snapshotLoaded = false;
  
  Serial.println("Snapshot data cleared!");
}

void clearAllData() {
  // Clear snapshot data
  clearSnapshot();
  
  // Clear WiFi credentials
  Serial.println("Clearing WiFi credentials...");
  wifiManager.resetSettings();
  
  Serial.println("All data cleared!");
}

void loadSnapshot() {
  snapshotTimestamp = preferences.getULong("snapTime", 0);
  snapshotValues[2] = preferences.getLong("followers", 0);
  snapshotValues[3] = preferences.getLong("following", 0);
  snapshotValues[4] = preferences.getLong("boosts", 0);
  snapshotValues[5] = preferences.getLong("likes", 0);
  snapshotValues[6] = preferences.getLong("downloads", 0);
  snapshotValues[7] = preferences.getLong("prints", 0);
  
  if (snapshotTimestamp > 0) {
    snapshotLoaded = true;
    Serial.println("Loaded snapshot from " + formatDate(snapshotTimestamp));
    for (int i = 2; i < 8; i++) {
      Serial.print("  ");
      Serial.print(displayLabels[i]);
      Serial.print(": ");
      Serial.println(snapshotValues[i]);
    }
  } else {
    Serial.println("No snapshot found in storage");
  }
}

void saveSnapshot() {
  unsigned long currentTime = getCurrentTimestamp();
  
  preferences.putULong("snapTime", currentTime);
  preferences.putLong("followers", convertToNumber(displayValues[2]));
  preferences.putLong("following", convertToNumber(displayValues[3]));
  preferences.putLong("boosts", convertToNumber(displayValues[4]));
  preferences.putLong("likes", convertToNumber(displayValues[5]));
  preferences.putLong("downloads", convertToNumber(displayValues[6]));
  preferences.putLong("prints", convertToNumber(displayValues[7]));
  
  snapshotTimestamp = currentTime;
  for (int i = 2; i < 8; i++) {
    snapshotValues[i] = convertToNumber(displayValues[i]);
  }
  snapshotLoaded = true;
  
  Serial.println("Saved new snapshot at " + formatDate(currentTime));
}

void checkAndUpdateSnapshot() {
  unsigned long currentTime = getCurrentTimestamp();
  
  // First run - no snapshot exists
  if (snapshotTimestamp == 0) {
    Serial.println("First run - saving initial snapshot");
    saveSnapshot();
    return;
  }
  
  // Check if 7 days have passed since last snapshot
  unsigned long elapsed = currentTime - snapshotTimestamp;
  if (elapsed >= (SNAPSHOT_INTERVAL_DAYS * 24 * 60 * 60)) {
    Serial.println("7 days passed - updating snapshot");
    saveSnapshot();
  }
}

// Convert suffixed numbers to long
long convertToNumber(String str) {
  str.trim();
  long multiplier = 1;
  if (str.endsWith("k") || str.endsWith("K")) {
    multiplier = 1000;
    str = str.substring(0, str.length() - 1);
  } else if (str.endsWith("m") || str.endsWith("M")) {
    multiplier = 1000000;
    str = str.substring(0, str.length() - 1);
  }
  str.trim();
  return (long)(str.toFloat() * multiplier);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setup started");

  // Initialize boot button pin
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Initialize Preferences
  preferences.begin("makerstats", false);
  
  // Load saved URL from preferences (if exists)
  String savedUrl = preferences.getString("dataUrl", "");
  if (savedUrl.length() > 0) {
    savedUrl.toCharArray(dataUrl, sizeof(dataUrl));
    Serial.print("Loaded saved URL: ");
    Serial.println(dataUrl);
  }

  // Initialize display first
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Check if BOOT button is pressed during startup to clear snapshot data
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    Serial.println("BOOT button pressed - clearing snapshot data!");
    
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("Clearing", 120, 90, 4);
    tft.drawCentreString("snapshot data...", 120, 120, 4);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("(WiFi kept)", 120, 160, 2);
    
    clearSnapshot();
    delay(2000);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString("Data cleared!", 120, 100, 4);
    tft.drawCentreString("Restarting...", 120, 140, 2);
    delay(1500);
    ESP.restart();
  } else {
    loadSnapshot();
  }

  // Show startup info screen
  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(120, 120, 119, 0x3186);
  
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString("MakerStats", 120, 30, 4);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (snapshotLoaded && snapshotTimestamp > 0) {
    tft.drawCentreString("Last snapshot:", 120, 70, 2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString(formatDate(snapshotTimestamp), 120, 90, 2);
  } else {
    tft.drawCentreString("No saved data", 120, 80, 2);
  }
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawCentreString("Hold BOOT button", 120, 130, 2);
  tft.drawCentreString("for 2s to wipe data", 120, 150, 2);
  
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawCentreString("Starting in 3s...", 120, 190, 2);
  
  delay(3000);
  tft.fillScreen(TFT_BLACK);

  // Initialize Sprite (Width 200 to stay within the round screen center)
  scrollSprite.setColorDepth(8);
  if (!scrollSprite.createSprite(200, 40)) {
    Serial.println("Sprite RAM Allocation Failed!");
  } else {
    Serial.println("Sprite created successfully");
  }

  // Setup WiFiManager
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Connecting WiFi...", 120, 100, 2);
  tft.drawCentreString("If not configured,", 120, 130, 2);
  tft.drawCentreString("connect to:", 120, 150, 2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawCentreString(AP_NAME, 120, 175, 2);

  // WiFiManager settings
  wifiManager.setDebugOutput(true);
  wifiManager.setParamsPage(true);  // Enable separate "Setup" page for parameters
  
  // Update custom parameter with current URL value
  customUrl.setValue(dataUrl, 255);
  
  // Add custom parameter to WiFiManager
  wifiManager.addParameter(&customUrl);
  
  // Set callback for saving config
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // Set timeout for portal (3 minutes)
  wifiManager.setConfigPortalTimeout(180);
  
  // Try to connect, if fails start config portal
  Serial.println("Starting WiFiManager...");
  if (!wifiManager.autoConnect(AP_NAME)) {
    Serial.println("Failed to connect and hit timeout");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("WiFi Failed!", 120, 100, 4);
    tft.drawCentreString("Restarting...", 120, 140, 2);
    delay(3000);
    ESP.restart();
  }

  // If we get here, we're connected
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Save custom parameters if needed
  if (shouldSaveConfig) {
    Serial.println("Saving custom config...");
    strcpy(dataUrl, customUrl.getValue());
    preferences.putString("dataUrl", dataUrl);
    Serial.print("Saved URL: ");
    Serial.println(dataUrl);
    shouldSaveConfig = false;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawCentreString("WiFi Connected!", 120, 100, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString(WiFi.localIP().toString(), 120, 130, 2);
  delay(1500);

  // Initialize NTP time sync
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Syncing time...", 120, 110, 2);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Wait for time to sync
  struct tm timeinfo;
  int retries = 0;
  while (!getLocalTime(&timeinfo) && retries < 10) {
    delay(500);
    retries++;
  }
  
  if (retries < 10) {
    Serial.println("Time synchronized!");
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.println(timeStr);
  } else {
    Serial.println("Failed to sync time");
  }

  fetchAndParse();
}

void loop() {
  // 0. Check boot button for data clearing
  checkBootButton();
  
  // 1. Periodic Data Refresh
  if (millis() - lastFetchTime >= fetchInterval) {
    Serial.println("Refreshing data from server...");
    fetchAndParse();
  }

  // 2. Page Rotator (Non-blocking) - 9 screens including Uptime
  if (millis() - lastStatSwitch >= STAT_SWITCH_INTERVAL) {
    lastStatSwitch = millis();
    currentStatIndex = (currentStatIndex + 1) % 9;
    Serial.print("Switching to stat index: ");
    Serial.println(currentStatIndex);

    // Reset scroll state for the new word
    scrollPos = 0;
    scrollLeft = true;

    // Update uptime value before displaying
    if (currentStatIndex == 8) {
      displayValues[8] = formatUptime(millis());
    }

    // Draw the static background/label immediately
    drawStaticUI(displayLabels[currentStatIndex], displayValues[currentStatIndex], currentStatIndex);
  }

  // 3. Constant Scroll Update
  String val = displayValues[currentStatIndex].length() > 0 ? displayValues[currentStatIndex] : "—";
  if ((currentStatIndex == 0 || currentStatIndex == 1) && val.length() > 12) {
    updateScrollingText(val);
  }
}

// --- Draw Functions ---
void drawStaticUI(String label, String value, int index) {
  Serial.print("Drawing UI for index ");
  Serial.print(index);
  Serial.print(": ");
  Serial.println(label);

  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(120, 120, 119, 0x3186); // Decorative border

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString(label, 120, 35, 4);  // Moved title up from 50 to 35

  // Only draw value here if it's NOT a scrolling field
  if (!((index == 0 || index == 1) && value.length() > 12)) {
    drawValueNormal(value, index);
  }
}

void drawValueNormal(String value, int index) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // For Name and User screens (index 0 and 1), use font 4 which supports text
  if (index == 0 || index == 1) {
    tft.drawCentreString(value, 120, 110, 4);  // Moved down from 100 to 110
    return;
  }

  // For Uptime screen, show the value
  if (index == 8) {
    tft.drawCentreString(value, 120, 110, 4);  // Moved down from 100 to 110
    return;
  }

  long current = convertToNumber(value);
  long snapshot = snapshotValues[index];
  long delta = current - snapshot;

  bool showDelta = false;

  // Show delta if there's a positive change and we have a valid snapshot
  if (index >= 2 && index <= 7 && snapshotLoaded && snapshot > 0 && delta > 0) {
    showDelta = true;
  }

  // Get snapshot date for display
  String snapshotDate = formatDate(snapshotTimestamp);

  Serial.print("Drawing value: ");
  Serial.print(value);
  if (showDelta) {
    Serial.print(" (+" + String(delta) + " since " + snapshotDate + ")");
  }
  Serial.println();

  // Handle suffix alignment
  String lower = value;
  lower.toLowerCase();
  bool hasSuffix = (lower.endsWith("k") || lower.endsWith("m"));

  if (hasSuffix && !showDelta) {
    // Large display for main value with suffix
    String numPart = value.substring(0, value.length() - 1);
    String suffixPart = value.substring(value.length() - 1);

    tft.drawCentreString(numPart, 120, 100, 7);  // Moved down from 90 to 100
    int w = tft.textWidth(numPart, 7);
    tft.drawString(suffixPart, 125 + (w / 2), 105, 4);  // Moved down from 95 to 105
  } else if (showDelta) {
    // Show value in large font and delta below with date
    // Handle suffix separately since Font 7 only supports numbers
    if (hasSuffix) {
      String numPart = value.substring(0, value.length() - 1);
      String suffixPart = value.substring(value.length() - 1);
      
      tft.drawCentreString(numPart, 120, 80, 7);  // Moved down from 70 to 80
      int w = tft.textWidth(numPart, 7);
      tft.drawString(suffixPart, 125 + (w / 2), 85, 4);  // Moved down from 75 to 85
    } else {
      tft.drawCentreString(value, 120, 80, 7);  // Moved down from 70 to 80
    }
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString("+" + String(delta), 120, 135, 4);  // Moved down from 125 to 135
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("since " + snapshotDate, 120, 170, 2);  // Moved down from 160 to 170
  } else {
    // No suffix, no delta - show numeric value in large font
    tft.drawCentreString(value, 120, 105, 7);  // Moved down from 95 to 105
  }
}

void updateScrollingText(String text) {
  int font = 4;
  int textWidth = tft.textWidth(text, font);
  int viewWidth = 200;

  if (millis() - lastScroll > SCROLL_SPEED) {
    lastScroll = millis();

    if (scrollLeft) {
      scrollPos -= 2;
      if (scrollPos <= -(textWidth - viewWidth + 10)) scrollLeft = false;
    } else {
      scrollPos += 2;
      if (scrollPos >= 10) scrollLeft = true;
    }

    scrollSprite.fillSprite(TFT_BLACK);
    scrollSprite.setTextColor(TFT_WHITE);
    scrollSprite.drawString(text, scrollPos, 5, font);
    scrollSprite.pushSprite(20, 100);
  }
}

// --- Data Fetching ---
void showFetchingIndicator() {
  tft.fillRect(0, 200, 240, 40, TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawCentreString("Fetching...", 120, 210, 2);
}

void clearFetchingIndicator() {
  tft.fillRect(0, 200, 240, 40, TFT_BLACK);
}

bool fetchAndParse() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot fetch data.");
    return false;
  }

  showFetchingIndicator();

  Serial.print("Fetching data from URL: ");
  Serial.println(dataUrl);

  HTTPClient http;
  http.begin(dataUrl);
  int httpCode = http.GET();

  Serial.print("HTTP response code: ");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Failed to fetch data. HTTP code: ");
    Serial.println(httpCode);

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("Data fetch failed!", 120, 100, 4);
    tft.drawCentreString("HTTP code: " + String(httpCode), 120, 140, 2);

    http.end();
    return false;
  }

  String payload = http.getString();
  Serial.println("Raw payload received:");
  Serial.println(payload);
  payload.trim();

  int tokenIndex = 0;
  int startPos = 0;
  String tokens[20];

  while (startPos < payload.length() && tokenIndex < 20) {
    int spacePos = payload.indexOf(' ', startPos);
    if (spacePos == -1) {
      tokens[tokenIndex++] = payload.substring(startPos);
      break;
    } else {
      String token = payload.substring(startPos, spacePos);
      if (token.length() > 0) {
        tokens[tokenIndex++] = token;
      }
      startPos = spacePos + 1;
    }
  }

  Serial.print("Total tokens parsed: ");
  Serial.println(tokenIndex);

  if (tokenIndex >= 2) {
    displayValues[0] = tokens[0]; // Name
    displayValues[1] = tokens[1]; // User

    int i = 2;

    // Followers
    if (i < tokenIndex) {
      String followersValue = tokens[i];
      if (i + 1 < tokenIndex && (tokens[i + 1].equalsIgnoreCase("k") || tokens[i + 1].equalsIgnoreCase("m"))) {
        followersValue += tokens[i + 1];
        i += 2;
      } else {
        i++;
      }
      displayValues[2] = followersValue;
    }

    if (i < tokenIndex && tokens[i].equalsIgnoreCase("Followers")) i++;

    // Following
    if (i < tokenIndex) {
      String followingValue = tokens[i];
      if (i + 1 < tokenIndex && (tokens[i + 1].equalsIgnoreCase("k") || tokens[i + 1].equalsIgnoreCase("m"))) {
        followingValue += tokens[i + 1];
        i += 2;
      } else {
        i++;
      }
      displayValues[3] = followingValue;
    }

    if (i < tokenIndex && tokens[i].equalsIgnoreCase("Following")) i++;

    int fieldIndex = 4;
    while (i < tokenIndex && fieldIndex < 8) {
      String value = tokens[i];
      if (i + 1 < tokenIndex && (tokens[i + 1].equalsIgnoreCase("k") || tokens[i + 1].equalsIgnoreCase("m"))) {
        value += tokens[i + 1];
        i += 2;
      } else {
        i++;
      }
      displayValues[fieldIndex++] = value;
    }
  }

  // Check if we need to save/update snapshot
  checkAndUpdateSnapshot();

  Serial.println("\nParsed values:");
  for (int i = 0; i < 8; i++) {
    Serial.print(displayLabels[i]);
    Serial.print(": ");
    Serial.println(displayValues[i]);
  }

  lastFetchTime = millis();
  dataLoaded = true;

  drawStaticUI(displayLabels[currentStatIndex], displayValues[currentStatIndex], currentStatIndex);

  http.end();
  Serial.println("Data fetch and parse completed successfully.");
  return true;
}

