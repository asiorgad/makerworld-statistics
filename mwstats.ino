#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>


const char* ssid     = "WIFI SSID HERE";
const char* password = "WIFI PASSWD HERE";
const char* url      = "https://gist.githubusercontent.com/GITHUB_USER_HERE/GIST_ID_HERE/raw/bambu.txt"; 
// --- Timing Variables ---
unsigned long lastFetchTime = 0;
const unsigned long fetchInterval = 300000UL; // 5 minutes
unsigned long lastStatSwitch = 0;
const unsigned long STAT_SWITCH_INTERVAL = 4000; // 4 seconds per screen
unsigned long lastScroll = 0;
const int SCROLL_SPEED = 30; // ms between movement

// --- Display Objects ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite scrollSprite = TFT_eSprite(&tft);

// --- Data Variables ---
String displayLabels[9] = {"Name", "User", "Followers", "Following", "Boosts", "Likes", "Downloads", "Prints", "Uptime"};
String displayValues[9] = {"", "", "", "", "", "", "", "", ""};
long initialValues[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0}; // values at boot
bool initialValuesSet = false; // flag to track if initial values are captured
int currentStatIndex = 0;
bool dataLoaded = false;

// --- Scrolling Variables ---
int scrollPos = 0;
bool scrollLeft = true;

// --- Uptime formatting ---
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

  // 2. Page Rotator (Non-blocking) - now 9 screens including Uptime
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
  tft.drawCentreString(label, 120, 50, 4);

  // Only draw value here if it's NOT a scrolling field
  if (!((index == 0 || index == 1) && value.length() > 12)) {
    drawValueNormal(value, index);
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

void drawValueNormal(String value, int index) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // For Name and User screens (index 0 and 1), use font 4 which supports text
  // Font 7 only supports numbers!
  if (index == 0 || index == 1) {
    tft.drawCentreString(value, 120, 100, 4); // Font 4 for text values
    return;
  }

  // For Uptime screen, show the value in large font
  if (index == 8) {
    tft.drawCentreString(value, 120, 100, 4); // Font 4 for uptime (contains letters)
    return;
  }

  long current = convertToNumber(value);
  long initial = initialValues[index];
  long delta = current - initial;

  bool showDelta = false;

  // Debug logging
  Serial.print("Index ");
  Serial.print(index);
  Serial.print(" (");
  Serial.print(displayLabels[index]);
  Serial.print("): current=");
  Serial.print(current);
  Serial.print(", initial=");
  Serial.print(initial);
  Serial.print(", delta=");
  Serial.println(delta);

  // Only show delta for numeric stats (Followers, Following, Boosts, Likes, Downloads, Prints)
  // Show delta if there's any positive change (even if initial was 0, as long as initialValuesSet is true)
  if (index >= 2 && index <= 7 && initialValuesSet && delta > 0) {
    showDelta = true;
  }

  // Build the uptime text for display
  String uptimeText = formatUptime(millis());

  Serial.print("Drawing value: ");
  Serial.print(value);
  if (showDelta) {
    Serial.print(" (+" + String(delta) + " since boot)");
  }
  Serial.println();

  // Handle suffix alignment
  String lower = value;
  lower.toLowerCase();
  bool hasSuffix = (lower.endsWith("k") || lower.endsWith("m"));

  if (hasSuffix && !showDelta) {
    // Large display for main value with suffix (font 7 is the largest, numbers only)
    String numPart = value.substring(0, value.length() - 1);
    String suffixPart = value.substring(value.length() - 1);

    tft.drawCentreString(numPart, 120, 90, 7);
    int w = tft.textWidth(numPart, 7);
    tft.drawString(suffixPart, 125 + (w / 2), 95, 4);
  } else if (showDelta) {
    // Show value in large font and delta below
    tft.drawCentreString(value, 120, 75, 7); // Large font 7 for numeric value
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString("+" + String(delta), 120, 130, 4); // Larger font for delta
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("since boot (" + uptimeText + ")", 120, 165, 2);
  } else {
    // No suffix, no delta - show numeric value in large font
    tft.drawCentreString(value, 120, 95, 7); // Font 7 for numbers
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

// --- Data Fetching ---
void showFetchingIndicator() {
  // Draw a small "Fetching..." indicator at the bottom of the screen
  tft.fillRect(0, 200, 240, 40, TFT_BLACK); // Clear bottom area
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawCentreString("Fetching...", 120, 210, 2);
}

void clearFetchingIndicator() {
  // Clear the fetching indicator
  tft.fillRect(0, 200, 240, 40, TFT_BLACK);
}

bool fetchAndParse() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot fetch data.");
    return false;
  }

  // Show fetching indicator on display
  showFetchingIndicator();

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

  // Capture initial values on first successful fetch
  if (!initialValuesSet) {
    Serial.println("Capturing initial values at boot:");
    for (int i = 2; i < 8; i++) {
      initialValues[i] = convertToNumber(displayValues[i]);
      Serial.print("  ");
      Serial.print(displayLabels[i]);
      Serial.print(": ");
      Serial.println(initialValues[i]);
    }
    initialValuesSet = true;
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

