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
const unsigned long fetchInterval = 3600000UL; 
unsigned long lastStatSwitch = 0;
const unsigned long STAT_SWITCH_INTERVAL = 4000; 
unsigned long lastScroll = 0;
const int SCROLL_SPEED = 30; 

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
int VIEW_WIDTH = 200; // Visible width for the sprite

void setup() {
  Serial.begin(115200);
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Initialize Sprite
  scrollSprite.setColorDepth(8);
  if (!scrollSprite.createSprite(VIEW_WIDTH, 50)) {
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
  if (millis() - lastFetchTime >= fetchInterval) {
    fetchAndParse();
  }

  // Page Rotator
  if (millis() - lastStatSwitch >= STAT_SWITCH_INTERVAL) {
    lastStatSwitch = millis();
    currentStatIndex = (currentStatIndex + 1) % 8;
    
    scrollPos = 0;
    scrollLeft = true;
    
    drawStaticUI(displayLabels[currentStatIndex]);
  }

  // Constant Scroll Update - NOW APPLIES TO EVERYTHING
  String val = displayValues[currentStatIndex].length() > 0 ? displayValues[currentStatIndex] : "—";
  
  // Calculate width using Font 4
  int textWidth = tft.textWidth(val, 4);

  if (textWidth > VIEW_WIDTH) {
    // Scroll if too long
    updateScrollingText(val, true);
  } else {
    // Just center it within the sprite if it fits (no movement)
    updateScrollingText(val, false);
  }
}

void drawStaticUI(String label) {
  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(120, 120, 119, 0x3186); 

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString(label, 120, 50, 4); // Label stays at top
}

void updateScrollingText(String text, bool shouldMove) {
  int font = 4;
  int textWidth = tft.textWidth(text, font);

  if (millis() - lastScroll > SCROLL_SPEED) {
    lastScroll = millis();

    if (shouldMove) {
      if (scrollLeft) {
        scrollPos -= 2;
        if (scrollPos <= -(textWidth - VIEW_WIDTH + 15)) scrollLeft = false;
      } else {
        scrollPos += 2;
        if (scrollPos >= 15) scrollLeft = true;
      }
    } else {
      // Center static text inside the sprite area
      scrollPos = (VIEW_WIDTH - textWidth) / 2;
    }

    scrollSprite.fillSprite(TFT_BLACK);
    scrollSprite.setTextColor(TFT_WHITE);
    scrollSprite.drawString(text, scrollPos, 10, font);
    scrollSprite.pushSprite(20, 100); // Draw at the center vertical position
  }
}

bool fetchAndParse() {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();
    
    int tokenIndex = 0;
    int startPos = 0;
    String tokens[25]; 
    
    while (startPos < payload.length() && tokenIndex < 25) {
      int spacePos = payload.indexOf(' ', startPos);
      if (spacePos == -1) {
        tokens[tokenIndex++] = payload.substring(startPos);
        break;
      } else {
        String token = payload.substring(startPos, spacePos);
        if (token.length() > 0) tokens[tokenIndex++] = token;
        startPos = spacePos + 1;
      }
    }
    
    if (tokenIndex >= 2) {
      displayValues[0] = tokens[0]; 
      displayValues[1] = tokens[1]; 
      
      int i = 2;
      for(int field = 2; field < 8; field++) {
        // Skip label words like "Followers" or "Following" if they appear in the stream
        if (i < tokenIndex && (tokens[i].equalsIgnoreCase("Followers") || tokens[i].equalsIgnoreCase("Following"))) {
            i++;
        }

        if (i < tokenIndex) {
          String val = tokens[i];
          // Check if next token is a unit (k/m)
          if (i + 1 < tokenIndex && (tokens[i + 1].equalsIgnoreCase("k") || tokens[i + 1].equalsIgnoreCase("m"))) {
            val += tokens[i + 1].toUpperCase();
            i += 2;
          } else {
            i++;
          }
          displayValues[field] = val;
        }
      }
    }
    
    lastFetchTime = millis();
    dataLoaded = true;
    drawStaticUI(displayLabels[currentStatIndex]);
    http.end();
    return true;
  }
  http.end();
  return false;
}
