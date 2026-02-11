#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// --- Configuration ---
const char* ssid     = "WIFI SSID HERE";
const char* password = "WIFI PASSWD HERE";
const char* url      = "https://gist.githubusercontent.com/GITHUB_USER_HERE/GIST_ID_HERE/raw";

// --- Timing Variables ---
unsigned long lastFetchTime = 0;
const unsigned long fetchInterval = 3600000UL; // 1 hour
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
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Initialize Sprite (Width 200 to stay within the round screen center)
  scrollSprite.setColorDepth(8);
  if (!scrollSprite.createSprite(200, 40)) {
    Serial.println("Sprite RAM Allocation Failed!");
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Connecting WiFi...", 120, 110, 2);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  fetchAndParse();
}

void loop() {
  // 1. Periodic Data Refresh
  if (millis() - lastFetchTime >= fetchInterval) {
    fetchAndParse();
  }

  // 2. Page Rotator (Non-blocking)
  if (millis() - lastStatSwitch >= STAT_SWITCH_INTERVAL) {
    lastStatSwitch = millis();
    currentStatIndex = (currentStatIndex + 1) % 8;
    
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
  
  String lower = value;
  lower.toLowerCase();
  bool hasSuffix = (lower.endsWith("k") || lower.endsWith("m"));

  if (hasSuffix) {
    // Logic for numbers with suffixes (K/M)
    String numPart = value.substring(0, value.length() - 1);
    numPart.trim();
    String suffixPart = value.substring(value.length() - 1);
    suffixPart.toUpperCase();
    
    tft.drawCentreString(numPart, 120, 100, 7);
    int w = tft.textWidth(numPart, 7);
    tft.drawString(suffixPart, 125 + (w / 2), 105, 4);
  } else {
    // Default center draw for regular numbers
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
  }
}

bool fetchAndParse() {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Debug: Print raw payload
    Serial.println("Raw payload:");
    Serial.println(payload);
    
    payload.trim();
    
    // Parse the data based on the actual format
    // Format: aoprint @aoegad 38 k Followers 18 Following 79 1.8 k 3.6 k 1.7 k
    
    int tokenIndex = 0;
    int startPos = 0;
    String tokens[20]; // Temporary array to hold all tokens
    
    // Tokenize by spaces
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
    
    // Now map tokens to displayValues
    // Expected format: Name Username FollowersNum FollowersUnit Username FollowingNum FollowingUnit BoostsNum BoostsUnit LikesNum LikesUnit DownloadsNum DownloadsUnit PrintsNum PrintsUnit
    
    if (tokenIndex >= 2) {
      displayValues[0] = tokens[0]; // Name (aoprint)
      displayValues[1] = tokens[1]; // User (@aoegad)
      
      // Parse Followers (38 k -> "38k")
      int i = 2;
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
      
      // Skip "Followers" text
      if (i < tokenIndex && tokens[i].equalsIgnoreCase("Followers")) {
        i++;
      }
      
      // Parse Following (18 or 18 k -> "18" or "18k")
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
      
      // Skip "Following" text
      if (i < tokenIndex && tokens[i].equalsIgnoreCase("Following")) {
        i++;
      }
      
      // Parse remaining numeric fields: Boosts, Likes, Downloads, Prints
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
    
    // Debug: Print parsed values
    Serial.println("\nParsed values:");
    for (int i = 0; i < 8; i++) {
      Serial.print(displayLabels[i]);
      Serial.print(": ");
      Serial.println(displayValues[i]);
    }
    
    lastFetchTime = millis();
    dataLoaded = true;
    
    // Update the display with the current stat
    drawStaticUI(displayLabels[currentStatIndex], displayValues[currentStatIndex], currentStatIndex);
    
    http.end();
    return true;
  }
  
  http.end();
  return false;
}
