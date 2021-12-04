#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

DynamicJsonDocument doc(4096); // Allocate a large container for parsing JSON

// TODO: enter wifi details
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// TODO: enter proxy server details
const char* MTAPI_ENDPOINT = "";
/** format: http://[SERVER_ADDRESS]/by-id/[comma_separated_list_of_ids] **/
/** e.g.    http://192.168.1.23:8080/by-id/18c4,69ba                    **/

#define POLL_DELAY 15000
unsigned long lastMillis = POLL_DELAY;

// One struct object per screen
typedef struct {
  uint8_t tft_cs;         // Chip Select pin for each display
  uint8_t rotation;       // Screen orientation
  uint8_t station_index;  // 0-index in comma separated list from above endpoint
  char*   dir;            // direction to choose from JSON (e.g. "N" for North, "S" for South)
} screenInfo_t;

#define NUM_SCREENS 4
screenInfo_t screenInfo[] = {
/** CHIP SELECT || ROTATION || STATION || DIRECTION **/
  {     32,           1,          0,         "N"    },
  {     13,           1,          0,         "S"    },
  {     25,           1,          1,         "N"    },
  {     33,           3,          1,         "S"    },
};

void debugPrint(String text, bool clearScreen = false);

void setup(void) {
  Serial.begin(115200);

  Serial.println("Setting up screens...");

  for (int i = 0; i < NUM_SCREENS; i++) {
    pinMode(screenInfo[i].tft_cs, OUTPUT);
    digitalWrite(screenInfo[i].tft_cs, LOW);
  }

  tft.init();

  for (int i = 0; i < NUM_SCREENS; i++) {
    digitalWrite(screenInfo[i].tft_cs, HIGH);
  }

  for (int i = 0; i < NUM_SCREENS; i++) {
    digitalWrite(screenInfo[i].tft_cs, LOW);
    tft.setRotation(screenInfo[i].rotation);
    tft.fillScreen(TFT_WHITE);
    digitalWrite(screenInfo[i].tft_cs, HIGH);
  }

  // Use this to see available networks instead of connecting to WiFi
  // (Useful for debugging)
  // listNetworks();

  debugPrint("Setting up wifi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    debugPrint(".");
  }

  debugPrint("Setup done");
}

void loop() {
  if (millis() - lastMillis >= POLL_DELAY) {
    lastMillis = millis();

    // Make HTTP request
    HTTPClient http;
    http.begin(MTAPI_ENDPOINT);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      // debugPrint("HTTP Response code: ", true);
      // debugPrint(String(httpResponseCode));
      String response = http.getString();
      // debugPrint(response);

      // Parse JSON response
      DeserializationError error = deserializeJson(doc, response);
      if (error) {
        debugPrint("Error!", true);
        debugPrint(error.f_str());
        return;
      }

      // Update each screen
      for (int i = 0; i < NUM_SCREENS; i++) {
        digitalWrite(screenInfo[i].tft_cs, LOW);
        tft.fillScreen(TFT_WHITE);

        // Find correct info for this screen inside the parsed JSON
        JsonArray train_info = doc["data"][screenInfo[i].station_index][screenInfo[i].dir];
        if (!train_info[0].isNull()) {
          int minutes_till_train = train_info[0]["countdown"].as<int>() / 60; /* rounds down */
          tft.setTextColor(TFT_BLACK, TFT_BLACK);

          if (minutes_till_train < 10) {
            tft.drawString(String(minutes_till_train), 20, 20, 8);
            String minOrMins = minutes_till_train == 1 ? "min" : "mins";
            tft.drawString(minOrMins, 85, 50, 4);

            if (!train_info[1].isNull()) {
              // Display next train info in case you miss this one
              int minutes_till_train_after = train_info[1]["countdown"].as<int>() / 60; /* rounds down */
              String next_info = "Next: " + String(minutes_till_train_after) + " mins";
              tft.drawString(next_info, 75, 110, 2);
            }

          } else {
            tft.drawString(String(minutes_till_train), 2, 20, 8);
            tft.drawString("mins", 120, 53, 2);
          }
        }
        digitalWrite(screenInfo[i].tft_cs, HIGH);
      }

    } else {
      debugPrint("HTTP Error code: ", true);
      debugPrint(String(httpResponseCode));
    }

    // Free resources
    http.end();
  }
}

void debugPrint(String text, bool clearScreen) {
  static int y = 0;
  digitalWrite(screenInfo[0].tft_cs, LOW);
  if (clearScreen) {
    y = 0;
    tft.fillScreen(TFT_WHITE);
  }
  tft.setTextColor(TFT_BLACK, TFT_BLACK);
  tft.drawString(text, 0, y, 2);
  digitalWrite(screenInfo[0].tft_cs, HIGH);
  y += 10;
  
  for (int i = 1; i < NUM_SCREENS; i++) {
    digitalWrite(screenInfo[i].tft_cs, LOW);
    tft.fillScreen(TFT_BLACK);
    digitalWrite(screenInfo[i].tft_cs, HIGH);
  }
}

void listNetworks() {
  while (true) {
    debugPrint("** Scan Networks **", true);
    int numSsid = WiFi.scanNetworks();
    if (numSsid == -1) {
      debugPrint("Couldn't get a wifi connection");
    } else {
      // print the network name and signal strength for each network found:
      for (int thisNet = 0; thisNet < numSsid; thisNet++) {
        debugPrint(String(WiFi.SSID(thisNet)));
        debugPrint(String(WiFi.RSSI(thisNet)));
      }
    }
    delay(30000);
  }
}
