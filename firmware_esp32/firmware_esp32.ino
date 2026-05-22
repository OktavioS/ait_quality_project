#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include "DHT.h"

const char* serverName = "http://DenysPolitech.pythonanywhere.com/api/data";

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define SHARP_LED_PIN 5
#define SHARP_VO_PIN 2

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 3600000;

unsigned long lastReadTime = 0;
const unsigned long readInterval = 5000;

float lastSentT = -100.0;
float lastSentH = -100.0;
float lastSentDust = -100.0;

void localLog(String message) {
  Serial.print("[");
  Serial.print(millis() / 1000);
  Serial.print("s] LOG: ");
  Serial.println(message);
}

void setup() {
  Serial.begin(115200);
  localLog("Start of smart system");

  dht.begin();
  pinMode(SHARP_LED_PIN, OUTPUT);
  digitalWrite(SHARP_LED_PIN, HIGH);

  WiFiManager wm;

  localLog("Looking for saved network...");

  if (!wm.autoConnect("AirMonitor_Setup", "12345678")) {
    localLog("Could not connect. Restart...");
    delay(3000);
    ESP.restart();
  }

  localLog("Connected to Wi-Fi!");
  localLog("IP Address: " + WiFi.localIP().toString());
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastReadTime >= readInterval) {
    lastReadTime = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    digitalWrite(SHARP_LED_PIN, LOW);
    delayMicroseconds(280);
    int voMeasured = analogRead(SHARP_VO_PIN);
    delayMicroseconds(40);
    digitalWrite(SHARP_LED_PIN, HIGH);

    float calcVoltage = voMeasured * (3.3 / 4095.0);
    float dustDensity = (0.17 * calcVoltage - 0.47) * 1000.0;
    if (dustDensity < 0) dustDensity = 0.0;

    if (isnan(h) || isnan(t)) {
      localLog("Error DHT11!");
      return;
    }

    bool tempChanged = abs(t - lastSentT) >= 1.0;
    bool humChanged = abs(h - lastSentH) >= 5.0;
    bool dustChanged = abs(dustDensity - lastSentDust) >= 11.0;

    bool timePassed = (currentMillis - lastSendTime) >= sendInterval;
    bool firstRun = (lastSendTime == 0);

    if (tempChanged || humChanged || dustChanged || timePassed || firstRun) {

        if(WiFi.status() == WL_CONNECTED){
          HTTPClient http;
          http.begin(serverName);
          http.addHeader("Content-Type", "application/json");

          String jsonPayload = "{\"pm25\":" + String(dustDensity) + ",\"temperature\":" + String(t) + ",\"humidity\":" + String(h) + "}";

          if(timePassed || firstRun) localLog("Scheduled sending");
          else localLog("Emergency sending");

          localLog("Data: " + jsonPayload);

          int httpResponseCode = http.POST(jsonPayload);

          if (httpResponseCode == 200) {
            localLog("Success delivered on server");
            lastSentT = t;
            lastSentH = h;
            lastSentDust = dustDensity;
            lastSendTime = currentMillis;
          } else {
            localLog("Error HTTP: " + String(httpResponseCode));
          }
          http.end();
        } else {
          localLog("No Wi-Fi connection");
        }
    }
  }
}