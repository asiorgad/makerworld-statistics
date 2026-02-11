#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// --- Configuration ---
const char* ssid     = "WIFI SSID HERE";
const char* password = "WIFI PASSWD HERE";
const char* url      = "https://gist.githubusercontent.com/GITHUB_USER_HERE/GIST_ID_HERE/raw/FILENAME_HERE"; 

// --- Timing Variables ---
unsigned long lastFetchTime = 0;
const unsigned long fetchInterval = 600000UL; // 10 minutes fetch interval
unsigned long lastStatSwitch = 0;
const unsigned long STAT_SWITCH_INTERVAL = 4000; // 4 seconds per screen
unsigned long lastScroll = 0;
const int SCROLL_SPEED = 30; // ms between movement

// --- Display Objects ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite scrollSprite = TFT_eSprite(&tft);

// --- Data Variables ---
String displayLabels[8] = {"Name", "User", "Followers", "Following", "Boosts", "Likes", "Downloads", "Prints"};
String displayValues[8] = {"", "", "", "", "", "", "", ""};
int currentStatIndex = 0;
bool dataLoaded = false;

// --- Scrolling Variables ---
int scrollPos = 0;
bool scrollLeft = true;

void setup() {
  Serial.begin(115200);
  Serial.println("Setup started");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Initialize Sprite (Width 200 to stay within the round screen center)
  scrollSprite.setColorDepth(8);
  if (!scrollSprite.createSprite(200, 40)) {
    Serial.println("Sprite RAM Allocation Failed!");
  } else {
    Serial.println("Sprite created successfully");
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Connecting WiFi...", 120, 110, 2);

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  fetchAndParse();
}

void loop() {
  // 1. Periodic Data Refresh
  if (millis() - lastFetchTime >= fetchInterval) {
    Serial.println("Refreshing data from server...");
    fetchAndParse();
  }

  // 2. Page Rotator (Non-blocking)
  if (millis() - lastStatSwitch >= STAT_SWITCH_INTERVAL) {
    lastStatSwitch = millis();
    currentStatIndex = (currentStatIndex + 1) % 8;
    Serial.print("Switching to stat index: ");
    Serial.println(currentStatIndex);

    // Reset scroll state for the new word
    scrollPos = 0;
    scrollLeft = true;

    // Draw the static background/label immediately
    drawStaticUI(displayLabels[currentStatIndex], displayValues[currentStatIndex], currentStatIndex);
  }

  // 3. Constant Scroll Update
  String val = displayValues[currentStatIndex].length() > 0 ? displayValues[currentStatIndex] : "—";
  if ((currentStatIndex == 0 || currentStatIndex == 1) && val.length() > 12) {
    updateScrollingText(val);
  }
}

void drawStaticUI(String label, String value, int index) {
  Serial.print("Drawing UI for index ");
  Serial.print(index);
  Serial.print(": ");
  Serial.println(label);

  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(120, 120, 119, 0x3186); // Decorative border

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString(label, 120, 50, 4);

  // Only draw value here if it's NOT a scrolling field
  if (!((index == 0 || index == 1) && value.length() > 12)) {
    drawValueNormal(value);
  }
}

void drawValueNormal(String value) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  Serial.print("Drawing value normally: ");
  Serial.println(value);

  String lower = value;
  lower.toLowerCase();
  bool hasSuffix = (lower.endsWith("k") || lower.endsWith("m"));

  if (hasSuffix) {
    String numPart = value.substring(0, value.length() - 1);
    numPart.trim();
    String suffixPart = value.substring(value.length() - 1);
    suffixPart.toUpperCase();

    tft.drawCentreString(numPart, 120, 100, 7);
    int w = tft.textWidth(numPart, 7);
    tft.drawString(suffixPart, 125 + (w / 2), 105, 4);

    Serial.print("Number part: ");
    Serial.print(numPart);
    Serial.print(", Suffix: ");
    Serial.println(suffixPart);
  } else {
    tft.drawCentreString(value, 120, 110, 4);
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
    scrollSprite.pushSprite(20, 100); // Centered on the GC9A01

    Serial.print("Scrolling text '");
    Serial.print(text);
    Serial.print("' at position: ");
    Serial.println(scrollPos);
  }
}

bool fetchAndParse() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot fetch data.");
    return false;
  }

  Serial.print("Fetching data from URL: ");
  Serial.println(url);

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  Serial.print("HTTP response code: ");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Failed to fetch data. HTTP code: ");
    Serial.println(httpCode);

    // Show error on TFT
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

  Serial.println("\nParsed values:");
  for (int i = 0; i < 8; i++) {
    Serial.print(displayLabels[i]);
    Serial.print(": ");
    Serial.println(displayValues[i]);
  }

  lastFetchTime = millis();
  dataLoaded = true;

  // Update display
  drawStaticUI(displayLabels[currentStatIndex], displayValues[currentStatIndex], currentStatIndex);

  http.end();
  Serial.println("Data fetch and parse completed successfully.");
  return true;
}
